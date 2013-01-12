#ifndef STORYFILE_H_INCLUDED
#define STORYFILE_H_INCLUDED

#include "native.h"
#include "context.h"

/* Parameters from the Glulx header: */
extern const uint32_t init_ramstart;
extern const uint32_t init_extstart;
extern const uint32_t init_endmem;
extern const uint32_t init_stack_size;
extern const uint32_t init_start_func;
extern const uint32_t init_decoding_tbl;
extern const uint32_t init_checksum;

/* Function map covering addresses from 0 to init_ramstart: */
extern uint32_t (* const func_map[])(uint32_t*);
#define func(addr) func_map[addr/4]


#endif /* ndef STORYFILE_H_INCLUDED */
