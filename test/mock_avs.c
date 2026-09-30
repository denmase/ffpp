
#include "avisynth_c.h"
#include "mock_avs.h"
#include <stdlib.h>
#include <string.h>

struct AVS_ScriptEnvironment { int version; };
struct AVS_Clip {
    int refcount;
    int is_child;
    AVS_VideoInfo vi;
    AVS_FilterInfo *fi;   /* only for filter result clips */
};

static AVS_ScriptEnvironment g_env = { 12 };
static AVS_ApplyFunc g_apply[4];
static int g_napply;

AVS_ScriptEnvironment *mock_env(void) { return &g_env; }

int avs_get_version(AVS_Clip *clip) { (void)clip; return 12; }

int avs_add_function(AVS_ScriptEnvironment *e, const char *name, const char *params,
                     AVS_ApplyFunc apply, void *user_data)
{
    (void)e; (void)name; (void)params; (void)user_data;
    if (g_napply < 4) g_apply[g_napply++] = apply;
    return 1;
}

AVS_Value mock_apply(AVS_Value args)  { return g_apply[0](&g_env, args, NULL); }
AVS_Value mock_apply2(AVS_Value args) { return g_apply[1](&g_env, args, NULL); }

static int plane_width(const AVS_VideoInfo *vi, int plane)
{
    int s = avs_get_plane_width_subsampling(vi, plane);
    return s ? vi->width >> s : vi->width;
}
static int plane_h(const AVS_VideoInfo *vi, int plane)
{
    int s = avs_get_plane_height_subsampling(vi, plane);
    return s ? vi->height >> s : vi->height;
}

static AVS_VideoFrame *frame_new(const AVS_VideoInfo *vi, int align, int zero)
{
    const int pl[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };
    int rowsY = vi->width, hY = vi->height;
    int pitch  = (rowsY + align - 1) & ~(align - 1);
    int rowsUV = plane_width(vi, pl[1]);
    int hUV    = plane_h(vi, pl[1]);
    int pitchUV = (rowsUV + align - 1) & ~(align - 1);
    size_t szY  = (size_t)pitch * hY;
    size_t szUV = (size_t)pitchUV * hUV;
    AVS_VideoFrameBuffer *vfb = (AVS_VideoFrameBuffer *)calloc(1, sizeof(*vfb));
    AVS_VideoFrame *f = (AVS_VideoFrame *)calloc(1, sizeof(*f));
    vfb->data = (BYTE *)(zero ? calloc(1, szY + 2 * szUV) : malloc(szY + 2 * szUV));
    vfb->data_size = (int)(szY + 2 * szUV);
    vfb->refcount = 1;
    f->refcount = 1;
    f->vfb = vfb;
    f->offset = 0;
    f->pitch = pitch; f->row_size = rowsY; f->height = hY;
    f->offsetU = (int)szY; f->offsetV = (int)(szY + szUV);
    f->pitchUV = pitchUV; f->row_sizeUV = rowsUV; f->heightUV = hUV;
    /* offsetA/pitchA/row_sizeA = 0 (calloc) -> tanpa alpha */
    return f;
}

static void frame_free(AVS_VideoFrame *f)
{
    if (!f) return;
    if (--f->refcount <= 0) {
        if (f->vfb && --f->vfb->refcount <= 0) {
            free(f->vfb->data);
            free(f->vfb);
        }
        free(f);
    }
}

static BYTE *frame_ptr(AVS_VideoFrame *f, int plane)
{
    switch (plane) {
    case AVS_PLANAR_Y: return f->vfb->data + f->offset;
    case AVS_PLANAR_U: return f->vfb->data + f->offsetU;
    case AVS_PLANAR_V: return f->vfb->data + f->offsetV;
    case AVS_PLANAR_A: return f->vfb->data + f->offsetA;
    }
    return NULL;
}

AVS_VideoFrame *avs_get_frame(AVS_Clip *clip, int n)
{
    AVS_VideoFrame *f = frame_new(&clip->vi, AVS_FRAME_ALIGN, 1);
    mock_fill_frame(f, &clip->vi, n);
    return f;
}

void avs_release_video_frame(AVS_VideoFrame *f) { frame_free(f); }

AVS_VideoFrame *avs_new_video_frame_p_a(AVS_ScriptEnvironment *env, const AVS_VideoInfo *vi,
                                        const AVS_VideoFrame *prop_src, int align)
{
    (void)env; (void)prop_src;
    return frame_new(vi, align, 1);
}

const unsigned char *avs_get_read_ptr_p(const AVS_VideoFrame *f, int plane)
{
    switch (plane) {
    case AVS_PLANAR_Y: return (const unsigned char *)f->vfb->data + f->offset;
    case AVS_PLANAR_U: return (const unsigned char *)f->vfb->data + f->offsetU;
    case AVS_PLANAR_V: return (const unsigned char *)f->vfb->data + f->offsetV;
    case AVS_PLANAR_A: return (const unsigned char *)f->vfb->data + f->offsetA;
    }
    return NULL;
}
BYTE *avs_get_write_ptr_p(const AVS_VideoFrame *f, int plane) { return frame_ptr((AVS_VideoFrame *)f, plane); }
int avs_get_pitch_p(const AVS_VideoFrame *f, int plane)
{
    if (plane == AVS_PLANAR_Y) return f->pitch;
    if (plane == AVS_PLANAR_A) return f->pitchA;
    return f->pitchUV;
}
int avs_get_row_size_p(const AVS_VideoFrame *f, int plane)
{
    if (plane == AVS_PLANAR_Y) return f->row_size;
    if (plane == AVS_PLANAR_A) return f->row_sizeA;
    return f->row_sizeUV;
}
int avs_get_height_p(const AVS_VideoFrame *f, int plane)
{
    if (plane == AVS_PLANAR_A) return f->height;
    if (plane == AVS_PLANAR_Y) return f->height;
    return f->heightUV;
}

