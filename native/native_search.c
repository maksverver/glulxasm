#include "native.h"
#include "inline.h"
#include <stdbool.h>
#include <assert.h>

static INLINE bool is_zero_key(uint32_t offset, uint32_t size)
{
    while (size-- > 0) if (mem[offset++] != 0) return false;
    return true;
}

static INLINE int compare_key(
    uint32_t key, uint32_t key_size, bool key_indirect, uint32_t offset )
{
    if (!key_indirect)
    {
        uint32_t value;
        switch (key_size)
        {
        case 1: return (int)(uint8_t)key  - (int)get_byte(offset);
        case 2: return (int)(uint16_t)key - (int)get_shrt(offset);
        case 4: value = get_long(offset); return key < value ? -1 : key > value;
        }
        assert(0);
    }
    else
    {
        return memcmp(mem + key, mem + offset, key_size);
    }
}

INLINE uint32_t native_linearsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options )
{
    bool key_indirect        = options&0x01;
    bool zero_key_terminates = options&0x02;
    bool return_index        = options&0x04;
    uint32_t offset;
    for (offset = start; num_structs-- > 0; offset += struct_size)
    {
        if (compare_key(key, key_size, key_indirect, offset + key_offset) == 0)
            return return_index ? (offset - start)/struct_size : offset;
        if (zero_key_terminates && is_zero_key(offset + key_offset, key_size))
            break;
    }
    return 0 - return_index;
}

INLINE uint32_t native_binarysearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options )
{
    bool key_indirect = options&0x01;
    bool return_index = options&0x04;
    uint32_t lo = 0, hi = num_structs;
    while (lo < hi)
    {
        uint32_t index = (lo + hi)/2, offset = start + index*struct_size;
        long long i = compare_key(key, key_size, key_indirect, offset + key_offset);
        if (i < 0) hi = index;
        else if (i > 0) lo = index + 1;
        else return return_index ? index : offset;
    }
    return 0 - return_index;
}

INLINE uint32_t native_linkedsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t key_offset,
    uint32_t next_offset, uint32_t options )
{
    bool key_indirect        = options&0x01;
    bool zero_key_terminates = options&0x02;
    uint32_t offset;
    for (offset = start; offset != 0; offset = get_long(offset + next_offset))
    {
        if (compare_key(key, key_size, key_indirect, offset + key_offset) == 0)
            return offset;
        if (zero_key_terminates && is_zero_key(offset + key_offset, key_size))
            break;
    }
    return 0;
}
