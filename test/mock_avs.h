
#ifndef MOCK_AVS_H
#define MOCK_AVS_H
#include "avisynth_c.h"
#include <stdint.h>

/* Minimal mock host to verify the FFPP plugin in the sandbox (not a real AviSynth). */
struct AVS_ScriptEnvironment;
struct AVS_Clip;
struct AVS_VideoFrame;

AVS_ScriptEnvironment *mock_env(void);          /* env with interface version 12 */
AVS_Clip *mock_child(int w, int h, int pixel_type, int nframes);
uint8_t mock_pattern(int x, int y, int n, int plane);
AVS_Value mock_apply(AVS_Value args);   /* first registered filter (FFPP) */
AVS_Value mock_apply2(AVS_Value args);  /* second registered filter (FFPP2) */           /* invoke the registered create() function */
void mock_fill_frame(AVS_VideoFrame *f, const AVS_VideoInfo *vi, int n);

/* internal mock accessors for the harness */
struct AVS_Clip *mock_clip_from_value(AVS_Value v);
struct AVS_FilterInfo *mock_clip_fi(struct AVS_Clip *clip);
#endif
