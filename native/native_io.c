#include "native.h"
#include "inline.h"
#include "storyfile.h"
#include "messages.h"
#include "glkop.h"
#include <assert.h>
#include <stdio.h>

#ifdef NATIVE_DEBUG_GLK
#include "gi_dispa.h"
#endif

static uint32_t cur_iosys_mode      = 0;
static uint32_t cur_iosys_rock      = 0;
static uint32_t cur_decoding_tbl    = 0;

/* The string table is encoded in the story file as a Huffman tree. When it
   is loaded, it is converted to a look-up table of size 2**H, where H is the
   height of the tree, where we can look up multiple bits at once. Each lookup
   results in one node being selected and a number of bits being shifted.

   Note that building the look-up table (and therefore the setstringtbl opcode)
   is relatively slow, so games shouldn't do that often!
*/
static int strtbl_bits = 0;
static struct StringTableEntry
{
    int      bits_used;
    uint32_t node_offset;
} *strtbl = NULL;


/* Temporarily exported stack pointer (for use by GLK dispatch layer): */
uint32_t **glk_stack_ptr = NULL;

/* Defined in glulxe.c; reused here: */
extern char *make_temp_string(glui32 addr);
extern glui32 *make_temp_ustring(glui32 addr);
extern void free_temp_string(char *str);
extern void free_temp_ustring(glui32 *str);


void native_getiosys(uint32_t *mode, uint32_t *rock)
{
    *mode = cur_iosys_mode;
    *rock = cur_iosys_rock;
}

uint32_t native_getstringtbl()
{
    return cur_decoding_tbl;
}

uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp)
{
    uint32_t n, *args, res;

    /* Copy arguments from stack to temporary buffer.
       (This is necessary since Glk may pop/push data on the stack too. */
    args = alloca(sizeof(narg)*sizeof(uint32_t));
    *sp -= narg;
    for (n = 0; n < narg; ++n)
        args[n] = (*sp)[narg - n - 1];

#ifdef NATIVE_DEBUG_GLK
    printf( "glk(0x%02x, %d) %s(", selector, narg,
            gidispatch_get_function_by_id(selector)->name );
    for (n = 0; n < narg; ++n) {
        if (n > 0) printf(", ");
        printf("%d", args[n]);
    }
    printf(")");
    fflush(stdout);
#endif /* def NATIVE_DEBUG_GLK */

    glk_stack_ptr = sp;
    res = perform_glk(selector, narg, args);
    glk_stack_ptr = NULL;

#ifdef NATIVE_DEBUG_GLK
    printf(" => %d\n", res);
    fflush(stdout);
#endif

    return res;
}

void native_setiosys(uint32_t mode, uint32_t rock)
{
    switch (mode)
    {
    default:
        error("invalid I/O mode selected (%u, %u)", mode, rock);
        cur_iosys_mode = mode;
        cur_iosys_rock = rock;
        break;

    case 0:  /* null iosys */
        cur_iosys_mode = 0;
        cur_iosys_rock = rock;
        break;

    /* NB. case 1 missing because filter is not implemented */

    case 2: /* Glk */
        cur_iosys_mode = 2;
        cur_iosys_rock = rock;
        break;
    }
}

static int get_tree_height(uint32_t offset)
{
    int a, b;
    if (get_byte(offset) != 0) return 0;
    a = get_tree_height(get_long(offset + 1));
    b = get_tree_height(get_long(offset + 5));
    return 1 + (a > b ? a : b);
}

static void build_string_table(uint32_t offset, uint32_t value, int bits_used)
{
    if (get_byte(offset) == 0)
    {
        /* branch node */
        build_string_table(get_long(offset + 1), value, bits_used + 1);
        value |= 1<<bits_used;
        build_string_table(get_long(offset + 5), value, bits_used + 1);
    }
    else
    {
        /* leaf node */
        uint32_t n, bits_unused = strtbl_bits - bits_used;
        for (n = 0; n < (1u<<bits_unused); ++n)
        {
            struct StringTableEntry *entry = &strtbl[(n << bits_used)|value];
            entry->bits_used   = bits_used;
            entry->node_offset = offset;
        }
    }
}

void native_setstringtbl(uint32_t offset)
{
    /* Avoid rebuilding table if it hasn't changed */
    if (offset == cur_decoding_tbl) return;

    cur_decoding_tbl = offset;

    if (strtbl != NULL)
    {
        free(strtbl);
        strtbl = NULL;
    }

    {
        uint32_t end = offset + get_long(offset);
        uint32_t root = get_long(offset + 8);
        strtbl_bits = get_tree_height(root);
        if (offset > 0 && end <= init_ramstart && strtbl_bits <= 20)
        {
            strtbl = malloc((1<<strtbl_bits)*sizeof(*strtbl));
            assert(strtbl != NULL);
            build_string_table(root, 0, 0);
        }
    }
}

