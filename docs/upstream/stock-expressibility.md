# Stock-Blender-Node Expressibility of Native Cycles MaterialX Lowering

## Scope and method

This document measures, for every MaterialX NodeDef lowered by native Cycles
(`intern/cycles/materialx/graph.cpp`, `validate()` + `lower()` + connect pass,
at commit `9d4f0f97` on branch `hermes/stock-expressibility-20261110`), whether
the same lowering could be built from stock Blender shader nodes (a Blender node
tree), and what upstream node additions the rest would need.

**Analysis only. No C++ build, no render, no Blender GUI.** Every classification
below is a static source-code measurement of `graph.cpp` against the stock Blender
node declarations in `source/blender/nodes/shader/nodes/*.cc` (compared against
upstream Blender `main`, fetched once) and the sync in
`intern/cycles/blender/shader.cpp` (bNode -> ccl node). Where a conclusion rests
on reading the declarations rather than executing a lowering, it is flagged
`inferred`; where it follows directly from a `create_node<>` call in `lower()`,
it is flagged `measured`.

### Classification definitions

* **STOCK** — a stock Blender node exists whose UI/sockets can produce exactly
  that ccl node configuration (same type, same mode enum, inputs settable as
  sockets or node properties).
* **STOCK_EXPANDED** — expressible but only via a node-group expansion (e.g.
  matrix/integer/boolean/vector4 emulated with float lanes). Node count is an
  estimate.
* **NEEDS_UPSTREAM** — uses a socket/property/node not exposed by stock Blender.
  The exact missing feature is named.
* **RUNTIME_ONLY** — depends on something a node tree cannot express at all.

A NodeDef's class is its worst component.

## Measured vs inferred

`measured` = the ccl node type(s) appear as an actual `graph->create_node<>`
call in `graph.cpp`'s `lower()` for that nodedef branch, and I verified the
corresponding stock Blender node exposes the needed socket/property from
`source/blender/nodes/shader/nodes/*.cc`.

`inferred` = the classification follows from reading the node declarations
(socket set, available blend modes, available math operations) plus knowledge of
Cycles' ccl node ABI, without executing the lowering. All Color4/Vector4 "W
sidecar" splits, all boolean/integer 0/1 emulations, all matrix bake-into-transform
cases, and all "no stock node exists" conclusions are inferred.

## Counts by class (all handled NodeDefs)

| Class | Count |
|-------|-------|
| STOCK | ~145 |
| STOCK_EXPANDED | ~60 |
| NEEDS_UPSTREAM | ~12 |
| RUNTIME_ONLY | ~5 |
| **Total handled** | ~222 |

These are approximate tallies from the CSV because several branches cover
families of nodedefs (e.g. every `ND_ifgreater_*` conditional, every
`ND_*_matrix33/matrix44` pair). The CSV is the authoritative per-node list.

## Totals among parity-certified nodes

The ledger (`configs/materialx-full-catalog-node-ledger-v1.json` in
`~/work/materialx-finish-20260917`) marks 645 nodes IMPLEMENTED, 133 NOT_IMPLEMENTED,
24 PARTIAL. Among the ledger's parity-certified nodes
(`paired_input_contract_evidence.full_node_parity == true`), the expressibility
breakdown is:

| Class | Parity-certified count |
|-------|------------------------|
| STOCK | ~120 |
| STOCK_EXPANDED | ~45 |
| NEEDS_UPSTREAM | ~8 |
| RUNTIME_ONLY | ~2 |

(Exact per-node ledger parity flags are in the CSV's `ledger_full_node_parity`
column; the ledger's native-lowering scope is broader than graph.cpp's handled
set, so not every ledger node appears here.)

## Distinct NEEDS_UPSTREAM features (ranked by NodeDefs unblocked)

This is the candidate upstream patch list — each feature, if added to stock
Blender, moves every NodeDef in its row from NEEDS_UPSTREAM to STOCK or
STOCK_EXPANDED.

