#include "native.h"
#include "inline.h"
#include "messages.h"
#include "storyfile.h"
#include "glkop.h"  /* for glk_exit() */
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

static uint32_t cur_protect_offset  = 0;
static uint32_t cur_protect_size    = 0;
static uint32_t cur_rng_base        = 0;
static uint32_t cur_rng_carry       = 0;

void native_accelfunc(uint32_t l1, uint32_t l2)
{
    /* not implemented */
    (void)l1;
    (void)l2;
}

void native_accelparam(uint32_t l1, uint32_t l2)
{
    /* not implemented */
    (void)l1;
    (void)l2;
}

void native_debugtrap(uint32_t argument)
{
    (void)argument;
    abort();
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
        return (NATIVE_VERSION_MAJOR<<16) |
               (NATIVE_VERSION_MINOR<< 8) |
               (NATIVE_VERSION_REVIS<< 0);

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

void native_invalidop(uint32_t offset, const char *descr)
{
    error("unsupported operation at offset 0x%08x: %s", offset, descr);
}

uint32_t native_malloc(uint32_t size)
{
    /* not implemented */
    (void)size;
    return 0;
}

void native_mfree(uint32_t offset)
{
    /* not implemented */
    (void)offset;
}

void native_protect(uint32_t offset, uint32_t size)
{
    cur_protect_offset = offset;
    cur_protect_size   = size;
}

void native_quit()
{
    glk_exit();
}

static INLINE uint32_t rng_next()
{
    uint64_t tmp = 1554115554ull*cur_rng_base + cur_rng_carry;
    cur_rng_base  = tmp;
    cur_rng_carry = tmp >> 32;
    return cur_rng_base;
}

int32_t native_random(int32_t range)
{
    uint32_t roll = rng_next();
    if (range > 0) return +(int32_t)(roll%(uint32_t)+range);
    if (range < 0) return -(int32_t)(roll%(uint32_t)-range);
    return roll;
}

void native_reset()
{
    extern const uint8_t *glulx_data;
    extern size_t         glulx_size;

    /* Copy initialized data section: */
    if (glulx_size >= init_extstart)
    {
        memcpy(mem, glulx_data, init_extstart);
        memset(mem + init_extstart, 0, init_endmem - init_extstart);
    }
    else /* glulx_size < init_extstart */
    {
        /* this shouldn't normally happen! */
        error("story data is truncated");
        memcpy(mem, glulx_data, glulx_size);
        memset(mem + glulx_size, 0, init_endmem - glulx_size);
    }

    if (get_long(32) != init_checksum)
        fatal("checksum mismatch between story data and story code");

    /* Reset RNG */
    native_setrandom(0);

    /* Reset string decoding table */
    native_setstringtbl(init_decoding_tbl);

    /* Clear stack (not really necessary, though nice for debugging) */
    memset(stack, 0, init_stack_size);
}

void native_restart()
{
    /* not implemented */
    error("restart not supported");
}

uint32_t native_restore(uint32_t stream)
{
    /* not implemented */
    (void)stream;
    return 1; /* indicates failure! */
}

uint32_t native_restoreundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

uint32_t native_save(uint32_t stream)
{
    /* not implemented */
    (void)stream;
    return 1; /* indicates failure! */
}

uint32_t native_saveundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

void native_start()
{
    stack[0] = 0;
    func(init_start_func)(&stack[0]);
}

uint32_t native_setmemsize(uint32_t new_size)
{
    /* not implemented */
    (void)new_size;
    return 1; /* indicates failure! */
}

void native_setrandom(uint32_t seed)
{
    cur_rng_base  = (seed != 0) ? seed : time(NULL);
    cur_rng_carry = 0;
}

void native_stkroll(uint32_t size, int32_t steps, uint32_t *sp)
{
    uint32_t *tmp, i, j;

    /* NOTE: we assume size < 0x80000000 here! */
    if (size == 0) return;
    steps %= (int32_t)size;
    if (steps == 0) return;
    if (steps < 0) steps += size;

    /* Move elements from the stack to a temporary buffer */
    sp -= size;
    tmp = alloca(sizeof(uint32_t)*size);
    memcpy(tmp, sp, sizeof(uint32_t)*size);

    /* Copy elements from temporary buffer to stack in the right order */
    i = 0;
    j = steps;
    while (i < size)
    {
        sp[j++] = tmp[i++];
        if (j == size) j = 0;
    }
}

uint32_t native_verify()
{
    /* no need to do anything; game was verified on start */
    return 0;  /* indicate successs */
}
