#include "native.h"
#include <math.h>
#include <stdint.h>


static int32_t clamp(float f)
{
    /* Note that f <= INT32_MAX does not work here, because INT32_MAX is not
       a power of two and cannot be represented as a float exactly.  It will
       be rounded up for the comparison, and that means values higher than
       INT32_MAX would slip through!

       f >= INT32_MIN would work, in principle, but handling f == INT32_MIN as
       out-of-bounds works just as well, since the result will be INT32_MIN
       either way.
    */
    if (f > INT32_MIN && f < INT32_MAX)
    {
        return (int32_t)f;
    }
    else  /* out of bounds, infinity, or NaN */
    {
        return signbit(f) ? INT32_MIN : INT32_MAX;
    }
}

int32_t native_ftonumz(float f)
{
    return clamp(truncf(f));
}

int32_t native_ftonumn(float f)
{
    return clamp(roundf(f));
}
