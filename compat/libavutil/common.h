
#ifndef FFPP_COMMON_H
#define FFPP_COMMON_H
#include "attributes.h"
#include "avutil.h"
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define av_clip(a, amin, amax) ((a) < (amin) ? (amin) : ((a) > (amax) ? (amax) : (a)))
static av_always_inline av_const uint8_t av_clip_uint8_c(int a)
{ if (a & (~0xFF)) return (~a) >> 31; else return (uint8_t)a; }
#define av_clip_uint8 av_clip_uint8_c
#endif
