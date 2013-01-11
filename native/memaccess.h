#ifndef MEMACCESS_H_INCLUDED
#define MEMACCESS_H_INCLUDED

#include "xtoy.h"
#include "string.h"  /* for memcpy */

union long_float { long l; float f; };

extern uint8_t mem[];

#define get_byte(a) (mem[a])
#define set_byte(a,v) ((void)(mem[a] = (v)))

/* These macro's respect strict aliasing rules: */
#define get_shrt(a) ({ uint16_t v; memcpy(&v, &mem[a], 2); ntohs(v); })
#define get_long(a) ({ uint32_t v; memcpy(&v, &mem[a], 4); ntohl(v); })
#define set_shrt(a,v) ((void)({uint16_t w = htons(v); memcpy(&mem[a], &w, 2);}))
#define set_long(a,v) ((void)({uint32_t w = htonl(v); memcpy(&mem[a], &w, 4);}))

#define get_float(a) ({ union long_float lf; lf.l = get_long(a); lf.f; })
#define set_float(a,v) ({ union long_float lf; lf.f = v; set_long(a, lf.l); })

/* These require -fno-strict-aliasing (or equivalent): */
/*
#define get_shrt(a) ntohs(*(uint16_t*)&mem[a])
#define get_long(a) ntohl(*(uint32_t*)&mem[a])

#define set_shrt(a,v) ((void)(*(uint16_t*)&mem[a] = htons(v)))
#define set_long(a,v) ((void)(*(uint32_t*)&mem[a] = htonl(v)))
*/

#endif /* ndef MEMACCESS_H_INCLUDED */
