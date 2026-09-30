
#ifndef FFPP_AVUTIL_H
#define FFPP_AVUTIL_H
#include <stdint.h>
#include <stddef.h>
#include "rational.h"
#include "mathematics.h"
#define AV_VERSION_INT(a, b, c) (((a) << 16) | ((b) << 8) | (c))

#ifndef FFPP_STRIDE_T
#define FFPP_STRIDE_T
typedef int stride_t;   /* ARCH_X86_64=0 -> int, exactly like the generic config */
#endif

#ifndef FFPP_FFMACROS
#define FFPP_FFMACROS
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))
#define FFMAX(a, b) ((a) > (b) ? (a) : (b))
#define FFABS(a)    ((a) >= 0 ? (a) : (-(a)))
#define FFSIGN(a)   ((a) > 0 ? 1 : ((a) < 0 ? -1 : 0))
#ifndef DECLARE_ASM_CONST
#define DECLARE_ASM_CONST(n, t, v) static const t __attribute__((aligned(n))) v
#endif
#ifndef DECLARE_ALIGNED
#define DECLARE_ALIGNED(n, t, v)   t __attribute__((aligned(n))) v
#endif
#ifndef attribute_align_arg
#define attribute_align_arg
#endif
#endif
#endif
#include "mem.h"
#include "common.h"
