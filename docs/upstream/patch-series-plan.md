# MaterialX/Cycles upstream patch-series plan (blnd02)

This plan decomposes the current MaterialX/Cycles work into an honest patch series
whose patches are small enough to review, build independently, and keep measured by
the existing test/oracle lanes. It is based on reading the current repository, not
on Blender Foundation process claims.

## Scope and evidence read

Current measured core size in this worktree:

- `intern/cycles/materialx/graph.cpp`: 26,871 lines.
- `intern/cycles/materialx/usdshade_reader.cpp`: 22,354 lines.
- `intern/cycles/materialx/graph.h`: 201 lines.
- `intern/cycles/materialx/usdshade_reader.h`: 65 lines.
- `intern/cycles/test/materialx_graph_test.cpp`: 18,190 lines, 365 `TEST(...)` cases.
- `intern/cycles/test/materialx_usdshade_reader_test.cpp`: 25,992 lines, 448 `TEST(...)` cases.

Repo evidence for the proposed split:

- The existing MaterialX library boundary is already its own CMake target,
  `cycles_materialx`, with `authority.cpp` and `graph.cpp` always built and
  `authority_pipeline.cpp`/`usdshade_reader.cpp` added only under `WITH_USD`
  (`intern/cycles/materialx/CMakeLists.txt:12-39`).
- The public graph IR is small: `Type`, `Link`, `SelectedOutput`, `Node`, and
  `Graph` are declared in `intern/cycles/materialx/graph.h:20-197`.
- The USDShade reader has exactly two public entry points:
  `read_usdshade_graph()` and `resolve_manifest_outputs()`
  (`intern/cycles/materialx/usdshade_reader.h:31-61`).
- The reader is optional with USD/MaterialX in tests: the reader test is appended
  only when both are enabled (`intern/cycles/test/CMakeLists.txt:58-66`).
- Blender integration is a separate seam: `shader.cpp` calls
  `build_materialx_graph_from_id_authority()` under `WITH_USD` and otherwise
  falls back to regular node-tree/default material construction
  (`intern/cycles/blender/shader.cpp:1738-1748`).
- The Blender authority bridge reads explicit material ID properties and a USDA
  `Text` datablock (`intern/cycles/blender/materialx_authority.cpp:66-104`),
  while the Python operator creates those properties from a Blender USD export
  (`intern/cycles/blender/addon/operators.py:234-287`).
- The current `lower()` function already records that some branches were hoisted
  only to avoid MSVC C1061 nesting (`intern/cycles/materialx/graph.cpp:12529-12535`).
  A reviewable series should not preserve the single 26k-line implementation as
  the final shape.
- The current reader already documents atomic terminal discovery and commit
  (`intern/cycles/materialx/usdshade_reader.cpp:20973-20981` and
  `intern/cycles/materialx/usdshade_reader.cpp:22013-22064`). That should remain
  a seam and a testable invariant.
- The current manifest resolver is a separate feature: it authenticates render
  context, reachability, exact NodeDef, output name, and USD type before creating
  probe inputs (`intern/cycles/materialx/usdshade_reader.cpp:22175-22349`).
- The existing release runbook already says production patches must exclude
  `tools/materialx/**`, delivery docs, Horde/controller/ledger state, add-on RQ,
  generated output, and private topology artifacts
  (`release/materialx_upstream_patch_runbook.md:11-18`).
- The machine patch-series JSON marks static NodeDef counts as route-specific and
  not runtime qualification (`release/materialx_upstream_patch_series.v1.json:68-73`)
  and forbids claim sources such as dashboards, historical Horde receipts, and
  static NodeDef count alone (`release/materialx_upstream_patch_series.v1.json:153-166`).

## What is not claimed

This repository contains no `CONTRIBUTING.md`, and I did not find Blender developer
submission rules in-tree. Therefore every statement below about "reviewable" means
only: small enough for a human reviewer to read in one sitting, independently
buildable, and locally justified by source/test evidence in this repository. It is
UNVERIFIED whether Blender's upstream process would prefer different patch order,
commit style, review venue, squashing, or feature-flag policy. Confirm that from
current Blender developer documentation and the eventual reviewers, not from this
repo.

## Natural seams

The existing two-file split, `usdshade_reader.cpp` versus `graph.cpp`, is a real
semantic split but not sufficient.