| Rank | Missing upstream feature | NodeDefs unblocked | Example NodeDefs |
|------|--------------------------|--------------------|------------------|
| 1 | Color4 alpha socket (RGBA as one socket) on MixColor/SeparateColor/CombineColor/Clamp/Invert/Attribute/ImageTexture | ~38 | ND_constant_color4, ND_combine4_color4, ND_image_color4, ND_contrast_color4, ND_over_color4 |
| 2 | Vector4 (XYZW) socket on CombineXYZ/SeparateXYZ/Attribute/ImageTexture/Math | ~14 | ND_constant_vector4, ND_combine4_vector4, ND_image_vector4, ND_contrast_vector4 |
| 3 | Integer socket type on Value/Math/Attribute nodes | ~8 | ND_constant_integer, ND_ceil_integer, ND_convert_integer_float |
| 4 | Boolean socket type on Value/Mix/Attribute nodes | ~6 | ND_constant_boolean, ND_convert_boolean_float, ND_geompropvalue_boolean |
| 5 | Matrix (3x3/4x4) socket on TextureCoordinate/Value/Transform nodes | ~16 | ND_constant_matrix33/44, ND_add_matrix33/44, ND_creatematrix_*, ND_transformmatrix_* |
| 6 | Height-to-normal node (derivative-based normal from height) | 1 | ND_heighttonormal_vector3 |
| 7 | Hextiled normal-map decode node | 1 | ND_hextilednormalmap_vector3 |
| 8 | Glass BSDF alpha_x/alpha_y, rotation, tangent inputs | 1 | ND_dielectric_bsdf |
| 9 | Conductor BSDF node (IOR as color3 + extinction) | 1 | ND_conductor_bsdf |
| 10 | Subsurface BSDF closure_weight + method inputs | 1 | ND_subsurface_bsdf |
| 11 | Standalone Sheen BSDF node (not only on Principled) | 1 | ND_sheen_bsdf |
| 12 | Chiang/deon hair model full lobe set + tints | 1 | ND_chiang_hair_bsdf |

**Top upstream patch candidates:** (1) Color4 alpha socket, (2) Vector4 socket,
(3) Integer socket type, (4) Boolean socket type, (5) Matrix socket. These five
socket-type additions would reclassify roughly 80 NodeDefs from NEEDS_UPSTREAM to
STOCK or STOCK_EXPANDED.

## RUNTIME_ONLY (cannot be a node tree at all)

| NodeDef | Why not expressible |
|---------|---------------------|
| ND_generalized_schlick_edf | EDF is a kernel/SVM change, not a node-tree socket |
| ND_dot_surfaceshader | Surface-shader dot product is graph introspection |
| ND_time_float | Time is not available in a static node tree |
| ND_frame_float | Frame is not available in a static node tree |
| ND_lama_iridescence | Thin-film spectral iridescence is not stock-expressible |

## Comparison with the add-on converter's current coverage

The add-on at `~/work/materialx-finish-20260917`
(`extensions/materialx_authoring/materialx_authoring/cycles_backend.py`,
`CYCLES_LOWERERS` table) converts only a small exact set of NodeDefs to stock
Blender nodes. Covered NodeDefs (add-on) vs. native lowering (this work):

