
#ifndef FFPP_X86_ASM_H
#define FFPP_X86_ASM_H
#define emms_c() do {} while (0)
#define AV_CPU_FLAG_MMX      0
#define AV_CPU_FLAG_MMX2     0
#define AV_CPU_FLAG_MMXEXT   0
#define AV_CPU_FLAG_3DNOW    0
#define AV_CPU_FLAG_SSE2     0
static inline int av_get_cpu_flags(void) { return 0; }
#endif