1. Reader seam: USDShade -> renderer-neutral IR.
   - It handles USD types, contexts, `UsdShadeMaterial`, terminal discovery,
     reachable shader paths, and literal-or-connected input decoding.
   - It should not construct Cycles nodes.
   - It is naturally split further into: terminal enumeration, typed output
     readers, scalar/color/vector/matrix value readers, texture readers, closure
     readers, surface-model readers, and manifest resolution.

2. Graph seam: IR validation + IR -> Cycles `ShaderNode` graph.
   - It handles exact semantic boundaries, acyclicity, typed link validation,
     sidecar Color4/Vector4 channels, and Cycles node construction/connection.
   - It is naturally split further into: IR/types, validation helpers,
     validation modules by family, lowering helpers, lowering modules by family,
     and terminal wiring.
   - Keeping all families inside one `lower()` chain carries the same MSVC C1061
     risk recorded in the current code. The final upstream shape should dispatch
     through family lowerer functions/tables, not through one deeply nested body.

3. Authority seam: authenticated source contract.
   - `authority.cpp` validates UUID/digest/material path/manifest shape.
   - `authority_pipeline.cpp` parses the USDA string and calls either the whole
     material reader/lowerer or the manifest-output resolver.
   - This can be reviewed independently from most node-family semantics.

4. Blender bridge seam.
   - C++ authority lookup and Cycles graph selection are separate from the Python
     convenience operator. The C++ bridge is production runtime plumbing; the
     operator is authoring UX and should not be required for the first runtime
     patch.

5. Test seam.
   - `materialx_graph_test.cpp` tests IR validation/lowering without USD.
   - `materialx_usdshade_reader_test.cpp` tests real `pxr::UsdStage`/USDShade
     parsing and is optional under `WITH_USD AND WITH_MATERIALX`.
   - The current files are too large to keep as two monoliths. New upstreamable
     tests should be split along the same family seams as the implementation.

## Smallest acceptable first patch

The smallest first patch that stands alone and is useful is a vertical slice:

`Cycles: add authenticated MaterialX USDShade OpenPBR constants/literals`

It should contain:

- The `cycles_materialx` target and small public IR (`Type::{Float, Color3,
  SurfaceShader}`, `Link`, `FloatInput`, `Color3Input`, `Node`, `Graph`).
- Authority digest validation sufficient to bind one USDA `Text` datablock to one
  material path.
- A USDShade reader for one connected surface terminal whose source is
  `ND_open_pbr_surface_surfaceshader` or a declared one-hop OpenPBR inherit, with
  literal float/color3 inputs for the direct Cycles-equivalent OpenPBR subset.
- A lowerer that creates one `PrincipledBsdfNode` and connects it to the Cycles
  `Surface` output.
- The C++ Blender authority bridge so the feature is actually reachable when a
  material carries the explicit ID-property contract.
- A narrow test set: valid OpenPBR literal material, invalid digest, missing
  material path, unsupported surface model, and fallback to the existing Blender
  node tree when no authority is selected.

Rough size: 2,500-3,500 production/test lines if written directly, not by copying
large unused helpers. It is useful alone because a Blender material that already
carries the authority properties can render a basic MaterialX/OpenPBR material in
Cycles; it is not merely scaffolding for patch 7.

I would not make the authority-only code the first patch. It is smaller, but it is
not independently useful to Blender until something can be rendered from it.

## Required final file layout before upstream review

Do not upstream the current `graph.cpp`/`usdshade_reader.cpp`/two-test-file shape.
A reviewer should see modules with bounded responsibilities, for example:

- `intern/cycles/materialx/graph.h` and `graph.cpp`: IR structs and tiny public
  `validate()`/`lower()` dispatch only.
- `intern/cycles/materialx/graph_validate.cc`: graph-level checks, acyclicity,
  shared typed-link helpers.
- `intern/cycles/materialx/graph_lower.cc`: shared lower dispatch, terminal
  wiring, rollback/error handling.
- `intern/cycles/materialx/lower_<family>.cc` and `validate_<family>.cc` for
  surface terminals, texture/geometry, scalar math, vector/color math, Color4,
  Vector4, integer/boolean, matrix, ramps/splits, procedural, closures.
