#include "native.h"
#include "storycode.h"
#include <stdbool.h>
#include <assert.h>

/* Serializes the story file state into a long string. */
char *native_serialize(uint32_t *data_sp, char *call_sp, size_t *size)
{
    size_t data_stack_size;
    size_t call_stack_size;
    size_t data_size;
    char *data, *pos;

    data_stack_size = sizeof(*data_sp)*(data_sp - data_stack);
    call_stack_size = call_stack + init_stack_size - call_sp;

    data_size = init_endmem - init_ramstart;    /* interpreter memory */
    data_size += sizeof(size_t);                /* data stack size */
    data_size += data_stack_size;               /* data stack */
    data_size += sizeof(size_t);                /* call stack size */
    data_size += call_stack_size;               /* call stack */
    data_size += 2*sizeof(struct Context*);     /* start/stop points */

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
        memcpy(pos, call_stack + init_stack_size - call_stack_size,
                    call_stack_size);
        pos += call_stack_size;

        /* start point */
        memcpy(pos, &story_start, sizeof(story_start));
        pos += sizeof(story_start);

        /* stop point */
        memcpy(pos, &story_stop, sizeof(story_stop));
        pos += sizeof(story_stop);
    }
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
