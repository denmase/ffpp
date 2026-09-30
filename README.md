# FFPP — ffdshow "Post Processing" for AviSynth+

An AviSynth+ plugin port of the **FFmpeg libpostproc** version shipped with
ffdshow-tryouts (`src/ffmpeg/libpostproc/`) — **pure C path**, filter kernel left
completely untouched. It exposes the full documented libpostproc filter-string
language exactly like ffdshow's *decoder → Post processing* page.

- Formats: 8-bit planar YUV — YV12 / YUV420P8, YV16 / YUV422P8, YV24 / YUV444P8
  (YUVA alpha is passed through untouched)
- Thread-safe: state protected by a mutex + AviSynth+ MT serialized mode
- Verified: 32/32 scenarios byte-identical against a direct `pp_postprocess()` call

---

## Two kernels: `FFPP` vs `FFPP2`

This plugin ships **two independent libpostproc kernels** in one DLL:

| Filter | Kernel | Source | Use it for |
|---|---|---|---|
| `FFPP` | ffdshow-tryouts libpostproc (frozen) | `src/libpostproc/` | Byte-exact reproduction of ffdshow's Post processing (parity with old screenshots/workflows) |
| `FFPP2` | modern FFmpeg libpostproc (n7.1) | `src/libpostproc2/` | Day-to-day use: upstream bug fixes, `ffmpeg -vf pp` parity, better auto-vectorization |

Both accept **identical parameters** (`pp`, `qp`, `quality`) and the same filter
strings. Differences to be aware of:

- Output can differ slightly between `FFPP` and `FFPP2` — that is expected
  (upstream bug fixes), not a malfunction. Each kernel is verified byte-exact
  against its own original code (32 + 31 scenarios).
- `al` (autolevels): in the classic C path it only adapts QP; in the modern
  kernel it also applies the luma level fix to pixels.
- The legacy `tempNoiseReducer` out-of-bounds read before its buffer
  (`tempBlurredPast[-256]`) exists only in the classic kernel; the modern
  kernel fixed it.

```avisynth
classic = FFPP(src,  pp="default", qp=15)   ; frozen ffdshow behavior
modern  = FFPP2(src, pp="default", qp=15)   ; maintained FFmpeg behavior
```

## Usage

```avisynth
LoadPlugin("...\ffpp.dll")

a = FFPP(src, pp="default", qp=15, quality=6)   ; ffdshow's default preset
b = FFPP(src, pp="ac", qp=10)                   ; strong deblocking preset
c = FFPP(src, pp="vb:y,dr:n", qp=20)            ; luma vb only, no deringing
d = FFPP(src, pp="tn:64:128:256/fq:8", qp=8)    ; temporal denoise + quantizer
```

---

## Plugin parameters

### `clip`
Input clip. **Must be 8-bit planar YUV with 420/422/444 subsampling**
(YV12, YUV420P8, YV16, YUV422P8, YV24, YUV444P8). Anything else (RGB, packed,
10-bit+) is rejected with an error message. If the clip has an alpha plane
(YUVA variants), the alpha plane is copied through untouched.

### `pp` (string, default `"default"`)
A libpostproc filter string — see the full reference below. `"default"` expands
to `hb:a,vb:a,dr:a`, the same preset as ffdshow's Post processing page.

### `qp` (int, 1–31, default 15)
**Forced quantizer.** libpostproc derives its deblocking strength from the
quantizer of each macroblock; since an AviSynth clip carries no QP information,
FFPP forces a constant QP for the whole frame (equivalent to appending
`fq:<qp>` to the string, and it overrides any `fq:n` inside `pp`).

| qp value | Effect |
|---|---|
| 1–5    | Very gentle; almost no deblocking, dering barely moves pixels |
| 6–10   | Light cleanup; good for high-quality sources with mild blocking |
| 11–20  | **Default range**; typical TV/web encodes (start at 15) |
| 21–31  | Aggressive; strong blocking, low-bitrate sources |

Luma and chroma use the same forced QP. The value only *scales* filter
strength — filters that are not enabled in `pp` do nothing regardless of `qp`.

### `quality` (int, 0–6, default 6)
Quality scale for the **`a` (autoq) option**. A filter written with the `a`
option is only enabled when `quality >=` the filter's minimum quality
(see the filter table below). Filters **without** `a` ignore `quality` entirely.

| quality | Filters enabled (with `:a` syntax) |
|---|---|
| 0 | none (auto filters all off) |
| 1 | `hb`, `ha`, `al` (luma) |
| 2 | + `vb`, `va` |
| 3 | + `hb` chroma |
| 4 | + `vb` chroma, + all deinterlacers |
| 5 | + `dr` luma |
| 6 | + `dr` chroma (full default) |

