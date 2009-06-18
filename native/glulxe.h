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
#include <assert.h>

#define FALSE false
#define TRUE true

#define glulx_malloc malloc
#define glulx_free   free
#define glulx_random rand

#define memmap ((uint8_t*)mem)
#define stackptr (*(uint8_t**)glk_stack_ptr)

#define Mem4(adr)       get_long(adr)
#define MemW4(adr, vl)  set_long(adr, vl)

#define Stk4(ptr)       (*ptr)
#define StkW4(ptr, vl)  (*(uint32_t**)ptr = (vl))

uint32_t *native_ustring_dup(uint32_t offset);
extern uint32_t **glk_stack_ptr;

#define fatal_error(msg) fatal("%s", msg)
#define nonfatal_warning(msg) warn("%s", msg)

#define make_temp_string(addr)  ((char*)&mem[(addr) + 1])
#define free_temp_string(str)   ((void)str)
#define make_temp_ustring(addr) (native_ustring_dup((addr) + 4))
#define free_temp_ustring(str)  (free(str))

/* Forward declarations defined in glkop.c itself: */
strid_t find_stream_by_id(glui32 objid);

#endif /* ndef GLULXE_H_INCLUDED */