- `intern/cycles/materialx/usdshade_reader.cpp`: public orchestration only.
- `intern/cycles/materialx/usdshade_reader_<family>.cc` for value readers,
  texture readers, surface readers, closure readers, terminal readers, manifest
  resolver.
- Split tests into matching files such as
  `materialx_graph_scalar_test.cpp`, `materialx_graph_texture_test.cpp`,
  `materialx_usdshade_reader_surface_test.cpp`, etc. Keep each test file under
  roughly the size of existing Cycles test files, not 18k-26k lines.

That file layout is not an upstream process claim; it is a technical fix for the
observed C1061/nesting and giant-file problems.

## Proposed patch series

Honest total: 34 patches for the native Cycles/USDShade core. Hydra, dashboards,
Horde scheduling, generated ledgers, and broad authoring UX are intentionally not
inside this total.

Line counts are rough patch-size budgets, not exact diffs. Each patch must build
and run its focused tests before the next patch lands. Each NodeDef-support patch
must update the oracle/qualification input used by this project so the 562/604
catalog-node status remains measurable after the patch. If that oracle is outside
this repository, the patch should record the external command and result as
UNVERIFIED-IN-REPO evidence rather than pretending the repo alone proves it.

### 1. Minimal authenticated OpenPBR vertical slice

- Contains: `cycles_materialx` target; minimal `Authority`; minimal graph IR;
  `read_usdshade_graph()` for one OpenPBR surface; `lower()` to one
  `PrincipledBsdfNode`; C++ Blender authority lookup and shader fallback.
- Depends on: none.
- Rough size: 2.5k-3.5k lines.
- Standalone value: renders a basic authenticated MaterialX/OpenPBR surface in
  Cycles and fails closed on malformed authority.
- Tests: OpenPBR literal positive; digest/path negative; unsupported model
  negative; material-without-authority fallback.

### 2. Atomic reader/lowerer contracts and named errors

- Contains: rollback-on-failure in lowerer, named validate failures, atomic reader
  commit for terminal parsing, duplicate-node/cycle guards.
- Depends on: patch 1.
- Rough size: 1k-1.5k lines.
- Standalone value: makes failures diagnosable and prevents partially committed
  MaterialX graphs.
- Tests: malformed child node does not mutate destination; duplicate names/cycles
  fail with the named node.

### 3. Typed scalar/color graph inputs for OpenPBR

- Contains: connected `Float`/`Color3` links into OpenPBR inputs; constants,
  dot/value passthroughs, add/subtract/multiply/divide/min/max for float and
  color3 where Cycles has direct nodes.
- Depends on: patch 2.
- Rough size: 2k-3k lines.
- Standalone value: nontrivial authored scalar/color networks can drive OpenPBR
  base color, weights, roughness, metalness, and emission.
- Tests: connected scalar/color chains; divide-by-zero/domain negatives.

### 4. Vector2/Vector3 coordinates and typed conversion adapters

- Contains: `Type::Vector2`, `Type::Vector3`; vector constants; vector<->color3
  and vector<->float conversions that are exact; vector dot/extract/combine.
- Depends on: patch 3.
- Rough size: 2k-3k lines.
- Standalone value: coordinate and normal-producing graphs can be represented
  without pretending vectors are colors.
- Tests: typed output sockets, invalid type mismatch, literal/linked operands.

### 5. Geometry and primvar readers

- Contains: `ND_geompropvalue_{float,color3,vector2,vector3}` and
  `ND_UsdPrimvarReader_{float,vector2,vector3}` with fallback defaults where the
  current `Node` IR supports them.
- Depends on: patch 4.
- Rough size: 1.5k-2.5k lines.
- Standalone value: MaterialX graphs can read UVs and named attributes through
  Cycles `AttributeNode` instead of baking everything as constants.
- Tests: literal geomprop/varname; fallback value; missing/connected string
  negative.

### 6. Image texture basics

- Contains: `ND_image_{float,color3,vector2,vector3}` with exact file and
  texcoord handling; address/interpolation subset that maps to Cycles; asset
  path handling.
- Depends on: patch 5.
- Rough size: 2k-3k lines.
- Standalone value: basic textured OpenPBR materials render.
- Tests: generated tiny image fixture; color/data colorspace distinction; bad
  file/type/address negatives.

### 7. Fixture-bound image authentication

