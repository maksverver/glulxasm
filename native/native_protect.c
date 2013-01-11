#include "native.h"
#include "storycode.h"
#include <assert.h>

static uint32_t cur_protect_offset  = 0;
static uint32_t cur_protect_size    = 0;

void native_protect(uint32_t offset, uint32_t size)
{
    cur_protect_offset = offset;
    cur_protect_size   = size;
}

static void *data = NULL;
static size_t offset, size;

void push_protected()
{
    assert(data == NULL);
    if (cur_protect_offset < init_endmem && cur_protect_size > 0)
    {
        offset = cur_protect_offset;
        size   = cur_protect_size;
        if (init_endmem - cur_protect_offset < size)
        {
            size = init_endmem - cur_protect_offset;
        }
        data = malloc(size);
        assert(data != NULL);
        memcpy(data, &mem[offset], size);
    }
}

void pop_protected()
{
    if (data != NULL)
    {
        memcpy(&mem[offset], data, size);
        data = NULL;
    }
}