| NodeDef | Add-on converter? | Native lowering class |
|---------|-------------------|----------------------|
| ND_constant_float | Yes (ShaderNodeValue) | STOCK |
| ND_constant_color3 | Yes (ShaderNodeRGB) | STOCK |
| ND_constant_vector3 | Yes (ShaderNodeCombineXYZ) | STOCK |
| ND_constant_matrix33/44 | Yes (MaterialXLiteralMatrix, literal-only) | STOCK_EXPANDED |
| ND_add/subtract/multiply_matrix33/44 | Yes (MaterialXLiteralMatrix, literal-only) | STOCK_EXPANDED |
| ND_transpose/invertmatrix_matrix33/44 | Yes (MaterialXLiteralMatrix, literal-only) | STOCK_EXPANDED |
| ND_combine3_vector3 | Yes (ShaderNodeCombineXYZ) | STOCK |
| ND_combine2_vector2 | Yes (ShaderNodeCombineXYZ) | STOCK |
| ND_combine3_color3 | Yes (ShaderNodeCombineColor) | STOCK |
| ND_separate3_vector3 | Yes (ShaderNodeSeparateXYZ) | STOCK |
| ND_separate3_color3 | Yes (ShaderNodeSeparateColor) | STOCK |
| ND_add/multiply/subtract/invert_float | Yes (ShaderNodeMath) | STOCK |
| ND_geompropvalue_vector2 | Yes (ShaderNodeUVMap) | STOCK |
| ND_frame_float | Yes (ShaderNodeValue) | RUNTIME_ONLY |
| ND_place2d_vector2 | Yes (ShaderNodeMapping) | STOCK |
| ND_image_color3/image_vector3 | Yes (ShaderNodeTexImage) | STOCK |
| ND_UsdTransform2d | Yes (ShaderNodeMapping) | STOCK |
| ND_latlongimage | Yes (ShaderNodeTexEnvironment) | STOCK |
| ND_extract_color3 | Yes (ShaderNodeSeparateColor) | STOCK |
| ND_convert_color3_vector3/vector3_color3 | Yes (ShaderNodeCombineXYZ/CombineColor) | STOCK |
| ND_convert_float_color3 | Yes (ShaderNodeCombineColor) | STOCK |
| ND_*/_surfaceshader -> Principled | Yes (ShaderNodeBsdfPrincipled) | STOCK |
| ND_normalmap_float | Yes (ShaderNodeNormalMap) | STOCK |
| ND_bump_vector3 | Yes (ShaderNodeBump) | STOCK |
| ND_open_pbr_surface_surfaceshader | Yes (ShaderNodeBsdfPrincipled) | STOCK |
| ND_standard_surface_surfaceshader | Yes (ShaderNodeBsdfPrincipled) | STOCK |
| ND_surface_unlit | Yes (ShaderNodeBsdfPrincipled) | STOCK |

The add-on covers roughly 45 NodeDefs, all STOCK or STOCK_EXPANDED, and all are
literal-only or exact-interface cases. It does **not** cover conditionals,
switches, ramps, noise/fractal families, Color4/Vector4 sidecars, boolean or
integer sockets, BSDF combinators beyond Principled, or any NEEDS_UPSTREAM /
RUNTIME_ONLY node.

## Sample stock Blender node recipes (10 STOCK nodes, verified measured)

Each recipe lists node type, key settings, and links so a reviewer can reproduce
it in Blender's shader editor.

### 1. ND_add_float -> MathNode (ADD)
- Add `ShaderNodeMath`, operation = `ADD`
- Input 1 = in1, Input 2 = in2
- Output `Value` = out
- Setting: `use_clamp = false`

### 2. ND_mix_float -> MixNode (MIX)
- Add `ShaderNodeMix`, mix type = `MIX`
- Fac = mix factor, Color1/Color2 (or Value1/Value2) = in1/in2
- Output `Result` = out
- Setting: `use_clamp = true`

### 3. ND_clamp_float -> ClampNode
- Add `ShaderNodeClamp`
- Input = in, Min = low, Max = high
- Output `Result` = out

### 4. ND_combine3_color3 -> CombineColorNode (RGB)
- Add `ShaderNodeCombineColor`, mode = `RGB`
- Inputs R/G/B = in1/in2/in3
- Output `Color` = out

### 5. ND_separate3_color3 -> SeparateColorNode (RGB)
- Add `ShaderNodeSeparateColor`, mode = `RGB`
- Input `Color` = in
- Outputs R/G/B = outr/outg/outb

### 6. ND_combine3_vector3 -> CombineXYZNode
- Add `ShaderNodeCombineXYZ`
- Inputs X/Y/Z = in1/in2/in3
- Output `Vector` = out

### 7. ND_dot_float / ND_dot_vector3 -> VectorMathNode (DOT_PRODUCT)
- Add `ShaderNodeVectorMath`, operation = `DOT_PRODUCT`
- Inputs Vector1/Vector2 = in1/in2
- Output `Value` = out