int avs_get_plane_width_subsampling(const AVS_VideoInfo *vi, int plane)
{
    if (plane == AVS_PLANAR_U || plane == AVS_PLANAR_V) {
        if (avs_is_420(vi)) return 1;
        if (avs_is_422(vi)) return 1;
    }
    return 0;
}
int avs_get_plane_height_subsampling(const AVS_VideoInfo *vi, int plane)
{
    if (plane == AVS_PLANAR_U || plane == AVS_PLANAR_V) {
        if (avs_is_420(vi)) return 1;
    }
    return 0;
}


#define FMT_EQ(pt, gen) \
    (((pt) & AVS_CS_PLANAR_MASK & ~AVS_CS_SAMPLE_BITS_MASK) == ((gen) & AVS_CS_PLANAR_FILTER))
int avs_is_420(const AVS_VideoInfo *p)
{ return FMT_EQ(p->pixel_type, AVS_CS_GENERIC_YUV420); }
int avs_is_422(const AVS_VideoInfo *p)
{ return FMT_EQ(p->pixel_type, AVS_CS_GENERIC_YUV422); }
int avs_is_444(const AVS_VideoInfo *p)
{ return FMT_EQ(p->pixel_type, AVS_CS_GENERIC_YUV444); }
int avs_component_size(const AVS_VideoInfo *p)
{
    if (p->pixel_type & AVS_CS_SAMPLE_BITS_32) return 4;
    if (p->pixel_type & AVS_CS_SAMPLE_BITS_16) return 2;
    return 1;
}
int avs_num_components(const AVS_VideoInfo *p)
{ return (p->pixel_type & AVS_CS_YUVA) ? 4 : 3; }

AVS_Clip *avs_new_c_filter(AVS_ScriptEnvironment *e, AVS_FilterInfo **fi,
                           AVS_Value child, int storeChild)
{
    AVS_Clip *childclip = child.d.clip;
    AVS_Clip *self = (AVS_Clip *)calloc(1, sizeof(*self));
    AVS_FilterInfo *f = (AVS_FilterInfo *)calloc(1, sizeof(*f));
    f->env = e;
    f->vi  = childclip->vi;
    if (storeChild) f->child = childclip;
    *fi = f;
    self->refcount = 1;
    self->vi = childclip->vi;
    self->fi = f;
    return self;
}

void avs_release_clip(AVS_Clip *clip)
{
    if (!clip) return;
    if (--clip->refcount > 0) return;
    if (!clip->is_child && clip->fi) {
        if (clip->fi->free_filter) clip->fi->free_filter(clip->fi);
        free(clip->fi);
    }
    free(clip);
}

void avs_set_to_clip(AVS_Value *v, AVS_Clip *p)
{
    p->refcount++;
    v->type   = 'c';
    v->d.clip = p;
}

AVS_Clip *mock_clip_from_value(AVS_Value v) { return v.d.clip; }
AVS_FilterInfo *mock_clip_fi(AVS_Clip *clip) { return clip->fi; }

AVS_Clip *mock_child(int w, int h, int pixel_type, int nframes)
{
    AVS_Clip *c = (AVS_Clip *)calloc(1, sizeof(*c));
    c->refcount = 1;
    c->is_child = 1;
    c->vi.width = w;
    c->vi.height = h;
    c->vi.pixel_type = pixel_type;
    c->vi.fps_numerator = 25;
    c->vi.fps_denominator = 1;
    c->vi.num_frames = nframes;
    return c;
}

static uint32_t xs32(uint32_t s)
{
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

uint8_t mock_pattern(int x, int y, int n, int plane)
{
    uint32_t v = xs32((uint32_t)x * 2654435761u + (uint32_t)y * 40503u * 7u +
                      (uint32_t)n * 104729u + (uint32_t)plane * 31u + 1u);
    int blocky = (((x >> 3) + (y >> 3)) & 1) ? 150 : 40;
    return (uint8_t)(blocky + ((v >> 16) & 63));
}

void mock_fill_frame(AVS_VideoFrame *f, const AVS_VideoInfo *vi, int n)
{
    const int planes[3] = { AVS_PLANAR_Y, AVS_PLANAR_U, AVS_PLANAR_V };
    int p, y, x;
    (void)vi;
    for (p = 0; p < 3; p++) {
        unsigned char *d = frame_ptr(f, planes[p]);
        int pitch = avs_get_pitch_p(f, planes[p]);
        int rows  = avs_get_row_size_p(f, planes[p]);
        int h     = avs_get_height_p(f, planes[p]);
        for (y = 0; y < h; y++)
            for (x = 0; x < rows; x++)
                d[(size_t)y * pitch + x] = mock_pattern(x, y, n, p);
    }
}