- Contains: manifest fixture digest map and byte authentication for image assets,
  equivalent in shape to the current `authenticate_resolved_fixture_bytes()` seam
  (`intern/cycles/materialx/authority_pipeline.cpp:144-175`).
- Depends on: patch 6.
- Rough size: 700-1,200 lines.
- Standalone value: image fixtures used by tests/authorities are not silently
  swapped under the same USD graph digest.
- Tests: matching digest, missing digest, unreadable fixture, digest mismatch.

### 8. Displacement terminals

- Contains: scalar `ND_displacement_float`, vector `ND_displacement_vector3`, and
  material terminal parsing for displacement.
- Depends on: patches 4 and 6.
- Rough size: 1.5k-2k lines.
- Standalone value: MaterialX displacement reaches Cycles `DisplacementNode` /
  `VectorDisplacementNode` without being folded into surface color.
- Tests: scalar/vector displacement, linked scale, malformed terminal negative.

### 9. Volume terminal slice

- Contains: `ND_volume`, `ND_absorption_vdf`, `ND_anisotropic_vdf`, optional
  `ND_uniform_edf` emission, and material volume terminal handling.
- Depends on: patches 3 and 4.
- Rough size: 1.5k-2.5k lines.
- Standalone value: volume-only and surface+volume MaterialX materials lower to
  Cycles `VolumeCoefficientsNode`.
- Tests: volume-only accepted, co-authored surface+volume atomicity, linked
  absorption/scattering/anisotropy, invalid VDF negative.

### 10. Light terminal discovery only

- Contains: authenticated `ND_light`/lightshader discovery into `Graph::has_light`
  without connecting it to material surface/volume outputs.
- Depends on: patch 9.
- Rough size: 600-1,000 lines.
- Standalone value: callers can detect lightshader material content without a
  false material-output lowering.
- Tests: light-only accepted as discovered; light not wired to material output;
  unsupported light input negative.

### 11. Manifest-bound selected output resolver v1

- Contains: `SelectedOutput`, authority `render_context`/`selected_outputs`,
  `resolve_manifest_outputs()` for Float/Color3/Vector2/Vector3 only, with exact
  reachability and type checks.
- Depends on: patches 3-5.
- Rough size: 2k-3k lines.
- Standalone value: a caller can request authenticated typed outputs from inside
  a MaterialX graph without lowering a full surface material.
- Tests: exact selected output positive, wrong context, wrong path, wrong
  NodeDef, wrong output, wrong type, unreachable node, partial multi-output
  failure.

### 12. Boolean and integer exact-domain literals

- Contains: `Type::Boolean`, `Type::Integer`; constants, dot/passthrough,
  boolean/integer conversions that do not coerce through float except at display
  adapters; logical ops where exact.
- Depends on: patch 11.
- Rough size: 2k-3k lines.
- Standalone value: manifest observation and simple graphs preserve int/bool
  domains instead of treating everything as float.
- Tests: bool/int constants, exact conversions, out-of-domain int negative.

### 13. Matrix literal boundary

- Contains: `Type::Matrix33`, `Type::Matrix44`; matrix constants; affine-only
  Matrix44 validation; transform-matrix vector operations that Cycles can
  represent as `Transform`/coordinate math.
- Depends on: patches 4 and 11.
- Rough size: 2k-3k lines.
- Standalone value: matrix-valued MaterialX graph leaves can be observed and used
  at the exact affine boundary.
- Tests: Matrix33 exact, Matrix44 affine accepted, non-affine rejected,
  transformmatrix vector cases.

### 14. Float math and domains

- Contains: remaining exact float math family: unary trig/exponential/log where
  finite/domain guarded, modulo/safepower/trianglewave, clamp, abs/floor/ceil/
  round/fract/sign.
- Depends on: patch 3.
- Rough size: 2k-3k lines.
- Standalone value: most scalar utility graphs used by materials lower natively.
- Tests: literal/linked operands, domain failures, finite result checks.

### 15. Vector2/Vector3 math family

- Contains: vector add/subtract/multiply/divide/modulo, dot/cross/normalize,
  length/distance, reflect/refract where exact, scalar-second variants.
- Depends on: patch 14.
- Rough size: 2.5k-3.5k lines.
- Standalone value: coordinate/normal math graphs become usable before Color4 or
  procedural families land.
