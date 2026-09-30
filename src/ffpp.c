
/*
 * FFPP - ffdshow "Post Processing" (FFmpeg libpostproc, jalur C murni)
 * Plugin AviSynth+ (C API, interface v12+).
 *
 * FFPP(clip, string "pp", int "qp", int "quality")
 *   pp      : libpostproc filter string, default "default" (= hb:a,vb:a,dr:a)
 *   qp      : forced quantizer 1..31 (equivalent to fq:<qp>), default 15
 *   quality : 0..6, used by the "a"/autoq option, default 6
 *
 * Deliberate deviations (documentation):
 *  - AviSynth carries no per-MB QP table -> always FORCE_QUANT (QP_store=NULL),
 *    nonBQP=0, exactly the original C behavior without a decoder QP table.
 *  - x1 (h1/v1) are no-ops: their masks are 0 in ffdshow's own postprocess_internal.h.
 *  - "al" does not change pixels in the C path (the C blockCopy ignores levelFix);
 *    it only affects QP via QPCorrecture. Ported as-is.
 *  - Temporal state (tn, al histogram) + mutex -> reported as MT_SERIALIZED.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include "avisynth_c.h"
#include "postprocess.h"
#include "postprocess_internal.h"
#include "ffpp2.h"   /* struct PPMode: forcedQuant, FORCE_QUANT */

typedef struct {
    const FFPPKernel *k;
    pp_mode    *mode;
    pp_context *ctx;
    int qp;
    unsigned char *scratch[3];
    size_t scratchSize[3];
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mtx;
#endif
} FFPPData;

static void ffpp_lock(FFPPData *d)
{
#ifdef _WIN32
    EnterCriticalSection(&d->cs);
#else
    pthread_mutex_lock(&d->mtx);
#endif
}
static void ffpp_unlock(FFPPData *d)
{
#ifdef _WIN32
    LeaveCriticalSection(&d->cs);
#else
    pthread_mutex_unlock(&d->mtx);
#endif
}

static int grow_scratch(FFPPData *d, int plane, size_t need)
{
    if (d->scratchSize[plane] >= need) return 1;
    free(d->scratch[plane]);
    d->scratch[plane] = (unsigned char *)malloc(need + 64);
    if (!d->scratch[plane]) { d->scratchSize[plane] = 0; return 0; }
    d->scratchSize[plane] = need + 64;
    return 1;
}

static AVS_VideoFrame *AVSC_CC get_frame(AVS_FilterInfo *fi, int n)
{
    FFPPData *d = (FFPPData *)fi->user_data;
    AVS_ScriptEnvironment *env = fi->env;
    AVS_VideoFrame *src, *dst;
    const unsigned char *srcp[3];
    unsigned char *dstp[3];
    int srcStride[3], dstStride[3];
    const int planes[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };
    int p;

    src = avs_get_frame(fi->child, n);
    if (!src) { fi->error = "FFPP: failed to get source frame"; return NULL; }

    dst = avs_new_video_frame_p_a(env, &fi->vi, src, AVS_FRAME_ALIGN);
    if (!dst) { fi->error = "FFPP: frame allocation failed"; return NULL; }

    ffpp_lock(d);

    /* copy src -> scratch = pristine "source" (like decoder output) */
    for (p = 0; p < 3; p++) {
        int w = avs_get_row_size_p(src, planes[p]);
        int h = avs_get_height_p(src, planes[p]);
        int srcPitch = avs_get_pitch_p(src, planes[p]);
        const unsigned char *s = avs_get_read_ptr_p(src, planes[p]);
        int y;
        if (!grow_scratch(d, p, (size_t)srcPitch * h + 64)) {
            ffpp_unlock(d);
            avs_release_video_frame(src);
            if (dst) avs_release_video_frame(dst);
            fi->error = "FFPP: out of memory";
            return NULL;
        }
        for (y = 0; y < h; y++) {
            memcpy(d->scratch[p] + (size_t)y * srcPitch, s + (size_t)y * srcPitch, (size_t)w);
            if (srcPitch > w)  /* zero the padding for deterministic output */
                memset(d->scratch[p] + (size_t)y * srcPitch + w, 0, (size_t)(srcPitch - w));
        }
        srcp[p]      = d->scratch[p];
        srcStride[p] = srcPitch;
        dstp[p]      = avs_get_write_ptr_p(dst, planes[p]);
        dstStride[p] = avs_get_pitch_p(dst, planes[p]);
    }

    /* QP_store=NULL -> forced QP (see documentation above) */
    d->k->postprocess(srcp, srcStride, dstp, dstStride,
                      fi->vi.width, fi->vi.height,
                      NULL, 0, d->mode, d->ctx, 0);

    /* alpha (YUVA) is untouched by pp -> copy as-is */
    if (avs_num_components(&fi->vi) == 4) {
        int w  = avs_get_row_size_p(src, AVS_PLANAR_A);
        int h  = avs_get_height_p(src, AVS_PLANAR_A);
        int sp = avs_get_pitch_p(src, AVS_PLANAR_A);
        int dp = avs_get_pitch_p(dst, AVS_PLANAR_A);
        const unsigned char *s = avs_get_read_ptr_p(src, AVS_PLANAR_A);
        unsigned char *dd = avs_get_write_ptr_p(dst, AVS_PLANAR_A);
        int y;
        for (y = 0; y < h; y++)
            memcpy(dd + (size_t)y * dp, s + (size_t)y * sp, (size_t)w);
    }

    ffpp_unlock(d);
    avs_release_video_frame(src);
    return dst;
}

