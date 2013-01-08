#include "native.h"
#include "glk.h"
#include "storycode.h"
#include <stdbool.h>
#include <assert.h>

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
    size_t data_stack_size;
    size_t call_stack_size;
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

void native_save_serialized(const char *data, size_t size, strid_t stream)
{
    size_t ram_size = init_endmem - init_ramstart;

    write_fourcc(stream, "FORM");
    write_uint32(stream, size + 8*4 + 128);
    write_fourcc(stream, "IFZS");
    write_fourcc(stream, "IFhd");
    write_uint32(stream, 128);
    glk_put_buffer_stream(stream, (char*)mem, 128);
    write_fourcc(stream, "UMem");
    write_uint32(stream, 4 + ram_size);
    write_uint32(stream, ram_size);
    glk_put_buffer_stream(stream, (char*)data, ram_size);
    data += ram_size;
    size -= ram_size;
    write_fourcc(stream, "XStk");
    write_uint32(stream, size);
    glk_put_buffer_stream(stream, (char*)data, size);
}