- Tests: vector math matrix; zero-length/domain negatives.

### 16. Color3 adjustment and compositing basics

- Contains: mix/plus/minus/difference/burn/dodge/screen/overlay for float/color3
  where direct or exactly composable; contrast, range/remap, smoothstep,
  luminance, hsv/rgb, saturate/hsvadjust/colorcorrect for Color3.
- Depends on: patches 14 and 15.
- Rough size: 3k-4k lines.
- Standalone value: common color-correction MaterialX networks can drive OpenPBR.
- Tests: operation matrix, gamma/range domain, connected/literal combinations.

### 17. Color4 sidecar foundation

- Contains: `Type::Color4`; constant/image/extract/combine; alpha sidecar
  helpers; Color4 -> Color3/float adapters; manifest resolver widened to Color4.
- Depends on: patches 6 and 16.
- Rough size: 2.5k-3.5k lines.
- Standalone value: RGBA textures and alpha-aware reads work without lying that
  alpha is part of RGB.
- Tests: alpha output from image/attribute, adapters, invalid alpha-source
  negative.

### 18. Color4 operations and alpha compositing

- Contains: Color4 mix/arithmetic/adjustment plus exact alpha composite nodes
  currently represented by RGB + scalar-alpha paths.
- Depends on: patch 17.
- Rough size: 3k-4k lines.
- Standalone value: compositing MaterialX Color4 graphs preserve alpha through
  Cycles node graphs.
- Tests: RGB and alpha assertions for every operation family; literal and linked
  operands; unsupported proxy cases rejected.

### 19. Vector4 foundation and operations

- Contains: `Type::Vector4`; constants, image, extract/combine, vector4 math,
  W sidecar helpers, manifest resolver widened to Vector4.
- Depends on: patches 15 and 17.
- Rough size: 3k-4k lines.
- Standalone value: non-color-role four-component data is represented separately
  from Color4.
- Tests: Vector4 XYZ+W sidecar, image_vector4, adapters, malformed W extraction
  negative.

### 20. Conditional nodes

- Contains: float-predicate, integer-predicate, and boolean-predicate conditional
  families across already-supported value types, with matrix conditionals folded
  only when exact/literal as needed.
- Depends on: patches 12, 13, 18, 19.
- Rough size: 3k-4k lines.
- Standalone value: MaterialX select/if graphs lower for all supported scalar,
  vector, color, bool/int, and matrix domains.
- Tests: predicate type matrix, linked result arms where supported, literal-only
  matrix boundary, malformed predicates.

### 21. Switch nodes

- Contains: `ND_switch_*` and `ND_switch_*I` for supported output types, with the
  same explicit selector boundary described in the current code comments.
- Depends on: patch 20.
- Rough size: 2k-3k lines.
- Standalone value: ten-way selection graphs are supported without a giant
  late-series merge.
- Tests: all selector intervals, out-of-range default, typed arms, malformed
  unselected data does not leak.

### 22. Ramp and split families

- Contains: scalar/color/vector ramp, ramp4, split, extract, combine, and their
  reader/lowerer tests.
- Depends on: patches 18-21.
- Rough size: 3k-4k lines.
- Standalone value: common authored gradients and channel routing work.
- Tests: interpolated ramp stops, split outputs, Color4/Vector4 sidecars, Blender
  ramp smoke if kept in production tests.

### 23. Procedural noise/fractal family

- Contains: native noise/fractal/cell/worley procedural nodes where exact enough
  to express with Cycles nodes and documented boundaries for the rest.
- Depends on: patches 15, 16, and 19.
- Rough size: 3k-4k lines.
- Standalone value: procedural texture graphs can be used without image files.
- Tests: deterministic seeded cases, dimensionality, amplitude/pivot/octave
  domains, unsupported procedural negative.

### 24. Procedural 2D shapes and placement

- Contains: place2d/transform2d, checker/circle/line/tiled patterns, hextiled
  coordinate helpers that do not need image sampling.
- Depends on: patches 15, 22, and 23.
- Rough size: 2.5k-3.5k lines.
- Standalone value: non-image procedural patterns and UV transforms work.
- Tests: transform conventions, tiling options, domain negatives.

### 25. Advanced texture2d: tiled, hextiled, latlong

