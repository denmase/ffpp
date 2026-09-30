
#ifndef FFPP2_H
#define FFPP2_H
/* FFPP2 kernel: modern FFmpeg libpostproc (n7.1), public symbols renamed
   to ffpp2_* at build time (see NSFLAGS in the Makefile) to avoid collisions
   with the classic ffdshow kernel living in the same binary. */
#include "postprocess.h"

pp_mode *ffpp2_get_mode_by_name_and_quality(const char *name, int quality);
void ffpp2_free_mode(pp_mode *mode);
pp_context *ffpp2_get_context(int width, int height, int flags);
void ffpp2_free_context(pp_context *ppContext);
void ffpp2_postprocess(const uint8_t *src[3], const int srcStride[3],
                       uint8_t *dst[3], const int dstStride[3],
                       int horizontalSize, int verticalSize,
                       const int8_t *QP_store, int QP_stride,
                       pp_mode *mode, pp_context *ppContext, int pict_type);

typedef struct FFPPKernel {
    pp_mode *(*get_mode)(const char *, int);
    void (*free_mode)(pp_mode *);
    pp_context *(*get_context)(int, int, int);
    void (*free_context)(pp_context *);
    void (*postprocess)(const uint8_t *src[3], const int srcStride[3],
                        uint8_t *dst[3], const int dstStride[3],
                        int, int, const int8_t *, int,
                        pp_mode *, pp_context *, int);
} FFPPKernel;

#define FFPP_KERNEL_CLASSIC { pp_get_mode_by_name_and_quality, pp_free_mode, \
    pp_get_context, pp_free_context, pp_postprocess }
#define FFPP_KERNEL_MODERN  { ffpp2_get_mode_by_name_and_quality, ffpp2_free_mode, \
    ffpp2_get_context, ffpp2_free_context, ffpp2_postprocess }

#endif
