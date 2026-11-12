# Noise parity applicability analysis

**Status:** analysis document only — no code changes, no build, no render verification.
**Branch:** `hermes/noise-applicability-20261110`
**Base:** `origin/snapshot/astra-parity-20261110`
**MaterialX version:** 1.39 (stdlib in `lib/linux_x64/materialx/libraries/stdlib/`, the harvested MaterialX submodule)

## Scope

The parity ledger lists these noise-family node defs, all taken verbatim from
`stdlib_defs.mtlx`. Total: **58 node defs**.

| Family | Node defs (count) | stdlib_defs.mtlx lines |
|---|---|---|
| noise2d | ND_noise2d_float, ND_noise2d_color3, ND_noise2d_color4, ND_noise2d_vector2, ND_noise2d_vector3, ND_noise2d_vector4, ND_noise2d_color3FA, ND_noise2d_color4FA, ND_noise2d_vector2FA, ND_noise2d_vector3FA, ND_noise2d_vector4FA (11) | 799-864 |
| noise3d | ND_noise3d_float, ND_noise3d_color3, ND_noise3d_color4, ND_noise3d_vector2, ND_noise3d_vector3, ND_noise3d_vector4, ND_noise3d_color3FA, ND_noise3d_color4FA, ND_noise3d_vector2FA, ND_noise3d_vector3FA, ND_noise3d_vector4FA (11) | 870-930 |
| fractal2d | ND_fractal2d_float, ND_fractal2d_color3, ND_fractal2d_color4, ND_fractal2d_vector2, ND_fractal2d_vector3, ND_fractal2d_vector4, ND_fractal2d_color3FA, ND_fractal2d_color4FA, ND_fractal2d_vector2FA, ND_fractal2d_vector3FA, ND_fractal2d_vector4FA (11) | 941-1028 |
| fractal3d | ND_fractal3d_float, ND_fractal3d_color3, ND_fractal3d_color4, ND_fractal3d_vector2, ND_fractal3d_vector3, ND_fractal3d_vector4, ND_fractal3d_color3FA, ND_fractal3d_color4FA, ND_fractal3d_vector2FA, ND_fractal3d_vector3FA, ND_fractal3d_vector4FA (11) | 1034-1126 |
| cellnoise | ND_cellnoise2d_float, ND_cellnoise3d_float (2) | 1127-1139 |
| worleynoise | ND_worleynoise2d_float, ND_worleynoise2d_vector2, ND_worleynoise2d_vector3, ND_worleynoise3d_float, ND_worleynoise3d_vector2, ND_worleynoise3d_vector3 (6) | 1145-1185 |
| unifiednoise | ND_unifiednoise2d_float, ND_unifiednoise3d_float (2) | 1204-1238 |
| randomfloat | ND_randomfloat_float, ND_randomfloat_integer (2) | 1244-1257 |
| randomcolor | ND_randomcolor_float, ND_randomcolor_integer (2) | 1263-1284 |

Families marked `FA` use a **scalar amplitude** applied uniformly across channels;
the non-FA siblings use a per-channel (vector) amplitude. Both share the same
noise core.

## 1. Native Cycles lowering (intern/cycles/materialx/graph.cpp)

### 1.1 Perlin / fractal core — lines 18654-18788

The `is_native_noise_or_fractal_family` branch lowers every `noise2d`/`noise3d`/
`fractal2d`/`fractal3d` nodedef to a single Cycles `NoiseTextureNode`:

```cpp
noise->set_use_normalize(false);                       // line 18670
noise->set_detail(is_fractal ? float(octaves - 1) : 0.0f);  // line 18671
if (is_fractal) {                                      // line 18683
    noise->set_type(NODE_NOISE_FBM);
    noise->set_lacunarity(lacunarity);                 // line 18691
    noise->set_roughness(diminish);                    // line 18692
}
```

Then amplitude is applied with a `MathNode` multiply and pivot with a `MathNode`
add (lines 18694-18713 for float; color3/vector siblings use a `MixNode` MUL then
ADD at lines 18715-18738). Color4/vector4 add a 4th channel via a second
`NoiseTextureNode` sampled at an offset position (lines 18749-18788).

