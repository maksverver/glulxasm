#include "storyfile.h"
#include "xtoy.h"
#include "glkop.h"
#include "glkstart.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#if 0  /* this is for later */
#if defined(__i386) && !defined(__i686)
#error Architecture i386 not supported; compile with -march=i686 instead!
#elif defined(__i686)
#elif defined(__x86_64)
#error Architecture x86_64 not yet implemented!
#else
#error Unknown architecture!
#endif
#endif

extern const uint8_t _binary_storyfile_ulx_start[];

glkunix_argumentlist_t glkunix_arguments[] = {
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
    (void)data;
    return TRUE;
}

void glk_main()
{
    if (!init_dispatch()) return;

#if 0  /* this is for later */
    /* Allocate the story call stack */
    uint8_t *call_stack = malloc(init_stack_size);
    if (call_stack == NULL) return;
    memset(call_stack, 0, init_stack_size);
#endif

    /* Initialize state */
    native_reset_memory(_binary_storyfile_ulx_start);
    native_setrandom(0);
    native_setstringtbl(init_decoding_tbl);
    stack[0] = 0;
    func(init_start_func)(&stack[0]);

#if 0 /* this is for later */
    free(call_stack);
#endif
}
