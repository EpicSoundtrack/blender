/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/usdshade_reader.h"

#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/input.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/output.h>
#include <pxr/usd/usdShade/shader.h>

#include "util/path.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

namespace {

constexpr const char *open_pbr_surface_id = "ND_open_pbr_surface_surfaceshader";
constexpr const char *add_float_id = "ND_add_float";
constexpr const char *subtract_float_id = "ND_subtract_float";
constexpr const char *multiply_float_id = "ND_multiply_float";
constexpr const char *power_float_id = "ND_power_float";
constexpr const char *modulo_float_id = "ND_modulo_float";
constexpr const char *ifgreater_float_id = "ND_ifgreater_float";
constexpr const char *ifgreatereq_float_id = "ND_ifgreatereq_float";
constexpr const char *ifequal_float_id = "ND_ifequal_float";
constexpr const char *ifgreater_color3_id = "ND_ifgreater_color3";
constexpr const char *ifgreatereq_color3_id = "ND_ifgreatereq_color3";
constexpr const char *ifequal_color3_id = "ND_ifequal_color3";
constexpr const char *ifgreater_vector3_id = "ND_ifgreater_vector3";
constexpr const char *ifgreatereq_vector3_id = "ND_ifgreatereq_vector3";
constexpr const char *ifequal_vector3_id = "ND_ifequal_vector3";
/* MaterialX stdlib_defs.mtlx declares float-predicate vector2/color4
 * conditional siblings; graph.cpp lowers them with the same real predicate
 * select pattern as the existing float/color3/vector3 family. */
constexpr const char *ifgreater_vector2_id = "ND_ifgreater_vector2";
constexpr const char *ifgreatereq_vector2_id = "ND_ifgreatereq_vector2";
constexpr const char *ifequal_vector2_id = "ND_ifequal_vector2";
constexpr const char *ifgreater_color4_id = "ND_ifgreater_color4";
constexpr const char *ifgreatereq_color4_id = "ND_ifgreatereq_color4";
constexpr const char *ifequal_color4_id = "ND_ifequal_color4";
constexpr const char *mix_float_id = "ND_mix_float";
constexpr const char *plus_float_id = "ND_plus_float";
constexpr const char *minus_float_id = "ND_minus_float";
constexpr const char *difference_float_id = "ND_difference_float";
constexpr const char *burn_float_id = "ND_burn_float";
constexpr const char *dodge_float_id = "ND_dodge_float";
constexpr const char *screen_float_id = "ND_screen_float";
constexpr const char *overlay_float_id = "ND_overlay_float";
constexpr const char *mix_color3_id = "ND_mix_color3";
constexpr const char *plus_color3_id = "ND_plus_color3";
constexpr const char *minus_color3_id = "ND_minus_color3";
constexpr const char *difference_color3_id = "ND_difference_color3";
constexpr const char *burn_color3_id = "ND_burn_color3";
constexpr const char *dodge_color3_id = "ND_dodge_color3";
constexpr const char *screen_color3_id = "ND_screen_color3";
constexpr const char *overlay_color3_id = "ND_overlay_color3";
constexpr const char *mix_color3_color3_id = "ND_mix_color3_color3";
/* MaterialX stdlib_defs.mtlx compositing mix siblings (ND_mix_color4*,
 * ND_mix_vector2*, ND_mix_vector3_vector3) use the same input contract as
 * the existing mix_color3 family; genosl/stdlib_genosl_impl.mtlx maps them
 * to sourcecode="mix({{bg}}, {{fg}}, {{mix}})". */
constexpr const char *mix_color4_id = "ND_mix_color4";
constexpr const char *mix_color4_color4_id = "ND_mix_color4_color4";
constexpr const char *mix_vector2_id = "ND_mix_vector2";
constexpr const char *mix_vector2_vector2_id = "ND_mix_vector2_vector2";
constexpr const char *mix_vector3_id = "ND_mix_vector3";
constexpr const char *mix_vector3_vector3_id = "ND_mix_vector3_vector3";
/* MaterialX stdlib_defs.mtlx compositing mask nodes: inside = in * mask,
 * outside = in * (1 - mask), for float/color3/color4. */
constexpr const char *inside_float_id = "ND_inside_float";
constexpr const char *outside_float_id = "ND_outside_float";
constexpr const char *inside_color3_id = "ND_inside_color3";
constexpr const char *outside_color3_id = "ND_outside_color3";
constexpr const char *inside_color4_id = "ND_inside_color4";
constexpr const char *outside_color4_id = "ND_outside_color4";
constexpr const char *divide_float_id = "ND_divide_float";
constexpr const char *invert_float_id = "ND_invert_float";
constexpr const char *clamp_float_id = "ND_clamp_float";
constexpr const char *absval_float_id = "ND_absval_float";
constexpr const char *floor_float_id = "ND_floor_float";
constexpr const char *ceil_float_id = "ND_ceil_float";
constexpr const char *round_float_id = "ND_round_float";
constexpr const char *sqrt_float_id = "ND_sqrt_float";
constexpr const char *fract_float_id = "ND_fract_float";
constexpr const char *sign_float_id = "ND_sign_float";
constexpr const char *min_float_id = "ND_min_float";
constexpr const char *max_float_id = "ND_max_float";
constexpr const char *sin_float_id = "ND_sin_float";
constexpr const char *cos_float_id = "ND_cos_float";
constexpr const char *tan_float_id = "ND_tan_float";
constexpr const char *exp_float_id = "ND_exp_float";
constexpr const char *acos_float_id = "ND_acos_float";
constexpr const char *asin_float_id = "ND_asin_float";
constexpr const char *atan2_float_id = "ND_atan2_float";
constexpr const char *ln_float_id = "ND_ln_float";
constexpr const char *safepower_float_id = "ND_safepower_float";
constexpr const char *smoothstep_float_id = "ND_smoothstep_float";
constexpr const char *remap_float_id = "ND_remap_float";
constexpr const char *range_float_id = "ND_range_float";
constexpr const char *remap_color3_id = "ND_remap_color3";
constexpr const char *remap_color3fa_id = "ND_remap_color3FA";
constexpr const char *range_color3_id = "ND_range_color3";
constexpr const char *range_color3fa_id = "ND_range_color3FA";
constexpr const char *contrast_float_id = "ND_contrast_float";
constexpr const char *contrast_color3_id = "ND_contrast_color3";
constexpr const char *contrast_color3fa_id = "ND_contrast_color3FA";
constexpr const char *contrast_vector2_id = "ND_contrast_vector2";
constexpr const char *contrast_vector2fa_id = "ND_contrast_vector2FA";
constexpr const char *contrast_vector3_id = "ND_contrast_vector3";
constexpr const char *contrast_vector3fa_id = "ND_contrast_vector3FA";
constexpr const char *noise2d_float_id = "ND_noise2d_float";
constexpr const char *noise2d_color3_id = "ND_noise2d_color3";
constexpr const char *noise2d_color3fa_id = "ND_noise2d_color3FA";
constexpr const char *noise2d_vector2_id = "ND_noise2d_vector2";
constexpr const char *noise2d_vector2fa_id = "ND_noise2d_vector2FA";
constexpr const char *noise2d_vector3_id = "ND_noise2d_vector3";
constexpr const char *noise2d_vector3fa_id = "ND_noise2d_vector3FA";
constexpr const char *cellnoise2d_float_id = "ND_cellnoise2d_float";
constexpr const char *cellnoise3d_float_id = "ND_cellnoise3d_float";
constexpr const char *noise3d_float_id = "ND_noise3d_float";
constexpr const char *noise3d_color3_id = "ND_noise3d_color3";
constexpr const char *noise3d_color3fa_id = "ND_noise3d_color3FA";
constexpr const char *noise3d_vector2_id = "ND_noise3d_vector2";
constexpr const char *noise3d_vector2fa_id = "ND_noise3d_vector2FA";
constexpr const char *noise3d_vector3_id = "ND_noise3d_vector3";
constexpr const char *noise3d_vector3fa_id = "ND_noise3d_vector3FA";
constexpr const char *fractal2d_float_id = "ND_fractal2d_float";
constexpr const char *fractal3d_float_id = "ND_fractal3d_float";
constexpr const char *fractal2d_color3_id = "ND_fractal2d_color3";
constexpr const char *fractal3d_color3_id = "ND_fractal3d_color3";
constexpr const char *fractal2d_color3fa_id = "ND_fractal2d_color3FA";
constexpr const char *fractal3d_color3fa_id = "ND_fractal3d_color3FA";
constexpr const char *fractal2d_vector2_id = "ND_fractal2d_vector2";
constexpr const char *fractal3d_vector2_id = "ND_fractal3d_vector2";
constexpr const char *fractal2d_vector2fa_id = "ND_fractal2d_vector2FA";
constexpr const char *fractal3d_vector2fa_id = "ND_fractal3d_vector2FA";
constexpr const char *fractal2d_vector3_id = "ND_fractal2d_vector3";
constexpr const char *fractal3d_vector3_id = "ND_fractal3d_vector3";
constexpr const char *fractal2d_vector3fa_id = "ND_fractal2d_vector3FA";
constexpr const char *fractal3d_vector3fa_id = "ND_fractal3d_vector3FA";
/* stdlib_defs.mtlx declares four-channel procedural Perlin/fBm siblings;
 * graph.cpp lowers them as RGB plus a real sidecar fourth scalar sample, using
 * the genosl mx_noise*_vector4/mx_fractal*_vector4 definitions as authority. */
constexpr const char *noise2d_color4_id = "ND_noise2d_color4";
constexpr const char *noise2d_color4fa_id = "ND_noise2d_color4FA";
constexpr const char *noise2d_vector4_id = "ND_noise2d_vector4";
constexpr const char *noise2d_vector4fa_id = "ND_noise2d_vector4FA";
constexpr const char *noise3d_color4_id = "ND_noise3d_color4";
constexpr const char *noise3d_color4fa_id = "ND_noise3d_color4FA";
constexpr const char *noise3d_vector4_id = "ND_noise3d_vector4";
constexpr const char *noise3d_vector4fa_id = "ND_noise3d_vector4FA";
constexpr const char *fractal2d_color4_id = "ND_fractal2d_color4";
constexpr const char *fractal2d_color4fa_id = "ND_fractal2d_color4FA";
constexpr const char *fractal2d_vector4_id = "ND_fractal2d_vector4";
constexpr const char *fractal2d_vector4fa_id = "ND_fractal2d_vector4FA";
constexpr const char *fractal3d_color4_id = "ND_fractal3d_color4";
constexpr const char *fractal3d_color4fa_id = "ND_fractal3d_color4FA";
constexpr const char *fractal3d_vector4_id = "ND_fractal3d_vector4";
constexpr const char *fractal3d_vector4fa_id = "ND_fractal3d_vector4FA";
constexpr const char *checkerboard_color3_id = "ND_checkerboard_color3";
constexpr const char *rgbtohsv_color3_id = "ND_rgbtohsv_color3";
constexpr const char *hsvtorgb_color3_id = "ND_hsvtorgb_color3";
constexpr const char *remap_vector2_id = "ND_remap_vector2";
constexpr const char *range_vector2_id = "ND_range_vector2";
constexpr const char *remap_vector2fa_id = "ND_remap_vector2FA";
constexpr const char *remap_vector3_id = "ND_remap_vector3";
constexpr const char *remap_vector3fa_id = "ND_remap_vector3FA";
constexpr const char *range_vector3_id = "ND_range_vector3";
constexpr const char *range_vector3fa_id = "ND_range_vector3FA";
constexpr const char *clamp_vector2_id = "ND_clamp_vector2";
constexpr const char *clamp_vector2fa_id = "ND_clamp_vector2FA";
constexpr const char *clamp_vector3_id = "ND_clamp_vector3";
constexpr const char *clamp_vector3fa_id = "ND_clamp_vector3FA";
constexpr const char *luminance_color3_id = "ND_luminance_color3";
constexpr const char *convert_float_color3_id = "ND_convert_float_color3";
constexpr const char *convert_color3_vector3_id = "ND_convert_color3_vector3";
constexpr const char *convert_vector3_color3_id = "ND_convert_vector3_color3";
constexpr const char *convert_float_vector3_id = "ND_convert_float_vector3";
constexpr const char *convert_float_vector2_id = "ND_convert_float_vector2";
constexpr const char *convert_color3_vector2_id = "ND_convert_color3_vector2";
constexpr const char *convert_vector2_color3_id = "ND_convert_vector2_color3";
constexpr const char *convert_boolean_color3_id = "ND_convert_boolean_color3";
constexpr const char *convert_integer_color3_id = "ND_convert_integer_color3";
constexpr const char *convert_boolean_float_id = "ND_convert_boolean_float";
constexpr const char *convert_integer_float_id = "ND_convert_integer_float";
constexpr const char *convert_boolean_vector2_id = "ND_convert_boolean_vector2";
constexpr const char *convert_integer_vector2_id = "ND_convert_integer_vector2";
constexpr const char *convert_boolean_vector3_id = "ND_convert_boolean_vector3";
constexpr const char *convert_integer_vector3_id = "ND_convert_integer_vector3";
constexpr const char *convert_boolean_vector4_id = "ND_convert_boolean_vector4";
constexpr const char *convert_integer_vector4_id = "ND_convert_integer_vector4";
constexpr const char *convert_vector4_color3_id = "ND_convert_vector4_color3";
constexpr const char *convert_color3_vector4_id = "ND_convert_color3_vector4";
constexpr const char *convert_vector2_vector4_id = "ND_convert_vector2_vector4";
constexpr const char *convert_vector4_vector2_id = "ND_convert_vector4_vector2";
constexpr const char *convert_color4_vector2_id = "ND_convert_color4_vector2";
constexpr const char *convert_color4_vector3_id = "ND_convert_color4_vector3";
constexpr const char *convert_color4_vector4_id = "ND_convert_color4_vector4";
constexpr const char *convert_vector4_color4_id = "ND_convert_vector4_color4";
/* A generic, untyped `<convert>` node -- i.e. one whose UsdShade `info:id`
 * literally reads "ND_convert" rather than a specific typed nodedef id such
 * as "ND_convert_vector3_color3". Real MaterialX documents that author a
 * bare `<convert type="color3">` element with no explicit `nodedef=`
 * attribute (letting the type checker resolve the concrete nodedef from the
 * node's connected `in` source type and its own declared output type)
 * translate to exactly this id when structurally carried over to USD (see
 * mtlx_to_usda.py: `nodedef = child.get("nodedef") or ("ND_" + child.tag)`).
 * This reader must perform the same real type resolution at read time: look
 * at the declared USD type of the `in` input and dispatch to whichever
 * already-verified typed color3 conversion applies. */
constexpr const char *convert_generic_id = "ND_convert";
constexpr const char *convert_vector2_vector3_id = "ND_convert_vector2_vector3";
constexpr const char *combine3_color3_id = "ND_combine3_color3";
constexpr const char *separate3_color3_id = "ND_separate3_color3";
constexpr const char *constant_float_id = "ND_constant_float";
constexpr const char *constant_color3_id = "ND_constant_color3";
constexpr const char *add_color3_id = "ND_add_color3";
constexpr const char *subtract_color3_id = "ND_subtract_color3";
constexpr const char *multiply_color3_id = "ND_multiply_color3";
constexpr const char *divide_color3_id = "ND_divide_color3";
constexpr const char *min_color3_id = "ND_min_color3";
constexpr const char *max_color3_id = "ND_max_color3";
constexpr const char *invert_color3_id = "ND_invert_color3";
constexpr const char *invert_color3fa_id = "ND_invert_color3FA";
constexpr const char *absval_color3_id = "ND_absval_color3";
constexpr const char *floor_color3_id = "ND_floor_color3";
constexpr const char *ceil_color3_id = "ND_ceil_color3";
constexpr const char *fract_color3_id = "ND_fract_color3";
constexpr const char *round_color3_id = "ND_round_color3";
constexpr const char *sign_color3_id = "ND_sign_color3";
constexpr const char *add_color3_fa_id = "ND_add_color3FA";
constexpr const char *subtract_color3_fa_id = "ND_subtract_color3FA";
constexpr const char *multiply_color3_fa_id = "ND_multiply_color3FA";
constexpr const char *divide_color3_fa_id = "ND_divide_color3FA";
constexpr const char *min_color3_fa_id = "ND_min_color3FA";
constexpr const char *max_color3_fa_id = "ND_max_color3FA";
constexpr const char *modulo_color3_id = "ND_modulo_color3";
constexpr const char *power_color3_id = "ND_power_color3";
constexpr const char *modulo_color3fa_id = "ND_modulo_color3FA";
constexpr const char *power_color3fa_id = "ND_power_color3FA";
constexpr const char *clamp_color3_id = "ND_clamp_color3";
constexpr const char *clamp_color3fa_id = "ND_clamp_color3FA";
constexpr const char *safepower_color3_id = "ND_safepower_color3";
constexpr const char *safepower_color3fa_id = "ND_safepower_color3FA";
constexpr const char *extract_color3_id = "ND_extract_color3";
constexpr const char *geompropvalue_float_id = "ND_geompropvalue_float";
constexpr const char *geompropvalue_color3_id = "ND_geompropvalue_color3";
constexpr const char *geompropvalue_vector2_id = "ND_geompropvalue_vector2";
constexpr const char *geompropvalue_vector3_id = "ND_geompropvalue_vector3";
/** MaterialX stdlib_defs.mtlx declares ND_geompropvalue_boolean/integer/vector4
 *  with uniform string "geomprop", typed "default", and typed "out". Cycles'
 *  AttributeNode reads Blender attributes for float/bool/int/vector payloads;
 *  Blender's attribute sync converts bool and int custom data to float storage
 *  (see intern/cycles/blender/attribute_convert.h), matching MaterialX's own
 *  bool/int -> float -> color3 convert nodegraphs for display use. */
constexpr const char *geompropvalue_boolean_id = "ND_geompropvalue_boolean";
constexpr const char *geompropvalue_integer_id = "ND_geompropvalue_integer";
constexpr const char *geompropvalue_vector4_id = "ND_geompropvalue_vector4";
/**
 * geometric_primvar_source_admission: real stdlib_defs.mtlx nodedefs for the
 * geometric-source family (<tangent>/<bitangent>, <texcoord>, <bump>) plus
 * usd_preview_surface.mtlx's <UsdPrimvarReader> family, and nprlib_defs.mtlx's
 * <viewdirection>. `ND_normal_vector3`/`ND_position_vector3` are declared
 * further below (already-committed geometric-source-readers work) and are
 * NOT redeclared here. Of this batch, only `ND_UsdPrimvarReader_float`,
 * `ND_UsdPrimvarReader_vector2`, `ND_UsdPrimvarReader_vector3`,
 * `ND_texcoord_vector2`, `ND_texcoord_vector3`, and `ND_viewdirection_vector3`
 * get a real, verified Cycles lowering below (AttributeNode / UVMapNode /
 * GeometryNode "Incoming" respectively -- see read_float_output() /
 * read_vector2_output() / read_vector3_output() below and graph.cpp's
 * mirrored validate()/lower()/lowered_output() branches). `ND_tangent_vector3`
 * / `ND_bitangent_vector3` / `ND_bump_vector3` are deliberately left
 * unadmitted and fail closed with a named reason (see their dedicated checks
 * in read_vector3_output() below) -- Cycles' TangentNode has no bitangent
 * output and no established index-to-attribute-name convention exists for
 * MaterialX's UV-indexed tangent/bitangent, and Cycles' BumpNode needs
 * derivative-sampled height inputs (sample_center/sample_x/sample_y) this
 * reader's single-value Link model does not yet support; neither is a
 * verified honest mapping, so neither gets a proxy/substitute lowering.
 */
constexpr const char *usdprimvarreader_float_id = "ND_UsdPrimvarReader_float";
constexpr const char *usdprimvarreader_vector2_id = "ND_UsdPrimvarReader_vector2";
constexpr const char *usdprimvarreader_vector3_id = "ND_UsdPrimvarReader_vector3";
constexpr const char *tangent_vector3_id = "ND_tangent_vector3";
constexpr const char *bitangent_vector3_id = "ND_bitangent_vector3";
constexpr const char *texcoord_vector2_id = "ND_texcoord_vector2";
constexpr const char *texcoord_vector3_id = "ND_texcoord_vector3";
/** nprlib_defs.mtlx ND_viewdirection_vector3: `space` defaults to "world"
 *  (unlike normal/position/tangent/bitangent, whose stdlib default is
 *  "object"). Its real genosl implementation
 *  (nprlib/genosl/nprlib_genosl_impl.mtlx: `sourcecode="transform({{space}},
 *  I)"`) uses OSL's incident ray direction `I` directly, with no sign flip --
 *  which is exactly Cycles' GeometryNode "Incoming" output (node_geometry.osl:
 *  `Incoming = I;`). Only space="world" is admitted, the same documented
 *  boundary as normal_vector3_id/position_vector3_id above. */
constexpr const char *viewdirection_vector3_id = "ND_viewdirection_vector3";
constexpr const char *bump_vector3_id = "ND_bump_vector3";
/**
 * <geompropvalue> with an authored color4 'geomprop' (stdlib_defs.mtlx
 * ND_geompropvalue_color4: uniform string "geomprop" + color4 "default",
 * output color4 "out") -- same shape as ND_geompropvalue_color3 above, one
 * component wider.
 */
constexpr const char *geompropvalue_color4_id = "ND_geompropvalue_color4";
/**
 * <geomcolor> (stdlib_defs.mtlx ND_geomcolor_float/_color3/_color4): takes
 * only a uniform integer "index" (default 0), no geomprop string -- the
 * nodedef leaves which underlying geometric color primvar "index" selects
 * up to the target renderer. Lowered here by mapping the index to the same
 * geomprop-name convention Blender's own USD importer uses for vertex
 * colors ("displayColor" for the primary/index-0 color set; see
 * usdtokens::displayColor in source/blender/io/usd/intern/usd_reader_mesh.cc)
 * and reusing the existing ND_geompropvalue_{float,color3,color4} Cycles
 * lowering via that synthesized geomprop name -- see
 * geomcolor_attribute_name() below.
 */
constexpr const char *geomcolor_float_id = "ND_geomcolor_float";
constexpr const char *geomcolor_color3_id = "ND_geomcolor_color3";
constexpr const char *geomcolor_color4_id = "ND_geomcolor_color4";
/** Geometric-source observation (real gap closed): `ND_normal_vector3` and
 *  `ND_position_vector3` (stdlib_defs.mtlx, nodegroup="geometric"). Both
 *  declare a `space` parameter with enum {model, object, world}; only
 *  `space="world"` has a verified honest native Cycles equivalent in this
 *  pass (Cycles' GeometryNode has no space parameter at all -- its
 *  Position/Normal outputs are always world space, per
 *  kernel/osl/shaders/node_geometry.osl: `Position = P; Normal = N;` where
 *  P/N are Cycles' world-space shading position/normal). `space="object"`
 *  and `space="model"` are deliberately out of scope here (object-space
 *  would need a follow-up that chains a VectorTransformNode the same way
 *  `is_space_transform` already does; model-space -- USD's bind-pose local
 *  space -- has no Cycles equivalent at all) and fail closed with a named
 *  reason rather than silently substituting the world-space value. */
constexpr const char *normal_vector3_id = "ND_normal_vector3";
constexpr const char *position_vector3_id = "ND_position_vector3";
constexpr const char *image_float_id = "ND_image_float";
constexpr const char *image_color3_id = "ND_image_color3";
constexpr const char *image_color4_id = "ND_image_color4";
/* MaterialX stdlib_defs.mtlx convolution2d blur nodes declare inputs
 * (in, size, uniform filtertype) and output out. stdlib_ng.mtlx explicitly
 * says its blur nodegraphs are pass-throughs, not a real blur implementation;
 * graph.cpp therefore admits only the exact size=0 identity case. The reader
 * mirrors that boundary and rejects nonzero blur / heighttonormal instead of
 * manufacturing image-kernel or derivative sampling this compiler lacks. */
constexpr const char *blur_float_id = "ND_blur_float";
constexpr const char *blur_color3_id = "ND_blur_color3";
constexpr const char *blur_color4_id = "ND_blur_color4";
constexpr const char *blur_vector2_id = "ND_blur_vector2";
constexpr const char *blur_vector3_id = "ND_blur_vector3";
constexpr const char *blur_vector4_id = "ND_blur_vector4";
constexpr const char *heighttonormal_vector3_id = "ND_heighttonormal_vector3";
constexpr const char *constant_color4_id = "ND_constant_color4";
/** Task 4: four-component observation, Vector4 side. */
constexpr const char *constant_vector4_id = "ND_constant_vector4";
/* MaterialX stdlib_defs.mtlx / stdlib_ng.mtlx Vector4 channel adapters:
 * convert_vector3_vector4 copies XYZ and fixes W to 1.0; convert_vector4_vector3
 * drops W; extract_vector4 selects one indexed component. */
constexpr const char *convert_vector3_vector4_id = "ND_convert_vector3_vector4";
constexpr const char *convert_vector4_vector3_id = "ND_convert_vector4_vector3";
constexpr const char *extract_vector4_id = "ND_extract_vector4";
/* convert_boolean_vector4_id and convert_integer_vector4_id are declared
 * above alongside the other bool/int vector converts. */
constexpr const char *convert_float_vector4_id = "ND_convert_float_vector4";
/** Task 5: boolean/integer exact-domain observation. */
constexpr const char *constant_boolean_id = "ND_constant_boolean";
constexpr const char *constant_integer_id = "ND_constant_integer";
/** MaterialX 1.39 value-typed <dot> identity family. */
constexpr const char *dot_float_id = "ND_dot_float";
constexpr const char *dot_color3_id = "ND_dot_color3";
constexpr const char *dot_color4_id = "ND_dot_color4";
constexpr const char *dot_vector2_id = "ND_dot_vector2";
constexpr const char *dot_vector3_id = "ND_dot_vector3";
constexpr const char *dot_vector4_id = "ND_dot_vector4";
constexpr const char *dot_boolean_id = "ND_dot_boolean";
constexpr const char *dot_integer_id = "ND_dot_integer";
constexpr const char *dot_matrix33_id = "ND_dot_matrix33";
constexpr const char *dot_matrix44_id = "ND_dot_matrix44";
/** Task 6: matrix boundary. */
constexpr const char *constant_matrix33_id = "ND_constant_matrix33";
constexpr const char *constant_matrix44_id = "ND_constant_matrix44";
constexpr const char *absval_color4_id = "ND_absval_color4";
constexpr const char *ceil_color4_id = "ND_ceil_color4";
constexpr const char *floor_color4_id = "ND_floor_color4";
constexpr const char *fract_color4_id = "ND_fract_color4";
constexpr const char *round_color4_id = "ND_round_color4";
constexpr const char *sign_color4_id = "ND_sign_color4";
constexpr const char *invert_color4_id = "ND_invert_color4";
constexpr const char *safepower_color4_id = "ND_safepower_color4";
constexpr const char *invert_color4fa_id = "ND_invert_color4FA";
constexpr const char *safepower_color4fa_id = "ND_safepower_color4FA";
/* MaterialX stdlib_defs.mtlx declares ND_clamp_color4 with Color4 low/high
 * bounds, the per-channel sibling of the already-supported scalar-bound
 * ND_clamp_color4FA. */
constexpr const char *clamp_color4_id = "ND_clamp_color4";
constexpr const char *clamp_color4fa_id = "ND_clamp_color4FA";
constexpr const char *add_color4_id = "ND_add_color4";
constexpr const char *subtract_color4_id = "ND_subtract_color4";
constexpr const char *multiply_color4_id = "ND_multiply_color4";
constexpr const char *divide_color4_id = "ND_divide_color4";
constexpr const char *min_color4_id = "ND_min_color4";
constexpr const char *max_color4_id = "ND_max_color4";
constexpr const char *modulo_color4_id = "ND_modulo_color4";
constexpr const char *power_color4_id = "ND_power_color4";
constexpr const char *add_color4fa_id = "ND_add_color4FA";
constexpr const char *subtract_color4fa_id = "ND_subtract_color4FA";
constexpr const char *multiply_color4fa_id = "ND_multiply_color4FA";
constexpr const char *divide_color4fa_id = "ND_divide_color4FA";
constexpr const char *min_color4fa_id = "ND_min_color4FA";
constexpr const char *max_color4fa_id = "ND_max_color4FA";
constexpr const char *modulo_color4fa_id = "ND_modulo_color4FA";
constexpr const char *power_color4fa_id = "ND_power_color4FA";
constexpr const char *image_vector2_id = "ND_image_vector2";
constexpr const char *image_vector3_id = "ND_image_vector3";
/* MaterialX stdlib_defs.mtlx ND_image_vector4 samples image RGBA as XYZW;
 * graph.cpp lowers XYZ through ImageTexture Color and W through Alpha. */
constexpr const char *image_vector4_id = "ND_image_vector4";
constexpr const char *extract_color4_id = "ND_extract_color4";
constexpr const char *convert_color4_color3_id = "ND_convert_color4_color3";
/* MaterialX stdlib_defs.mtlx / stdlib_ng.mtlx declare these Color4 channel
 * adapters as exact scalar/color component assembly: float/bool/int broadcast
 * through convert-to-float, combine2_color4CF preserves Color3 RGB plus a
 * scalar alpha, and combine4_color4 maps four scalar inputs to RGBA. */
constexpr const char *convert_float_color4_id = "ND_convert_float_color4";
constexpr const char *convert_boolean_color4_id = "ND_convert_boolean_color4";
constexpr const char *convert_integer_color4_id = "ND_convert_integer_color4";
constexpr const char *combine2_color4cf_id = "ND_combine2_color4CF";
constexpr const char *combine4_color4_id = "ND_combine4_color4";
/* MaterialX stdlib_defs.mtlx declares ND_separate4_color4 as a Color4 input
 * split into outr/outg/outb/outa floats; graph.cpp lowers RGB with
 * SeparateColor and resolves A through the existing Color4 sidecar. */
constexpr const char *separate4_color4_id = "ND_separate4_color4";
/* MaterialX stdlib_defs.mtlx declares the non-color-role Vector4 channel
 * constructors/separator below; genosl/genglsl implementations assemble and
 * extract exactly {x,y,z,w}. Cycles carries them as XYZ plus a W sidecar. */
constexpr const char *combine2_vector4vf_id = "ND_combine2_vector4VF";
constexpr const char *combine2_vector4vv_id = "ND_combine2_vector4VV";
constexpr const char *combine4_vector4_id = "ND_combine4_vector4";
constexpr const char *separate4_vector4_id = "ND_separate4_vector4";
/* MaterialX stdlib_defs.mtlx Vector4 math siblings: component-wise
 * add/subtract/multiply/divide plus vector/scalar FA variants and clamp. */
constexpr const char *add_vector4_id = "ND_add_vector4";
constexpr const char *subtract_vector4_id = "ND_subtract_vector4";
constexpr const char *multiply_vector4_id = "ND_multiply_vector4";
constexpr const char *divide_vector4_id = "ND_divide_vector4";
constexpr const char *add_vector4fa_id = "ND_add_vector4FA";
constexpr const char *subtract_vector4fa_id = "ND_subtract_vector4FA";
constexpr const char *multiply_vector4fa_id = "ND_multiply_vector4FA";
constexpr const char *divide_vector4fa_id = "ND_divide_vector4FA";
constexpr const char *clamp_vector4_id = "ND_clamp_vector4";
constexpr const char *clamp_vector4fa_id = "ND_clamp_vector4FA";
/* MaterialX stdlib_defs.mtlx declares ND_convert_color3_color4 as a
 * color3-to-color4 adapter; stdlib_ng.mtlx's NG_convert_color3_color4
 * separates the source RGB and combines it with literal alpha 1.0. */
constexpr const char *convert_color3_color4_id = "ND_convert_color3_color4";
/* Real MaterialX nodedefs from the vendored libraries/bxdf/usd_preview_surface.mtlx:
 * ND_UsdUVTexture_23 (node="UsdUVTexture", version "2.3", isdefaultversion="true") has
 * inputs file/st/wrapS/wrapT/fallback/scale/bias and outputs r,g,b,a,rgb.
 * ND_UsdUVTexture (version "2.2", inherit="ND_UsdUVTexture_23") adds only a color4
 * "rgba" output on top of the same inputs. Both nodedefs' reference implementation
 * (IMP_UsdUVTexture_23 / IMP_UsdUVTexture_22) is identical: an <image> node (type
 * color4; file/fallback/st/wrapS/wrapT feed image/default/texcoord/uaddressmode/
 * vaddressmode) multiplied by 'scale', then biased by 'bias'. This reader composes
 * that graph from the already-verified ND_image_color4 / ND_multiply_color4 /
 * ND_add_color4 / ND_convert_color4_color3 lowerers (see
 * compose_usd_uv_texture_color4() below) instead of adding a new native node kind.
 * Previously Cycles had no native lowering for this node at all (zero occurrences of
 * "UsdUVTexture" anywhere in intern/cycles/materialx/) -- see
 * docs/findings/materialx/place2d-cycles-ovrtx-disagreement.md for the real
 * CYCLES-vs-OVRTX place2d/UsdUVTexture disagreement this closes the gap for. */
constexpr const char *usd_uv_texture_id = "ND_UsdUVTexture";
constexpr const char *usd_uv_texture_23_id = "ND_UsdUVTexture_23";
constexpr const char *normalmap_float_id = "ND_normalmap_float";
constexpr const char *constant_vector3_id = "ND_constant_vector3";
/** USD Preview Surface's bundled usd_preview_surface.mtlx declares
 *  ND_UsdPrimvarReader_boolean/integer/vector4 as node="UsdPrimvarReader" with
 *  uniform string "varname", typed "fallback", and typed "out"; its own
 *  implementation nodegraphs forward varname/fallback to the matching
 *  ND_geompropvalue_* node. */
constexpr const char *usd_primvar_reader_boolean_id = "ND_UsdPrimvarReader_boolean";
constexpr const char *usd_primvar_reader_integer_id = "ND_UsdPrimvarReader_integer";
constexpr const char *usd_primvar_reader_vector4_id = "ND_UsdPrimvarReader_vector4";
constexpr const char *constant_vector2_id = "ND_constant_vector2";
constexpr const char *combine2_vector2_id = "ND_combine2_vector2";
constexpr const char *convert_vector3_vector2_id = "ND_convert_vector3_vector2";
constexpr const char *place2d_vector2_id = "ND_place2d_vector2";
constexpr const char *rotate2d_vector2_id = "ND_rotate2d_vector2";
constexpr const char *extract_vector2_id = "ND_extract_vector2";
constexpr const char *ramplr_color3_id = "ND_ramplr_color3";
constexpr const char *ramptb_color3_id = "ND_ramptb_color3";
constexpr const char *ramplr_color4_id = "ND_ramplr_color4";
constexpr const char *ramptb_color4_id = "ND_ramptb_color4";
constexpr const char *ramplr_float_id = "ND_ramplr_float";
constexpr const char *ramptb_float_id = "ND_ramptb_float";
constexpr const char *splitlr_float_id = "ND_splitlr_float";
constexpr const char *splittb_float_id = "ND_splittb_float";
constexpr const char *splitlr_color3_id = "ND_splitlr_color3";
constexpr const char *splittb_color3_id = "ND_splittb_color3";
constexpr const char *splitlr_color4_id = "ND_splitlr_color4";
constexpr const char *splittb_color4_id = "ND_splittb_color4";
constexpr const char *combine3_vector3_id = "ND_combine3_vector3";
constexpr const char *rotate3d_vector3_id = "ND_rotate3d_vector3";
constexpr const char *extract_vector3_id = "ND_extract_vector3";
constexpr const char *separate3_vector3_id = "ND_separate3_vector3";
constexpr const char *normalize_vector3_id = "ND_normalize_vector3";
constexpr const char *absval_vector3_id = "ND_absval_vector3";
constexpr const char *floor_vector3_id = "ND_floor_vector3";
constexpr const char *ceil_vector3_id = "ND_ceil_vector3";
constexpr const char *fract_vector3_id = "ND_fract_vector3";
constexpr const char *sin_vector3_id = "ND_sin_vector3";
constexpr const char *cos_vector3_id = "ND_cos_vector3";
constexpr const char *tan_vector3_id = "ND_tan_vector3";
constexpr const char *min_vector3_id = "ND_min_vector3";
constexpr const char *max_vector3_id = "ND_max_vector3";
constexpr const char *sign_vector3_id = "ND_sign_vector3";
constexpr const char *acos_vector3_id = "ND_acos_vector3";
constexpr const char *asin_vector3_id = "ND_asin_vector3";
constexpr const char *exp_vector3_id = "ND_exp_vector3";
constexpr const char *ln_vector3_id = "ND_ln_vector3";
constexpr const char *sqrt_vector3_id = "ND_sqrt_vector3";
constexpr const char *multiply_vector3_fa_id = "ND_multiply_vector3FA";
constexpr const char *add_vector3_fa_id = "ND_add_vector3FA";
constexpr const char *subtract_vector3_fa_id = "ND_subtract_vector3FA";
constexpr const char *safepower_vector3_id = "ND_safepower_vector3";
constexpr const char *safepower_vector3_fa_id = "ND_safepower_vector3FA";
constexpr const char *invert_vector3_id = "ND_invert_vector3";
constexpr const char *invert_vector3_fa_id = "ND_invert_vector3FA";
constexpr const char *crossproduct_vector3_id = "ND_crossproduct_vector3";
constexpr const char *magnitude_vector3_id = "ND_magnitude_vector3";
constexpr const char *dotproduct_vector3_id = "ND_dotproduct_vector3";
constexpr const char *distance_vector3_id = "ND_distance_vector3";
constexpr const char *reflect_vector3_id = "ND_reflect_vector3";
constexpr const char *refract_vector3_id = "ND_refract_vector3";
constexpr const char *magnitude_vector2_id = "ND_magnitude_vector2";
constexpr const char *dotproduct_vector2_id = "ND_dotproduct_vector2";
constexpr const char *distance_vector2_id = "ND_distance_vector2";
constexpr const char *multiply_vector2_fa_id = "ND_multiply_vector2FA";
constexpr const char *add_vector2_fa_id = "ND_add_vector2FA";
constexpr const char *subtract_vector2_fa_id = "ND_subtract_vector2FA";
constexpr const char *safepower_vector2_id = "ND_safepower_vector2";
constexpr const char *safepower_vector2_fa_id = "ND_safepower_vector2FA";
constexpr const char *invert_vector2_id = "ND_invert_vector2";
constexpr const char *invert_vector2_fa_id = "ND_invert_vector2FA";
constexpr const char *smoothstep_vector2_id = "ND_smoothstep_vector2";
constexpr const char *smoothstep_vector2_fa_id = "ND_smoothstep_vector2FA";
constexpr const char *smoothstep_vector3_id = "ND_smoothstep_vector3";
constexpr const char *smoothstep_vector3_fa_id = "ND_smoothstep_vector3FA";
constexpr const char *transformpoint_vector3_id = "ND_transformpoint_vector3";
constexpr const char *transformvector_vector3_id = "ND_transformvector_vector3";
constexpr const char *transformnormal_vector3_id = "ND_transformnormal_vector3";
/* Real MaterialX 1.39 nodedefs (pbrlib/pbrlib_defs.mtlx, lines ~381-404) --
 * see graph.cpp's matching declaration comment for the full reference
 * implementation citation (pbrlib/genosl/mx_roughness_anisotropy.osl and
 * pbrlib/pbrlib_ng.mtlx's IMP_glossiness_anisotropy) and why the "if
 * (anisotropy > 0.0)" branch collapses into one unconditional expression.
 * ND_roughness_anisotropy(float roughness=0.0, float anisotropy=0.0) -> vector2
 * ND_glossiness_anisotropy(float glossiness=1.0, float anisotropy=0.0) -> vector2 */
constexpr const char *roughness_anisotropy_id = "ND_roughness_anisotropy";
constexpr const char *glossiness_anisotropy_id = "ND_glossiness_anisotropy";
/* Real MaterialX 1.39 nodedef (pbrlib/pbrlib_defs.mtlx, ~line 391) -- see
 * graph.cpp's matching declaration comment for the full reference
 * implementation citation (pbrlib/genosl/mx_roughness_dual.osl) and why the
 * "if (roughness.y < 0.0)" branch is a genuine runtime sentinel select
 * (unlike roughness_anisotropy's collapsible branch above), requiring the
 * same real compare+select primitive as ifgreater_float_id.
 * ND_roughness_dual(vector2 roughness=0,0) -> vector2 out */
constexpr const char *roughness_dual_id = "ND_roughness_dual";
/* libraries/bxdf/open_pbr_surface.mtlx declares ND_open_pbr_anisotropy as a
 * direct nodegraph expansion over float roughness/anisotropy (see graph.cpp's
 * matching declaration comment for the exact arithmetic chain). */
constexpr const char *open_pbr_anisotropy_id = "ND_open_pbr_anisotropy";
/* MaterialX pbrlib/pbrlib_defs.mtlx declares ND_blackbody as
 * blackbody(float temperature=5000.0) -> color3. Cycles exposes a native
 * BlackbodyNode with the same temperature input; graph.cpp lowers it directly. */
constexpr const char *blackbody_id = "ND_blackbody";
/* Real MaterialX 1.39 nodedefs (pbrlib/pbrlib_defs.mtlx): both are
 * constructor nodes (node="displacement") for the displacementshader type,
 * distinguished by the type of their 'displacement' input -- float for
 * ND_displacement_float, vector3 for ND_displacement_vector3. There is no
 * "ND_displacementshader" nodedef in real MaterialX; a material's
 * displacement output connects directly to one of these two. */
constexpr const char *displacement_float_id = "ND_displacement_float";
constexpr const char *displacement_vector3_id = "ND_displacement_vector3";
constexpr const char *mix_displacementshader_id = "ND_mix_displacementshader";
const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);

/* Task 3: metadata-driven terminal routing. */
constexpr const char *standard_surface_id = "ND_standard_surface_surfaceshader";
/* libraries/bxdf/standard_surface.mtlx declares two nodedefs for the same
 * "standard_surface" node: ND_standard_surface_surfaceshader_100 (version
 * "1.0.0") and ND_standard_surface_surfaceshader (version "1.0.1",
 * isdefaultversion="true", inherit="ND_standard_surface_surfaceshader_100").
 * The 1.0.1 nodedef only overrides the *default values* of two inputs
 * (base: 1.0 vs 0.8; base_color: 0.8,0.8,0.8 vs 1,1,1) -- every input name,
 * type, and the rest of the field set are identical between the two
 * versions, and both share the same NG_standard_surface_surfaceshader_100
 * implementation nodegraph. A document that authors every input explicitly
 * (as this delivery phase already requires for the fields graph.cpp reads)
 * is therefore semantically identical under either nodedef id -- so the
 * already-verified standard_surface lowerer below is reused verbatim by
 * treating the _100 id as the same surface model, with no new field logic. */
constexpr const char *standard_surface_100_id = "ND_standard_surface_surfaceshader_100";
constexpr const char *surface_unlit_id = "ND_surface_unlit";
/* Real ND_convert_*_surfaceshader semantic lowerers. Verified directly
 * against the real bundled libraries/stdlib/stdlib_ng.mtlx functional
 * nodegraph implementations (NG_convert_<type>_surfaceshader) -- these are
 * NOT genglsl/genosl-backed nodes; MaterialX defines their semantics purely
 * as a nodegraph. Every one of the eight graphs is, structurally, "build an
 * <surface_unlit> node fed only by `emission_color` (the converted `in`
 * value, or its RGB channels for color4/vector4) and, for color4/vector4
 * only, `opacity` (the source's alpha/w channel)" -- transmission and the
 * scalar `emission` weight are left at ND_surface_unlit's own defaults
 * (0 and 1 respectively) in every one of the eight reference graphs. This
 * reader therefore reuses graph.cpp's existing, already-verified
 * ND_surface_unlit lowerer verbatim by constructing the equivalent `unlit`
 * Node below -- there is no separate lowering path in graph.cpp for these
 * eight ids. Connected `in` inputs are read through the same typed value
 * graph resolvers used by ordinary material fields, then wired into the
 * constructed surface_unlit node's emission_color (and opacity for
 * color4/vector4). */
constexpr const char *convert_color3_surfaceshader_id = "ND_convert_color3_surfaceshader";
constexpr const char *convert_color4_surfaceshader_id = "ND_convert_color4_surfaceshader";
constexpr const char *convert_float_surfaceshader_id = "ND_convert_float_surfaceshader";
constexpr const char *convert_vector2_surfaceshader_id = "ND_convert_vector2_surfaceshader";
constexpr const char *convert_vector3_surfaceshader_id = "ND_convert_vector3_surfaceshader";
constexpr const char *convert_vector4_surfaceshader_id = "ND_convert_vector4_surfaceshader";
constexpr const char *convert_integer_surfaceshader_id = "ND_convert_integer_surfaceshader";
constexpr const char *convert_boolean_surfaceshader_id = "ND_convert_boolean_surfaceshader";
bool is_convert_to_surfaceshader_id(const string &nodedef)
{
  return nodedef == convert_color3_surfaceshader_id || nodedef == convert_color4_surfaceshader_id ||
         nodedef == convert_float_surfaceshader_id || nodedef == convert_vector2_surfaceshader_id ||
         nodedef == convert_vector3_surfaceshader_id || nodedef == convert_vector4_surfaceshader_id ||
         nodedef == convert_integer_surfaceshader_id || nodedef == convert_boolean_surfaceshader_id;
}
/* Real terminal admission, verified against the real bundled
 * libraries/bxdf/usd_preview_surface.mtlx and libraries/bxdf/gltf_pbr.mtlx
 * nodedefs -- see graph.cpp's usd_preview_surface_id / gltf_pbr_id
 * comments for exactly which of their real inputs get a real Cycles
 * Principled BSDF lowering versus being admitted only at their inert
 * default value in this delivery phase. */
constexpr const char *usd_preview_surface_id = "ND_UsdPreviewSurface_surfaceshader";
constexpr const char *gltf_pbr_id = "ND_gltf_pbr_surfaceshader";
/* Real terminal admission for libraries/bxdf/disney_principled.mtlx's
 * ND_disney_principled -- see graph.cpp's disney_principled_id comment for
 * the real Cycles Principled BSDF field-by-field lowering. */
constexpr const char *disney_principled_id = "ND_disney_principled";
/* Generic <surface> closure-composition terminal -- see graph.cpp's
 * generic_surface_id comment for its deliberately scoped upstream closure
 * set. */
constexpr const char *generic_surface_id = "ND_surface";
constexpr const char *mix_surfaceshader_id = "ND_mix_surfaceshader";
constexpr const char *lama_surface_id = "ND_lama_surface";
/* MaterialX 1.39 stdlib_defs.mtlx declares the dot shader family as
 * organization-only identity nodes: input "in" and output "out" have the
 * same shader type, and every genosl/genglsl/genmdl implementation is
 * sourcecode="{{in}}". The reader elides them at USDShade connection
 * boundaries rather than creating semantic Cycles nodes. */
constexpr const char *dot_surfaceshader_id = "ND_dot_surfaceshader";
constexpr const char *dot_displacementshader_id = "ND_dot_displacementshader";
constexpr const char *dot_lightshader_id = "ND_dot_lightshader";
constexpr const char *dot_volumeshader_id = "ND_dot_volumeshader";
constexpr const char *volume_combinator_id = "ND_volume";
constexpr const char *absorption_vdf_id = "ND_absorption_vdf";
constexpr const char *anisotropic_vdf_id = "ND_anisotropic_vdf";
constexpr const char *oren_nayar_diffuse_bsdf_id = "ND_oren_nayar_diffuse_bsdf";
constexpr const char *translucent_bsdf_id = "ND_translucent_bsdf";
constexpr const char *sheen_bsdf_id = "ND_sheen_bsdf";
constexpr const char *subsurface_bsdf_id = "ND_subsurface_bsdf";
constexpr const char *conductor_bsdf_id = "ND_conductor_bsdf";
constexpr const char *dielectric_bsdf_id = "ND_dielectric_bsdf";
constexpr const char *chiang_hair_bsdf_id = "ND_chiang_hair_bsdf";
constexpr const char *uniform_edf_id = "ND_uniform_edf";
/* LAMA (Layered Material) nodedefs from MaterialX's libraries/bxdf/lama directory.
 * This reader lowers the subset whose reference nodegraphs reduce to native
 * pbrlib leaves/combinators already represented in graph.cpp; true vertical
 * layering and generalized/iridescent Fresnel variants remain rejected below. */
constexpr const char *lama_diffuse_id = "ND_lama_diffuse";
constexpr const char *lama_translucent_id = "ND_lama_translucent";
constexpr const char *lama_emission_id = "ND_lama_emission";
constexpr const char *lama_sss_id = "ND_lama_sss";
constexpr const char *lama_conductor_id = "ND_lama_conductor";
constexpr const char *lama_iridescence_id = "ND_lama_iridescence";
constexpr const char *lama_add_bsdf_id = "ND_lama_add_bsdf";
constexpr const char *lama_add_edf_id = "ND_lama_add_edf";
constexpr const char *lama_mix_bsdf_id = "ND_lama_mix_bsdf";
constexpr const char *lama_mix_edf_id = "ND_lama_mix_edf";
/* Closure combinator lowering: VDF-typed combinators over the
 * absorption_vdf/anisotropic_vdf leaves above. See read_vdf_coefficients()
 * for the real native mapping and its honest limits. */
constexpr const char *multiply_vdff_id = "ND_multiply_vdfF";
constexpr const char *multiply_vdfc_id = "ND_multiply_vdfC";
constexpr const char *add_vdf_id = "ND_add_vdf";
/* ND_mix_vdf (pbrlib/pbrlib_defs.mtlx): a real, native lerp of two VDFs'
 * absorption/scattering coefficients -- see read_vdf_coefficients()'s
 * mix_vdf_id branch for why this hits the exact same anisotropy-
 * superposition ceiling as add_vdf_id above (a genuine Cycles
 * VolumeCoefficientsNode limit, not a missing mapping) and nothing more.
 * ND_layer_vdf remains unsupported: its output is BSDF-typed (layering a
 * BSDF "top" over a VDF "base" interior) and there is no BSDF-typed link in
 * this IR yet. */
constexpr const char *mix_vdf_id = "ND_mix_vdf";
/* ND_mix_volumeshader (pbrlib/pbrlib_defs.mtlx): the volumeshader-typed
 * sibling of mix_vdf_id above, mixing two volumeshader terminals (each
 * either a bare VDF or an ND_volume(vdf, edf) combinator) rather than two
 * VDFs directly -- see resolve_volume_terminal_source()'s mix_volumeshader_id
 * branch. */
constexpr const char *mix_volumeshader_id = "ND_mix_volumeshader";
/* MaterialX 1.39 stdlib_defs.mtlx ND_surfacematerial is the material-binding
 * wrapper for surfaceshader/backsurfaceshader/displacementshader inputs. This
 * compiler already stores surface and displacement terminals directly, so a
 * surfacematerial found at the material surface output is unwrapped to those
 * slots rather than lowered as a fabricated material-valued node. The optional
 * backsurfaceshader input remains an explicit boundary: Cycles ShaderGraph has
 * one Surface output here and no importer-level backface material slot. */
constexpr const char *surface_material_id = "ND_surfacematerial";
/* MaterialX 1.39 pbrlib/pbrlib_defs.mtlx declares ND_light as a lightshader
 * constructor over an EDF plus literal intensity/exposure. The reader only
 * records authenticated light terminals for a caller-side light-object binding
 * (Graph::has_light), so this is an exact discovery/unwrapping step rather
 * than a ShaderGraph material-output lowering. */
constexpr const char *light_shader_id = "ND_light";
/* MaterialX 1.39 libraries/lights/lights_defs.mtlx declares direct
 * lightshader terminal nodes for point, directional, and spot lights. This
 * reader still only authenticates the terminal for caller-side Light binding,
 * but authored inputs must match those real nodedef shapes instead of
 * accepting arbitrary parameters. */
constexpr const char *point_light_id = "ND_point_light";
constexpr const char *directional_light_id = "ND_directional_light";
constexpr const char *spot_light_id = "ND_spot_light";
/* MaterialX 1.39 stdlib_defs.mtlx ND_volumematerial is the material-binding
 * wrapper for one volumeshader input, analogous to ND_surfacematerial for
 * surface/backsurface/displacement. This compiler's Graph already stores the
 * volume terminal fields directly, so the reader unwraps volumematerial at a
 * volume terminal instead of adding a new material-valued IR node. */
constexpr const char *volume_material_id = "ND_volumematerial";
/* Closure combinators for the generic <surface> terminal's admitted
 * upstream closure set (bsdf/edf variants -- see generic_surface_id
 * above). */
constexpr const char *mix_bsdf_id = "ND_mix_bsdf";
constexpr const char *mix_edf_id = "ND_mix_edf";
constexpr const char *add_bsdf_id = "ND_add_bsdf";
constexpr const char *add_edf_id = "ND_add_edf";
/* BSDF-typed multiply combinators over the generic <surface> terminal's
 * admitted upstream closure set (mirrors multiply_vdff_id/multiply_vdfc_id
 * above, one level up the closure hierarchy) -- see graph.cpp's
 * multiply_bsdff_id/multiply_bsdfc_id lowering (is_bsdf_combinator() and its
 * validate()/lower() cases) for the real MixClosureNode mapping and its
 * literal-only, uniform-tint limits. */
constexpr const char *multiply_bsdff_id = "ND_multiply_bsdfF";
constexpr const char *multiply_bsdfc_id = "ND_multiply_bsdfC";
/* EDF-typed sibling of multiply_bsdff_id/multiply_bsdfc_id above (pbrlib/
 * pbrlib_defs.mtlx ND_multiply_edfF/ND_multiply_edfC) -- read_connected_
 * surface_closure()'s multiply_bsdff_id/multiply_bsdfc_id branch already
 * threads its `expected_kind` parameter generically, so this reuses that
 * same branch for the EDF flavor (see graph.cpp's multiply_edff_id/
 * multiply_edfc_id validate()/lower() for the SurfaceShader-typed,
 * no-other-flavor IR side). */
constexpr const char *multiply_edff_id = "ND_multiply_edfF";
constexpr const char *multiply_edfc_id = "ND_multiply_edfC";
/* ND_generalized_schlick_edf (pbrlib/pbrlib_defs.mtlx) modifies a base EDF by
 * a directional generalized Schlick factor. This reader admits only the exact
 * constant scalar subset (color0 == color90 and uniform RGB), which graph.cpp
 * lowers using the same real MixClosureNode scalar weighting as multiply_edf*;
 * directional/color-varying cases are rejected by name. */
constexpr const char *generalized_schlick_edf_id = "ND_generalized_schlick_edf";
/* Custom USD attribute (single-hop) recording that a NodeDef inherits from
 * another. There is no live MaterialX/Sdr registry wired into this reader
 * yet -- only explicitly authored version/inherit metadata is honored. This
 * is a documented boundary, not a full NodeDefProvider registry lookup. */
constexpr const char *nodedef_inherit_attr = "info:mtlx:inherit";

bool read_vector2_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         int depth,
                         string *error_message);

bool is_contrast_float(const string &nodedef)
{
  return nodedef == contrast_float_id;
}

bool is_contrast_color3(const string &nodedef)
{
  return nodedef == contrast_color3_id || nodedef == contrast_color3fa_id;
}

bool is_contrast_vector2(const string &nodedef)
{
  return nodedef == contrast_vector2_id || nodedef == contrast_vector2fa_id;
}

bool is_contrast_vector3(const string &nodedef)
{
  return nodedef == contrast_vector3_id || nodedef == contrast_vector3fa_id;
}

bool contrast_uses_scalar_parameters(const string &nodedef)
{
  return nodedef == contrast_color3fa_id || nodedef == contrast_vector2fa_id ||
         nodedef == contrast_vector3fa_id;
}

bool is_space_transform(const string &nodedef)
{
  return nodedef == transformpoint_vector3_id || nodedef == transformvector_vector3_id ||
         nodedef == transformnormal_vector3_id;
}

bool is_supported_transform_space(const string &space)
{
  return space == "world" || space == "object" || space == "camera";
}

void set_error(string *error_message, const string &message)
{
  if (error_message) {
    *error_message = message;
  }
}

bool resolve_connected_shader(const pxr::UsdShadeConnectableAPI &source,
                              const pxr::TfToken &source_name,
                              const pxr::UsdShadeAttributeType source_type,
                              const char *expected_id,
                              const pxr::SdfValueTypeName &expected_type,
                              pxr::UsdShadeShader *shader,
                              std::unordered_set<string> *active_endpoints,
                              const int depth,
                              string *error_message,
                              const char *expected_output_name = "out")
{
  if (depth > 64) {
    set_error(error_message, "USDShade NodeGraph nesting exceeds maximum depth");
    return false;
  }

  const pxr::UsdPrim source_prim = source.GetPrim();
  if (!source_prim) {
    set_error(error_message, "USDShade connection source has no prim");
    return false;
  }

  const char *endpoint_kind = source_type == pxr::UsdShadeAttributeType::Input ? "input" :
                                                                                 "output";
  const string endpoint = source_prim.GetPath().GetString() + "." + endpoint_kind + ":" +
                          source_name.GetString();
  if (!active_endpoints->insert(endpoint).second) {
    set_error(error_message, string("USDShade NodeGraph connection is cyclic at ") + endpoint);
    return false;
  }
  const auto finish = [&](const bool success) {
    active_endpoints->erase(endpoint);
    return success;
  };

  if (source_type == pxr::UsdShadeAttributeType::Input) {
    const pxr::UsdShadeNodeGraph source_graph(source_prim);
    if (!source_graph) {
      set_error(error_message,
                string("USDShade interface input source is not a NodeGraph: ") + endpoint);
      return finish(false);
    }
    const pxr::UsdShadeInput input = source_graph.GetInput(source_name);
    if (!input) {
      set_error(error_message,
                string("USDShade NodeGraph interface input does not exist: ") + endpoint);
      return finish(false);
    }
    if (input.GetTypeName() != expected_type) {
      set_error(error_message,
                string("USDShade NodeGraph interface input type mismatch at ") + endpoint);
      return finish(false);
    }
    const auto sources = input.GetConnectedSources();
    if (sources.size() != 1) {
      set_error(error_message,
                string("USDShade NodeGraph interface input must connect exactly one source: ") +
                    endpoint);
      return finish(false);
    }
    return finish(resolve_connected_shader(sources[0].source,
                                           sources[0].sourceName,
                                           sources[0].sourceType,
                                           expected_id,
                                           expected_type,
                                           shader,
                                           active_endpoints,
                                           depth + 1,
                                           error_message,
                                           expected_output_name));
  }

  if (source_type != pxr::UsdShadeAttributeType::Output) {
    set_error(error_message,
              string("USDShade connection has an invalid endpoint type at ") + endpoint);
    return finish(false);
  }

  const pxr::UsdShadeNodeGraph source_graph(source_prim);
  if (source_graph) {
    const pxr::UsdShadeOutput output = source_graph.GetOutput(source_name);
    if (!output) {
      set_error(error_message, string("USDShade NodeGraph output does not exist: ") + endpoint);
      return finish(false);
    }
    if (output.GetTypeName() != expected_type) {
      set_error(error_message, string("USDShade NodeGraph output type mismatch at ") + endpoint);
      return finish(false);
    }
    const auto sources = output.GetConnectedSources();
    if (sources.size() != 1) {
      set_error(error_message,
                string("USDShade NodeGraph output must connect exactly one source: ") + endpoint);
      return finish(false);
    }
    return finish(resolve_connected_shader(sources[0].source,
                                           sources[0].sourceName,
                                           sources[0].sourceType,
                                           expected_id,
                                           expected_type,
                                           shader,
                                           active_endpoints,
                                           depth + 1,
                                           error_message,
                                           expected_output_name));
  }

  const pxr::UsdShadeShader source_shader(source_prim);
  const pxr::UsdShadeOutput source_output = source_shader.GetOutput(source_name);
  if (!source_shader || !source_output) {
    set_error(error_message,
              string("USDShade connection requires a shader output at ") + endpoint);
    return finish(false);
  }
  if (source_output.GetTypeName() != expected_type) {
    set_error(error_message, string("USDShade shader output type mismatch at ") + endpoint);
    return finish(false);
  }

  pxr::TfToken source_id;
  if (!source_shader.GetShaderId(&source_id) ||
      (source_name.GetString() != string(expected_output_name) &&
       source_id.GetString() != separate3_vector3_id &&
       source_id.GetString() != separate3_color3_id &&
       source_id.GetString() != separate4_color4_id &&
       source_id.GetString() != separate4_vector4_id &&
       source_id.GetString() != usd_uv_texture_id &&
       source_id.GetString() != usd_uv_texture_23_id) ||
      (expected_id != nullptr && source_id.GetString() != expected_id))
  {
    set_error(error_message,
              expected_id ?
                  string("USDShade connection requires ") + expected_id + " at " + endpoint :
                  string("USDShade connection has an invalid shader source at ") + endpoint);
    return finish(false);
  }

  *shader = source_shader;
  return finish(true);
}

bool connected_shader(const pxr::UsdShadeInput &input,
                      const char *expected_id,
                      pxr::UsdShadeShader *shader,
                      string *error_message,
                      const char *expected_output_name = "out")
{
  if (!input || !input.HasConnectedSource()) {
    set_error(error_message, "MaterialX input has no connected source");
    return false;
  }

  const auto sources = input.GetConnectedSources();
  if (sources.size() != 1) {
    set_error(error_message, "MaterialX input must connect exactly one source");
    return false;
  }

  std::unordered_set<string> active_endpoints;
  if (!resolve_connected_shader(sources[0].source,
                                sources[0].sourceName,
                                sources[0].sourceType,
                                expected_id,
                                input.GetTypeName(),
                                shader,
                                &active_endpoints,
                                0,
                                error_message,
                                expected_output_name))
  {
    if (error_message) {
      *error_message = string("MaterialX input '") + input.GetBaseName().GetString() +
                       "': " + *error_message;
    }
    return false;
  }
  return true;
}

bool resolve_identity_dot_shader(const pxr::UsdShadeShader &dot,
                                 const char *dot_id,
                                 const pxr::SdfValueTypeName &expected_type,
                                 pxr::UsdShadeShader *shader,
                                 string *error_message,
                                 const int depth = 0)
{
  if (depth > 64) {
    set_error(error_message, string(dot_id) + " nesting exceeds maximum depth");
    return false;
  }
  const pxr::UsdShadeInput input = dot.GetInput(pxr::TfToken("in"));
  if (!input || !input.HasConnectedSource()) {
    set_error(error_message, string(dot_id) + " requires a connected 'in' input");
    return false;
  }
  if (input.GetTypeName() != expected_type) {
    set_error(error_message, string(dot_id) + " 'in' type does not match its shader output type");
    return false;
  }
  for (const pxr::UsdShadeInput &dot_input : dot.GetInputs()) {
    const string name = dot_input.GetBaseName().GetString();
    if (name != "in" && name != "note") {
      set_error(error_message, string(dot_id) + " has no direct Cycles equivalent: " + name);
      return false;
    }
    if (name == "note" && dot_input.HasConnectedSource()) {
      set_error(error_message, string(dot_id) + " note input must be a literal organization hint");
      return false;
    }
  }
  const auto sources = input.GetConnectedSources();
  if (sources.size() != 1) {
    set_error(error_message, string(dot_id) + " 'in' input must have exactly one source");
    return false;
  }
  std::unordered_set<string> active_endpoints;
  if (!resolve_connected_shader(sources[0].source,
                                sources[0].sourceName,
                                sources[0].sourceType,
                                nullptr,
                                input.GetTypeName(),
                                shader,
                                &active_endpoints,
                                0,
                                error_message))
  {
    return false;
  }
  pxr::TfToken resolved_id;
  if (shader->GetShaderId(&resolved_id) && resolved_id.GetString() == dot_id) {
    return resolve_identity_dot_shader(*shader, dot_id, expected_type, shader, error_message, depth + 1);
  }
  return true;
}

bool connected_shader_eliding_identity_dot(const pxr::UsdShadeInput &input,
                                           const char *dot_id,
                                           pxr::UsdShadeShader *shader,
                                           string *error_message)
{
  if (!input || !input.HasConnectedSource()) {
    set_error(error_message, "MaterialX input has no connected source");
    return false;
  }
  pxr::UsdShadeShader first;
  if (!connected_shader(input, nullptr, &first, error_message)) {
    return false;
  }
  pxr::TfToken first_id;
  if (first.GetShaderId(&first_id) && first_id.GetString() == dot_id) {
    return resolve_identity_dot_shader(first, dot_id, input.GetTypeName(), shader, error_message);
  }
  *shader = first;
  return true;
}

bool shader_has_exact_signature(const pxr::UsdShadeShader &shader,
                                const std::initializer_list<const char *> expected_inputs,
                                const std::initializer_list<const char *> expected_outputs,
                                string *error_message);

const char *value_dot_id_for_type(const pxr::SdfValueTypeName &type)
{
  if (type == pxr::SdfValueTypeNames->Float) {
    return dot_float_id;
  }
  if (type == pxr::SdfValueTypeNames->Color3f) {
    return dot_color3_id;
  }
  if (type == pxr::SdfValueTypeNames->Color4f) {
    return dot_color4_id;
  }
  if (type == pxr::SdfValueTypeNames->Float2) {
    return dot_vector2_id;
  }
  if (type == pxr::SdfValueTypeNames->Float3) {
    return dot_vector3_id;
  }
  if (type == pxr::SdfValueTypeNames->Float4) {
    return dot_vector4_id;
  }
  if (type == pxr::SdfValueTypeNames->Bool) {
    return dot_boolean_id;
  }
  if (type == pxr::SdfValueTypeNames->Int) {
    return dot_integer_id;
  }
  if (type == pxr::SdfValueTypeNames->Matrix3d) {
    return dot_matrix33_id;
  }
  if (type == pxr::SdfValueTypeNames->Matrix4d) {
    return dot_matrix44_id;
  }
  return nullptr;
}

bool blur_id_type(const string &nodedef, pxr::SdfValueTypeName *type = nullptr)
{
  pxr::SdfValueTypeName result;
  if (nodedef == blur_float_id) {
    result = pxr::SdfValueTypeNames->Float;
  }
  else if (nodedef == blur_color3_id) {
    result = pxr::SdfValueTypeNames->Color3f;
  }
  else if (nodedef == blur_color4_id) {
    result = pxr::SdfValueTypeNames->Color4f;
  }
  else if (nodedef == blur_vector2_id) {
    result = pxr::SdfValueTypeNames->Float2;
  }
  else if (nodedef == blur_vector3_id) {
    result = pxr::SdfValueTypeNames->Float3;
  }
  else if (nodedef == blur_vector4_id) {
    result = pxr::SdfValueTypeNames->Float4;
  }
  else {
    return false;
  }
  if (type) {
    *type = result;
  }
  return true;
}

bool validate_degenerate_blur_shader(const pxr::UsdShadeShader &shader,
                                     const string &nodedef,
                                     const pxr::SdfValueTypeName &value_type,
                                     string *error_message)
{
  if (!shader_has_exact_signature(shader, {"in", "size", "filtertype"}, {"out"}, error_message)) {
    return false;
  }
  const pxr::UsdShadeOutput output = shader.GetOutput(pxr::TfToken("out"));
  if (!output || output.GetTypeName() != value_type) {
    set_error(error_message, nodedef + " requires a correctly typed output 'out'");
    return false;
  }
  const pxr::UsdShadeInput value = shader.GetInput(pxr::TfToken("in"));
  if (!value || value.GetTypeName() != value_type || !value.HasConnectedSource()) {
    set_error(error_message, nodedef + " requires connected input 'in'");
    return false;
  }
  const pxr::UsdShadeInput size = shader.GetInput(pxr::TfToken("size"));
  float size_value = 0.0f;
  if (!size || size.GetTypeName() != pxr::SdfValueTypeNames->Float || size.HasConnectedSource() ||
      !size.Get(&size_value) || size_value != 0.0f)
  {
    set_error(error_message, nodedef + " requires literal size 0.0 for exact identity lowering");
    return false;
  }
  const pxr::UsdShadeInput filter = shader.GetInput(pxr::TfToken("filtertype"));
  string filter_value;
  if (!filter || filter.GetTypeName() != pxr::SdfValueTypeNames->String || filter.HasConnectedSource() ||
      !filter.Get(&filter_value) || (filter_value != "box" && filter_value != "gaussian"))
  {
    set_error(error_message, nodedef + " requires literal filtertype 'box' or 'gaussian'");
    return false;
  }
  return true;
}

bool connected_shader_eliding_value_dot(const pxr::UsdShadeInput &input,
                                        pxr::UsdShadeShader *shader,
                                        string *error_message)
{
  const char *dot_id = value_dot_id_for_type(input.GetTypeName());
  if (!dot_id) {
    return connected_shader(input, nullptr, shader, error_message);
  }
  return connected_shader_eliding_identity_dot(input, dot_id, shader, error_message);
}

bool resolve_terminal_source_eliding_identity_dot(const pxr::UsdShadeConnectableAPI &source,
                                                  const pxr::TfToken &source_name,
                                                  const pxr::UsdShadeAttributeType source_type,
                                                  const char *dot_id,
                                                  const pxr::SdfValueTypeName &expected_type,
                                                  pxr::UsdShadeShader *shader,
                                                  string *error_message)
{
  std::unordered_set<string> active_endpoints;
  if (!resolve_connected_shader(source,
                                source_name,
                                source_type,
                                nullptr,
                                expected_type,
                                shader,
                                &active_endpoints,
                                0,
                                error_message))
  {
    return false;
  }
  pxr::TfToken first_id;
  if (shader->GetShaderId(&first_id) && first_id.GetString() == dot_id) {
    return resolve_identity_dot_shader(*shader, dot_id, expected_type, shader, error_message);
  }
  return true;
}

/**
 * Task 3 NodeDefProvider (bounded): true if `shader` explicitly declares,
 * via the single-hop custom attribute `info:mtlx:inherit`, that it inherits
 * from `base_id`. This lets a versioned/customized NodeDef (a different
 * `info:id`) still be admitted for a terminal category that requires
 * `base_id`, without a live MaterialX/Sdr registry lookup. Only one level
 * of inheritance is followed; this is a documented boundary, not a full
 * registry-driven inheritance chain.
 */
bool nodedef_inherits_from(const pxr::UsdShadeShader &shader, const char *base_id)
{
  const pxr::UsdAttribute inherit_attr = shader.GetPrim().GetAttribute(
      pxr::TfToken(nodedef_inherit_attr));
  if (!inherit_attr) {
    return false;
  }
  string inherits;
  if (!inherit_attr.Get(&inherits)) {
    return false;
  }
  return inherits == string(base_id);
}

/**
 * Generic, allowlist-free downward traversal used only by the Phase 1
 * manifest-bound resolver (Task 2) to authenticate that a node is actually
 * part of the graph feeding an authored material terminal.
 *
 * Unlike `resolve_connected_shader`, this does not require a specific
 * NodeDef, output name, or declared type at any hop: it records every
 * UsdShadeShader prim path reachable by following every connected source of
 * every input, starting from one terminal connection. It fails closed only
 * on cyclic connections or nesting beyond the shared depth limit; anything
 * else it cannot resolve is simply not reachable rather than an error, since
 * the manifest authenticates the specific selected node/output separately.
 */
bool collect_reachable_shader_paths(const pxr::UsdShadeConnectableAPI &source,
                                    const pxr::TfToken &source_name,
                                    const pxr::UsdShadeAttributeType source_type,
                                    std::unordered_set<string> *visited_endpoints,
                                    std::unordered_set<string> *reachable_shader_paths,
                                    const int depth,
                                    string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "USDShade NodeGraph nesting exceeds maximum depth");
    return false;
  }
  const pxr::UsdPrim source_prim = source.GetPrim();
  if (!source_prim) {
    return true;
  }
  const char *endpoint_kind = source_type == pxr::UsdShadeAttributeType::Input ? "input" :
                                                                                 "output";
  const string endpoint = source_prim.GetPath().GetString() + "." + endpoint_kind + ":" +
                          source_name.GetString();
  if (!visited_endpoints->insert(endpoint).second) {
    /* Already explored (shared DAG node or a cycle guarded elsewhere by the
     * typed readers themselves); do not re-descend or fail here. */
    return true;
  }

  if (source_type == pxr::UsdShadeAttributeType::Input) {
    const pxr::UsdShadeNodeGraph source_graph(source_prim);
    if (!source_graph) {
      return true;
    }
    const pxr::UsdShadeInput input = source_graph.GetInput(source_name);
    if (!input) {
      return true;
    }
    for (const auto &connected : input.GetConnectedSources()) {
      if (!collect_reachable_shader_paths(connected.source,
                                          connected.sourceName,
                                          connected.sourceType,
                                          visited_endpoints,
                                          reachable_shader_paths,
                                          depth + 1,
                                          error_message))
      {
        return false;
      }
    }
    return true;
  }

  const pxr::UsdShadeNodeGraph source_graph(source_prim);
  if (source_graph) {
    const pxr::UsdShadeOutput output = source_graph.GetOutput(source_name);
    if (!output) {
      return true;
    }
    for (const auto &connected : output.GetConnectedSources()) {
      if (!collect_reachable_shader_paths(connected.source,
                                          connected.sourceName,
                                          connected.sourceType,
                                          visited_endpoints,
                                          reachable_shader_paths,
                                          depth + 1,
                                          error_message))
      {
        return false;
      }
    }
    return true;
  }

  const pxr::UsdShadeShader source_shader(source_prim);
  if (!source_shader) {
    return true;
  }
  reachable_shader_paths->insert(source_prim.GetPath().GetString());
  for (const pxr::UsdShadeInput &input : source_shader.GetInputs()) {
    if (!input.HasConnectedSource()) {
      continue;
    }
    for (const auto &connected : input.GetConnectedSources()) {
      if (!collect_reachable_shader_paths(connected.source,
                                          connected.sourceName,
                                          connected.sourceType,
                                          visited_endpoints,
                                          reachable_shader_paths,
                                          depth + 1,
                                          error_message))
      {
        return false;
      }
    }
  }
  return true;
}

bool is_scalar_math(const string &nodedef)
{
  return nodedef == add_float_id || nodedef == subtract_float_id || nodedef == multiply_float_id ||
         nodedef == power_float_id || nodedef == modulo_float_id || nodedef == divide_float_id ||
         nodedef == absval_float_id || nodedef == floor_float_id || nodedef == ceil_float_id ||
         nodedef == round_float_id || nodedef == sqrt_float_id || nodedef == fract_float_id ||
         nodedef == sign_float_id ||
         nodedef == min_float_id || nodedef == max_float_id || nodedef == sin_float_id ||
         nodedef == cos_float_id || nodedef == tan_float_id || nodedef == exp_float_id ||
         nodedef == acos_float_id || nodedef == asin_float_id || nodedef == atan2_float_id ||
         nodedef == ln_float_id;
}

bool is_float_conditional(const string &nodedef)
{
  return nodedef == ifgreater_float_id || nodedef == ifgreatereq_float_id ||
         nodedef == ifequal_float_id;
}

struct CellNoiseSpec {
  const char *nodedef;
  const char *input_name;
  Type input_type;
};

const CellNoiseSpec *cellnoise_spec(const string &nodedef)
{
  static const CellNoiseSpec specs[] = {
      {cellnoise2d_float_id, "texcoord", Type::Vector2},
      {cellnoise3d_float_id, "position", Type::Vector3},
  };
  for (const CellNoiseSpec &spec : specs) {
    if (nodedef == spec.nodedef) {
      return &spec;
    }
  }
  return nullptr;
}

bool is_color_conditional(const string &nodedef)
{
  return nodedef == ifgreater_color3_id || nodedef == ifgreatereq_color3_id ||
         nodedef == ifequal_color3_id;
}

bool is_vector2_conditional(const string &nodedef)
{
  return nodedef == ifgreater_vector2_id || nodedef == ifgreatereq_vector2_id ||
         nodedef == ifequal_vector2_id;
}

bool is_vector_conditional(const string &nodedef)
{
  return nodedef == ifgreater_vector3_id || nodedef == ifgreatereq_vector3_id ||
         nodedef == ifequal_vector3_id;
}

bool is_color4_conditional(const string &nodedef)
{
  return nodedef == ifgreater_color4_id || nodedef == ifgreatereq_color4_id ||
         nodedef == ifequal_color4_id;
}

bool is_inside_outside_float(const string &nodedef)
{
  return nodedef == inside_float_id || nodedef == outside_float_id;
}

bool is_inside_outside_color3(const string &nodedef)
{
  return nodedef == inside_color3_id || nodedef == outside_color3_id;
}

bool is_inside_outside_color4(const string &nodedef)
{
  return nodedef == inside_color4_id || nodedef == outside_color4_id;
}

bool is_color_math(const string &nodedef)
{
  return nodedef == add_color3_id || nodedef == subtract_color3_id ||
         nodedef == multiply_color3_id || nodedef == divide_color3_id ||
         nodedef == min_color3_id || nodedef == max_color3_id;
}

bool is_color_blend(const string &nodedef)
{
  return nodedef == plus_color3_id || nodedef == minus_color3_id ||
         nodedef == difference_color3_id || nodedef == burn_color3_id ||
         nodedef == dodge_color3_id || nodedef == screen_color3_id ||
         nodedef == overlay_color3_id;
}

bool is_color_unary_math(const string &nodedef)
{
  return nodedef == absval_color3_id || nodedef == floor_color3_id || nodedef == ceil_color3_id ||
         nodedef == fract_color3_id || nodedef == round_color3_id || nodedef == sign_color3_id;
}

bool is_color_scalar_math(const string &nodedef)
{
  return nodedef == add_color3_fa_id || nodedef == subtract_color3_fa_id ||
         nodedef == multiply_color3_fa_id || nodedef == divide_color3_fa_id ||
         nodedef == min_color3_fa_id || nodedef == max_color3_fa_id;
}

bool is_color_binary_component_math(const string &nodedef)
{
  return nodedef == modulo_color3_id || nodedef == power_color3_id || nodedef == safepower_color3_id;
}

bool is_color_scalar_component_math(const string &nodedef)
{
  return nodedef == modulo_color3fa_id || nodedef == power_color3fa_id ||
         nodedef == safepower_color3fa_id;
}

bool is_color4_unary_math(const string &nodedef)
{
  return nodedef == absval_color4_id || nodedef == ceil_color4_id ||
         nodedef == floor_color4_id || nodedef == fract_color4_id ||
         nodedef == round_color4_id || nodedef == sign_color4_id;
}

bool is_color4_binary_math(const string &nodedef)
{
  return nodedef == add_color4_id || nodedef == subtract_color4_id ||
         nodedef == multiply_color4_id || nodedef == divide_color4_id ||
         nodedef == min_color4_id || nodedef == max_color4_id ||
         nodedef == modulo_color4_id || nodedef == power_color4_id;
}

bool is_color4_scalar_math(const string &nodedef)
{
  return nodedef == add_color4fa_id || nodedef == subtract_color4fa_id ||
         nodedef == multiply_color4fa_id || nodedef == divide_color4fa_id ||
         nodedef == min_color4fa_id || nodedef == max_color4fa_id ||
         nodedef == modulo_color4fa_id || nodedef == power_color4fa_id ||
         nodedef == safepower_color4fa_id || nodedef == invert_color4fa_id;
}

bool is_color4_operation(const string &nodedef)
{
  return is_color4_unary_math(nodedef) || nodedef == invert_color4_id ||
         nodedef == invert_color4fa_id || nodedef == safepower_color4_id ||
         nodedef == safepower_color4fa_id || nodedef == clamp_color4_id ||
         nodedef == clamp_color4fa_id ||
         is_color4_binary_math(nodedef) || is_color4_scalar_math(nodedef);
}

bool is_vector4_math(const string &nodedef)
{
  return nodedef == add_vector4_id || nodedef == subtract_vector4_id ||
         nodedef == multiply_vector4_id || nodedef == divide_vector4_id ||
         nodedef == add_vector4fa_id || nodedef == subtract_vector4fa_id ||
         nodedef == multiply_vector4fa_id || nodedef == divide_vector4fa_id;
}

bool vector4_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == add_vector4fa_id || nodedef == subtract_vector4fa_id ||
         nodedef == multiply_vector4fa_id || nodedef == divide_vector4fa_id;
}

bool is_vector4_math_or_clamp(const string &nodedef)
{
  return is_vector4_math(nodedef) || nodedef == clamp_vector4_id || nodedef == clamp_vector4fa_id;
}

bool is_native_noise_family(const string &nodedef)
{
  return nodedef == noise2d_float_id || nodedef == noise2d_color3_id ||
         nodedef == noise2d_color3fa_id || nodedef == noise2d_color4_id ||
         nodedef == noise2d_color4fa_id || nodedef == noise2d_vector2_id ||
         nodedef == noise2d_vector2fa_id || nodedef == noise2d_vector3_id ||
         nodedef == noise2d_vector3fa_id || nodedef == noise2d_vector4_id ||
         nodedef == noise2d_vector4fa_id || nodedef == noise3d_float_id ||
         nodedef == noise3d_color3_id || nodedef == noise3d_color3fa_id ||
         nodedef == noise3d_color4_id || nodedef == noise3d_color4fa_id ||
         nodedef == noise3d_vector2_id || nodedef == noise3d_vector2fa_id ||
         nodedef == noise3d_vector3_id || nodedef == noise3d_vector3fa_id ||
         nodedef == noise3d_vector4_id || nodedef == noise3d_vector4fa_id;
}

bool is_native_fractal2d_family(const string &nodedef)
{
  return nodedef == fractal2d_float_id || nodedef == fractal2d_color3_id ||
         nodedef == fractal2d_color3fa_id || nodedef == fractal2d_color4_id ||
         nodedef == fractal2d_color4fa_id || nodedef == fractal2d_vector2_id ||
         nodedef == fractal2d_vector2fa_id || nodedef == fractal2d_vector3_id ||
         nodedef == fractal2d_vector3fa_id || nodedef == fractal2d_vector4_id ||
         nodedef == fractal2d_vector4fa_id;
}

bool is_native_fractal3d_family(const string &nodedef)
{
  return nodedef == fractal3d_float_id || nodedef == fractal3d_color3_id ||
         nodedef == fractal3d_color3fa_id || nodedef == fractal3d_color4_id ||
         nodedef == fractal3d_color4fa_id || nodedef == fractal3d_vector2_id ||
         nodedef == fractal3d_vector2fa_id || nodedef == fractal3d_vector3_id ||
         nodedef == fractal3d_vector3fa_id || nodedef == fractal3d_vector4_id ||
         nodedef == fractal3d_vector4fa_id;
}

bool is_native_noise_or_fractal_family(const string &nodedef)
{
  return is_native_noise_family(nodedef) || is_native_fractal2d_family(nodedef) ||
         is_native_fractal3d_family(nodedef);
}

bool native_noise_or_fractal_is_3d(const string &nodedef)
{
  return nodedef.find("noise3d") != string::npos || nodedef.find("fractal3d") != string::npos;
}

bool native_noise_or_fractal_is_float(const string &nodedef)
{
  return nodedef == noise2d_float_id || nodedef == noise3d_float_id ||
         nodedef == fractal2d_float_id || nodedef == fractal3d_float_id;
}

bool native_noise_or_fractal_is_color3(const string &nodedef)
{
  return nodedef == noise2d_color3_id || nodedef == noise2d_color3fa_id ||
         nodedef == noise3d_color3_id || nodedef == noise3d_color3fa_id ||
         nodedef == fractal2d_color3_id || nodedef == fractal2d_color3fa_id ||
         nodedef == fractal3d_color3_id || nodedef == fractal3d_color3fa_id;
}

bool native_noise_or_fractal_is_color4(const string &nodedef)
{
  return nodedef == noise2d_color4_id || nodedef == noise2d_color4fa_id ||
         nodedef == noise3d_color4_id || nodedef == noise3d_color4fa_id ||
         nodedef == fractal2d_color4_id || nodedef == fractal2d_color4fa_id ||
         nodedef == fractal3d_color4_id || nodedef == fractal3d_color4fa_id;
}

bool native_noise_or_fractal_is_vector2(const string &nodedef)
{
  return nodedef == noise2d_vector2_id || nodedef == noise2d_vector2fa_id ||
         nodedef == noise3d_vector2_id || nodedef == noise3d_vector2fa_id ||
         nodedef == fractal2d_vector2_id || nodedef == fractal2d_vector2fa_id ||
         nodedef == fractal3d_vector2_id || nodedef == fractal3d_vector2fa_id;
}

bool native_noise_or_fractal_uses_scalar_amplitude(const string &nodedef)
{
  return nodedef == noise2d_float_id || nodedef == noise2d_color3fa_id ||
         nodedef == noise2d_color4fa_id || nodedef == noise2d_vector2fa_id ||
         nodedef == noise2d_vector3fa_id || nodedef == noise2d_vector4fa_id ||
         nodedef == noise3d_float_id || nodedef == noise3d_color3fa_id ||
         nodedef == noise3d_color4fa_id || nodedef == noise3d_vector2fa_id ||
         nodedef == noise3d_vector3fa_id || nodedef == noise3d_vector4fa_id ||
         nodedef == fractal2d_float_id || nodedef == fractal2d_color3fa_id ||
         nodedef == fractal2d_color4fa_id || nodedef == fractal2d_vector2fa_id ||
         nodedef == fractal2d_vector3fa_id || nodedef == fractal2d_vector4fa_id ||
         nodedef == fractal3d_float_id || nodedef == fractal3d_color3fa_id ||
         nodedef == fractal3d_color4fa_id || nodedef == fractal3d_vector2fa_id ||
         nodedef == fractal3d_vector3fa_id || nodedef == fractal3d_vector4fa_id;
}

const pxr::SdfValueTypeName &native_noise_or_fractal_usd_output_type(const string &nodedef)
{
  if (native_noise_or_fractal_is_float(nodedef)) {
    return pxr::SdfValueTypeNames->Float;
  }
  if (native_noise_or_fractal_is_color3(nodedef)) {
    return pxr::SdfValueTypeNames->Color3f;
  }
  if (native_noise_or_fractal_is_vector2(nodedef)) {
    return pxr::SdfValueTypeNames->Float2;
  }
  if (native_noise_or_fractal_is_color4(nodedef)) {
    return pxr::SdfValueTypeNames->Color4f;
  }
  if (nodedef.find("vector4") != string::npos) {
    return pxr::SdfValueTypeNames->Float4;
  }
  return pxr::SdfValueTypeNames->Float3;
}

bool read_native_noise_or_fractal_parameters(const pxr::UsdShadeShader &source,
                                             Node *node,
                                             string *error_message)
{
  const string nodedef = node->nodedef;
  const bool is_fractal = is_native_fractal2d_family(nodedef) ||
                          is_native_fractal3d_family(nodedef);
  const bool scalar_amplitude = native_noise_or_fractal_uses_scalar_amplitude(nodedef);
  const bool is_vector2 = native_noise_or_fractal_is_vector2(nodedef);
  const bool is_four_channel = native_noise_or_fractal_usd_output_type(nodedef) ==
                               pxr::SdfValueTypeNames->Color4f ||
                               native_noise_or_fractal_usd_output_type(nodedef) ==
                                   pxr::SdfValueTypeNames->Float4;
  const pxr::UsdShadeInput amplitude = source.GetInput(pxr::TfToken("amplitude"));
  if (scalar_amplitude) {
    float value;
    if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        amplitude.HasConnectedSource() || !amplitude.Get(&value) || !std::isfinite(value))
    {
      set_error(error_message, nodedef + " requires literal finite float input 'amplitude'");
      return false;
    }
    node->inputs["amplitude"] = value;
  }
  else if (is_vector2) {
    pxr::GfVec2f value;
    if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
        amplitude.HasConnectedSource() || !amplitude.Get(&value) || !std::isfinite(value[0]) ||
        !std::isfinite(value[1]))
    {
      set_error(error_message, nodedef + " requires literal finite vector2 input 'amplitude'");
      return false;
    }
    node->vector2_inputs["amplitude"] = make_float2(value[0], value[1]);
  }
  else if (is_four_channel) {
    pxr::GfVec4f value;
    if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float4 ||
        amplitude.HasConnectedSource() || !amplitude.Get(&value) || !std::isfinite(value[0]) ||
        !std::isfinite(value[1]) || !std::isfinite(value[2]) || !std::isfinite(value[3]))
    {
      set_error(error_message, nodedef + " requires literal finite vector4 input 'amplitude'");
      return false;
    }
    node->vector4_inputs["amplitude"] = make_float4(value[0], value[1], value[2], value[3]);
  }
  else {
    pxr::GfVec3f value;
    if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
        amplitude.HasConnectedSource() || !amplitude.Get(&value) || !std::isfinite(value[0]) ||
        !std::isfinite(value[1]) || !std::isfinite(value[2]))
    {
      set_error(error_message, nodedef + " requires literal finite vector3 input 'amplitude'");
      return false;
    }
    node->vector3_inputs["amplitude"] = make_float3(value[0], value[1], value[2]);
  }
  if (is_fractal) {
    const pxr::UsdShadeInput octaves = source.GetInput(pxr::TfToken("octaves"));
    if (!octaves || octaves.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        octaves.HasConnectedSource() || !octaves.Get(&node->int_inputs["octaves"]) ||
        node->int_inputs["octaves"] < 1)
    {
      set_error(error_message, nodedef + " requires literal integer input 'octaves' >= 1");
      return false;
    }
    for (const char *name : {"lacunarity", "diminish"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(name));
      float value;
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          input.HasConnectedSource() || !input.Get(&value) || !std::isfinite(value) ||
          (string(name) == "lacunarity" && value <= 0.0f))
      {
        set_error(error_message, nodedef + " requires literal finite float input '" + name + "'");
        return false;
      }
      node->inputs[name] = value;
    }
  }
  else {
    const pxr::UsdShadeInput pivot = source.GetInput(pxr::TfToken("pivot"));
    float value;
    if (!pivot || pivot.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        pivot.HasConnectedSource() || !pivot.Get(&value) || !std::isfinite(value))
    {
      set_error(error_message, nodedef + " requires literal finite float input 'pivot'");
      return false;
    }
    node->inputs["pivot"] = value;
  }
  return true;
}

bool is_color4_ramp(const string &nodedef)
{
  return nodedef == ramplr_color4_id || nodedef == ramptb_color4_id;
}

bool is_scalar_split(const string &nodedef)
{
  return nodedef == splitlr_float_id || nodedef == splittb_float_id;
}

bool is_color3_split(const string &nodedef)
{
  return nodedef == splitlr_color3_id || nodedef == splittb_color3_id;
}

bool is_color4_split(const string &nodedef)
{
  return nodedef == splitlr_color4_id || nodedef == splittb_color4_id;
}

bool split_is_top_to_bottom(const string &nodedef)
{
  return nodedef == splittb_float_id || nodedef == splittb_color3_id ||
         nodedef == splittb_color4_id;
}

bool color4_is_finite(const pxr::GfVec4f &value)
{
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]) &&
         std::isfinite(value[3]);
}

/**
 * ND_geomcolor_* has no 'geomprop' input to author -- only a uniform integer
 * "index" (stdlib_defs.mtlx, default 0). Cycles' AttributeNode lowering
 * (reused via ND_geompropvalue_{float,color3,color4}, see graph.cpp) needs a
 * named geomprop, so this maps "index" to the geomprop name Blender's USD
 * importer assigns vertex-color primvars: "displayColor" is the primary
 * color set (index 0); additional sets are disambiguated by appending the
 * index, matching how e.g. "displayColor1" would name a second set.
 */
string geomcolor_attribute_name(const int index)
{
  return index == 0 ? string("displayColor") : string("displayColor") + std::to_string(index);
}

/**
 * ND_texcoord_vector2/_vector3 (stdlib_defs.mtlx) have no 'geomprop' input --
 * only a uniform integer "index" (default 0), the same shape as
 * ND_geomcolor_*. Cycles' UVMapNode lowering (reused via
 * ND_geompropvalue_vector2 for the vector2 case, see graph.cpp) needs a
 * named UV primvar, so this maps "index" to the primvar name Blender's USD
 * importer treats as the primary/active UV set: "st" (usdtokens::st in
 * source/blender/io/usd/intern/usd_reader_mesh.cc) is the primary set
 * (index 0); additional sets follow the same numbered-suffix convention
 * already established by geomcolor_attribute_name() above.
 */
string texcoord_attribute_name(const int index)
{
  return index == 0 ? string("st") : string("st") + std::to_string(index);
}

string unique_node_name(const Graph &graph, const string &base_name, const string &shader_path)
{
  const auto is_used = [&](const string &candidate) {
    for (const Node &node : graph.nodes) {
      if (node.name == candidate) {
        return true;
      }
    }
    return false;
  };

  if (!is_used(base_name)) {
    return base_name;
  }
  if (!is_used(shader_path)) {
    return shader_path;
  }
  for (size_t suffix = 2;; suffix++) {
    const string candidate = shader_path + "#" + std::to_string(suffix);
    if (!is_used(candidate)) {
      return candidate;
    }
  }
}

bool read_float_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_shaders,
                       std::unordered_map<string, string> *emitted_color4_shaders,
                       int depth,
                       string *error_message);

bool read_float_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_shaders,
                       int depth,
                       string *error_message);

bool read_vector3_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         int depth,
                         string *error_message);

bool read_normalmap_output(const pxr::UsdShadeInput &input,
                           Graph *graph,
                           Link *result,
                           std::unordered_map<string, string> *emitted_shaders,
                           string *error_message);

bool read_color_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_color4_shaders,
                       int depth,
                       string *error_message,
                       std::unordered_map<string, string> *emitted_float_shaders = nullptr);

bool read_color_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       int depth,
                       string *error_message);

bool read_color4_output(const pxr::UsdShadeInput &input,
                        Graph *graph,
                        Link *result,
                        std::unordered_set<string> *active_shaders,
                        std::unordered_map<string, string> *emitted_shaders,
                        int depth,
                        string *error_message);

/** Task 4: four-component observation, Vector4 side. Mirrors
 *  `read_color4_output`'s signature exactly; scoped to literal/attribute
 *  Vector4 producers plus scalar bool/int conversion adapters whose XYZ
 *  broadcast and W=0 semantics come directly from stdlib_ng.mtlx. */
bool read_vector4_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         int depth,
                         string *error_message);

/** Task 5: boolean/integer exact-domain observation. Both are scoped to
 *  their literal `ND_constant_*` NodeDef only in this pass -- see the
 *  definitions for the documented boundary. */
bool read_boolean_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         int depth,
                         string *error_message);

bool read_integer_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         int depth,
                         string *error_message);

/** Task 6: matrix boundary. Both scoped to their literal `ND_constant_*`
 *  NodeDef only; Matrix44 additionally rejects any non-affine literal
 *  (last row not exactly {0, 0, 0, 1}) at this same read step. */
bool read_matrix33_output(const pxr::UsdShadeInput &input,
                          Graph *graph,
                          Link *result,
                          std::unordered_set<string> *active_shaders,
                          std::unordered_map<string, string> *emitted_shaders,
                          int depth,
                          string *error_message);

bool read_matrix44_output(const pxr::UsdShadeInput &input,
                          Graph *graph,
                          Link *result,
                          std::unordered_set<string> *active_shaders,
                          std::unordered_map<string, string> *emitted_shaders,
                          int depth,
                          string *error_message);

bool read_float_operand(const pxr::UsdShadeShader &shader,
                        const string &nodedef,
                        const char *input_name,
                        Graph *graph,
                        Node *node,
                        std::unordered_set<string> *active_shaders,
                        std::unordered_map<string, string> *emitted_shaders,
                        std::unordered_map<string, string> *emitted_color4_shaders,
                         int depth,
                         string *error_message);

bool shader_has_exact_signature(const pxr::UsdShadeShader &shader,
                                const std::initializer_list<const char *> expected_inputs,
                                const std::initializer_list<const char *> expected_outputs,
                                string *error_message)
{
  std::unordered_set<string> actual_inputs;
  std::unordered_set<string> actual_outputs;
  for (const pxr::UsdShadeInput &input : shader.GetInputs()) {
    actual_inputs.insert(input.GetBaseName().GetString());
  }
  for (const pxr::UsdShadeOutput &output : shader.GetOutputs()) {
    actual_outputs.insert(output.GetBaseName().GetString());
  }
  const std::unordered_set<string> required_inputs(expected_inputs.begin(), expected_inputs.end());
  const std::unordered_set<string> required_outputs(expected_outputs.begin(), expected_outputs.end());
  if (actual_inputs != required_inputs || actual_outputs != required_outputs) {
    set_error(error_message, "MaterialX node does not have the exact required signature");
    return false;
  }
  return true;
}

bool read_constant_color4_output(const pxr::UsdShadeShader &source,
                                 pxr::GfVec4f *value,
                                 string *error_message)
{
  const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken("value"));
  const size_t input_count = source.GetInputs().size();
  const size_t output_count = source.GetOutputs().size();
  *value = pxr::GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
  if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
                       value_input.HasConnectedSource() || !value_input.Get(value))) ||
      !color4_is_finite(*value) || input_count != size_t(value_input ? 1 : 0) ||
      output_count != 1 || !source.GetOutput(pxr::TfToken("out")) ||
      source.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Color4f)
  {
    set_error(error_message, "ND_constant_color4 requires a literal finite color4 'value' input");
    return false;
  }
  return true;
}

/** Task 4: the Vector4 analogue of `read_constant_color4_output`. Vector4
 *  is the non-color-role float4 USD type (`Float4`, not `Color4f`) --
 *  everything else about the literal/finiteness/exact-signature
 *  authentication is identical. */
bool read_constant_vector4_output(const pxr::UsdShadeShader &source,
                                  pxr::GfVec4f *value,
                                  string *error_message)
{
  const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken("value"));
  const size_t input_count = source.GetInputs().size();
  const size_t output_count = source.GetOutputs().size();
  *value = pxr::GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);
  if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Float4 ||
                       value_input.HasConnectedSource() || !value_input.Get(value))) ||
      !color4_is_finite(*value) || input_count != size_t(value_input ? 1 : 0) ||
      output_count != 1 || !source.GetOutput(pxr::TfToken("out")) ||
      source.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float4)
  {
    set_error(error_message, "ND_constant_vector4 requires a literal finite vector4 'value' input");
    return false;
  }
  return true;
}

/**
 * Task 4: four-component observation, Vector4 side.
 *
 * Deliberately narrow: only `ND_constant_vector4`, `ND_image_vector4`, the exact
 * Vector3/Float/Boolean/Integer-to-Vector4 broadcast adapters, Vector4
 * combine/separate channel nodes, and named Vector4 primvar/geomprop readers
 * are recognized as native Vector4 lowerers. Vector4 arithmetic/ramp operations
 * still fail closed because Cycles only has native XYZ sockets and no general
 * W-sidecar ABI. ND_image_vector4 is admitted because MaterialX stdlib_defs.mtlx
 * declares it as the non-color-role sibling of ND_image_color4: RGBA samples map
 * directly to XYZW, and Cycles exposes the same sampled alpha socket. The scalar
 * (float/bool/int)-to-Vector4 adapters are supported because MaterialX's
 * stdlib nodegraphs first convert the source to float, then broadcast that
 * value to vector4, giving a verified XYZ+W broadcast through the existing
 * constant Vector4 sidecar convention. Any other Vector4-typed node
 * (arithmetic/ramp/split operations, ...) fails closed with a
 * named boundary error -- this mirrors how Color4 support was itself built up
 * incrementally (constant first, then image, then each operation family),
 * and is an honest, not silent, gap: Color4 already has that fuller
 * operation library from prior work; Vector4 does not yet.
 */
bool read_vector4_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         const int depth,
                         string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Vector4 graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float4) {
    set_error(error_message, "MaterialX Vector4 input must use Float4");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Vector4};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Vector4 graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == blur_vector4_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source_shader, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_vector4_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &input_link,
                             active_shaders,
                             emitted_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    Node blur;
    blur.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    blur.nodedef = nodedef;
    blur.links["in"] = input_link;
    blur.inputs["size"] = 0.0f;
    blur.string_inputs["filtertype"] = "box";
    blur.outputs["out"] = Type::Vector4;
    *result = {blur.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, blur.name);
    graph->nodes.push_back(std::move(blur));
    return finish(true);
  }

  if (nodedef == constant_vector4_id) {
    pxr::GfVec4f value;
    if (!read_constant_vector4_output(source_shader, &value, error_message)) {
      return finish(false);
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_vector4_id;
    constant.vector4_inputs["value"] = make_float4(value[0], value[1], value[2], value[3]);
    constant.outputs["out"] = Type::Vector4;
    *result = {constant.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  if (is_native_noise_or_fractal_family(nodedef) &&
      native_noise_or_fractal_usd_output_type(nodedef) == pxr::SdfValueTypeNames->Float4)
  {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source_shader,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != pxr::SdfValueTypeNames->Float4 ||
        !read_native_noise_or_fractal_parameters(source_shader, &noise, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source_shader.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["position"] = position;
    }
    else {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["texcoord"] = texcoord;
    }
    noise.outputs["out"] = Type::Vector4;
    *result = {noise.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, noise.name);
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (is_vector4_math_or_clamp(nodedef)) {
    Node operation;
    operation.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    operation.nodedef = nodedef;
    const bool clamp = nodedef == clamp_vector4_id || nodedef == clamp_vector4fa_id;
    const bool scalar_second = vector4_math_uses_scalar_second(nodedef);
    const bool scalar_clamp = nodedef == clamp_vector4fa_id;
    const auto read_vector4_operand = [&](const char *input_name) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(input_name));
      if (!operand) {
        return true;
      }
      if (operand.GetTypeName() != pxr::SdfValueTypeNames->Float4) {
        set_error(error_message, nodedef + " requires vector4 input '" + input_name + "'");
        return false;
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_vector4_output(operand, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message)) {
          return false;
        }
        operation.links[input_name] = link;
      }
      else {
        pxr::GfVec4f value;
        if (!operand.Get(&value) || !color4_is_finite(value)) {
          set_error(error_message, nodedef + " requires literal finite vector4 input '" + input_name + "'");
          return false;
        }
        if (nodedef == divide_vector4_id && input_name == string("in2") &&
            (value[0] == 0.0f || value[1] == 0.0f || value[2] == 0.0f || value[3] == 0.0f))
        {
          set_error(error_message, nodedef + " requires nonzero vector4 input 'in2'");
          return false;
        }
        operation.vector4_inputs[input_name] = make_float4(value[0], value[1], value[2], value[3]);
      }
      return true;
    };
    const auto read_float_operand = [&](const char *input_name) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(input_name));
      if (!operand) {
        return true;
      }
      if (operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + input_name + "'");
        return false;
      }
      if (operand.HasConnectedSource()) {
        Link link;
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(operand,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return false;
        }
        operation.links[input_name] = link;
      }
      else {
        float value = 0.0f;
        if (!operand.Get(&value) || !std::isfinite(value) ||
            (nodedef == divide_vector4fa_id && input_name == string("in2") && value == 0.0f))
        {
          set_error(error_message, nodedef + " requires literal finite float input '" + input_name + "'");
          return false;
        }
        operation.inputs[input_name] = value;
      }
      return true;
    };
    if (clamp) {
      if (!read_vector4_operand("in") ||
          !(scalar_clamp ? (read_float_operand("low") && read_float_operand("high")) :
                            (read_vector4_operand("low") && read_vector4_operand("high"))))
      {
        return finish(false);
      }
    }
    else if (!read_vector4_operand("in1") ||
             !(scalar_second ? read_float_operand("in2") : read_vector4_operand("in2")))
    {
      return finish(false);
    }
    operation.outputs["out"] = Type::Vector4;
    *result = {operation.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, operation.name);
    graph->nodes.push_back(std::move(operation));
    return finish(true);
  }

  if (nodedef == image_vector4_id) {
    const pxr::UsdShadeInput file_input = source_shader.GetInput(pxr::TfToken("file"));
    pxr::SdfAssetPath asset_path;
    if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
        file_input.HasConnectedSource() || !file_input.Get(&asset_path))
    {
      set_error(error_message, "ND_image_vector4 requires a literal asset 'file' input");
      return finish(false);
    }
    string file_path = asset_path.GetResolvedPath();
    if (file_path.empty()) {
      file_path = asset_path.GetAssetPath();
    }
    if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
        path_file_size(file_path) == 0)
    {
      set_error(error_message, "ND_image_vector4 file asset is unavailable or invalid");
      return finish(false);
    }

    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }

    Node image;
    image.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    image.nodedef = image_vector4_id;
    image.asset_inputs["file"] = file_path;
    image.links["texcoord"] = texcoord;
    const pxr::UsdShadeInput default_input = source_shader.GetInput(pxr::TfToken("default"));
    if (default_input) {
      pxr::GfVec4f value;
      if (default_input.GetTypeName() != pxr::SdfValueTypeNames->Float4 ||
          default_input.HasConnectedSource() || !default_input.Get(&value) ||
          !color4_is_finite(value))
      {
        set_error(error_message, "ND_image_vector4 requires a literal finite vector4 'default' input");
        return finish(false);
      }
      image.vector4_inputs["default"] = make_float4(value[0], value[1], value[2], value[3]);
    }
    image.outputs["out"] = Type::Vector4;
    *result = {image.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, image.name);
    graph->nodes.push_back(std::move(image));
    return finish(true);
  }

  if (nodedef == combine2_vector4vf_id || nodedef == combine2_vector4vv_id ||
      nodedef == combine4_vector4_id)
  {
    Node combine;
    combine.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    combine.nodedef = nodedef;
    const auto read_float_component = [&](const char *input_name) {
      const pxr::UsdShadeInput component = source_shader.GetInput(pxr::TfToken(input_name));
      if (!component || component.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + input_name + "'");
        return false;
      }
      if (component.HasConnectedSource()) {
        Link value;
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(component,
                               graph,
                               &value,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return false;
        }
        combine.links[input_name] = value;
      }
      else {
        float value = 0.0f;
        if (!component.Get(&value) || !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires finite float input '" + input_name + "'");
          return false;
        }
        combine.inputs[input_name] = value;
      }
      return true;
    };
    if (nodedef == combine2_vector4vf_id) {
      const pxr::UsdShadeInput vector = source_shader.GetInput(pxr::TfToken("in1"));
      if (!vector || vector.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, nodedef + " requires vector3 input 'in1'");
        return finish(false);
      }
      if (vector.HasConnectedSource()) {
        Link value;
        std::unordered_set<string> active_vector3_shaders;
        if (!read_vector3_output(vector, graph, &value, &active_vector3_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        combine.links["in1"] = value;
      }
      else {
        pxr::GfVec3f value;
        if (!vector.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
            !std::isfinite(value[2]))
        {
          set_error(error_message, nodedef + " requires literal finite vector3 input 'in1'");
          return finish(false);
        }
        combine.vector3_inputs["in1"] = make_float3(value[0], value[1], value[2]);
      }
      if (!read_float_component("in2")) {
        return finish(false);
      }
    }
    else if (nodedef == combine2_vector4vv_id) {
      for (const char *input_name : {"in1", "in2"}) {
        const pxr::UsdShadeInput vector = source_shader.GetInput(pxr::TfToken(input_name));
        if (!vector || vector.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
          set_error(error_message, nodedef + " requires vector2 input '" + input_name + "'");
          return finish(false);
        }
        if (vector.HasConnectedSource()) {
          Link value;
          std::unordered_set<string> active_vector2_shaders;
          if (!read_vector2_output(vector, graph, &value, &active_vector2_shaders, depth + 1, error_message)) {
            return finish(false);
          }
          combine.links[input_name] = value;
        }
        else {
          pxr::GfVec2f value;
          if (!vector.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
            set_error(error_message, nodedef + " requires literal finite vector2 input '" + input_name + "'");
            return finish(false);
          }
          combine.vector2_inputs[input_name] = make_float2(value[0], value[1]);
        }
      }
    }
    else {
      for (const char *input_name : {"in1", "in2", "in3", "in4"}) {
        if (!read_float_component(input_name)) {
          return finish(false);
        }
      }
    }
    if (!source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float4)
    {
      set_error(error_message, nodedef + " requires Float4 output 'out'");
      return finish(false);
    }
    combine.outputs["out"] = Type::Vector4;
    *result = {combine.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, combine.name);
    graph->nodes.push_back(std::move(combine));
    return finish(true);
  }

  if (nodedef == convert_vector3_vector4_id || nodedef == convert_color3_vector4_id ||
      nodedef == convert_vector2_vector4_id || nodedef == convert_color4_vector4_id)
  {
    Link vector_source;
    if (nodedef == convert_color3_vector4_id) {
      std::unordered_set<string> active_color3_shaders;
      if (!read_color_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector_source,
                             &active_color3_shaders,
                             depth + 1,
                             error_message))
      {
        return finish(false);
      }
    }
    else if (nodedef == convert_color4_vector4_id) {
      std::unordered_set<string> active_color4_shaders;
      if (!read_color4_output(source_shader.GetInput(pxr::TfToken("in")),
                              graph,
                              &vector_source,
                              &active_color4_shaders,
                              emitted_shaders,
                              depth + 1,
                              error_message))
      {
        return finish(false);
      }
    }
    else if (nodedef == convert_vector2_vector4_id) {
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &vector_source,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    else {
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &vector_source,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    if (!source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float4)
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = nodedef;
    convert.links["in"] = vector_source;
    convert.outputs["out"] = Type::Vector4;
    *result = {convert.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, convert.name);
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == convert_float_vector4_id || nodedef == convert_boolean_vector4_id ||
      nodedef == convert_integer_vector4_id)
  {
    Link source;
    if (nodedef == convert_float_vector4_id) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      if (!read_float_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &source,
                             &active_float_shaders,
                             &emitted_float_shaders,
                             depth + 1,
                             error_message))
      {
        return finish(false);
      }
    }
    else if (nodedef == convert_boolean_vector4_id) {
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &source,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    else {
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &source,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = nodedef;
    convert.links["in"] = source;
    convert.outputs["out"] = Type::Vector4;
    *result = {convert.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, convert.name);
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == geompropvalue_vector4_id || nodedef == usd_primvar_reader_vector4_id) {
    const char *input_name = nodedef == geompropvalue_vector4_id ? "geomprop" : "varname";
    const pxr::UsdShadeInput geomprop = source_shader.GetInput(pxr::TfToken(input_name));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty() ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float4)
    {
      set_error(error_message, nodedef + " requires a literal non-empty string '" + input_name +
                                   "' input and Float4 'out' output");
      return finish(false);
    }
    Node attribute;
    attribute.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    attribute.nodedef = nodedef;
    attribute.string_inputs[input_name] = value;
    attribute.outputs["out"] = Type::Vector4;
    *result = {attribute.name, "out", Type::Vector4};
    emitted_shaders->emplace(shader_path, attribute.name);
    graph->nodes.push_back(std::move(attribute));
    return finish(true);
  }

  set_error(error_message,
           "MaterialX Vector4 node '" + nodedef +
               "' is not a supported native Vector4 lowerer (only constants, attributes, "
               "combine/separate channel nodes, and float/bool/int conversion adapters are implemented)");
  return finish(false);
}

/**
 * Task 5: boolean exact-domain observation.
 *
 * Scoped to `ND_constant_boolean` only -- any other boolean-typed node
 * fails closed with a named boundary error. The literal value must be
 * exactly USD `bool` (`false`/`true`), authenticated as such by
 * `GetTypeName() == Bool`; there is no float/int coercion path.
 */
bool read_boolean_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         const int depth,
                         string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Boolean graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Bool) {
    set_error(error_message, "MaterialX Boolean input must use Bool");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Boolean};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Boolean graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == geompropvalue_boolean_id || nodedef == usd_primvar_reader_boolean_id) {
    const char *input_name = nodedef == geompropvalue_boolean_id ? "geomprop" : "varname";
    const pxr::UsdShadeInput geomprop = source_shader.GetInput(pxr::TfToken(input_name));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty() ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Bool)
    {
      set_error(error_message, nodedef + " requires a literal non-empty string '" + input_name +
                                   "' input and Bool 'out' output");
      return finish(false);
    }
    Node attribute;
    attribute.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    attribute.nodedef = nodedef;
    attribute.string_inputs[input_name] = value;
    attribute.outputs["out"] = Type::Boolean;
    *result = {attribute.name, "out", Type::Boolean};
    emitted_shaders->emplace(shader_path, attribute.name);
    graph->nodes.push_back(std::move(attribute));
    return finish(true);
  }

  if (nodedef == constant_boolean_id) {
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("value"));
    const size_t input_count = source_shader.GetInputs().size();
    const size_t output_count = source_shader.GetOutputs().size();
    bool literal = false;
    if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
                         value_input.HasConnectedSource() || !value_input.Get(&literal))) ||
        input_count != size_t(value_input ? 1 : 0) || output_count != 1 ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Bool)
    {
      set_error(error_message, "ND_constant_boolean requires a literal 'value' input");
      return finish(false);
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_boolean_id;
    constant.int_inputs["value"] = literal ? 1 : 0;
    constant.outputs["out"] = Type::Boolean;
    *result = {constant.name, "out", Type::Boolean};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  set_error(error_message,
           "MaterialX Boolean node '" + nodedef +
               "' is not a supported native Boolean lowerer (only ND_constant_boolean is "
               "implemented)");
  return finish(false);
}

/**
 * Task 5: integer exact-domain observation.
 *
 * Scoped to `ND_constant_integer` only. MaterialX's `integer` domain is a
 * full signed 32-bit int -- no value-range restriction is applied beyond
 * authenticating the literal USD `Int` type; every int32 value is valid.
 */
bool read_integer_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         std::unordered_map<string, string> *emitted_shaders,
                         const int depth,
                         string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Integer graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Int) {
    set_error(error_message, "MaterialX Integer input must use Int");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Integer};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Integer graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == geompropvalue_integer_id || nodedef == usd_primvar_reader_integer_id) {
    const char *input_name = nodedef == geompropvalue_integer_id ? "geomprop" : "varname";
    const pxr::UsdShadeInput geomprop = source_shader.GetInput(pxr::TfToken(input_name));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty() ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Int)
    {
      set_error(error_message, nodedef + " requires a literal non-empty string '" + input_name +
                                   "' input and Int 'out' output");
      return finish(false);
    }
    Node attribute;
    attribute.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    attribute.nodedef = nodedef;
    attribute.string_inputs[input_name] = value;
    attribute.outputs["out"] = Type::Integer;
    *result = {attribute.name, "out", Type::Integer};
    emitted_shaders->emplace(shader_path, attribute.name);
    graph->nodes.push_back(std::move(attribute));
    return finish(true);
  }

  if (nodedef == constant_integer_id) {
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("value"));
    const size_t input_count = source_shader.GetInputs().size();
    const size_t output_count = source_shader.GetOutputs().size();
    int literal = 0;
    if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
                         value_input.HasConnectedSource() || !value_input.Get(&literal))) ||
        input_count != size_t(value_input ? 1 : 0) || output_count != 1 ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Int)
    {
      set_error(error_message, "ND_constant_integer requires a literal 'value' input");
      return finish(false);
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_integer_id;
    constant.int_inputs["value"] = literal;
    constant.outputs["out"] = Type::Integer;
    *result = {constant.name, "out", Type::Integer};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  set_error(error_message,
           "MaterialX Integer node '" + nodedef +
               "' is not a supported native Integer lowerer (only ND_constant_integer is "
               "implemented)");
  return finish(false);
}

/**
 * Task 6: matrix boundary, Matrix33 side.
 *
 * Scoped to `ND_constant_matrix33` only. USD's `Matrix3d` is double
 * precision; components are narrowed to float for IR storage, consistent
 * with the rest of this float-based IR/renderer (the same precision
 * policy already implicit for every other numeric type here) -- not a
 * lossy semantic re-encoding.
 */
bool read_matrix33_output(const pxr::UsdShadeInput &input,
                          Graph *graph,
                          Link *result,
                          std::unordered_set<string> *active_shaders,
                          std::unordered_map<string, string> *emitted_shaders,
                          const int depth,
                          string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Matrix33 graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Matrix3d) {
    set_error(error_message, "MaterialX Matrix33 input must use Matrix3d");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Matrix33};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Matrix33 graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == constant_matrix33_id) {
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("value"));
    const size_t input_count = source_shader.GetInputs().size();
    const size_t output_count = source_shader.GetOutputs().size();
    pxr::GfMatrix3d literal(1.0);
    if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Matrix3d ||
                         value_input.HasConnectedSource() || !value_input.Get(&literal))) ||
        input_count != size_t(value_input ? 1 : 0) || output_count != 1 ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Matrix3d)
    {
      set_error(error_message, "ND_constant_matrix33 requires a literal 'value' input");
      return finish(false);
    }
    std::array<float, 9> value{};
    for (int row = 0; row < 3; row++) {
      for (int col = 0; col < 3; col++) {
        const double component = literal[row][col];
        if (!std::isfinite(component)) {
          set_error(error_message, "ND_constant_matrix33 requires a finite literal 'value'");
          return finish(false);
        }
        value[size_t(row * 3 + col)] = float(component);
      }
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_matrix33_id;
    constant.matrix33_inputs["value"] = value;
    constant.outputs["out"] = Type::Matrix33;
    *result = {constant.name, "out", Type::Matrix33};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  set_error(error_message,
           "MaterialX Matrix33 node '" + nodedef +
               "' is not a supported native Matrix33 lowerer (only ND_constant_matrix33 is "
               "implemented)");
  return finish(false);
}

/**
 * Task 6: matrix boundary, Matrix44 side.
 *
 * Scoped to `ND_constant_matrix44` only, and further scoped to genuinely
 * affine matrices: Cycles' native `Transform` device representation
 * (see graph.cpp's `lower()`) is an affine 4x3 -- it has no native slot
 * for a non-{0,0,0,1} last row. Rather than silently drop that row (a
 * lossy truncation the design spec explicitly forbids -- "determinant or
 * color encodings are not equivalent") a non-affine literal is rejected
 * here as an explicit, honest boundary.
 */
bool read_matrix44_output(const pxr::UsdShadeInput &input,
                          Graph *graph,
                          Link *result,
                          std::unordered_set<string> *active_shaders,
                          std::unordered_map<string, string> *emitted_shaders,
                          const int depth,
                          string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Matrix44 graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Matrix4d) {
    set_error(error_message, "MaterialX Matrix44 input must use Matrix4d");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Matrix44};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Matrix44 graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == constant_matrix44_id) {
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("value"));
    const size_t input_count = source_shader.GetInputs().size();
    const size_t output_count = source_shader.GetOutputs().size();
    pxr::GfMatrix4d literal(1.0);
    if ((value_input && (value_input.GetTypeName() != pxr::SdfValueTypeNames->Matrix4d ||
                         value_input.HasConnectedSource() || !value_input.Get(&literal))) ||
        input_count != size_t(value_input ? 1 : 0) || output_count != 1 ||
        !source_shader.GetOutput(pxr::TfToken("out")) ||
        source_shader.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Matrix4d)
    {
      set_error(error_message, "ND_constant_matrix44 requires a literal 'value' input");
      return finish(false);
    }
    std::array<float, 16> value{};
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
        const double component = literal[row][col];
        if (!std::isfinite(component)) {
          set_error(error_message, "ND_constant_matrix44 requires a finite literal 'value'");
          return finish(false);
        }
        value[size_t(row * 4 + col)] = float(component);
      }
    }
    if (value[12] != 0.0f || value[13] != 0.0f || value[14] != 0.0f || value[15] != 1.0f) {
      set_error(error_message,
               "ND_constant_matrix44 is not affine (last row is not {0, 0, 0, 1}) -- no native "
               "Cycles device representation exists for a general projective Matrix44; this is "
               "an honest boundary, not a truncation");
      return finish(false);
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_matrix44_id;
    constant.matrix44_inputs["value"] = value;
    constant.outputs["out"] = Type::Matrix44;
    *result = {constant.name, "out", Type::Matrix44};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  set_error(error_message,
           "MaterialX Matrix44 node '" + nodedef +
               "' is not a supported native Matrix44 lowerer (only ND_constant_matrix44 is "
               "implemented)");
  return finish(false);
}

/* Builds the real ND_UsdUVTexture(_23) -> Cycles graph shared by the color4 ("rgba")
 * and color3 ("rgb") readers below: an ND_image_color4 node (file/st/fallback wired to
 * file/texcoord/default, exactly like the existing ND_image_color4 lowerer) optionally
 * followed by an ND_multiply_color4 (for a non-identity 'scale') and an ND_add_color4
 * (for a non-zero 'bias') -- see the usd_uv_texture_id / usd_uv_texture_23_id comment
 * above for the real nodedef/nodegraph this mirrors. Fails closed for anything this
 * delivery phase does not faithfully reproduce: any input this nodedef does not
 * declare, a non-literal (connected) fallback/scale/bias (all three read as literal
 * uniform-frequency values here, matching this reader's other image_* conventions), or
 * a non-default wrapS/wrapT addressing mode (the existing ND_image_color4 Cycles
 * lowering has no addressing-mode control to wire one to) -- rather than silently
 * substituting a wrong or constant value. In particular 'st' is always read through
 * read_vector2_output(), the same real per-fragment UV path ND_image_color4 already
 * uses, so a wired place2d (or any other) UV source is genuinely honored instead of
 * being dropped in favor of a constant UV -- the failure mode root-caused in
 * docs/findings/materialx/place2d-cycles-ovrtx-disagreement.md. */
bool compose_usd_uv_texture_color4(const pxr::UsdShadeShader &source_shader,
                                   const string &nodedef,
                                   const string &shader_path,
                                   Graph *graph,
                                   Link *result,
                                   const int depth,
                                   string *error_message)
{
  for (const pxr::UsdShadeInput &texture_input : source_shader.GetInputs()) {
    const string name = texture_input.GetBaseName().GetString();
    if (name != "file" && name != "st" && name != "wrapS" && name != "wrapT" &&
        name != "fallback" && name != "scale" && name != "bias")
    {
      set_error(error_message, nodedef + " has no supported Cycles control: " + name);
      return false;
    }
  }

  const pxr::UsdShadeInput file_input = source_shader.GetInput(pxr::TfToken("file"));
  pxr::SdfAssetPath asset_path;
  if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
      file_input.HasConnectedSource() || !file_input.Get(&asset_path))
  {
    set_error(error_message, nodedef + " requires a literal asset 'file' input");
    return false;
  }
  string file_path = asset_path.GetResolvedPath();
  if (file_path.empty()) {
    file_path = asset_path.GetAssetPath();
  }
  if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
      path_file_size(file_path) == 0)
  {
    set_error(error_message, nodedef + " file asset is unavailable or invalid");
    return false;
  }

  for (const char *wrap_name : {"wrapS", "wrapT"}) {
    const pxr::UsdShadeInput wrap_input = source_shader.GetInput(pxr::TfToken(wrap_name));
    if (!wrap_input) {
      continue;
    }
    string wrap_value;
    if (wrap_input.GetTypeName() != pxr::SdfValueTypeNames->String ||
        wrap_input.HasConnectedSource() || !wrap_input.Get(&wrap_value) ||
        wrap_value != "periodic")
    {
      set_error(error_message,
               nodedef + " only supports the default 'periodic' " + string(wrap_name) +
                   " addressing mode in this delivery phase");
      return false;
    }
  }

  float4 fallback = make_float4(0.0f, 0.0f, 0.0f, 1.0f);
  bool has_fallback = false;
  const pxr::UsdShadeInput fallback_input = source_shader.GetInput(pxr::TfToken("fallback"));
  if (fallback_input) {
    pxr::GfVec4f value;
    if (fallback_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
        fallback_input.HasConnectedSource() || !fallback_input.Get(&value) ||
        !color4_is_finite(value))
    {
      set_error(error_message, nodedef + " requires a literal finite color4 'fallback' input");
      return false;
    }
    fallback = make_float4(value[0], value[1], value[2], value[3]);
    has_fallback = true;
  }

  float4 scale = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
  const pxr::UsdShadeInput scale_input = source_shader.GetInput(pxr::TfToken("scale"));
  if (scale_input) {
    pxr::GfVec4f value;
    if (scale_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
        scale_input.HasConnectedSource() || !scale_input.Get(&value) || !color4_is_finite(value))
    {
      set_error(error_message, nodedef + " requires a literal finite color4 'scale' input");
      return false;
    }
    scale = make_float4(value[0], value[1], value[2], value[3]);
  }

  float4 bias = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  const pxr::UsdShadeInput bias_input = source_shader.GetInput(pxr::TfToken("bias"));
  if (bias_input) {
    pxr::GfVec4f value;
    if (bias_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
        bias_input.HasConnectedSource() || !bias_input.Get(&value) || !color4_is_finite(value))
    {
      set_error(error_message, nodedef + " requires a literal finite color4 'bias' input");
      return false;
    }
    bias = make_float4(value[0], value[1], value[2], value[3]);
  }

  Link texcoord;
  std::unordered_set<string> active_vector2_shaders;
  if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("st")),
                           graph,
                           &texcoord,
                           &active_vector2_shaders,
                           depth + 1,
                           error_message))
  {
    return false;
  }

  Node image;
  image.name = unique_node_name(
      *graph, source_shader.GetPrim().GetName().GetString() + ".image", shader_path + ".image");
  image.nodedef = image_color4_id;
  image.asset_inputs["file"] = file_path;
  image.links["texcoord"] = texcoord;
  if (has_fallback) {
    image.float4_inputs["default"] = fallback;
  }
  image.outputs["out"] = Type::Color4;
  Link chain = {image.name, "out", Type::Color4};
  graph->nodes.push_back(std::move(image));

  if (scale.x != 1.0f || scale.y != 1.0f || scale.z != 1.0f || scale.w != 1.0f) {
    Node multiply;
    multiply.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString() + ".scale", shader_path + ".scale");
    multiply.nodedef = multiply_color4_id;
    multiply.links["in1"] = chain;
    multiply.float4_inputs["in2"] = scale;
    multiply.outputs["out"] = Type::Color4;
    chain = {multiply.name, "out", Type::Color4};
    graph->nodes.push_back(std::move(multiply));
  }

  if (bias.x != 0.0f || bias.y != 0.0f || bias.z != 0.0f || bias.w != 0.0f) {
    Node add;
    add.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString() + ".bias", shader_path + ".bias");
    add.nodedef = add_color4_id;
    add.links["in1"] = chain;
    add.float4_inputs["in2"] = bias;
    add.outputs["out"] = Type::Color4;
    chain = {add.name, "out", Type::Color4};
    graph->nodes.push_back(std::move(add));
  }

  *result = chain;
  return true;
}

bool read_color4_output(const pxr::UsdShadeInput &input,
                        Graph *graph,
                        Link *result,
                        std::unordered_set<string> *active_shaders,
                        std::unordered_map<string, string> *emitted_shaders,
                        const int depth,
                        string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX Color4 graph nesting exceeds maximum depth");
    return false;
  }
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
    set_error(error_message, "MaterialX Color4 input must use Color4f");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(shader_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Color4};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX Color4 graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == blur_color4_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source_shader, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_color4_output(source_shader.GetInput(pxr::TfToken("in")),
                            graph,
                            &input_link,
                            active_shaders,
                            emitted_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    Node blur;
    blur.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    blur.nodedef = nodedef;
    blur.links["in"] = input_link;
    blur.inputs["size"] = 0.0f;
    blur.string_inputs["filtertype"] = "box";
    blur.outputs["out"] = Type::Color4;
    *result = {blur.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, blur.name);
    graph->nodes.push_back(std::move(blur));
    return finish(true);
  }

  if (nodedef == constant_color4_id) {
    pxr::GfVec4f value;
    if (!read_constant_color4_output(source_shader, &value, error_message)) {
      return finish(false);
    }
    Node constant;
    constant.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    constant.nodedef = constant_color4_id;
    constant.float4_inputs["value"] = make_float4(value[0], value[1], value[2], value[3]);
    constant.outputs["out"] = Type::Color4;
    *result = {constant.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, constant.name);
    graph->nodes.push_back(std::move(constant));
    return finish(true);
  }

  if (is_inside_outside_color4(nodedef)) {
    Node mask_node;
    mask_node.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    mask_node.nodedef = nodedef;
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("in"));
    if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
      set_error(error_message, nodedef + " requires color4 input 'in'");
      return finish(false);
    }
    if (value_input.HasConnectedSource()) {
      Link link;
      if (!read_color4_output(value_input, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      mask_node.links["in"] = link;
    }
    else {
      pxr::GfVec4f value;
      if (!value_input.Get(&value) || !color4_is_finite(value)) {
        set_error(error_message, nodedef + " requires literal finite or connected color4 input 'in'");
        return finish(false);
      }
      mask_node.float4_inputs["in"] = make_float4(value[0], value[1], value[2], value[3]);
    }
    if (!read_float_operand(source_shader,
                            nodedef,
                            "mask",
                            graph,
                            &mask_node,
                            active_shaders,
                            emitted_shaders,
                            emitted_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    mask_node.outputs["out"] = Type::Color4;
    *result = {mask_node.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, mask_node.name);
    graph->nodes.push_back(std::move(mask_node));
    return finish(true);
  }

  if (nodedef == mix_color4_id || nodedef == mix_color4_color4_id) {
    Node mix;
    mix.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    mix.nodedef = nodedef;
    for (const char *name : {"bg", "fg"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
        set_error(error_message, nodedef + " requires color4 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_color4_output(
                operand, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message))
        {
          return finish(false);
        }
        mix.links[name] = link;
      }
      else {
        pxr::GfVec4f value;
        if (!operand.Get(&value) || !color4_is_finite(value)) {
          set_error(error_message,
                    nodedef + " requires literal finite or connected color4 input '" + name + "'");
          return finish(false);
        }
        mix.float4_inputs[name] = make_float4(value[0], value[1], value[2], value[3]);
      }
    }
    const bool color_factor = nodedef == mix_color4_color4_id;
    const pxr::UsdShadeInput factor = source_shader.GetInput(pxr::TfToken("mix"));
    if (!factor || factor.GetTypeName() != (color_factor ? pxr::SdfValueTypeNames->Color4f :
                                                           pxr::SdfValueTypeNames->Float))
    {
      set_error(error_message,
                nodedef + " requires " + string(color_factor ? "color4" : "float") +
                    " input 'mix'");
      return finish(false);
    }
    if (factor.HasConnectedSource()) {
      Link link;
      if (color_factor) {
        if (!read_color4_output(
                factor, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message))
        {
          return finish(false);
        }
      }
      else {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(factor,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               emitted_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
      }
      mix.links["mix"] = link;
    }
    else if (color_factor) {
      pxr::GfVec4f value;
      if (!factor.Get(&value) || !color4_is_finite(value)) {
        set_error(error_message, nodedef + " requires literal finite or connected color4 input 'mix'");
        return finish(false);
      }
      mix.float4_inputs["mix"] = make_float4(value[0], value[1], value[2], value[3]);
    }
    else {
      float value;
      if (!factor.Get(&value) || !std::isfinite(value)) {
        set_error(error_message, nodedef + " requires literal finite or connected float input 'mix'");
        return finish(false);
      }
      mix.inputs["mix"] = value;
    }
    mix.outputs["out"] = Type::Color4;
    *result = {mix.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, mix.name);
    graph->nodes.push_back(std::move(mix));
    return finish(true);
  }

  if (is_color4_conditional(nodedef)) {
    Node conditional;
    conditional.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    conditional.nodedef = nodedef;
    for (const char *name : {"value1", "value2"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        Link link;
        if (!read_float_output(operand,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               emitted_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        conditional.links[name] = link;
      }
      else if (!operand.Get(&conditional.inputs[name]) || !std::isfinite(conditional.inputs[name])) {
        set_error(error_message, nodedef + " requires literal finite or connected float input '" + name + "'");
        return finish(false);
      }
    }
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
        set_error(error_message, nodedef + " requires color4 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_color4_output(operand, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        conditional.links[name] = link;
      }
      else {
        pxr::GfVec4f value;
        if (!operand.Get(&value) || !color4_is_finite(value)) {
          set_error(error_message, nodedef + " requires literal finite or connected color4 input '" + name + "'");
          return finish(false);
        }
        conditional.float4_inputs[name] = make_float4(value[0], value[1], value[2], value[3]);
      }
    }
    conditional.outputs["out"] = Type::Color4;
    *result = {conditional.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, conditional.name);
    graph->nodes.push_back(std::move(conditional));
    return finish(true);
  }

  if (is_native_noise_or_fractal_family(nodedef) && native_noise_or_fractal_is_color4(nodedef)) {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source_shader,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
        !read_native_noise_or_fractal_parameters(source_shader, &noise, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source_shader.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["position"] = position;
    }
    else {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["texcoord"] = texcoord;
    }
    noise.outputs["out"] = Type::Color4;
    *result = {noise.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, noise.name);
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (is_color4_operation(nodedef)) {
    Node operation;
    operation.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    operation.nodedef = nodedef;
    const bool unary = is_color4_unary_math(nodedef);
    const bool scalar_second = is_color4_scalar_math(nodedef);
    const bool scalar_invert = nodedef == invert_color4fa_id;
    const bool invert = nodedef == invert_color4_id || scalar_invert;
    const bool full_clamp = nodedef == clamp_color4_id;
    const bool scalar_clamp = nodedef == clamp_color4fa_id;
    const bool clamp = full_clamp || scalar_clamp;
    const char *first_name = (unary || scalar_invert || clamp) ?
                                 "in" :
                                 (invert ? "amount" : "in1");
    const char *second_name = scalar_invert ? "amount" : (invert ? "in" : "in2");
    for (const char *input_name : {first_name, second_name}) {
      if ((unary || clamp) && input_name == second_name) {
        break;
      }
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(input_name));
      if (!operand) {
        continue;
      }
      if (scalar_second && input_name == second_name) {
        if (operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input '" + input_name + "'");
          return finish(false);
        }
        if (operand.HasConnectedSource()) {
          Link link;
          std::unordered_set<string> active_float_shaders;
          std::unordered_map<string, string> emitted_float_shaders;
          if (!read_float_output(operand,
                                 graph,
                                 &link,
                                 &active_float_shaders,
                                 &emitted_float_shaders,
                                 emitted_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          operation.links[input_name] = link;
        }
        else {
          float value = 0.0f;
          if (!operand.Get(&value) || !std::isfinite(value) ||
              ((nodedef == divide_color4fa_id || nodedef == modulo_color4fa_id) && value == 0.0f))
          {
            set_error(error_message,
                      nodedef + " requires literal finite" +
                          ((nodedef == divide_color4fa_id || nodedef == modulo_color4fa_id) ?
                               " nonzero" :
                               "") +
                          " float input '" + input_name + "'");
            return finish(false);
          }
          operation.inputs[input_name] = value;
        }
        continue;
      }
      if (operand.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
        set_error(error_message, nodedef + " requires color4 input '" + input_name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_color4_output(
                operand, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message))
        {
          return finish(false);
        }
        operation.links[input_name] = link;
      }
      else {
        pxr::GfVec4f value;
        if (!operand.Get(&value) || !color4_is_finite(value)) {
          set_error(error_message,
                    nodedef + " requires literal finite or connected color4 input '" +
                        input_name + "'");
          return finish(false);
        }
        if ((nodedef == divide_color4_id || nodedef == modulo_color4_id) &&
            input_name == second_name &&
            (value[0] == 0.0f || value[1] == 0.0f || value[2] == 0.0f || value[3] == 0.0f))
        {
          set_error(error_message,
                    nodedef + " requires literal finite nonzero color4 input '" + input_name +
                        "'");
          return finish(false);
        }
        operation.float4_inputs[input_name] =
            make_float4(value[0], value[1], value[2], value[3]);
      }
    }
    if (clamp) {
      for (const char *edge_name : {"low", "high"}) {
        const pxr::UsdShadeInput edge = source_shader.GetInput(pxr::TfToken(edge_name));
        if (!edge) {
          continue;
        }
        if (scalar_clamp) {
          if (edge.GetTypeName() != pxr::SdfValueTypeNames->Float) {
            set_error(error_message, nodedef + " requires float input '" + edge_name + "'");
            return finish(false);
          }
          if (edge.HasConnectedSource()) {
            Link link;
            std::unordered_set<string> active_float_shaders;
            std::unordered_map<string, string> emitted_float_shaders;
            if (!read_float_output(edge,
                                   graph,
                                   &link,
                                   &active_float_shaders,
                                   &emitted_float_shaders,
                                   emitted_shaders,
                                   depth + 1,
                                   error_message))
            {
              return finish(false);
            }
            operation.links[edge_name] = link;
          }
          else {
            float value = 0.0f;
            if (!edge.Get(&value) || !std::isfinite(value)) {
              set_error(error_message,
                        nodedef + " requires literal finite or connected float input '" +
                            edge_name + "'");
              return finish(false);
            }
            operation.inputs[edge_name] = value;
          }
        }
        else {
          if (edge.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
            set_error(error_message, nodedef + " requires color4 input '" + edge_name + "'");
            return finish(false);
          }
          if (edge.HasConnectedSource()) {
            Link link;
            if (!read_color4_output(
                    edge, graph, &link, active_shaders, emitted_shaders, depth + 1, error_message))
            {
              return finish(false);
            }
            operation.links[edge_name] = link;
          }
          else {
            pxr::GfVec4f value;
            if (!edge.Get(&value) || !color4_is_finite(value)) {
              set_error(error_message,
                        nodedef + " requires literal finite or connected color4 input '" +
                            edge_name + "'");
              return finish(false);
            }
            operation.float4_inputs[edge_name] = make_float4(value[0], value[1], value[2], value[3]);
          }
        }
      }
      if (operation.inputs.contains("low") && operation.inputs.contains("high") &&
          operation.inputs.at("low") > operation.inputs.at("high"))
      {
        set_error(error_message, nodedef + " requires low <= high");
        return finish(false);
      }
      if (operation.float4_inputs.contains("low") && operation.float4_inputs.contains("high") &&
          (operation.float4_inputs.at("low").x > operation.float4_inputs.at("high").x ||
           operation.float4_inputs.at("low").y > operation.float4_inputs.at("high").y ||
           operation.float4_inputs.at("low").z > operation.float4_inputs.at("high").z ||
           operation.float4_inputs.at("low").w > operation.float4_inputs.at("high").w))
      {
        set_error(error_message, nodedef + " requires low <= high in every component");
        return finish(false);
      }
    }
    operation.outputs["out"] = Type::Color4;
    *result = {operation.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, operation.name);
    graph->nodes.push_back(std::move(operation));
    return finish(true);
  }

  if (is_color4_ramp(nodedef) || is_color4_split(nodedef)) {
    const bool split = is_color4_split(nodedef);
    const bool top_to_bottom = split ? split_is_top_to_bottom(nodedef) : nodedef == ramptb_color4_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    Node ramp;
    ramp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    ramp.nodedef = nodedef;
    const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
    if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Color4f ||
        source_shader.GetOutputs().size() != 1)
    {
      set_error(error_message, nodedef + " requires exactly one color4 output 'out'");
      return finish(false);
    }
    for (const pxr::UsdShadeInput &input : source_shader.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != first_name && name != second_name && name != "texcoord" &&
          (!split || name != "center")) {
        set_error(error_message, nodedef + " has unsupported input '" + name + "'");
        return finish(false);
      }
    }
    if (split) {
      const pxr::UsdShadeInput center = source_shader.GetInput(pxr::TfToken("center"));
      if (center) {
        if (center.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input 'center'");
          return finish(false);
        }
        if (center.HasConnectedSource()) {
          std::unordered_set<string> active_float_shaders;
          std::unordered_map<string, string> emitted_float_shaders;
          Link center_link;
          if (!read_float_output(center,
                                 graph,
                                 &center_link,
                                 &active_float_shaders,
                                 &emitted_float_shaders,
                                 emitted_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          ramp.links["center"] = center_link;
        }
        else if (!center.Get(&ramp.inputs["center"]) || !std::isfinite(ramp.inputs["center"])) {
          set_error(error_message, nodedef + " requires literal finite float input 'center'");
          return finish(false);
        }
      }
      else {
        ramp.inputs["center"] = 0.5f;
      }
    }
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput color_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec4f color(0.0f, 0.0f, 0.0f, 0.0f);
      if (color_input) {
        if (color_input.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
          set_error(error_message,
                    nodedef + " requires color4 input '" + input_name + "'");
          return finish(false);
        }
        if (color_input.HasConnectedSource()) {
          pxr::UsdShadeShader connected;
          if (!connected_shader(color_input, nullptr, &connected, error_message)) {
            if (error_message) {
              *error_message = nodedef + " requires connectable color4 input '" + input_name +
                               "': " + *error_message;
            }
            return finish(false);
          }
          pxr::TfToken connected_id;
          connected.GetShaderId(&connected_id);
          if (connected_id == pxr::TfToken(constant_color4_id)) {
            if (!read_constant_color4_output(connected, &color, error_message)) {
              return finish(false);
            }
            Node constant;
            constant.name = unique_node_name(
                *graph, connected.GetPrim().GetName().GetString(), connected.GetPath().GetString());
            constant.nodedef = constant_color4_id;
            constant.float4_inputs["value"] =
                make_float4(color[0], color[1], color[2], color[3]);
            constant.outputs["out"] = Type::Color4;
            ramp.links[input_name] = {constant.name, "out", Type::Color4};
            emitted_shaders->emplace(connected.GetPath().GetString(), constant.name);
            graph->nodes.push_back(std::move(constant));
            continue;
          }
          Link color_link;
          if (!read_color4_output(color_input,
                                  graph,
                                  &color_link,
                                  active_shaders,
                                  emitted_shaders,
                                  depth + 1,
                                  error_message))
          {
            if (error_message) {
              *error_message = nodedef + " requires connectable color4 input '" + input_name +
                               "': " + *error_message;
            }
            return finish(false);
          }
          ramp.links[input_name] = color_link;
          continue;
        }
        if (!color_input.Get(&color) || !color4_is_finite(color)) {
          set_error(error_message,
                    nodedef + " requires literal finite color4 input '" + input_name + "'");
          return finish(false);
        }
      }
      ramp.float4_inputs[input_name] = make_float4(color[0], color[1], color[2], color[3]);
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    ramp.links["texcoord"] = texcoord;
    ramp.outputs["out"] = Type::Color4;
    *result = {ramp.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, ramp.name);
    graph->nodes.push_back(std::move(ramp));
    return finish(true);
  }

  if (nodedef == convert_float_color4_id || nodedef == convert_boolean_color4_id ||
      nodedef == convert_integer_color4_id)
  {
    Link value;
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = nodedef;
    if (nodedef == convert_float_color4_id) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      if (!read_float_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &value,
                             &active_float_shaders,
                             &emitted_float_shaders,
                             emitted_shaders,
                             depth + 1,
                             error_message))
      {
        return finish(false);
      }
    }
    else if (nodedef == convert_boolean_color4_id) {
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    else {
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(source_shader.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    convert.links["in"] = value;
    convert.outputs["out"] = Type::Color4;
    *result = {convert.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, convert.name);
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == combine2_color4cf_id) {
    Link color;
    Link alpha;
    std::unordered_set<string> active_color_shaders;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in1")),
                           graph,
                           &color,
                           &active_color_shaders,
                           emitted_shaders,
                           depth + 1,
                           error_message) ||
        !read_float_output(source_shader.GetInput(pxr::TfToken("in2")),
                           graph,
                           &alpha,
                           &active_float_shaders,
                           &emitted_float_shaders,
                           emitted_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    Node combine;
    combine.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    combine.nodedef = nodedef;
    combine.links["in1"] = color;
    combine.links["in2"] = alpha;
    combine.outputs["out"] = Type::Color4;
    *result = {combine.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, combine.name);
    graph->nodes.push_back(std::move(combine));
    return finish(true);
  }

  if (nodedef == combine4_color4_id) {
    Node combine;
    combine.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    combine.nodedef = nodedef;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    for (const char *input_name : {"in1", "in2", "in3", "in4"}) {
      const pxr::UsdShadeInput component = source_shader.GetInput(pxr::TfToken(input_name));
      if (!component || component.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + input_name + "'");
        return finish(false);
      }
      if (component.HasConnectedSource()) {
        Link value;
        if (!read_float_output(component,
                               graph,
                               &value,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               emitted_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        combine.links[input_name] = value;
      }
      else {
        float value = 0.0f;
        if (!component.Get(&value) || !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires finite float input '" + input_name + "'");
          return finish(false);
        }
        combine.inputs[input_name] = value;
      }
    }
    combine.outputs["out"] = Type::Color4;
    *result = {combine.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, combine.name);
    graph->nodes.push_back(std::move(combine));
    return finish(true);
  }

  if (nodedef == convert_vector4_color4_id) {
    Link vector4;
    std::unordered_set<string> active_vector4_shaders;
    std::unordered_map<string, string> emitted_vector4_shaders;
    if (!read_vector4_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector4,
                             &active_vector4_shaders,
                             &emitted_vector4_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = convert_vector4_color4_id;
    convert.links["in"] = vector4;
    convert.outputs["out"] = Type::Color4;
    *result = {convert.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, convert.name);
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == convert_color3_color4_id) {
    Link color3;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in")),
                           graph,
                           &color3,
                           &active_color_shaders,
                           emitted_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = convert_color3_color4_id;
    convert.links["in"] = color3;
    convert.outputs["out"] = Type::Color4;
    *result = {convert.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, convert.name);
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == geompropvalue_color4_id) {
    const pxr::UsdShadeInput geomprop = source_shader.GetInput(pxr::TfToken("geomprop"));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_geompropvalue_color4 requires a literal string 'geomprop' input");
      return finish(false);
    }

    Node color;
    color.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    color.nodedef = geompropvalue_color4_id;
    color.string_inputs["geomprop"] = value;
    color.outputs["out"] = Type::Color4;
    *result = {color.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, color.name);
    graph->nodes.push_back(std::move(color));
    return finish(true);
  }

  if (nodedef == geomcolor_color4_id) {
    int index = 0;
    const pxr::UsdShadeInput index_input = source_shader.GetInput(pxr::TfToken("index"));
    if (index_input) {
      if (index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
          index_input.HasConnectedSource() || !index_input.Get(&index) || index < 0)
      {
        set_error(error_message, "ND_geomcolor_color4 'index' must be a literal non-negative integer");
        return finish(false);
      }
    }

    Node color;
    color.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    /* Reuses the ND_geompropvalue_color4 Cycles lowering just above -- see
     * geomcolor_color3_id in read_color_output() for the same
     * index-to-geomprop-name mapping (graph.cpp lowers this to an
     * AttributeNode for RGB plus a literal alpha=1.0, since geomcolor/
     * geompropvalue have no notion of a stored alpha default here). */
    color.nodedef = geompropvalue_color4_id;
    color.string_inputs["geomprop"] = geomcolor_attribute_name(index);
    color.outputs["out"] = Type::Color4;
    *result = {color.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, color.name);
    graph->nodes.push_back(std::move(color));
    return finish(true);
  }

  if (nodedef == usd_uv_texture_id) {
    /* ND_UsdUVTexture_23 (the default-version nodedef) does not declare an 'rgba'
     * output at all -- only ND_UsdUVTexture (v2.2) does -- so a v2.3 node reaching
     * here (Color4-typed connection) or any output name other than 'rgba' falls
     * through to the generic error below rather than being silently accepted. */
    const auto sources = input.GetConnectedSources();
    const string source_output = sources.size() == 1 ? sources[0].sourceName.GetString() : "out";
    if (source_output == "rgba") {
      Link texture;
      if (!compose_usd_uv_texture_color4(
              source_shader, nodedef, shader_path, graph, &texture, depth, error_message))
      {
        return finish(false);
      }
      *result = texture;
      emitted_shaders->emplace(shader_path, texture.source_node);
      return finish(true);
    }
  }

  if (nodedef != image_color4_id) {
    set_error(error_message,
              string("MaterialX Color4 input requires ND_image_color4 or a supported color4 "
                     "operation, got ") +
                  nodedef);
    return finish(false);
  }

  for (const pxr::UsdShadeInput &image_input : source_shader.GetInputs()) {
    const string name = image_input.GetBaseName().GetString();
    if (name != "file" && name != "texcoord") {
      set_error(error_message,
                string("ND_image_color4 has no supported Cycles control: ") + name);
      return finish(false);
    }
  }

  const pxr::UsdShadeInput file_input = source_shader.GetInput(pxr::TfToken("file"));
  pxr::SdfAssetPath asset_path;
  if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
      file_input.HasConnectedSource() || !file_input.Get(&asset_path))
  {
    set_error(error_message, "ND_image_color4 requires a literal asset 'file' input");
    return finish(false);
  }
  string file_path = asset_path.GetResolvedPath();
  if (file_path.empty()) {
    file_path = asset_path.GetAssetPath();
  }
  if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
      path_file_size(file_path) == 0)
  {
    set_error(error_message, "ND_image_color4 file asset is unavailable or invalid");
    return finish(false);
  }

  Link texcoord;
  std::unordered_set<string> active_vector2_shaders;
  if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                           graph,
                           &texcoord,
                           &active_vector2_shaders,
                           depth + 1,
                           error_message))
  {
    return finish(false);
  }

  Node image;
  image.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
  image.nodedef = image_color4_id;
  image.asset_inputs["file"] = file_path;
  image.links["texcoord"] = texcoord;
  image.outputs["out"] = Type::Color4;
  *result = {image.name, "out", Type::Color4};
  emitted_shaders->emplace(shader_path, image.name);
  graph->nodes.push_back(std::move(image));
  return finish(true);
}

bool read_color_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_color4_shaders,
                       const int depth,
                       string *error_message,
                       std::unordered_map<string, string> *emitted_float_shaders)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX color graph nesting exceeds maximum depth");
    return false;
  }

  pxr::UsdShadeShader source_shader;
  if (!connected_shader_eliding_value_dot(input, &source_shader, error_message)) {
    return false;
  }
  const string shader_path = source_shader.GetPath().GetString();
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX color graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source_shader.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();

  if (nodedef == blur_color3_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source_shader, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in")),
                           graph,
                           &input_link,
                           active_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message,
                           emitted_float_shaders))
    {
      return finish(false);
    }
    Node blur;
    blur.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    blur.nodedef = nodedef;
    blur.links["in"] = input_link;
    blur.inputs["size"] = 0.0f;
    blur.string_inputs["filtertype"] = "box";
    blur.outputs["out"] = Type::Color3;
    *result = {blur.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(blur));
    return finish(true);
  }

  if (nodedef == convert_color4_color3_id) {
    Link color4;
    std::unordered_set<string> active_color4_shaders;
    if (!read_color4_output(source_shader.GetInput(pxr::TfToken("in")),
                            graph,
                            &color4,
                            &active_color4_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = convert_color4_color3_id;
    convert.links["in"] = color4;
    convert.outputs["out"] = Type::Color3;
    *result = {convert.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == constant_color3_id) {
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("value"));
    pxr::GfVec3f value;
    if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
        value_input.HasConnectedSource() || !value_input.Get(&value))
    {
      set_error(error_message, "ND_constant_color3 requires a literal color3 'value' input");
      return finish(false);
    }

    Node color;
    color.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    color.nodedef = constant_color3_id;
    color.color3_inputs["value"] = make_float3(value[0], value[1], value[2]);
    color.outputs["out"] = Type::Color3;
    *result = {color.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(color));
    return finish(true);
  }

  if (nodedef == blackbody_id) {
    /* See blackbody_id's declaration comment above for the real nodedef and
     * native Cycles node citation. */
    Node blackbody;
    blackbody.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    blackbody.nodedef = blackbody_id;
    std::unordered_map<string, string> local_emitted_float_shaders;
    if (!read_float_operand(source_shader,
                            nodedef,
                            "temperature",
                            graph,
                            &blackbody,
                            active_shaders,
                            emitted_float_shaders ? emitted_float_shaders :
                                                    &local_emitted_float_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    blackbody.outputs["out"] = Type::Color3;
    *result = {blackbody.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(blackbody));
    return finish(true);
  }

  if (nodedef == geompropvalue_color3_id) {
    const pxr::UsdShadeInput geomprop = source_shader.GetInput(pxr::TfToken("geomprop"));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_geompropvalue_color3 requires a literal string 'geomprop' input");
      return finish(false);
    }

    Node color;
    color.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    color.nodedef = geompropvalue_color3_id;
    color.string_inputs["geomprop"] = value;
    color.outputs["out"] = Type::Color3;
    *result = {color.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(color));
    return finish(true);
  }

  if (nodedef == geomcolor_color3_id) {
    int index = 0;
    const pxr::UsdShadeInput index_input = source_shader.GetInput(pxr::TfToken("index"));
    if (index_input) {
      if (index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
          index_input.HasConnectedSource() || !index_input.Get(&index) || index < 0)
      {
        set_error(error_message, "ND_geomcolor_color3 'index' must be a literal non-negative integer");
        return finish(false);
      }
    }

    Node color;
    color.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    /* Reuses the existing ND_geompropvalue_color3 Cycles lowering (see
     * graph.cpp geompropvalue_float_id/geompropvalue_color3_id AttributeNode
     * case) -- geomcolor's index is resolved to a geomprop name here rather
     * than adding a parallel lowering path for an id-only shape difference. */
    color.nodedef = geompropvalue_color3_id;
    color.string_inputs["geomprop"] = geomcolor_attribute_name(index);
    color.outputs["out"] = Type::Color3;
    *result = {color.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(color));
    return finish(true);
  }

  if (nodedef == convert_float_color3_id) {
    Link value;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    if (!read_float_output(source_shader.GetInput(pxr::TfToken("in")),
                           graph,
                           &value,
                           &active_float_shaders,
                           &emitted_float_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = convert_float_color3_id;
    convert.links["in"] = value;
    convert.outputs["out"] = Type::Color3;
    *result = {convert.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == convert_vector4_color3_id) {
    Link value;
    std::unordered_set<string> active_vector4_shaders;
    std::unordered_map<string, string> emitted_vector4_shaders;
    if (!read_vector4_output(source_shader.GetInput(pxr::TfToken("in")),
                             graph,
                             &value,
                             &active_vector4_shaders,
                             &emitted_vector4_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = nodedef;
    convert.links["in"] = value;
    convert.outputs["out"] = Type::Color3;
    *result = {convert.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == convert_vector3_color3_id || nodedef == convert_vector2_color3_id) {
    Link value;
    std::unordered_set<string> active_vector_shaders;
    const bool read_ok = nodedef == convert_vector3_color3_id ?
        read_vector3_output(source_shader.GetInput(pxr::TfToken("in")), graph, &value,
                            &active_vector_shaders, depth + 1, error_message) :
        read_vector2_output(source_shader.GetInput(pxr::TfToken("in")), graph, &value,
                            &active_vector_shaders, depth + 1, error_message);
    if (!read_ok) return finish(false);
    Node convert;
    convert.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = nodedef;
    convert.links["in"] = value;
    convert.outputs["out"] = Type::Color3;
    *result = {convert.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if (nodedef == convert_generic_id) {
    const pxr::UsdShadeInput in_input = source_shader.GetInput(pxr::TfToken("in"));
    if (!in_input) {
      set_error(error_message, "MaterialX <convert> node has no 'in' input");
      return finish(false);
    }
    const pxr::SdfValueTypeName in_type = in_input.GetTypeName();
    Link value;
    string resolved_nodedef;
    bool ok = false;
    if (in_type == pxr::SdfValueTypeNames->Float) {
      resolved_nodedef = convert_float_color3_id;
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      ok = read_float_output(in_input,
                             graph,
                             &value,
                             &active_float_shaders,
                             &emitted_float_shaders,
                             emitted_color4_shaders,
                             depth + 1,
                             error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Float3) {
      resolved_nodedef = convert_vector3_color3_id;
      std::unordered_set<string> active_vector_shaders;
      ok = read_vector3_output(
          in_input, graph, &value, &active_vector_shaders, depth + 1, error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Float2) {
      resolved_nodedef = convert_vector2_color3_id;
      std::unordered_set<string> active_vector_shaders;
      ok = read_vector2_output(
          in_input, graph, &value, &active_vector_shaders, depth + 1, error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Color4f) {
      resolved_nodedef = convert_color4_color3_id;
      std::unordered_set<string> active_color4_shaders;
      ok = read_color4_output(in_input,
                              graph,
                              &value,
                              &active_color4_shaders,
                              emitted_color4_shaders,
                              depth + 1,
                              error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Bool) {
      resolved_nodedef = convert_boolean_color3_id;
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      ok = read_boolean_output(in_input,
                               graph,
                               &value,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Int) {
      resolved_nodedef = convert_integer_color3_id;
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      ok = read_integer_output(in_input,
                               graph,
                               &value,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message);
    }
    else if (in_type == pxr::SdfValueTypeNames->Float4) {
      resolved_nodedef = convert_vector4_color3_id;
      std::unordered_set<string> active_vector4_shaders;
      std::unordered_map<string, string> emitted_vector4_shaders;
      ok = read_vector4_output(in_input,
                               graph,
                               &value,
                               &active_vector4_shaders,
                               &emitted_vector4_shaders,
                               depth + 1,
                               error_message);
    }
    else {
      set_error(error_message,
                string("MaterialX generic <convert> to color3 has no defined or supported "
                       "conversion from its 'in' type (") +
                    in_type.GetAsToken().GetString() + ")");
      return finish(false);
    }
    if (!ok) {
      return finish(false);
    }
    Node convert;
    convert.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    convert.nodedef = resolved_nodedef;
    convert.links["in"] = value;
    convert.outputs["out"] = Type::Color3;
    *result = {convert.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(convert));
    return finish(true);
  }

  if ((is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) && native_noise_or_fractal_is_color3(nodedef)) {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source_shader,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != native_noise_or_fractal_usd_output_type(nodedef) ||
        !read_native_noise_or_fractal_parameters(source_shader, &noise, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source_shader.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["position"] = position;
    }
    else {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      noise.links["texcoord"] = texcoord;
    }
    noise.outputs["out"] = Type::Color3;
    *result = {noise.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (nodedef == combine3_color3_id) {
    Node combine;
    combine.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    combine.nodedef = nodedef;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    for (const char *name : {"in1", "in2", "in3"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) return finish(false);
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_float_output(operand, graph, &link, &active_float_shaders, &emitted_float_shaders,
                               emitted_color4_shaders,
                               depth + 1, error_message)) return finish(false);
        combine.links[name] = link;
      }
      else if (!operand.Get(&combine.inputs[name])) return finish(false);
    }
    combine.outputs["out"] = Type::Color3;
    *result = {combine.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(combine));
    return finish(true);
  }

  if (nodedef == noise2d_color3fa_id) {
    Node noise;
    for (const auto &[input_name, expected] : {std::pair{"amplitude", 1.0f},
                                                std::pair{"pivot", 0.0f}})
    {
      const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken(input_name));
      float value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || value != expected)
      {
        set_error(error_message, "ND_noise2d_color3FA requires its direct identity parameters");
        return finish(false);
      }
      noise.inputs[input_name] = value;
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = noise2d_color3fa_id;
    noise.links["texcoord"] = texcoord;
    noise.outputs["out"] = Type::Color3;
    *result = {noise.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (nodedef == noise3d_color3_id || nodedef == noise3d_color3fa_id) {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeInput pivot = source_shader.GetInput(pxr::TfToken("pivot"));
    if (!pivot || pivot.GetTypeName() != pxr::SdfValueTypeNames->Float || pivot.HasConnectedSource() ||
        !pivot.Get(&noise.inputs["pivot"]) || !std::isfinite(noise.inputs["pivot"]))
    {
      set_error(error_message, nodedef + " requires literal finite float input 'pivot'");
      return finish(false);
    }
    const pxr::UsdShadeInput amplitude = source_shader.GetInput(pxr::TfToken("amplitude"));
    if (nodedef == noise3d_color3_id) {
      pxr::GfVec3f value;
      if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
          amplitude.HasConnectedSource() || !amplitude.Get(&value))
      {
        set_error(error_message, nodedef + " requires literal vector3 input 'amplitude'");
        return finish(false);
      }
      noise.vector3_inputs["amplitude"] = make_float3(value[0], value[1], value[2]);
    }
    else if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Float ||
             amplitude.HasConnectedSource() || !amplitude.Get(&noise.inputs["amplitude"]) ||
             !std::isfinite(noise.inputs["amplitude"]))
    {
      set_error(error_message, nodedef + " requires literal finite float input 'amplitude'");
      return finish(false);
    }
    Link position;
    std::unordered_set<string> active_vector_shaders;
    if (!read_vector3_output(source_shader.GetInput(pxr::TfToken("position")),
                             graph,
                             &position,
                             &active_vector_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    noise.links["position"] = position;
    noise.outputs["out"] = Type::Color3;
    *result = {noise.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (nodedef == noise2d_color3_id) {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeInput pivot = source_shader.GetInput(pxr::TfToken("pivot"));
    if (!pivot || pivot.GetTypeName() != pxr::SdfValueTypeNames->Float || pivot.HasConnectedSource() ||
        !pivot.Get(&noise.inputs["pivot"]) || !std::isfinite(noise.inputs["pivot"]))
    {
      set_error(error_message, nodedef + " requires literal finite float input 'pivot'");
      return finish(false);
    }
    const pxr::UsdShadeInput amplitude = source_shader.GetInput(pxr::TfToken("amplitude"));
    pxr::GfVec3f amplitude_value;
    if (!amplitude || amplitude.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
        amplitude.HasConnectedSource() || !amplitude.Get(&amplitude_value) ||
        !std::isfinite(amplitude_value[0]) || !std::isfinite(amplitude_value[1]) ||
        !std::isfinite(amplitude_value[2]))
    {
      set_error(error_message, nodedef + " requires literal finite color3 input 'amplitude'");
      return finish(false);
    }
    noise.vector3_inputs["amplitude"] = make_float3(
        amplitude_value[0], amplitude_value[1], amplitude_value[2]);
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    noise.links["texcoord"] = texcoord;
    noise.outputs["out"] = Type::Color3;
    *result = {noise.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(noise));
    return finish(true);
  }

  if (nodedef == checkerboard_color3_id) {
    Node checker;
    checker.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    checker.nodedef = checkerboard_color3_id;
    for (const char *input_name : {"color1", "color2"}) {
      const pxr::UsdShadeInput color_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec3f color;
      if (!color_input || color_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          color_input.HasConnectedSource() || !color_input.Get(&color))
      {
        set_error(error_message, "ND_checkerboard_color3 requires literal color inputs");
        return finish(false);
      }
      checker.color3_inputs[input_name] = make_float3(color[0], color[1], color[2]);
    }
    for (const char *input_name : {"uvtiling", "uvoffset"}) {
      const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec2f value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
          value_input.HasConnectedSource() || !value_input.Get(&value))
      {
        set_error(error_message, "ND_checkerboard_color3 requires literal vector2 coordinates");
        return finish(false);
      }
      checker.vector2_inputs[input_name] = make_float2(value[0], value[1]);
    }
    const float2 tiling = checker.vector2_inputs["uvtiling"];
    const float2 offset = checker.vector2_inputs["uvoffset"];
    if (tiling.x != tiling.y || tiling.x <= 0.0f || offset.x != 0.0f || offset.y != 0.0f) {
      set_error(error_message, "ND_checkerboard_color3 requires uniform positive tiling and zero offset");
      return finish(false);
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    checker.links["texcoord"] = texcoord;
    checker.outputs["out"] = Type::Color3;
    *result = {checker.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(checker));
    return finish(true);
  }

  if (nodedef == image_color3_id) {
    const pxr::UsdShadeInput file_input = source_shader.GetInput(pxr::TfToken("file"));
    pxr::SdfAssetPath asset_path;
    if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
        file_input.HasConnectedSource() || !file_input.Get(&asset_path))
    {
      set_error(error_message, "ND_image_color3 requires a literal asset 'file' input");
      return finish(false);
    }
    string file_path = asset_path.GetResolvedPath();
    if (file_path.empty()) {
      file_path = asset_path.GetAssetPath();
    }
    if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
        path_file_size(file_path) == 0)
    {
      set_error(error_message, "ND_image_color3 file asset is unavailable or invalid");
      return finish(false);
    }

    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    Node image;
    image.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    image.nodedef = image_color3_id;
    image.asset_inputs["file"] = file_path;
    image.links["texcoord"] = texcoord;
    image.outputs["out"] = Type::Color3;

    *result = {image.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(image));
    return finish(true);
  }

  if (nodedef == ramplr_color3_id || nodedef == ramptb_color3_id || is_color3_split(nodedef)) {
    const bool split = is_color3_split(nodedef);
    const bool top_to_bottom = split ? split_is_top_to_bottom(nodedef) : nodedef == ramptb_color3_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    Node ramp;
    ramp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    ramp.nodedef = nodedef;
    if (split) {
      const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
      if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          source_shader.GetOutputs().size() != 1)
      {
        set_error(error_message, nodedef + " requires exactly one color3 output 'out'");
        return finish(false);
      }
      for (const pxr::UsdShadeInput &input : source_shader.GetInputs()) {
        const string name = input.GetBaseName().GetString();
        if (name != first_name && name != second_name && name != "center" && name != "texcoord") {
          set_error(error_message, nodedef + " has unsupported input '" + name + "'");
          return finish(false);
        }
      }
      const pxr::UsdShadeInput center = source_shader.GetInput(pxr::TfToken("center"));
      if (center) {
        if (center.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input 'center'");
          return finish(false);
        }
        if (center.HasConnectedSource()) {
          std::unordered_set<string> active_float_shaders;
          std::unordered_map<string, string> emitted_float_shaders;
          Link center_link;
          if (!read_float_output(center,
                                 graph,
                                 &center_link,
                                 &active_float_shaders,
                                 &emitted_float_shaders,
                                 emitted_color4_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          ramp.links["center"] = center_link;
        }
        else if (!center.Get(&ramp.inputs["center"]) || !std::isfinite(ramp.inputs["center"])) {
          set_error(error_message, nodedef + " requires literal finite float input 'center'");
          return finish(false);
        }
      }
      else {
        ramp.inputs["center"] = 0.5f;
      }
    }
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput color_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec3f color(0.0f, 0.0f, 0.0f);
      if (color_input) {
        if (color_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
          set_error(error_message,
                    nodedef + " requires color3 input '" + input_name + "'");
          return finish(false);
        }
        if (color_input.HasConnectedSource()) {
          Link color_link;
          if (!read_color_output(color_input,
                                 graph,
                                 &color_link,
                                 active_shaders,
                                 emitted_color4_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          ramp.links[input_name] = color_link;
          continue;
        }
        if (!color_input.Get(&color) || !std::isfinite(color[0]) || !std::isfinite(color[1]) ||
            !std::isfinite(color[2]))
        {
          set_error(error_message,
                    nodedef + " requires literal finite color3 input '" + input_name + "'");
          return finish(false);
        }
      }
      ramp.color3_inputs[input_name] = make_float3(color[0], color[1], color[2]);
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    ramp.links["texcoord"] = texcoord;
    ramp.outputs["out"] = Type::Color3;
    *result = {ramp.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(ramp));
    return finish(true);
  }

  if (nodedef == ramplr_float_id || nodedef == ramptb_float_id || is_scalar_split(nodedef)) {
    const bool split = is_scalar_split(nodedef);
    const bool top_to_bottom = split ? split_is_top_to_bottom(nodedef) : nodedef == ramptb_float_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    Node ramp;
    ramp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    ramp.nodedef = nodedef;
    if (split) {
      const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
      if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          source_shader.GetOutputs().size() != 1)
      {
        set_error(error_message, nodedef + " requires exactly one float output 'out'");
        return finish(false);
      }
      for (const pxr::UsdShadeInput &input : source_shader.GetInputs()) {
        const string name = input.GetBaseName().GetString();
        if (name != first_name && name != second_name && name != "center" && name != "texcoord") {
          set_error(error_message, nodedef + " has unsupported input '" + name + "'");
          return finish(false);
        }
      }
      const pxr::UsdShadeInput center = source_shader.GetInput(pxr::TfToken("center"));
      if (center) {
        if (center.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input 'center'");
          return finish(false);
        }
        if (center.HasConnectedSource()) {
          Link center_link;
          if (!read_float_output(center,
                                 graph,
                                 &center_link,
                                 active_shaders,
                                 emitted_color4_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          ramp.links["center"] = center_link;
        }
        else if (!center.Get(&ramp.inputs["center"]) || !std::isfinite(ramp.inputs["center"])) {
          set_error(error_message, nodedef + " requires literal finite float input 'center'");
          return finish(false);
        }
      }
      else {
        ramp.inputs["center"] = 0.5f;
      }
    }
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken(input_name));
      float value = 0.0f;
      if (value_input) {
        if (value_input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input '" + input_name + "'");
          return finish(false);
        }
        if (value_input.HasConnectedSource()) {
          Link value_link;
          if (!read_float_output(value_input,
                                 graph,
                                 &value_link,
                                 active_shaders,
                                 emitted_color4_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          ramp.links[input_name] = value_link;
          continue;
        }
        if (!value_input.Get(&value) || !std::isfinite(value)) {
          set_error(error_message,
                    nodedef + " requires literal finite float input '" + input_name + "'");
          return finish(false);
        }
      }
      ramp.inputs[input_name] = value;
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source_shader.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    ramp.links["texcoord"] = texcoord;
    ramp.outputs["out"] = Type::Float;
    *result = {ramp.name, "out", Type::Float};
    graph->nodes.push_back(std::move(ramp));
    return finish(true);
  }

  if (is_color_blend(nodedef) || nodedef == mix_color3_id ||
      nodedef == mix_color3_color3_id)
  {
    const bool color_factor = nodedef == mix_color3_color3_id;
    Node mix;
    mix.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    mix.nodedef = nodedef;
    for (const char *name : {"bg", "fg"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
        set_error(error_message, nodedef + " requires color3 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_color_output(operand,
                               graph,
                               &link,
                               active_shaders,
                               emitted_color4_shaders,
                               depth + 1,
                               error_message,
                               emitted_float_shaders))
          return finish(false);
        mix.links[name] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!operand.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
            !std::isfinite(value[2]))
        {
          set_error(error_message, nodedef + " requires literal or connected color3 input '" + name + "'");
          return finish(false);
        }
        mix.color3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput factor = source_shader.GetInput(pxr::TfToken("mix"));
    if (!factor ||
        factor.GetTypeName() !=
            (color_factor ? pxr::SdfValueTypeNames->Color3f : pxr::SdfValueTypeNames->Float))
    {
      set_error(error_message,
                nodedef + " requires " + string(color_factor ? "color3" : "float") +
                    " input 'mix'");
      return finish(false);
    }
    if (factor.HasConnectedSource()) {
      Link link;
      if (color_factor) {
        if (!read_color_output(factor,
                               graph,
                               &link,
                               active_shaders,
                               emitted_color4_shaders,
                               depth + 1,
                               error_message,
                               emitted_float_shaders))
        {
          return finish(false);
        }
      }
      else {
        std::unordered_map<string, string> local_emitted_float_shaders;
        if (!read_float_output(factor,
                               graph,
                               &link,
                               active_shaders,
                               emitted_float_shaders ? emitted_float_shaders :
                                                       &local_emitted_float_shaders,
                               emitted_color4_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
      }
      mix.links["mix"] = link;
    }
    else if (color_factor) {
      pxr::GfVec3f value;
      if (!factor.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal or connected color3 input 'mix'");
        return finish(false);
      }
      mix.color3_inputs["mix"] = make_float3(value[0], value[1], value[2]);
    }
    else {
      float value;
      if (!factor.Get(&value) || !std::isfinite(value)) {
        set_error(error_message, nodedef + " requires literal or connected float input 'mix'");
        return finish(false);
      }
      mix.inputs["mix"] = value;
    }
    mix.outputs["out"] = Type::Color3;
    *result = {mix.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(mix));
    return finish(true);
  }

  if (is_inside_outside_color3(nodedef)) {
    Node mask_node;
    mask_node.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    mask_node.nodedef = nodedef;
    const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken("in"));
    if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
      set_error(error_message, nodedef + " requires color3 input 'in'");
      return finish(false);
    }
    if (value_input.HasConnectedSource()) {
      if (!read_color_output(value_input,
                             graph,
                             &mask_node.links["in"],
                             active_shaders,
                             emitted_color4_shaders,
                             depth + 1,
                             error_message,
                             emitted_float_shaders))
      {
        return finish(false);
      }
    }
    else {
      pxr::GfVec3f value;
      if (!value_input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal finite or connected color3 input 'in'");
        return finish(false);
      }
      mask_node.color3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    std::unordered_map<string, string> local_emitted_float_shaders;
    if (!read_float_operand(source_shader,
                            nodedef,
                            "mask",
                            graph,
                            &mask_node,
                            active_shaders,
                            emitted_float_shaders ? emitted_float_shaders : &local_emitted_float_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    mask_node.outputs["out"] = Type::Color3;
    *result = {mask_node.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(mask_node));
    return finish(true);
  }

  if (is_color_conditional(nodedef)) {
    Node conditional;
    conditional.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    conditional.nodedef = nodedef;
    for (const char *name : {"value1", "value2"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        Link link;
        if (!read_float_output(operand,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               emitted_color4_shaders,
                               depth + 1,
                               error_message))
          return finish(false);
        conditional.links[name] = link;
      }
      else if (!operand.Get(&conditional.inputs[name])) {
        set_error(error_message, nodedef + " requires literal or connected float input '" + name + "'");
        return finish(false);
      }
    }
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
        set_error(error_message, nodedef + " requires color3 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_color_output(
                operand, graph, &link, active_shaders, emitted_color4_shaders, depth + 1, error_message))
          return finish(false);
        conditional.links[name] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!operand.Get(&value)) {
          set_error(error_message, nodedef + " requires literal or connected color3 input '" + name + "'");
          return finish(false);
        }
        conditional.color3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    conditional.outputs["out"] = Type::Color3;
    *result = {conditional.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(conditional));
    return finish(true);
  }

  if (nodedef == rgbtohsv_color3_id || nodedef == hsvtorgb_color3_id) {
    const pxr::UsdShadeInput input = source_shader.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
      set_error(error_message, nodedef + " requires color3 input 'in'");
      return finish(false);
    }
    Link color;
    if (!read_color_output(
            input, graph, &color, active_shaders, emitted_color4_shaders, depth + 1, error_message)) {
      return finish(false);
    }
    Node conversion;
    conversion.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    conversion.nodedef = nodedef;
    conversion.links["in"] = color;
    conversion.outputs["out"] = Type::Color3;
    *result = {conversion.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(conversion));
    return finish(true);
  }

  if (is_contrast_color3(nodedef)) {
    const bool scalar_parameters = contrast_uses_scalar_parameters(nodedef);
    Node contrast;
    contrast.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    contrast.nodedef = nodedef;
    for (const char *input_name : {"pivot", "amount"}) {
      const pxr::UsdShadeInput parameter = source_shader.GetInput(pxr::TfToken(input_name));
      if (!parameter || parameter.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal parameter inputs");
        return finish(false);
      }
      if (scalar_parameters) {
        float value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Float || !parameter.Get(&value) ||
            !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires literal finite float input '" + input_name + "'");
          return finish(false);
        }
        contrast.inputs[input_name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Color3f || !parameter.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
          set_error(error_message, nodedef + " requires literal finite color3 input '" + input_name + "'");
          return finish(false);
        }
        contrast.color3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput input = source_shader.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
      set_error(error_message, nodedef + " requires color3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_color_output(
              input, graph, &link, active_shaders, emitted_color4_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      contrast.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal finite or connected color3 input 'in'");
        return finish(false);
      }
      contrast.color3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    contrast.outputs["out"] = Type::Color3;
    *result = {contrast.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(contrast));
    return finish(true);
  }

  if (nodedef == remap_color3_id || nodedef == range_color3_id || nodedef == remap_color3fa_id ||
      nodedef == range_color3fa_id) {
    const bool scalar_bounds = nodedef == remap_color3fa_id || nodedef == range_color3fa_id;
    Node range;
    range.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    range.nodedef = nodedef;
    for (const char *input_name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput range_input = source_shader.GetInput(pxr::TfToken(input_name));
      if (!range_input || range_input.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal bounds");
        return finish(false);
      }
      if (scalar_bounds) {
        float value;
        if (range_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            !range_input.Get(&value) || !std::isfinite(value)) {
          set_error(error_message,
                    nodedef + " requires literal finite float input '" + input_name + "'");
          return finish(false);
        }
        range.inputs[input_name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (range_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
            !range_input.Get(&value) || !std::isfinite(value[0]) ||
            !std::isfinite(value[1]) || !std::isfinite(value[2]))
        {
          set_error(error_message,
                    nodedef + " requires literal finite color3 input '" + input_name + "'");
          return finish(false);
        }
        range.color3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
      }
    }
    if (scalar_bounds ? range.inputs.at("inlow") == range.inputs.at("inhigh") :
                        (range.color3_inputs.at("inlow").x == range.color3_inputs.at("inhigh").x ||
                         range.color3_inputs.at("inlow").y == range.color3_inputs.at("inhigh").y ||
                         range.color3_inputs.at("inlow").z == range.color3_inputs.at("inhigh").z)) {
      set_error(error_message, nodedef + " requires inlow != inhigh in every component");
      return finish(false);
    }
    if (nodedef == range_color3_id || nodedef == range_color3fa_id) {
      const pxr::UsdShadeInput gamma_input = source_shader.GetInput(pxr::TfToken("gamma"));
      if (scalar_bounds) {
        float gamma;
        if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) || gamma != 1.0f)
        {
          set_error(error_message, nodedef + " requires literal gamma 1.0");
          return finish(false);
        }
      }
      else {
        pxr::GfVec3f gamma;
        if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
            gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) || gamma[0] != 1.0f ||
            gamma[1] != 1.0f || gamma[2] != 1.0f)
        {
          set_error(error_message, nodedef + " requires literal gamma (1, 1, 1)");
          return finish(false);
        }
      }
      const pxr::UsdShadeInput clamp_input = source_shader.GetInput(pxr::TfToken("doclamp"));
      bool do_clamp;
      if (!clamp_input || clamp_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
          clamp_input.HasConnectedSource() || !clamp_input.Get(&do_clamp))
      {
        set_error(error_message, nodedef + " requires literal boolean 'doclamp'");
        return finish(false);
      }
      const float3 outlow = scalar_bounds ? make_float3(range.inputs.at("outlow")) :
                                           range.color3_inputs.at("outlow");
      const float3 outhigh = scalar_bounds ? make_float3(range.inputs.at("outhigh")) :
                                            range.color3_inputs.at("outhigh");
      if (do_clamp && (outlow.x > outhigh.x || outlow.y > outhigh.y || outlow.z > outhigh.z)) {
        set_error(error_message, nodedef + " requires outlow <= outhigh in every component when clamped");
        return finish(false);
      }
      range.int_inputs["doclamp"] = do_clamp ? 1 : 0;
    }
    const pxr::UsdShadeInput input = source_shader.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
      set_error(error_message, nodedef + " requires color3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_color_output(
              input, graph, &link, active_shaders, emitted_color4_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      range.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal finite or connected color3 input 'in'");
        return finish(false);
      }
      range.color3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    range.outputs["out"] = Type::Color3;
    *result = {range.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(range));
    return finish(true);
  }

  if (is_color_unary_math(nodedef)) {
    Link value;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in")),
                           graph,
                           &value,
                           active_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
      return finish(false);
    Node math;
    math.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    math.nodedef = nodedef;
    math.links["in"] = value;
    math.outputs["out"] = Type::Color3;
    *result = {math.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(math));
    return finish(true);
  }

  if (is_color_math(nodedef) || is_color_binary_component_math(nodedef) ||
      nodedef == invert_color3_id || nodedef == invert_color3fa_id)
  {
    Link first;
    Link second;
    const bool scalar_amount = nodedef == invert_color3fa_id;
    const char *first_input = (nodedef == invert_color3_id || scalar_amount) ? "amount" : "in1";
    const char *second_input = (nodedef == invert_color3_id || scalar_amount) ? "in" : "in2";
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    Node math;
    if ((scalar_amount ? !read_float_operand(source_shader,
                                             nodedef,
                                             first_input,
                                             graph,
                                             &math,
                                             &active_float_shaders,
                                             &emitted_float_shaders,
                                             emitted_color4_shaders,
                                             depth + 1,
                                             error_message) :
                       !read_color_output(source_shader.GetInput(pxr::TfToken(first_input)),
                                          graph,
                                          &first,
                                          active_shaders,
                                          emitted_color4_shaders,
                                          depth + 1,
                                          error_message)) ||
        !read_color_output(source_shader.GetInput(pxr::TfToken(second_input)),
                           graph,
                           &second,
                           active_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }

    math.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    math.nodedef = nodedef;
    if (!scalar_amount) {
      math.links[first_input] = first;
    }
    math.links[second_input] = second;
    math.outputs["out"] = Type::Color3;
    *result = {math.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(math));
    return finish(true);
  }

  if (is_color_scalar_component_math(nodedef)) {
    Link color;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    Node math;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in1")),
                           graph,
                           &color,
                           active_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    if (nodedef == safepower_color3fa_id) {
      if (!read_float_operand(source_shader,
                              nodedef,
                              "in2",
                              graph,
                              &math,
                              &active_float_shaders,
                              &emitted_float_shaders,
                              emitted_color4_shaders,
                              depth + 1,
                              error_message))
      {
        return finish(false);
      }
    }
    else {
      Link scalar;
      if (!read_float_output(source_shader.GetInput(pxr::TfToken("in2")),
                             graph,
                             &scalar,
                             &active_float_shaders,
                             &emitted_float_shaders,
                             emitted_color4_shaders,
                             depth + 1,
                             error_message))
      {
        return finish(false);
      }
      math.links["in2"] = scalar;
    }
    math.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    math.nodedef = nodedef;
    math.links["in1"] = color;
    math.outputs["out"] = Type::Color3;
    *result = {math.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(math));
    return finish(true);
  }

  if (nodedef == clamp_color3_id || nodedef == clamp_color3fa_id) {
    const bool scalar_bounds = nodedef == clamp_color3fa_id;
    Node clamp;
    clamp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    clamp.nodedef = nodedef;
    for (const char *name : {"low", "high"}) {
      const pxr::UsdShadeInput bound = source_shader.GetInput(pxr::TfToken(name));
      if (!bound || bound.HasConnectedSource()) return finish(false);
      if (scalar_bounds) {
        float value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Float || !bound.Get(&value) || !std::isfinite(value)) return finish(false);
        clamp.inputs[name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Color3f || !bound.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) return finish(false);
        clamp.color3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput input = source_shader.GetInput(pxr::TfToken("in"));
    Link color;
    if (!read_color_output(input, graph, &color, active_shaders, emitted_color4_shaders, depth + 1, error_message)) return finish(false);
    clamp.links["in"] = color;
    clamp.outputs["out"] = Type::Color3;
    *result = {clamp.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(clamp));
    return finish(true);
  }

  if (is_color_scalar_math(nodedef)) {
    Link color;
    Link scalar;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    if (!read_color_output(source_shader.GetInput(pxr::TfToken("in1")),
                           graph,
                           &color,
                           active_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message) ||
        !read_float_output(source_shader.GetInput(pxr::TfToken("in2")),
                           graph,
                           &scalar,
                           &active_float_shaders,
                           &emitted_float_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    Node math;
    math.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    math.nodedef = nodedef;
    math.links["in1"] = color;
    math.links["in2"] = scalar;
    math.outputs["out"] = Type::Color3;
    *result = {math.name, "out", Type::Color3};
    graph->nodes.push_back(std::move(math));
    return finish(true);
  }

  if (nodedef == usd_uv_texture_id || nodedef == usd_uv_texture_23_id) {
    const auto sources = input.GetConnectedSources();
    const string source_output = sources.size() == 1 ? sources[0].sourceName.GetString() : "out";
    if (source_output == "rgb") {
      Link texture;
      if (!compose_usd_uv_texture_color4(
              source_shader, nodedef, shader_path, graph, &texture, depth, error_message))
      {
        return finish(false);
      }
      Node convert;
      convert.name = unique_node_name(
          *graph,
          source_shader.GetPrim().GetName().GetString() + ".rgb",
          shader_path + ".rgb");
      convert.nodedef = convert_color4_color3_id;
      convert.links["in"] = texture;
      convert.outputs["out"] = Type::Color3;
      *result = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return finish(true);
    }
    /* Every other output ('r'/'g'/'b'/'a'/'rgba') is not a color3 output of this
     * nodedef at all (r/g/b/a are float, rgba is color4) -- fall through to the
     * generic error below rather than silently accepting a mismatched connection. */
  }

  set_error(error_message,
            string("MaterialX color input requires a supported color3 node, got ") + nodedef);
  return finish(false);
}

bool read_color_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       const int depth,
                       string *error_message)
{
  std::unordered_map<string, string> emitted_color4_shaders;
  return read_color_output(input,
                           graph,
                           result,
                           active_shaders,
                           &emitted_color4_shaders,
                           depth,
                           error_message,
                           nullptr);
}

bool read_color_graph(const pxr::UsdShadeInput &input,
                      const char *input_name,
                      Graph *graph,
                      Node *open_pbr,
                      std::unordered_map<string, string> *emitted_float_shaders,
                      std::unordered_map<string, string> *emitted_color4_shaders,
                      string *error_message)
{
  Link source;
  std::unordered_set<string> active_shaders;
  if (!read_color_output(input,
                         graph,
                         &source,
                         &active_shaders,
                         emitted_color4_shaders,
                         0,
                         error_message,
                         emitted_float_shaders))
  {
    return false;
  }
  open_pbr->links[input_name] = source;
  return true;
}

bool read_float_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_shaders,
                       int depth,
                       string *error_message);

bool read_vector3_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         int depth,
                         string *error_message);

bool read_float_operand(const pxr::UsdShadeShader &shader,
                        const string &nodedef,
                        const char *input_name,
                        Graph *graph,
                        Node *node,
                        std::unordered_set<string> *active_shaders,
                        std::unordered_map<string, string> *emitted_shaders,
                        std::unordered_map<string, string> *emitted_color4_shaders,
                        const int depth,
                        string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
    set_error(error_message, nodedef + " requires float input '" + input_name + "'");
    return false;
  }

  if (!input.HasConnectedSource()) {
    float value;
    if (!input.Get(&value) || !std::isfinite(value)) {
      set_error(error_message,
                nodedef + " requires finite literal or connected float input '" + input_name + "'");
      return false;
    }
    node->inputs[input_name] = value;
    return true;
  }

  pxr::UsdShadeShader source;
  if (!connected_shader(input, nullptr, &source, error_message)) {
    return false;
  }
  pxr::TfToken source_id;
  source.GetShaderId(&source_id);
  if (source_id.GetString() == constant_float_id) {
    const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken("value"));
    float value;
    if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
    {
      set_error(error_message, "ND_constant_float requires a finite literal float 'value' input");
      return false;
    }
    node->inputs[input_name] = value;
    return true;
  }

  Link source_link;
  if (!read_float_output(
          input,
          graph,
          &source_link,
          active_shaders,
          emitted_shaders,
          emitted_color4_shaders,
          depth,
          error_message))
  {
    return false;
  }
  node->links[input_name] = source_link;
  return true;
}

bool read_vector2_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         const int depth,
                         string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX vector2 graph nesting exceeds maximum depth");
    return false;
  }
  pxr::UsdShadeShader source;
  if (!connected_shader_eliding_value_dot(input, &source, error_message)) return false;
  const string path = source.GetPath().GetString();
  if (!active_shaders->insert(path).second) {
    set_error(error_message, "MaterialX vector2 graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) { active_shaders->erase(path); return success; };
  pxr::TfToken id;
  source.GetShaderId(&id);
  const string nodedef = id.GetString();
  Node node;
  node.name = unique_node_name(*graph, source.GetPrim().GetName().GetString(), path);
  node.nodedef = nodedef;
  node.outputs["out"] = Type::Vector2;
  if (nodedef == blur_vector2_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &input_link,
                             active_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = input_link;
    node.inputs["size"] = 0.0f;
    node.string_inputs["filtertype"] = "box";
  }
  else
  if (is_native_noise_or_fractal_family(nodedef) && native_noise_or_fractal_is_vector2(nodedef)) {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
        !read_native_noise_or_fractal_parameters(source, &node, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["position"] = position;
    }
    else {
      Link texcoord;
      if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               active_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["texcoord"] = texcoord;
    }
  }
  else if (is_vector2_conditional(nodedef)) {
    for (const char *name : {"value1", "value2"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        Link link;
        if (!read_float_output(operand, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[name] = link;
      }
      else if (!operand.Get(&node.inputs[name]) || !std::isfinite(node.inputs[name])) {
        set_error(error_message, nodedef + " requires literal finite or connected float input '" + name + "'");
        return finish(false);
      }
    }
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
        set_error(error_message, nodedef + " requires vector2 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_vector2_output(operand, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        pxr::GfVec2f value;
        if (!operand.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message, nodedef + " requires literal finite or connected vector2 input '" + name + "'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
  }
  else if (nodedef == mix_vector2_id || nodedef == mix_vector2_vector2_id) {
    const bool vector_factor = nodedef == mix_vector2_vector2_id;
    for (const char *name : {"bg", "fg"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
        set_error(error_message, nodedef + " requires vector2 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_vector2_output(operand, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        pxr::GfVec2f value;
        if (!operand.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message, nodedef + " requires literal finite or connected vector2 input '" + name + "'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
    const pxr::UsdShadeInput factor = source.GetInput(pxr::TfToken("mix"));
    if (!factor || factor.GetTypeName() != (vector_factor ? pxr::SdfValueTypeNames->Float2 :
                                                           pxr::SdfValueTypeNames->Float))
    {
      set_error(error_message,
                nodedef + " requires " + string(vector_factor ? "vector2" : "float") +
                    " input 'mix'");
      return finish(false);
    }
    if (factor.HasConnectedSource()) {
      Link link;
      if (vector_factor) {
        if (!read_vector2_output(factor, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
      }
      else {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(factor, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) {
          return finish(false);
        }
      }
      node.links["mix"] = link;
    }
    else if (vector_factor) {
      pxr::GfVec2f value;
      if (!factor.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal finite or connected vector2 input 'mix'");
        return finish(false);
      }
      node.vector2_inputs["mix"] = make_float2(value[0], value[1]);
    }
    else if (!factor.Get(&node.inputs["mix"]) || !std::isfinite(node.inputs["mix"])) {
      set_error(error_message, nodedef + " requires literal finite or connected float input 'mix'");
      return finish(false);
    }
  }
  else if (nodedef == image_vector2_id) {
    const pxr::UsdShadeInput file_input = source.GetInput(pxr::TfToken("file"));
    pxr::SdfAssetPath asset_path;
    if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
        file_input.HasConnectedSource() || !file_input.Get(&asset_path))
    {
      set_error(error_message, "ND_image_vector2 requires a literal asset 'file' input");
      return finish(false);
    }
    string file_path = asset_path.GetResolvedPath();
    if (file_path.empty()) {
      file_path = asset_path.GetAssetPath();
    }
    if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
        path_file_size(file_path) == 0)
    {
      set_error(error_message, "ND_image_vector2 file asset is unavailable or invalid");
      return finish(false);
    }
    Link texcoord;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             active_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.asset_inputs["file"] = file_path;
    node.links["texcoord"] = texcoord;
  }
  else if (nodedef == geompropvalue_vector2_id) {
    const pxr::UsdShadeInput geomprop = source.GetInput(pxr::TfToken("geomprop"));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty()) {
      set_error(error_message, "ND_geompropvalue_vector2 requires a literal string 'geomprop' input");
      return finish(false);
    }
    node.string_inputs["geomprop"] = value;
  }
  else if (nodedef == constant_vector2_id) {
    const pxr::UsdShadeInput value = source.GetInput(pxr::TfToken("value"));
    pxr::GfVec2f vector;
    if (!value || value.GetTypeName() != pxr::SdfValueTypeNames->Float2 || value.HasConnectedSource() ||
        !value.Get(&vector)) {
      set_error(error_message, "ND_constant_vector2 requires literal vector2 'value'");
      return finish(false);
    }
    node.vector2_inputs["value"] = make_float2(vector[0], vector[1]);
  }
  else if (nodedef == convert_vector3_vector2_id || nodedef == convert_vector4_vector2_id ||
           nodedef == convert_color4_vector2_id)
  {
    Link value;
    if (nodedef == convert_color4_vector2_id) {
      std::unordered_set<string> active_color4_shaders;
      std::unordered_map<string, string> emitted_color4_shaders;
      if (!read_color4_output(source.GetInput(pxr::TfToken("in")),
                              graph,
                              &value,
                              &active_color4_shaders,
                              &emitted_color4_shaders,
                              depth + 1,
                              error_message)) {
        return finish(false);
      }
    }
    else if (nodedef == convert_vector4_vector2_id) {
      std::unordered_set<string> active_vector4_shaders;
      std::unordered_map<string, string> emitted_vector4_shaders;
      if (!read_vector4_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_vector4_shaders,
                               &emitted_vector4_shaders,
                               depth + 1,
                               error_message)) {
        return finish(false);
      }
    }
    else {
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source.GetInput(pxr::TfToken("in")), graph, &value,
                               &active_vector3_shaders, depth + 1, error_message)) {
        return finish(false);
      }
    }
    node.links["in"] = value;
  }
  else if (nodedef == convert_float_vector2_id || nodedef == convert_boolean_vector2_id ||
           nodedef == convert_integer_vector2_id)
  {
    Link value;
    if (nodedef == convert_float_vector2_id) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      if (!read_float_output(source.GetInput(pxr::TfToken("in")), graph, &value, &active_float_shaders,
                             &emitted_float_shaders, depth + 1, error_message)) return finish(false);
    }
    else if (nodedef == convert_boolean_vector2_id) {
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message)) return finish(false);
    }
    else {
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message)) return finish(false);
    }
    node.links["in"] = value;
  }
  else if (nodedef == convert_color3_vector2_id) {
    Link value; std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source.GetInput(pxr::TfToken("in")), graph, &value, &active_color_shaders,
                           depth + 1, error_message)) return finish(false);
    node.links["in"] = value;
  }
  else if (nodedef == place2d_vector2_id) {
    const pxr::UsdShadeInput texcoord = source.GetInput(pxr::TfToken("texcoord"));
    if (!texcoord || texcoord.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
        !texcoord.HasConnectedSource()) {
      set_error(error_message, "ND_place2d_vector2 requires connected vector2 input 'texcoord'");
      return finish(false);
    }
    Link link;
    if (!read_vector2_output(texcoord, graph, &link, active_shaders, depth + 1, error_message)) {
      return finish(false);
    }
    node.links["texcoord"] = link;
    for (const char *name : {"pivot", "scale", "offset"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(name));
      pxr::GfVec2f value;
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
          input.HasConnectedSource() || !input.Get(&value) || !std::isfinite(value[0]) ||
          !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal finite vector2 input '" + name + "'");
        return finish(false);
      }
      node.vector2_inputs[name] = make_float2(value[0], value[1]);
    }
    if (node.vector2_inputs["scale"].x == 0.0f || node.vector2_inputs["scale"].y == 0.0f) {
      set_error(error_message, "ND_place2d_vector2 requires nonzero scale");
      return finish(false);
    }
    for (const char *name : {"rotate", "operationorder"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(name));
      float value;
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          input.HasConnectedSource() || !input.Get(&value) || !std::isfinite(value)) {
        set_error(error_message, nodedef + " requires literal finite float input '" + name + "'");
        return finish(false);
      }
      node.inputs[name] = value;
    }
  }
  else if (nodedef == rotate2d_vector2_id) {
    if (!shader_has_exact_signature(source, {"in", "amount"}, {"out"}, error_message) ||
        source.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float2)
    {
      return finish(false);
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (input) {
      if (input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
        set_error(error_message, "ND_rotate2d_vector2 requires vector2 input 'in'");
        return finish(false);
      }
      if (input.HasConnectedSource()) {
        Link link;
        if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links["in"] = link;
      }
      else {
        pxr::GfVec2f value;
        if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message,
                    "ND_rotate2d_vector2 requires literal finite or connected vector2 input 'in'");
          return finish(false);
        }
        node.vector2_inputs["in"] = make_float2(value[0], value[1]);
      }
    }
    const pxr::UsdShadeInput amount = source.GetInput(pxr::TfToken("amount"));
    if (amount) {
      if (amount.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_rotate2d_vector2 requires float input 'amount'");
        return finish(false);
      }
      if (amount.HasConnectedSource()) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        Link link;
        if (!read_float_output(amount,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        node.links["amount"] = link;
      }
      else if (!amount.Get(&node.inputs["amount"]) || !std::isfinite(node.inputs["amount"])) {
        set_error(error_message,
                  "ND_rotate2d_vector2 requires literal finite or connected float input 'amount'");
        return finish(false);
      }
    }
  }
  else if (nodedef == "ND_add_vector2" || nodedef == "ND_subtract_vector2" ||
           nodedef == "ND_multiply_vector2" || nodedef == "ND_divide_vector2" ||
           nodedef == "ND_min_vector2" || nodedef == "ND_max_vector2" ||
           nodedef == "ND_modulo_vector2" || nodedef == "ND_power_vector2" ||
           nodedef == safepower_vector2_id) {
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
        set_error(error_message, nodedef + " requires vector2 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        if (nodedef == "ND_divide_vector2" && string(name) == "in2") {
          set_error(error_message,
                    "ND_divide_vector2 requires a literal finite nonzero vector2 input 'in2'");
          return finish(false);
        }
        Link link;
        if (!read_vector2_output(operand, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
        node.links[name] = link;
      }
      else {
        pxr::GfVec2f value;
        if (!operand.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message,
                    nodedef + " requires literal finite or connected vector2 input '" + name + "'");
          return finish(false);
        }
        if (nodedef == "ND_divide_vector2" && string(name) == "in2" &&
            (!std::isfinite(value[0]) || !std::isfinite(value[1]) || value[0] == 0.0f ||
             value[1] == 0.0f)) {
          set_error(error_message,
                    "ND_divide_vector2 requires a literal finite nonzero vector2 input 'in2'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
  }
  else if (nodedef == "ND_normalize_vector2" || nodedef == "ND_absval_vector2" ||
           nodedef == "ND_floor_vector2" || nodedef == "ND_ceil_vector2" ||
           nodedef == "ND_fract_vector2" || nodedef == "ND_sin_vector2" ||
           nodedef == "ND_cos_vector2" || nodedef == "ND_tan_vector2" ||
           nodedef == "ND_sign_vector2" || nodedef == "ND_acos_vector2" ||
           nodedef == "ND_asin_vector2" || nodedef == "ND_exp_vector2" ||
           nodedef == "ND_ln_vector2" || nodedef == "ND_sqrt_vector2" ||
           nodedef == "ND_round_vector2") {
    const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken("in"));
    if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, "ND_normalize_vector2 requires vector2 input 'in'");
      return finish(false);
    }
    if (operand.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(operand, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!operand.Get(&value)) {
        set_error(error_message, "ND_normalize_vector2 requires literal or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == "ND_atan2_vector2") {
    for (const char *name : {"iny", "inx"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
        set_error(error_message, nodedef + " requires vector2 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_vector2_output(operand, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        pxr::GfVec2f value;
        if (!operand.Get(&value)) {
          set_error(error_message, nodedef + " requires literal or connected vector2 input '" + name + "'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
  }
  else if (nodedef == multiply_vector2_fa_id || nodedef == add_vector2_fa_id ||
           nodedef == subtract_vector2_fa_id || nodedef == "ND_modulo_vector2FA" ||
           nodedef == "ND_power_vector2FA" || nodedef == "ND_min_vector2FA" ||
           nodedef == "ND_max_vector2FA" || nodedef == "ND_divide_vector2FA" ||
           nodedef == safepower_vector2_fa_id)
  {
    const pxr::UsdShadeInput vector = source.GetInput(pxr::TfToken("in1"));
    if (!vector || vector.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in1'");
      return finish(false);
    }
    if (vector.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(vector, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["in1"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!vector.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message,
                  nodedef + " requires literal finite or connected vector2 input 'in1'");
        return finish(false);
      }
      node.vector2_inputs["in1"] = make_float2(value[0], value[1]);
    }
    const pxr::UsdShadeInput scale = source.GetInput(pxr::TfToken("in2"));
    if (!scale || scale.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, nodedef + " requires float input 'in2'");
      return finish(false);
    }
    if (scale.HasConnectedSource()) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      Link link;
      if (!read_float_output(scale, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false);
      node.links["in2"] = link;
    }
    else if (!scale.Get(&node.inputs["in2"]) || !std::isfinite(node.inputs["in2"]) ||
             (nodedef == "ND_divide_vector2FA" && node.inputs["in2"] == 0.0f)) {
      set_error(error_message, nodedef + " requires literal or connected float input 'in2'");
      return finish(false);
    }
  }
  else if (nodedef == invert_vector2_id || nodedef == invert_vector2_fa_id) {
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
    const bool scalar_amount = nodedef == invert_vector2_fa_id;
    const pxr::UsdShadeInput amount = source.GetInput(pxr::TfToken("amount"));
    if (!amount || amount.GetTypeName() != (scalar_amount ? pxr::SdfValueTypeNames->Float : pxr::SdfValueTypeNames->Float2)) {
      set_error(error_message, nodedef + " requires " + string(scalar_amount ? "float" : "vector2") + " input 'amount'");
      return finish(false);
    }
    if (amount.HasConnectedSource()) {
      Link link;
      if (scalar_amount) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(amount, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false);
      }
      else if (!read_vector2_output(amount, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["amount"] = link;
    }
    else if (scalar_amount) {
      if (!amount.Get(&node.inputs["amount"]) || !std::isfinite(node.inputs["amount"])) {
        set_error(error_message, nodedef + " requires literal or connected float input 'amount'");
        return finish(false);
      }
    }
    else {
      pxr::GfVec2f value;
      if (!amount.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal or connected vector2 input 'amount'");
        return finish(false);
      }
      node.vector2_inputs["amount"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == smoothstep_vector2_id || nodedef == smoothstep_vector2_fa_id) {
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
    const bool scalar_edges = nodedef == smoothstep_vector2_fa_id;
    for (const char *name : {"low", "high"}) {
      const pxr::UsdShadeInput edge = source.GetInput(pxr::TfToken(name));
      if (!edge || edge.HasConnectedSource() || edge.GetTypeName() != (scalar_edges ? pxr::SdfValueTypeNames->Float : pxr::SdfValueTypeNames->Float2)) {
        set_error(error_message, nodedef + " requires literal " + string(scalar_edges ? "float" : "vector2") + " input '" + name + "'");
        return finish(false);
      }
      if (scalar_edges) {
        if (!edge.Get(&node.inputs[name]) || !std::isfinite(node.inputs[name])) {
          set_error(error_message, nodedef + " requires finite float input '" + name + "'");
          return finish(false);
        }
      }
      else {
        pxr::GfVec2f value;
        if (!edge.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message, nodedef + " requires finite vector2 input '" + name + "'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
    if (scalar_edges ? node.inputs["low"] >= node.inputs["high"] :
                       node.vector2_inputs["low"].x >= node.vector2_inputs["high"].x || node.vector2_inputs["low"].y >= node.vector2_inputs["high"].y) {
      set_error(error_message, nodedef + " requires low < high per component");
      return finish(false);
    }
  }
  else if (nodedef == clamp_vector2fa_id) {
    for (const char *name : {"low", "high"}) {
      const pxr::UsdShadeInput bound = source.GetInput(pxr::TfToken(name));
      if (!bound || bound.HasConnectedSource() || bound.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          !bound.Get(&node.inputs[name]) || !std::isfinite(node.inputs[name])) {
        set_error(error_message, "ND_clamp_vector2FA requires literal finite float bounds");
        return finish(false);
      }
    }
    if (node.inputs.at("low") > node.inputs.at("high")) {
      set_error(error_message, "ND_clamp_vector2FA requires low <= high");
      return finish(false);
    }
    const pxr::UsdShadeInput value = source.GetInput(pxr::TfToken("in"));
    if (!value || value.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, "ND_clamp_vector2FA requires vector2 input 'in'");
      return finish(false);
    }
    if (value.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(value, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f literal;
      if (!value.Get(&literal) || !std::isfinite(literal[0]) || !std::isfinite(literal[1])) {
        set_error(error_message, "ND_clamp_vector2FA requires literal finite or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(literal[0], literal[1]);
    }
  }
  else if (nodedef == combine2_vector2_id) {
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_combine2_vector2 requires float inputs 'in1' and 'in2'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_float_output(operand, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false);
        node.links[name] = link;
      }
      else if (!operand.Get(&node.inputs[name])) {
        set_error(error_message, string("ND_combine2_vector2 requires literal or connected float input '") + name + "'");
        return finish(false);
      }
    }
  }
  else if (is_contrast_vector2(nodedef)) {
    const bool scalar_parameters = contrast_uses_scalar_parameters(nodedef);
    for (const char *name : {"pivot", "amount"}) {
      const pxr::UsdShadeInput parameter = source.GetInput(pxr::TfToken(name));
      if (!parameter || parameter.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal parameter inputs");
        return finish(false);
      }
      if (scalar_parameters) {
        float value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Float || !parameter.Get(&value) ||
            !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires literal finite float input '" + name + "'");
          return finish(false);
        }
        node.inputs[name] = value;
      }
      else {
        pxr::GfVec2f value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Float2 || !parameter.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1])) {
          set_error(error_message, nodedef + " requires literal finite vector2 input '" + name + "'");
          return finish(false);
        }
        node.vector2_inputs[name] = make_float2(value[0], value[1]);
      }
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal finite vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == remap_vector2_id || nodedef == range_vector2_id) {
    for (const char *input_name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(input_name));
      pxr::GfVec2f value;
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
          input.HasConnectedSource() || !input.Get(&value) || !std::isfinite(value[0]) ||
          !std::isfinite(value[1]))
      {
        set_error(error_message,
                  nodedef + " requires literal finite vector2 input '" + input_name + "'");
        return finish(false);
      }
      node.vector2_inputs[input_name] = make_float2(value[0], value[1]);
    }
    const float2 &inlow = node.vector2_inputs.at("inlow");
    const float2 &inhigh = node.vector2_inputs.at("inhigh");
    if (inlow.x == inhigh.x || inlow.y == inhigh.y) {
      set_error(error_message, nodedef + " requires inlow != inhigh in every component");
      return finish(false);
    }
    if (nodedef == range_vector2_id) {
      const pxr::UsdShadeInput gamma_input = source.GetInput(pxr::TfToken("gamma"));
      pxr::GfVec2f gamma;
      if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
          gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) ||
          gamma[0] != 1.0f || gamma[1] != 1.0f)
      {
        set_error(error_message, "ND_range_vector2 requires literal gamma (1, 1)");
        return finish(false);
      }
      const pxr::UsdShadeInput clamp_input = source.GetInput(pxr::TfToken("doclamp"));
      bool do_clamp;
      if (!clamp_input || clamp_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
          clamp_input.HasConnectedSource() || !clamp_input.Get(&do_clamp))
      {
        set_error(error_message, "ND_range_vector2 requires literal boolean 'doclamp'");
        return finish(false);
      }
      const float2 &outlow = node.vector2_inputs.at("outlow");
      const float2 &outhigh = node.vector2_inputs.at("outhigh");
      if (do_clamp && (outlow.x > outhigh.x || outlow.y > outhigh.y)) {
        set_error(error_message,
                  "ND_range_vector2 requires outlow <= outhigh in every component when clamped");
        return finish(false);
      }
      node.int_inputs["doclamp"] = do_clamp ? 1 : 0;
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal finite or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == remap_vector2fa_id) {
    for (const char *name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput bound = source.GetInput(pxr::TfToken(name));
      float value;
      if (!bound || bound.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          bound.HasConnectedSource() || !bound.Get(&value) || !std::isfinite(value)) {
        set_error(error_message, nodedef + " requires literal finite float input '" + name + "'");
        return finish(false);
      }
      node.inputs[name] = value;
    }
    if (node.inputs.at("inlow") == node.inputs.at("inhigh")) {
      set_error(error_message, nodedef + " requires inlow != inhigh");
      return finish(false);
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, nodedef + " requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, nodedef + " requires literal finite vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == clamp_vector2_id) {
    for (const char *input_name : {"low", "high"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(input_name));
      pxr::GfVec2f value;
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2 ||
          input.HasConnectedSource() || !input.Get(&value) || !std::isfinite(value[0]) ||
          !std::isfinite(value[1]))
      {
        set_error(error_message,
                  string("ND_clamp_vector2 requires literal finite vector2 input '") + input_name + "'");
        return finish(false);
      }
      node.vector2_inputs[input_name] = make_float2(value[0], value[1]);
    }
    const float2 &low = node.vector2_inputs.at("low");
    const float2 &high = node.vector2_inputs.at("high");
    if (low.x > high.x || low.y > high.y) {
      set_error(error_message, "ND_clamp_vector2 requires low <= high in every component");
      return finish(false);
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, "ND_clamp_vector2 requires vector2 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message, "ND_clamp_vector2 requires literal finite or connected vector2 input 'in'");
        return finish(false);
      }
      node.vector2_inputs["in"] = make_float2(value[0], value[1]);
    }
  }
  else if (nodedef == texcoord_vector2_id) {
    int index = 0;
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (index_input) {
      if (index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
          index_input.HasConnectedSource() || !index_input.Get(&index) || index < 0)
      {
        set_error(error_message, "ND_texcoord_vector2 'index' must be a literal non-negative integer");
        return finish(false);
      }
    }
    /* Reuses the existing ND_geompropvalue_vector2 Cycles lowering (UVMapNode)
     * -- see the geomcolor_float_id case in read_float_output() for the same
     * index-to-primvar-name reuse pattern. */
    node.nodedef = geompropvalue_vector2_id;
    node.string_inputs["geomprop"] = texcoord_attribute_name(index);
  }
  else if (nodedef == usdprimvarreader_vector2_id) {
    const pxr::UsdShadeInput varname = source.GetInput(pxr::TfToken("varname"));
    string value;
    if (!varname || varname.GetTypeName() != pxr::SdfValueTypeNames->String ||
        varname.HasConnectedSource() || !varname.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_UsdPrimvarReader_vector2 requires a literal string 'varname' input");
      return finish(false);
    }
    node.string_inputs["varname"] = value;
  }
  else if (nodedef == roughness_anisotropy_id || nodedef == glossiness_anisotropy_id) {
    /* See roughness_anisotropy_id's declaration comment above for the real
     * nodedef/reference-implementation citation. */
    const char *first_name = nodedef == glossiness_anisotropy_id ? "glossiness" : "roughness";
    for (const char *name : {first_name, "anisotropy"}) {
      const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken(name));
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, nodedef + " requires float input '" + string(name) + "'");
        return finish(false);
      }
      if (value_input.HasConnectedSource()) {
        Link link;
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(value_input,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        float value;
        if (!value_input.Get(&value) || !std::isfinite(value)) {
          set_error(error_message,
                    nodedef + " requires literal finite or connected float input '" +
                        string(name) + "'");
          return finish(false);
        }
        node.inputs[name] = value;
      }
    }
  }
  else if (nodedef == open_pbr_anisotropy_id) {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, "ND_open_pbr_anisotropy requires vector2 output 'out'");
      return finish(false);
    }
    for (const char *name : {"roughness", "anisotropy"}) {
      const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken(name));
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_open_pbr_anisotropy requires float input '" + string(name) + "'");
        return finish(false);
      }
      if (value_input.HasConnectedSource()) {
        Link link;
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(value_input,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        float value;
        if (!value_input.Get(&value) || !std::isfinite(value)) {
          set_error(error_message,
                    "ND_open_pbr_anisotropy requires literal finite or connected float input '" +
                        string(name) + "'");
          return finish(false);
        }
        node.inputs[name] = value;
      }
    }
  }
  else if (nodedef == roughness_dual_id) {
    /* See roughness_dual_id's declaration comment above for the real
     * nodedef/reference-implementation citation. */
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("roughness"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, "ND_roughness_dual requires vector2 input 'roughness'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector2_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["roughness"] = link;
    }
    else {
      pxr::GfVec2f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
        set_error(error_message,
                  "ND_roughness_dual requires literal finite or connected vector2 input 'roughness'");
        return finish(false);
      }
      node.vector2_inputs["roughness"] = make_float2(value[0], value[1]);
    }
  }
  else {
    set_error(error_message, string("MaterialX texcoord requires a supported vector2 node, got ") + nodedef);
    return finish(false);
  }
  *result = {node.name, "out", Type::Vector2};
  graph->nodes.push_back(std::move(node));
  return finish(true);
}

bool read_float_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_shaders,
                       std::unordered_map<string, string> *emitted_color4_shaders,
                       const int depth,
                       string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX float graph nesting exceeds maximum depth");
    return false;
  }

  pxr::UsdShadeShader source;
  if (!connected_shader_eliding_value_dot(input, &source, error_message)) {
    return false;
  }
  const string shader_path = source.GetPath().GetString();
  const auto sources = input.GetConnectedSources();
  const string source_output = sources.size() == 1 ? sources[0].sourceName.GetString() : "out";
  const string emitted_key = shader_path + "." + source_output;
  const auto emitted = emitted_shaders->find(emitted_key);
  if (emitted != emitted_shaders->end()) {
    *result = {emitted->second, source_output, Type::Float};
    return true;
  }
  if (!active_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX float graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken source_id;
  source.GetShaderId(&source_id);
  const string nodedef = source_id.GetString();
  Node node;
  node.name = unique_node_name(*graph, source.GetPrim().GetName().GetString(), shader_path);
  node.nodedef = nodedef;

  if (nodedef == blur_float_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_float_output(source.GetInput(pxr::TfToken("in")),
                           graph,
                           &input_link,
                           active_shaders,
                           emitted_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message))
    {
      return finish(false);
    }
    node.links["in"] = input_link;
    node.inputs["size"] = 0.0f;
    node.string_inputs["filtertype"] = "box";
    node.outputs["out"] = Type::Float;
    *result = {node.name, "out", Type::Float};
    emitted_shaders->emplace(emitted_key, node.name);
    graph->nodes.push_back(std::move(node));
    return finish(true);
  }

  if (nodedef == heighttonormal_vector3_id) {
    set_error(error_message,
              "ND_heighttonormal_vector3 requires derivative/Sobel texture sampling not available "
              "in this MaterialX-to-Cycles lowering path");
    return finish(false);
  }

  if (nodedef == separate4_vector4_id) {
    if (source_output != "outx" && source_output != "outy" && source_output != "outz" &&
        source_output != "outw")
    {
      set_error(error_message, "ND_separate4_vector4 requires outx, outy, outz, or outw output");
      return finish(false);
    }
    for (const char *output_name : {"outx", "outy", "outz", "outw"}) {
      const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken(output_name));
      if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_separate4_vector4 requires float outputs outx/outy/outz/outw");
        return finish(false);
      }
    }
    Link vector;
    std::unordered_set<string> active_vector4_shaders;
    std::unordered_map<string, string> emitted_vector4_shaders;
    if (!read_vector4_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector,
                             &active_vector4_shaders,
                             &emitted_vector4_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = vector;
    node.outputs["outx"] = Type::Float;
    node.outputs["outy"] = Type::Float;
    node.outputs["outz"] = Type::Float;
    node.outputs["outw"] = Type::Float;
    *result = {node.name, source_output, Type::Float};
    emitted_shaders->emplace(emitted_key, node.name);
    graph->nodes.push_back(std::move(node));
    return finish(true);
  }

  if (nodedef == separate3_vector3_id) {
    if (source_output != "outx" && source_output != "outy" && source_output != "outz") {
      set_error(error_message, "ND_separate3_vector3 requires outx, outy, or outz output");
      return finish(false);
    }
    Link input_link;
    std::unordered_set<string> active_vector3_shaders;
    if (!read_vector3_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &input_link,
                             &active_vector3_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = input_link;
    node.outputs["outx"] = Type::Float;
    node.outputs["outy"] = Type::Float;
    node.outputs["outz"] = Type::Float;
    *result = {node.name, source_output, Type::Float};
    emitted_shaders->emplace(emitted_key, node.name);
    graph->nodes.push_back(std::move(node));
    return finish(true);
  }
  if (nodedef == separate3_color3_id) {
    if (source_output != "outx" && source_output != "outy" && source_output != "outz") return finish(false);
    Link color;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source.GetInput(pxr::TfToken("in")), graph, &color, &active_color_shaders,
                           emitted_color4_shaders, depth + 1, error_message)) return finish(false);
    node.links["in"] = color;
    node.outputs = {{"outx", Type::Float}, {"outy", Type::Float}, {"outz", Type::Float}};
    *result = {node.name, source_output, Type::Float};
    emitted_shaders->emplace(emitted_key, node.name);
    graph->nodes.push_back(std::move(node));
    return finish(true);
  }
  if (nodedef == separate4_color4_id) {
    if (source_output != "outr" && source_output != "outg" && source_output != "outb" &&
        source_output != "outa")
    {
      set_error(error_message, "ND_separate4_color4 requires outr, outg, outb, or outa output");
      return finish(false);
    }
    if (!emitted_color4_shaders) {
      set_error(error_message, "ND_separate4_color4 requires Color4 emission state");
      return finish(false);
    }
    Link color;
    std::unordered_set<string> active_color_shaders;
    if (!read_color4_output(source.GetInput(pxr::TfToken("in")),
                            graph,
                            &color,
                            &active_color_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    node.links["in"] = color;
    node.outputs = {{"outr", Type::Float},
                    {"outg", Type::Float},
                    {"outb", Type::Float},
                    {"outa", Type::Float}};
    *result = {node.name, source_output, Type::Float};
    emitted_shaders->emplace(emitted_key, node.name);
    graph->nodes.push_back(std::move(node));
    return finish(true);
  }
  if (is_native_noise_or_fractal_family(nodedef) && native_noise_or_fractal_is_float(nodedef)) {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        !read_native_noise_or_fractal_parameters(source, &node, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      std::unordered_set<string> active_vector_shaders;
      if (!read_vector3_output(source.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               &active_vector_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["position"] = position;
    }
    else {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["texcoord"] = texcoord;
    }
  }
  else if (nodedef == geompropvalue_float_id) {
    const pxr::UsdShadeInput geomprop = source.GetInput(pxr::TfToken("geomprop"));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_geompropvalue_float requires a literal string 'geomprop' input");
      return finish(false);
    }
    node.string_inputs["geomprop"] = value;
  }
  else if (nodedef == geomcolor_float_id) {
    int index = 0;
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (index_input) {
      if (index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
          index_input.HasConnectedSource() || !index_input.Get(&index) || index < 0)
      {
        set_error(error_message, "ND_geomcolor_float 'index' must be a literal non-negative integer");
        return finish(false);
      }
    }
    /* Reuses the existing ND_geompropvalue_float Cycles lowering -- see the
     * geomcolor_color3_id case in read_color_output() for the same
     * index-to-geomprop-name mapping. */
    node.nodedef = geompropvalue_float_id;
    node.string_inputs["geomprop"] = geomcolor_attribute_name(index);
  }
  else if (nodedef == image_float_id) {
    const pxr::UsdShadeInput file_input = source.GetInput(pxr::TfToken("file"));
    pxr::SdfAssetPath asset_path;
    if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
        file_input.HasConnectedSource() || !file_input.Get(&asset_path))
    {
      set_error(error_message, "ND_image_float requires a literal asset 'file' input");
      return finish(false);
    }
    string file_path = asset_path.GetResolvedPath();
    if (file_path.empty()) {
      file_path = asset_path.GetAssetPath();
    }
    if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
        path_file_size(file_path) == 0)
    {
      set_error(error_message, "ND_image_float file asset is unavailable or invalid");
      return finish(false);
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.asset_inputs["file"] = file_path;
    node.links["texcoord"] = texcoord;
  }
  else if (nodedef == extract_color3_id) {
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (!index_input || index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        index_input.HasConnectedSource() || !index_input.Get(&node.int_inputs["index"]) ||
        node.int_inputs["index"] < 0 || node.int_inputs["index"] > 2)
    {
      set_error(error_message, "ND_extract_color3 'index' must be a literal 0, 1, or 2");
      return finish(false);
    }
    Link color_source;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source.GetInput(pxr::TfToken("in")),
                           graph,
                           &color_source,
                           &active_color_shaders,
                           emitted_color4_shaders,
                           0,
                           error_message))
    {
      return finish(false);
    }
    node.links["in"] = color_source;
  }
  else if (nodedef == extract_color4_id) {
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (!index_input || index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        index_input.HasConnectedSource() || !index_input.Get(&node.int_inputs["index"]) ||
        node.int_inputs["index"] < 0 || node.int_inputs["index"] > 3)
    {
      set_error(error_message, "ND_extract_color4 'index' must be a literal 0, 1, 2, or 3");
      return finish(false);
    }
    Link color4_source;
    std::unordered_set<string> active_color4_shaders;
    if (!read_color4_output(source.GetInput(pxr::TfToken("in")),
                            graph,
                            &color4_source,
                            &active_color4_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    node.links["in"] = color4_source;
  }
  else if (nodedef == extract_vector3_id) {
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (!index_input || index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        index_input.HasConnectedSource() || !index_input.Get(&node.int_inputs["index"]) ||
        node.int_inputs["index"] < 0 || node.int_inputs["index"] > 2)
    {
      set_error(error_message, "ND_extract_vector3 'index' must be a literal 0, 1, or 2");
      return finish(false);
    }
    Link vector_source;
    std::unordered_set<string> active_vector_shaders;
    if (!read_vector3_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector_source,
                             &active_vector_shaders,
                             0,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = vector_source;
  }
  else if (nodedef == extract_vector2_id) {
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (!index_input || index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        index_input.HasConnectedSource() || !index_input.Get(&node.int_inputs["index"]) ||
        node.int_inputs["index"] < 0 || node.int_inputs["index"] > 1)
    {
      set_error(error_message, "ND_extract_vector2 'index' must be a literal 0 or 1");
      return finish(false);
    }
    Link vector_source;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector_source,
                             &active_vector2_shaders,
                             0,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = vector_source;
  }
  else if (nodedef == extract_vector4_id) {
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (!index_input || index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
        index_input.HasConnectedSource() || !index_input.Get(&node.int_inputs["index"]) ||
        node.int_inputs["index"] < 0 || node.int_inputs["index"] > 3)
    {
      set_error(error_message, "ND_extract_vector4 'index' must be a literal 0, 1, 2, or 3");
      return finish(false);
    }
    Link vector_source;
    std::unordered_set<string> active_vector4_shaders;
    std::unordered_map<string, string> emitted_vector4_shaders;
    if (!read_vector4_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &vector_source,
                             &active_vector4_shaders,
                             &emitted_vector4_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = vector_source;
  }
  else if (nodedef == magnitude_vector3_id || nodedef == dotproduct_vector3_id ||
           nodedef == distance_vector3_id)
  {
    const char *first = nodedef == magnitude_vector3_id ? "in" : "in1";
    Link first_source;
    std::unordered_set<string> active_vector_shaders;
    if (!read_vector3_output(source.GetInput(pxr::TfToken(first)),
                             graph,
                             &first_source,
                             &active_vector_shaders,
                             0,
                             error_message))
    {
      return finish(false);
    }
    node.links[first] = first_source;
    if (nodedef == dotproduct_vector3_id || nodedef == distance_vector3_id) {
      Link second_source;
      if (!read_vector3_output(source.GetInput(pxr::TfToken("in2")),
                               graph,
                               &second_source,
                               &active_vector_shaders,
                               0,
                               error_message))
      {
        return finish(false);
      }
      node.links["in2"] = second_source;
    }
  }
  else if (nodedef == magnitude_vector2_id || nodedef == dotproduct_vector2_id ||
           nodedef == distance_vector2_id)
  {
    const char *first = nodedef == magnitude_vector2_id ? "in" : "in1";
    Link first_source;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source.GetInput(pxr::TfToken(first)),
                             graph,
                             &first_source,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links[first] = first_source;
    if (nodedef != magnitude_vector2_id) {
      Link second_source;
      if (!read_vector2_output(source.GetInput(pxr::TfToken("in2")),
                               graph,
                               &second_source,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["in2"] = second_source;
    }
  }
  else if (nodedef == constant_float_id) {
    const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken("value"));
    if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
        value_input.HasConnectedSource() || !value_input.Get(&node.inputs["value"]))
    {
      set_error(error_message, "ND_constant_float requires a literal float 'value' input");
      return finish(false);
    }
  }
  else if (nodedef == convert_boolean_float_id || nodedef == convert_integer_float_id ||
           nodedef == convert_boolean_vector2_id || nodedef == convert_integer_vector2_id ||
           nodedef == convert_boolean_vector3_id || nodedef == convert_integer_vector3_id)
  {
    Link value;
    if (nodedef == convert_boolean_float_id || nodedef == convert_boolean_vector2_id ||
        nodedef == convert_boolean_vector3_id)
    {
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    else {
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
    }
    node.links["in"] = value;
  }
  else if (nodedef == ramplr_float_id || nodedef == ramptb_float_id || is_scalar_split(nodedef)) {
    /* Ramps (and splits, which share the same lr/tb axis and valuel/valuer or
     * valuet/valueb inputs, plus a 'center' threshold) produce a float, so they
     * must be recognized on the same recursive path as other scalar producers
     * (rather than only at a top-level input). */
    const bool split = is_scalar_split(nodedef);
    const bool top_to_bottom = split ? split_is_top_to_bottom(nodedef) : nodedef == ramptb_float_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    if (split) {
      const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
      if (!output || output.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          source.GetOutputs().size() != 1)
      {
        set_error(error_message, nodedef + " requires exactly one float output 'out'");
        return finish(false);
      }
      for (const pxr::UsdShadeInput &split_input : source.GetInputs()) {
        const string split_input_name = split_input.GetBaseName().GetString();
        if (split_input_name != first_name && split_input_name != second_name &&
            split_input_name != "center" && split_input_name != "texcoord")
        {
          set_error(error_message, nodedef + " has unsupported input '" + split_input_name + "'");
          return finish(false);
        }
      }
      const pxr::UsdShadeInput center = source.GetInput(pxr::TfToken("center"));
      if (center) {
        if (center.GetTypeName() != pxr::SdfValueTypeNames->Float) {
          set_error(error_message, nodedef + " requires float input 'center'");
          return finish(false);
        }
        if (center.HasConnectedSource()) {
          Link center_link;
          if (!read_float_output(center,
                                 graph,
                                 &center_link,
                                 active_shaders,
                                 emitted_shaders,
                                 emitted_color4_shaders,
                                 depth + 1,
                                 error_message))
          {
            return finish(false);
          }
          node.links["center"] = center_link;
        }
        else if (!center.Get(&node.inputs["center"]) || !std::isfinite(node.inputs["center"])) {
          set_error(error_message, nodedef + " requires literal finite float input 'center'");
          return finish(false);
        }
      }
      else {
        node.inputs["center"] = 0.5f;
      }
    }
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken(input_name));
      float value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  nodedef + " requires literal finite float input '" + input_name + "'");
        return finish(false);
      }
      node.inputs[input_name] = value;
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["texcoord"] = texcoord;
  }
  else if (nodedef == smoothstep_float_id) {
    /* Target shader languages leave equal or reversed smoothstep edges undefined.
     * Keep the canonical reader deterministic by accepting only finite, increasing,
     * literal edges; the value itself may remain a typed nested float link. */
    for (const char *input_name : {"low", "high"}) {
      const pxr::UsdShadeInput edge = source.GetInput(pxr::TfToken(input_name));
      float value;
      if (!edge || edge.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          edge.HasConnectedSource() || !edge.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  "ND_smoothstep_float requires literal finite float input '" + string(input_name) + "'");
        return finish(false);
      }
      node.inputs[input_name] = value;
    }
    if (node.inputs["low"] >= node.inputs["high"]) {
      set_error(error_message, "ND_smoothstep_float requires low < high");
      return finish(false);
    }
    if (!read_float_operand(source,
                            nodedef,
                            "in",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (is_contrast_float(nodedef)) {
    for (const char *input_name : {"pivot", "amount"}) {
      const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken(input_name));
      float value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  nodedef + " requires literal finite float input '" + string(input_name) + "'");
        return finish(false);
      }
      node.inputs[input_name] = value;
    }
    if (!read_float_operand(source,
                            nodedef,
                            "in",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (nodedef == remap_float_id || nodedef == range_float_id) {
    /* The scalar Map Range node has the same linear formula as remap.  Its zero-width
     * input range deliberately produces zero, whereas MaterialX remap divides by zero;
     * reject that undefined source configuration instead of changing its result. */
    for (const char *input_name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput value_input = source.GetInput(pxr::TfToken(input_name));
      float value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  nodedef + " requires literal finite float input '" + string(input_name) + "'");
        return finish(false);
      }
      node.inputs[input_name] = value;
    }
    if (node.inputs["inlow"] == node.inputs["inhigh"]) {
      set_error(error_message, nodedef + " requires inlow != inhigh");
      return finish(false);
    }
    if (nodedef == range_float_id) {
      const pxr::UsdShadeInput gamma_input = source.GetInput(pxr::TfToken("gamma"));
      float gamma;
      if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) || gamma != 1.0f)
      {
        set_error(error_message, "ND_range_float requires literal gamma 1.0");
        return finish(false);
      }
      const pxr::UsdShadeInput clamp_input = source.GetInput(pxr::TfToken("doclamp"));
      bool do_clamp;
      if (!clamp_input || clamp_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
          clamp_input.HasConnectedSource() || !clamp_input.Get(&do_clamp))
      {
        set_error(error_message, "ND_range_float requires literal boolean 'doclamp'");
        return finish(false);
      }
      if (do_clamp && node.inputs["outlow"] > node.inputs["outhigh"]) {
        set_error(error_message, "ND_range_float requires outlow <= outhigh when doclamp is true");
        return finish(false);
      }
      node.int_inputs["doclamp"] = do_clamp ? 1 : 0;
    }
    if (!read_float_operand(source,
                            nodedef,
                            "in",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (nodedef == luminance_color3_id) {
    const pxr::UsdShadeInput coefficients_input = source.GetInput(pxr::TfToken("lumacoeffs"));
    pxr::GfVec3f coefficients;
    if (!coefficients_input || coefficients_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
        coefficients_input.HasConnectedSource() || !coefficients_input.Get(&coefficients) ||
        !std::isfinite(coefficients[0]) || !std::isfinite(coefficients[1]) ||
        !std::isfinite(coefficients[2]) || !coefficients_input.GetAttr().GetColorSpace().IsEmpty())
    {
      set_error(error_message,
                "ND_luminance_color3 requires literal finite color-space-independent color3 'lumacoeffs'");
      return finish(false);
    }
    const pxr::UsdShadeInput color_input = source.GetInput(pxr::TfToken("in"));
    if (!color_input || color_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
        !color_input.GetAttr().GetColorSpace().IsEmpty())
    {
      set_error(error_message,
                "ND_luminance_color3 requires color-space-independent color3 input 'in'");
      return finish(false);
    }
    Link color;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(color_input,
                           graph,
                           &color,
                           &active_color_shaders,
                           emitted_color4_shaders,
                           depth + 1,
                           error_message)) {
      return finish(false);
    }
    node.color3_inputs["lumacoeffs"] = make_float3(coefficients[0], coefficients[1], coefficients[2]);
    node.links["in"] = color;
  }
  else if (nodedef == mix_float_id || nodedef == plus_float_id || nodedef == minus_float_id ||
           nodedef == difference_float_id || nodedef == burn_float_id || nodedef == dodge_float_id ||
           nodedef == screen_float_id || nodedef == overlay_float_id) {
    if (!read_float_operand(source,
                            nodedef,
                            "bg",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        !read_float_operand(source,
                            nodedef,
                            "fg",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        !read_float_operand(source,
                            nodedef,
                            "mix",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (is_inside_outside_float(nodedef)) {
    for (const char *input_name : {"in", "mask"}) {
      if (!read_float_operand(source,
                              nodedef,
                              input_name,
                              graph,
                              &node,
                              active_shaders,
                              emitted_shaders,
                              emitted_color4_shaders,
                              depth + 1,
                              error_message))
      {
        return finish(false);
      }
    }
  }
  else if (is_float_conditional(nodedef)) {
    for (const char *input_name : {"value1", "value2", "in1", "in2"}) {
      if (!read_float_operand(source,
                              nodedef,
                              input_name,
                              graph,
                              &node,
                              active_shaders,
                              emitted_shaders,
                              emitted_color4_shaders,
                              depth + 1,
                              error_message))
      {
        return finish(false);
      }
    }
  }
  else if (nodedef == safepower_float_id) {
    if (!read_float_operand(source,
                            nodedef,
                            "in1",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        !read_float_operand(source,
                            nodedef,
                            "in2",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (is_scalar_math(nodedef) || nodedef == invert_float_id) {
    const bool is_unary = nodedef == absval_float_id || nodedef == floor_float_id ||
                          nodedef == ceil_float_id || nodedef == round_float_id ||
                          nodedef == sqrt_float_id || nodedef == fract_float_id ||
                          nodedef == sign_float_id || nodedef == sin_float_id ||
                          nodedef == cos_float_id || nodedef == tan_float_id ||
                          nodedef == exp_float_id || nodedef == acos_float_id ||
                          nodedef == asin_float_id || nodedef == ln_float_id;
    const bool is_atan2 = nodedef == atan2_float_id;
    const char *first_input = is_unary ? "in" : (nodedef == invert_float_id ? "amount" : (is_atan2 ? "iny" : "in1"));
    const char *second_input = nodedef == invert_float_id ? "in" : (is_atan2 ? "inx" : "in2");
    if (nodedef == divide_float_id) {
      const pxr::UsdShadeInput denominator = source.GetInput(pxr::TfToken("in2"));
      float value;
      if (!denominator || denominator.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          denominator.HasConnectedSource() || !denominator.Get(&value) || !std::isfinite(value) ||
          value == 0.0f)
      {
        set_error(error_message, "ND_divide_float requires a literal finite nonzero float input 'in2'");
        return finish(false);
      }
      node.inputs["in2"] = value;
      if (!read_float_operand(source,
                              nodedef,
                              first_input,
                              graph,
                              &node,
                              active_shaders,
                              emitted_shaders,
                              emitted_color4_shaders,
                              depth + 1,
                              error_message))
      {
        return finish(false);
      }
    }
    else if (!read_float_operand(source,
                            nodedef,
                            first_input,
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        (!is_unary && !read_float_operand(source,
                            nodedef,
                            second_input,
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message)))
    {
      return finish(false);
    }
  }
  else if (nodedef == clamp_float_id) {
    if (!read_float_operand(source,
                            nodedef,
                            "in",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        !read_float_operand(source,
                            nodedef,
                            "low",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message) ||
        !read_float_operand(source,
                            nodedef,
                            "high",
                            graph,
                            &node,
                            active_shaders,
                            emitted_shaders,
                            emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
  }
  else if (const CellNoiseSpec *spec = cellnoise_spec(nodedef)) {
    const pxr::SdfValueTypeName input_type = spec->input_type == Type::Vector2 ?
                                                pxr::SdfValueTypeNames->Float2 :
                                                pxr::SdfValueTypeNames->Float3;
    if (!shader_has_exact_signature(source, {spec->input_name}, {"out"}, error_message) ||
        source.GetInput(pxr::TfToken(spec->input_name)).GetTypeName() != input_type ||
        source.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float)
    {
      set_error(error_message, nodedef + " does not match its exact MaterialX signature");
      return finish(false);
    }
    if (spec->input_type == Type::Vector2) {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source.GetInput(pxr::TfToken(spec->input_name)),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links[spec->input_name] = texcoord;
    }
    else {
      Link position;
      std::unordered_set<string> active_vector3_shaders;
      if (!read_vector3_output(source.GetInput(pxr::TfToken(spec->input_name)),
                               graph,
                               &position,
                               &active_vector3_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links[spec->input_name] = position;
    }
  }
  else if (nodedef == usdprimvarreader_float_id) {
    const pxr::UsdShadeInput varname = source.GetInput(pxr::TfToken("varname"));
    string value;
    if (!varname || varname.GetTypeName() != pxr::SdfValueTypeNames->String ||
        varname.HasConnectedSource() || !varname.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_UsdPrimvarReader_float requires a literal string 'varname' input");
      return finish(false);
    }
    node.string_inputs["varname"] = value;
  }
  else {
    set_error(error_message,
              string("MaterialX float input requires a supported float node, got ") + nodedef);
    return finish(false);
  }
  node.outputs["out"] = Type::Float;

  *result = {node.name, "out", Type::Float};
  emitted_shaders->emplace(emitted_key, node.name);
  graph->nodes.push_back(std::move(node));
  return finish(true);
}

bool read_float_output(const pxr::UsdShadeInput &input,
                       Graph *graph,
                       Link *result,
                       std::unordered_set<string> *active_shaders,
                       std::unordered_map<string, string> *emitted_shaders,
                       const int depth,
                       string *error_message)
{
  std::unordered_map<string, string> emitted_color4_shaders;
  return read_float_output(input,
                           graph,
                           result,
                           active_shaders,
                           emitted_shaders,
                           &emitted_color4_shaders,
                           depth,
                           error_message);
}

bool read_scalar_graph(const pxr::UsdShadeInput &input,
                       const char *input_name,
                       Graph *graph,
                       Node *open_pbr,
                       std::unordered_map<string, string> *emitted_shaders,
                       std::unordered_map<string, string> *emitted_color4_shaders,
                       string *error_message)
{
  Link source;
  std::unordered_set<string> active_shaders;
  if (!read_float_output(input,
                         graph,
                         &source,
                         &active_shaders,
                         emitted_shaders,
                         emitted_color4_shaders,
                         0,
                         error_message))
  {
    return false;
  }
  open_pbr->links[input_name] = source;
  return true;
}

bool read_float_terminal_input(const pxr::UsdShadeShader &surface,
                               const char *input_name,
                               Graph *graph,
                               Node *open_pbr,
                               bool *has_supported_input,
                               std::unordered_map<string, string> *emitted_shaders,
                               std::unordered_map<string, string> *emitted_color4_shaders,
                               string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
    set_error(error_message, string("OpenPBR ") + input_name + " must have float type");
    return false;
  }
  if (input.HasConnectedSource()) {
    if (!read_scalar_graph(input,
                           input_name,
                           graph,
                           open_pbr,
                           emitted_shaders,
                           emitted_color4_shaders,
                           error_message))
    {
      return false;
    }
  }
  else {
    float value;
    if (!input.Get(&value)) {
      set_error(error_message, string("OpenPBR ") + input_name + " has no float value");
      return false;
    }
    open_pbr->inputs[input_name] = value;
  }
  *has_supported_input = true;
  return true;
}

bool read_color_terminal_input(const pxr::UsdShadeShader &surface,
                               const char *input_name,
                               Graph *graph,
                               Node *open_pbr,
                               bool *has_supported_input,
                               std::unordered_map<string, string> *emitted_float_shaders,
                               std::unordered_map<string, string> *emitted_color4_shaders,
                               string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
    set_error(error_message, string("OpenPBR ") + input_name + " must have color3f type");
    return false;
  }
  if (input.HasConnectedSource()) {
    if (!read_color_graph(input,
                          input_name,
                          graph,
                          open_pbr,
                          emitted_float_shaders,
                          emitted_color4_shaders,
                          error_message))
    {
      return false;
    }
  }
  else {
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, string("OpenPBR ") + input_name + " has no color3f value");
      return false;
    }
    open_pbr->color3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
  }
  *has_supported_input = true;
  return true;
}

bool read_vector3_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         const int depth,
                         string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX vector graph nesting exceeds maximum depth");
    return false;
  }
  pxr::UsdShadeShader source;
  if (!connected_shader_eliding_value_dot(input, &source, error_message)) return false;
  const string path = source.GetPath().GetString();
  if (!active_shaders->insert(path).second) {
    set_error(error_message, "MaterialX vector graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) { active_shaders->erase(path); return success; };
  pxr::TfToken id;
  source.GetShaderId(&id);
  const string nodedef = id.GetString();
  Node node;
  node.name = unique_node_name(*graph, source.GetPrim().GetName().GetString(), path);
  node.nodedef = nodedef;
  node.outputs["out"] = Type::Vector3;
  if (nodedef == blur_vector3_id) {
    pxr::SdfValueTypeName value_type;
    if (!blur_id_type(nodedef, &value_type) ||
        !validate_degenerate_blur_shader(source, nodedef, value_type, error_message))
    {
      return finish(false);
    }
    Link input_link;
    if (!read_vector3_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &input_link,
                             active_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = input_link;
    node.inputs["size"] = 0.0f;
    node.string_inputs["filtertype"] = "box";
  }
  else if (nodedef == heighttonormal_vector3_id) {
    set_error(error_message,
              "ND_heighttonormal_vector3 requires derivative/Sobel texture sampling not available "
              "in this MaterialX-to-Cycles lowering path");
    return finish(false);
  }
  else
  if (is_native_noise_or_fractal_family(nodedef) && !native_noise_or_fractal_is_float(nodedef) &&
      !native_noise_or_fractal_is_color3(nodedef) && !native_noise_or_fractal_is_vector2(nodedef))
  {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    (is_native_fractal2d_family(nodedef) || is_native_fractal3d_family(nodedef)) ?
                                        std::initializer_list<const char *>({"amplitude", "octaves", "lacunarity", "diminish", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}) :
                                        std::initializer_list<const char *>({"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"}),
                                    {"out"},
                                    error_message) ||
        !output || output.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
        !read_native_noise_or_fractal_parameters(source, &node, error_message))
    {
      return finish(false);
    }
    if (native_noise_or_fractal_is_3d(nodedef)) {
      Link position;
      if (!read_vector3_output(source.GetInput(pxr::TfToken("position")),
                               graph,
                               &position,
                               active_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["position"] = position;
    }
    else {
      Link texcoord;
      std::unordered_set<string> active_vector2_shaders;
      if (!read_vector2_output(source.GetInput(pxr::TfToken("texcoord")),
                               graph,
                               &texcoord,
                               &active_vector2_shaders,
                               depth + 1,
                               error_message))
      {
        return finish(false);
      }
      node.links["texcoord"] = texcoord;
    }
  }
  else if (nodedef == constant_vector3_id) {
    const pxr::UsdShadeInput value = source.GetInput(pxr::TfToken("value"));
    pxr::GfVec3f vector;
    if (!value || value.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
        value.HasConnectedSource() || !value.Get(&vector)) {
      set_error(error_message, "ND_constant_vector3 requires literal vector3 'value'");
      return finish(false);
    }
    node.vector3_inputs["value"] = make_float3(vector[0], vector[1], vector[2]);
  }
  else if (nodedef == geompropvalue_vector3_id) {
    const pxr::UsdShadeInput geomprop = source.GetInput(pxr::TfToken("geomprop"));
    string value;
    if (!geomprop || geomprop.GetTypeName() != pxr::SdfValueTypeNames->String ||
        geomprop.HasConnectedSource() || !geomprop.Get(&value) || value != "Nworld") {
      set_error(error_message, "ND_geompropvalue_vector3 requires literal geomprop 'Nworld'");
      return finish(false);
    }
    node.string_inputs["geomprop"] = value;
  }
  else if (nodedef == normal_vector3_id || nodedef == position_vector3_id) {
    /* Geometric-source observation (real gap closed): only world-space is a
     * verified honest Cycles native equivalent -- see normal_vector3_id's
     * declaration comment. object/model space fail closed by name rather
     * than silently returning the world-space value. */
    const pxr::UsdShadeInput space_input = source.GetInput(pxr::TfToken("space"));
    string space = "object";
    if (space_input) {
      if (space_input.GetTypeName() != pxr::SdfValueTypeNames->String ||
          space_input.HasConnectedSource() || !space_input.Get(&space))
      {
        set_error(error_message, nodedef + " requires a literal string 'space' input");
        return finish(false);
      }
    }
    if (space != "world") {
      set_error(error_message,
                nodedef + " space '" + space +
                    "' has no honest native Cycles equivalent in this pass "
                    "(only space=\"world\" is supported; Cycles' GeometryNode carries "
                    "no space parameter)");
      return finish(false);
    }
    node.string_inputs["space"] = space;
  }
  else if (is_space_transform(nodedef)) {
    if (!shader_has_exact_signature(source, {"in", "fromspace", "tospace"}, {"out"}, error_message)) {
      return finish(false);
    }
    const pxr::UsdShadeInput vector = source.GetInput(pxr::TfToken("in"));
    if (!vector || vector.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (vector.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(vector, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!vector.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal finite or connected vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    for (const char *name : {"fromspace", "tospace"}) {
      const pxr::UsdShadeInput space = source.GetInput(pxr::TfToken(name));
      string value;
      if (!space || space.GetTypeName() != pxr::SdfValueTypeNames->String ||
          space.HasConnectedSource() || !space.Get(&value) || !is_supported_transform_space(value))
      {
        set_error(error_message, nodedef + " requires literal supported transform space '" + name + "'");
        return finish(false);
      }
      node.string_inputs[name] = value;
    }
  }
  else if (nodedef == convert_vector2_vector3_id) {
    Link value; std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(source.GetInput(pxr::TfToken("in")), graph, &value,
                             &active_vector2_shaders, depth + 1, error_message)) return finish(false);
    node.links["in"] = value;
  }
  else if (nodedef == convert_color4_vector3_id) {
    Link value;
    std::unordered_set<string> active_color4_shaders;
    std::unordered_map<string, string> emitted_color4_shaders;
    if (!read_color4_output(source.GetInput(pxr::TfToken("in")),
                            graph,
                            &value,
                            &active_color4_shaders,
                            &emitted_color4_shaders,
                            depth + 1,
                            error_message))
    {
      return finish(false);
    }
    node.links["in"] = value;
  }
  else if (nodedef == convert_vector4_vector3_id) {
    Link value;
    std::unordered_set<string> active_vector4_shaders;
    std::unordered_map<string, string> emitted_vector4_shaders;
    if (!read_vector4_output(source.GetInput(pxr::TfToken("in")),
                             graph,
                             &value,
                             &active_vector4_shaders,
                             &emitted_vector4_shaders,
                             depth + 1,
                             error_message))
    {
      return finish(false);
    }
    node.links["in"] = value;
  }
  else if (nodedef == convert_color3_vector3_id) {
    Link color;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source.GetInput(pxr::TfToken("in")), graph, &color, &active_color_shaders,
                           depth + 1, error_message)) return finish(false);
    node.links["in"] = color;
  }
  else if (nodedef == convert_float_vector3_id || nodedef == convert_boolean_vector3_id ||
           nodedef == convert_integer_vector3_id)
  {
    Link value;
    if (nodedef == convert_float_vector3_id) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      if (!read_float_output(source.GetInput(pxr::TfToken("in")), graph, &value, &active_float_shaders,
                             &emitted_float_shaders, depth + 1, error_message)) return finish(false);
    }
    else if (nodedef == convert_boolean_vector3_id) {
      std::unordered_set<string> active_boolean_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_boolean_shaders,
                               &emitted_boolean_shaders,
                               depth + 1,
                               error_message)) return finish(false);
    }
    else {
      std::unordered_set<string> active_integer_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(source.GetInput(pxr::TfToken("in")),
                               graph,
                               &value,
                               &active_integer_shaders,
                               &emitted_integer_shaders,
                               depth + 1,
                               error_message)) return finish(false);
    }
    node.links["in"] = value;
  }
  else if (nodedef == normalize_vector3_id || nodedef == absval_vector3_id ||
           nodedef == floor_vector3_id || nodedef == ceil_vector3_id ||
           nodedef == fract_vector3_id || nodedef == sin_vector3_id ||
           nodedef == cos_vector3_id || nodedef == tan_vector3_id ||
           nodedef == sign_vector3_id || nodedef == acos_vector3_id ||
           nodedef == asin_vector3_id || nodedef == exp_vector3_id ||
           nodedef == ln_vector3_id || nodedef == sqrt_vector3_id ||
           nodedef == "ND_round_vector3")
  {
    Link source_link;
    if (!read_vector3_output(source.GetInput(pxr::TfToken("in")),
                             graph, &source_link, active_shaders, depth + 1, error_message)) {
      return finish(false);
    }
    node.links["in"] = source_link;
  }
  else if (is_contrast_vector3(nodedef)) {
    const bool scalar_parameters = contrast_uses_scalar_parameters(nodedef);
    for (const char *name : {"pivot", "amount"}) {
      const pxr::UsdShadeInput parameter = source.GetInput(pxr::TfToken(name));
      if (!parameter || parameter.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal parameter inputs");
        return finish(false);
      }
      if (scalar_parameters) {
        float value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Float || !parameter.Get(&value) ||
            !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires literal finite float input '" + name + "'");
          return finish(false);
        }
        node.inputs[name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (parameter.GetTypeName() != pxr::SdfValueTypeNames->Float3 || !parameter.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
          set_error(error_message, nodedef + " requires literal finite vector3 input '" + name + "'");
          return finish(false);
        }
        node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal finite vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
  }
  else if (nodedef == remap_vector3_id || nodedef == remap_vector3fa_id ||
           nodedef == range_vector3_id || nodedef == range_vector3fa_id) {
    const bool scalar_bounds = nodedef == remap_vector3fa_id || nodedef == range_vector3fa_id;
    for (const char *name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput bound = source.GetInput(pxr::TfToken(name));
      if (!bound || bound.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal bounds");
        return finish(false);
      }
      if (scalar_bounds) {
        float value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Float || !bound.Get(&value) ||
            !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires finite float bounds");
          return finish(false);
        }
        node.inputs[name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Float3 || !bound.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
          set_error(error_message, nodedef + " requires finite vector3 bounds");
          return finish(false);
        }
        node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    if (scalar_bounds ? node.inputs.at("inlow") == node.inputs.at("inhigh") :
                        (node.vector3_inputs.at("inlow").x == node.vector3_inputs.at("inhigh").x ||
                         node.vector3_inputs.at("inlow").y == node.vector3_inputs.at("inhigh").y ||
                         node.vector3_inputs.at("inlow").z == node.vector3_inputs.at("inhigh").z)) {
      set_error(error_message, nodedef + " requires inlow != inhigh in every component");
      return finish(false);
    }
    if (nodedef == range_vector3_id || nodedef == range_vector3fa_id) {
      const pxr::UsdShadeInput gamma_input = source.GetInput(pxr::TfToken("gamma"));
      if (scalar_bounds) {
        float gamma;
        if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) || gamma != 1.0f)
        {
          set_error(error_message, nodedef + " requires literal gamma 1.0");
          return finish(false);
        }
      }
      else {
        pxr::GfVec3f gamma;
        if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
            gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) || gamma[0] != 1.0f ||
            gamma[1] != 1.0f || gamma[2] != 1.0f)
        {
          set_error(error_message, nodedef + " requires literal gamma (1, 1, 1)");
          return finish(false);
        }
      }
      const pxr::UsdShadeInput clamp_input = source.GetInput(pxr::TfToken("doclamp"));
      bool do_clamp;
      if (!clamp_input || clamp_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
          clamp_input.HasConnectedSource() || !clamp_input.Get(&do_clamp))
      {
        set_error(error_message, nodedef + " requires literal boolean 'doclamp'");
        return finish(false);
      }
      const float3 outlow = scalar_bounds ? make_float3(node.inputs.at("outlow")) :
                                           node.vector3_inputs.at("outlow");
      const float3 outhigh = scalar_bounds ? make_float3(node.inputs.at("outhigh")) :
                                            node.vector3_inputs.at("outhigh");
      if (do_clamp && (outlow.x > outhigh.x || outlow.y > outhigh.y || outlow.z > outhigh.z)) {
        set_error(error_message, nodedef + " requires outlow <= outhigh in every component when clamped");
        return finish(false);
      }
      node.int_inputs["doclamp"] = do_clamp ? 1 : 0;
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal finite vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
  }
  else if (nodedef == clamp_vector3_id || nodedef == clamp_vector3fa_id) {
    const bool scalar_bounds = nodedef == clamp_vector3fa_id;
    for (const char *name : {"low", "high"}) {
      const pxr::UsdShadeInput bound = source.GetInput(pxr::TfToken(name));
      if (!bound || bound.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal bounds");
        return finish(false);
      }
      if (scalar_bounds) {
        float value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Float || !bound.Get(&value) ||
            !std::isfinite(value)) {
          set_error(error_message, nodedef + " requires finite float bounds");
          return finish(false);
        }
        node.inputs[name] = value;
      }
      else {
        pxr::GfVec3f value;
        if (bound.GetTypeName() != pxr::SdfValueTypeNames->Float3 || !bound.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
          set_error(error_message, nodedef + " requires finite vector3 bounds");
          return finish(false);
        }
        node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    if (scalar_bounds) {
      if (node.inputs.at("low") > node.inputs.at("high")) {
        set_error(error_message, nodedef + " requires low <= high");
        return finish(false);
      }
    }
    else {
      const float3 &low = node.vector3_inputs.at("low");
      const float3 &high = node.vector3_inputs.at("high");
      if (low.x > high.x || low.y > high.y || low.z > high.z) {
        set_error(error_message, nodedef + " requires low <= high in every component");
        return finish(false);
      }
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal finite or connected vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
  }
  else if (nodedef == rotate3d_vector3_id) {
    if (!shader_has_exact_signature(source, {"in", "amount", "axis"}, {"out"}, error_message) ||
        source.GetOutput(pxr::TfToken("out")).GetTypeName() != pxr::SdfValueTypeNames->Float3)
    {
      return finish(false);
    }
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (input) {
      if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, "ND_rotate3d_vector3 requires vector3 input 'in'");
        return finish(false);
      }
      if (input.HasConnectedSource()) {
        Link link;
        if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links["in"] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
            !std::isfinite(value[2])) {
          set_error(error_message,
                    "ND_rotate3d_vector3 requires literal finite or connected vector3 input 'in'");
          return finish(false);
        }
        node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput amount = source.GetInput(pxr::TfToken("amount"));
    if (amount) {
      if (amount.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_rotate3d_vector3 requires float input 'amount'");
        return finish(false);
      }
      if (amount.HasConnectedSource()) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        Link link;
        if (!read_float_output(amount,
                               graph,
                               &link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        node.links["amount"] = link;
      }
      else if (!amount.Get(&node.inputs["amount"]) || !std::isfinite(node.inputs["amount"])) {
        set_error(error_message,
                  "ND_rotate3d_vector3 requires literal finite or connected float input 'amount'");
        return finish(false);
      }
    }
    const pxr::UsdShadeInput axis = source.GetInput(pxr::TfToken("axis"));
    if (axis) {
      pxr::GfVec3f axis_value;
      if (axis.GetTypeName() != pxr::SdfValueTypeNames->Float3 || axis.HasConnectedSource() ||
          !axis.Get(&axis_value) || !std::isfinite(axis_value[0]) || !std::isfinite(axis_value[1]) ||
          !std::isfinite(axis_value[2]) || axis_value.GetLength() == 0.0f)
      {
        set_error(error_message, "ND_rotate3d_vector3 requires literal finite nonzero vector3 input 'axis'");
        return finish(false);
      }
      node.vector3_inputs["axis"] = make_float3(axis_value[0], axis_value[1], axis_value[2]);
    }
  }
  else if (nodedef == "ND_atan2_vector3") {
    for (const char *name : {"iny", "inx"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, nodedef + " requires vector3 input '" + name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link link;
        if (!read_vector3_output(operand, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[name] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!operand.Get(&value)) {
          set_error(error_message, nodedef + " requires literal or connected vector3 input '" + name + "'");
          return finish(false);
        }
        node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
  }
  else if (nodedef == mix_vector3_id || nodedef == mix_vector3_vector3_id) {
    const bool vector_factor = nodedef == mix_vector3_vector3_id;
    for (const char *input_name : {"bg", "fg"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(input_name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, nodedef + " requires vector3 input '" + input_name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        Link source_link;
        if (!read_vector3_output(operand, graph, &source_link, active_shaders, depth + 1, error_message)) return finish(false);
        node.links[input_name] = source_link;
      }
      else {
        pxr::GfVec3f value;
        if (!operand.Get(&value)) {
          set_error(error_message, nodedef + " requires literal or connected vector3 input '" + input_name + "'");
          return finish(false);
        }
        node.vector3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput factor = source.GetInput(pxr::TfToken("mix"));
    if (!factor || factor.GetTypeName() != (vector_factor ? pxr::SdfValueTypeNames->Float3 :
                                                           pxr::SdfValueTypeNames->Float))
    {
      set_error(error_message,
                nodedef + " requires " + string(vector_factor ? "vector3" : "float") +
                    " input 'mix'");
      return finish(false);
    }
    if (factor.HasConnectedSource()) {
      Link source_link;
      if (vector_factor) {
        if (!read_vector3_output(factor, graph, &source_link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
      }
      else {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(factor,
                               graph,
                               &source_link,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
      }
      node.links["mix"] = source_link;
    }
    else if (vector_factor) {
      pxr::GfVec3f value;
      if (!factor.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires literal finite or connected vector3 input 'mix'");
        return finish(false);
      }
      node.vector3_inputs["mix"] = make_float3(value[0], value[1], value[2]);
    }
    else if (!factor.Get(&node.inputs["mix"]) || !std::isfinite(node.inputs["mix"])) {
      set_error(error_message, nodedef + " requires literal finite or connected float input 'mix'");
      return finish(false);
    }
  }
  else if (is_vector_conditional(nodedef)) {
    for (const char *name : {"value1", "value2"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(name));
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float) { set_error(error_message, nodedef + " requires float input '" + name + "'"); return finish(false); }
      if (input.HasConnectedSource()) { std::unordered_set<string> active_float_shaders; std::unordered_map<string, string> emitted_float_shaders; Link link; if (!read_float_output(input, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false); node.links[name] = link; }
      else if (!input.Get(&node.inputs[name])) { set_error(error_message, nodedef + " requires literal or connected float input '" + name + "'"); return finish(false); }
    }
    for (const char *name : {"in1", "in2"}) {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(name));
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) { set_error(error_message, nodedef + " requires vector3 input '" + name + "'"); return finish(false); }
      if (input.HasConnectedSource()) { Link link; if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) return finish(false); node.links[name] = link; }
      else { pxr::GfVec3f value; if (!input.Get(&value)) { set_error(error_message, nodedef + " requires literal or connected vector3 input '" + name + "'"); return finish(false); } node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]); }
    }
  }
  else if (nodedef == "ND_add_vector3" || nodedef == "ND_subtract_vector3" ||
           nodedef == "ND_multiply_vector3" || nodedef == "ND_divide_vector3" ||
           nodedef == crossproduct_vector3_id || nodedef == min_vector3_id ||
           nodedef == max_vector3_id || nodedef == "ND_modulo_vector3" ||
           nodedef == "ND_power_vector3" || nodedef == safepower_vector3_id)
  {
    for (const char *input_name : {"in1", "in2"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(input_name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, nodedef + " requires vector3 input '" + input_name + "'");
        return finish(false);
      }
      if (operand.HasConnectedSource()) {
        if (nodedef == "ND_divide_vector3" && string(input_name) == "in2") {
          set_error(error_message,
                    "ND_divide_vector3 requires a literal finite nonzero vector3 input 'in2'");
          return finish(false);
        }
        Link source_link;
        if (!read_vector3_output(
                operand, graph, &source_link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[input_name] = source_link;
      }
      else {
        pxr::GfVec3f value;
        if (!operand.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
            !std::isfinite(value[2]))
        {
          set_error(error_message, nodedef + " requires literal finite or connected vector3 input '" +
                                       input_name + "'");
          return finish(false);
        }
        if (nodedef == "ND_divide_vector3" && string(input_name) == "in2" &&
            (!std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]) ||
             value[0] == 0.0f || value[1] == 0.0f || value[2] == 0.0f)) {
          set_error(error_message,
                    "ND_divide_vector3 requires a literal finite nonzero vector3 input 'in2'");
          return finish(false);
        }
        node.vector3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
      }
    }
  }
  else if (nodedef == reflect_vector3_id) {
    for (const auto &[source_name, lowered_name] :
         {std::pair{"in", "in1"}, std::pair{"normal", "in2"}})
    {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(source_name));
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, "ND_reflect_vector3 requires vector3 input '" + string(source_name) + "'");
        return finish(false);
      }
      if (input.HasConnectedSource()) {
        Link link;
        if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[lowered_name] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!input.Get(&value)) {
          set_error(error_message,
                    "ND_reflect_vector3 requires literal or connected vector3 input '" +
                        string(source_name) + "'");
          return finish(false);
        }
        node.vector3_inputs[lowered_name] = make_float3(value[0], value[1], value[2]);
      }
    }
  }
  else if (nodedef == refract_vector3_id) {
    for (const auto &[source_name, lowered_name] :
         {std::pair{"in", "in1"}, std::pair{"normal", "in2"}})
    {
      const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken(source_name));
      if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
        set_error(error_message, "ND_refract_vector3 requires vector3 input '" + string(source_name) + "'");
        return finish(false);
      }
      if (input.HasConnectedSource()) {
        Link link;
        if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) {
          return finish(false);
        }
        node.links[lowered_name] = link;
      }
      else {
        pxr::GfVec3f value;
        if (!input.Get(&value)) {
          set_error(error_message,
                    "ND_refract_vector3 requires literal or connected vector3 input '" +
                        string(source_name) + "'");
          return finish(false);
        }
        node.vector3_inputs[lowered_name] = make_float3(value[0], value[1], value[2]);
      }
    }
    const pxr::UsdShadeInput ior = source.GetInput(pxr::TfToken("ior"));
    if (!ior || ior.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, "ND_refract_vector3 requires float input 'ior'");
      return finish(false);
    }
    if (ior.HasConnectedSource()) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      Link link;
      if (!read_float_output(
              ior, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["scale"] = link;
    }
    else if (!ior.Get(&node.inputs["scale"]) || !std::isfinite(node.inputs["scale"])) {
      set_error(error_message, "ND_refract_vector3 requires literal finite or connected float input 'ior'");
      return finish(false);
    }
  }
  else if (nodedef == multiply_vector3_fa_id || nodedef == add_vector3_fa_id ||
           nodedef == subtract_vector3_fa_id || nodedef == "ND_modulo_vector3FA" ||
           nodedef == "ND_power_vector3FA" || nodedef == "ND_min_vector3FA" ||
           nodedef == "ND_max_vector3FA" || nodedef == "ND_divide_vector3FA" ||
           nodedef == safepower_vector3_fa_id)
  {
    const pxr::UsdShadeInput vector = source.GetInput(pxr::TfToken("in1"));
    if (!vector || vector.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in1'");
      return finish(false);
    }
    if (vector.HasConnectedSource()) {
      Link source_link;
      if (!read_vector3_output(vector, graph, &source_link, active_shaders, depth + 1, error_message)) {
        return finish(false);
      }
      node.links["in1"] = source_link;
    }
    else {
      pxr::GfVec3f value;
      if (!vector.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message,
                  nodedef + " requires literal finite or connected vector3 input 'in1'");
        return finish(false);
      }
      node.vector3_inputs["in1"] = make_float3(value[0], value[1], value[2]);
    }
    const pxr::UsdShadeInput scale = source.GetInput(pxr::TfToken("in2"));
    if (!scale || scale.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, nodedef + " requires float input 'in2'");
      return finish(false);
    }
    if (scale.HasConnectedSource()) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      Link source_link;
      if (!read_float_output(scale,
                             graph,
                             &source_link,
                             &active_float_shaders,
                             &emitted_float_shaders,
                             depth + 1,
                             error_message))
      {
        return finish(false);
      }
      node.links["in2"] = source_link;
    }
    else if (!scale.Get(&node.inputs["in2"]) || !std::isfinite(node.inputs["in2"]) ||
             (nodedef == "ND_divide_vector3FA" && node.inputs["in2"] == 0.0f)) {
      set_error(error_message, nodedef + " requires literal or connected float input 'in2'");
      return finish(false);
    }
  }
  else if (nodedef == invert_vector3_id || nodedef == invert_vector3_fa_id) {
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal or connected vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    const bool scalar_amount = nodedef == invert_vector3_fa_id;
    const pxr::UsdShadeInput amount = source.GetInput(pxr::TfToken("amount"));
    if (!amount || amount.GetTypeName() != (scalar_amount ? pxr::SdfValueTypeNames->Float : pxr::SdfValueTypeNames->Float3)) {
      set_error(error_message, nodedef + " requires " + string(scalar_amount ? "float" : "vector3") + " input 'amount'");
      return finish(false);
    }
    if (amount.HasConnectedSource()) {
      Link link;
      if (scalar_amount) {
        std::unordered_set<string> active_float_shaders;
        std::unordered_map<string, string> emitted_float_shaders;
        if (!read_float_output(amount, graph, &link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false);
      }
      else if (!read_vector3_output(amount, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["amount"] = link;
    }
    else if (scalar_amount) {
      if (!amount.Get(&node.inputs["amount"]) || !std::isfinite(node.inputs["amount"])) {
        set_error(error_message, nodedef + " requires literal or connected float input 'amount'");
        return finish(false);
      }
    }
    else {
      pxr::GfVec3f value;
      if (!amount.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal or connected vector3 input 'amount'");
        return finish(false);
      }
      node.vector3_inputs["amount"] = make_float3(value[0], value[1], value[2]);
    }
  }
  else if (nodedef == smoothstep_vector3_id || nodedef == smoothstep_vector3_fa_id) {
    const pxr::UsdShadeInput input = source.GetInput(pxr::TfToken("in"));
    if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, nodedef + " requires vector3 input 'in'");
      return finish(false);
    }
    if (input.HasConnectedSource()) {
      Link link;
      if (!read_vector3_output(input, graph, &link, active_shaders, depth + 1, error_message)) return finish(false);
      node.links["in"] = link;
    }
    else {
      pxr::GfVec3f value;
      if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
        set_error(error_message, nodedef + " requires literal or connected vector3 input 'in'");
        return finish(false);
      }
      node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
    }
    const bool scalar_edges = nodedef == smoothstep_vector3_fa_id;
    for (const char *name : {"low", "high"}) {
      const pxr::UsdShadeInput edge = source.GetInput(pxr::TfToken(name));
      if (!edge || edge.HasConnectedSource() || edge.GetTypeName() != (scalar_edges ? pxr::SdfValueTypeNames->Float : pxr::SdfValueTypeNames->Float3)) {
        set_error(error_message, nodedef + " requires literal " + string(scalar_edges ? "float" : "vector3") + " input '" + name + "'");
        return finish(false);
      }
      if (scalar_edges) {
        if (!edge.Get(&node.inputs[name]) || !std::isfinite(node.inputs[name])) {
          set_error(error_message, nodedef + " requires finite float input '" + name + "'");
          return finish(false);
        }
      }
      else {
        pxr::GfVec3f value;
        if (!edge.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2])) {
          set_error(error_message, nodedef + " requires finite vector3 input '" + name + "'");
          return finish(false);
        }
        node.vector3_inputs[name] = make_float3(value[0], value[1], value[2]);
      }
    }
    if (scalar_edges ? node.inputs["low"] >= node.inputs["high"] :
                       node.vector3_inputs["low"].x >= node.vector3_inputs["high"].x || node.vector3_inputs["low"].y >= node.vector3_inputs["high"].y || node.vector3_inputs["low"].z >= node.vector3_inputs["high"].z) {
      set_error(error_message, nodedef + " requires low < high per component");
      return finish(false);
    }
  }
  else if (nodedef == combine3_vector3_id) {
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    for (const char *input_name : {"in1", "in2", "in3"}) {
      const pxr::UsdShadeInput operand = source.GetInput(pxr::TfToken(input_name));
      if (!operand || operand.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_combine3_vector3 requires float inputs 'in1', 'in2', and 'in3'");
        return finish(false);
      }
      if (!operand.HasConnectedSource()) {
        float value;
        if (!operand.Get(&value)) {
          set_error(error_message,
                    string("ND_combine3_vector3 requires literal or connected float input '") +
                        input_name + "'");
          return finish(false);
        }
        node.inputs[input_name] = value;
      }
      else {
        Link scalar_source;
        if (!read_float_output(operand,
                               graph,
                               &scalar_source,
                               &active_float_shaders,
                               &emitted_float_shaders,
                               depth + 1,
                               error_message))
        {
          return finish(false);
        }
        node.links[input_name] = scalar_source;
      }
    }
  }
  else if (nodedef == usdprimvarreader_vector3_id) {
    const pxr::UsdShadeInput varname = source.GetInput(pxr::TfToken("varname"));
    string value;
    if (!varname || varname.GetTypeName() != pxr::SdfValueTypeNames->String ||
        varname.HasConnectedSource() || !varname.Get(&value) || value.empty())
    {
      set_error(error_message, "ND_UsdPrimvarReader_vector3 requires a literal string 'varname' input");
      return finish(false);
    }
    node.string_inputs["varname"] = value;
  }
  else if (nodedef == texcoord_vector3_id) {
    int index = 0;
    const pxr::UsdShadeInput index_input = source.GetInput(pxr::TfToken("index"));
    if (index_input) {
      if (index_input.GetTypeName() != pxr::SdfValueTypeNames->Int ||
          index_input.HasConnectedSource() || !index_input.Get(&index) || index < 0)
      {
        set_error(error_message, "ND_texcoord_vector3 'index' must be a literal non-negative integer");
        return finish(false);
      }
    }
    node.string_inputs["geomprop"] = texcoord_attribute_name(index);
  }
  else if (nodedef == viewdirection_vector3_id) {
    /* Geometric-source observation (real gap closed): only space="world" has
     * a verified honest native Cycles equivalent -- see
     * viewdirection_vector3_id's declaration comment above. */
    const pxr::UsdShadeInput space_input = source.GetInput(pxr::TfToken("space"));
    string space = "world";
    if (space_input) {
      if (space_input.GetTypeName() != pxr::SdfValueTypeNames->String ||
          space_input.HasConnectedSource() || !space_input.Get(&space))
      {
        set_error(error_message, nodedef + " requires a literal string 'space' input");
        return finish(false);
      }
    }
    if (space != "world") {
      set_error(error_message,
                nodedef + " space '" + space +
                    "' has no honest native Cycles equivalent in this pass "
                    "(only space=\"world\" is supported; Cycles' GeometryNode carries "
                    "no space parameter)");
      return finish(false);
    }
    node.string_inputs["space"] = space;
  }
  else if (nodedef == tangent_vector3_id || nodedef == bitangent_vector3_id) {
    /* Documented boundary, not a fabricated substitute: Cycles' TangentNode
     * has no bitangent output at all, and MaterialX's <tangent>/<bitangent>
     * select their UV set by integer "index" while Cycles' TangentNode
     * addresses a UV set by a named attribute -- no index-to-attribute-name
     * convention for tangents has been verified against a real Blender/Cycles
     * source (unlike texcoord_attribute_name()/geomcolor_attribute_name()
     * above, which cite a real USD-importer convention). Failing closed here
     * rather than guessing a name. */
    set_error(error_message,
              nodedef + " has no verified honest native Cycles equivalent in this pass");
    return finish(false);
  }
  else if (nodedef == bump_vector3_id) {
    /* Documented boundary, not a fabricated substitute: Cycles' BumpNode
     * (scene/shader_nodes.h) takes derivative-sampled height inputs
     * (sample_center/sample_x/sample_y), which requires evaluating its
     * 'height' input three times at offset shading positions -- this
     * reader's single-value Link model has no mechanism for that, so this is
     * left unadmitted rather than wiring a proxy. */
    set_error(error_message,
              nodedef + " has no verified honest native Cycles equivalent in this pass");
    return finish(false);
  }
  else if (nodedef == normalmap_float_id) {
    /* ND_normalmap_float was previously only reachable as the direct source of
     * an OpenPBR normal/coat_normal terminal input (read_normal_terminal_input);
     * wire it into the general vector3 dispatch so it is also recognized when
     * used as a connected source anywhere else a vector3 is required. */
    std::unordered_map<string, string> emitted_normalmap_shaders;
    Link normalmap_result;
    if (!read_normalmap_output(
            input, graph, &normalmap_result, &emitted_normalmap_shaders, error_message))
    {
      return finish(false);
    }
    *result = normalmap_result;
    return finish(true);
  }
  else {
    set_error(error_message, string("MaterialX vector input requires a supported vector3 node, got ") + nodedef);
    return finish(false);
  }
  *result = {node.name, "out", Type::Vector3};
  graph->nodes.push_back(std::move(node));
  return finish(true);
}

bool read_normalmap_output(const pxr::UsdShadeInput &input,
                           Graph *graph,
                           Link *result,
                           std::unordered_map<string, string> *emitted_shaders,
                           string *error_message)
{
  pxr::UsdShadeShader normalmap;
  if (!connected_shader(input, normalmap_float_id, &normalmap, error_message)) {
    return false;
  }
  const string normalmap_path = normalmap.GetPath().GetString();
  if (const auto emitted = emitted_shaders->find(normalmap_path); emitted != emitted_shaders->end()) {
    *result = {emitted->second, "out", Type::Vector3};
    return true;
  }

  for (const pxr::UsdShadeInput &normalmap_input : normalmap.GetInputs()) {
    const string name = normalmap_input.GetBaseName().GetString();
    if (name != "in" && name != "scale") {
      set_error(error_message,
                string("ND_normalmap_float has unsupported explicit basis/input '") + name + "'");
      return false;
    }
  }

  Node node;
  node.name = unique_node_name(
      *graph, normalmap.GetPrim().GetName().GetString(), normalmap.GetPath().GetString());
  node.nodedef = normalmap_float_id;
  node.outputs["out"] = Type::Vector3;

  const pxr::UsdShadeInput scale = normalmap.GetInput(pxr::TfToken("scale"));
  if (scale) {
    float scale_value;
    if (scale.GetTypeName() != pxr::SdfValueTypeNames->Float || scale.HasConnectedSource() ||
        !scale.Get(&scale_value) || !std::isfinite(scale_value) || scale_value != 1.0f)
    {
      set_error(error_message,
                "ND_normalmap_float scale must be omitted or be a literal unit value");
      return false;
    }
    node.inputs["scale"] = scale_value;
  }

  const pxr::UsdShadeInput normal_input = normalmap.GetInput(pxr::TfToken("in"));
  if (!normal_input || normal_input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
    set_error(error_message, "ND_normalmap_float requires float3 input 'in'");
    return false;
  }
  if (normal_input.HasConnectedSource()) {
    pxr::UsdShadeShader image;
    if (!connected_shader(normal_input, image_vector3_id, &image, error_message)) {
      std::unordered_set<string> active_vector_shaders;
      Link vector_source;
      if (!read_vector3_output(normal_input,
                               graph,
                               &vector_source,
                               &active_vector_shaders,
                               0,
                               error_message)) {
        return false;
      }
      node.links["in"] = vector_source;
      *result = {node.name, "out", Type::Vector3};
      graph->nodes.push_back(std::move(node));
      return true;
    }
    const pxr::UsdShadeInput file_input = image.GetInput(pxr::TfToken("file"));
    pxr::SdfAssetPath asset_path;
    if (!file_input || file_input.GetTypeName() != pxr::SdfValueTypeNames->Asset ||
        file_input.HasConnectedSource() || !file_input.Get(&asset_path))
    {
      set_error(error_message, "ND_image_vector3 requires a literal asset 'file' input");
      return false;
    }
    string file_path = asset_path.GetResolvedPath();
    if (file_path.empty()) {
      file_path = asset_path.GetAssetPath();
    }
    if (file_path.empty() || path_is_relative(file_path) || !path_is_file(file_path) ||
        path_file_size(file_path) == 0)
    {
      set_error(error_message, "ND_image_vector3 file asset is unavailable or invalid");
      return false;
    }
    Link texcoord;
    std::unordered_set<string> active_vector2_shaders;
    if (!read_vector2_output(image.GetInput(pxr::TfToken("texcoord")),
                             graph,
                             &texcoord,
                             &active_vector2_shaders,
                             0,
                             error_message))
    {
      return false;
    }

    Node image_node;
    image_node.name = unique_node_name(
        *graph, image.GetPrim().GetName().GetString(), image.GetPath().GetString());
    image_node.nodedef = image_vector3_id;
    image_node.asset_inputs["file"] = file_path;
    image_node.links["texcoord"] = texcoord;
    image_node.outputs["out"] = Type::Vector3;
    node.links["in"] = {image_node.name, "out", Type::Vector3};
    graph->nodes.push_back(std::move(image_node));
  }
  else {
    pxr::GfVec3f value;
    if (!normal_input.Get(&value)) {
      set_error(error_message, "ND_normalmap_float 'in' must be a literal or a supported image");
      return false;
    }
    node.vector3_inputs["in"] = make_float3(value[0], value[1], value[2]);
  }

  *result = {node.name, "out", Type::Vector3};
  emitted_shaders->emplace(normalmap_path, node.name);
  graph->nodes.push_back(std::move(node));
  return true;
}

bool read_normal_terminal_input(const pxr::UsdShadeShader &surface,
                                const char *input_name,
                                Graph *graph,
                                Node *open_pbr,
                                bool *has_supported_input,
                                std::unordered_map<string, string> *emitted_shaders,
                                string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3 || !input.HasConnectedSource()) {
    set_error(error_message,
              string("OpenPBR ") + input_name + " must be a connected float3 normalmap");
    return false;
  }
  Link source;
  if (!read_normalmap_output(input, graph, &source, emitted_shaders, error_message)) {
    return false;
  }
  open_pbr->links[input_name] = source;
  *has_supported_input = true;
  return true;
}

bool read_displacement_float_input(const pxr::UsdShadeShader &displacement,
                                   const char *input_name,
                                   const char *nodedef_id,
                                   const float default_value,
                                   Graph *graph,
                                   FloatInput *result,
                                   std::unordered_map<string, string> *emitted_shaders,
                                   string *error_message)
{
  const pxr::UsdShadeInput input = displacement.GetInput(pxr::TfToken(input_name));
  if (!input) {
    result->value = default_value;
    result->is_linked = false;
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
    set_error(error_message, string(nodedef_id) + " '" + input_name + "' must be a float");
    return false;
  }
  if (!input.HasConnectedSource()) {
    if (!input.Get(&result->value)) {
      set_error(error_message,
                string(nodedef_id) + " '" + input_name + "' must be a literal float");
      return false;
    }
    result->is_linked = false;
    return true;
  }
  std::unordered_set<string> active_shaders;
  if (!read_float_output(
          input, graph, &result->link, &active_shaders, emitted_shaders, 0, error_message))
  {
    return false;
  }
  result->is_linked = true;
  return true;
}

/**
 * ND_displacement_vector3's 'displacement' input (real MaterialX 1.39
 * nodedef: pbrlib/pbrlib_defs.mtlx declares it as vector3 -- "Vector
 * displacement in (dPdu, dPdv, N) tangent/normal space"). Mirrors
 * read_volume_color_input()'s literal-or-linked vector3 handling.
 */
bool read_displacement_vector3_input(const pxr::UsdShadeShader &displacement,
                                     Graph *graph,
                                     Color3Input *result,
                                     string *error_message)
{
  const pxr::UsdShadeInput input = displacement.GetInput(pxr::TfToken("displacement"));
  if (!input) {
    result->is_linked = false;
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
    set_error(error_message, "ND_displacement_vector3 'displacement' must be a vector3");
    return false;
  }
  if (!input.HasConnectedSource()) {
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message,
                "ND_displacement_vector3 'displacement' has no vector3 value");
      return false;
    }
    result->value = make_float3(value[0], value[1], value[2]);
    result->is_linked = false;
    return true;
  }
  std::unordered_set<string> active_shaders;
  if (!read_vector3_output(input, graph, &result->link, &active_shaders, 0, error_message)) {
    return false;
  }
  result->is_linked = true;
  return true;
}

struct DisplacementValue {
  bool is_vector3 = false;
  FloatInput scalar;
  Color3Input vector;
  FloatInput scale = {1.0f};
};

Link emit_scaled_scalar_displacement(Graph *graph,
                                     const FloatInput &height,
                                     const FloatInput &scale,
                                     const string &synth_path)
{
  Node math;
  math.name = unique_node_name(*graph, synth_path, synth_path + "#");
  math.nodedef = multiply_float_id;
  if (height.is_linked) {
    math.links["in1"] = height.link;
  }
  else {
    math.inputs["in1"] = height.value;
  }
  if (scale.is_linked) {
    math.links["in2"] = scale.link;
  }
  else {
    math.inputs["in2"] = scale.value;
  }
  math.outputs["out"] = Type::Float;
  const Link link{math.name, "out", Type::Float};
  graph->nodes.push_back(std::move(math));
  return link;
}

Link emit_scaled_vector_displacement(Graph *graph,
                                     const Color3Input &vector,
                                     const FloatInput &scale,
                                     const string &synth_path)
{
  Node math;
  math.name = unique_node_name(*graph, synth_path, synth_path + "#");
  math.nodedef = multiply_vector3_fa_id;
  if (vector.is_linked) {
    math.links["in1"] = vector.link;
  }
  else {
    math.vector3_inputs["in1"] = vector.value;
  }
  if (scale.is_linked) {
    math.links["in2"] = scale.link;
  }
  else {
    math.inputs["in2"] = scale.value;
  }
  math.outputs["out"] = Type::Vector3;
  const Link link{math.name, "out", Type::Vector3};
  graph->nodes.push_back(std::move(math));
  return link;
}

bool read_displacement_shader(const pxr::UsdShadeShader &displacement,
                              Graph *graph,
                              DisplacementValue *result,
                              std::unordered_map<string, string> *emitted_float_shaders,
                              std::unordered_set<string> *active_displacement_shaders,
                              string *error_message)
{
  const string shader_path = displacement.GetPath().GetString();
  if (!active_displacement_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX displacementshader graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_displacement_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken displacement_id;
  displacement.GetShaderId(&displacement_id);
  const string nodedef = displacement_id.GetString();

  if (nodedef == displacement_float_id) {
    result->is_vector3 = false;
    if (!read_displacement_float_input(displacement,
                                       "displacement",
                                       displacement_float_id,
                                       0.0f,
                                       graph,
                                       &result->scalar,
                                       emitted_float_shaders,
                                       error_message) ||
        !read_displacement_float_input(displacement,
                                       "scale",
                                       displacement_float_id,
                                       1.0f,
                                       graph,
                                       &result->scale,
                                       emitted_float_shaders,
                                       error_message))
    {
      return finish(false);
    }
    for (const pxr::UsdShadeInput &input : displacement.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "displacement" && name != "scale") {
        set_error(error_message,
                  string(displacement_float_id) + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
    return finish(true);
  }

  if (nodedef == displacement_vector3_id) {
    result->is_vector3 = true;
    if (!read_displacement_vector3_input(displacement, graph, &result->vector, error_message) ||
        !read_displacement_float_input(displacement,
                                       "scale",
                                       displacement_vector3_id,
                                       1.0f,
                                       graph,
                                       &result->scale,
                                       emitted_float_shaders,
                                       error_message))
    {
      return finish(false);
    }
    for (const pxr::UsdShadeInput &input : displacement.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "displacement" && name != "scale") {
        set_error(error_message,
                  string(displacement_vector3_id) + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
    return finish(true);
  }

  if (nodedef == mix_displacementshader_id) {
    DisplacementValue bg;
    DisplacementValue fg;
    pxr::UsdShadeShader bg_shader;
    pxr::UsdShadeShader fg_shader;
    const pxr::UsdShadeInput bg_input = displacement.GetInput(pxr::TfToken("bg"));
    const pxr::UsdShadeInput fg_input = displacement.GetInput(pxr::TfToken("fg"));
    if (!bg_input || bg_input.GetTypeName() != pxr::SdfValueTypeNames->Token ||
        !fg_input || fg_input.GetTypeName() != pxr::SdfValueTypeNames->Token) {
      set_error(error_message, string(mix_displacementshader_id) + " requires displacementshader 'bg' and 'fg' inputs");
      return finish(false);
    }
    if (!connected_shader_eliding_identity_dot(bg_input, dot_displacementshader_id, &bg_shader, error_message) ||
        !connected_shader_eliding_identity_dot(fg_input, dot_displacementshader_id, &fg_shader, error_message) ||
        !read_displacement_shader(bg_shader,
                                  graph,
                                  &bg,
                                  emitted_float_shaders,
                                  active_displacement_shaders,
                                  error_message) ||
        !read_displacement_shader(fg_shader,
                                  graph,
                                  &fg,
                                  emitted_float_shaders,
                                  active_displacement_shaders,
                                  error_message))
    {
      return finish(false);
    }
    if (bg.is_vector3 != fg.is_vector3) {
      set_error(error_message,
                string(mix_displacementshader_id) + " requires bg and fg to use the same displacement flavor");
      return finish(false);
    }

    FloatInput mix;
    if (!read_displacement_float_input(displacement,
                                       "mix",
                                       mix_displacementshader_id,
                                       0.0f,
                                       graph,
                                       &mix,
                                       emitted_float_shaders,
                                       error_message))
    {
      return finish(false);
    }

    Node mix_node;
    mix_node.name = unique_node_name(*graph, displacement.GetPrim().GetName().GetString(), shader_path);
    mix_node.nodedef = bg.is_vector3 ? mix_vector3_id : mix_float_id;
    if (mix.is_linked) {
      mix_node.links["mix"] = mix.link;
    }
    else {
      mix_node.inputs["mix"] = mix.value;
    }
    if (bg.is_vector3) {
      mix_node.links["bg"] = emit_scaled_vector_displacement(graph, bg.vector, bg.scale, shader_path + ".bg_scale");
      mix_node.links["fg"] = emit_scaled_vector_displacement(graph, fg.vector, fg.scale, shader_path + ".fg_scale");
      mix_node.outputs["out"] = Type::Vector3;
      result->is_vector3 = true;
      result->vector = {};
      result->vector.is_linked = true;
      result->vector.link = {mix_node.name, "out", Type::Vector3};
    }
    else {
      mix_node.links["bg"] = emit_scaled_scalar_displacement(graph, bg.scalar, bg.scale, shader_path + ".bg_scale");
      mix_node.links["fg"] = emit_scaled_scalar_displacement(graph, fg.scalar, fg.scale, shader_path + ".fg_scale");
      mix_node.outputs["out"] = Type::Float;
      result->is_vector3 = false;
      result->scalar = {};
      result->scalar.is_linked = true;
      result->scalar.link = {mix_node.name, "out", Type::Float};
    }
    result->scale = {1.0f};

    for (const pxr::UsdShadeInput &input : displacement.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "fg" && name != "bg" && name != "mix") {
        set_error(error_message,
                  string(mix_displacementshader_id) + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
    graph->nodes.push_back(std::move(mix_node));
    return finish(true);
  }

  set_error(error_message,
            string("MaterialX displacement: USDShade connection requires ") + displacement_float_id +
                ", " + displacement_vector3_id + ", or " + mix_displacementshader_id);
  return finish(false);
}

bool read_displacement_terminal(const pxr::UsdShadeMaterial &material,
                                Graph *graph,
                                std::unordered_map<string, string> *emitted_shaders,
                                string *error_message)
{
  pxr::UsdShadeOutput output = material.GetDisplacementOutput(mtlx_render_context);
  if (!output) {
    output = material.GetDisplacementOutput();
  }
  if (!output) {
    return true;
  }
  if (!output.HasConnectedSource()) {
    /* Material outputs are optional terminals. USDShade can return the standard
     * displacement output even when it has not been authored, so only lower a
     * displacement graph when there is an actual source connection. */
    return true;
  }
  const auto sources = output.GetConnectedSources();
  if (sources.size() != 1) {
    set_error(error_message, "MaterialX displacement output must have exactly one source");
    return false;
  }

  pxr::UsdShadeShader displacement_source;
  if (!resolve_terminal_source_eliding_identity_dot(sources[0].source,
                                                   sources[0].sourceName,
                                                   sources[0].sourceType,
                                                   dot_displacementshader_id,
                                                   output.GetTypeName(),
                                                   &displacement_source,
                                                   error_message))
  {
    return false;
  }

  DisplacementValue displacement;
  std::unordered_set<string> active_displacement_shaders;
  if (!read_displacement_shader(displacement_source,
                                graph,
                                &displacement,
                                emitted_shaders,
                                &active_displacement_shaders,
                                error_message))
  {
    return false;
  }

  graph->displacement_is_vector3 = displacement.is_vector3;
  if (displacement.is_vector3) {
    graph->displacement_vector3 = displacement.vector;
  }
  else {
    graph->displacement = displacement.scalar;
  }
  graph->displacement_scale = displacement.scale;
  graph->has_displacement = true;
  return true;
}

/**
 * Task 3: volume terminal. Read one literal-or-linked vector3 VDF input
 * (`absorption`, `scattering`) directly into a Color3Input, without
 * requiring a throwaway Node the way the OpenPBR terminal-input helpers do.
 *
 * ND_absorption_vdf/ND_anisotropic_vdf's real MaterialX 1.39 nodedef
 * (pbrlib/pbrlib_defs.mtlx) declares 'absorption'/'scattering' as
 * `vector3`, not `color3` -- UsdMtlx therefore surfaces them as
 * SdfValueTypeNames->Float3, matching this reader's own established
 * vector3 convention elsewhere (e.g. the fractal3d 'amplitude' input).
 * A Color3f-typed input here is a genuine type mismatch against the
 * nodedef and fails closed rather than being silently accepted.
 */
bool read_volume_color_input(const pxr::UsdShadeShader &vdf,
                             const char *input_name,
                             Graph *graph,
                             Color3Input *result,
                             string *error_message)
{
  const pxr::UsdShadeInput input = vdf.GetInput(pxr::TfToken(input_name));
  if (!input) {
    result->is_linked = false;
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
    set_error(error_message, string("MaterialX volume '") + input_name + "' must be a vector3");
    return false;
  }
  if (!input.HasConnectedSource()) {
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, string("MaterialX volume '") + input_name + "' has no vector3 value");
      return false;
    }
    result->value = make_float3(value[0], value[1], value[2]);
    result->is_linked = false;
    return true;
  }
  std::unordered_set<string> active_shaders;
  if (!read_vector3_output(input, graph, &result->link, &active_shaders, 0, error_message)) {
    return false;
  }
  result->is_linked = true;
  return true;
}

/**
 * Task 3: read ND_uniform_edf's `color` input (real MaterialX 1.39 nodedef:
 * pbrlib/pbrlib_defs.mtlx declares it as color3 -- unlike 'absorption'/
 * 'scattering' above, emission color genuinely is a color3, so this one
 * correctly checks Color3f and reuses read_color_output.
 */
bool read_volume_emission_color(const pxr::UsdShadeShader &edf,
                                Graph *graph,
                                Color3Input *result,
                                string *error_message)
{
  const pxr::UsdShadeInput input = edf.GetInput(pxr::TfToken("color"));
  if (!input) {
    result->is_linked = false;
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
    set_error(error_message, "MaterialX volume 'edf' color input must be a color3f");
    return false;
  }
  if (!input.HasConnectedSource()) {
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, "MaterialX volume 'edf' color input has no color3f value");
      return false;
    }
    result->value = make_float3(value[0], value[1], value[2]);
    result->is_linked = false;
    return true;
  }
  std::unordered_set<string> active_shaders;
  if (!read_color_output(input, graph, &result->link, &active_shaders, 0, error_message)) {
    return false;
  }
  result->is_linked = true;
  return true;
}

/** Task 3: read one literal-or-linked float VDF input (`anisotropy`). */
bool read_volume_float_input(const pxr::UsdShadeShader &vdf,
                             const char *input_name,
                             const float default_value,
                             Graph *graph,
                             FloatInput *result,
                             string *error_message)
{
  const pxr::UsdShadeInput input = vdf.GetInput(pxr::TfToken(input_name));
  if (!input) {
    result->value = default_value;
    result->is_linked = false;
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
    set_error(error_message, string("MaterialX volume '") + input_name + "' must be a float");
    return false;
  }
  if (!input.HasConnectedSource()) {
    if (!input.Get(&result->value)) {
      set_error(error_message, string("MaterialX volume '") + input_name + "' has no float value");
      return false;
    }
    result->is_linked = false;
    return true;
  }
  std::unordered_set<string> active_shaders;
  std::unordered_map<string, string> emitted_shaders;
  if (!read_float_output(
          input, graph, &result->link, &active_shaders, &emitted_shaders, 0, error_message))
  {
    return false;
  }
  result->is_linked = true;
  return true;
}

/**
 * Closure combinator lowering: a resolved VDF's coefficient bundle, in the
 * same absorption/scattering/anisotropy shape VolumeCoefficientsNode
 * natively carries. `has_scattering` distinguishes "scattering is exactly
 * zero" (an absorption_vdf branch, or a subtree built only from such
 * branches) from "scattering may be non-zero" (an anisotropic_vdf branch),
 * which read_vdf_coefficients() needs to decide whether two operands'
 * anisotropy values can be honestly combined (see ND_add_vdf below).
 */
struct VdfCoefficients {
  Color3Input absorption;
  Color3Input scattering;
  FloatInput anisotropy;
  bool has_scattering = false;
};

/** Emit a real binary vector3 math node (ND_add_vector3, ND_multiply_vector3)
 *  combining two absorption/scattering operands, each either literal or
 *  already-linked -- mirroring read_vector3_output()'s own construction of
 *  the same nodedefs (see its "ND_add_vector3"/"ND_multiply_vector3"
 *  branch) when reading them directly out of an authored MaterialX graph.
 *  Vector3, not Color3: ND_absorption_vdf/ND_anisotropic_vdf's real 1.39
 *  nodedef declares 'absorption'/'scattering' as vector3 (see
 *  read_volume_color_input() above), so this combinator machinery has to
 *  stay in that same type family to combine with what those leaves
 *  actually produce. */
Link combine_vector3(Graph *graph,
                     const char *nodedef,
                     const Color3Input &in1,
                     const Color3Input &in2,
                     const string &synth_path)
{
  Node math;
  math.name = unique_node_name(*graph, synth_path, synth_path + "#");
  math.nodedef = nodedef;
  if (in1.is_linked) {
    math.links["in1"] = in1.link;
  }
  else {
    math.vector3_inputs["in1"] = in1.value;
  }
  if (in2.is_linked) {
    math.links["in2"] = in2.link;
  }
  else {
    math.vector3_inputs["in2"] = in2.value;
  }
  math.outputs["out"] = Type::Vector3;
  const Link link{math.name, "out", Type::Vector3};
  graph->nodes.push_back(std::move(math));
  return link;
}

/** Emit a real ND_multiply_vector3FA node (vector3 * literal float scale)
 *  scaling an absorption/scattering operand by a literal weight -- mirrors
 *  read_vector3_output()'s own construction of the same nodedef. */
Link scale_vector3(Graph *graph, const Color3Input &in1, const float scale, const string &synth_path)
{
  Node math;
  math.name = unique_node_name(*graph, synth_path, synth_path + "#");
  math.nodedef = multiply_vector3_fa_id;
  if (in1.is_linked) {
    math.links["in1"] = in1.link;
  }
  else {
    math.vector3_inputs["in1"] = in1.value;
  }
  math.inputs["in2"] = scale;
  math.outputs["out"] = Type::Vector3;
  const Link link{math.name, "out", Type::Vector3};
  graph->nodes.push_back(std::move(math));
  return link;
}

/** Emit a real ND_mix_vector3 node lerping two absorption/scattering
 *  operands by a literal weight -- mirrors combine_vector3()/scale_vector3()
 *  above, and read_vector3_output()'s own construction of the same nodedef
 *  (see its "ND_mix_vector3" branch, using the real nodedef's "fg"/"bg"/
 *  "mix" input names) when reading it directly out of an authored MaterialX
 *  graph. */
Link mix_vector3(Graph *graph,
                 const Color3Input &fg,
                 const Color3Input &bg,
                 const float mix_weight,
                 const string &synth_path)
{
  Node math;
  math.name = unique_node_name(*graph, synth_path, synth_path + "#");
  math.nodedef = mix_vector3_id;
  if (fg.is_linked) {
    math.links["fg"] = fg.link;
  }
  else {
    math.vector3_inputs["fg"] = fg.value;
  }
  if (bg.is_linked) {
    math.links["bg"] = bg.link;
  }
  else {
    math.vector3_inputs["bg"] = bg.value;
  }
  math.inputs["mix"] = mix_weight;
  math.outputs["out"] = Type::Vector3;
  const Link link{math.name, "out", Type::Vector3};
  graph->nodes.push_back(std::move(math));
  return link;
}

/** Emit a real ND_mix_color3 node lerping two emission-color operands by a
 *  literal weight -- same construction as mix_vector3() above, but Color3
 *  (not Vector3): ND_uniform_edf's real 1.39 nodedef declares 'color' as
 *  color3 (see read_volume_emission_color()'s comment), and graph.cpp's
 *  validate() requires Graph::volume_emission's link to be Type::Color3, so
 *  this has to stay in that type family rather than reusing mix_vector3(). */
Link mix_color3(Graph *graph,
                const Color3Input &fg,
                const Color3Input &bg,
                const float mix_weight,
                const string &synth_path)
{
  Node mix;
  mix.name = unique_node_name(*graph, synth_path, synth_path + "#");
  mix.nodedef = mix_color3_id;
  if (fg.is_linked) {
    mix.links["fg"] = fg.link;
  }
  else {
    mix.color3_inputs["fg"] = fg.value;
  }
  if (bg.is_linked) {
    mix.links["bg"] = bg.link;
  }
  else {
    mix.color3_inputs["bg"] = bg.value;
  }
  mix.inputs["mix"] = mix_weight;
  mix.outputs["out"] = Type::Color3;
  const Link link{mix.name, "out", Type::Color3};
  graph->nodes.push_back(std::move(mix));
  return link;
}

/** Task 3 EndpointResolver, generalized for closure combinators: resolve a
 *  connection to one of the VDF NodeDefs this reader has a real, native
 *  mapping for -- the two leaves (ND_anisotropic_vdf, ND_absorption_vdf),
 *  or the additive/scaling/mixing combinators over them (ND_add_vdf,
 *  ND_multiply_vdfF, ND_multiply_vdfC, ND_mix_vdf). ND_layer_vdf is
 *  deliberately not in this list -- see read_vdf_coefficients() callers for
 *  why (an honest, not-yet-attempted boundary, not a silent substitution).
 */
bool resolve_vdf_shader(const pxr::UsdShadeConnectableAPI &source,
                        const pxr::TfToken &source_name,
                        const pxr::UsdShadeAttributeType source_type,
                        const pxr::SdfValueTypeName &expected_type,
                        pxr::UsdShadeShader *vdf,
                        string *matched_id)
{
  static const char *candidates[] = {anisotropic_vdf_id,
                                     absorption_vdf_id,
                                     multiply_vdff_id,
                                     multiply_vdfc_id,
                                     add_vdf_id,
                                     mix_vdf_id};
  for (const char *id : candidates) {
    std::unordered_set<string> active_endpoints;
    if (resolve_connected_shader(
            source, source_name, source_type, id, expected_type, vdf, &active_endpoints, 0, nullptr))
    {
      *matched_id = id;
      return true;
    }
  }
  return false;
}

/**
 * Real native Cycles lowering for the VDF closure combinators, recursively
 * resolving a VDF connection down to its absorption/scattering/anisotropy
 * coefficients (the shape VolumeCoefficientsNode natively carries):
 *
 *  - ND_multiply_vdfF/ND_multiply_vdfC scale a VDF's absorption and
 *    scattering coefficients by a literal float/color3 weight -- an exact,
 *    physically-based mapping (scaling extinction/scattering coefficients
 *    is literally what "weighting" a VDF's contribution means). The weight
 *    must be a literal: a dynamic/graph-driven scaling weight is an
 *    explicit, unsupported boundary this pass, not a silent narrowing.
 *  - ND_add_vdf sums two VDFs' absorption and scattering coefficients --
 *    also exact (Beer-Lambert extinction/scattering coefficients add under
 *    superposition). Their anisotropy cannot always be combined honestly:
 *    VolumeCoefficientsNode carries exactly one scalar anisotropy (a single
 *    Henyey-Greenstein g) for its whole coefficient bundle, so if both
 *    operands contribute scattering with different (or graph-driven,
 *    unprovable) anisotropy, this fails closed with an explicit error
 *    rather than averaging or arbitrarily picking one -- a genuine
 *    architectural gap in Cycles' volume closure representation, not a
 *    missing mapping.
 *  - ND_mix_vdf lerps two VDFs' absorption and scattering coefficients by a
 *    literal 'mix' weight -- also exact (a linear interpolation of Beer-
 *    Lambert coefficients is a physically well-defined VDF, the same way
 *    ND_mix_vector3 lerps any other vector3 quantity). It hits the exact
 *    same anisotropy-superposition ceiling as ND_add_vdf above when both
 *    operands scatter with differing anisotropy, and requires a literal
 *    'mix' weight for the same reason ND_multiply_vdfF/C require a literal
 *    weight: a dynamic/graph-driven mix factor is an explicit, unsupported
 *    boundary this pass, not a silent narrowing.
 *
 * ND_layer_vdf is not attempted this pass: its output is BSDF-typed
 * (layering a BSDF "top" over a VDF "base" interior) -- there is no
 * BSDF-typed link anywhere in this IR yet (Type::BSDF does not exist; only
 * terminal SurfaceShader/VolumeShader/LightShader types do), so it has no
 * home to lower into without inventing generic closure plumbing first.
 */
bool read_vdf_coefficients(const pxr::UsdShadeConnectableAPI &source,
                           const pxr::TfToken &source_name,
                           const pxr::UsdShadeAttributeType source_type,
                           const pxr::SdfValueTypeName &expected_type,
                           Graph *graph,
                           const int depth,
                           VdfCoefficients *result,
                           string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX VDF graph nesting exceeds maximum depth");
    return false;
  }

  pxr::UsdShadeShader vdf;
  string matched_id;
  if (!resolve_vdf_shader(source, source_name, source_type, expected_type, &vdf, &matched_id)) {
    set_error(error_message,
             "MaterialX VDF input must connect one of: ND_anisotropic_vdf, ND_absorption_vdf, "
             "ND_multiply_vdfF, ND_multiply_vdfC, ND_add_vdf, ND_mix_vdf (ND_layer_vdf is not "
             "yet supported -- see read_vdf_coefficients())");
    return false;
  }

  if (matched_id == anisotropic_vdf_id || matched_id == absorption_vdf_id) {
    const bool is_anisotropic = matched_id == anisotropic_vdf_id;
    if (!read_volume_color_input(vdf, "absorption", graph, &result->absorption, error_message)) {
      return false;
    }
    if (is_anisotropic) {
      if (!read_volume_color_input(vdf, "scattering", graph, &result->scattering, error_message)) {
        return false;
      }
      if (!read_volume_float_input(
              vdf, "anisotropy", 0.0f, graph, &result->anisotropy, error_message))
      {
        return false;
      }
    }
    else {
      result->scattering = Color3Input();
      result->anisotropy = FloatInput();
    }
    for (const pxr::UsdShadeInput &input : vdf.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      const bool allowed = (name == "absorption") ||
                           (is_anisotropic && (name == "scattering" || name == "anisotropy"));
      if (!allowed) {
        set_error(error_message,
                 string(is_anisotropic ? "ND_anisotropic_vdf" : "ND_absorption_vdf") +
                     " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
    result->has_scattering = is_anisotropic;
    return true;
  }

  if (matched_id == multiply_vdff_id || matched_id == multiply_vdfc_id) {
    const bool color_weight = matched_id == multiply_vdfc_id;
    const pxr::UsdShadeInput in1 = vdf.GetInput(pxr::TfToken("in1"));
    const pxr::UsdShadeInput in2 = vdf.GetInput(pxr::TfToken("in2"));
    if (!in1 || !in1.HasConnectedSource()) {
      set_error(error_message, matched_id + " requires a connected 'in1' VDF input");
      return false;
    }
    if (!in2 || in2.HasConnectedSource()) {
      set_error(error_message,
               matched_id +
                   " weight ('in2') must be a literal value -- a dynamic/graph-driven VDF "
                   "scaling weight is an explicit, unsupported boundary this pass");
      return false;
    }
    float scalar_weight = 1.0f;
    float3 color_weight_value = make_float3(1.0f, 1.0f, 1.0f);
    if (color_weight) {
      if (in2.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
        set_error(error_message, "ND_multiply_vdfC 'in2' must be a color3f");
        return false;
      }
      pxr::GfVec3f value;
      if (!in2.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, "ND_multiply_vdfC requires a finite literal 'in2'");
        return false;
      }
      color_weight_value = make_float3(value[0], value[1], value[2]);
    }
    else {
      if (in2.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_multiply_vdfF 'in2' must be a float");
        return false;
      }
      if (!in2.Get(&scalar_weight) || !std::isfinite(scalar_weight)) {
        set_error(error_message, "ND_multiply_vdfF requires a finite literal 'in2'");
        return false;
      }
    }
    const auto in1_sources = in1.GetConnectedSources();
    if (in1_sources.size() != 1) {
      set_error(error_message, matched_id + " 'in1' input must have exactly one source");
      return false;
    }
    VdfCoefficients sub;
    if (!read_vdf_coefficients(in1_sources[0].source,
                               in1_sources[0].sourceName,
                               in1_sources[0].sourceType,
                               in1.GetTypeName(),
                               graph,
                               depth + 1,
                               &sub,
                               error_message))
    {
      return false;
    }
    const string synth_base = vdf.GetPath().GetString();
    if (color_weight) {
      Color3Input weight_input;
      weight_input.value = color_weight_value;
      result->absorption.link = combine_vector3(
          graph, "ND_multiply_vector3", sub.absorption, weight_input, synth_base + ".absorption");
      result->scattering.link = combine_vector3(
          graph, "ND_multiply_vector3", sub.scattering, weight_input, synth_base + ".scattering");
    }
    else {
      result->absorption.link = scale_vector3(
          graph, sub.absorption, scalar_weight, synth_base + ".absorption");
      result->scattering.link = scale_vector3(
          graph, sub.scattering, scalar_weight, synth_base + ".scattering");
    }
    result->absorption.is_linked = true;
    result->scattering.is_linked = true;
    result->anisotropy = sub.anisotropy;
    result->has_scattering = sub.has_scattering;

    for (const pxr::UsdShadeInput &input : vdf.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "in1" && name != "in2") {
        set_error(error_message, matched_id + " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
    return true;
  }

  if (matched_id == add_vdf_id) {
    const pxr::UsdShadeInput in1 = vdf.GetInput(pxr::TfToken("in1"));
    const pxr::UsdShadeInput in2 = vdf.GetInput(pxr::TfToken("in2"));
    if (!in1 || !in1.HasConnectedSource() || !in2 || !in2.HasConnectedSource()) {
      set_error(error_message, "ND_add_vdf requires connected 'in1' and 'in2' VDF inputs");
      return false;
    }
    const auto in1_sources = in1.GetConnectedSources();
    const auto in2_sources = in2.GetConnectedSources();
    if (in1_sources.size() != 1 || in2_sources.size() != 1) {
      set_error(error_message, "ND_add_vdf inputs must each have exactly one source");
      return false;
    }
    VdfCoefficients a;
    VdfCoefficients b;
    if (!read_vdf_coefficients(in1_sources[0].source,
                               in1_sources[0].sourceName,
                               in1_sources[0].sourceType,
                               in1.GetTypeName(),
                               graph,
                               depth + 1,
                               &a,
                               error_message))
    {
      return false;
    }
    if (!read_vdf_coefficients(in2_sources[0].source,
                               in2_sources[0].sourceName,
                               in2_sources[0].sourceType,
                               in2.GetTypeName(),
                               graph,
                               depth + 1,
                               &b,
                               error_message))
    {
      return false;
    }

    if (a.has_scattering && b.has_scattering) {
      const bool anisotropy_matches = !a.anisotropy.is_linked && !b.anisotropy.is_linked &&
                                      a.anisotropy.value == b.anisotropy.value;
      if (!anisotropy_matches) {
        set_error(error_message,
                 "ND_add_vdf: both operands contribute scattering with different (or "
                 "graph-driven) anisotropy -- Cycles' VolumeCoefficientsNode carries a single "
                 "scalar anisotropy for its whole coefficient bundle and cannot represent the "
                 "superposition of two independently-anisotropic phase functions; this is a "
                 "genuine architectural gap, not a missing mapping");
        return false;
      }
      result->anisotropy = a.anisotropy;
    }
    else if (a.has_scattering) {
      result->anisotropy = a.anisotropy;
    }
    else if (b.has_scattering) {
      result->anisotropy = b.anisotropy;
    }
    else {
      result->anisotropy = FloatInput();
    }

    const string synth_base = vdf.GetPath().GetString();
    result->absorption.link = combine_vector3(
        graph, "ND_add_vector3", a.absorption, b.absorption, synth_base + ".absorption");
    result->absorption.is_linked = true;

    result->scattering.link = combine_vector3(
        graph, "ND_add_vector3", a.scattering, b.scattering, synth_base + ".scattering");
    result->scattering.is_linked = true;
    result->has_scattering = a.has_scattering || b.has_scattering;

    for (const pxr::UsdShadeInput &input : vdf.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "in1" && name != "in2") {
        set_error(error_message, string("ND_add_vdf has no direct Cycles equivalent: ") + name);
        return false;
      }
    }
    return true;
  }

  if (matched_id == mix_vdf_id) {
    const pxr::UsdShadeInput fg = vdf.GetInput(pxr::TfToken("fg"));
    const pxr::UsdShadeInput bg = vdf.GetInput(pxr::TfToken("bg"));
    if (!fg || !fg.HasConnectedSource() || !bg || !bg.HasConnectedSource()) {
      set_error(error_message, "ND_mix_vdf requires connected 'fg' and 'bg' VDF inputs");
      return false;
    }
    const auto fg_sources = fg.GetConnectedSources();
    const auto bg_sources = bg.GetConnectedSources();
    if (fg_sources.size() != 1 || bg_sources.size() != 1) {
      set_error(error_message, "ND_mix_vdf inputs must each have exactly one source");
      return false;
    }
    VdfCoefficients a;
    VdfCoefficients b;
    if (!read_vdf_coefficients(fg_sources[0].source,
                               fg_sources[0].sourceName,
                               fg_sources[0].sourceType,
                               fg.GetTypeName(),
                               graph,
                               depth + 1,
                               &a,
                               error_message))
    {
      return false;
    }
    if (!read_vdf_coefficients(bg_sources[0].source,
                               bg_sources[0].sourceName,
                               bg_sources[0].sourceType,
                               bg.GetTypeName(),
                               graph,
                               depth + 1,
                               &b,
                               error_message))
    {
      return false;
    }

    const pxr::UsdShadeInput mix_input = vdf.GetInput(pxr::TfToken("mix"));
    if (!mix_input || mix_input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, "ND_mix_vdf requires float input 'mix'");
      return false;
    }
    float mix_weight = 0.0f;
    if (mix_input.HasConnectedSource() || !mix_input.Get(&mix_weight)) {
      set_error(error_message,
               "ND_mix_vdf requires a literal 'mix' weight: a dynamic/graph-driven mix factor "
               "is an explicit, unsupported boundary this pass (mirrors ND_multiply_vdfF/C's "
               "literal-weight requirement above), not a silent narrowing");
      return false;
    }

    if (a.has_scattering && b.has_scattering) {
      const bool anisotropy_matches = !a.anisotropy.is_linked && !b.anisotropy.is_linked &&
                                      a.anisotropy.value == b.anisotropy.value;
      if (!anisotropy_matches) {
        set_error(error_message,
                 "ND_mix_vdf: both operands contribute scattering with different (or "
                 "graph-driven) anisotropy -- the exact same architectural ceiling as "
                 "ND_add_vdf above (Cycles' VolumeCoefficientsNode carries a single scalar "
                 "anisotropy for its whole coefficient bundle and cannot represent the "
                 "superposition of two independently-anisotropic phase functions)");
        return false;
      }
      result->anisotropy = a.anisotropy;
    }
    else if (a.has_scattering) {
      result->anisotropy = a.anisotropy;
    }
    else if (b.has_scattering) {
      result->anisotropy = b.anisotropy;
    }
    else {
      result->anisotropy = FloatInput();
    }

    const string synth_base = vdf.GetPath().GetString();
    result->absorption.link = mix_vector3(
        graph, a.absorption, b.absorption, mix_weight, synth_base + ".absorption");
    result->absorption.is_linked = true;

    result->scattering.link = mix_vector3(
        graph, a.scattering, b.scattering, mix_weight, synth_base + ".scattering");
    result->scattering.is_linked = true;
    result->has_scattering = a.has_scattering || b.has_scattering;

    for (const pxr::UsdShadeInput &input : vdf.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "fg" && name != "bg" && name != "mix") {
        set_error(error_message, string("ND_mix_vdf has no direct Cycles equivalent: ") + name);
        return false;
      }
    }
    return true;
  }

  set_error(error_message, "MaterialX VDF resolution reached an unhandled matched NodeDef");
  return false;
}

/** A resolved volumeshader terminal: the VDF coefficient bundle plus the
 *  optional emission color ND_volume's 'edf' input contributes -- the same
 *  shape read_volume_terminal() used to build inline, now factored out so
 *  ND_mix_volumeshader (mix_volumeshader_id below) can recurse on it. */
struct VolumeTerminal {
  VdfCoefficients coefficients;
  Color3Input emission;
};

/**
 * Task 3 TerminalRouter: volume terminal, generalized for recursion. Accepts
 * a bare VDF connected directly (via read_vdf_coefficients(), covering its
 * own ND_mix_vdf/ND_add_vdf/... combinators), the standard
 * ND_volume(vdf, edf) combinator, or ND_mix_volumeshader(fg, bg, mix)
 * lerping two such volumeshader terminals by a literal weight -- the
 * volumeshader-typed sibling of read_vdf_coefficients()'s mix_vdf_id branch,
 * hitting the exact same anisotropy-superposition ceiling and literal-weight
 * requirement. ND_volume's 'edf' (emission) input is an explicit, honest
 * boundary: if authored and connected to anything but ND_uniform_edf, this
 * fails closed rather than silently dropping emission.
 */
bool resolve_volume_terminal_source(const pxr::UsdShadeConnectableAPI &source,
                                    const pxr::TfToken &source_name,
                                    const pxr::UsdShadeAttributeType source_type,
                                    const pxr::SdfValueTypeName &expected_type,
                                    Graph *graph,
                                    const int depth,
                                    VolumeTerminal *result,
                                    string *error_message)
{
  if (depth > 64) {
    set_error(error_message, "MaterialX volumeshader graph nesting exceeds maximum depth");
    return false;
  }

  {
    std::unordered_set<string> active_endpoints;
    pxr::UsdShadeShader dot;
    if (resolve_connected_shader(source,
                                 source_name,
                                 source_type,
                                 dot_volumeshader_id,
                                 expected_type,
                                 &dot,
                                 &active_endpoints,
                                 0,
                                 nullptr))
    {
      const pxr::UsdShadeInput input = dot.GetInput(pxr::TfToken("in"));
      if (!input || !input.HasConnectedSource() || input.GetTypeName() != expected_type) {
        set_error(error_message, "ND_dot_volumeshader requires a connected volumeshader 'in' input");
        return false;
      }
      for (const pxr::UsdShadeInput &dot_input : dot.GetInputs()) {
        const string name = dot_input.GetBaseName().GetString();
        if (name != "in" && name != "note") {
          set_error(error_message,
                    string(dot_volumeshader_id) + " has no direct Cycles equivalent: " + name);
          return false;
        }
        if (name == "note" && dot_input.HasConnectedSource()) {
          set_error(error_message,
                    string(dot_volumeshader_id) + " note input must be a literal organization hint");
          return false;
        }
      }
      const auto sources = input.GetConnectedSources();
      if (sources.size() != 1) {
        set_error(error_message, "ND_dot_volumeshader 'in' input must have exactly one source");
        return false;
      }
      return resolve_volume_terminal_source(sources[0].source,
                                            sources[0].sourceName,
                                            sources[0].sourceType,
                                            input.GetTypeName(),
                                            graph,
                                            depth + 1,
                                            result,
                                            error_message);
    }
  }

  {
    std::unordered_set<string> active_endpoints;
    pxr::UsdShadeShader material;
    if (resolve_connected_shader(source,
                                 source_name,
                                 source_type,
                                 volume_material_id,
                                 expected_type,
                                 &material,
                                 &active_endpoints,
                                 0,
                                 nullptr))
    {
      const pxr::UsdShadeInput input = material.GetInput(pxr::TfToken("volumeshader"));
      if (!input || !input.HasConnectedSource()) {
        set_error(error_message, "ND_volumematerial requires a connected 'volumeshader' input");
        return false;
      }
      if (input.GetTypeName() != expected_type) {
        set_error(error_message,
                  "ND_volumematerial 'volumeshader' input must match the volume terminal type");
        return false;
      }
      for (const pxr::UsdShadeInput &material_input : material.GetInputs()) {
        const string name = material_input.GetBaseName().GetString();
        if (name != "volumeshader") {
          set_error(error_message,
                    string(volume_material_id) + " has no direct Cycles equivalent: " + name);
          return false;
        }
      }
      const auto sources = input.GetConnectedSources();
      if (sources.size() != 1) {
        set_error(error_message, "ND_volumematerial 'volumeshader' input must have exactly one source");
        return false;
      }
      return resolve_volume_terminal_source(sources[0].source,
                                            sources[0].sourceName,
                                            sources[0].sourceType,
                                            input.GetTypeName(),
                                            graph,
                                            depth + 1,
                                            result,
                                            error_message);
    }
  }

  {
    std::unordered_set<string> active_endpoints;
    pxr::UsdShadeShader mix;
    if (resolve_connected_shader(source,
                                 source_name,
                                 source_type,
                                 mix_volumeshader_id,
                                 expected_type,
                                 &mix,
                                 &active_endpoints,
                                 0,
                                 nullptr))
    {
      const pxr::UsdShadeInput fg = mix.GetInput(pxr::TfToken("fg"));
      const pxr::UsdShadeInput bg = mix.GetInput(pxr::TfToken("bg"));
      if (!fg || !fg.HasConnectedSource() || !bg || !bg.HasConnectedSource()) {
        set_error(error_message,
                 "ND_mix_volumeshader requires connected 'fg' and 'bg' volumeshader inputs");
        return false;
      }
      const auto fg_sources = fg.GetConnectedSources();
      const auto bg_sources = bg.GetConnectedSources();
      if (fg_sources.size() != 1 || bg_sources.size() != 1) {
        set_error(error_message, "ND_mix_volumeshader inputs must each have exactly one source");
        return false;
      }
      VolumeTerminal a;
      VolumeTerminal b;
      if (!resolve_volume_terminal_source(fg_sources[0].source,
                                          fg_sources[0].sourceName,
                                          fg_sources[0].sourceType,
                                          fg.GetTypeName(),
                                          graph,
                                          depth + 1,
                                          &a,
                                          error_message))
      {
        return false;
      }
      if (!resolve_volume_terminal_source(bg_sources[0].source,
                                          bg_sources[0].sourceName,
                                          bg_sources[0].sourceType,
                                          bg.GetTypeName(),
                                          graph,
                                          depth + 1,
                                          &b,
                                          error_message))
      {
        return false;
      }

      const pxr::UsdShadeInput mix_input = mix.GetInput(pxr::TfToken("mix"));
      if (!mix_input || mix_input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
        set_error(error_message, "ND_mix_volumeshader requires float input 'mix'");
        return false;
      }
      float mix_weight = 0.0f;
      if (mix_input.HasConnectedSource() || !mix_input.Get(&mix_weight)) {
        set_error(error_message,
                 "ND_mix_volumeshader requires a literal 'mix' weight: a dynamic/graph-driven "
                 "mix factor is an explicit, unsupported boundary this pass (mirrors "
                 "ND_mix_vdf's literal-weight requirement), not a silent narrowing");
        return false;
      }

      const VdfCoefficients &ca = a.coefficients;
      const VdfCoefficients &cb = b.coefficients;
      if (ca.has_scattering && cb.has_scattering) {
        const bool anisotropy_matches = !ca.anisotropy.is_linked && !cb.anisotropy.is_linked &&
                                        ca.anisotropy.value == cb.anisotropy.value;
        if (!anisotropy_matches) {
          set_error(error_message,
                   "ND_mix_volumeshader: both operands contribute scattering with different "
                   "(or graph-driven) anisotropy -- the exact same architectural ceiling as "
                   "ND_mix_vdf/ND_add_vdf (Cycles' VolumeCoefficientsNode carries a single "
                   "scalar anisotropy for its whole coefficient bundle)");
          return false;
        }
        result->coefficients.anisotropy = ca.anisotropy;
      }
      else if (ca.has_scattering) {
        result->coefficients.anisotropy = ca.anisotropy;
      }
      else if (cb.has_scattering) {
        result->coefficients.anisotropy = cb.anisotropy;
      }
      else {
        result->coefficients.anisotropy = FloatInput();
      }

      const string synth_base = mix.GetPath().GetString();
      result->coefficients.absorption.link = mix_vector3(
          graph, ca.absorption, cb.absorption, mix_weight, synth_base + ".absorption");
      result->coefficients.absorption.is_linked = true;
      result->coefficients.scattering.link = mix_vector3(
          graph, ca.scattering, cb.scattering, mix_weight, synth_base + ".scattering");
      result->coefficients.scattering.is_linked = true;
      result->coefficients.has_scattering = ca.has_scattering || cb.has_scattering;

      result->emission.link = mix_color3(
          graph, a.emission, b.emission, mix_weight, synth_base + ".emission");
      result->emission.is_linked = true;

      for (const pxr::UsdShadeInput &input : mix.GetInputs()) {
        const string name = input.GetBaseName().GetString();
        if (name != "fg" && name != "bg" && name != "mix") {
          set_error(error_message,
                    string("ND_mix_volumeshader has no direct Cycles equivalent: ") + name);
          return false;
        }
      }
      return true;
    }
  }

  bool matched_combinator = false;
  {
    std::unordered_set<string> active_endpoints;
    pxr::UsdShadeShader combinator;
    matched_combinator = resolve_connected_shader(source,
                                                  source_name,
                                                  source_type,
                                                  volume_combinator_id,
                                                  expected_type,
                                                  &combinator,
                                                  &active_endpoints,
                                                  0,
                                                  nullptr);
    if (matched_combinator) {
      const pxr::UsdShadeInput edf_input = combinator.GetInput(pxr::TfToken("edf"));
      if (edf_input && edf_input.HasConnectedSource()) {
        /* ND_uniform_edf (pbrlib/pbrlib_defs.mtlx) is the one EDF NodeDef
         * with a real, direct Cycles equivalent: its 'color' input maps
         * onto VolumeCoefficientsNode's existing "Emission Coefficients"
         * socket (previously always hardcoded to zero by lower(), see
         * graph.cpp). Any other EDF (directional_light, etc.) has no such
         * mapping and fails closed, same as unsupported VDFs. */
        const auto edf_sources = edf_input.GetConnectedSources();
        if (edf_sources.size() != 1) {
          set_error(error_message, "ND_volume 'edf' input must have exactly one source");
          return false;
        }
        pxr::UsdShadeShader edf;
        std::unordered_set<string> active_endpoints_edf;
        if (!resolve_connected_shader(edf_sources[0].source,
                                      edf_sources[0].sourceName,
                                      edf_sources[0].sourceType,
                                      uniform_edf_id,
                                      edf_input.GetTypeName(),
                                      &edf,
                                      &active_endpoints_edf,
                                      0,
                                      nullptr))
        {
          set_error(error_message,
                    "ND_volume 'edf' input has no direct Cycles equivalent: only "
                    "ND_uniform_edf is supported");
          return false;
        }
        if (!read_volume_emission_color(edf, graph, &result->emission, error_message)) {
          return false;
        }
        for (const pxr::UsdShadeInput &input : edf.GetInputs()) {
          const string name = input.GetBaseName().GetString();
          if (name != "color") {
            set_error(error_message,
                      string("ND_uniform_edf has no direct Cycles equivalent: ") + name);
            return false;
          }
        }
      }
      const pxr::UsdShadeInput vdf_input = combinator.GetInput(pxr::TfToken("vdf"));
      if (!vdf_input || !vdf_input.HasConnectedSource()) {
        set_error(error_message, "ND_volume requires a connected 'vdf' input");
        return false;
      }
      const auto vdf_sources = vdf_input.GetConnectedSources();
      if (vdf_sources.size() != 1) {
        set_error(error_message, "ND_volume 'vdf' input must have exactly one source");
        return false;
      }
      if (!read_vdf_coefficients(vdf_sources[0].source,
                                 vdf_sources[0].sourceName,
                                 vdf_sources[0].sourceType,
                                 vdf_input.GetTypeName(),
                                 graph,
                                 0,
                                 &result->coefficients,
                                 error_message))
      {
        return false;
      }
      for (const pxr::UsdShadeInput &input : combinator.GetInputs()) {
        const string name = input.GetBaseName().GetString();
        if (name != "vdf" && name != "edf") {
          set_error(error_message, string("ND_volume has no direct Cycles equivalent: ") + name);
          return false;
        }
      }
    }
  }
  if (!matched_combinator) {
    if (!read_vdf_coefficients(
            source, source_name, source_type, expected_type, graph, 0, &result->coefficients, error_message))
    {
      return false;
    }
  }
  return true;
}

/**
 * Task 3 TerminalRouter: volume terminal. Independently discovered -- unlike
 * the pre-Task-3 reader, this is never gated on a surface terminal existing.
 * Thin wrapper over resolve_volume_terminal_source() -- see its docstring
 * for the admitted shapes.
 */
bool read_volume_terminal(const pxr::UsdShadeMaterial &material,
                          Graph *graph,
                          bool *volume_present,
                          string *error_message)
{
  *volume_present = false;
  pxr::UsdShadeOutput output = material.GetVolumeOutput(mtlx_render_context);
  if (!output) {
    output = material.GetVolumeOutput();
  }
  if (!output || !output.HasConnectedSource()) {
    /* Optional terminal: absent/unconnected is not an error. */
    return true;
  }
  const auto sources = output.GetConnectedSources();
  if (sources.size() != 1) {
    set_error(error_message, "MaterialX volume output must have exactly one source");
    return false;
  }

  VolumeTerminal terminal;
  if (!resolve_volume_terminal_source(sources[0].source,
                                      sources[0].sourceName,
                                      sources[0].sourceType,
                                      output.GetTypeName(),
                                      graph,
                                      0,
                                      &terminal,
                                      error_message))
  {
    return false;
  }

  graph->volume_absorption = terminal.coefficients.absorption;
  graph->volume_scattering = terminal.coefficients.scattering;
  graph->volume_anisotropy = terminal.coefficients.anisotropy;
  graph->volume_emission = terminal.emission;
  *volume_present = true;
  return true;
}

/**
 * Task 3 TerminalRouter: lightshader terminal. A lightshader must never be
 * folded into the material's Surface output -- it is routed through the
 * light path instead. This function only discovers and authenticates it
 * (reachability/shape); binding it to a Light-object shading slot is
 * outside this reader's scope (see Graph::has_light).
 */
bool read_light_terminal(const pxr::UsdShadeMaterial &material,
                         string *light_node_name,
                         string *light_nodedef,
                         bool *light_present,
                         string *error_message)
{
  *light_present = false;
  pxr::UsdShadeOutput output = material.GetOutput(
      pxr::TfToken(mtlx_render_context.GetString() + ":light"));
  if (!output) {
    output = material.GetOutput(pxr::TfToken("light"));
  }
  if (!output || !output.HasConnectedSource()) {
    return true;
  }
  const auto sources = output.GetConnectedSources();
  if (sources.size() != 1) {
    set_error(error_message, "MaterialX light output must have exactly one source");
    return false;
  }
  pxr::UsdShadeShader light;
  if (!resolve_terminal_source_eliding_identity_dot(sources[0].source,
                                                   sources[0].sourceName,
                                                   sources[0].sourceType,
                                                   dot_lightshader_id,
                                                   output.GetTypeName(),
                                                   &light,
                                                   error_message))
  {
    if (error_message) {
      *error_message = string("MaterialX light: ") + *error_message;
    }
    return false;
  }
  pxr::TfToken id;
  light.GetShaderId(&id);
  if (id.GetString() == light_shader_id) {
    const pxr::UsdShadeInput edf = light.GetInput(pxr::TfToken("edf"));
    if (!edf || edf.GetTypeName() != pxr::SdfValueTypeNames->Token || !edf.HasConnectedSource()) {
      set_error(error_message, "ND_light requires a connected EDF 'edf' input");
      return false;
    }
    pxr::UsdShadeShader edf_shader;
    if (!connected_shader(edf, uniform_edf_id, &edf_shader, error_message)) {
      if (error_message) {
        *error_message = string("ND_light 'edf': ") + *error_message;
      }
      return false;
    }
    const pxr::UsdShadeInput color = edf_shader.GetInput(pxr::TfToken("color"));
    if (color) {
      pxr::GfVec3f value;
      if (color.GetTypeName() != pxr::SdfValueTypeNames->Color3f || color.HasConnectedSource() ||
          !color.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message,
                  "ND_light requires ND_uniform_edf 'color' to be a literal finite color3f");
        return false;
      }
    }
    for (const pxr::UsdShadeInput &edf_input : edf_shader.GetInputs()) {
      const string name = edf_input.GetBaseName().GetString();
      if (name != "color") {
        set_error(error_message, string(uniform_edf_id) + " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
    for (const char *float_name : {"intensity", "exposure"}) {
      const pxr::UsdShadeInput value_input = light.GetInput(pxr::TfToken(float_name));
      if (!value_input) {
        continue;
      }
      float value;
      if (value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  string(light_shader_id) + " requires literal finite " + float_name);
        return false;
      }
    }
    for (const pxr::UsdShadeInput &input : light.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "edf" && name != "intensity" && name != "exposure") {
        set_error(error_message, string(light_shader_id) + " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
  }
  else if (id.GetString() == point_light_id || id.GetString() == directional_light_id ||
           id.GetString() == spot_light_id)
  {
    const string nodedef = id.GetString();
    for (const pxr::UsdShadeInput &input : light.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      const bool expects_vector = name == "position" || name == "direction";
      const bool expects_color = name == "color";
      const bool expects_float = name == "intensity" || name == "decay_rate" ||
                                  name == "inner_angle" || name == "outer_angle";
      const bool allowed = (nodedef == point_light_id &&
                            (name == "position" || name == "color" ||
                             name == "intensity" || name == "decay_rate")) ||
                           (nodedef == directional_light_id &&
                            (name == "direction" || name == "color" || name == "intensity")) ||
                           (nodedef == spot_light_id &&
                            (name == "position" || name == "direction" || name == "color" ||
                             name == "intensity" || name == "decay_rate" ||
                             name == "inner_angle" || name == "outer_angle"));
      if (!allowed) {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return false;
      }
      if (input.HasConnectedSource()) {
        set_error(error_message, nodedef + " requires literal light input: " + name);
        return false;
      }
      if (expects_vector) {
        pxr::GfVec3f value;
        if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3 || !input.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
        {
          set_error(error_message, nodedef + " input '" + name + "' must be a finite vector3");
          return false;
        }
      }
      else if (expects_color) {
        pxr::GfVec3f value;
        if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f || !input.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
        {
          set_error(error_message, nodedef + " input '" + name + "' must be a finite color3f");
          return false;
        }
      }
      else if (expects_float) {
        float value;
        if (input.GetTypeName() != pxr::SdfValueTypeNames->Float || !input.Get(&value) ||
            !std::isfinite(value))
        {
          set_error(error_message, nodedef + " input '" + name + "' must be a finite float");
          return false;
        }
      }
    }
  }
  *light_node_name = light.GetPrim().GetName().GetString();
  *light_nodedef = id.GetString();
  *light_present = true;
  return true;
}

bool is_supported_open_pbr_input(const string &name)
{
  return name == "base_color" || name == "base_weight" || name == "base_metalness" ||
         name == "specular_ior" || name == "specular_roughness" ||
         name == "geometry_opacity" || name == "emission_color" ||
         name == "emission_luminance" || name == "geometry_normal" ||
         name == "geometry_coat_normal" || name == "coat_weight" || name == "coat_color" ||
         name == "coat_roughness" || name == "coat_ior" || name == "fuzz_weight" ||
         name == "fuzz_color" || name == "fuzz_roughness";
}

/* Real, verified against the bundled MaterialX 1.39
 * libraries/stdlib/stdlib_defs.mtlx ND_surface_unlit nodedef: the only five
 * inputs it declares (emission, emission_color, transmission,
 * transmission_color, opacity). surface_unlit is NOT an OpenPBR synonym --
 * it is a minimal emission/transmission surface with its own semantics
 * (see libraries/stdlib/genosl/mx_surface_unlit.osl, the reference
 * implementation this lowerer mirrors). */
bool is_supported_surface_unlit_input(const string &name)
{
  return name == "emission" || name == "emission_color" || name == "transmission" ||
         name == "transmission_color" || name == "opacity";
}



/* Real, verified against the bundled MaterialX 1.39
 * libraries/bxdf/usd_preview_surface.mtlx ND_UsdPreviewSurface_surfaceshader
 * nodedef: all 14 real inputs it declares. diffuseColor, metallic,
 * roughness, clearcoat, clearcoatRoughness, ior, and emissiveColor are
 * lowered to real Cycles Principled BSDF equivalents (connectable); the
 * rest (useSpecularWorkflow, specularColor, opacity, opacityMode,
 * opacityThreshold, normal, displacement, occlusion) are admitted here as
 * "known" real inputs but are further restricted below to their inert
 * default value, since none has a faithful Cycles equivalent yet. */
bool is_supported_usd_preview_surface_input(const string &name)
{
  return name == "diffuseColor" || name == "emissiveColor" ||
         name == "useSpecularWorkflow" || name == "specularColor" || name == "metallic" ||
         name == "roughness" || name == "clearcoat" || name == "clearcoatRoughness" ||
         name == "opacity" || name == "opacityMode" || name == "opacityThreshold" ||
         name == "ior" || name == "normal" || name == "displacement" || name == "occlusion";
}

/* Real, verified against the bundled MaterialX 1.39
 * libraries/bxdf/gltf_pbr.mtlx ND_gltf_pbr_surfaceshader nodedef: all 24
 * real inputs it declares. base_color, metallic, roughness, clearcoat,
 * clearcoat_roughness, ior, emissive, and emissive_strength are lowered to
 * real Cycles Principled BSDF equivalents (connectable); the rest are
 * admitted here as "known" real inputs but are further restricted below --
 * either to their inert default value (normal, tangent, occlusion,
 * transmission, specular, specular_color, alpha, alpha_mode, alpha_cutoff,
 * iridescence, sheen_color, clearcoat_normal, anisotropy_strength), or
 * accepted unconditionally because the real reference nodegraph never
 * wires them to the nodedef's "out" surfaceshader output at all
 * (iridescence_ior, iridescence_thickness, sheen_roughness,
 * anisotropy_rotation once their gating inputs are forced to their inert
 * default; thickness, attenuation_distance, attenuation_color, which only
 * ever feed a volume closure this surfaceshader-only nodedef never
 * outputs). */
bool is_supported_gltf_pbr_input(const string &name)
{
  return name == "base_color" || name == "metallic" || name == "roughness" ||
         name == "normal" || name == "tangent" || name == "occlusion" ||
         name == "transmission" || name == "specular" || name == "specular_color" ||
         name == "ior" || name == "alpha" || name == "alpha_mode" || name == "alpha_cutoff" ||
         name == "iridescence" || name == "iridescence_ior" || name == "iridescence_thickness" ||
         name == "sheen_color" || name == "sheen_roughness" || name == "clearcoat" ||
         name == "clearcoat_roughness" || name == "clearcoat_normal" || name == "emissive" ||
         name == "emissive_strength" || name == "thickness" ||
         name == "attenuation_distance" || name == "attenuation_color" ||
         name == "anisotropy_strength" || name == "anisotropy_rotation";
}

/* Real, verified against the bundled MaterialX 1.39
 * libraries/bxdf/disney_principled.mtlx ND_disney_principled nodedef: all
 * 14 real inputs it declares. Every one is lowered to a real Cycles
 * Principled BSDF equivalent -- see disney_principled_id's comment in
 * graph.cpp for the field-by-field mapping (and the two inputs,
 * specularTint and sheenTint, that need a real color MixNode rather than a
 * 1:1 socket correspondence). Unlike gltf_pbr/UsdPreviewSurface, no field
 * of this nodedef is admitted-but-inert -- ND_disney_principled has no
 * inputs this delivery phase leaves unmodeled. */
bool is_supported_disney_principled_input(const string &name)
{
  return name == "baseColor" || name == "metallic" || name == "roughness" ||
         name == "anisotropic" || name == "specular" || name == "specularTint" ||
         name == "sheen" || name == "sheenTint" || name == "clearcoat" ||
         name == "clearcoatGloss" || name == "specTrans" || name == "ior" ||
         name == "subsurface" || name == "subsurfaceDistance";
}

bool is_supported_standard_surface_input(const string &name)
{
  return name == "base" || name == "base_color" || name == "diffuse_roughness" ||
         name == "metalness" || name == "specular" || name == "specular_color" ||
         name == "specular_roughness" || name == "specular_IOR" ||
         name == "specular_anisotropy" || name == "specular_rotation" ||
         name == "transmission" || name == "transmission_color" || name == "subsurface" ||
         name == "subsurface_radius" || name == "subsurface_scale" ||
         name == "subsurface_anisotropy" || name == "sheen" || name == "sheen_color" ||
         name == "sheen_roughness" || name == "coat" || name == "coat_color" ||
         name == "coat_roughness" || name == "coat_IOR" || name == "thin_film_thickness" ||
         name == "thin_film_IOR" || name == "emission" || name == "emission_color" ||
         name == "opacity" || name == "thin_walled" || name == "normal" ||
         name == "coat_normal" || name == "tangent" ||
         /* Real ND_standard_surface_surfaceshader_100 inputs admitted only
          * at their inert real default value -- see the comment where these
          * are checked with require_default_float/require_default_color3
          * above. */
         name == "transmission_depth" || name == "transmission_scatter" ||
         name == "transmission_scatter_anisotropy" || name == "transmission_dispersion" ||
         name == "transmission_extra_roughness" || name == "subsurface_color" ||
         name == "coat_anisotropy" || name == "coat_rotation" ||
         name == "coat_affect_color" || name == "coat_affect_roughness";
}

bool read_boolean_terminal_input(const pxr::UsdShadeShader &surface,
                                 const char *input_name,
                                 Node *node,
                                 bool *has_supported_input,
                                 string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Bool || input.HasConnectedSource()) {
    set_error(error_message,
              string("standard_surface ") + input_name + " must be a literal bool");
    return false;
  }
  bool value;
  if (!input.Get(&value)) {
    set_error(error_message, string("standard_surface ") + input_name + " has no bool value");
    return false;
  }
  node->int_inputs[input_name] = value ? 1 : 0;
  *has_supported_input = true;
  return true;
}

bool read_vector3_terminal_input(const pxr::UsdShadeShader &surface,
                                 const char *input_name,
                                 Graph *graph,
                                 Node *node,
                                 bool *has_supported_input,
                                 std::unordered_map<string, string> *emitted_normalmap_shaders,
                                 string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
    set_error(error_message,
              string("standard_surface ") + input_name + " must have float3 type");
    return false;
  }
  if (input.HasConnectedSource()) {
    Link source;
    if (!read_normalmap_output(input, graph, &source, emitted_normalmap_shaders, error_message)) {
      return false;
    }
    node->links[input_name] = source;
  }
  else {
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, string("standard_surface ") + input_name + " has no float3 value");
      return false;
    }
    node->vector3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
  }
  *has_supported_input = true;
  return true;
}

/** Reject a real, admitted-but-out-of-scope input unless it is either
 *  unauthored/absent or authored with exactly its real nodedef default
 *  value (and not connected) -- the same "literal-default-only" delivery
 *  boundary already established for ND_surface_unlit's transmission/opacity,
 *  generalized here since ND_UsdPreviewSurface_surfaceshader and
 *  ND_gltf_pbr_surfaceshader each have several such inert-in-scope fields. */
bool require_default_float(const pxr::UsdShadeShader &surface,
                           const char *input_name,
                           const float default_value,
                           const char *model_name,
                           string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float || input.HasConnectedSource()) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent when connected (literal default only in "
                 "this delivery phase)");
    return false;
  }
  float value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent for a non-default value in this delivery "
                 "phase");
    return false;
  }
  return true;
}

bool require_default_int(const pxr::UsdShadeShader &surface,
                         const char *input_name,
                         const int default_value,
                         const char *model_name,
                         string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Int || input.HasConnectedSource()) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent when connected (literal default only in "
                 "this delivery phase)");
    return false;
  }
  int value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent for a non-default value in this delivery "
                 "phase");
    return false;
  }
  return true;
}

bool require_default_vector3(const pxr::UsdShadeShader &surface,
                             const char *input_name,
                             const pxr::GfVec3f &default_value,
                             const char *model_name,
                             string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3 || input.HasConnectedSource()) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent when connected (literal default only in "
                 "this delivery phase)");
    return false;
  }
  pxr::GfVec3f value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent for a non-default value in this delivery "
                 "phase");
    return false;
  }
  return true;
}

/** Like require_default_vector3, but for the real gltf_pbr inputs (normal,
 *  tangent, clearcoat_normal) that declare no literal "value" default at
 *  all -- only a `defaultgeomprop` (the real nodedef falls back to reading
 *  the named geometry primvar, e.g. Nworld/Tworld, when unconnected). An
 *  unconnected input of this kind is exactly Cycles' own default behavior
 *  (no override -- use the true shading normal/tangent), so there is no
 *  literal value to compare against; only a connected source is rejected
 *  as out of scope. */
bool require_default_color3(const pxr::UsdShadeShader &surface,
                            const char *input_name,
                            const pxr::GfVec3f &default_value,
                            const char *model_name,
                            string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f || input.HasConnectedSource()) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent when connected (literal default only in "
                 "this delivery phase)");
    return false;
  }
  pxr::GfVec3f value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent for a non-default value in this delivery "
                 "phase");
    return false;
  }
  return true;
}

bool require_unconnected_vector3(const pxr::UsdShadeShader &surface,
                                 const char *input_name,
                                 const char *model_name,
                                 string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.HasConnectedSource()) {
    set_error(error_message,
             string(model_name) + " " + input_name +
                 " has no direct Cycles equivalent when connected in this delivery phase");
    return false;
  }
  return true;
}

/* --- Generic <surface> closure-graph readers (hermes-generic-surface) --- */
bool read_surface_float_input(const pxr::UsdShadeShader &shader,
                              const char *nodedef,
                              const char *input_name,
                              Graph *graph,
                              Node *node,
                              std::unordered_map<string, string> *emitted_float_shaders,
                              std::unordered_map<string, string> *emitted_color4_shaders,
                              string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' must be a float");
    return false;
  }
  if (input.HasConnectedSource()) {
    Link source;
    std::unordered_set<string> active_shaders;
    if (!read_float_output(input,
                           graph,
                           &source,
                           &active_shaders,
                           emitted_float_shaders,
                           emitted_color4_shaders,
                           0,
                           error_message))
    {
      return false;
    }
    node->links[input_name] = source;
    return true;
  }
  float value;
  if (!input.Get(&value) || !std::isfinite(value)) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a finite float");
    return false;
  }
  node->inputs[input_name] = value;
  return true;
}

bool read_surface_color_input(const pxr::UsdShadeShader &shader,
                              const char *nodedef,
                              const char *input_name,
                              Graph *graph,
                              Node *node,
                              std::unordered_map<string, string> *emitted_float_shaders,
                              std::unordered_map<string, string> *emitted_color4_shaders,
                              string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' must be a color3f");
    return false;
  }
  if (input.HasConnectedSource()) {
    Link source;
    std::unordered_set<string> active_shaders;
    if (!read_color_output(input,
                           graph,
                           &source,
                           &active_shaders,
                           emitted_color4_shaders,
                           0,
                           error_message,
                           emitted_float_shaders))
    {
      return false;
    }
    node->links[input_name] = source;
    return true;
  }
  pxr::GfVec3f value;
  if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
      !std::isfinite(value[2]))
  {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a finite color3f");
    return false;
  }
  node->color3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
  return true;
}

bool read_surface_vector3_input(const pxr::UsdShadeShader &shader,
                                const char *nodedef,
                                const char *input_name,
                                Graph *graph,
                                Node *node,
                                string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' must be a vector3");
    return false;
  }
  if (input.HasConnectedSource()) {
    Link source;
    std::unordered_set<string> active_shaders;
    if (!read_vector3_output(input, graph, &source, &active_shaders, 0, error_message)) {
      return false;
    }
    node->links[input_name] = source;
    return true;
  }
  pxr::GfVec3f value;
  if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
      !std::isfinite(value[2]))
  {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a finite vector3");
    return false;
  }
  node->vector3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
  return true;
}

bool read_surface_vector2_literal_input(const pxr::UsdShadeShader &shader,
                                        const char *nodedef,
                                        const char *input_name,
                                        Node *node,
                                        string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float2 || input.HasConnectedSource()) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name +
                  "' must be a literal vector2 in this delivery phase");
    return false;
  }
  pxr::GfVec2f value;
  if (!input.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1])) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a finite vector2");
    return false;
  }
  node->vector2_inputs[input_name] = make_float2(value[0], value[1]);
  return true;
}

bool read_surface_string_literal_input(const pxr::UsdShadeShader &shader,
                                       const char *nodedef,
                                       const char *input_name,
                                       Node *node,
                                       string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->String || input.HasConnectedSource()) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a literal string");
    return false;
  }
  string value;
  if (!input.Get(&value)) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' has no string value");
    return false;
  }
  node->string_inputs[input_name] = value;
  return true;
}

bool read_surface_boolean_input(const pxr::UsdShadeShader &shader,
                                const char *nodedef,
                                const char *input_name,
                                Node *node,
                                string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Bool || input.HasConnectedSource()) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must be a literal boolean");
    return false;
  }
  bool value = false;
  if (!input.Get(&value)) {
    set_error(error_message,
              string(nodedef) + " input '" + input_name + "' must have a boolean value");
    return false;
  }
  node->int_inputs[input_name] = value ? 1 : 0;
  return true;
}

enum class SurfaceClosureKind {
  BSDF,
  EDF,
};

const char *surface_closure_kind_name(const SurfaceClosureKind kind)
{
  return kind == SurfaceClosureKind::BSDF ? "BSDF" : "EDF";
}

bool is_lama_leaf_bsdf(const string &nodedef)
{
  return nodedef == lama_diffuse_id || nodedef == lama_translucent_id || nodedef == lama_sss_id ||
         nodedef == lama_conductor_id || nodedef == lama_iridescence_id;
}

bool require_lama_default_float_input(const pxr::UsdShadeShader &shader,
                                      const char *nodedef,
                                      const char *input_name,
                                      const float default_value,
                                      string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Float || input.HasConnectedSource()) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' has no direct Cycles equivalent when connected");
    return false;
  }
  float value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' has no direct Cycles equivalent for a non-default value");
    return false;
  }
  return true;
}

bool require_lama_default_integer_input(const pxr::UsdShadeShader &shader,
                                        const char *nodedef,
                                        const char *input_name,
                                        const int default_value,
                                        string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Int || input.HasConnectedSource()) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' must be a literal integer default");
    return false;
  }
  int value;
  if (!input.Get(&value) || value != default_value) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' has no direct Cycles equivalent for a non-default value");
    return false;
  }
  return true;
}

bool require_lama_default_color_input(const pxr::UsdShadeShader &shader,
                                      const char *nodedef,
                                      const char *input_name,
                                      const float3 default_value,
                                      string *error_message)
{
  const pxr::UsdShadeInput input = shader.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f || input.HasConnectedSource()) {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' must be a literal color3f default");
    return false;
  }
  pxr::GfVec3f value;
  if (!input.Get(&value) || value[0] != default_value.x || value[1] != default_value.y ||
      value[2] != default_value.z)
  {
    set_error(error_message, string(nodedef) + " input '" + input_name + "' has no direct Cycles equivalent for a non-default value");
    return false;
  }
  return true;
}

bool surface_closure_kind(const string &nodedef, SurfaceClosureKind *kind)
{
  if (nodedef == oren_nayar_diffuse_bsdf_id || nodedef == translucent_bsdf_id ||
      nodedef == sheen_bsdf_id || nodedef == subsurface_bsdf_id ||
      nodedef == conductor_bsdf_id || nodedef == dielectric_bsdf_id ||
      nodedef == chiang_hair_bsdf_id || is_lama_leaf_bsdf(nodedef) ||
      nodedef == mix_bsdf_id || nodedef == add_bsdf_id ||
      nodedef == multiply_bsdff_id || nodedef == multiply_bsdfc_id ||
      nodedef == lama_add_bsdf_id || nodedef == lama_mix_bsdf_id)
  {
    *kind = SurfaceClosureKind::BSDF;
    return true;
  }
  if (nodedef == uniform_edf_id || nodedef == lama_emission_id || nodedef == mix_edf_id ||
      nodedef == add_edf_id || nodedef == lama_add_edf_id || nodedef == lama_mix_edf_id ||
      nodedef == multiply_edff_id || nodedef == multiply_edfc_id ||
      nodedef == generalized_schlick_edf_id) {
    *kind = SurfaceClosureKind::EDF;
    return true;
  }
  return false;
}

bool read_connected_surface_closure(
    const pxr::UsdShadeInput &input,
    const SurfaceClosureKind expected_kind,
    Graph *graph,
    Link *result,
    std::unordered_map<string, string> *emitted_float_shaders,
    std::unordered_map<string, string> *emitted_color4_shaders,
    std::unordered_map<string, string> *emitted_closure_shaders,
    std::unordered_set<string> *active_closure_shaders,
    string *error_message)
{
  if (!input) {
    set_error(error_message,
              string("ND_surface requires a connected ") + surface_closure_kind_name(expected_kind) +
                  " closure input");
    return false;
  }
  if (input.GetTypeName() != pxr::SdfValueTypeNames->Token) {
    set_error(error_message, "ND_surface closure input must be a token");
    return false;
  }
  if (!input.HasConnectedSource()) {
    set_error(error_message,
              string("ND_surface requires a connected ") + surface_closure_kind_name(expected_kind) +
                  " closure input");
    return false;
  }

  pxr::UsdShadeShader closure;
  if (!connected_shader(input, nullptr, &closure, error_message)) {
    return false;
  }

  const string shader_path = closure.GetPath().GetString();
  if (const auto emitted = emitted_closure_shaders->find(shader_path);
      emitted != emitted_closure_shaders->end())
  {
    *result = {emitted->second, "out", Type::SurfaceShader};
    return true;
  }
  if (!active_closure_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX surface closure graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_closure_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken closure_id;
  closure.GetShaderId(&closure_id);
  const string nodedef = closure_id.GetString();
  SurfaceClosureKind actual_kind;
  if (!surface_closure_kind(nodedef, &actual_kind) || actual_kind != expected_kind) {
    set_error(error_message,
              "ND_surface closure input has no direct Cycles equivalent: " + nodedef +
                  " cannot lower as " + surface_closure_kind_name(expected_kind));
    return finish(false);
  }

  Node closure_node;
  closure_node.name = unique_node_name(
      *graph, closure.GetPrim().GetName().GetString(), closure.GetPath().GetString());
  closure_node.nodedef = nodedef;
  closure_node.outputs["out"] = Type::SurfaceShader;
  if (closure_node.nodedef == oren_nayar_diffuse_bsdf_id) {
    if (!read_surface_float_input(closure,
                                  oren_nayar_diffuse_bsdf_id,
                                  "weight",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_color_input(closure,
                                  oren_nayar_diffuse_bsdf_id,
                                  "color",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_float_input(closure,
                                  oren_nayar_diffuse_bsdf_id,
                                  "roughness",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_vector3_input(closure,
                                    oren_nayar_diffuse_bsdf_id,
                                    "normal",
                                    graph,
                                    &closure_node,
                                    error_message) ||
        !read_surface_boolean_input(
            closure, oren_nayar_diffuse_bsdf_id, "energy_compensation", &closure_node, error_message))
    {
      return finish(false);
    }
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "weight" && name != "color" && name != "roughness" && name != "normal" &&
          name != "energy_compensation")
      {
        set_error(error_message,
                  string("ND_oren_nayar_diffuse_bsdf has no direct Cycles equivalent: ") + name);
        return finish(false);
      }
    }
  }
  else if (closure_node.nodedef == translucent_bsdf_id || closure_node.nodedef == sheen_bsdf_id ||
           closure_node.nodedef == subsurface_bsdf_id || closure_node.nodedef == conductor_bsdf_id ||
           closure_node.nodedef == dielectric_bsdf_id)
  {
    /* Real pbrlib BSDF leaves, verified against MaterialX 1.39
     * libraries/pbrlib/pbrlib_defs.mtlx and its genosl implementations:
     * translucent -> translucent_bsdf(normal,color); sheen -> sheen_bsdf
     * (only explicit mode="zeltner" validates in graph.cpp, matching Cycles'
     * SheenBsdfNode); subsurface -> subsurface_bssrdf; conductor ->
     * conductor_bsdf/MetallicBsdfNode physical-conductor; dielectric ->
     * dielectric_bsdf/GlassBsdfNode for the explicit scatter_mode="RT" case.
     * Unsupported siblings in this exact91 batch (Burley, generalized
     * Schlick, Chiang hair, layer, conical/measured/generalized EDF) remain
     * absent from surface_closure_kind() and fail closed by name. */
    const char *id = closure_node.nodedef.c_str();
    if (!read_surface_float_input(closure,
                                  id,
                                  "weight",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message))
    {
      return finish(false);
    }
    if (closure_node.nodedef == translucent_bsdf_id || closure_node.nodedef == sheen_bsdf_id ||
        closure_node.nodedef == subsurface_bsdf_id)
    {
      if (!read_surface_color_input(closure,
                                    id,
                                    "color",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message))
      {
        return finish(false);
      }
    }
    if (closure_node.nodedef == sheen_bsdf_id) {
      if (!read_surface_float_input(closure,
                                    id,
                                    "roughness",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_string_literal_input(closure, id, "mode", &closure_node, error_message))
      {
        return finish(false);
      }
    }
    if (closure_node.nodedef == subsurface_bsdf_id) {
      if (!read_surface_color_input(closure,
                                    id,
                                    "radius",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "anisotropy",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message))
      {
        return finish(false);
      }
    }
    if (closure_node.nodedef == conductor_bsdf_id) {
      if (!read_surface_color_input(closure,
                                    id,
                                    "ior",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_color_input(closure,
                                    id,
                                    "extinction",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_vector2_literal_input(closure, id, "roughness", &closure_node, error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "thinfilm_thickness",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "thinfilm_ior",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_string_literal_input(closure, id, "distribution", &closure_node, error_message))
      {
        return finish(false);
      }
    }
    if (closure_node.nodedef == dielectric_bsdf_id) {
      if (!read_surface_color_input(closure,
                                    id,
                                    "tint",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "ior",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_vector2_literal_input(closure, id, "roughness", &closure_node, error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "thinfilm_thickness",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_float_input(closure,
                                    id,
                                    "thinfilm_ior",
                                    graph,
                                    &closure_node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    error_message) ||
          !read_surface_string_literal_input(closure, id, "distribution", &closure_node, error_message) ||
          !read_surface_string_literal_input(closure, id, "scatter_mode", &closure_node, error_message))
      {
        return finish(false);
      }
    }
    if (!read_surface_vector3_input(closure, id, "normal", graph, &closure_node, error_message)) {
      return finish(false);
    }
    if (closure_node.nodedef == conductor_bsdf_id || closure_node.nodedef == dielectric_bsdf_id) {
      if (!read_surface_vector3_input(closure, id, "tangent", graph, &closure_node, error_message)) {
        return finish(false);
      }
    }
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "weight" && name != "color" && name != "roughness" && name != "normal" &&
          name != "mode" && name != "radius" && name != "anisotropy" && name != "ior" &&
          name != "extinction" && name != "thinfilm_thickness" && name != "thinfilm_ior" &&
          name != "tangent" && name != "distribution" && name != "tint" &&
          name != "scatter_mode")
      {
        set_error(error_message, closure_node.nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (closure_node.nodedef == chiang_hair_bsdf_id) {
    if (!read_surface_color_input(closure,
                                  chiang_hair_bsdf_id,
                                  "tint_R",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_color_input(closure,
                                  chiang_hair_bsdf_id,
                                  "tint_TT",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_color_input(closure,
                                  chiang_hair_bsdf_id,
                                  "tint_TRT",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_float_input(closure,
                                  chiang_hair_bsdf_id,
                                  "ior",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_vector2_literal_input(
            closure, chiang_hair_bsdf_id, "roughness_R", &closure_node, error_message) ||
        !read_surface_vector2_literal_input(
            closure, chiang_hair_bsdf_id, "roughness_TT", &closure_node, error_message) ||
        !read_surface_vector2_literal_input(
            closure, chiang_hair_bsdf_id, "roughness_TRT", &closure_node, error_message) ||
        !read_surface_float_input(closure,
                                  chiang_hair_bsdf_id,
                                  "cuticle_angle",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message) ||
        !read_surface_vector3_input(
            closure, chiang_hair_bsdf_id, "absorption_coefficient", graph, &closure_node, error_message) ||
        !read_surface_vector3_input(
            closure, chiang_hair_bsdf_id, "normal", graph, &closure_node, error_message) ||
        !read_surface_vector3_input(
            closure, chiang_hair_bsdf_id, "curve_direction", graph, &closure_node, error_message))
    {
      return finish(false);
    }
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "tint_R" && name != "tint_TT" && name != "tint_TRT" &&
          name != "ior" && name != "roughness_R" && name != "roughness_TT" &&
          name != "roughness_TRT" && name != "cuticle_angle" &&
          name != "absorption_coefficient" && name != "normal" && name != "curve_direction")
      {
        set_error(error_message, string(chiang_hair_bsdf_id) + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (is_lama_leaf_bsdf(closure_node.nodedef)) {
    /* LAMA leaf nodegraphs verified against MaterialX 1.39
     * libraries/bxdf/lama: diffuse -> oren_nayar with roughness^2*0.5
     * (only energyCompensation=0 is admitted because Cycles has no exposed
     * compensated Oren-Nayar closure); translucent -> translucent_bsdf
     * (roughness/energyCompensation are unused by the real reference graph);
     * sss -> subsurface_bsdf with radius*sssScale*sssUnitLength. */
    if (closure_node.nodedef == lama_diffuse_id) {
      const pxr::UsdShadeInput energy = closure.GetInput(pxr::TfToken("energyCompensation"));
      if (!energy || energy.GetTypeName() != pxr::SdfValueTypeNames->Float || energy.HasConnectedSource()) {
        set_error(error_message, string(lama_diffuse_id) + " requires literal energyCompensation=0.0 in this delivery phase");
        return finish(false);
      }
      float energy_value = 0.0f;
      if (!energy.Get(&energy_value) || energy_value != 0.0f) {
        set_error(error_message, string(lama_diffuse_id) + " requires energyCompensation=0.0 because Cycles exposes no compensated Oren-Nayar closure");
        return finish(false);
      }
      closure_node.inputs["energyCompensation"] = energy_value;
      const pxr::UsdShadeInput roughness = closure.GetInput(pxr::TfToken("roughness"));
      if (roughness) {
        float roughness_value = 0.0f;
        if (roughness.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            roughness.HasConnectedSource() || !roughness.Get(&roughness_value) ||
            !std::isfinite(roughness_value))
        {
          set_error(error_message, string(lama_diffuse_id) + " requires literal finite roughness because its reference graph squares the value");
          return finish(false);
        }
        closure_node.inputs["roughness"] = roughness_value;
      }
      if (!read_surface_color_input(closure, lama_diffuse_id, "color", graph, &closure_node, emitted_float_shaders, emitted_color4_shaders, error_message) ||
          !read_surface_vector3_input(closure, lama_diffuse_id, "normal", graph, &closure_node, error_message))
      {
        return finish(false);
      }
      for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
        const string name = closure_input.GetBaseName().GetString();
        if (name != "color" && name != "roughness" && name != "normal" && name != "energyCompensation") {
          set_error(error_message, string(lama_diffuse_id) + " has no direct Cycles equivalent: " + name);
          return finish(false);
        }
      }
    }
    else if (closure_node.nodedef == lama_translucent_id) {
      if (!read_surface_color_input(closure, lama_translucent_id, "color", graph, &closure_node, emitted_float_shaders, emitted_color4_shaders, error_message) ||
          !read_surface_vector3_input(closure, lama_translucent_id, "normal", graph, &closure_node, error_message) ||
          !require_lama_default_float_input(closure, lama_translucent_id, "roughness", 0.0f, error_message) ||
          !require_lama_default_float_input(closure, lama_translucent_id, "energyCompensation", 1.0f, error_message))
      {
        return finish(false);
      }
      for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
        const string name = closure_input.GetBaseName().GetString();
        if (name != "color" && name != "normal" && name != "roughness" && name != "energyCompensation") {
          set_error(error_message, string(lama_translucent_id) + " has no direct Cycles equivalent: " + name);
          return finish(false);
        }
      }
    }
    else if (closure_node.nodedef == lama_conductor_id) {
      int fresnel_mode = 0;
      const pxr::UsdShadeInput fresnel = closure.GetInput(pxr::TfToken("fresnelMode"));
      if (fresnel) {
        if (fresnel.GetTypeName() != pxr::SdfValueTypeNames->Int || fresnel.HasConnectedSource() ||
            !fresnel.Get(&fresnel_mode) || (fresnel_mode != 0 && fresnel_mode != 1))
        {
          set_error(error_message, string(lama_conductor_id) + " requires literal fresnelMode 0 or 1");
          return finish(false);
        }
      }
      /* Scientific mode feeds literal IOR/extinction into the same physical conductor
       * closure exposed by Cycles' MetallicBsdfNode. Artistic mode requires the
       * MaterialX artistic_ior transform plus the final BSDF tint; those are not
       * represented here without adding non-closure math around IOR/k or proxy closure
       * tinting, so they remain an explicit rejected boundary. */
      if (fresnel_mode != 1) {
        set_error(error_message, string(lama_conductor_id) + " has no direct Cycles equivalent for Artistic fresnelMode in this phase");
        return finish(false);
      }
      if (!require_lama_default_color_input(closure, lama_conductor_id, "tint", make_float3(1.0f, 1.0f, 1.0f), error_message) ||
          !require_lama_default_float_input(closure, lama_conductor_id, "anisotropy", 0.0f, error_message) ||
          !require_lama_default_float_input(closure, lama_conductor_id, "anisotropyRotation", 0.0f, error_message) ||
          !read_surface_vector3_input(closure, lama_conductor_id, "normal", graph, &closure_node, error_message))
      {
        return finish(false);
      }
      const pxr::UsdShadeInput ior = closure.GetInput(pxr::TfToken("IOR"));
      if (ior) {
        pxr::GfVec3f value;
        if (ior.GetTypeName() != pxr::SdfValueTypeNames->Float3 || ior.HasConnectedSource() ||
            !ior.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
            !std::isfinite(value[2]))
        {
          set_error(error_message, string(lama_conductor_id) + " requires literal finite IOR");
          return finish(false);
        }
        closure_node.color3_inputs["ior"] = make_float3(value[0], value[1], value[2]);
      }
      const pxr::UsdShadeInput extinction = closure.GetInput(pxr::TfToken("extinction"));
      if (extinction) {
        pxr::GfVec3f value;
        if (extinction.GetTypeName() != pxr::SdfValueTypeNames->Float3 ||
            extinction.HasConnectedSource() || !extinction.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
        {
          set_error(error_message, string(lama_conductor_id) + " requires literal finite extinction");
          return finish(false);
        }
        closure_node.color3_inputs["extinction"] = make_float3(value[0], value[1], value[2]);
      }
      const pxr::UsdShadeInput roughness = closure.GetInput(pxr::TfToken("roughness"));
      if (roughness) {
        float value = 0.0f;
        if (roughness.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            roughness.HasConnectedSource() || !roughness.Get(&value) || !std::isfinite(value))
        {
          set_error(error_message, string(lama_conductor_id) + " requires literal finite roughness");
          return finish(false);
        }
        closure_node.vector2_inputs["roughness"] = make_float2(value * value, value * value);
      }
      for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
        const string name = closure_input.GetBaseName().GetString();
        if (name != "tint" && name != "fresnelMode" && name != "IOR" &&
            name != "extinction" && name != "roughness" && name != "normal" &&
            name != "anisotropy" && name != "anisotropyDirection" &&
            name != "anisotropyRotation")
        {
          set_error(error_message, string(lama_conductor_id) + " has no direct Cycles equivalent: " + name);
          return finish(false);
        }
      }
    }
    else if (closure_node.nodedef == lama_iridescence_id) {
      /* MaterialX lama_iridescence lowers to a dielectric_bsdf with ior=1, scatter_mode=RT,
       * and thin-film sockets. Cycles GlassBsdfNode has the same IOR/thin-film/full-glass
       * closure, but no exposed anisotropic roughness/tangent controls (shader_nodes.cpp
       * GlassBsdfNode only has Color/Normal/Roughness/IOR/Thin Film), so only the
       * isotropic defaults are admitted. */
      if (!require_lama_default_float_input(closure, lama_iridescence_id, "anisotropy", 0.0f, error_message) ||
          !require_lama_default_float_input(closure, lama_iridescence_id, "anisotropyRotation", 0.0f, error_message))
      {
        return finish(false);
      }
      const pxr::UsdShadeInput roughness = closure.GetInput(pxr::TfToken("roughness"));
      if (roughness) {
        float value = 0.0f;
        if (roughness.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            roughness.HasConnectedSource() || !roughness.Get(&value) || !std::isfinite(value))
        {
          set_error(error_message, string(lama_iridescence_id) + " requires literal finite roughness");
          return finish(false);
        }
        closure_node.vector2_inputs["roughness"] = make_float2(value * value, value * value);
      }
      float relative = 0.5f;
      float min_thickness = 400.0f;
      float max_thickness = 800.0f;
      for (const auto [name, destination] : {std::pair{"relativeFilmThickness", &relative},
                                             std::pair{"minFilmThickness", &min_thickness},
                                             std::pair{"maxFilmThickness", &max_thickness}})
      {
        const pxr::UsdShadeInput input = closure.GetInput(pxr::TfToken(name));
        if (input && (input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
                      input.HasConnectedSource() || !input.Get(destination) ||
                      !std::isfinite(*destination)))
        {
          set_error(error_message, string(lama_iridescence_id) + " requires literal finite " + name);
          return finish(false);
        }
      }
      closure_node.inputs["thinfilm_thickness"] = min_thickness +
                                                   (max_thickness - min_thickness) * relative;
      const pxr::UsdShadeInput film_ior = closure.GetInput(pxr::TfToken("filmIOR"));
      if (film_ior) {
        float value = 0.0f;
        if (film_ior.GetTypeName() != pxr::SdfValueTypeNames->Float ||
            film_ior.HasConnectedSource() || !film_ior.Get(&value) || !std::isfinite(value))
        {
          set_error(error_message, string(lama_iridescence_id) + " requires literal finite filmIOR");
          return finish(false);
        }
        closure_node.inputs["thinfilm_ior"] = value;
      }
      for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
        const string name = closure_input.GetBaseName().GetString();
        if (name != "roughness" && name != "anisotropy" &&
            name != "anisotropyDirection" && name != "anisotropyRotation" &&
            name != "relativeFilmThickness" && name != "minFilmThickness" &&
            name != "maxFilmThickness" && name != "filmIOR")
        {
          set_error(error_message, string(lama_iridescence_id) + " has no direct Cycles equivalent: " + name);
          return finish(false);
        }
      }
    }
    else if (closure_node.nodedef == lama_sss_id) {
      const pxr::UsdShadeInput radius_input = closure.GetInput(pxr::TfToken("sssRadius"));
      if (radius_input) {
        pxr::GfVec3f value;
        if (radius_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
            radius_input.HasConnectedSource() || !radius_input.Get(&value) ||
            !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
        {
          set_error(error_message, string(lama_sss_id) + " requires literal finite sssRadius because its reference graph scales the radius");
          return finish(false);
        }
        closure_node.color3_inputs["sssRadius"] = make_float3(value[0], value[1], value[2]);
      }
      for (const char *scaled_input : {"sssScale", "sssUnitLength"}) {
        const pxr::UsdShadeInput input = closure.GetInput(pxr::TfToken(scaled_input));
        if (input) {
          float value = 0.0f;
          if (input.GetTypeName() != pxr::SdfValueTypeNames->Float || input.HasConnectedSource() ||
              !input.Get(&value) || !std::isfinite(value))
          {
            set_error(error_message, string(lama_sss_id) + " requires literal finite " + scaled_input + " because its reference graph scales the radius");
            return finish(false);
          }
          closure_node.inputs[scaled_input] = value;
        }
      }
      if (!read_surface_color_input(closure, lama_sss_id, "color", graph, &closure_node, emitted_float_shaders, emitted_color4_shaders, error_message) ||
          !read_surface_float_input(closure, lama_sss_id, "sssAnisotropy", graph, &closure_node, emitted_float_shaders, emitted_color4_shaders, error_message) ||
          !read_surface_vector3_input(closure, lama_sss_id, "normal", graph, &closure_node, error_message) ||
          !require_lama_default_integer_input(closure, lama_sss_id, "sssMode", 0, error_message) ||
          !require_lama_default_float_input(closure, lama_sss_id, "sssIOR", 1.0f, error_message) ||
          !require_lama_default_float_input(closure, lama_sss_id, "sssBleed", 0.0f, error_message) ||
          !require_lama_default_float_input(closure, lama_sss_id, "sssFollowTopology", 0.0f, error_message) ||
          !require_lama_default_integer_input(closure, lama_sss_id, "sssContinuationRays", 0, error_message) ||
          !require_lama_default_integer_input(closure, lama_sss_id, "mode", 0, error_message) ||
          !require_lama_default_integer_input(closure, lama_sss_id, "albedoInversionMethod", 0, error_message))
      {
        return finish(false);
      }
      const pxr::UsdShadeInput subset = closure.GetInput(pxr::TfToken("sssSubset"));
      if (subset) {
        string value;
        if (subset.GetTypeName() != pxr::SdfValueTypeNames->String || subset.HasConnectedSource() ||
            !subset.Get(&value) || !value.empty())
        {
          set_error(error_message, string(lama_sss_id) + " input 'sssSubset' has no direct Cycles equivalent for a non-default value");
          return finish(false);
        }
      }
      for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
        const string name = closure_input.GetBaseName().GetString();
        if (name != "color" && name != "normal" && name != "sssRadius" && name != "sssScale" &&
            name != "sssUnitLength" && name != "sssAnisotropy" && name != "sssMode" &&
            name != "sssIOR" && name != "sssBleed" && name != "sssFollowTopology" &&
            name != "sssSubset" && name != "sssContinuationRays" && name != "mode" &&
            name != "albedoInversionMethod")
        {
          set_error(error_message, string(lama_sss_id) + " has no direct Cycles equivalent: " + name);
          return finish(false);
        }
      }
    }
  }
  else if (closure_node.nodedef == uniform_edf_id || closure_node.nodedef == lama_emission_id) {
    if (!read_surface_color_input(closure,
                                  nodedef.c_str(),
                                  "color",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message))
    {
      return finish(false);
    }
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "color") {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (closure_node.nodedef == mix_bsdf_id || closure_node.nodedef == mix_edf_id ||
           closure_node.nodedef == lama_mix_bsdf_id || closure_node.nodedef == lama_mix_edf_id) {
    Link bg;
    Link fg;
    const pxr::TfToken bg_input(closure_node.nodedef == lama_mix_bsdf_id || closure_node.nodedef == lama_mix_edf_id ? "material1" : "bg");
    const pxr::TfToken fg_input(closure_node.nodedef == lama_mix_bsdf_id || closure_node.nodedef == lama_mix_edf_id ? "material2" : "fg");
    if (!read_connected_surface_closure(closure.GetInput(bg_input),
                                        expected_kind,
                                        graph,
                                        &bg,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message) ||
        !read_connected_surface_closure(closure.GetInput(fg_input),
                                        expected_kind,
                                        graph,
                                        &fg,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message) ||
        !read_surface_float_input(closure,
                                  nodedef.c_str(),
                                  "mix",
                                  graph,
                                  &closure_node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message))
    {
      return finish(false);
    }
    closure_node.links["bg"] = bg;
    closure_node.links["fg"] = fg;
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "fg" && name != "bg" && name != "material1" && name != "material2" && name != "mix") {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (closure_node.nodedef == multiply_bsdff_id || closure_node.nodedef == multiply_bsdfc_id ||
           closure_node.nodedef == multiply_edff_id || closure_node.nodedef == multiply_edfc_id) {
    /* Scale a BSDF or EDF (per `expected_kind`, already threaded generically
     * through this function) by a literal weight -- 'in1' is the
     * recursively-lowered closure subgraph, 'in2' is a literal-only float
     * (bsdfF/edfF) or literal uniform-channel color3 (bsdfC/edfC): see
     * graph.cpp's multiply_bsdff_id/multiply_bsdfc_id and multiply_edff_id/
     * multiply_edfc_id validate() cases for exactly why 'in2' cannot be
     * linked or, for the color3 flavor, non-uniform -- Cycles has no
     * per-channel closure-weighting primitive (MixClosureNode.fac is a
     * single scalar), matching the same real limitation already documented
     * for ND_multiply_vdfC in read_vdf_coefficients() above. */
    const bool color_weight = closure_node.nodedef == multiply_bsdfc_id ||
                              closure_node.nodedef == multiply_edfc_id;
    Link in1;
    if (!read_connected_surface_closure(closure.GetInput(pxr::TfToken("in1")),
                                        expected_kind,
                                        graph,
                                        &in1,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message))
    {
      return finish(false);
    }
    closure_node.links["in1"] = in1;

    const pxr::UsdShadeInput in2 = closure.GetInput(pxr::TfToken("in2"));
    if (color_weight) {
      if (!in2 || in2.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          in2.HasConnectedSource())
      {
        set_error(error_message,
                  nodedef + " weight ('in2') must be a literal color3f -- a "
                  "dynamic/graph-driven or non-uniform-only-representable closure tint is an "
                  "explicit, unsupported boundary this pass");
        return finish(false);
      }
      pxr::GfVec3f value;
      if (!in2.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message, nodedef + " requires a finite literal 'in2'");
        return finish(false);
      }
      if (value[0] != value[1] || value[1] != value[2]) {
        /* Cycles has no per-channel closure-weighting primitive -- only a
         * uniform (R==G==B) tint degenerates exactly to MixClosureNode.fac
         * with no loss. See graph.cpp's multiply_bsdfc_id/multiply_edfc_id
         * validate() comments for the rejected non-uniform-folding
         * alternative and why it was not implemented. */
        set_error(error_message,
                  nodedef + " 'in2' must be a uniform-channel (R==G==B) color3f -- "
                  "Cycles has no per-channel closure-weighting primitive");
        return finish(false);
      }
      closure_node.color3_inputs["in2"] = make_float3(value[0], value[1], value[2]);
    }
    else {
      if (!in2 || in2.GetTypeName() != pxr::SdfValueTypeNames->Float || in2.HasConnectedSource()) {
        set_error(error_message,
                  nodedef + " weight ('in2') must be a literal float -- a "
                  "dynamic/graph-driven closure scaling weight is an explicit, unsupported "
                  "boundary this pass");
        return finish(false);
      }
      float value;
      if (!in2.Get(&value) || !std::isfinite(value)) {
        set_error(error_message, nodedef + " requires a finite literal 'in2'");
        return finish(false);
      }
      closure_node.inputs["in2"] = value;
    }

    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "in1" && name != "in2") {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (closure_node.nodedef == generalized_schlick_edf_id) {
    Link base;
    if (!read_connected_surface_closure(closure.GetInput(pxr::TfToken("base")),
                                        expected_kind,
                                        graph,
                                        &base,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message))
    {
      return finish(false);
    }
    closure_node.links["base"] = base;

    for (const char *color_name : {"color0", "color90"}) {
      const pxr::UsdShadeInput color = closure.GetInput(pxr::TfToken(color_name));
      if (!color) {
        closure_node.color3_inputs[color_name] = make_float3(1.0f, 1.0f, 1.0f);
        continue;
      }
      if (color.GetTypeName() != pxr::SdfValueTypeNames->Color3f || color.HasConnectedSource()) {
        set_error(error_message,
                  string(generalized_schlick_edf_id) + " requires literal color input '" +
                      color_name + "' in the constant-scalar subset");
        return finish(false);
      }
      pxr::GfVec3f value;
      if (!color.Get(&value) || !std::isfinite(value[0]) || !std::isfinite(value[1]) ||
          !std::isfinite(value[2]))
      {
        set_error(error_message,
                  string(generalized_schlick_edf_id) + " requires finite color input '" +
                      color_name + "'");
        return finish(false);
      }
      closure_node.color3_inputs[color_name] = make_float3(value[0], value[1], value[2]);
    }
    const float3 color0 = closure_node.color3_inputs.at("color0");
    const float3 color90 = closure_node.color3_inputs.at("color90");
    if (color0 != color90 || color0.x != color0.y || color0.y != color0.z) {
      set_error(error_message,
                string(generalized_schlick_edf_id) +
                    " only supports the constant uniform-channel subset (color0 == color90, R==G==B)");
      return finish(false);
    }

    const pxr::UsdShadeInput exponent = closure.GetInput(pxr::TfToken("exponent"));
    if (exponent) {
      float value = 0.0f;
      if (exponent.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          exponent.HasConnectedSource() || !exponent.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  string(generalized_schlick_edf_id) +
                      " requires literal finite exponent in the constant-scalar subset");
        return finish(false);
      }
      closure_node.inputs["exponent"] = value;
    }
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "base" && name != "color0" && name != "color90" && name != "exponent") {
        set_error(error_message,
                  string(generalized_schlick_edf_id) + " has no direct Cycles equivalent: " +
                      name);
        return finish(false);
      }
    }
  }
  else {
    Link in1;
    Link in2;
    const bool lama_add = closure_node.nodedef == lama_add_bsdf_id || closure_node.nodedef == lama_add_edf_id;
    const pxr::TfToken in1_input(lama_add ? "material1" : "in1");
    const pxr::TfToken in2_input(lama_add ? "material2" : "in2");
    if (lama_add) {
      for (const char *weight_name : {"weight1", "weight2"}) {
        const pxr::UsdShadeInput weight = closure.GetInput(pxr::TfToken(weight_name));
        if (!weight) {
          closure_node.inputs[weight_name] = string(weight_name) == "weight1" ? 1.0f : 0.0f;
          continue;
        }
        float value = 0.0f;
        if (weight.GetTypeName() != pxr::SdfValueTypeNames->Float || weight.HasConnectedSource() ||
            !weight.Get(&value) || !std::isfinite(value))
        {
          set_error(error_message, nodedef + " requires literal finite " + weight_name +
                                  " because LAMA add lowers each weighted closure with a MixClosureNode factor");
          return finish(false);
        }
        closure_node.inputs[weight_name] = value;
      }
    }
    if (!read_connected_surface_closure(closure.GetInput(in1_input),
                                        expected_kind,
                                        graph,
                                        &in1,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message) ||
        !read_connected_surface_closure(closure.GetInput(in2_input),
                                        expected_kind,
                                        graph,
                                        &in2,
                                        emitted_float_shaders,
                                        emitted_color4_shaders,
                                        emitted_closure_shaders,
                                        active_closure_shaders,
                                        error_message))
    {
      return finish(false);
    }
    closure_node.links["in1"] = in1;
    closure_node.links["in2"] = in2;
    for (const pxr::UsdShadeInput &closure_input : closure.GetInputs()) {
      const string name = closure_input.GetBaseName().GetString();
      if (name != "in1" && name != "in2" && name != "material1" && name != "material2" &&
          name != "weight1" && name != "weight2")
      {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }

  *result = {closure_node.name, "out", Type::SurfaceShader};
  emitted_closure_shaders->emplace(shader_path, closure_node.name);
  graph->nodes.push_back(std::move(closure_node));
  return finish(true);
}

bool read_surface_closure_input(const pxr::UsdShadeShader &surface,
                                const char *input_name,
                                const SurfaceClosureKind expected_kind,
                                Graph *graph,
                                Node *node,
                                std::unordered_map<string, string> *emitted_float_shaders,
                                std::unordered_map<string, string> *emitted_color4_shaders,
                                std::unordered_map<string, string> *emitted_closure_shaders,
                                string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken(input_name));
  if (!input) {
    return true;
  }
  if (!input.HasConnectedSource()) {
    return true;
  }
  Link closure;
  std::unordered_set<string> active_closure_shaders;
  if (!read_connected_surface_closure(input,
                                      expected_kind,
                                      graph,
                                      &closure,
                                      emitted_float_shaders,
                                      emitted_color4_shaders,
                                      emitted_closure_shaders,
                                      &active_closure_shaders,
                                      error_message))
  {
    return false;
  }
  node->links[input_name] = closure;
  return true;
}

bool read_connected_unit_opacity_surface_shader(
    const pxr::UsdShadeInput &input,
    Graph *graph,
    Link *result,
    std::unordered_map<string, string> *emitted_float_shaders,
    std::unordered_map<string, string> *emitted_color4_shaders,
    std::unordered_map<string, string> *emitted_closure_shaders,
    std::unordered_map<string, string> *emitted_surface_shaders,
    std::unordered_set<string> *active_surface_shaders,
    string *error_message)
{
  if (!input || input.GetTypeName() != pxr::SdfValueTypeNames->Token || !input.HasConnectedSource()) {
    set_error(error_message, "ND_mix_surfaceshader requires connected surfaceshader inputs");
    return false;
  }

  pxr::UsdShadeShader surface;
  if (!connected_shader_eliding_identity_dot(input, dot_surfaceshader_id, &surface, error_message)) {
    return false;
  }
  const string shader_path = surface.GetPath().GetString();
  if (const auto emitted = emitted_surface_shaders->find(shader_path);
      emitted != emitted_surface_shaders->end())
  {
    *result = {emitted->second, "out", Type::SurfaceShader};
    return true;
  }
  if (!active_surface_shaders->insert(shader_path).second) {
    set_error(error_message, "MaterialX surfaceshader graph connection is cyclic");
    return false;
  }
  const auto finish = [&](const bool success) {
    active_surface_shaders->erase(shader_path);
    return success;
  };

  pxr::TfToken surface_id;
  surface.GetShaderId(&surface_id);
  const string nodedef = surface_id.GetString();
  Node node;
  node.name = unique_node_name(*graph, surface.GetPrim().GetName().GetString(), shader_path);
  node.nodedef = nodedef;
  node.outputs["out"] = Type::SurfaceShader;

  if (nodedef == generic_surface_id) {
    if (!read_surface_closure_input(surface,
                                    "bsdf",
                                    SurfaceClosureKind::BSDF,
                                    graph,
                                    &node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    emitted_closure_shaders,
                                    error_message) ||
        !read_surface_closure_input(surface,
                                    "edf",
                                    SurfaceClosureKind::EDF,
                                    graph,
                                    &node,
                                    emitted_float_shaders,
                                    emitted_color4_shaders,
                                    emitted_closure_shaders,
                                    error_message))
    {
      return finish(false);
    }
    if (!node.links.contains("bsdf") && !node.links.contains("edf")) {
      set_error(error_message, "ND_mix_surfaceshader input ND_surface requires a connected bsdf or edf");
      return finish(false);
    }
    const pxr::UsdShadeInput opacity = surface.GetInput(pxr::TfToken("opacity"));
    if (opacity && opacity.HasConnectedSource()) {
      set_error(error_message,
                "ND_mix_surfaceshader cannot yet mix non-unit surfaceshader opacity: " + nodedef);
      return finish(false);
    }
    if (opacity) {
      float value = 1.0f;
      if (opacity.GetTypeName() != pxr::SdfValueTypeNames->Float || !opacity.Get(&value) ||
          value != 1.0f)
      {
        set_error(error_message,
                  "ND_mix_surfaceshader currently admits only unit-opacity ND_surface inputs");
        return finish(false);
      }
    }
    const pxr::UsdShadeInput thin_walled = surface.GetInput(pxr::TfToken("thin_walled"));
    if (thin_walled && (!read_surface_boolean_input(surface, nodedef.c_str(), "thin_walled", &node,
                                                    error_message) ||
                        node.int_inputs.at("thin_walled") != 0))
    {
      set_error(error_message, "ND_surface thin_walled has no Cycles equivalent in mix_surfaceshader");
      return finish(false);
    }
    for (const pxr::UsdShadeInput &surface_input : surface.GetInputs()) {
      const string name = surface_input.GetBaseName().GetString();
      if (name != "bsdf" && name != "edf" && name != "opacity" && name != "thin_walled") {
        set_error(error_message, nodedef + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else if (nodedef == mix_surfaceshader_id) {
    Link bg;
    Link fg;
    if (!read_connected_unit_opacity_surface_shader(surface.GetInput(pxr::TfToken("bg")),
                                                    graph,
                                                    &bg,
                                                    emitted_float_shaders,
                                                    emitted_color4_shaders,
                                                    emitted_closure_shaders,
                                                    emitted_surface_shaders,
                                                    active_surface_shaders,
                                                    error_message) ||
        !read_connected_unit_opacity_surface_shader(surface.GetInput(pxr::TfToken("fg")),
                                                    graph,
                                                    &fg,
                                                    emitted_float_shaders,
                                                    emitted_color4_shaders,
                                                    emitted_closure_shaders,
                                                    emitted_surface_shaders,
                                                    active_surface_shaders,
                                                    error_message) ||
        !read_surface_float_input(surface,
                                  mix_surfaceshader_id,
                                  "mix",
                                  graph,
                                  &node,
                                  emitted_float_shaders,
                                  emitted_color4_shaders,
                                  error_message))
    {
      return finish(false);
    }
    node.links["bg"] = bg;
    node.links["fg"] = fg;
    for (const pxr::UsdShadeInput &surface_input : surface.GetInputs()) {
      const string name = surface_input.GetBaseName().GetString();
      if (name != "fg" && name != "bg" && name != "mix") {
        set_error(error_message, string(mix_surfaceshader_id) + " has no direct Cycles equivalent: " + name);
        return finish(false);
      }
    }
  }
  else {
    set_error(error_message,
              "ND_mix_surfaceshader input has no registered unit-opacity surfaceshader lowerer: " +
                  nodedef);
    return finish(false);
  }

  graph->nodes.push_back(std::move(node));
  emitted_surface_shaders->emplace(shader_path, graph->nodes.back().name);
  *result = {graph->nodes.back().name, "out", Type::SurfaceShader};
  return finish(true);
}

/** Real ND_convert_*_surfaceshader semantic lowerer: reads the single real
 *  `in` input (its declared type is exactly determined by `convert_id`,
 *  matching the real nodedef in libraries/stdlib/stdlib_defs.mtlx) and
 *  populates `unlit` with the emission_color (and, for color4/vector4,
 *  opacity) that the real NG_convert_<type>_surfaceshader reference
 *  nodegraph in stdlib_ng.mtlx computes. Connected inputs are resolved by
 *  the existing typed value readers and inserted into `graph` before the
 *  constructed terminal surface is committed. */
bool read_convert_to_surfaceshader_in(const pxr::UsdShadeShader &surface,
                                      const string &convert_id,
                                      Graph *graph,
                                      Node *unlit,
                                      std::unordered_map<string, string> *emitted_float_shaders,
                                      std::unordered_map<string, string> *emitted_color4_shaders,
                                      string *error_message)
{
  const pxr::UsdShadeInput input = surface.GetInput(pxr::TfToken("in"));
  if (!input) {
    set_error(error_message, convert_id + " has no 'in' value");
    return false;
  }
  if (convert_id == convert_color3_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Color3f) {
      set_error(error_message, convert_id + " 'in' must have color3f type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      if (!read_color_output(input,
                             graph,
                             &value,
                             &active_shaders,
                             emitted_color4_shaders,
                             0,
                             error_message,
                             emitted_float_shaders))
      {
        return false;
      }
      unlit->links["emission_color"] = value;
      return true;
    }
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no color3f value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value[0], value[1], value[2]);
    return true;
  }
  if (convert_id == convert_color4_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Color4f) {
      set_error(error_message, convert_id + " 'in' must have color4f type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      if (!read_color4_output(input, graph, &value, &active_shaders, emitted_color4_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_color4_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      Node opacity;
      opacity.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".opacity",
                                      surface.GetPath().GetString() + ".opacity");
      opacity.nodedef = extract_color4_id;
      opacity.links["in"] = value;
      opacity.int_inputs["index"] = 3;
      opacity.outputs["out"] = Type::Float;
      unlit->links["opacity"] = {opacity.name, "out", Type::Float};
      graph->nodes.push_back(std::move(convert));
      graph->nodes.push_back(std::move(opacity));
      return true;
    }
    pxr::GfVec4f value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no color4f value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value[0], value[1], value[2]);
    unlit->inputs["opacity"] = value[3];
    return true;
  }
  if (convert_id == convert_float_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, convert_id + " 'in' must have float type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      if (!read_float_output(input,
                             graph,
                             &value,
                             &active_shaders,
                             emitted_float_shaders,
                             emitted_color4_shaders,
                             0,
                             error_message))
      {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_float_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return true;
    }
    float value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no float value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value, value, value);
    return true;
  }
  if (convert_id == convert_vector2_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Float2) {
      set_error(error_message, convert_id + " 'in' must have float2 type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      if (!read_vector2_output(input, graph, &value, &active_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_vector2_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return true;
    }
    pxr::GfVec2f value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no float2 value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value[0], value[1], 0.0f);
    return true;
  }
  if (convert_id == convert_vector3_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Float3) {
      set_error(error_message, convert_id + " 'in' must have float3 type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      if (!read_vector3_output(input, graph, &value, &active_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_vector3_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return true;
    }
    pxr::GfVec3f value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no float3 value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value[0], value[1], value[2]);
    return true;
  }
  if (convert_id == convert_vector4_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Float4) {
      set_error(error_message, convert_id + " 'in' must have float4 type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      std::unordered_map<string, string> emitted_vector4_shaders;
      if (!read_vector4_output(input, graph, &value, &active_shaders, &emitted_vector4_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_vector4_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      Node opacity;
      opacity.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".opacity",
                                      surface.GetPath().GetString() + ".opacity");
      opacity.nodedef = extract_vector4_id;
      opacity.links["in"] = value;
      opacity.int_inputs["index"] = 3;
      opacity.outputs["out"] = Type::Float;
      unlit->links["opacity"] = {opacity.name, "out", Type::Float};
      graph->nodes.push_back(std::move(convert));
      graph->nodes.push_back(std::move(opacity));
      return true;
    }
    pxr::GfVec4f value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no float4 value");
      return false;
    }
    unlit->color3_inputs["emission_color"] = make_float3(value[0], value[1], value[2]);
    unlit->inputs["opacity"] = value[3];
    return true;
  }
  if (convert_id == convert_integer_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Int) {
      set_error(error_message, convert_id + " 'in' must have int type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      std::unordered_map<string, string> emitted_integer_shaders;
      if (!read_integer_output(input, graph, &value, &active_shaders, &emitted_integer_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_integer_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return true;
    }
    int value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no int value");
      return false;
    }
    const float f = float(value);
    unlit->color3_inputs["emission_color"] = make_float3(f, f, f);
    return true;
  }
  if (convert_id == convert_boolean_surfaceshader_id) {
    if (input.GetTypeName() != pxr::SdfValueTypeNames->Bool) {
      set_error(error_message, convert_id + " 'in' must have bool type");
      return false;
    }
    if (input.HasConnectedSource()) {
      Link value;
      std::unordered_set<string> active_shaders;
      std::unordered_map<string, string> emitted_boolean_shaders;
      if (!read_boolean_output(input, graph, &value, &active_shaders, &emitted_boolean_shaders, 0, error_message)) {
        return false;
      }
      Node convert;
      convert.name = unique_node_name(*graph,
                                      surface.GetPrim().GetName().GetString() + ".emission_color",
                                      surface.GetPath().GetString() + ".emission_color");
      convert.nodedef = convert_boolean_color3_id;
      convert.links["in"] = value;
      convert.outputs["out"] = Type::Color3;
      unlit->links["emission_color"] = {convert.name, "out", Type::Color3};
      graph->nodes.push_back(std::move(convert));
      return true;
    }
    bool value;
    if (!input.Get(&value)) {
      set_error(error_message, convert_id + " 'in' has no bool value");
      return false;
    }
    const float f = value ? 1.0f : 0.0f;
    unlit->color3_inputs["emission_color"] = make_float3(f, f, f);
    return true;
  }
  set_error(error_message, convert_id + " is not a recognized convert-to-surfaceshader NodeDef");
  return false;
}

}  // namespace

bool read_usdshade_graph(const pxr::UsdShadeMaterial &material,
                         Graph *graph,
                         string *error_message)
{
  if (!material || !graph) {
    set_error(error_message, "A valid USDShade material and destination graph are required");
    return false;
  }

  /* Task 3 TerminalEnumerator: every authored terminal is discovered
   * independently. Previously this function returned immediately when no
   * surface terminal was connected, which meant a volume-only (or
   * displacement-only, or light-only) material was silently rejected even
   * though its volume/displacement/light content was completely valid --
   * this is the volume early-return the compiler is meant to remove. All
   * terminals are still committed atomically: nothing is written to
   * `*graph` until every authored terminal below has independently
   * validated. */
  Graph parsed;
  bool has_any_terminal = false;

  string light_node_name;
  string light_nodedef;
  bool light_present = false;
  if (!read_light_terminal(material, &light_node_name, &light_nodedef, &light_present, error_message)) {
    return false;
  }
  if (light_present) {
    has_any_terminal = true;
  }

  pxr::UsdShadeOutput surface_output = material.GetSurfaceOutput(mtlx_render_context);
  if (!surface_output) {
    surface_output = material.GetSurfaceOutput();
  }
  const bool surface_present = surface_output && surface_output.HasConnectedSource();

  Node open_pbr;
  string open_pbr_path_for_naming;
  Node standard_surface;
  string standard_surface_path_for_naming;
  Node unlit;
  string unlit_path_for_naming;
  Node usd_preview;
  string usd_preview_path_for_naming;
  Node gltf_pbr_node;
  string gltf_pbr_path_for_naming;
  Node disney_principled;
  string disney_principled_path_for_naming;
  Node generic_surface;
  string generic_surface_path_for_naming;
  Node mix_surface;
  string mix_surface_path_for_naming;
  Node lama_surface;
  string lama_surface_path_for_naming;
  bool committed_is_open_pbr_family = false;
  bool committed_is_standard_surface = false;
  bool committed_is_usd_preview_surface = false;
  bool committed_is_gltf_pbr = false;
  bool committed_is_disney_principled = false;
  bool committed_is_generic_surface = false;
  bool committed_is_mix_surface = false;
  bool committed_is_lama_surface = false;
  bool has_supported_input = false;
  std::unordered_map<string, string> emitted_float_shaders;
  std::unordered_map<string, string> emitted_color4_shaders;
  std::unordered_map<string, string> emitted_closure_shaders;
  std::unordered_map<string, string> emitted_normalmap_shaders;

  if (surface_present) {
  const auto surface_sources = surface_output.GetConnectedSources();
  if (surface_sources.size() != 1) {
    set_error(error_message, "USDShade MaterialX surface output has an invalid source");
    return false;
  }

  pxr::UsdShadeConnectableAPI effective_surface_source = surface_sources[0].source;
  pxr::TfToken effective_surface_source_name = surface_sources[0].sourceName;
  pxr::UsdShadeAttributeType effective_surface_source_type = surface_sources[0].sourceType;
  pxr::SdfValueTypeName effective_surface_type = surface_output.GetTypeName();

  {
    std::unordered_set<string> active_endpoints;
    pxr::UsdShadeShader surface_material;
    if (resolve_connected_shader(effective_surface_source,
                                 effective_surface_source_name,
                                 effective_surface_source_type,
                                 surface_material_id,
                                 effective_surface_type,
                                 &surface_material,
                                 &active_endpoints,
                                 0,
                                 nullptr))
    {
      const pxr::UsdShadeInput front = surface_material.GetInput(pxr::TfToken("surfaceshader"));
      if (!front || !front.HasConnectedSource()) {
        set_error(error_message, "ND_surfacematerial requires a connected 'surfaceshader' input");
        return false;
      }
      if (front.GetTypeName() != effective_surface_type) {
        set_error(error_message,
                  "ND_surfacematerial 'surfaceshader' input must match the material surface terminal type");
        return false;
      }
      const pxr::UsdShadeInput back = surface_material.GetInput(pxr::TfToken("backsurfaceshader"));
      if (back && back.HasConnectedSource()) {
        set_error(error_message,
                  "ND_surfacematerial backsurfaceshader has no direct Cycles equivalent in this "
                  "reader (single Surface output only)");
        return false;
      }
      const pxr::UsdShadeInput displacement = surface_material.GetInput(
          pxr::TfToken("displacementshader"));
      if (displacement && displacement.HasConnectedSource()) {
        pxr::UsdShadeShader displacement_source;
        if (!connected_shader_eliding_identity_dot(
                displacement, dot_displacementshader_id, &displacement_source, error_message))
        {
          return false;
        }
        DisplacementValue displacement_value;
        std::unordered_set<string> active_displacement_shaders;
        if (!read_displacement_shader(displacement_source,
                                      &parsed,
                                      &displacement_value,
                                      &emitted_float_shaders,
                                      &active_displacement_shaders,
                                      error_message))
        {
          return false;
        }
        parsed.displacement_is_vector3 = displacement_value.is_vector3;
        if (displacement_value.is_vector3) {
          parsed.displacement_vector3 = displacement_value.vector;
        }
        else {
          parsed.displacement = displacement_value.scalar;
        }
        parsed.displacement_scale = displacement_value.scale;
        parsed.has_displacement = true;
      }
      for (const pxr::UsdShadeInput &material_input : surface_material.GetInputs()) {
        const string name = material_input.GetBaseName().GetString();
        if (name != "surfaceshader" && name != "backsurfaceshader" &&
            name != "displacementshader")
        {
          set_error(error_message,
                    string(surface_material_id) + " has no direct Cycles equivalent: " + name);
          return false;
        }
      }
      const auto front_sources = front.GetConnectedSources();
      if (front_sources.size() != 1) {
        set_error(error_message,
                  "ND_surfacematerial 'surfaceshader' input must have exactly one source");
        return false;
      }
      effective_surface_source = front_sources[0].source;
      effective_surface_source_name = front_sources[0].sourceName;
      effective_surface_source_type = front_sources[0].sourceType;
      effective_surface_type = front.GetTypeName();
    }
  }

  pxr::UsdShadeShader surface;
  if (!resolve_terminal_source_eliding_identity_dot(effective_surface_source,
                                                   effective_surface_source_name,
                                                   effective_surface_source_type,
                                                   dot_surfaceshader_id,
                                                   effective_surface_type,
                                                   &surface,
                                                   error_message))
  {
    if (error_message) {
      *error_message = string("USDShade MaterialX surface: ") + *error_message;
    }
    return false;
  }

  /* Task 3 NodeDefProvider: admit ND_open_pbr_surface_surfaceshader itself,
   * or any NodeDef that explicitly declares (via the single-hop
   * `info:mtlx:inherit` attribute) that it inherits from it. Any other
   * surface model -- Standard Surface, surface_unlit, glTF PBR, Disney,
   * ... -- authenticates as a real, reachable surfaceshader terminal but
   * has no registered Semantic Lowerer yet in this delivery phase, and
   * fails closed with a complete, named boundary record rather than being
   * silently coerced into OpenPBR or crashing. Registry-driven support for
   * every exact91 surface model is Phase 6 per the design spec, not Task 3. */
  pxr::TfToken surface_id;
  surface.GetShaderId(&surface_id);
  const bool is_open_pbr_family = surface_id.GetString() == open_pbr_surface_id ||
                                  nodedef_inherits_from(surface, open_pbr_surface_id);
  /* Real semantic lowerer for ND_surface_unlit (see
   * is_supported_surface_unlit_input above) -- this is NOT treated as
   * OpenPBR-family; it gets its own field-name mapping below and in
   * graph.cpp's lower()/validate(). */
  const bool is_standard_surface = surface_id.GetString() == standard_surface_id ||
                                   surface_id.GetString() == standard_surface_100_id;
  const bool is_convert_to_surfaceshader = is_convert_to_surfaceshader_id(surface_id.GetString());
  const bool is_surface_unlit = surface_id.GetString() == surface_unlit_id ||
                                is_convert_to_surfaceshader;
  /* Real semantic lowerers for USD's own preview surface and glTF's
   * metallic-roughness PBR terminal -- neither is OpenPBR- or
   * surface_unlit-family; each gets its own real field-name mapping below
   * and in graph.cpp's lower()/validate(). */
  const bool is_usd_preview_surface = surface_id.GetString() == usd_preview_surface_id;
  const bool is_gltf_pbr = surface_id.GetString() == gltf_pbr_id;
  /* Real semantic lowerer for libraries/bxdf/disney_principled.mtlx's
   * ND_disney_principled -- also not OpenPBR-/standard_surface-family; gets
   * its own field-name mapping below and in graph.cpp's lower()/validate().
   * See disney_principled_id's comment in graph.cpp for the field-by-field
   * correspondence with Cycles' PrincipledBsdfNode. */
  const bool is_disney_principled = surface_id.GetString() == disney_principled_id;
  /* Real semantic lowerer for the generic <surface> closure-composition
   * terminal -- also not OpenPBR- or surface_unlit-family; composes
   * whatever real BSDF/EDF closures are connected to its bsdf/edf inputs
   * (see generic_surface_id / read_surface_closure_input above). */
  const bool is_generic_surface = surface_id.GetString() == generic_surface_id;
  const bool is_mix_surface = surface_id.GetString() == mix_surfaceshader_id;
  const bool is_lama_surface = surface_id.GetString() == lama_surface_id;
  committed_is_open_pbr_family = is_open_pbr_family;
  committed_is_standard_surface = is_standard_surface;
  committed_is_usd_preview_surface = is_usd_preview_surface;
  committed_is_gltf_pbr = is_gltf_pbr;
  committed_is_disney_principled = is_disney_principled;
  committed_is_generic_surface = is_generic_surface;
  committed_is_mix_surface = is_mix_surface;
  committed_is_lama_surface = is_lama_surface;
  if (!is_open_pbr_family && !is_standard_surface && !is_surface_unlit && !is_usd_preview_surface &&
      !is_gltf_pbr && !is_disney_principled && !is_generic_surface && !is_mix_surface &&
      !is_lama_surface)
  {
    const char *known_model = nullptr;
    set_error(error_message,
             string("MaterialX surface model has no registered semantic lowerer yet: ") +
                 surface_id.GetString() +
                 (known_model ? string(" (recognized as ") + known_model + ", but only " :
                                string(" (only ")) +
                 "ND_open_pbr_surface_surfaceshader, ND_standard_surface_surfaceshader, "
                 "ND_standard_surface_surfaceshader_100, ND_disney_principled, "
                 "ND_surface_unlit, ND_UsdPreviewSurface_surfaceshader, "
                 "ND_gltf_pbr_surfaceshader, ND_surface, ND_mix_surfaceshader, "
                 "ND_lama_surface, one of the eight "
                 "ND_convert_*_surfaceshader NodeDefs, or a NodeDef that declares "
                 "info:mtlx:inherit=ND_open_pbr_surface_surfaceshader, is supported for the "
                 "surfaceshader slot in this delivery phase)");
    return false;
  }

  if (is_open_pbr_family) {
  open_pbr.name = surface.GetPrim().GetName().GetString();
  open_pbr_path_for_naming = surface.GetPath().GetString();
  open_pbr.nodedef = open_pbr_surface_id;
  open_pbr.outputs["out"] = Type::SurfaceShader;

  if (!read_color_terminal_input(
          surface,
          "base_color",
          &parsed,
          &open_pbr,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "base_weight",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "base_metalness",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "specular_ior",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "specular_roughness",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "geometry_opacity",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_color_terminal_input(
          surface,
          "emission_color",
          &parsed,
          &open_pbr,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "emission_luminance",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_normal_terminal_input(
          surface,
          "geometry_normal",
          &parsed,
          &open_pbr,
          &has_supported_input,
          &emitted_normalmap_shaders,
          error_message) ||
      !read_normal_terminal_input(surface,
                                  "geometry_coat_normal",
                                  &parsed,
                                  &open_pbr,
                                  &has_supported_input,
                                  &emitted_normalmap_shaders,
                                  error_message) ||
      !read_float_terminal_input(surface,
                                 "coat_weight",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_color_terminal_input(
          surface,
          "coat_color",
          &parsed,
          &open_pbr,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "coat_roughness",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "coat_ior",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_float_terminal_input(surface,
                                 "fuzz_weight",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_color_terminal_input(
          surface,
          "fuzz_color",
          &parsed,
          &open_pbr,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "fuzz_roughness",
                                 &parsed,
                                 &open_pbr,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message))
  {
    return false;
  }
  }
  else if (is_standard_surface) {
  standard_surface.name = surface.GetPrim().GetName().GetString();
  standard_surface_path_for_naming = surface.GetPath().GetString();
  standard_surface.nodedef = standard_surface_id;
  standard_surface.outputs["out"] = Type::SurfaceShader;

  if (!read_float_terminal_input(surface, "base", &parsed, &standard_surface, &has_supported_input,
                                 &emitted_float_shaders, &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "base_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "diffuse_roughness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "metalness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "specular_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular_roughness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular_IOR", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular_anisotropy", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular_rotation", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "transmission", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "transmission_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "subsurface", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "subsurface_radius", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "subsurface_scale", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "subsurface_anisotropy", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "sheen", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "sheen_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "sheen_roughness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "coat", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "coat_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "coat_roughness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "coat_IOR", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "thin_film_thickness", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "thin_film_IOR", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "emission", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "emission_color", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "opacity", &parsed, &standard_surface,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_boolean_terminal_input(surface, "thin_walled", &standard_surface,
                                   &has_supported_input, error_message) ||
      !read_vector3_terminal_input(surface, "normal", &parsed, &standard_surface,
                                   &has_supported_input, &emitted_normalmap_shaders,
                                   error_message) ||
      !read_vector3_terminal_input(surface, "coat_normal", &parsed, &standard_surface,
                                   &has_supported_input, &emitted_normalmap_shaders,
                                   error_message) ||
      !read_vector3_terminal_input(surface, "tangent", &parsed, &standard_surface,
                                   &has_supported_input, &emitted_normalmap_shaders,
                                   error_message))
  {
    return false;
  }
  /* The rest of ND_standard_surface_surfaceshader_100's real 40-input field
   * set (see libraries/bxdf/standard_surface.mtlx) has no faithful Cycles
   * equivalent yet in this delivery phase -- same "admitted only at its
   * real nodedef default value" boundary already established for
   * gltf_pbr/UsdPreviewSurface's own inert fields above. subsurface_color
   * only tints NG_standard_surface_surfaceshader_100's subsurface_bsdf,
   * which this lowerer does not construct (subsurface is composed via
   * Principled's own native Subsurface Weight/Radius instead -- see
   * graph.cpp's standard_surface_id lowering), so it is real but currently
   * unused; the coat_anisotropy/coat_rotation/coat_affect_* four are
   * likewise real but not yet modeled by graph.cpp's coat_bsdf lowering.
   * transmission_depth/_scatter/_scatter_anisotropy/_dispersion/
   * _extra_roughness are real but Cycles' GlassBsdfNode (used for
   * transmission below) has no equivalent socket for any of them. */
  if (!require_default_float(surface, "transmission_depth", 0.0f, "standard_surface",
                             error_message) ||
      !require_default_color3(surface, "transmission_scatter", pxr::GfVec3f(0.0f, 0.0f, 0.0f),
                              "standard_surface", error_message) ||
      !require_default_float(surface, "transmission_scatter_anisotropy", 0.0f,
                             "standard_surface", error_message) ||
      !require_default_float(surface, "transmission_dispersion", 0.0f, "standard_surface",
                             error_message) ||
      !require_default_float(surface, "transmission_extra_roughness", 0.0f, "standard_surface",
                             error_message) ||
      !require_default_color3(surface, "subsurface_color", pxr::GfVec3f(1.0f, 1.0f, 1.0f),
                              "standard_surface", error_message) ||
      !require_default_float(surface, "coat_anisotropy", 0.0f, "standard_surface",
                             error_message) ||
      !require_default_float(surface, "coat_rotation", 0.0f, "standard_surface", error_message) ||
      !require_default_float(surface, "coat_affect_color", 0.0f, "standard_surface",
                             error_message) ||
      !require_default_float(surface, "coat_affect_roughness", 0.0f, "standard_surface",
                             error_message))
  {
    return false;
  }
  }
  else if (is_convert_to_surfaceshader) {
  /* Real ND_convert_*_surfaceshader semantic lowerer -- see
   * read_convert_to_surfaceshader_in() and the comment on
   * convert_color3_surfaceshader_id above for why this reuses
   * ND_surface_unlit's lowering verbatim. */
  unlit.name = surface.GetPrim().GetName().GetString();
  unlit_path_for_naming = surface.GetPath().GetString();
  unlit.nodedef = surface_unlit_id;
  unlit.outputs["out"] = Type::SurfaceShader;
  if (!read_convert_to_surfaceshader_in(surface,
                                        surface_id.GetString(),
                                        &parsed,
                                        &unlit,
                                        &emitted_float_shaders,
                                        &emitted_color4_shaders,
                                        error_message))
  {
    return false;
  }
  has_supported_input = true;
  }
  else if (is_surface_unlit) {
  /* Real ND_surface_unlit semantic lowerer. Field names and
   * defaults are taken directly from the bundled
   * libraries/stdlib/stdlib_defs.mtlx ND_surface_unlit nodedef -- this is
   * not a reuse of OpenPBR's field names. */
  unlit.name = surface.GetPrim().GetName().GetString();
  unlit_path_for_naming = surface.GetPath().GetString();
  unlit.nodedef = surface_unlit_id;
  unlit.outputs["out"] = Type::SurfaceShader;

  if (!read_float_terminal_input(surface,
                                 "emission",
                                 &parsed,
                                 &unlit,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_color_terminal_input(
          surface,
          "emission_color",
          &parsed,
          &unlit,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "transmission",
                                 &parsed,
                                 &unlit,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message) ||
      !read_color_terminal_input(
          surface,
          "transmission_color",
          &parsed,
          &unlit,
          &has_supported_input,
          &emitted_float_shaders,
          &emitted_color4_shaders,
          error_message) ||
      !read_float_terminal_input(surface,
                                 "opacity",
                                 &parsed,
                                 &unlit,
                                 &has_supported_input,
                                 &emitted_float_shaders,
                                 &emitted_color4_shaders,
                                 error_message))
  {
    return false;
  }
  /* graph.cpp's surface_unlit lowerer composes `trans = clamp(transmission,
   * 0, 1)` and `opacity = clamp(opacity, 0, 1)` as compile-time constants
   * (they scale both closures in the composition, including the
   * transmission_color/emission tint paths). A connected source for either
   * is a real, honest scope boundary for this delivery phase -- rejected
   * here rather than silently lowered with the wrong (unclamped/unscaled)
   * semantics. */
  if (unlit.links.count("transmission") || unlit.links.count("opacity")) {
    set_error(error_message,
             "surface_unlit with a connected transmission or opacity input is not yet "
             "supported (literal values only in this delivery phase)");
    return false;
  }
  }
  else if (is_usd_preview_surface) {
  /* Real ND_UsdPreviewSurface_surfaceshader semantic lowerer. Field names
   * are the real ND_UsdPreviewSurface_surfaceshader inputs from the
   * bundled libraries/bxdf/usd_preview_surface.mtlx nodedef -- see
   * is_supported_usd_preview_surface_input above. */
  usd_preview.name = surface.GetPrim().GetName().GetString();
  usd_preview_path_for_naming = surface.GetPath().GetString();
  usd_preview.nodedef = usd_preview_surface_id;
  usd_preview.outputs["out"] = Type::SurfaceShader;

  if (!read_color_terminal_input(surface, "diffuseColor", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "metallic", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "roughness", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoat", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoatRoughness", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "ior", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "emissiveColor", &parsed, &usd_preview,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message))
  {
    return false;
  }
  /* A bare-default UsdPreviewSurface (no authored inputs at all) is still
   * a real, meaningful, renderable surface -- unlike OpenPBR/surface_unlit
   * this delivery phase never requires at least one authored core input. */
  has_supported_input = true;

  if (!require_default_int(surface, "useSpecularWorkflow", 0, "UsdPreviewSurface",
                           error_message) ||
      !require_default_int(surface, "opacityMode", 0, "UsdPreviewSurface", error_message) ||
      !require_default_float(surface, "opacity", 1.0f, "UsdPreviewSurface", error_message) ||
      !require_default_vector3(surface, "normal", pxr::GfVec3f(0.0f, 0.0f, 1.0f),
                               "UsdPreviewSurface", error_message) ||
      !require_default_float(surface, "displacement", 0.0f, "UsdPreviewSurface", error_message) ||
      !require_default_float(surface, "occlusion", 1.0f, "UsdPreviewSurface", error_message))
  {
    return false;
  }
  /* specularColor and opacityThreshold are provably inert once
   * useSpecularWorkflow/opacityMode/opacity are pinned to their real
   * defaults above -- admitted at any authored value without further
   * checks (see is_supported_usd_preview_surface_input's comment). */
  }
  else if (is_gltf_pbr) {
  /* Real ND_gltf_pbr_surfaceshader semantic lowerer. Field names are the
   * real ND_gltf_pbr_surfaceshader inputs from the bundled
   * libraries/bxdf/gltf_pbr.mtlx nodedef -- see
   * is_supported_gltf_pbr_input above. */
  gltf_pbr_node.name = surface.GetPrim().GetName().GetString();
  gltf_pbr_path_for_naming = surface.GetPath().GetString();
  gltf_pbr_node.nodedef = gltf_pbr_id;
  gltf_pbr_node.outputs["out"] = Type::SurfaceShader;

  if (!read_color_terminal_input(surface, "base_color", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "metallic", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "roughness", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoat", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoat_roughness", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "ior", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "emissive", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "emissive_strength", &parsed, &gltf_pbr_node,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message))
  {
    return false;
  }
  /* A bare-default gltf_pbr (no authored inputs at all) is still a real,
   * meaningful, renderable surface. */
  has_supported_input = true;

  if (!require_unconnected_vector3(surface, "normal", "gltf_pbr", error_message) ||
      !require_unconnected_vector3(surface, "tangent", "gltf_pbr", error_message) ||
      !require_unconnected_vector3(surface, "clearcoat_normal", "gltf_pbr", error_message) ||
      !require_default_float(surface, "occlusion", 1.0f, "gltf_pbr", error_message) ||
      !require_default_float(surface, "transmission", 0.0f, "gltf_pbr", error_message) ||
      !require_default_float(surface, "specular", 1.0f, "gltf_pbr", error_message) ||
      !require_default_color3(surface, "specular_color", pxr::GfVec3f(1.0f, 1.0f, 1.0f),
                              "gltf_pbr", error_message) ||
      !require_default_int(surface, "alpha_mode", 0, "gltf_pbr", error_message) ||
      !require_default_float(surface, "iridescence", 0.0f, "gltf_pbr", error_message) ||
      !require_default_color3(surface, "sheen_color", pxr::GfVec3f(0.0f, 0.0f, 0.0f),
                              "gltf_pbr", error_message) ||
      !require_default_float(surface, "anisotropy_strength", 0.0f, "gltf_pbr", error_message))
  {
    return false;
  }
  /* alpha, alpha_cutoff, iridescence_ior, iridescence_thickness,
   * sheen_roughness, anisotropy_rotation are provably inert once their
   * gating inputs above are pinned to their real defaults; thickness,
   * attenuation_distance, attenuation_color feed a volume closure the
   * real reference nodegraph never wires to this nodedef's "out"
   * surfaceshader output at all -- all six are admitted at any authored
   * value without further checks (see is_supported_gltf_pbr_input's
   * comment). */
  }
  else if (is_disney_principled) {
  /* Real ND_disney_principled semantic lowerer. Field names are the real
   * ND_disney_principled inputs from the bundled
   * libraries/bxdf/disney_principled.mtlx nodedef -- see
   * is_supported_disney_principled_input above and disney_principled_id's
   * comment in graph.cpp for the Cycles Principled BSDF field mapping. */
  disney_principled.name = surface.GetPrim().GetName().GetString();
  disney_principled_path_for_naming = surface.GetPath().GetString();
  disney_principled.nodedef = disney_principled_id;
  disney_principled.outputs["out"] = Type::SurfaceShader;

  if (!read_color_terminal_input(surface, "baseColor", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "metallic", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "roughness", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "anisotropic", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specular", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specularTint", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "sheen", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "sheenTint", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoat", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "clearcoatGloss", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "specTrans", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "ior", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_float_terminal_input(surface, "subsurface", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message) ||
      !read_color_terminal_input(surface, "subsurfaceDistance", &parsed, &disney_principled,
                                 &has_supported_input, &emitted_float_shaders,
                                 &emitted_color4_shaders, error_message))
  {
    return false;
  }
  /* A bare-default disney_principled (no authored inputs at all) is still a
   * real, meaningful, renderable surface -- same as UsdPreviewSurface and
   * gltf_pbr. */
  has_supported_input = true;
  }
  else if (is_generic_surface) {
    generic_surface.name = surface.GetPrim().GetName().GetString();
    generic_surface_path_for_naming = surface.GetPath().GetString();
    generic_surface.nodedef = generic_surface_id;
    generic_surface.outputs["out"] = Type::SurfaceShader;

    if (!read_surface_closure_input(surface,
                                    "bsdf",
                                    SurfaceClosureKind::BSDF,
                                    &parsed,
                                    &generic_surface,
                                    &emitted_float_shaders,
                                    &emitted_color4_shaders,
                                    &emitted_closure_shaders,
                                    error_message) ||
        !read_surface_closure_input(surface,
                                    "edf",
                                    SurfaceClosureKind::EDF,
                                    &parsed,
                                    &generic_surface,
                                    &emitted_float_shaders,
                                    &emitted_color4_shaders,
                                    &emitted_closure_shaders,
                                    error_message) ||
        !read_surface_float_input(surface,
                                  generic_surface_id,
                                  "opacity",
                                  &parsed,
                                  &generic_surface,
                                  &emitted_float_shaders,
                                  &emitted_color4_shaders,
                                  error_message) ||
        !read_surface_boolean_input(
            surface, generic_surface_id, "thin_walled", &generic_surface, error_message))
    {
      return false;
    }
    for (const pxr::UsdShadeInput &input : surface.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "bsdf" && name != "edf" && name != "opacity" && name != "thin_walled") {
        set_error(error_message, string("ND_surface has no direct Cycles equivalent: ") + name);
        return false;
      }
    }
    if (generic_surface.links.find("bsdf") == generic_surface.links.end() &&
        generic_surface.links.find("edf") == generic_surface.links.end())
    {
      set_error(error_message, "ND_surface requires a supported connected bsdf or edf input");
      return false;
    }
    if (generic_surface.int_inputs.find("thin_walled") != generic_surface.int_inputs.end() &&
        generic_surface.int_inputs.at("thin_walled") != 0)
    {
      set_error(error_message,
                "ND_surface thin_walled=true is not yet supported by the Cycles lowerer");
      return false;
    }
    has_supported_input = true;
  }
  else if (is_mix_surface) {
    mix_surface.name = surface.GetPrim().GetName().GetString();
    mix_surface_path_for_naming = surface.GetPath().GetString();
    mix_surface.nodedef = mix_surfaceshader_id;
    mix_surface.outputs["out"] = Type::SurfaceShader;
    Link bg;
    Link fg;
    std::unordered_map<string, string> emitted_surface_shaders;
    std::unordered_set<string> active_surface_shaders;
    if (!read_connected_unit_opacity_surface_shader(surface.GetInput(pxr::TfToken("bg")),
                                                    &parsed,
                                                    &bg,
                                                    &emitted_float_shaders,
                                                    &emitted_color4_shaders,
                                                    &emitted_closure_shaders,
                                                    &emitted_surface_shaders,
                                                    &active_surface_shaders,
                                                    error_message) ||
        !read_connected_unit_opacity_surface_shader(surface.GetInput(pxr::TfToken("fg")),
                                                    &parsed,
                                                    &fg,
                                                    &emitted_float_shaders,
                                                    &emitted_color4_shaders,
                                                    &emitted_closure_shaders,
                                                    &emitted_surface_shaders,
                                                    &active_surface_shaders,
                                                    error_message) ||
        !read_surface_float_input(surface,
                                  mix_surfaceshader_id,
                                  "mix",
                                  &parsed,
                                  &mix_surface,
                                  &emitted_float_shaders,
                                  &emitted_color4_shaders,
                                  error_message))
    {
      return false;
    }
    mix_surface.links["bg"] = bg;
    mix_surface.links["fg"] = fg;
    for (const pxr::UsdShadeInput &input : surface.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "fg" && name != "bg" && name != "mix") {
        set_error(error_message, string(mix_surfaceshader_id) +
                              " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
    has_supported_input = true;
  }
  else {
    lama_surface.name = surface.GetPrim().GetName().GetString();
    lama_surface_path_for_naming = surface.GetPath().GetString();
    lama_surface.nodedef = lama_surface_id;
    lama_surface.outputs["out"] = Type::SurfaceShader;
    if (!read_surface_closure_input(surface,
                                    "materialFront",
                                    SurfaceClosureKind::BSDF,
                                    &parsed,
                                    &lama_surface,
                                    &emitted_float_shaders,
                                    &emitted_color4_shaders,
                                    &emitted_closure_shaders,
                                    error_message) ||
        !read_surface_closure_input(surface,
                                    "materialBack",
                                    SurfaceClosureKind::BSDF,
                                    &parsed,
                                    &lama_surface,
                                    &emitted_float_shaders,
                                    &emitted_color4_shaders,
                                    &emitted_closure_shaders,
                                    error_message) ||
        !read_surface_float_input(surface,
                                  lama_surface_id,
                                  "presence",
                                  &parsed,
                                  &lama_surface,
                                  &emitted_float_shaders,
                                  &emitted_color4_shaders,
                                  error_message))
    {
      return false;
    }
    for (const pxr::UsdShadeInput &input : surface.GetInputs()) {
      const string name = input.GetBaseName().GetString();
      if (name != "materialFront" && name != "materialBack" && name != "presence") {
        set_error(error_message, string(lama_surface_id) + " has no direct Cycles equivalent: " + name);
        return false;
      }
    }
    if (!lama_surface.links.contains("materialFront")) {
      set_error(error_message, "ND_lama_surface requires a supported connected materialFront BSDF");
      return false;
    }
    has_supported_input = true;
  }

  /* generic_surface's own field set (bsdf/edf/opacity/thin_walled) was
   * already fully validated by name inline above -- this catch-all pass
   * covers the five fixed-shape terminals, whose supported-input sets are
   * each real, verified nodedef input lists (is_supported_*_input). */
  if (!is_generic_surface && !is_mix_surface && !is_lama_surface) {
    for (const pxr::UsdShadeInput &input : surface.GetInputs()) {
      const bool supported_input =
          is_convert_to_surfaceshader ? input.GetBaseName().GetString() == "in" :
          is_open_pbr_family ?
              is_supported_open_pbr_input(input.GetBaseName().GetString()) :
          is_standard_surface ? is_supported_standard_surface_input(input.GetBaseName().GetString()) :
          is_surface_unlit ? is_supported_surface_unlit_input(input.GetBaseName().GetString()) :
          is_usd_preview_surface ?
              is_supported_usd_preview_surface_input(input.GetBaseName().GetString()) :
          is_gltf_pbr ? is_supported_gltf_pbr_input(input.GetBaseName().GetString()) :
              is_supported_disney_principled_input(input.GetBaseName().GetString());
      if (!supported_input) {
        const string model_name = is_convert_to_surfaceshader ? surface_id.GetString() :
                                  is_open_pbr_family     ? "OpenPBR" :
                                  is_standard_surface ? "standard_surface" :
                                  is_surface_unlit    ? "surface_unlit" :
                                  is_usd_preview_surface ? "UsdPreviewSurface" :
                                  is_gltf_pbr             ? "gltf_pbr" :
                                                           "disney_principled";
        set_error(error_message,
                  model_name + " input has no direct Cycles equivalent: " +
                      input.GetBaseName().GetString());
        return false;
      }
    }
  }

  if (!has_supported_input) {
    set_error(error_message,
             is_open_pbr_family     ? "OpenPBR has no supported inputs" :
             is_standard_surface    ? "standard_surface has no supported inputs" :
             is_usd_preview_surface ? "UsdPreviewSurface has no supported inputs" :
             is_gltf_pbr            ? "gltf_pbr has no supported inputs" :
             is_disney_principled   ? "disney_principled has no supported inputs" :
             is_generic_surface     ? "ND_surface has no supported inputs" :
             is_mix_surface         ? "ND_mix_surfaceshader has no supported inputs" :
             is_lama_surface        ? "ND_lama_surface has no supported inputs" :
                                      "surface_unlit has no supported inputs");
    return false;
  }

  has_any_terminal = true;
  }  // if (surface_present)

  /* Task 3 TerminalRouter: volume terminal, discovered and validated
   * independently of whether a surface terminal exists at all -- this is
   * the change that removes the volume early-return. */
  bool volume_present = false;
  if (!read_volume_terminal(material, &parsed, &volume_present, error_message)) {
    return false;
  }
  if (volume_present) {
    has_any_terminal = true;
  }

  /* Displacement terminal, likewise independent of surface presence. */
  if (!read_displacement_terminal(material, &parsed, &emitted_float_shaders, error_message)) {
    return false;
  }
  if (parsed.has_displacement) {
    has_any_terminal = true;
  }

  if (!has_any_terminal) {
    set_error(error_message,
             "USDShade material has no connected MaterialX surface, volume, displacement, or "
             "light output");
    return false;
  }

  /* Task 3: co-authored surface/volume/displacement/light terminal slots are
   * preserved atomically -- every branch above returns false, leaving
   * `*graph` untouched, before any authored slot fails to validate. Only
   * once every authored terminal has independently validated is any of
   * them committed, together, in one assignment. */
  if (surface_present) {
    if (committed_is_open_pbr_family) {
      open_pbr.name = unique_node_name(parsed, open_pbr.name, open_pbr_path_for_naming);
      parsed.nodes.push_back(std::move(open_pbr));
    }
    else if (committed_is_standard_surface) {
      standard_surface.name = unique_node_name(
          parsed, standard_surface.name, standard_surface_path_for_naming);
      parsed.nodes.push_back(std::move(standard_surface));
    }
    else if (committed_is_usd_preview_surface) {
      usd_preview.name = unique_node_name(parsed, usd_preview.name, usd_preview_path_for_naming);
      parsed.nodes.push_back(std::move(usd_preview));
    }
    else if (committed_is_gltf_pbr) {
      gltf_pbr_node.name = unique_node_name(
          parsed, gltf_pbr_node.name, gltf_pbr_path_for_naming);
      parsed.nodes.push_back(std::move(gltf_pbr_node));
    }
    else if (committed_is_disney_principled) {
      disney_principled.name = unique_node_name(
          parsed, disney_principled.name, disney_principled_path_for_naming);
      parsed.nodes.push_back(std::move(disney_principled));
    }
    else if (committed_is_generic_surface) {
      generic_surface.name = unique_node_name(
          parsed, generic_surface.name, generic_surface_path_for_naming);
      parsed.nodes.push_back(std::move(generic_surface));
    }
    else if (committed_is_mix_surface) {
      mix_surface.name = unique_node_name(parsed, mix_surface.name, mix_surface_path_for_naming);
      parsed.nodes.push_back(std::move(mix_surface));
    }
    else if (committed_is_lama_surface) {
      lama_surface.name = unique_node_name(parsed, lama_surface.name, lama_surface_path_for_naming);
      parsed.nodes.push_back(std::move(lama_surface));
    }
    else {
      unlit.name = unique_node_name(parsed, unlit.name, unlit_path_for_naming);
      parsed.nodes.push_back(std::move(unlit));
    }
  }
  parsed.has_volume = volume_present;
  parsed.has_light = light_present;
  parsed.light_node_name = light_node_name;
  parsed.light_nodedef = light_nodedef;
  *graph = std::move(parsed);
  return true;
}

namespace {

/** Map a manifest-declared output-port type to the exact USD type the
 *  Phase 1 typed resolver authenticates the selected output against. */
const pxr::SdfValueTypeName &manifest_output_usd_type(const Type type)
{
  switch (type) {
    case Type::Float:
      return pxr::SdfValueTypeNames->Float;
    case Type::Color3:
      return pxr::SdfValueTypeNames->Color3f;
    case Type::Vector2:
      return pxr::SdfValueTypeNames->Float2;
    case Type::Vector3:
      return pxr::SdfValueTypeNames->Float3;
    /* Task 4: four-component observation. Color4 keeps its color-role USD
     * type (Color4f); Vector4 uses the non-color-role Float4 -- the same
     * type-name distinction the IR's Type::Color4 vs Type::Vector4 split
     * enforces. */
    case Type::Color4:
      return pxr::SdfValueTypeNames->Color4f;
    case Type::Vector4:
      return pxr::SdfValueTypeNames->Float4;
    /* Task 5: boolean/integer exact-domain observation. */
    case Type::Boolean:
      return pxr::SdfValueTypeNames->Bool;
    case Type::Integer:
      return pxr::SdfValueTypeNames->Int;
    /* Task 6: matrix boundary. */
    case Type::Matrix33:
      return pxr::SdfValueTypeNames->Matrix3d;
    case Type::Matrix44:
      return pxr::SdfValueTypeNames->Matrix4d;
    default:
      break;
  }
  return pxr::SdfValueTypeNames->Float;
}

/**
 * Dispatch to exactly one of the four existing typed readers by the
 * manifest-declared output type.
 *
 * This is the one typed output resolver that unifies the previously
 * duplicated float/color3/vector2/vector3 entry points: a caller supplies
 * only the manifest-authenticated type, and dispatch happens once, here,
 * rather than requiring every call site to already know which of the four
 * reader functions applies. No device ABI is added or widened -- the same
 * four already-tested typed IR paths are reused unchanged.
 */
bool dispatch_typed_output(const pxr::UsdShadeInput &probe_input,
                           const Type type,
                           Graph *graph,
                           Link *result,
                           string *error_message)
{
  std::unordered_set<string> active_shaders;
  switch (type) {
    case Type::Float: {
      std::unordered_map<string, string> emitted_shaders;
      return read_float_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Color3:
      return read_color_output(probe_input, graph, result, &active_shaders, 0, error_message);
    case Type::Vector2:
      return read_vector2_output(probe_input, graph, result, &active_shaders, 0, error_message);
    case Type::Vector3:
      return read_vector3_output(probe_input, graph, result, &active_shaders, 0, error_message);
    case Type::Color4: {
      std::unordered_map<string, string> emitted_shaders;
      return read_color4_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Vector4: {
      std::unordered_map<string, string> emitted_shaders;
      return read_vector4_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Boolean: {
      std::unordered_map<string, string> emitted_shaders;
      return read_boolean_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Integer: {
      std::unordered_map<string, string> emitted_shaders;
      return read_integer_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Matrix33: {
      std::unordered_map<string, string> emitted_shaders;
      return read_matrix33_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    case Type::Matrix44: {
      std::unordered_map<string, string> emitted_shaders;
      return read_matrix44_output(
          probe_input, graph, result, &active_shaders, &emitted_shaders, 0, error_message);
    }
    default:
      set_error(error_message, "Manifest output type is not supported by Phase 1 admission");
      return false;
  }
}

}  // namespace

bool resolve_manifest_outputs(const pxr::UsdShadeMaterial &material,
                              const string &render_context,
                              const vector<SelectedOutput> &selected_outputs,
                              Graph *graph,
                              vector<Link> *results,
                              string *error_message)
{
  if (!material || graph == nullptr || results == nullptr) {
    set_error(error_message, "A valid USDShade material and destination outputs are required");
    return false;
  }
  if (selected_outputs.empty()) {
    set_error(error_message, "Manifest requires at least one selected output");
    return false;
  }

  const pxr::UsdStageWeakPtr stage = material.GetPrim().GetStage();
  if (!stage) {
    set_error(error_message, "USDShade material has no owning stage");
    return false;
  }

  /* Authenticate the exact, non-fallback render context. There is no
   * "try mtlx, then fall back to universal" step here: missing, changed,
   * ambiguous, or fallback context selection all fail closed. */
  pxr::UsdShadeOutput terminal;
  if (render_context.empty()) {
    const pxr::UsdShadeOutput mtlx_terminal = material.GetSurfaceOutput(pxr::TfToken("mtlx"));
    if (mtlx_terminal && mtlx_terminal.HasConnectedSource()) {
      set_error(error_message,
                "Manifest render context is ambiguous: the material authors a named 'mtlx' "
                "surface terminal but the manifest selected the universal context");
      return false;
    }
    terminal = material.GetSurfaceOutput();
  }
  else {
    terminal = material.GetSurfaceOutput(pxr::TfToken(render_context));
  }
  if (!terminal || !terminal.HasConnectedSource()) {
    set_error(error_message,
              "Manifest render context has no connected material surface terminal");
    return false;
  }
  const auto terminal_sources = terminal.GetConnectedSources();
  if (terminal_sources.size() != 1) {
    set_error(error_message, "Manifest material surface terminal has an invalid source");
    return false;
  }

  /* Authenticate reachability once for the whole manifest: every selected
   * node must be part of the same terminal-rooted graph, with no NodeDef
   * allowlist applied during the walk. */
  std::unordered_set<string> visited_endpoints;
  std::unordered_set<string> reachable_shader_paths;
  if (!collect_reachable_shader_paths(terminal_sources[0].source,
                                      terminal_sources[0].sourceName,
                                      terminal_sources[0].sourceType,
                                      &visited_endpoints,
                                      &reachable_shader_paths,
                                      0,
                                      error_message))
  {
    return false;
  }

  /* Resolve into a local graph/result set first; the caller-visible
   * destinations are only replaced after every selected output has
   * authenticated and resolved, so a mid-list failure clears the complete
   * observation instead of leaving a partial multi-output receipt. */
  Graph local_graph;
  vector<Link> local_results;
  local_results.reserve(selected_outputs.size());

  int probe_counter = 0;
  for (const SelectedOutput &selected : selected_outputs) {
    switch (selected.type) {
      case Type::Float:
      case Type::Color3:
      case Type::Vector2:
      case Type::Vector3:
      /* Task 4: four-component observation admitted into the same manifest
       * resolver. */
      case Type::Color4:
      case Type::Vector4:
      /* Task 5: boolean/integer exact-domain observation admitted into the
       * same manifest resolver. */
      case Type::Boolean:
      case Type::Integer:
      /* Task 6: matrix boundary admitted into the same manifest resolver
       * -- affine-only for Matrix44, enforced by read_matrix44_output. */
      case Type::Matrix33:
      case Type::Matrix44:
        break;
      default:
        set_error(error_message,
                  "Selected output '" + selected.output_name +
                      "' uses a type not supported by Phase 1/4/5/6 manifest admission");
        return false;
    }

    const pxr::UsdPrim node_prim = stage->GetPrimAtPath(pxr::SdfPath(selected.node_path));
    const pxr::UsdShadeShader node_shader(node_prim);
    if (!node_prim || !node_shader) {
      set_error(error_message,
                "Selected output node path does not exist or is not a shader: " +
                    selected.node_path);
      return false;
    }

    /* Generic NodeDef admission: any identifier authenticates as long as it
     * matches the manifest exactly. No fixed allowlist is consulted. */
    pxr::TfToken actual_id;
    if (!node_shader.GetShaderId(&actual_id) || actual_id.GetString() != selected.nodedef) {
      set_error(error_message,
                "Selected output NodeDef mismatch at " + selected.node_path +
                    ": manifest requires " + selected.nodedef);
      return false;
    }

    if (reachable_shader_paths.find(selected.node_path) == reachable_shader_paths.end()) {
      set_error(
          error_message,
          "Selected output node is not reachable from the authenticated material terminal: " +
              selected.node_path);
      return false;
    }

    const pxr::UsdShadeOutput node_output = node_shader.GetOutput(
        pxr::TfToken(selected.output_name));
    if (!node_output) {
      set_error(error_message,
                "Selected output does not exist on " + selected.node_path + ": " +
                    selected.output_name);
      return false;
    }
    const pxr::SdfValueTypeName &expected_usd_type = manifest_output_usd_type(selected.type);
    if (node_output.GetTypeName() != expected_usd_type) {
      set_error(error_message,
                "Selected output type mismatch at " + selected.node_path + "." +
                    selected.output_name);
      return false;
    }

    /* Reuse the existing, already-tested typed readers by constructing a
     * transient probe input wired directly to the selected node/output. */
    const pxr::SdfPath probe_path = pxr::SdfPath(
        "/MaterialXManifestProbe/Probe" + std::to_string(probe_counter++));
    pxr::UsdShadeShader probe = pxr::UsdShadeShader::Define(stage, probe_path);
    if (!probe) {
      set_error(error_message, "Could not allocate a manifest probe shader");
      return false;
    }
    pxr::UsdShadeInput probe_input = probe.CreateInput(pxr::TfToken("in"), expected_usd_type);
    if (!probe_input.ConnectToSource(node_shader.ConnectableAPI(),
                                     pxr::TfToken(selected.output_name)))
    {
      set_error(error_message,
                "Could not bind manifest probe to selected output: " + selected.node_path + "." +
                    selected.output_name);
      return false;
    }

    Link resolved;
    if (!dispatch_typed_output(
            probe_input, selected.type, &local_graph, &resolved, error_message))
    {
      return false;
    }
    local_results.push_back(resolved);
  }

  *graph = std::move(local_graph);
  *results = std::move(local_results);
  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
