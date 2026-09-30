
#ifndef FFPP_ATTRIBUTES_H
#define FFPP_ATTRIBUTES_H
#define av_always_inline inline __attribute__((always_inline))
#define av_noinline   __attribute__((noinline))
#define av_pure       __attribute__((pure))
#define av_const      __attribute__((const))
#define av_unused     __attribute__((unused))
#define av_deprecated __attribute__((deprecated))
#define av_cold __attribute__((cold))
#define av_flatten    __attribute__((flatten))
#endif

#ifndef AV_GCC_VERSION_AT_LEAST
#define AV_GCC_VERSION_AT_LEAST(x, y) \
    ((__GNUC__ > (x)) || ((__GNUC__ == (x)) && (__GNUC_MINOR__ >= (y))))
#endif
