#ifndef NATIVE_H_INCLUDED
#define NATIVE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>

void native_accelfunc(uint32_t a1, uint32_t a2);
void native_accelparam(uint32_t a1, uint32_t a2);
uint32_t native_linearsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options );
uint32_t native_binarysearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options );
uint32_t native_linkedsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t key_offset,
    uint32_t next_offset, uint32_t options );
uint32_t native_gestalt(uint32_t argument, uint32_t selector);
uint32_t native_getmemsize();
uint32_t native_getstringtbl();
uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp);
uint32_t native_malloc(uint32_t a1);
void native_mfree(uint32_t a1);
uint32_t native_malloc(uint32_t a1);
void native_mfree(uint32_t a1);
void native_quit();
int32_t native_random(int32_t a1);
void native_restart();
uint32_t native_restore(uint32_t a1);
uint32_t native_restoreundo();
uint32_t native_save(uint32_t a1);
uint32_t native_saveundo();
void native_setiosys(uint32_t a1, uint32_t a2);
void native_setrandom(uint32_t a1);
void native_setstringtbl(uint32_t offset);
void native_stkroll(uint32_t size, int32_t steps, uint32_t *sp);
void native_streamchar(uint32_t ch);
void native_streamnum(int32_t n);
void native_streamstr(uint32_t offset);
uint32_t native_verify();

#endif /* ndef NATIVE_H_INCLUDED */