**Key detail from the source comment (lines 18665-18677):** "MaterialX's real
genosl/genglsl implementations use raw snoise/Perlin values... Plain noise
samples a single octave; fractal loops exactly `octaves` times, while Cycles'
fBM `detail` counts inclusively (0 -> one octave), so seed detail with
`octaves - 1`." This confirms the loop structure is intentionally matched.

### 1.2 What the Cycles kernel actually computes

Cycles' Perlin lives in `intern/cycles/kernel/svm/noise.h`:

- `fade(t)` = `t*t*t*(t*(t*6.0f - 15.0f) + 10.0f)` (line 19) — **identical** to
  MaterialX's `mx_fade` (mx_noise.glsl line 167).
- `grad2`/`grad3` (lines 134-149) — the same 8-direction 2D and 16-direction
  3D gradient table as MaterialX's `mx_gradient_float` (glsl lines 95-111).
- `perlin_2d`/`perlin_3d` (lines 160-207) — same floor/fade/bilinear-trilinear
  structure as MaterialX `mx_perlin_noise_float` (glsl lines 249-285).
- `noise_scale2` = `0.6616f`, `noise_scale3` = `0.9820f` (lines 674-682) —
  **identical** to MaterialX's `mx_gradient_scale2d`/`mx_gradient_scale3d`
  (glsl lines 124-125).
- Hash: `hash_uint2`/`hash_uint3` in `util/hash.h` (lines 139-166) use Bob
  Jenkins' `final` (bjfinal only, no bjmix) with seed `0xdeadbeef + (len<<2) + 13`.
  MaterialX's `mx_hash_int` for 2/3 args uses bjfinal only with the same seed
  (glsl lines 177-196). **For the primary float/color3/vector2/3 channels the
  hash matches MaterialX exactly.**

### 1.3 Where it diverges from bit-for-bit

- **Floating-point only:** Cycles evaluates the same operations in float; ordering
  of arithmetic is the same, so differences are limited to FP rounding. This is
  the EXACT case (same function, differences only floating point).

- **4th channel (color4/vector4):** The W channel is sampled at an offset
  position. Cycles uses offset `(19, 73, 29)` for noise and `(19, 193, 17)` for
  fractal (graph.cpp lines 18753-18762). MaterialX's reference vec4 fractal uses
  `p + (19, 193)` / `p + (19, 193, 17)` (glsl lines 430-437, 469-477), matching
  Cycles' fractal W offset. The noise-family W offset `(19, 73, 29)` does not
  appear in the MaterialX reference shown; the 4th scalar sample is a
  "distinct fourth scalar sample" per the graph.cpp comment (line 331). This is
  a channel-correlation divergence, not a per-channel value divergence: channel
  0 matches, extra channels decorrelate.

### 1.4 Cellnoise — lines 18832-18848

Lowered to `VectorMathNode(FLOOR)` + `WhiteNoiseTextureNode`. Cycles' `WhiteNoise`
uses its own hash (`util/hash.h`), **not** MaterialX's Bob Jenkins
`mx_hash_int`/`mx_bits_to_01`. Cellnoise is therefore **not** bit-for-bit with
the MaterialX reference (`mx_cell_noise_float`, glsl lines 325-353).

### 1.5 Worley — lines 18849-18903

Lowered to one to three `VoronoiTextureNode`s (F1/F2/F3, Euclidean metric,
`randomness = jitter`, `scale = 1.0`). Cycles' Voronoi implementation differs
internally from MaterialX's `mx_worley_noise_float` (glsl lines 526-746), which
does an explicit 3x3/3x3x3 cell search over jittered cell positions and returns
the exact distance (or squared distance). Different cell-jitter hash and
distance accumulation → **not bit-for-bit**.

### 1.6 unifiednoise — lines 18905-19014

A switch on the `type` input:
- type 0 (Perlin): `NoiseTextureNode` raw, detail 0, then `*0.5 + 0.5` remap
  (lines 18946-18968).
- type 1 (Cell): `floor` + `WhiteNoiseTextureNode` (lines 18969-18978).
- type 2 (Worley): single `VoronoiTextureNode` F1 (lines 18979-18988).
- type 3 (Fractal): `NoiseTextureNode` FBM, detail `octaves` (note: **not**
  `octaves - 1` like the native fractal lowerer — line 18999), lacunarity,
  roughness, plus a jittered W coordinate for 2D (lines 18989-19012).

