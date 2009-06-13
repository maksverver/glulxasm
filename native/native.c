#include "native.h"
#include "glkop.h"
#include "storyfile.h"
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#ifndef INLINE
#define INLINE inline
#endif /* ndef INLINE */

/* Defined in glulxe.c; reused here: */
extern char *make_temp_string(glui32 addr);
extern glui32 *make_temp_ustring(glui32 addr);
extern void free_temp_string(char *str);
extern void free_temp_ustring(glui32 *str);

static uint32_t cur_iosys = 0;
static uint32_t cur_decoding_tbl = 0;

/* The string table is encoded in the story file as a Huffman tree. When it
   is loaded, it is converted to a look-up table of size 2**H, where H is the
   height of the tree, where we can look up multiple bits at once. Each lookup
   results in one node being selected and a number of bits being shifted.

   Note that building the look-up table (and therefore the setstringtbl opcode)
   is relatively slow, so games shouldn't do that often!
*/
int strtbl_bits = 0;
static struct StringTableEntry
{
    int      bits_used;
    uint32_t node_offset;
} *strtbl = NULL;

uint32_t *glk_stack_ptr = NULL;

#define get_byte(a) (*(uint8_t*)&mem[a])
#define get_shrt(a) (ntohs(*(uint16_t*)&mem[a]))
#define get_long(a) (ntohl(*(uint32_t*)&mem[a]))

void native_accelfunc(uint32_t a1, uint32_t a2)
{
    /* not implemented */
    (void)a1;
    (void)a2;
}

void native_accelparam(uint32_t a1, uint32_t a2)
{
    /* not implemented */
    (void)a1;
    (void)a2;
}

static INLINE bool check_search_params(
    uint32_t key, uint32_t key_size, uint32_t start,
    uint32_t struct_size, uint32_t num_structs, uint32_t key_offset,
    bool key_indirect, uint32_t *end )
{
    const uint32_t end_mem = native_getmemsize();

    if (key_offset > struct_size || struct_size - key_offset < key_size ||
        (key_indirect && (key > end_mem || end_mem - key > key_size)))
    {
        return false;
    }

    /* Calculate end offset */
    uint32_t max_structs = (end_mem - start)/struct_size;
    if (max_structs > num_structs) max_structs = num_structs;
    *end = start + num_structs*struct_size;
    return true;
}

static INLINE bool zero_key(uint32_t offset, uint32_t size)
{
    while (size-- > 0) if (mem[offset++] != 0) return false;
    return true;
}


static INLINE int compare_key(
    uint32_t key, uint32_t key_size, uint32_t offset,
    uint32_t key_offset, bool key_indirect )
{
    if (key_indirect)
    {
        return memcmp(&mem[key], &mem[offset + key_offset], key_size);
    }
    else  /* key direct */
    {
        uint32_t val;
        switch (key_size)
        {
        case 1: val = get_byte(offset + key_offset); break;
        case 2: val = get_shrt(offset + key_offset); break;
        case 4: val = get_long(offset + key_offset); break;
        default: return -1;
        }
        if (key < val) return -1;
        if (key > val) return +1;
        return 0;
    }
}

static INLINE uint32_t search_failure(bool return_index)
{
    return return_index ? (uint32_t)-1 : 0;
}

static INLINE uint32_t search_success(
    uint32_t start, uint32_t struct_size, uint32_t offset, bool return_index )
{
    return return_index ? (offset - start)/struct_size : offset;
}

uint32_t native_linearsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options )
{
    const bool key_indirect        = options&0x01;
    const bool zero_key_terminates = options&0x02;
    const bool return_index        = options&0x04;
    uint32_t offset, end;

    if (!check_search_params( key, key_size, start,
                              struct_size, num_structs,
                              key_offset, key_indirect, &end ) )
    {
        return search_failure(return_index);
    }

    /* Linear search */
    for (offset = start; offset < end; offset += struct_size)
    {
        if (compare_key(key, key_size, offset, key_offset, key_indirect) == 0)
            break;
        if (zero_key_terminates && zero_key(offset, key_size))
            break;
    }

    if (offset >= end) return search_failure(return_index);
    return search_success(start, struct_size, offset, return_index);
}

