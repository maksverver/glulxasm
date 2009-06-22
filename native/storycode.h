#ifndef STORYFILE_H_INCLUDED
#define STORYFILE_H_INCLUDED

#include "native.h"
#include "context.h"
#include <malloc.h>  /* for alloca on Windows */

/* Parameters from the Glulx header: */
extern const uint32_t init_ramstart;
extern const uint32_t init_extstart;
extern const uint32_t init_endmem;
extern const uint32_t init_stack_size;
extern const uint32_t init_start_func;
extern const uint32_t init_decoding_tbl;
extern const uint32_t init_checksum;

/* Story state:
    - mem contains the interpreter memory (init_endmem bytes)
    - data_stack contains the interpreter stack (init_stack_size bytes)
    - call_stack contains the C call stack (init_stack_size bytes)
    - story_start/story_stop points to a context stored on the call stack saved
      when the story started (for restarting) or stopped (for saving)
*/
extern uint8_t          mem[];
extern uint32_t         data_stack[];
extern char             call_stack[];
extern struct Context   *story_start;
extern struct Context   *story_stop;

/* Function map covering addresses from 0 to init_ramstart: */
extern uint32_t (* const func_map[])(uint32_t*);
#define func(addr) func_map[addr/4]


#endif /* ndef STORYFILE_H_INCLUDED */