- Contains: `ND_tiledimage_*`, hextiled image/normalmap pieces, latlongimage,
  address/interpolation subsets, and exact color/vector output mapping.
- Depends on: patches 6, 17, 19, 24.
- Rough size: 3k-4k lines.
- Standalone value: production texture patterns can render without requiring the
  entire procedural backlog.
- Tests: generated image fixtures, expected subgraph node names, mode/domain
  negatives.

### 26. Triplanar projection

- Contains: `ND_triplanarprojection_*` for supported types, using explicit three
  2D image samples rather than Cycles box projection. The current code comments
  explain why box projection is not an honest equivalent
  (`intern/cycles/materialx/graph.cpp:648-655`).
- Depends on: patches 15, 17, 19, and 25.
- Rough size: 2k-3k lines.
- Standalone value: a coherent texture3d/triplanar family lands as one readable
  patch with its known semantic boundary.
- Tests: three-file sampling, blend weights, up-axis boundary, type variants.

### 27. Surface model: `ND_surface_unlit` and convert-to-surfaceshader adapters

- Contains: surface_unlit and exact convert-to-surfaceshader wrappers.
- Depends on: patches 3, 12, and 16.
- Rough size: 1.5k-2.5k lines.
- Standalone value: unlit/emissive MaterialX surfaces render without pretending
  they are OpenPBR.
- Tests: emission/transmission/opacity defaults and unsupported non-defaults.

### 28. Surface model: Standard Surface

- Contains: `ND_standard_surface_surfaceshader` and `_100` exact subset, with
  default-only rejection for fields without direct Cycles equivalents.
- Depends on: patches 8, 16, and 22.
- Rough size: 2.5k-3.5k lines.
- Standalone value: a major interchange surface model becomes usable with named
  boundaries.
- Tests: every admitted input, default-only negatives, normal/coat-normal links.

### 29. Surface model: USD Preview Surface

- Contains: `ND_UsdPreviewSurface_surfaceshader` exact subset and default-only
  handling for unsupported workflow/opacity/normal/occlusion fields.
- Depends on: patches 5, 8, and 28.
- Rough size: 1.5k-2.5k lines.
- Standalone value: USD-native preview materials can render through the same path.
- Tests: diffuse/metallic/roughness/clearcoat/ior/emission positives;
  unsupported input negatives.

### 30. Surface model: glTF PBR

- Contains: `ND_gltf_pbr_surfaceshader` exact subset, plus dead/default-only
  field treatment matching the current comments in reader/graph.
- Depends on: patches 25 and 29.
- Rough size: 2k-3k lines.
- Standalone value: glTF metallic-roughness materials map to Cycles Principled
  where fields are genuinely equivalent.
- Tests: base/metallic/roughness/clearcoat/ior/emissive positives; alpha,
  tangent, occlusion, transmission, anisotropy negatives/default-only cases.

### 31. Surface model: Disney Principled

- Contains: `ND_disney_principled` mapping onto Cycles Principled, including
  exact color-mix handling for specularTint/sheenTint as documented in current
  comments (`intern/cycles/materialx/graph.cpp:1096-1100`).
- Depends on: patches 16 and 28.
- Rough size: 1.5k-2.5k lines.
- Standalone value: another complete PBR surface lands without coupling to LAMA
  or closure graphs.
- Tests: all 14 inputs, tint handling, unsupported/invalid domains.

### 32. BSDF/EDF leaf closures

- Contains: directly mappable BSDF leaves, EDF leaves, and `Type::BSDF`/
  `Type::SurfaceShader` closure output resolution. Keep LAMA out of this patch
  except where it is exactly the same leaf primitive.
- Depends on: patches 16 and 27.
- Rough size: 3k-4k lines.
- Standalone value: real closure graphs become observable/lowerable before
  closure composition is added.
- Tests: every leaf closure output, named output socket resolution, unsupported
  closure model negatives.

### 33. Closure composition and generic `<surface>`

- Contains: `ND_surface`, add/mix/multiply closure combinators, generic surface
  terminal, and exact rejection of layer/per-channel closure weighting where
  Cycles lacks a matching primitive. Current comments cite the Cycles limitation
  around `AddClosureNode`/`MixClosureNode` (`intern/cycles/materialx/graph.cpp:1323-1338`).
