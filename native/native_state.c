#include "native.h"
#include "messages.h"
#include "glk.h"
#include "storycode.h"
#include <stdbool.h>
#include <assert.h>

/* Serialized state consists of:

    (init_endmem - init_ramstart) bytes: mem[init_ramstart:init_endmem)
    4 bytes: data stack size, D
    D bytes: data_stack[0:D)
    4 bytes: call stack size, C
    C bytes: call_stack[init_stack_size - C, init_stack_size)
    4 bytes: pointer to execution context

  All values are stored in native byte-order.
*/

static uint32_t cur_protect_offset  = 0;
static uint32_t cur_protect_size    = 0;

void native_protect(uint32_t offset, uint32_t size)
{
    cur_protect_offset = offset;
    cur_protect_size   = size;
}

/* Serializes the story file state into a long string. */
char *native_serialize(uint32_t *data_sp, struct Context *ctx, size_t *size)
{
    const char *call_sp  = (char*)ctx->esp,
               *stack_end = call_stack + init_stack_size;
    uint32_t data_stack_size, call_stack_size;
    size_t data_size;
    char *data, *pos;

    /* Verify stack pointer is valid: */
    assert(call_sp >= call_stack && call_sp < stack_end);

    /* Verify context structure is on the stack: */
    assert((char*)ctx >= call_sp && (char*)(ctx + 1) <= stack_end);

    data_stack_size = sizeof(*data_sp)*(data_sp - data_stack);
    call_stack_size = stack_end - call_sp;

    data_size = init_endmem - init_ramstart;    /* interpreter memory */
    data_size += sizeof(size_t);                /* data stack size */
    data_size += data_stack_size;               /* data stack */
    data_size += sizeof(size_t);                /* call stack size */
    data_size += call_stack_size;               /* call stack */
    data_size += sizeof(ctx);                   /* execution context */

    data = pos = malloc(data_size);
    if (pos == NULL) return NULL;

    /* interpreter memory */
    memcpy(pos, mem + init_ramstart, init_endmem - init_ramstart);
    pos += init_endmem - init_ramstart;

    /* data stack */
    memcpy(pos, &data_stack_size, sizeof(data_stack_size));
    pos += sizeof(data_stack_size);
    memcpy(pos, data_stack, data_stack_size);
    pos += data_stack_size;

    /* call stack */
    memcpy(pos, &call_stack_size, sizeof(call_stack_size));
    pos += sizeof(call_stack_size);
    memcpy(pos, call_sp, call_stack_size);
    pos += call_stack_size;

    /* execution context */
    memcpy(pos, &ctx, sizeof(ctx));
    pos += sizeof(ctx);

    assert(pos == data + data_size);
    *size = data_size;
    return data;
}

/* Deserialize stored state */
struct Context *native_deserialize(char *data, size_t size)
{
    struct Context *ctx;
    uint32_t data_stack_size;
    uint32_t call_stack_size;
    char *pos = data;

    if (cur_protect_offset < init_endmem && cur_protect_size > 0)
    {
        /* Protected memory range not implemented yet! */
        assert(0);
    }

    /* interpreter memory */
    memcpy(mem + init_ramstart, pos, init_endmem - init_ramstart);
    pos += init_endmem - init_ramstart;

    /* data stack */
    memset(data_stack, 0, init_stack_size);  /* for debugging */
    memcpy(&data_stack_size, pos, sizeof data_stack_size);
    pos += sizeof data_stack_size;
    memcpy(data_stack, pos, data_stack_size);
    pos += data_stack_size;

    /* call stack */
    memset(call_stack, 0, init_stack_size);  /* for debugging */
    memcpy(&call_stack_size, pos, sizeof call_stack_size);
    pos += sizeof call_stack_size;
    memcpy(call_stack + init_stack_size - call_stack_size,
           pos, call_stack_size);
    pos += call_stack_size;

    /* execution context (assumed to be stored on stack!) */
    memcpy(&ctx, pos, sizeof ctx);
    pos += sizeof ctx;

    assert(pos == data + size);
    return ctx;
}

static void write_fourcc(strid_t stream, const char *fourcc)
{
    glk_put_char_stream(stream, fourcc[0]);
    glk_put_char_stream(stream, fourcc[1]);
    glk_put_char_stream(stream, fourcc[2]);
    glk_put_char_stream(stream, fourcc[3]);
}

static void write_uint32(strid_t stream, uint32_t value)
{
    glk_put_char_stream(stream, value >> 24 & 0xff);
    glk_put_char_stream(stream, value >> 16 & 0xff);
    glk_put_char_stream(stream, value >>  8 & 0xff);
    glk_put_char_stream(stream, value >>  0 & 0xff);
}