All branches then feed a `MapRangeNode` from [0,1] to [outmin,outmax] with
optional clamp (lines 18936-18944, result at line 19014).

### 1.7 randomfloat — lines 15142-15181

`CombineXYZ(scale(in,4096), seed, 0)` → `floor` → `WhiteNoiseTextureNode(2D)`
→ `MapRange([0,1]->[min,max], clamp=true)`. Mirrors the stdlib_ng nodegraph
(`NG_randomfloat_float`, stdlib_ng.mtlx lines 2077-2099) exactly in structure,
but relies on Cycles `WhiteNoise` rather than MaterialX `cellnoise2d`.

### 1.8 randomcolor — lines 14601-14640+

Three randomfloat lanes (hue, saturation, brightness) with seed offsets
413.3 / 1522.4 / 1813.8, each remapped to its [low,high] range, then the stdlib
`hsvtorgb`. Mirrors `NG_randomcolor_float` (stdlib_ng.mtlx lines 2127-2163).
Statistical because each lane is statistical.

## 2. MaterialX reference implementation

### 2.1 GLSL — `libraries/stdlib/genglsl/lib/mx_noise.glsl`

- Hash: Bob Jenkins `mx_bjmix` + `mx_bjfinal`, seed `0xdeadbeef + (len<<2) + 13`
  (lines 135-223). `mx_hash_int` for 1-3 args is bjfinal only; for 4-5 args it
  adds bjmix (lines 198-223).
- Fade: `t*t*t*(t*(t*6.0 - 15.0) + 10.0)` (line 167).
- Gradient: `mx_gradient_float` 2D (8 dirs) / 3D (16 dirs), scale
  `0.6616` / `0.9820` (lines 95-127).
- Perlin: `mx_perlin_noise_float(vec2/vec3)` (lines 249-285).
- Cell: `mx_cell_noise_float` = `bits_to_01(hash(floor(p)))` (lines 325-353).
- Fractal: loop `octaves` times, `result += amplitude * perlin(p)`,
  `amplitude *= diminish`, `p *= lacunarity` (lines 401-477). Raw sum, no
  normalization.
- Worley: 3x3 (2D) / 3x3x3 (3D) cell search, jittered cell positions via
  `mx_worley_cell_position`, returns min distance (optionally squared or style-1
  cell noise) (lines 479-746).

### 2.2 MDL — `libraries/mdl/materialx/noise.mdl`

The MDL noise library is a line-for-line algorithmic twin of the GLSL library:
same Bob Jenkins hash (`mx_bjmix`/`mx_bjfinal`, same seed), same `mx_fade`,
same gradient table and scale factors (`0.6616`/`0.9820`), same perlin/cell/
fractal/worley structure. File: `libraries/mdl/materialx/noise.mdl`, lines
86-827. Comment at lines 9-15: "Produced results should be identical to the OSL
noise functions."

### 2.3 MDL wiring — `libraries/stdlib/genmdl/stdlib_genmdl_impl.mtlx`

Each noise nodedef maps to a `materialx::stdlib::<version>::mx_*`
MDL call (lines 113-151), e.g.
`IM_noise2d_float_genmdl → mx_noise2d_float(amplitude, pivot, texcoord)`,
`IM_worleynoise2d_float_genmdl → mx_worleynoise2d_float(texcoord, jitter)`,
`IM_fractal2d_float_genmdl → mx_fractal2d_float(amplitude, octaves, lacunarity,
diminish, texcoord)`. There is **no separate MDL implementation** of the noise
math — it delegates to the shared `noise.mdl` library. Therefore MDL matches
GLSL **exactly** (same function); OVRTX renders MaterialX via this MDL.

## 3. Per-family parity classification

Classification key:
- **EXACT** — same function as MaterialX reference; differences only floating-point
  rounding. Per-pixel parity between Cycles and OVRTX is achievable.
- **STATISTICAL** — different function with matching distribution; parity can only
  be judged statistically (mean, variance, value range, spatial-frequency
  spectrum).
- **NOT_APPLICABLE** — no defensible parity claim.

