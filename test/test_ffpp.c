
#include "avisynth_c.h"
#include "mock_avs.h"
#include "postprocess.h"
#include "postprocess_internal.h"
#include "ffpp2.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static int g_pass = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL  %s  (line %d)\n", msg, __LINE__); g_fail++; } \
    else { printf("ok    %s\n", msg); g_pass++; } \
} while (0)

typedef struct { int hsub, vsub; } Subs;

static Subs subs_of(const AVS_VideoInfo *vi)
{
    Subs s = { 0, 0 };
    if (avs_is_420(vi)) { s.hsub = 1; s.vsub = 1; }
    else if (avs_is_422(vi)) { s.hsub = 1; }
    return s;
}

/* ---------- direct reference runner (ground truth = original code) ---------- */

typedef struct {
    unsigned char *data[3];
    int pitch[3], rows[3], h[3];
} RefPlanes;

typedef struct {
    pp_mode *mode;
    pp_context *ctx;
    RefPlanes ref;
    int w, h;
    const FFPPKernel *k;
    int modern;
} Ref;

static int ref_init(Ref *r, const AVS_VideoInfo *vi, const char *pp, int qp, int quality,
                    const FFPPKernel *k, int modern)
{
    Subs s = subs_of(vi);
    int p;
    memset(r, 0, sizeof(*r));
    r->w = vi->width;
    r->h = vi->height;
    r->k = k;
    r->modern = modern;
    {
        char buf[512];
        const char *p = pp;
        if (modern) {
            snprintf(buf, sizeof(buf), "%s,fq:%d", pp, qp);
            p = buf;
        }
        r->mode = k->get_mode(p, quality);
    }
    if (!r->mode) return 0;
    if (!modern) {
        PPMode *pm = (PPMode *)r->mode;
        pm->forcedQuant = qp;
        pm->lumMode |= FORCE_QUANT;
    }
    r->ctx = k->get_context(vi->width, vi->height, PP_FORMAT | s.hsub | (s.vsub << 4));
    if (!r->ctx) return 0;
    for (p = 0; p < 3; p++) {
        r->ref.rows[p]  = (p && s.hsub) ? vi->width >> s.hsub : vi->width;
        r->ref.h[p]     = (p && s.vsub) ? vi->height >> s.vsub : vi->height;
        r->ref.pitch[p] = (r->ref.rows[p] + AVS_FRAME_ALIGN - 1) & ~(AVS_FRAME_ALIGN - 1);
        r->ref.data[p]  = (unsigned char *)calloc(1, (size_t)r->ref.pitch[p] * r->ref.h[p]);
        if (!r->ref.data[p]) return 0;
    }
    return 1;
}

static void ref_free(Ref *r)
{
    int p;
    if (r->mode) r->k->free_mode(r->mode);
    if (r->ctx)  r->k->free_context(r->ctx);
    for (p = 0; p < 3; p++) free(r->ref.data[p]);
}

static void ref_run(Ref *r, int n)
{
    const unsigned char *srcp[3];
    unsigned char *dstp[3];
    int srcStride[3], dstStride[3];
    unsigned char *scratch[3] = { NULL, NULL, NULL };
    int p, y, x;
    for (p = 0; p < 3; p++) {
        int rows = r->ref.rows[p];
        int h    = r->ref.h[p];
        int pitch = r->ref.pitch[p];
        scratch[p] = (unsigned char *)calloc(1, (size_t)pitch * h);
        for (y = 0; y < h; y++)
            for (x = 0; x < rows; x++)
                scratch[p][(size_t)y * pitch + x] = mock_pattern(x, y, n, p);
        srcp[p] = scratch[p];
        srcStride[p] = pitch;
        dstp[p] = r->ref.data[p];
        dstStride[p] = pitch;
    }
    r->k->postprocess(srcp, srcStride, dstp, dstStride,
                      r->w, r->h, NULL, 0, r->mode, r->ctx, 0);
    for (p = 0; p < 3; p++) free(scratch[p]);
}

/* ---------- perbandingan ---------- */

