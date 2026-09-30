
#ifndef FFPP_INTMATH_H
#define FFPP_INTMATH_H
#include <stdint.h>
static inline int64_t ff_mul64(int64_t a, int64_t b) { return a * b; }
#define MUL64(a, b) ((int64_t)(a) * (int64_t)(b))
#define MAC64(d, a, b) ((d) + MUL64(a, b))
#define MID64(a, b, c) ((a) + (b) + ((c) >> 1))
#endif