/* Compresses a sequence of 32-bit words (prev) using another sequence (next)
   as a reference.  Guaranteed not to expand the output by more than 1 word
   for every sequence of 65535 words (or part thereof).

   TODO: document encoding used!
*/
static size_t compress_words( uint32_t *prev, size_t prev_size,
                              const uint32_t *next, size_t next_size,
                              uint32_t *dest )
{
    size_t i = 0, j = 0;
    while (i < prev_size)
    {
        size_t copy = 0, add = 0;

        /* Determine how many words to copy: as many as possible! */
        for ( ; copy < 0xffff && i < prev_size &&
                i < next_size && prev[i] == next[i]; ++i) ++copy;

        /* Determine how many words to add: */
        for ( ; add < 0xffff && i < prev_size && (i + 1 >= next_size ||
                prev[i] != next[i] || prev[i + 1] != next[i + 1] ); ++i ) ++add;

        /* Note:
            I could stop adding words as soon as I encounter the first copyable
            word, but I choose to instead continue until I hit at least two
            consecutive copyable words.  This compresses almost as well in
            practice*, but decreases the number of blocks created.

            Exercise for the reader: when does this increase the output size,
            and by how much (in the worst case)?  */

        if (!dest)
        {
            j += 1 + add;
        }
        else
        {
            size_t k = i - add;
            dest[j++] = copy | add << 16;
            while (k < i) dest[j++] = prev[k++];
        }
    }
    return j;
}

static size_t decompress_words( uint32_t *data, size_t data_size,
                                const uint32_t *next, size_t next_size,
                                uint32_t *prev )
{
    size_t i = 0, j = 0;
    while (i < data_size)
    {
        uint32_t info = data[i++];
        size_t copy = info & 0xffff, add = info >> 16;
        assert(i + add <= data_size);
        assert(copy == 0 || j + copy <= next_size);
        if (prev == NULL)
        {
            i += add;
            j += copy + add;
        }
        else
        {
            for ( ; copy-- > 0; ++j) prev[j] = next[j];
            for ( ; add--  > 0; ++j) prev[j] = data[i++];
        }
    }
    return j;
}

/* Compresses a serialized state.  Caller must free() the result! */
char *compress_state( char *prev, size_t prev_size,
                      const char *next, size_t next_size, size_t *size_out )
{
    char *data;
    size_t size;

    /* Pass 1: calculate size of compressed data. */
    size = compress_words( (uint32_t*)prev, prev_size / sizeof(uint32_t),
                     (const uint32_t*)next, next_size / sizeof(uint32_t),
                           NULL );

    /* Allocate required memory */
    size = size*sizeof(uint32_t);
    data = malloc(size);
    if (data == NULL) return NULL;

    /* Pass 2: compress into allocated buffer. */
    compress_words( (uint32_t*)prev, prev_size / sizeof(uint32_t),
              (const uint32_t*)next, next_size / sizeof(uint32_t),
                    (uint32_t*)data );

    *size_out = size;
    return data;
}

/* Decompresses a compressed state.  Caller must free() the result! */
char *decompress_state( char *prev, size_t prev_size,
                        const char *next, size_t next_size, size_t *size_out )
{
    char *data;
    size_t size;

    /* Pass 1: calculate size of decompressed data. */
    size = decompress_words( (uint32_t*)prev, prev_size / sizeof(uint32_t),
                       (const uint32_t*)next, next_size / sizeof(uint32_t),
                             NULL );

    /* Allocate required memory */
    size = size*sizeof(uint32_t);
    data = malloc(size);
    if (data == NULL) return NULL;

    /* Pass 2: decompress into allocated buffer. */
    decompress_words( (uint32_t*)prev, prev_size / sizeof(uint32_t),
                (const uint32_t*)next, next_size / sizeof(uint32_t),
                      (uint32_t*)data );

    *size_out = size;
    return data;
}

extern const uint8_t *glulx_data;
extern size_t         glulx_size;

/* Writes the compressed size of memory data[0:len) when XOR'ed against
   glulx_data[init_ramstart:glulx_size) and compressed as described in the
   Quetzal specification for CMem chunks: non-zero bytes are reproduced verbatim
   while zero-bytes are followed by a single run-length byte (with bias 1).

   If the `stream` parameter is NULL, nothing will be written, but the size of
   the compressed data will still be returned, which is useful to determine the
   space needed before actually writing anything.
*/
static size_t write_cmem(strid_t stream, const uint8_t *data, size_t size)
{
    size_t written = 0, run = 0, i;
    for (i = 0; i < size; ++i)
    {
        uint8_t ch = data[i];
        if (i + init_ramstart < glulx_size)
        {
            ch ^= glulx_data[i + init_ramstart];
        }
        if ((ch == 0 && run == 256) || (ch != 0 && run > 0))
        {
            if (stream)
            {
                glk_put_char_stream(stream, 0);
                glk_put_char_stream(stream, run - 1);
            }
            written += 2;
            run = 0;
        }
        if (ch == 0)
        {
            ++run;
        }
        else
        {
            if (stream)
            {
                glk_put_char_stream(stream, ch);
            }
            written += 1;
        }
    }
    if (run > 0)
    {
        if (stream)
        {
            glk_put_char_stream(stream, 0);
            glk_put_char_stream(stream, run - 1);
        }
        written += 2;
    }
    return written;
}

/* Returns pointer to a 16-byte binary identifier that can be used to verify
   whether savegames are binary compatible with this release.  (In particular,
   the call stack must be allocated to the same address!) */