static const char *g_label = "?";
static int g_frame = -1;

static int compare_with_ref(AVS_VideoFrame *got, const RefPlanes *ref)
{
    const int planes[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };
    int p, y, x;
    for (p = 0; p < 3; p++) {
        int rows = avs_get_row_size_p(got, planes[p]);
        int h    = avs_get_height_p(got, planes[p]);
        const unsigned char *g = avs_get_read_ptr_p(got, planes[p]);
        if (rows != ref->rows[p] || h != ref->h[p]) {
            printf("DIFF %s f%d plane=%d DIM got=%dx%d ref=%dx%d\n",
                   g_label, g_frame, p, rows, h, ref->rows[p], ref->h[p]);
            return 0;
        }
        for (y = 0; y < h; y++) {
            const unsigned char *gr = g + (size_t)y * avs_get_pitch_p(got, planes[p]);
            const unsigned char *rr = ref->data[p] + (size_t)y * ref->pitch[p];
            for (x = 0; x < rows; x++) {
                if (gr[x] != rr[x]) {
                    printf("DIFF %s f%d plane=%d x=%d y=%d got=%d ref=%d srcpat=%d\n",
                           g_label, g_frame, p, x, y, gr[x], rr[x],
                           mock_pattern(x, y, g_frame, p));
                    return 0;
                }
            }
        }
    }
    return 1;
}

static AVS_Value make_args(AVS_Clip *child, const char *pp, int qp, int quality)
{
    /* avs_new_value_array stores a POINTER to the elements; a local array would be
       gone by the time create() reads it. Use static storage (test is single-threaded). */
    static AVS_Value args[4];
    AVS_Value v;
    avs_set_to_clip(&args[0], child);
    args[1] = avs_new_value_string(pp);
    args[2] = avs_new_value_int(qp);
    args[3] = avs_new_value_int(quality);
    v = avs_new_value_array(args, 4);
    return v;
}

static AVS_VideoFrame *clip_get_frame(AVS_Clip *out, int n)
{
    AVS_FilterInfo *fi = mock_clip_fi(out);
    return fi->get_frame(fi, n);
}

static int run_case2(const char *label, int pixel_type, int w, int h,
                     const char *pp, int qp, int quality,
                     const int *frames, int nf, AVS_Value (*applyfn)(AVS_Value),
                     const FFPPKernel *k, int modern);

static int run_case(const char *label, int pixel_type, int w, int h,
                    const char *pp, int qp, int quality,
                    const int *frames, int nf)
{
    return run_case2(label, pixel_type, w, h, pp, qp, quality, frames, nf,
                     mock_apply, &((const FFPPKernel)FFPP_KERNEL_CLASSIC), 0);
}

static int run_case2(const char *label, int pixel_type, int w, int h,
                     const char *pp, int qp, int quality,
                     const int *frames, int nf, AVS_Value (*applyfn)(AVS_Value),
                     const FFPPKernel *k, int modern)
{
    AVS_Clip *child = mock_child(w, h, pixel_type, 10);
    AVS_Value args  = make_args(child, pp, qp, quality);
    AVS_Value v     = applyfn(args);
    AVS_Clip *out;
    AVS_FilterInfo *fi;
    Ref ref;
    int i;

    if (avs_is_error(v)) {
        printf("FAIL  %s: create error: %s\n", label, v.d.string);
        g_fail++;
        avs_release_clip(child);
        return 0;
    }
    out = mock_clip_from_value(v);
    fi  = mock_clip_fi(out);

    if (!ref_init(&ref, &fi->vi, pp, qp, quality, k, modern)) {
        printf("FAIL  %s: ref init\n", label);
        g_fail++;
        avs_release_clip(child);
        return 0;
    }

    for (i = 0; i < nf; i++) {
        AVS_VideoFrame *got = clip_get_frame(out, frames[i]);
        char msg[256];
        if (!got) {
            printf("FAIL  %s: get_frame(%d) NULL: %s\n", label, frames[i],
                   fi->error ? fi->error : "?");
            g_fail++;
            break;
        }
        g_label = label;
        g_frame = frames[i];
        ref_run(&ref, frames[i]);
        snprintf(msg, sizeof(msg), "%s frame %d == referensi langsung", label, frames[i]);
        CHECK(compare_with_ref(got, &ref.ref), msg);
        avs_release_video_frame(got);
    }

    ref_free(&ref);
    avs_release_clip(out);
    avs_release_clip(child);
    return 1;
}

