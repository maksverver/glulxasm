#include "native.h"
#include "inline.h"
#include <stdbool.h>
#include <assert.h>


static INLINE bool check_search_params(
    uint32_t key, uint32_t key_size, uint32_t start,
    uint32_t struct_size, uint32_t num_structs, uint32_t key_off,
    bool key_indirect, uint32_t *end )
{
    const uint32_t end_mem = native_getmemsize();

    if (key_off > struct_size || struct_size - key_off < key_size ||
        (key_indirect && (key > end_mem || end_mem - key < key_size)))
    {
        return false;
    }

    /* Calculate end offset */
    uint32_t max_structs = (end_mem - start)/struct_size;
    if (num_structs > max_structs) num_structs = max_structs;
    *end = start + num_structs*struct_size;
    return true;
}

static INLINE bool zero_key(uint32_t offset, uint32_t size)
{
    while (size-- > 0) if (mem[offset++] != 0) return false;
    return true;
}

static INLINE int compare_key(
    uint32_t key, uint32_t key_size, bool key_indirect, uint32_t offset )
{
    if (key_indirect)
    {
        return memcmp(mem + key, mem + offset, key_size);
    }
    else  /* key direct */
    {
        switch (key_size)
        {
        case 1: return get_byte(offset) - (uint8_t)key;
        case 2: return get_shrt(offset) - (uint16_t)key;
        case 4: return get_long(offset) - key;
        }
        assert(0);
    }
}

static INLINE uint32_t search_failure(bool return_index)
{
    return 0 - return_index;
}

static INLINE uint32_t search_success(
    uint32_t start, uint32_t struct_size, uint32_t offset, bool return_index )
{
    return return_index ? (offset - start)/struct_size : offset;
}

uint32_t native_linearsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_off, uint32_t options )
{
    const bool key_indirect        = options&0x01;
    const bool zero_key_terminates = options&0x02;
    const bool return_index        = options&0x04;
    uint32_t offset, end;

    if (!check_search_params( key, key_size, start,
                              struct_size, num_structs,
                              key_off, key_indirect, &end ) )
    {
        return search_failure(return_index);
    }

    /* Linear search */
    for (offset = start; offset < end; offset += struct_size)
    {
        if (compare_key(key, key_size, key_indirect, offset + key_off) == 0)
            break;
        if (zero_key_terminates && zero_key(offset + key_off, key_size))
            return search_failure(return_index);
    }

    if (offset >= end) return search_failure(return_index);
    return search_success(start, struct_size, offset, return_index);
}

uint32_t native_binarysearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_off, uint32_t options )
{
    const bool key_indirect        = options&0x01;
    const bool zero_key_terminates = options&0x02;
    const bool return_index        = options&0x04;
    uint32_t lo = 0, hi = num_structs;

    /* printf("binary search (key=0x%08x, key_size=%d, start=0x%08x, "
           "struct_size=%d, num_structs=%d, key_off=%d, options=%d)\n",
           key, key_size, start, struct_size, num_structs, key_off, options); */

    /* The Glulxe standard isn't clear on how the zero_key_terminates options
       works for binary search, so disallow it for the moment:*/
    /* assert(!zero_key_terminates); */
    (void)zero_key_terminates;

    #define binsearch_direct(type,get_type)                                    \
    {                                                                          \
        type a = key_indirect ? get_type(key) : (type)key;                     \
        while (lo < hi)                                                        \
        {                                                                      \
            uint32_t mid = (lo + hi)/2, off = start + mid*struct_size;         \
            type b = get_type(off + key_off);                                  \
            if (a < b) hi = mid;                                               \
            else if (a > b) lo = mid + 1;                                      \
            else return search_success(start, struct_size, off, return_index); \
        }                                                                      \
        return search_failure(return_index);                                   \
    }

    /* Binary search */
    switch (key_size)
    {
    case 1: binsearch_direct(uint8_t,  get_byte)
    case 2: binsearch_direct(uint16_t, get_shrt)
    case 4: binsearch_direct(uint32_t, get_long)
    default:
        /* key_indirect should be true */
        while (lo < hi)
        {
            uint32_t mid = (lo + hi)/2, off = start + mid*struct_size;
            int d = memcmp(&mem[key], &mem[off + key_off], key_size);
            if (d < 0) hi = mid;
            else if (d > 0) lo = mid + 1;
            else return search_success(start, struct_size, off, return_index);
        }
        return search_failure(return_index);
    }
    #undef binsearch_direct
}


uint32_t native_linkedsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t key_off,
    uint32_t next_offset, uint32_t options )
{
    /* not implemented */
    (void)key;
    (void)key_size;
    (void)start;
    (void)key_off;
    (void)next_offset;
    (void)options;
    assert(0);
    return 0;
}
