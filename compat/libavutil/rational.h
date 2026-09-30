
#ifndef FFPP_RATIONAL_H
#define FFPP_RATIONAL_H
#include <stdint.h>
typedef struct AVRational { int num; int den; } AVRational;
#define AV_TIME_BASE_Q (AVRational){1, 1000000}
#endif
