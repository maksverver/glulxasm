#include "native.h"
#include "storycode.h"
#include <stdbool.h>
#include <assert.h>

/* Serializes the story file state into a long string. */
char *native_serialize(uint32_t *data_sp, struct Context *ctx, size_t *size)
{
    size_t data_stack_size;
    size_t call_stack_size;
    size_t data_size;
    char *data, *pos, *call_sp = (char*)ctx->esp;

    data_stack_size = sizeof(*data_sp)*(data_sp - data_stack);
    call_stack_size = call_stack + init_stack_size - call_sp;

    data_size = init_endmem - init_ramstart;    /* interpreter memory */
    data_size += sizeof(size_t);                /* data stack size */
    data_size += data_stack_size;               /* data stack */
    data_size += sizeof(size_t);                /* call stack size */
    data_size += call_stack_size;               /* call stack */
    data_size += sizeof(ctx);                   /* execution context */

    data = pos = malloc(data_size);
    if (pos != NULL)
    {
        /* interpreter memory */
        memcpy(pos, &mem[init_ramstart], init_endmem - init_ramstart);
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

        /* execution context (assumed to be stored on stack!) */
        assert((size_t)((char*)ctx - call_sp) < call_stack_size);
        memcpy(pos, &ctx, sizeof(ctx));
        pos += sizeof(ctx);
    }
    assert(pos == data + data_size);
    *size = data_size;
    return data;
}

/* Deserialize stored state */
bool native_deserialize(char *data, size_t size)
{
    (void)data;
    (void)size;
    assert(0); /* TODO */
}
