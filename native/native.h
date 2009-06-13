#ifndef NATIVE_H_INCLUDED
#define NATIVE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>
#include "xtoy.h"

void native_accelfunc(uint32_t a1, uint32_t a2);
void native_accelparam(uint32_t a1, uint32_t a2);
uint32_t native_binarysearch(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7);
uint32_t native_gestalt(uint32_t a1, uint32_t a2);
uint32_t native_getmemsize();
uint32_t native_getstringtbl();
uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp);
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
void native_stkcopy(uint32_t a1);
void native_stkroll(uint32_t a1, uint32_t a2);
void native_streamchar(uint32_t ch);
void native_streamnum(int32_t n);
void native_streamstr(uint32_t offset);
void native_verify();

#endif /* ndef NATIVE_H_INCLUDED */