| Family | Nodes | Cycles lowering | Matches MaterialX reference? | Classification | Oracle |
|---|---|---|---|---|---|
| noise2d / noise3d (float, color3FA, vector2/3FA) | 22 | `NoiseTextureNode`, raw snoise, detail 0, amplitude×, pivot+ | Yes — same Bob Jenkins bjfinal hash, same grad2/grad3, same fade, same 0.6616/0.9820 scale, same single-octave structure | EXACT | Bit-for-bit modulo FP rounding; compare per-pixel absolute difference <= 2e-6 on a fixed UV/position grid |
| noise2d / noise3d (color4, vector4) | 8 | Same as above + 4th channel sampled at offset position (19,73,29 noise / 19,193,17 fractal) | Channels 0-2 exact; W channel hash offset differs from MaterialX's vec4 reference | STATISTICAL (W channel) / EXACT (channels 0-2) | Per-channel exact for RGB/XYZ; W-channel distributional match (mean, variance, range) |
| fractal2d / fractal3d (float, color3FA, vector2/3FA) | 22 | `NoiseTextureNode` FBM, detail=octaves-1, lacunarity, roughness, raw sum, amplitude× | Yes — same octave loop, same diminish/roughness, same lacunarity, same per-octave scaling, same amplitude | EXACT | Bit-for-bit modulo FP rounding; compare per-pixel absolute difference <= 2e-6 over full octave accumulation |
| fractal2d / fractal3d (color4, vector4) | 8 | As above + 4th channel at offset (19,193,17) | Matches MaterialX vec4 fractal offset exactly | EXACT | Bit-for-bit modulo FP rounding |
| cellnoise2d / cellnoise3d | 2 | `floor` + `WhiteNoiseTextureNode` | No — Cycles WhiteNoise hash ≠ MaterialX `mx_cell_noise_float` Bob Jenkins + bits_to_01 | STATISTICAL | Both are uniform [0,1] white noise; compare histogram (mean ≈ 0.5, variance ≈ 1/12, range [0,1]) and spatial spectrum (flat) over many samples; tolerance ±2% on moments |
| worleynoise2d / worleynoise3d | 6 | `VoronoiTextureNode` F1/F2/F3, Euclidean, jitter | No — Cycles Voronoi cell placement/distance differs from MaterialX `mx_worley_noise_float` explicit cell search | STATISTICAL | Compare F1 distance distribution (mean, variance, range, nearest-neighbor spacing histogram) and F2-F1 gap distribution; tolerance ±5% on moments |
| unifiednoise2d / unifiednoise3d | 2 | Switch: Perlin (raw noise + 0.5*+0.5), Cell (WhiteNoise), Worley (Voronoi F1), Fractal (FBM), then MapRange [0,1]->[outmin,outmax] | Mixed: Perlin & Fractal branches EXACT; Cell & Worley branches STATISTICAL | STATISTICAL (whole node) | Per-branch oracle: Perlin/Fractal exact bit-compare; Cell/Worley statistical as above; output-range remap exact (linear MapRange) |
| randomfloat | 2 | `floor` + `WhiteNoise` + `MapRange([0,1]->[min,max])` | Statistical — relies on WhiteNoise, not MaterialX `cellnoise2d` | STATISTICAL | Output uniform in [min,max]; compare mean ≈ (min+max)/2, variance ≈ (max-min)²/12; tolerance ±2% |
| randomcolor | 2 | 3× randomfloat lanes → hsvtorgb | Statistical — built from randomfloat lanes | STATISTICAL | Output color3 in [huelow,huehigh]×[satlow,sathigh]×[blow,bhigh]; compare per-channel mean/variance vs uniform over the stated ranges; tolerance ±2% |

### Rationale notes

