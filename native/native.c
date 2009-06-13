#include "native.h"
#include "glkop.h"
#include "storyfile.h"
#include <assert.h>
#include <stdio.h>

/* Defined in glulxe.c; reused here: */
extern char *make_temp_string(glui32 addr);
extern glui32 *make_temp_ustring(glui32 addr);
extern void free_temp_string(char *str);
extern void free_temp_ustring(glui32 *str);

static uint32_t cur_iosys = 0;
static uint32_t cur_decoding_tbl = 0;

uint32_t *glk_stack_ptr = NULL;


void native_accelfunc(uint32_t a1, uint32_t a2)
{
    /* not implemented */
    (void)a1;
    (void)a2;
}

void native_accelparam(uint32_t a1, uint32_t a2)
{
    /* not implemented */
    (void)a1;
    (void)a2;
}

uint32_t native_binarysearch(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7)
{
    /* TODO! */
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;
    (void)a7;
    assert(0);
    return 0;
}

uint32_t native_gestalt(uint32_t a1, uint32_t a2)
{
    /* Note that most of these check for support of opcodes, not necessarily
       functionality! e.g. we support the accelfunc and accelparam opcodes, but
       we don't accelerate any functions. */

    switch (a1)
    {
    case 0:  /* GlulxVersion */
        return 0x00030101;

    case 1:  /* TerpVersion */
        return 0x00000001;

    case 2:  /* ResizeMem */
        return 0;

    case 3:  /* Undo */
        return 0;

    case 4:  /* IOSystem */
        switch (a2)
        {
        case 0: return 1;  /* null */
        case 1: return 0;  /* filter */
        case 2: return 1;  /* glk */
        default: return 0;  /* others? */
        }

    case 5:  /* Unicode */
        return 1;

    case 6:  /* MemCopy */
        return 1;

    case 7: /* MAlloc */
        return 1;

    case 8: /* MAllocHeap */
        return 1;

    case 9: /* Accelleration */
        return 1;

    case 10: /* AccelFunc */
        switch (a2)
        {
            default: return 0;  /* unknown function */
        }

    default:
        return 0;
    }
}

uint32_t native_getmemsize()
{
    return init_endmem;
}

uint32_t native_getstringtbl()
{
    return cur_decoding_tbl;
}

uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp)
{
    uint32_t args[32], n;

    assert(narg <= 32);

    for (n = 0; n < narg; ++n)
        args[n] = *--*sp;

    return perform_glk(selector, narg, args);
}

uint32_t native_malloc(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 0;
}

void native_mfree(uint32_t a1)
{
    /* not implemented */
    (void)a1;
}

void native_quit()
{
    printf("native_quit()");
    exit(0);
}

int32_t native_random(int32_t a1)
{
    /* Dirty, but works well: */
    assert(RAND_MAX >= 256);
    uint32_t roll = random();
    roll ^= random() <<  8;
    roll ^= random() << 16;
    roll ^= random() << 24;

    if (a1 > 0)
        return (int32_t)(roll%a1);

    if ((uint32_t)a1 == 0x80000000)
        return (int32_t)(roll|0x80000000u);

    if (a1 < 0)
        return -(int32_t)(roll%-a1);

    return (int32_t)roll;
}

void native_restart()
{
    printf("native_restart()");
}

uint32_t native_restore(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 1; /* indicates failure! */
}

uint32_t native_restoreundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

uint32_t native_save(uint32_t a1)
{
    /* not implemented */
    (void)a1;
    return 1; /* indicates failure! */
}

uint32_t native_saveundo()
{
    /* not implemented */
    return 1; /* indicates failure! */
}

void native_setiosys(uint32_t a1, uint32_t a2)
{
    switch (a1)
    {
    case 0:
    default:
        cur_iosys = 0;  /* null iosys */
        break;

    case 2:
        (void)a2;  /* filtering not implemented yet */
        cur_iosys = 2;
        break;
    }
}

void native_setrandom(uint32_t a1)
{
    srandom(a1);
}

void native_setstringtbl(uint32_t offset)
{
    cur_decoding_tbl = offset;
}

void native_stkcopy(uint32_t a1)
{
    assert(0);
    /* TODO! */
}

void native_stkroll(uint32_t a1, uint32_t a2)
{
    assert(0);
    /* TODO! */
}

void native_streamchar(uint32_t ch)
{
    glk_put_char(ch);
}

void native_streamnum(int32_t n)
{
    char buf[12], *p = buf;

    snprintf(buf, sizeof(buf), "%d", n);
    while (*p) glk_put_char(*p++);
}

void native_streamstr(uint32_t offset)
{
    switch (mem[offset])
    {
    case 0xe0:  /* unencoded C-string */
        {
            char *p = make_temp_string(offset);
            glk_put_string(p);
            free_temp_string(p);
        } break;

    case 0xe2:  /* unencoded unicode string */
        {
            uint32_t *p = make_temp_ustring(offset);
            glk_put_string_uni(p);
            free_temp_ustring(p);
        }
        break;

    case 0xe1:  /* compressed string */
        printf("TODO: print compressed string!\n");
        break;
    }
}

void native_verify()
{
    /* no need to do anything; game was verified on start */
}
