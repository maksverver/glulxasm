#include "native.h"
#include "messages.h"
#include "storycode.h"
#include "glulxe.h"
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <string.h>

enum StorySignal {
    SIGNAL_RESTART = 1,
    SIGNAL_QUIT    = 2,
    SIGNAL_UNDO    = 3,
    SIGNAL_RESTORE = 4 };

struct Undo {
    struct Undo *previous;
    char *data;
    size_t size;
};

static struct Context *story_start  = NULL;
static struct Undo *undo = NULL;
static char *restore_data = NULL;
static size_t restore_size = 0;

/* Defined in native_state */
char *native_serialize(uint32_t *data_sp, struct Context *ctx, size_t *size);
struct Context *native_deserialize(char *data, size_t size);
void native_save_serialized(const char *data, size_t size, strid_t stream);
char *native_restore_serialized(strid_t stream, size_t *size);
char *compress_state( char *prev, size_t prev_size,
                      const char *next, size_t next_size, size_t *size_out );
char *decompress_state( char *prev, size_t prev_size,
                        const char *next, size_t next_size, size_t *size_out );


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

volatile uint32_t debug_arg;

void native_debugtrap(uint32_t argument)
{
    /* Ensure argument is stored somewhere: */
    debug_arg = argument;
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
        return 1;

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

    case 11: /* Float */
        return 1;  /* floating point opcodes supported */

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
    /* intentionally not implemented */
    (void)size;
    return 0;
}

void native_mfree(uint32_t offset)
{
    /* intentionally not implemented */
    (void)offset;
}

void native_quit()
{
    /* Signal quit */
    context_restore(story_start, (void*)SIGNAL_QUIT);
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

    /* Reset string decoding table */
    native_setstringtbl(init_decoding_tbl);

    /* Clear stack (not really necessary, though nice for debugging) */
    memset(data_stack, 0, init_stack_size);
    memset(call_stack, 0, init_stack_size);

    info("machine reset");
}

void native_restart()
{
    /* Signal restart */
    context_restore(story_start, (void*)SIGNAL_RESTART);
}

uint32_t native_restoreundo()
{
    /* Signal undo */
    if (undo == NULL) return 1;  /* error: no undo data available */
    context_restore(story_start, (void*)SIGNAL_UNDO);  /* should not return */
    return 1; /* indicates failure! */
}

uint32_t native_restore(uint32_t stream_id)
{
    strid_t stream = find_stream_by_id(stream_id);
    assert(restore_data == NULL);
    if (!stream) return 1;  /* indicates failure! */
    restore_data = native_restore_serialized(stream, &restore_size);
    if (restore_data == NULL) return 1;  /* indicates failure! */
    context_restore(story_start, (void*)SIGNAL_RESTORE); /* does not return */
    return 1;  /* indicates failure! */
}

uint32_t native_save(uint32_t stream_id, uint32_t *data_sp, struct Context *ctx)
{
    strid_t stream = find_stream_by_id(stream_id);
    size_t size;
    char *data;
    if (!stream) return 1;  /* indicates failure! */
    data = native_serialize(data_sp, ctx, &size);
    if (data == NULL) return 1;  /* indicates failure! */
    native_save_serialized(data, size, stream);
    free(data);
    return 0;  /* indicates success */
}

uint32_t native_saveundo(uint32_t *data_sp, struct Context *ctx)
{
    struct Undo *entry = malloc(sizeof *entry);
    if (entry == NULL) goto failed;
    entry->previous = undo;
    entry->data = native_serialize(data_sp, ctx, &entry->size);
    if (entry->data == NULL) goto failed;
    if (!undo)
    {
        info("undo state serialized to %d bytes", (int)entry->size);
    }
    else
    {
        size_t csize;
        char *cdata;
        cdata = compress_state(undo->data, undo->size,
                            entry->data, entry->size, &csize);
        assert(cdata != NULL);
        info("undo state compressed from %d to %d bytes",
            (int)entry->size, (int)csize);
        free(undo->data);
        undo->data = cdata;
        undo->size = csize;
    }
    undo = entry;
    return 0;
failed:
    if (entry != NULL) free(entry);
    return 1;
}

static void *start(void *arg)
{
    void *res;
    struct Context ctx;

    (void)arg;  /* unused */
    story_start = &ctx;
    res = context_save(story_start);
    if (res == NULL)
    {
        /* Invoke start function */
        data_stack[0] = 0;
        func(init_start_func)(&data_stack[0]);
    }
    return res;
}

static int pop_undo_state()
{
    struct Context *ctx;
    struct Undo *u = undo;

    assert(u != NULL);
    native_reset();
    ctx = native_deserialize(u->data, u->size);
    assert(ctx != NULL);
    undo = u->previous;
    if (undo != NULL)
    {
        size_t size;
        char *data;
        data = decompress_state(undo->data, undo->size,
                                u->data, u->size, &size);
        assert(data != NULL);
        free(undo->data);
        undo->data = data;
        undo->size = size;
    }
    free(u->data);
    free(u);
    return (int)context_restart(call_stack, init_stack_size, ctx, (void*)-1);
}

static int restore_state()
{
    struct Context *ctx;

    native_reset();
    ctx = native_deserialize(restore_data, restore_size);
    assert(ctx != NULL);
    free(restore_data);
    restore_data = NULL;
    restore_size = 0;
    return (int)context_restart(call_stack, init_stack_size, ctx, (void*)-1);
}

void native_start()
{
    int sig = SIGNAL_RESTART;
    for (;;)
    {
        switch (sig)
        {
        case 0:
        case SIGNAL_QUIT:
            return;

        case SIGNAL_RESTART:
            native_reset();
            sig = (int)context_start(call_stack, init_stack_size, start, NULL);
            info("story restarted");
            break;

        case SIGNAL_UNDO:
            sig = pop_undo_state();
            info("undone");
            break;

        case SIGNAL_RESTORE:
            sig = restore_state();
            info("story restored");
            break;

        default:
            fatal("Unknown signal (%d) received in start stub", sig);
            return;
        }
    }
}

uint32_t native_setmemsize(uint32_t new_size)
{
    /* intentionally not implemented */
    (void)new_size;
    return 1; /* indicate failure */
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
    return 0;  /* indicate success */
}