static int run_determinism(int pixel_type, int w, int h,
                           const char *pp, int qp, int quality, int frame)
{
    AVS_Clip *child, *out;
    AVS_VideoFrame *f1, *f2;
    int same = 1, p, y;
    const int planes[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };

    child = mock_child(w, h, pixel_type, 10);
    out = mock_clip_from_value(mock_apply(make_args(child, pp, qp, quality)));
    f1 = clip_get_frame(out, frame);
    f1 = f1; /* dirilis nanti */

    child2_hack:;
    {
        /* second identical run */
        AVS_Clip *child2 = mock_child(w, h, pixel_type, 10);
        AVS_Clip *out2 = mock_clip_from_value(mock_apply(make_args(child2, pp, qp, quality)));
        f2 = clip_get_frame(out2, frame);
        for (p = 0; p < 3 && same; p++) {
            int rows = avs_get_row_size_p(f1, planes[p]);
            int hh   = avs_get_height_p(f1, planes[p]);
            const unsigned char *a = avs_get_read_ptr_p(f1, planes[p]);
            const unsigned char *b = avs_get_read_ptr_p(f2, planes[p]);
            for (y = 0; y < hh; y++)
                if (memcmp(a + (size_t)y * avs_get_pitch_p(f1, planes[p]),
                           b + (size_t)y * avs_get_pitch_p(f2, planes[p]), (size_t)rows) != 0) { same = 0; break; }
        }
        avs_release_video_frame(f2);
        avs_release_clip(out2);
        avs_release_clip(child2);
    }
    avs_release_video_frame(f1);
    avs_release_clip(out);
    avs_release_clip(child);
    return same;
}

