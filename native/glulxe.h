#ifndef GLULXE_H_INCLUDED
#define GLULXE_H_INCLUDED

/* This file provides some macros similar to Glulxe for the benefit of gklop.c,
   which was taken verbatim from Glulxe. */

#include "messages.h"
#include "memaccess.h"
#include "glk.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define FALSE false
#define TRUE true

#define glulx_malloc malloc
#define glulx_free   free
#define glulx_random rand

#define memmap (mem)
#define stackptr (*glk_stack_ptr)

#define Mem4(adr)       get_long(adr)
#define MemW4(adr, vl)  set_long(adr, vl)

#define Stk4(ptr)       (*ptr)
#define StkW4(ptr, vl)  (*ptr = vl)

/* Defined in native.c */
extern uint32_t **glk_stack_ptr;

/* Defined in glulxe.c */
#define fatal_error(msg) fatal("%s", msg)
#define nonfatal_warning(msg) warn("%s", msg)
char *make_temp_string(glui32 addr);
glui32 *make_temp_ustring(glui32 addr);
void free_temp_string(char *str);
void free_temp_ustring(glui32 *str);

/* Forward declarations defined in glkop.c itself: */
strid_t find_stream_by_id(glui32 objid);


#endif /* ndef GLULXE_H_INCLUDED */