**Example:** `pp="hb:a,vb:a,dr:a"` with `quality=4` enables `hb` and `vb`
(luma+chroma) but keeps `dr` luma off (needs 5) and chroma off (needs 6).

---

## The `pp` string — libpostproc filter reference

### Syntax

```
pp = <filter>[,<filter>...]
filter = [-]name[:option[:option...]]
```

- Filters are separated by **`,` or `/`** (both equivalent).
- Options follow the name after `:`.
- A leading **`-`** disables the filter (useful to undo part of a preset:
  `"default,-dr"` = default without deringing).

### Options (apply to the filter they follow)

| Option | Long form | Meaning |
|---|---|---|
| `a` | `autoq` | enable only if `quality >=` the filter's minimum (see table) |
| `c` | `chrom` | **force chroma processing on** for this filter |
| `y` | `nochrom` | **force chroma processing off** for this filter |
| `n` | `noluma` | **force luma processing off** for this filter |
| `f` | `fullyrange` | only for `al`: treat range as full 0–255 |

Without `c`/`y`, chroma follows the filter's *chroma default* in the table below.

### Filter table

| Short | Long | Chroma default | Min lum q | Min chrom q | Description |
|---|---|---|---|---|---|
| `hb` | `hdeblock` | on | 1 | 3 | Horizontal deblocking (default thresholds) |
| `vb` | `vdeblock` | on | 2 | 4 | Vertical deblocking |
| `h1` | `x1hdeblock` | on | 1 | 3 | *(no-op — mask is 0 in ffdshow itself)* |
| `v1` | `x1vdeblock` | on | 2 | 4 | *(no-op — same reason)* |
| `ha` | `ahdeblock` | on | 1 | 3 | Horizontal "accurate" deblocking (1-pass) |
| `va` | `avdeblock` | on | 2 | 4 | Vertical "accurate" deblocking (1-pass) |
| `dr` | `dering` | on | 5 | 6 | Deringing (removes mosquito noise near edges) |
| `al` | `autolevels` | **off** | 1 | 2 | Autolevels — see note below |
| `lb` | `linblenddeint` | on | 1 | 4 | Deinterlace: linear blend |
| `li` | `linipoldeint` | on | 1 | 4 | Deinterlace: linear interpolation |
| `ci` | `cubicipoldeint` | on | 1 | 4 | Deinterlace: cubic interpolation |
| `md` | `mediandeint` | on | 1 | 4 | Deinterlace: median |
| `fd` | `ffmpegdeint` | on | 1 | 4 | Deinterlace: FFmpeg filter |
| `l5` | `lowpass5` | on | 1 | 4 | Deinterlace: lowpass 5-tap |
| `tn` | `tmpnoise` | on | 7 | 8 | Temporal noise reduction |
| `fq` | `forcequant` | on | 0 | 0 | Force quantizer (`n` = 1..31, default 15) |

**Deinterlacers** operate per 8-line block and are mutually exclusive by design —
if several are enabled, one wins by fixed internal priority
(`li` > `lb` > `md` > `ci` > `fd` > `l5`). Use at most one.

### Filters with numeric options

**`hb`, `vb`, `ha`, `va`** accept two optional thresholds:

```
hb[:baseDcDiff[:flatnessThreshold]]
```

- `baseDcDiff` (default **32**): base DC difference — scales the "is this a flat
  block" test together with the QP. Larger = fewer blocks classified as flat.
- `flatnessThreshold` (default **39**): number (0–56) of near-equal adjacent-pixel
  pairs a block needs to be treated as flat. Flat blocks get the low-pass path;
  busy blocks get the edge-directed default filter. Larger = stricter flatness.

Example: `pp="hb:128:7"` = deblock horizontally with very strict flatness
(only nearly-constant blocks get low-passed).

**`tn`** accepts three motion thresholds:

```
tn[:v1[:v2[:v3]]]        defaults 700 / 1500 / 3000
```

Per 8×8 block, a motion metric `d` (sum of squared differences against the
temporal reference) picks one of four outcomes:

| d | Action |
|---|---|
| `d < v1`     | strong temporal average `(ref*7 + cur + 4) >> 3` (heavy smoothing) |
| `v1..v2`     | moderate blend `(ref*3 + cur + 2) >> 2` |
| `v2..v3`     | 50/50 `(ref + cur + 1) >> 1` |
| `d >= v3`    | reset reference to current block (no smoothing) |

Lower thresholds = more smoothing of slow changes; higher = only still areas
are denoised. State is kept across frames (per filter instance).

**`fq`** accepts one number: `fq[:n]` (default 15) — see the `qp` parameter.
When the plugin `qp` argument is given it wins over any `fq:n` in the string.

