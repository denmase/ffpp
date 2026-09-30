
#ifndef FFPP_MACROS_H
#define FFPP_MACROS_H
#define FF_ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))
#define FF_PTRDIFF_MAX ((ptrdiff_t)((size_t)-1 / 2))
#define MKTAG(a, b, c, d) ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#endif