static const char *get_binary_id(void)
{
    static uint32_t bin_id[4];
    bin_id[0] = (uint32_t)call_stack;
    bin_id[1] = 0;
    bin_id[2] = 0;
    bin_id[3] = 0;
    return (char*)bin_id;
}

void native_save_serialized(const char *data, size_t size, strid_t stream)
{
    const size_t ram_size = init_endmem - init_ramstart;

    write_fourcc(stream, "FORM");
    write_uint32(stream, size + 8*4 + 128);
    write_fourcc(stream, "IFZS");
    write_fourcc(stream, "IFhd");
    write_uint32(stream, 128);
    glk_put_buffer_stream(stream, (char*)mem, 128);

    /*
    write_fourcc(stream, "UMem");
    write_uint32(stream, 4 + ram_size);
    write_uint32(stream, ram_size);
    glk_put_buffer_stream(stream, (char*)data, ram_size);
    */
    write_fourcc(stream, "CMem");
    write_uint32(stream, 4 + write_cmem(NULL, (const uint8_t*)data, ram_size));
    write_uint32(stream, ram_size);
    write_cmem(stream, (const uint8_t*)data, ram_size);
    data += ram_size;
    size -= ram_size;

    write_fourcc(stream, "XStk");
    write_uint32(stream, 16 + size);
    glk_put_buffer_stream(stream, (char*)get_binary_id(), 16);
    glk_put_buffer_stream(stream, (char*)data, size);
}

static uint32_t get_uint32(char buf[4])
{
    unsigned char *p = (unsigned char*)buf;
    return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

char *native_restore_serialized(strid_t stream, size_t *size_out)
{
    const size_t ram_size = init_endmem - init_ramstart;
    char buf[128], *data = NULL;
    size_t size = 0;

    /* Verify file header: */
    if (glk_get_buffer_stream(stream, buf, 12) != 12 ||
        memcmp(buf,     "FORM", 4) != 0 ||
        memcmp(buf + 8, "IFZS", 4) != 0)
    {
        error("wrong file type");
        goto failed;
    }

    /* Allocate memory */
    size = get_uint32(buf + 4);
    if (size < 8*4 + 128 + ram_size) goto failed;
    size -= 8*4 + 128;
    data = malloc(size);
    if (data == NULL) goto failed;

    /* Check IFhd */
    if (glk_get_buffer_stream(stream, buf, 8) != 8 ||
        memcmp(buf, "IFhd", 4) != 0 ||
        get_uint32(buf + 4) != 128 ||
        glk_get_buffer_stream(stream, buf, 128) != 128)
    {
        error("invalid/missing IFhd chunk");
        goto failed;
    }
    if (memcmp(buf, glulx_data, 128) != 0)
    {
        error("wrong story file");
        goto failed;
    }

    /* Read memory chunk */
    if (glk_get_buffer_stream(stream, buf, 12) != 12 ||
        (memcmp(buf, "UMem", 4) != 0 && memcmp(buf, "CMem", 4) != 0) ||
        get_uint32(buf + 8) != ram_size)
    {
        error("invalid/missing UMem/CMem chunk");
        goto failed;
    }
    if (memcmp(buf, "UMem", 4) == 0)
    {
        if (get_uint32(buf + 4) != 4 + ram_size ||
            glk_get_buffer_stream(stream, data, ram_size) != ram_size)
        {
            error("failed to read UMem chunk");
            goto failed;
        }
    }
    else
    if (memcmp(buf, "CMem", 4) == 0)
    {
        size_t csize = get_uint32(buf + 4) - 4, i, j;
        for (i = j = 0; i < csize; ++i)
        {
            int ch = glk_get_char_stream(stream);
            if (ch < 0)
            {
                error("failed to read CMem chunk");
                goto failed;
            }
            if (ch == 0)
            {
                int n = (i + 1 < csize) ? glk_get_char_stream(stream) : -1;
                if (n < 0)
                {
                    error("failed to read CMem chunk");
                    goto failed;
                }
                while (n >= 0 && j < ram_size) --n, data[j++] = 0;
                ++i;
            }
            else
            {
                if (j < ram_size) data[j++] = ch;
            }
        }
        assert(i == csize);
        while (j < ram_size) data[j++] = 0;
        for (j = init_ramstart; j < glulx_size; ++j)
        {
            data[j - init_ramstart] ^= glulx_data[j];
        }
    }

    /* Read XStk chunk */
    if (glk_get_buffer_stream(stream, buf, 24) != 24 ||
        memcmp(buf, "XStk", 4) != 0 ||
        get_uint32(buf + 4) != 16 + size - ram_size ||
        glk_get_buffer_stream(stream, data + ram_size, size - ram_size)
            != size - ram_size)
    {
        error("invalid/missing XStk chunk");
        goto failed;
    }
    if (memcmp(buf + 8, get_binary_id(), 16) != 0)
    {
        error("incompatible binary version");
        goto failed;
    }
    *size_out = size;
    return data;

failed:
    if (data != NULL) free(data);
    return NULL;
}