static int AVSC_CC set_cache_hints(AVS_FilterInfo *fi, int cachehints, int frame_range)
{
    (void)fi; (void)frame_range;
    return cachehints == AVS_CACHE_GET_MTMODE ? 2 : 0; /* 2 = serialized mode */
}

static void AVSC_CC free_filter(AVS_FilterInfo *fi)
{
    FFPPData *d = (FFPPData *)fi->user_data;
    int p;
    if (!d) return;
    d->k->free_mode(d->mode);
    d->k->free_context(d->ctx);
    for (p = 0; p < 3; p++) free(d->scratch[p]);
#ifdef _WIN32
    DeleteCriticalSection(&d->cs);
#else
    pthread_mutex_destroy(&d->mtx);
#endif
    free(d);
    fi->user_data = NULL;
}

static const FFPPKernel k_classic = FFPP_KERNEL_CLASSIC;
static const FFPPKernel k_modern  = FFPP_KERNEL_MODERN;

static AVS_Value create_ex(AVS_ScriptEnvironment *env, AVS_Value args,
                           const FFPPKernel *k, int modern);

static AVS_Value AVSC_CC create(AVS_ScriptEnvironment *env, AVS_Value args, void *user_data)
{
    (void)user_data;
    return create_ex(env, args, &k_classic, 0);
}

static AVS_Value AVSC_CC create2(AVS_ScriptEnvironment *env, AVS_Value args, void *user_data)
{
    (void)user_data;
    return create_ex(env, args, &k_modern, 1);
}

static AVS_Value create_ex(AVS_ScriptEnvironment *env, AVS_Value args,
                           const FFPPKernel *k, int modern)
{
    AVS_Value v;
    AVS_FilterInfo *fi = NULL;
    AVS_Clip *clip;
    const AVS_VideoInfo *vi;
    FFPPData *d;
    const char *ppStr;
    int qp, quality, hsub = 0, vsub = 0;

    clip = avs_new_c_filter(env, &fi, avs_array_elt(args, 0), 1);
    if (!clip) return avs_new_value_error("FFPP: new_c_filter failed");

    if (avs_get_version(clip) < 12) {
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: requires AviSynth+ interface v12 or newer");
    }

    vi = &fi->vi;
    if (!avs_has_video(vi)) {
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: clip must have video");
    }
    if (!avs_is_planar(vi) || !avs_is_yuv(vi) || avs_component_size(vi) != 1) {
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: planar YUV 8-bit only (YV12/YV16/YV24)");
    }
    if (avs_is_420(vi))      { hsub = 1; vsub = 1; }
    else if (avs_is_422(vi)) { hsub = 1; }
    else if (avs_is_444(vi)) { /* 0,0 */ }
    else {
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: only 420/422/444 subsampling");
    }

    ppStr   = avs_defined(avs_array_elt(args, 1)) ? avs_as_string(avs_array_elt(args, 1)) : "default";
    qp      = avs_defined(avs_array_elt(args, 2)) ? avs_as_int(avs_array_elt(args, 2)) : 15;
    quality = avs_defined(avs_array_elt(args, 3)) ? avs_as_int(avs_array_elt(args, 3)) : 6;
#ifdef FFPP_DEBUG
    fprintf(stderr, "DEBUG create: t1=%c t2=%c t3=%c qp=%d quality=%d\n",
            avs_array_elt(args,1).type, avs_array_elt(args,2).type,
            avs_array_elt(args,3).type, qp, quality);
#endif
    if (qp < 1 || qp > 31 || quality < 0 || quality > 6) {
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: qp must be 1..31, quality 0..6");
    }

    d = (FFPPData *)calloc(1, sizeof(FFPPData));
    d->k = k;
    if (!d) { avs_release_clip(clip); return avs_new_value_error("FFPP: out of memory"); }

#ifdef _WIN32
    InitializeCriticalSection(&d->cs);
#else
    pthread_mutex_init(&d->mtx, NULL);
#endif

    {
        char buf[512];
        const char *p = ppStr;
        if (modern) {
            /* modern kernel: inject the forced quantizer through the parser
               (the PPMode layout differs between kernels, so no struct cast) */
            snprintf(buf, sizeof(buf), "%s,fq:%d", ppStr, qp);
            p = buf;
        }
        d->mode = k->get_mode(p, quality);
    }
    if (!d->mode) {
        free(d);
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: invalid pp string");
    }
    d->qp = qp;

    if (!modern) {
        /* classic kernel: always force the quantizer (AviSynth has no decoder
           QP table). pp_mode (opaque in postprocess.h) = PPMode in
           postprocess_internal.h; the original code casts the pointer too. */
        PPMode *pm = (PPMode *)d->mode;
        pm->forcedQuant = qp;
        pm->lumMode |= FORCE_QUANT;
    }

    d->ctx = k->get_context(vi->width, vi->height, PP_FORMAT | hsub | (vsub << 4));
    if (!d->ctx) {
        pp_free_mode(d->mode);
        free(d);
        avs_release_clip(clip);
        return avs_new_value_error("FFPP: pp_get_context failed");
    }

    fi->user_data       = d;
    fi->get_frame       = get_frame;
    fi->set_cache_hints = set_cache_hints;
    fi->free_filter     = free_filter;

    avs_set_to_clip(&v, clip);
    avs_release_clip(clip);
    return v;
}

const char *AVSC_CC avisynth_c_plugin_init(AVS_ScriptEnvironment *env)
{
    avs_add_function(env, "FFPP",  "c[pp]s[qp]i[quality]i", create, 0);
    avs_add_function(env, "FFPP2", "c[pp]s[qp]i[quality]i", create2, 0);
    return "FFPP: ffdshow postproc (libpostproc jalur C) v0.1";
}