uint32_t native_binarysearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options )
{
    const bool key_indirect        = options&0x01;
    const bool zero_key_terminates = options&0x02;
    const bool return_index        = options&0x04;
    uint32_t end;

    /* The Glulxe standard isn't clear on what this options does for binary
       searches; I choose to ignore it entirely. */
    (void)zero_key_terminates;

    if (!check_search_params( key, key_size, start, struct_size, num_structs,
                              key_offset, key_indirect, &end ) )
    {
        return search_failure(return_index);
    }

    /* Binary search */
    {
        uint32_t lo = 0, hi = (end - start)/struct_size;
        while (lo < hi)
        {
            uint32_t mid = (lo + hi)/2, off = start + mid*struct_size;
            int d = compare_key(key, key_size, off, key_offset, key_indirect);
            if (d < 0) hi = mid;
            else if (d > 0) lo = mid + 1;
            else return search_success(start, struct_size, off, return_index);
        }
        return search_failure(return_index);
    }

}

uint32_t native_linkedsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t key_offset,
    uint32_t next_offset, uint32_t options )
{
    /* not implemented */
    (void)key;
    (void)key_size;
    (void)start;
    (void)key_offset;
    (void)next_offset;
    (void)options;
    assert(0);
    return 0;
}

uint32_t native_gestalt(uint32_t selector, uint32_t argument)
{
    /* Note that most of these check for support of opcodes, not necessarily
       functionality! e.g. we support the accelfunc and accelparam opcodes, but
       we don't accelerate any functions. */

    switch (selector)
    {
    case 0:  /* GlulxVersion */
        return 0x00030101;

    case 1:  /* TerpVersion */
        return 0x00000000;  /* FIXME */

    case 2:  /* ResizeMem */
        return 0;  /* cannot resize memory */

    case 3:  /* Undo */
        return 0;  /* FIXME: cannot undo */

    case 4:  /* IOSystem */
        switch (argument)
        {
        case 0: return 1;  /* null */
        case 1: return 0;  /* filter */
        case 2: return 1;  /* glk */
        default: return 0;  /* others? */
        }

    case 5:  /* Unicode */
        return 1; /* unicode operations/strings supported */

    case 6:  /* MemCopy */
        return 1; /* mzero/mcopy supported */

    case 7: /* MAlloc */
        return 1;  /* malloc/mfree opcodes supported */

    case 8: /* MAllocHeap */
        return 0;  /* offset of heap, or 0 if not used */

    case 9: /* Accelleration */
        return 1;  /* accelfunc/accelparam opcodes supported */

    case 10: /* AccelFunc */
        switch (argument)
        {
            default: return 0;  /* unknown function */
        }

    default: /* unknown gestalt selector */
        return 0;
    }
}

uint32_t native_getmemsize()
{
    return init_endmem;
}

uint32_t native_getstringtbl()
{
    return cur_decoding_tbl;
}

uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp)
{
    uint32_t args[32], n;

    assert(narg <= 32);

    for (n = 0; n < narg; ++n)
        args[n] = *--*sp;

    return perform_glk(selector, narg, args);
}

uint32_t native_malloc(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 0;
}

void native_mfree(uint32_t a1)
{
    /* not implemented */
    (void)a1;
}

void native_quit()
{
    printf("native_quit()");
    exit(0);
}

int32_t native_random(int32_t a1)
{
    /* Dirty, but works well: */
    assert(RAND_MAX >= 256);
    uint32_t roll = random();
    roll ^= random() <<  8;
    roll ^= random() << 16;
    roll ^= random() << 24;

    if (a1 > 0)
        return (int32_t)(roll%a1);

    if ((uint32_t)a1 == 0x80000000)
        return (int32_t)(roll|0x80000000u);

    if (a1 < 0)
        return -(int32_t)(roll%-a1);

    return (int32_t)roll;
}

void native_restart()
{
    printf("native_restart()");
}