static void native_put_string(char *s)
{
    if (cur_iosys_mode == 2) glk_put_string(s);
}

static void native_put_string_uni(uint32_t *s)
{
    if (cur_iosys_mode == 2) glk_put_string_uni(s);
}

static void native_put_char(uint32_t ch)
{
    if (cur_iosys_mode == 2) glk_put_char(ch);
}

static void native_put_char_uni(uint32_t ch)
{
    if (cur_iosys_mode == 2) glk_put_char_uni(ch);
}

void native_streamchar(uint32_t ch)
{
    native_put_char(ch);
}

void native_streamnum(int32_t n)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", n);
    native_put_string(buf);
}

/* Stream compressed stream data starting from `offset'. */
static void stream_compressed_string(uint32_t string_offset, uint32_t *sp)
{
    uint32_t value = 0;
    int bits = 0;

    if (cur_decoding_tbl == 0)
    {
        error("streaming a compressed string without a decoding table set");
        return;
    }

    for (;;)
    {
        uint32_t node_offset;  /* offset of next leaf node */

        /* Gather enough bits to look up value in the string table */
        /* Note that this may overflow memory, but that can happen anyway, as
           there is no guarantee that the encoded string is encoded properly. */
        while (bits < strtbl_bits)
        {
            value |= get_byte(string_offset++) << bits;
            bits += 8;
        }

        if (strtbl == NULL)
        {
            /* Look up the next leaf node bit-by-bit */
            node_offset = get_long(cur_decoding_tbl + 8);
            while (get_byte(node_offset) == 0)
            {
                node_offset = get_long(node_offset + ((value&1) ? 5 : 1));
                value >>= 1;
                bits   -= 1;
            }
        }
        else    /* strtbl != NULL */
        {
            /* Look up the next leaf node using the decoding table */
            const uint32_t mask = (1<<strtbl_bits)-1;
            struct StringTableEntry *entry = &strtbl[value&mask];
            node_offset = entry->node_offset;
            bits   -= entry->bits_used;
            value >>= entry->bits_used;
        }

        /* Process next leaf node */
        switch (get_byte(node_offset))
        {
        case 0x01:  /* string terminator */
            return;

        case 0x02:  /* single character */
            native_put_char(get_byte(node_offset + 1));
            break;

        case 0x03:  /* C-style string */
            native_put_string((char*)&mem[node_offset + 1]);
            break;

        case 0x04:  /* Unicode character */
            native_put_char_uni(get_long(node_offset + 1));
            break;

        case 0x05:  /* Unicode string */
            {
                uint32_t *p = native_ustring_dup(node_offset + 1);
                native_put_string_uni(p);
                free(p);
            } break;

        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            {
                uint8_t ref_type = get_byte(node_offset);
                uint32_t obj_offset = get_long(node_offset + 1);

                if (ref_type == 0x09 || ref_type == 0x0b)
                    obj_offset = get_long(obj_offset);

                switch (get_byte(obj_offset))
                {
                case 0xe0:
                case 0xe1:
                case 0xe2:
                    native_streamstr(obj_offset, sp);
                    break;

                case 0xc0:
                case 0xc1:
                    {
                        uint32_t narg, n;

                        /* Determine number of arguments */
                        if (ref_type == 0x0a || ref_type == 0x0b)
                            narg = get_long(node_offset + 5);
                        else
                            narg = 0;

                        /* Push arguments */
                        for (n = 0; n < narg; ++n)
                            sp[n] = get_long(node_offset + 5 + 4*(narg - n));
                        sp[narg] = narg;

                        /* Call function */
                        func(obj_offset)(sp + narg);
                    }
                    break;

                default:
                    error("unsupported object type (%d) in string reference",
                            get_byte(obj_offset));
                }
            } break;

        default:  /* unknown node type */
            error("unsupported node type (%d) in encoded string",
                    get_byte(node_offset));
        }
    }
}

void native_streamstr(uint32_t offset, uint32_t *sp)
{
    switch (mem[offset])
    {
    case 0xe0:  /* unencoded C-string */
        {
            native_put_string((char *)&mem[offset + 1]);
        } break;

    case 0xe2:  /* unencoded unicode string */
        {
            /* We need a temp-string to fix alignment & byte order */
            uint32_t *p = native_ustring_dup(offset + 1);
            native_put_string_uni(p);
            free(p);
        }
        break;

    case 0xe1:  /* compressed string */
        stream_compressed_string(offset + 1, sp);
        break;

    default:
        assert(0);
    }
}

void native_streamunichar(uint32_t ch)
{
    glk_put_char_uni(ch);
}

glui32 *native_ustring_dup(uint32_t offset)
{
    glui32 *res;
    uint32_t i, j, k;

    i = j = offset;
    while (get_long(j) != 0) ++j;
    res = malloc((j - i + 1)*sizeof(glui32));
    assert(res != NULL);
    for (k = i; k <= j; ++k) res[k - i] = get_long(k);
    return res;
}