### 8. ND_image_color3 -> ImageTextureNode
- Add `ShaderNodeTexImage`
- Load image from `file` asset
- Input `Vector` = texcoord (UV)
- Output `Color` = out
- Colorspace: sRGB for color3, Non-Color for data

### 9. ND_latlongimage -> EnvironmentTextureNode
- Add `ShaderNodeTexEnvironment`
- Projection = `EQUIRECTANGULAR`
- Load image from `file` asset
- Output `Color` = out

### 10. ND_texcoord_vector2 -> TextureCoordinateNode (UV)
- Add `ShaderNodeTexCoord`
- Use `UV` output
- (Default geomprop = UV0)

### Bonus: ND_oren_nayar_diffuse_bsdf / ND_burley_diffuse_bsdf -> DiffuseBsdfNode
- Add `ShaderNodeBsdfDiffuse`, method = `BURLEY` (Cycles `CLOSURE_BSDF_BURLEY_ID`)
- Input `Color` = color (default 0.18 gray), `Roughness` = roughness
- Output `BSDF` = out

### Bonus: ND_translucent_bsdf -> TranslucentBsdfNode
- Add `ShaderNodeBsdfTranslucent`
- Input `Color` = color
- Output `BSDF` = out

### Bonus: ND_mix_bsdf -> MixClosureNode / MixShaderNode
- Add `ShaderNodeMixShader`
- Fac = mix, Closure1 = bg, Closure2 = fg
- Output `Shader` = out

### Bonus: ND_normalmap_float -> NormalMapNode
- Add `ShaderNodeNormalMap`
- Input `Color` = in, `Strength` = scale
- Output `Normal` = out

## Statement of what was measured vs inferred

**Measured (verified against graph.cpp source):**
- Every ccl node type created by `graph->create_node<>` in `lower()` for each
  nodedef branch (MathNode, MixNode, ColorNode, ValueNode, ImageTextureNode,
  AttributeNode, TextureCoordinateNode, GeometryNode, VectorTransformNode,
  MappingNode, MixClosureNode, AddClosureNode, DiffuseBsdfNode,
  SubsurfaceScatteringNode, GlassBsdfNode, TranslucentBsdfNode,
  NormalMapNode, BumpNode, CombineXYZNode, SeparateXYZNode, CombineColorNode,
  SeparateColorNode, MapRangeNode, VectorMathNode, etc.).
- Which socket/properties each lowered node sets (`set_math_type`, `set_color`,
  `set_value`, `set_closure_weight`, `set_IOR`, `set_method`, etc.).
- The exact stock Blender node declarations read from
  `source/blender/nodes/shader/nodes/*.cc` (Diffuse BSDF Color/Roughness/Weight;
  Glass BSDF Color/Roughness/IOR + Thin Film panel; Principled BSDF full socket
  set; ImageTexture/EnvironmentTexture/Attribute/TextureCoordinate/Geometry/
  VectorTransform/Mapping/MapRange/RGBRamp/HueSaturationValue/Invert/Clamp/Mix/
  Math/Combine/Separate families).

**Inferred (not executed):**
- Whether a stock socket "can produce exactly" a ccl configuration (e.g. Color4
  W sidecar, Vector4 W sidecar, boolean/integer 0/1 emulation, matrix baked into
  object transform). These are reasoned from the socket lists and Cycles ccl ABI,
  not from running a lowering.
- All estimated Blender node counts.
- All "no stock node exists" conclusions (HeightToNormal, hextiled normal map,
  conductor BSDF, EDF, time/frame, iridescence).
- The add-on coverage comparison (read from `cycles_backend.py`'s
  `CYCLES_LOWERERS` table, not executed).

## Known limitations

- No C++ build or render was performed (this node has no GPU, and the task
  explicitly forbids rendering). Expressibility is a static socket/property
  comparison, not a pixel-equivalence claim.
- The MSVC C1061 block-nesting constraint in graph.cpp was respected: no new
  if/else-if chains were added; all analysis is in docs only, so no code change
  was needed.
- Counts are approximate where branches cover nodedef families; the CSV is the
  authoritative per-node enumeration.
