#ifndef GLULXE_H_INCLUDED
#define GLULXE_H_INCLUDED

/* This file provides some macros similar to Glulxe for the benefit of gklop.c,
   which was taken verbatim from Glulxe. */

#include "xtoy.h"
#include "glk.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define FALSE false
#define TRUE true

#define glulx_malloc malloc
#define glulx_free   free
#define glulx_random random

#define memmap ((uint8_t*)mem)
#define stackptr (*glk_stack_ptr)

#define Mem4(adr)       (ntohl(*(uint32_t*)&mem[adr]))
#define MemW4(adr, vl)  (*(uint32_t*)&mem[adr] = htonl(vl))

#define Stk4(ptr)       (glui32)(*ptr)
#define StkW4(ptr, vl)  (*ptr = vl)


/* Defined in storyfile.c */
extern uint8_t mem[];

/* Defined in native.c */
extern uint32_t **glk_stack_ptr;

/* Defined in glulxe.c */
void fatal_error(const char *msg);
void nonfatal_warning(const char *msg);
char *make_temp_string(glui32 addr);
glui32 *make_temp_ustring(glui32 addr);
void free_temp_string(char *str);
void free_temp_ustring(glui32 *str);

/* Forward declarations defined in glkop.c itself: */
strid_t find_stream_by_id(glui32 objid);


#endif /* ndef GLULXE_H_INCLUDED */
