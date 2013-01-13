#ifndef NATIVE_H_INCLUDED
#define NATIVE_H_INCLUDED

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include "memaccess.h"
#include "context.h"

#define MAX_MEM_SIZE        (16 << 20)
#define CALL_STACK_SIZE     ( 8 << 20)
#define DATA_STACK_SIZE     ( 4 << 20)

/* Interpreter version (major.minor.revis) */
#define NATIVE_VERSION_MAJOR 0
#define NATIVE_VERSION_MINOR 1
#define NATIVE_VERSION_REVIS 0

enum IoSys {
    IOSYS_NULL   = 0,
    IOSYS_FILTER = 1,
    IOSYS_GLK    = 2 };

/* VM state. Initialized in bss_*.c */
extern uint8_t          mem[];
extern uint32_t         data_stack[];
extern char             call_stack[];

void native_accelfunc(uint32_t l1, uint32_t l2);
void native_accelparam(uint32_t l1, uint32_t l2);
uint32_t native_linearsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options );
uint32_t native_binarysearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t struct_size,
    uint32_t num_structs, uint32_t key_offset, uint32_t options );
uint32_t native_linkedsearch(
    uint32_t key, uint32_t key_size, uint32_t start, uint32_t key_offset,
    uint32_t next_offset, uint32_t options );
void native_debugtrap(uint32_t argument);
uint32_t native_gestalt(uint32_t selector, uint32_t argument);
void native_getiosys(uint32_t *mode, uint32_t *rock);
uint32_t native_getmemsize();
uint32_t native_getstringtbl();
uint32_t native_glk(uint32_t selector, uint32_t narg, uint32_t **sp);
void native_invalidop(uint32_t offset, const char *descr);
uint32_t native_malloc(uint32_t size);
void native_mfree(uint32_t offset);
void native_protect(uint32_t offset, uint32_t size);
void native_quit();
int32_t native_random(int32_t range);
void native_reset();
void native_start();
void native_restart();
uint32_t native_restore(uint32_t stream);
uint32_t native_restoreundo();
uint32_t native_save(uint32_t stream, uint32_t *data_sp, struct Context *ctx);
uint32_t native_saveundo(uint32_t *data_sp, struct Context *ctx);
uint32_t native_setmemsize(uint32_t new_size);
void native_setiosys(uint32_t mode, uint32_t rock);
void native_setrandom(uint32_t l1);
void native_setstringtbl(uint32_t offset);
void native_stkroll(uint32_t size, int32_t steps, uint32_t *sp);
void native_streamchar(uint8_t ch, uint32_t *sp);
void native_streamunichar(uint32_t ch, uint32_t *sp);
void native_streamnum(int32_t n, uint32_t *sp);
void native_streamstr(uint32_t offset, uint32_t *sp);
uint32_t *native_ustring_dup(uint32_t offset);
uint32_t native_verify();
int32_t native_ftonumz(float f);
int32_t native_ftonumn(float f);

#endif /* ndef NATIVE_H_INCLUDED */
