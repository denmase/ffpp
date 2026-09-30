
#ifndef FFPP_INTREADWRITE_H
#define FFPP_INTREADWRITE_H
#include <stdint.h>
#include <string.h>
#define AV_RN(s, p)    ({ uint##s##_t __v; memcpy(&__v, (p), sizeof(__v)); __v; })
#define AV_WN(s, p, v) do { uint##s##_t __v = (v); memcpy((p), &__v, sizeof(__v)); } while (0)
#define AV_RN32(p)     AV_RN(32, p)
#define AV_WN32(p, v)  AV_WN(32, (p), (v))
#define AV_RN64(p)     AV_RN(64, p)
#define AV_WN64(p, v)  AV_WN(64, (p), (v))
#define AV_RL32(p)     AV_RN32(p)
#define AV_WL32(p, v)  AV_WN32((p), (v))
#define AV_RL64(p)     AV_RN64(p)
#define AV_WL64(p, v)  AV_WN64((p), (v))
#endif
