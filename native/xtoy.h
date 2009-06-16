#ifndef XTOY_H_INCLUDED
#define XTOY_H_INCLUDED

/* These macros require an x86 (Pentium Pro or better) or x86_64 processor. */

#include <stdint.h>

#define bswap16c(x) ( (x>>8&0x00ff) | (x<<8&0xff00) )
#define bswap32c(x) ( (x>>24&0x000000ff) | (x>> 8&0x0000ff00) \
                    | (x<< 8&0x00ff0000) | (x<<24&0xff000000) )

#define bswap16(x) ({ uint16_t tmp = (x), res;                              \
        if (__builtin_constant_p(tmp)) res = bswap16c(tmp);                 \
        else __asm__("rorw $8, %w0" : "=r"(res) : "0"(tmp) : "cc"); res; })

#define bswap32(x) ({ uint32_t tmp = (x), res;                              \
        if (__builtin_constant_p(tmp)) res = bswap32c(tmp);                 \
        else __asm__("bswapl %0" : "=r"(res) : "0"(tmp)); res; })

#define htons(x) bswap16(x)
#define ntohs(x) bswap16(x)
#define htonl(x) bswap32(x)
#define ntohl(x) bswap32(x)

#endif /* ndef XTOY_H_INCLUDED */