static int run_suite(AVS_Value (*applyfn)(AVS_Value), const FFPPKernel *k,
                     int modern, const char *tag)
{
    const char *msg;
    int f0123[4] = { 0, 1, 2, 3 };
    int f012[3]  = { 0, 1, 2 };
    int f0[1]    = { 0 };
    int f1a[1]   = { 1 };

    msg = avisynth_c_plugin_init(mock_env());
    CHECK(msg && strncmp(msg, "FFPP", 4) == 0, "plugin_init mengembalikan deskripsi");

    run_case("A1 default 320x240 YV12 f012", AVS_CS_YV12, 320, 240, "default", 15, 6, f012, 3);
    run_case("B1 191x107 YV12",  AVS_CS_YV12, 191, 107, "default", 15, 6, f0, 1);
    run_case("B2 33x17 YV12",    AVS_CS_YV12, 33, 17, "default", 15, 6, f0, 1);
    run_case("C1 322x242 YV16",  AVS_CS_YV16, 322, 242, "default", 15, 6, f0, 1);
    run_case("D1 96x64 YV24 f012", AVS_CS_YV24, 96, 64, "default", 15, 6, f012, 3);

    /* E: passthrough - "al" changes no pixels in the C path */
    {
        AVS_Clip *child = mock_child(64, 64, AVS_CS_YV12, 10);
        const char *pp_pass = modern ? "fq:15" : "al";
        AVS_Clip *out = mock_clip_from_value(applyfn(make_args(child, pp_pass, 15, 6)));
        AVS_VideoFrame *got = clip_get_frame(out, 0);
        AVS_VideoFrame *raw = avs_get_frame(child, 0);
        int same = 1, p, y;
        const int planes[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };
        for (p = 0; p < 3 && same; p++)
            for (y = 0; y < avs_get_height_p(raw, planes[p]); y++)
                if (memcmp(avs_get_read_ptr_p(got, planes[p]) + (size_t)y * avs_get_pitch_p(got, planes[p]),
                           avs_get_read_ptr_p(raw, planes[p]) + (size_t)y * avs_get_pitch_p(raw, planes[p]),
                           (size_t)avs_get_row_size_p(raw, planes[p])) != 0) { same = 0; break; }
        char emsg[128];
        snprintf(emsg, sizeof(emsg), "E1 pp=%s -> output identical to input (passthrough)", pp_pass);
        CHECK(same, emsg);
        avs_release_video_frame(got);
        avs_release_video_frame(raw);
        avs_release_clip(out);
        avs_release_clip(child);
    }

    if (!modern)
        CHECK(run_determinism(AVS_CS_YV12, 320, 240, "default", 15, 6, 1),
              "F1 two identical runs -> byte-identical");

    run_case("H1 tn 320x240 f0123", AVS_CS_YV12, 320, 240, "tn:64:128:256", 15, 6, f0123, 4);
    run_case("I1 lb", AVS_CS_YV12, 320, 240, "lb", 15, 6, f0, 1);
    run_case("I2 li", AVS_CS_YV12, 320, 240, "li", 15, 6, f0, 1);
    run_case("I3 ci", AVS_CS_YV12, 320, 240, "ci", 15, 6, f0, 1);
    run_case("I4 md", AVS_CS_YV12, 320, 240, "md", 15, 6, f0, 1);
    run_case("I5 fd", AVS_CS_YV12, 320, 240, "fd", 15, 6, f0, 1);
    run_case("I6 l5", AVS_CS_YV12, 320, 240, "l5", 15, 6, f0, 1);
    run_case("J1 qp=1",  AVS_CS_YV12, 128, 128, "default", 1, 6, f0, 1);
    run_case("J2 qp=31", AVS_CS_YV12, 128, 128, "default", 31, 6, f0, 1);
    run_case("K1 ac",        AVS_CS_YV12, 256, 128, "ac", 10, 6, f0, 1);
    run_case("K2 hb:128:7",  AVS_CS_YV12, 256, 128, "hb:128:7", 15, 6, f0, 1);
    run_case("K3 vb:y,dr:n", AVS_CS_YV12, 256, 128, "vb:y,dr:n", 20, 6, f0, 1);
    run_case("K4 fq:8",      AVS_CS_YV12, 256, 128, "fq:8", 8, 6, f1a, 1);
    run_case("K5 quality=0", AVS_CS_YV12, 256, 128, "default", 15, 0, f0, 1);

    /* G: jalur error */
    {
        AVS_Clip *child = mock_child(64, 64, AVS_CS_YV12, 10);
        AVS_Value v = applyfn(make_args(child, "zzz_no_such_filter", 15, 6));
        CHECK(avs_is_error(v), "G1 invalid pp string -> error");
        avs_release_clip(child);

        child = mock_child(64, 64, AVS_CS_YUY2, 10);
        v = applyfn(make_args(child, "default", 15, 6));
        CHECK(avs_is_error(v), "G2 YUY2 rejected");
        avs_release_clip(child);

        child = mock_child(64, 64, AVS_CS_YV12, 10);
        v = applyfn(make_args(child, "default", 99, 6));
        CHECK(avs_is_error(v), "G3 qp=99 rejected");
        avs_release_clip(child);
    }

    printf("== [%s] totals so far: %d ok, %d FAIL ==\n", tag, g_pass, g_fail);
    return 0;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    static const FFPPKernel k_classic = FFPP_KERNEL_CLASSIC;
    static const FFPPKernel k_modern  = FFPP_KERNEL_MODERN;
    int fails;

    run_suite(mock_apply,  &k_classic, 0, "FFPP");
    fails = g_fail;
    g_fail = 0;
    run_suite(mock_apply2, &k_modern,  1, "FFPP2");
    fails += g_fail;

    printf("\n== FINAL: %s ==\n", fails ? "FAILURES PRESENT" : "ALL GREEN");
    return fails ? 1 : 0;
}
