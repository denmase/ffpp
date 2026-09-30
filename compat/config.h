
#ifndef FFPP_CONFIG_H
#define FFPP_CONFIG_H
/* Pure C path: no MMX/AltiVec/runtime cpudetect (generic C build semantics). */
#define ARCH_X86                 0
#define ARCH_X86_64              0
#define HAVE_MMX                 0
#define HAVE_MMX2                0
#define HAVE_MMXEXT              0
#define HAVE_AMD3DNOW            0
#define HAVE_ALTIVEC             0
#define HAVE_ALTIVEC_H           0
#define HAVE_BIGENDIAN           0
#define HAVE_FAST_64BIT          0
#define HAVE_FAST_UNALIGNED      0
#define HAVE_THREADS             0
#define HAVE_INLINE_ASM          0
#define CONFIG_RUNTIME_CPUDETECT 0
#define av_restrict restrict
#define LIBAV_CONFIGURATION     "ffpp C-path"
#define LIBAV_LICENSE           "GPL version 2 or later"
#endif
