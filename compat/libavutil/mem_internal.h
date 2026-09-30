
#ifndef FFPP_MEM_INTERNAL_H
#define FFPP_MEM_INTERNAL_H
#include "mem.h"
/* FFPP kernels preallocate fixed-size buffers at init (constant clip size),
   so the modern av_malloc_array-style growth helpers are not needed. */
#define av_malloc_array(a, s) av_malloc((a) * (s))
#define av_mallocz_array(a, s) av_mallocz((a) * (s))
#define av_realloc_array(p, a, s) av_realloc((p), (a) * (s))
#define av_fast_malloc(p, s, min) do { if (*(s) < (min)) { av_freep(p); *(p) = av_malloc(min); *(s) = *(p) ? (min) : 0; } } while (0)
#define av_fast_mallocz(p, s, min) do { if (*(s) < (min)) { av_freep(p); *(p) = av_mallocz(min); *(s) = *(p) ? (min) : 0; } } while (0)
#define av_fast_padded_malloc(p, s, min) av_fast_malloc(p, s, (min) + 64)
#define av_fast_padded_mallocz(p, s, min) av_fast_mallocz(p, s, (min) + 64)
#endif