### `al` (autolevels) — important C-path note

In the **pure C path** (which this plugin ports), `al` does **not** change pixel
values at all: the C `blockCopy` ignores the level-fix parameters. Its only real
effect is through `QPCorrecture` — QP is scaled per frame from the frame's
luma histogram, which slightly adapts deblocking strength over time. It is kept
for parser compatibility. `al:f` only changes the histogram range (0–255 vs
16–235). Do not expect a brightness/contrast effect.

### Presets (expand inline)

| Preset | Expands to |
|---|---|
| `default`, `de` | `hb:a,vb:a,dr:a` |
| `fast`, `fa`    | `h1:a,v1:a,dr:a` (≈ dering only, since x1 are no-ops here) |
| `ac`            | `ha:a:128:7,va:a,dr:a` |

---

## Semantic notes (bug-compatible with ffdshow)

1. No per-MB QP table exists in AviSynth -> `FORCE_QUANT` is always active with
   `nonBQP = 0`, exactly the original C behavior when `QP_store == NULL`.
2. `h1`/`v1` are no-ops: their bit masks are 0 in ffdshow's own
   `postprocess_internal.h`.
3. `al` behaves as described above (C-path limitation, ported as-is).
4. Known legacy upstream UB (documented, deliberately unchanged):
   `tempNoiseReducer` reads `tempBlurredPast[-256]` — a heap read right before
   the buffer, identical in ffdshow.

## Building

### Windows (MinGW-w64, produces `ffpp.dll` for 64-bit AviSynth+)

Tool naming: standalone MinGW-w64 distributions (w64devkit, Debian's
`gcc-mingw-w64`, etc.) prefix the tools with the target triplet
(`x86_64-w64-mingw32-gcc`, `x86_64-w64-mingw32-dlltool`); inside an MSYS2
UCRT64 shell the same tools are unprefixed (`gcc`, `dlltool`) because the
environment already targets x86_64-w64-mingw32. The commands below use the
unprefixed MSYS2 names.

```bat
:: 0) import library for the avs_* functions (avisynth.dll is not needed at build time)
dlltool -d include/avs/avisynth.def -l libavisynth.a

:: 1) modern kernel object (FFPP2) - C99, namespaced symbols
gcc -std=gnu99 -fcommon -O2 -DHAVE_AV_CONFIG_H ^
  -Dpp_get_mode_by_name_and_quality=ffpp2_get_mode_by_name_and_quality ^
  -Dpp_free_mode=ffpp2_free_mode -Dpp_get_context=ffpp2_get_context ^
  -Dpp_free_context=ffpp2_free_context -Dpp_postprocess=ffpp2_postprocess ^
  -Dpp_help=ffpp2_help ^
  -Iinclude/avs -Icompat -Isrc/libpostproc2 -c -o postprocess2.o src/libpostproc2/postprocess.c

:: 2) generic build (any x64 CPU), links both kernels
gcc -std=gnu89 -fcommon -O2 -shared -DHAVE_AV_CONFIG_H -DAVS_STATIC_LIB ^
  -Iinclude/avs -Icompat -Isrc/libpostproc -Isrc ^
  -o ffpp.dll src/ffpp.c src/libpostproc/postprocess.c postprocess2.o ^
  -L. -lavisynth -Wl,--enable-auto-image-base -static-libgcc

:: 3) AVX2 build (requires Haswell/Zen or newer): replace step 2 with
gcc -std=gnu89 -fcommon -O3 -march=x86-64-v3 -fno-strict-aliasing -shared ^
  -DHAVE_AV_CONFIG_H -DAVS_STATIC_LIB -Iinclude/avs -Icompat -Isrc/libpostproc -Isrc ^
  -o ffpp.dll src/ffpp.c src/libpostproc/postprocess.c postprocess2.o ^
  -L. -lavisynth -Wl,--enable-auto-image-base -static-libgcc
```

> Important flags: `-std=gnu89 -fcommon` = GCC 12+ tolerance for the year-2004
> classic kernel; the modern kernel needs `-std=gnu99`. `-DAVS_STATIC_LIB` =
> disable `dllimport` from `capi.h` so the `avs_*` symbols are resolved by name
> from `avisynth.dll` at load time.

### Linux (verification/testing)

```bash
make         # libffpp.so
make check   # 32 byte-identical scenarios vs a direct pp_postprocess call
```

### CI

GitHub Actions (`.github/workflows/ci.yml`) runs the full Linux test suite and
builds both Windows variants (generic and AVX2) as downloadable artifacts.

## License

**GPL-2.0-or-later** — the kernel derives from FFmpeg libpostproc (GPL). The glue
(`src/ffpp.c`) and build stubs (`compat/`) are licensed GPL-2.0-or-later as well.
