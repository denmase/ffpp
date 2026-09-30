
#ifndef FFPP_MEM_H
#define FFPP_MEM_H
#include "avutil.h"
#include <stdlib.h>
#include <string.h>
#define av_malloc(s)     malloc(s)
#define av_mallocz(s)    calloc(1, (s) ? (s) : 1)
#define av_realloc(p, s) realloc((p), (s))
#define av_free(p)       free(p)
static void av_freep(void *arg)
{
    void **ptr = (void **)arg;
    free(*ptr);
    *ptr = NULL;
}
#define FFALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define av_strdup(s) ((s) ? strdup(s) : NULL)
#endif
