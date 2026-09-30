
#ifndef FFPP_CPU_H
#define FFPP_CPU_H
#include "attributes.h"
#define AV_CPU_FLAG_MMX        0x0001
#define AV_CPU_FLAG_MMX2       0x0002
#define AV_CPU_FLAG_MMXEXT     0x0002
#define AV_CPU_FLAG_SSE        0x0008
#define AV_CPU_FLAG_SSE2       0x0010
#define AV_CPU_FLAG_SSE3       0x0040
#define AV_CPU_FLAG_SSSE3      0x0080
#define AV_CPU_FLAG_SSE4       0x0100
#define AV_CPU_FLAG_SSE42      0x0200
#define AV_CPU_FLAG_AVX        0x4000
#define AV_CPU_FLAG_ALTIVEC   0x00010000
#define AV_CPU_FLAG_AVX2       0x8000
/* FFPP2 runs on x86-64 v3 (AVX2) or generic x86-64; no runtime detection:
   cpuCaps is derived from PP_FORMAT only, matching the classic build. */
static inline int av_get_cpu_flags(void) { return 0; }
#endif
