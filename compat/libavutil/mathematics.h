
#ifndef FFPP_MATHEMATICS_H
#define FFPP_MATHEMATICS_H
#include <stdint.h>
#include "rational.h"
#define AV_NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))
#define AV_TIME_BASE 1000000
static inline int64_t av_rescale(int64_t a, int64_t b, int64_t c)
{ return c ? (int64_t)((double)a * b / c + (a < 0 ? -0.5 : 0.5)) : 0; }
#endif