uint32_t native_restore(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 1; /* indicates failure! */
}

uint32_t native_restoreundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

uint32_t native_save(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 1; /* indicates failure! */
}

uint32_t native_saveundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

void native_setiosys(uint32_t a1, uint32_t a2)
{
    switch (a1)
    {
    case 0:
    default:
        cur_iosys = 0;  /* null iosys */
        break;

    case 2:
        (void)a2;  /* filtering not implemented yet */
        cur_iosys = 2;
        break;
    }
}

void native_setrandom(uint32_t a1)
{
    srandom(a1);
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
        build_string_table(get_long(offset + 1), value, bits_used + 1);
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
        strtbl_bits = 0;
    }

    if (offset != 0)
    {
        uint32_t root = get_long(offset + 8);
        strtbl_bits = get_tree_height(root);
        assert(strtbl_bits <= 20);  /* NB: this should be at most 24! */
        strtbl = malloc((1<<strtbl_bits)*sizeof(*strtbl));
        assert(strtbl != NULL);
        build_string_table(root, 0, 0);
    }
}

void native_stkroll(uint32_t size, int32_t steps, uint32_t *sp)
{
    /* FIXME: there should be a more efficient implementation for this */

    uint32_t *copy, i, j;

    if (size == 0 || steps == 0) return;

    steps %= size;
    if (steps < 0) steps = size - steps;

    sp -= size;
    copy = alloca(sizeof(uint32_t)*size);
    memcpy(copy, sp, sizeof(uint32_t)*size);

    i = 0;
    j = steps;
    while (i < size)
    {
        sp[j++] = copy[i++];
        if (j == size) j = 0;
    }
}

void native_streamchar(uint32_t ch)
{
    glk_put_char(ch);
}

void native_streamnum(int32_t n)
{
    char buf[12];
    snprintf(buf, sizeof(buf), "%d", n);
    glk_put_string(buf);
}

/* Stream compressed stream data starting from `offset'. */
static void stream_compressed_string(uint32_t offset)
{
    const uint32_t mask = (1<<strtbl_bits)-1;
    uint32_t value = 0;
    int bits = 0;

    if (strtbl == NULL) return;

    for (;;)
    {
        /* Gather enough bits to look up value in the string table */
        /* Note that this may overflow memory, but that can happen anyway, as
           there is no guarantee that the encoded string is encoded properly. */
        while (bits < strtbl_bits)
        {
            value |= get_byte(offset++) << bits;
            bits += 8;
        }

        /* Process next leaf node */
        {
            struct StringTableEntry *entry = &strtbl[value&mask];
            uint32_t node_offset = entry->node_offset;
            printf("a: bits=%d value=%08x bits_used=%d\n", bits, value, entry->bits_used);
            bits  -= entry->bits_used;
            value >>= entry->bits_used;
            printf("b: bits=%d value=%08x bits_used=%d\n", bits, value, entry->bits_used);

            switch (get_byte(node_offset))
            {
            case 0x01:  /* string terminator */
                return;

            case 0x02:  /* single character */
                native_streamchar(get_byte(node_offset + 1));
                break;

            case 0x03:  /* C-style string */
                glk_put_string((char*)&mem[node_offset + 1]);
                break;

            /* TODO: other string types */

            default:  /* unknown node type */
                assert(0);
            }
        }
    }
}

void native_streamstr(uint32_t offset)
{
    switch (mem[offset])
    {
    case 0xe0:  /* unencoded C-string */
        {
            char *p = make_temp_string(offset);
            glk_put_string(p);
            free_temp_string(p);
        } break;

    case 0xe2:  /* unencoded unicode string */
        {
            /* We need a temp-string to fix alignment & byte order */
            uint32_t *p = make_temp_ustring(offset);
            glk_put_string_uni(p);
            free_temp_ustring(p);
        }
        break;

    case 0xe1:  /* compressed string */
        stream_compressed_string(offset + 1);
        break;

    default:
        assert(0);
    }
}

uint32_t native_verify()
{
    /* no need to do anything; game was verified on start */
    return 0;  /* indicate successs */
}