- Depends on: patch 32.
- Rough size: 3k-4k lines.
- Standalone value: authored closure graphs can be composed without proxy
  mappings.
- Tests: add/mix/multiply positives, unit-opacity constraints, layer_bsdf and
  nonuniform closure-weight negatives.

### 34. LAMA honest subset

- Contains: only LAMA nodes whose semantics have a direct Cycles equivalent or a
  provably exact degenerate subset: e.g. supported surface wrapper, emission,
  conductor/iridescence microfacet subset, mix/add where they reuse patch 33
  primitives. Exclude LAMA sheen/throughput/layering if the exact closure model
  is not present.
- Depends on: patches 31-33.
- Rough size: 2k-3.5k lines.
- Standalone value: a clearly bounded LAMA subset lands without smuggling in
  approximations.
- Tests: supported LAMA leaves/combinators; explicit negatives for sheen/layer or
  any unsupported throughput-dependent behavior.

## Series-wide gates

Each patch should have:

1. Focused gtests for the family it adds.
2. Negative tests for every deliberately unsupported semantic boundary.
3. `cycles_test --gtest_filter='materialx_graph.*'` for graph-only patches, or
   the corresponding split-test binary filter once test files are decomposed.
4. `cycles_test --gtest_filter='materialx_usdshade_reader.*'` for reader patches
   when built with `WITH_USD AND WITH_MATERIALX`.
5. A MaterialX-off/fallback check for patches touching Blender runtime selection.
6. MSVC nesting/static check for any patch that touches lowerer dispatch. The
   current code already documents C1061 pressure; the target shape should make
   this check boring by construction.
7. Oracle update/evidence for the catalog nodes newly supported by that patch.
   The task-provided 562/604 PASSING number must stay reproducible after every
   patch. If the oracle command and evidence live outside this repo, record that
   explicitly as external evidence rather than treating in-repo gtests as the
   same thing.

## Parts that should not go upstream at all

These are validation/delivery apparatus or project-private workflow, not Blender
runtime functionality:

- `tools/materialx/materialx_batch_scheduler.py` and neighboring Horde/velocity
  tools. They schedule local project work against a ledger and worker pool; the
  existing runbook already excludes `tools/materialx/**` from production patches
  (`release/materialx_upstream_patch_runbook.md:13-18`).
- `tools/materialx/materialx_horde_dispatch.py`, `materialx_velocity_manifest`,
  ledger/catalog/backlog/test-cadence scripts, and any generated completion
  manifests. They are useful for this project, not for Blender users.
- `docs/superpowers/**`, `release/materialx_upstream_patch_runbook.md`, and
  `release/materialx_upstream_patch_series.v1.json`. They are planning and audit
  artifacts. They may inform commit construction but should not be submitted as
  product code.
- Any dashboard/RQ/Horde count or static NodeDef-count claim. The JSON itself says
  static accepted-code IDs are not runtime qualification
  (`release/materialx_upstream_patch_series.v1.json:68-73`) and forbids static
  counts/dashboards as claim sources (`release/materialx_upstream_patch_series.v1.json:153-166`).
- Broad Python authoring UX beyond a minimal opt-in operator is questionable for
  a first runtime series. The current operator writes hidden material ID
  properties and a hidden USDA `Text` block (`intern/cycles/blender/addon/operators.py:257-275`).
  That may be useful as a developer bridge, but it needs separate UX/product
  review. If included at all, put it after the runtime path is accepted, not in
  the first patch.
- Large generated fixture bundles or private texture assets. Upstream tests should
  generate tiny fixtures in the test itself or use already-accepted test-data
  patterns. The fixture digest mechanism can upstream; the project-private
  corpus should not.

## Why the old 11-patch release JSON is not small enough

`release/materialx_upstream_patch_series.v1.json` groups the work into 11 broad
patches. That is useful audit metadata, but it is too coarse for the measured
98k-line core. For example, its `typed-graph-usdshade` patch covers both giant
production files and both giant test files at once
(`release/materialx_upstream_patch_series.v1.json:83-88`), and later patches mix
Hydra, graph, reader, Blender smoke, and several node families in one step
(`release/materialx_upstream_patch_series.v1.json:124-150`).

The upstreamable series above deliberately expands that into 34 patches so each
review can answer: what semantic family landed, what exact boundaries remain, and
what Blender can do now that it could not do before.
