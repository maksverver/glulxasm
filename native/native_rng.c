#include "native.h"
#include "inline.h"
#include <time.h>

/* Implements a simple multiply-with-carry pseudo-RNG. */
static uint32_t rng_base  = 0;
static uint32_t rng_carry = 0;

int32_t native_random(int32_t range)
{
    uint64_t tmp;

    /* Check if RNG has been seeded: */
    if (rng_base == 0 && rng_carry == 0)
    {
        /* Seed "randomly" based on current time. */
        rng_base = (uint32_t)time(NULL);
        if (rng_base == 0) rng_base = 1;
    }

    /* Update RNG state. */
    tmp = 1554115554ull*rng_base + rng_carry;
    rng_base  = tmp;
    rng_carry = tmp >> 32;

    /* Return number in requested range: */
    if (range > 0) return +(int32_t)(rng_base % (uint32_t)+range);
    if (range < 0) return -(int32_t)(rng_base % (uint32_t)-range);
    return (int32_t)rng_base;
}

void native_setrandom(uint32_t seed)
{
    rng_base  = seed;
    rng_carry = 0;
}
