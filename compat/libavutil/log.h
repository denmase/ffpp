
#ifndef FFPP_LOG_H
#define FFPP_LOG_H
#include <stdarg.h>
#include <stdio.h>
typedef struct AVOption AVOption;
typedef struct AVClass {
    const char *class_name;
    const char *(*item_name)(void *ctx);
    const AVOption *option;
    int version;
    int log_level_offset;
    unsigned int category;
    void (*child_next)(void *obj, void *prev);
    const struct AVClass *(*child_class_iterate)(void **iter);
} AVClass;
#define AV_LOG_QUIET   -8
#define AV_LOG_ERROR   16
#define AV_LOG_WARNING 24
#define AV_LOG_INFO    32
#define AV_LOG_DEBUG   48
static void av_log(void *avcl, int level, const char *fmt, ...)
{
    (void)avcl; (void)level;
    va_list vl;
    va_start(vl, fmt);
    vfprintf(stderr, fmt, vl);
    va_end(vl);
}
#endif