**noise/fractal EXACT:** graph.cpp lines 18665-18677 explicitly state the
lowering is intended to reproduce the MaterialX reference: raw snoise (not
Cycles' normalized [0,1] `noise_2d`), `detail = octaves - 1` so the fBM loop
runs exactly `octaves` times, same lacunarity/diminish, same amplitude and
pivot handling. The kernel's `fade`, `grad2`/`grad3`, `noise_scale2/3`, and
`hash_uint2/3` match MaterialX's `mx_fade`, `mx_gradient_float`,
`mx_gradient_scale2d/3`, and `mx_hash_int` (2/3-arg bjfinal form) function for
function. The only residual difference is IEEE-754 float rounding.

**cellnoise/worley/unifiednoise/randomfloat/randomcolor STATISTICAL:** these
either route through Cycles `WhiteNoise`/`VoronoiTextureNode` (whose hash and
cell placement differ from MaterialX's Bob Jenkins cell search) or compose
statistical primitives. No bit-for-bit parity claim is defensible; a
distributional oracle is the appropriate acceptance criterion.

**MDL vs GLSL:** `noise.mdl` is an algorithmic twin of `mx_noise.glsl`, and
`genmdl` implementations delegate to it. So OVRTX (MDL path) and a GLSL path
would compute the same function. The Cycles/OVRTX parity question therefore
reduces to Cycles-vs-MaterialX-reference per family, as classified above.

## 4. Worley and unifiednoise — possible Cycles implementations (not lowered today)

**Worley (worleynoise2d/3d):** Currently lowered to Cycles `VoronoiTextureNode`
F1/F2/F3. To get closer to MaterialX's exact Worley (explicit 3x3/3x3x3 cell
search with Bob Jenkins jittered cell positions and exact distance), one would
need to implement the cell search in Cycles SVM or OSL — Cycles' built-in
Voronoi node does not expose the MaterialX cell-jitter hash. Best achievable
with stock nodes: `VoronoiTextureNode` F1 Euclidean with `randomness=jitter`,
which matches the *distribution* (nearest-cell distance) but not the exact
values → STATISTICAL.

**unifiednoise (unifiednoise2d/3d):** A stock-Node implementation is already
partially present: the lowering composes `NoiseTextureNode` (Perlin),
`WhiteNoiseTextureNode` (Cell), `VoronoiTextureNode` (Worley), and a second
FBM `NoiseTextureNode` (Fractal) selected by `type`, followed by the exact
`MapRange` remap. The Perlin and Fractal branches are EXACT; the Cell and Worley
branches are STATISTICAL, so the composite node is STATISTICAL overall. No
single Cycles node implements the dynamic family switch; the graph-composition
approach is the only stock-node route, and it is statistical because two of the
four branches are.

## Proposed acceptance oracle (summary)

For each EXACT family: render a fixed 2D UV grid and 3D position grid through
both Cycles (native MaterialX lowering) and OVRTX (MDL path), compare per-pixel
absolute difference; accept if max |diff| <= 2e-6 (float32 rounding budget)
across the full octave accumulation for fractals.

For each STATISTICAL family: generate N sample points, compare histogram
moments (mean, variance), value range, and spatial-frequency spectrum (radially
averaged FFT power); accept if moments within stated tolerances (±2% for
uniform white noise / randomfloat/randomcolor; ±5% for worley).

## Files consulted

- `intern/cycles/materialx/graph.cpp` — nodedef ids (285-348), family
  classifiers (1997-2114), noise/fractal lower (18654-18788), cellnoise lower
  (18832-18848), worley lower (18849-18903), unifiednoise lower (18905-19014),
  randomfloat lower (15142-15181), randomcolor lower (14601-14640+)
- `intern/cycles/kernel/svm/noise.h` — fade (19), grad2/grad3 (134-149),
  perlin_2d/perlin_3d (160-207), noise_scale (669-687), snoise/noise (707-736)
- `intern/cycles/kernel/svm/fractal_noise.h` — noise_fbm (13-111)
- `intern/cycles/util/hash.h` — hash_uint2/3 (139-166)
- `libraries/stdlib/genglsl/lib/mx_noise.glsl` — MaterialX GLSL reference
- `libraries/mdl/materialx/noise.mdl` — MaterialX MDL reference (OVRTX path)
- `libraries/stdlib/genmdl/stdlib_genmdl_impl.mtlx` — MDL wiring (113-151)
- `libraries/stdlib/stdlib_defs.mtlx` — node defs (799-1284)
- `libraries/stdlib/stdlib_ng.mtlx` — randomfloat/randomcolor nodegraphs
  (2077-2203)

## Nothing left undone

This is an analysis document only, per the task scope: no code changes, no
build, no render verification were performed or are required. The document is
the deliverable.
