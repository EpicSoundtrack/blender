/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/graph.h"

#include <algorithm>
#include <cmath>

#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/colorspace.h"
#include "util/path.h"
#include "util/set.h"
#include "util/transform.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

namespace {

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
/* MaterialX stdlib_defs.mtlx declares these value-typed conditional siblings
 * with float value1/value2 predicates and vector2/color4/vector4 result arms.
 * The reference implementation is the same select-by-predicate semantics as the
 * already lowered float/color3/vector3 conditional family; color4/vector4
 * preserve RGB/XYZ plus the sidecar alpha/W scalar exactly. */
constexpr const char *ifgreater_vector2_id = "ND_ifgreater_vector2";
constexpr const char *ifgreatereq_vector2_id = "ND_ifgreatereq_vector2";
constexpr const char *ifequal_vector2_id = "ND_ifequal_vector2";
constexpr const char *ifgreater_color4_id = "ND_ifgreater_color4";
constexpr const char *ifgreatereq_color4_id = "ND_ifgreatereq_color4";
constexpr const char *ifequal_color4_id = "ND_ifequal_color4";
constexpr const char *ifgreater_vector4_id = "ND_ifgreater_vector4";
constexpr const char *ifgreatereq_vector4_id = "ND_ifgreatereq_vector4";
constexpr const char *ifequal_vector4_id = "ND_ifequal_vector4";
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
constexpr const char *mix_vector3_id = "ND_mix_vector3";
/* MaterialX stdlib_defs.mtlx ND_inside_* and ND_outside_* (compositing):
 * inside = in * mask, outside = in * (1 - mask), for float/color3/color4. */
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
/* stdlib_defs.mtlx declares the four-channel Perlin/fBm siblings in the same
 * procedural2d/procedural3d families as the existing float/color3/vector2/3
 * lowerers; genosl mx_noise*_vector4.osl and mx_fractal*_vector4.osl compute
 * RGB from the native color noise plus a distinct fourth scalar sample. */
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
constexpr const char *geompropvalue_boolean_id = "ND_geompropvalue_boolean";
constexpr const char *geompropvalue_integer_id = "ND_geompropvalue_integer";
constexpr const char *geompropvalue_vector4_id = "ND_geompropvalue_vector4";
constexpr const char *constant_vector2_id = "ND_constant_vector2";
constexpr const char *invert_vector2_id = "ND_invert_vector2";
constexpr const char *invert_vector2_fa_id = "ND_invert_vector2FA";
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
constexpr const char *geompropvalue_vector3_id = "ND_geompropvalue_vector3";
/** Geometric-source observation (real gap closed): see usdshade_reader.cpp's
 *  matching declaration comment. Only space="world" is admitted by the
 *  reader, so `node.string_inputs.at("space")` is always "world" by the
 *  time `lower()`/`lowered_output()` see it here. */
constexpr const char *normal_vector3_id = "ND_normal_vector3";
constexpr const char *position_vector3_id = "ND_position_vector3";
/** ND_UsdPrimvarReader_float/_vector2/_vector3 (usd_preview_surface.mtlx):
 *  the reader admits only a literal 'varname' string (usdshade_reader.cpp's
 *  usdprimvarreader_*_id cases) -- lowered here as a Cycles AttributeNode
 *  reading that named primvar, the same generic node
 *  ND_geompropvalue_float/_color3/_color4 already reuse below. */
constexpr const char *usdprimvarreader_float_id = "ND_UsdPrimvarReader_float";
constexpr const char *usdprimvarreader_vector2_id = "ND_UsdPrimvarReader_vector2";
constexpr const char *usdprimvarreader_vector3_id = "ND_UsdPrimvarReader_vector3";
/** ND_texcoord_vector3 (stdlib_defs.mtlx): the vector2 sibling
 *  (ND_texcoord_vector2) is aliased in the reader onto the existing
 *  ND_geompropvalue_vector2 UVMapNode lowering; there is no vector3 UV
 *  lowering to alias onto, so this keeps its own nodedef id through to
 *  lower() below, reusing the same UVMapNode class -- its "UV" output socket
 *  is a native Cycles Point (3 components), so the vector3 case reads it
 *  directly instead of truncating to Vector2. */
constexpr const char *texcoord_vector3_id = "ND_texcoord_vector3";
/** ND_viewdirection_vector3 (nprlib_defs.mtlx): see usdshade_reader.cpp's
 *  matching declaration comment for why Cycles' GeometryNode "Incoming"
 *  output is a verified honest equivalent for space="world" (the reader's
 *  only admitted space, same boundary as normal_vector3_id/position_vector3_id
 *  above). */
constexpr const char *viewdirection_vector3_id = "ND_viewdirection_vector3";
constexpr const char *image_float_id = "ND_image_float";
constexpr const char *image_color3_id = "ND_image_color3";
constexpr const char *image_color4_id = "ND_image_color4";
/* MaterialX stdlib_defs.mtlx texture3d triplanarprojection nodes are not
 * volume/NanoVDB samplers: stdlib_ng.mtlx implements them as three ordinary
 * 2D <image> samples using UVs projected from the input position (YZ, XZ,
 * XY for the default Z up-axis), then blends those samples by normalized
 * abs(normal) weights raised to 1/max(blend, 0.03). Cycles has no native
 * three-file triplanar node (ImageTextureNode::NODE_IMAGE_PROJ_BOX samples one
 * file for all axes and uses Cycles' different box-projection orientation and
 * blend law), so these are lowered by composing the exact stdlib_ng image and
 * arithmetic graph for the honest subset below. */
constexpr const char *triplanarprojection_float_id = "ND_triplanarprojection_float";
constexpr const char *triplanarprojection_color3_id = "ND_triplanarprojection_color3";
constexpr const char *triplanarprojection_color4_id = "ND_triplanarprojection_color4";
constexpr const char *triplanarprojection_vector2_id = "ND_triplanarprojection_vector2";
constexpr const char *triplanarprojection_vector3_id = "ND_triplanarprojection_vector3";
constexpr const char *triplanarprojection_vector4_id = "ND_triplanarprojection_vector4";
/* MaterialX stdlib_defs.mtlx declares ND_blur_{float,color3,color4,vector2,
 * vector3,vector4} as convolution2d nodes with inputs: in, size, and uniform
 * filtertype {box, gaussian}. stdlib_ng.mtlx explicitly marks its blur
 * nodegraphs as pass-throughs that do not implement the blur specification.
 * Cycles has no MaterialX-space 2D convolution/image-sampling kernel here, so
 * only the exact degenerate size=0 case is admitted: a zero-radius blur is the
 * identity for both box and gaussian filters. Nonzero blur and
 * heighttonormal's derivative/Sobel sampling remain explicit fail-closed
 * architectural boundaries, not approximations. */
constexpr const char *blur_float_id = "ND_blur_float";
constexpr const char *blur_color3_id = "ND_blur_color3";
constexpr const char *blur_color4_id = "ND_blur_color4";
constexpr const char *blur_vector2_id = "ND_blur_vector2";
constexpr const char *blur_vector3_id = "ND_blur_vector3";
constexpr const char *blur_vector4_id = "ND_blur_vector4";
constexpr const char *constant_color4_id = "ND_constant_color4";
/**
 * <geompropvalue> with an authored color4 'geomprop' (stdlib_defs.mtlx
 * ND_geompropvalue_color4). Lowered like ND_geompropvalue_color3 below --
 * reusing a single AttributeNode -- except its "Alpha" output socket (a
 * genuine per-element lookup on the same named attribute, not a synthesized
 * value: see AttributeNode's NODE_DEFINE in scene/shader_nodes.cpp, which
 * declares SOCKET_OUT_COLOR "Color" and SOCKET_OUT_FLOAT "Alpha" side by
 * side on the one attribute read) is wired as this node's Color4 alpha
 * channel instead of a fabricated constant.
 */
constexpr const char *geompropvalue_color4_id = "ND_geompropvalue_color4";
/** Task 4: the only native Vector4 lowerer implemented in this pass --
 *  everything else (image_vector4, arithmetic ops, ramps, splits) is a
 *  documented boundary, matching how constant_color4/image_color4/color4
 *  operations were each added incrementally. */
constexpr const char *constant_vector4_id = "ND_constant_vector4";
/* MaterialX stdlib_defs.mtlx declares ND_convert_vector3_vector4 as a
 * vector3-to-vector4 adapter. stdlib_ng.mtlx's NG_convert_vector3_vector4
 * copies XYZ and feeds combine4 with W fixed to 1.0. */
constexpr const char *convert_vector3_vector4_id = "ND_convert_vector3_vector4";
/* MaterialX stdlib_ng.mtlx declares these as channel-preserving Vector4
 * adapters with W fixed to 1.0; Vector2 additionally fixes Z to 0.0. */
constexpr const char *convert_color3_vector4_id = "ND_convert_color3_vector4";
constexpr const char *convert_vector2_vector4_id = "ND_convert_vector2_vector4";
/* MaterialX stdlib_defs.mtlx declares ND_convert_vector4_vector3 as the
 * reciprocal truncating adapter; stdlib_ng.mtlx's NG_convert_vector4_vector3
 * separates the source Vector4 and combines only XYZ. */
constexpr const char *convert_vector4_vector3_id = "ND_convert_vector4_vector3";
constexpr const char *convert_vector4_vector2_id = "ND_convert_vector4_vector2";
/* MaterialX stdlib_defs.mtlx declares ND_extract_vector4 as a uniform-index
 * vector4 channel extractor, the four-component sibling of the already
 * lowered vector2/vector3 extract nodes. */
constexpr const char *extract_vector4_id = "ND_extract_vector4";
/* MaterialX stdlib_defs.mtlx / stdlib_ng.mtlx declare scalar-to-vector4
 * adapters by first converting the source to float, then broadcasting that
 * single scalar into all four vector components. convert_boolean_vector4_id
 * and convert_integer_vector4_id are declared above alongside the other
 * bool/int vector converts. */
constexpr const char *convert_float_vector4_id = "ND_convert_float_vector4";
/** MaterialX 1.39 stdlib_ng.mtlx declares contrast as
 *  (in - pivot) * amount + pivot for float/color/vector values, with the FA
 *  forms broadcasting scalar pivot/amount to every component. */
constexpr const char *usd_primvar_reader_boolean_id = "ND_UsdPrimvarReader_boolean";
constexpr const char *usd_primvar_reader_integer_id = "ND_UsdPrimvarReader_integer";
constexpr const char *usd_primvar_reader_vector4_id = "ND_UsdPrimvarReader_vector4";
/** Task 5: boolean/integer exact-domain observation. */
constexpr const char *constant_boolean_id = "ND_constant_boolean";
constexpr const char *constant_integer_id = "ND_constant_integer";
/** MaterialX 1.39 stdlib_defs.mtlx declares the value-typed <dot> family as
 * organization-only identity nodes: input "in" and output "out" share the
 * same type with defaultinput="in"; genosl/genglsl/genmdl all implement
 * them as sourcecode="{{in}}". */
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
/* MaterialX stdlib_defs.mtlx declares ND_clamp_color4 as the Color4 sibling
 * of ND_clamp_color3: color4 input plus color4 low/high bounds, clamped per
 * RGBA channel. */
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
/* MaterialX stdlib_defs.mtlx declares ND_image_vector4 as the non-color-role
 * sibling of ND_image_color4: sampled RGBA is exposed as XYZW. Cycles carries
 * XYZ through ImageTextureNode::Color (with data colorspace, like ND_image_vector3)
 * and W through ImageTextureNode::Alpha via the existing Vector4 sidecar path. */
constexpr const char *image_vector4_id = "ND_image_vector4";
constexpr const char *extract_color4_id = "ND_extract_color4";
constexpr const char *convert_color4_color3_id = "ND_convert_color4_color3";
/* MaterialX stdlib_defs.mtlx / stdlib_ng.mtlx declare Color4/Vector4 role
 * adapters as channel-preserving separate4/combineN graphs: color4->vector2/3
 * drops A, color4->vector4 preserves RGBA as XYZW, and vector4->color4
 * preserves XYZW as RGBA. */
constexpr const char *convert_color4_vector2_id = "ND_convert_color4_vector2";
constexpr const char *convert_color4_vector3_id = "ND_convert_color4_vector3";
constexpr const char *convert_color4_vector4_id = "ND_convert_color4_vector4";
constexpr const char *convert_vector4_color4_id = "ND_convert_vector4_color4";
/* MaterialX stdlib_defs.mtlx / stdlib_ng.mtlx declare these Color4 channel
 * adapters as exact scalar/color component assembly: float/bool/int broadcast
 * through convert-to-float, combine2_color4CF preserves Color3 RGB plus a
 * scalar alpha, and combine4_color4 maps four scalar inputs to RGBA. */
constexpr const char *convert_float_color4_id = "ND_convert_float_color4";
constexpr const char *convert_boolean_color4_id = "ND_convert_boolean_color4";
constexpr const char *convert_integer_color4_id = "ND_convert_integer_color4";
constexpr const char *combine2_color4cf_id = "ND_combine2_color4CF";
constexpr const char *combine4_color4_id = "ND_combine4_color4";
/* MaterialX stdlib_defs.mtlx declares ND_separate4_color4 as the Color4
 * sibling of ND_separate4_vector4, exposing RGBA as outr/outg/outb/outa; the
 * RGB channels map to Cycles SeparateColor and A uses the existing Color4
 * alpha sidecar. */
constexpr const char *separate4_color4_id = "ND_separate4_color4";
/* MaterialX stdlib_defs.mtlx declares the Vector4 combine/separate channel
 * family as the non-color-role siblings of the existing Color4 adapters:
 * ND_combine2_vector4VF(vector3,float), ND_combine2_vector4VV(vector2,vector2),
 * ND_combine4_vector4(four floats), and ND_separate4_vector4(vector4 -> four
 * float outputs). stdlib genosl/genglsl implementations assemble/extract
 * exactly {x,y,z,w}; Cycles represents that as CombineXYZ plus a W sidecar. */
constexpr const char *combine2_vector4vf_id = "ND_combine2_vector4VF";
constexpr const char *combine2_vector4vv_id = "ND_combine2_vector4VV";
constexpr const char *combine4_vector4_id = "ND_combine4_vector4";
constexpr const char *separate4_vector4_id = "ND_separate4_vector4";
/* MaterialX stdlib_defs.mtlx declares the Vector4 add/subtract/multiply/divide
 * and clamp siblings as the same component-wise math as Vector2/Vector3, with
 * FA forms broadcasting the scalar second operand or clamp bounds. Cycles has
 * native VectorMathNode support for XYZ; the existing Vector4 sidecar carries W
 * through an equivalent MathNode chain. */
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
 * separates the source RGB and feeds combine4 with alpha fixed to 1.0. */
constexpr const char *convert_color3_color4_id = "ND_convert_color3_color4";
constexpr const char *normalmap_float_id = "ND_normalmap_float";
constexpr const char *combine3_vector3_id = "ND_combine3_vector3";
constexpr const char *rotate3d_vector3_id = "ND_rotate3d_vector3";
constexpr const char *extract_vector3_id = "ND_extract_vector3";
constexpr const char *separate3_vector3_id = "ND_separate3_vector3";
constexpr const char *invert_vector3_id = "ND_invert_vector3";
constexpr const char *invert_vector3_fa_id = "ND_invert_vector3FA";
constexpr const char *smoothstep_vector2_id = "ND_smoothstep_vector2";
constexpr const char *smoothstep_vector2_fa_id = "ND_smoothstep_vector2FA";
constexpr const char *smoothstep_vector3_id = "ND_smoothstep_vector3";
constexpr const char *smoothstep_vector3_fa_id = "ND_smoothstep_vector3FA";
/* MaterialX 1.39 pbrlib/pbrlib_defs.mtlx declares ND_blackbody as
 * blackbody(float temperature=5000.0) -> color3. Its generator
 * implementations call mx_blackbody/math::blackbody, and Cycles exposes the
 * same blackbody-radiance primitive directly as BlackbodyNode. */
constexpr const char *blackbody_id = "ND_blackbody";
/* Real MaterialX 1.39 nodedefs (pbrlib/pbrlib_defs.mtlx, lines ~381-404):
 *   ND_roughness_anisotropy(float roughness=0.0, float anisotropy=0.0) -> vector2 out
 *   ND_glossiness_anisotropy(float glossiness=1.0, float anisotropy=0.0) -> vector2 out
 * Reference implementation for roughness_anisotropy is pbrlib/genosl/
 * mx_roughness_anisotropy.osl (mx_roughness_anisotropy()):
 *   roughness_sqr = clamp(roughness*roughness, M_FLOAT_EPS, 1.0);
 *   if (anisotropy > 0.0) { aspect = sqrt(1.0 - clamp(anisotropy, 0.0, 0.98));
 *     out.x = min(roughness_sqr/aspect, 1.0); out.y = roughness_sqr*aspect; }
 *   else { out.x = out.y = roughness_sqr; }
 * (M_FLOAT_EPS is MaterialX shadergen's float epsilon constant, 1e-8 --
 * stdlib/genglsl/lib/mx_math.glsl line 1.) The "if (anisotropy > 0.0) ... else"
 * branch is honestly collapsible without a select node: clamp(anisotropy, 0.0,
 * 0.98) floors any anisotropy <= 0.0 to exactly 0.0, making aspect = sqrt(1.0 -
 * 0.0) = 1.0 for every anisotropy <= 0.0 -- which reproduces the else branch's
 * out.x = out.y = roughness_sqr exactly (roughness_sqr/1.0 = roughness_sqr,
 * min(roughness_sqr, 1.0) = roughness_sqr since roughness_sqr is already
 * clamped to <= 1.0). So the single formula from the if-branch, evaluated
 * unconditionally, is bit-for-bit equivalent to the branch for every input --
 * no Cycles select/compare primitive is needed.
 * glossiness_anisotropy has no genosl/genglsl .osl/.glsl implementation of its
 * own -- pbrlib/pbrlib_ng.mtlx's IMP_glossiness_anisotropy nodegraph composes
 * it as <invert> (ND_invert_float, "amount" defaulting to 1.0 per stdlib_defs.
 * mtlx ND_invert_float, so roughness = 1.0 - glossiness) feeding the same
 * <roughness_anisotropy> above; lowered here by reusing the identical chain
 * with an extra leading MathNode(SUBTRACT) for that invert. */
constexpr const char *roughness_anisotropy_id = "ND_roughness_anisotropy";
constexpr const char *glossiness_anisotropy_id = "ND_glossiness_anisotropy";
/* Real MaterialX 1.39 nodedef (pbrlib/pbrlib_defs.mtlx, ~line 391):
 *   ND_roughness_dual(vector2 roughness=0,0) -> vector2 out
 * Reference implementation is pbrlib/genosl/mx_roughness_dual.osl
 * (mx_roughness_dual()):
 *   result.x = clamp(roughness.x * roughness.x, M_FLOAT_EPS, 1.0);
 *   if (roughness.y < 0.0) { result.y = result.x; }
 *   else { result.y = clamp(roughness.y * roughness.y, M_FLOAT_EPS, 1.0); }
 * Unlike roughness_anisotropy's "if (anisotropy > 0.0)" branch above, this
 * "if (roughness.y < 0.0)" branch is a genuine runtime sentinel-value
 * select -- roughness.y < 0 is an out-of-domain artist convention meaning
 * "isotropic, derive the second lobe from the first," and there is no
 * clamp expression that reproduces both sides for every input (unlike
 * anisotropy's clamp(anisotropy, 0.0, 0.98) trick, clamping roughness.y
 * itself would destroy the >= 0 branch's real value). So this is lowered
 * with the same real compare+select primitive already used by
 * ifgreater_float_id above (MathNode LESS_THAN as a 0/1 factor, then the
 * exact select-by-arithmetic form result = in2 + factor*(in1-in2), with
 * in1 = result.x (isotropic pick) and in2 = clamp(roughness.y^2, EPS, 1)
 * (explicit pick) -- bit-for-bit equivalent to the if/else for every
 * input, since factor is exactly 0.0 or 1.0). */
constexpr const char *roughness_dual_id = "ND_roughness_dual";
/* libraries/bxdf/open_pbr_surface.mtlx declares ND_open_pbr_anisotropy as a
 * float roughness/anisotropy -> vector2 helper. Its NG_open_pbr_anisotropy
 * graph computes:
 *   aniso_invert = 1 - anisotropy
 *   sqrt = sqrt(2 / ((aniso_invert * aniso_invert) + 1))
 *   rough_sq = roughness * roughness
 *   out.x = rough_sq * sqrt
 *   out.y = aniso_invert * out.x
 * This is a direct arithmetic nodegraph expansion, not an approximation. */
constexpr const char *open_pbr_anisotropy_id = "ND_open_pbr_anisotropy";
constexpr const char *safepower_vector2_id = "ND_safepower_vector2";
constexpr const char *safepower_vector2_fa_id = "ND_safepower_vector2FA";
constexpr const char *safepower_vector3_id = "ND_safepower_vector3";
constexpr const char *safepower_vector3_fa_id = "ND_safepower_vector3FA";
constexpr const char *transformpoint_vector3_id = "ND_transformpoint_vector3";
constexpr const char *transformvector_vector3_id = "ND_transformvector_vector3";
constexpr const char *transformnormal_vector3_id = "ND_transformnormal_vector3";
constexpr const char *open_pbr_surface_id = "ND_open_pbr_surface_surfaceshader";
constexpr const char *surface_unlit_id = "ND_surface_unlit";
/** Real semantic lowerer for USD's own preview surface terminal, verified
 *  against the bundled libraries/bxdf/usd_preview_surface.mtlx nodedef
 *  (14 real inputs). Only the subset with a direct, non-proxy Cycles
 *  Principled BSDF equivalent is admitted as connectable in this delivery
 *  phase (diffuseColor, metallic, roughness, clearcoat, clearcoatRoughness,
 *  ior, emissiveColor); everything else (specular workflow, opacity/cutout,
 *  tangent-space normal mapping, occlusion, displacement) has no faithful
 *  Cycles equivalent yet and is admitted only at its inert default value --
 *  see is_supported_usd_preview_surface_input in usdshade_reader.cpp. */
constexpr const char *usd_preview_surface_id = "ND_UsdPreviewSurface_surfaceshader";
/** Real semantic lowerer for glTF's metallic-roughness PBR terminal,
 *  verified against the bundled libraries/bxdf/gltf_pbr.mtlx nodedef (24
 *  real inputs). Same delivery-phase scoping rationale as
 *  usd_preview_surface_id above: base_color, metallic, roughness,
 *  clearcoat, clearcoat_roughness, ior, emissive, emissive_strength map
 *  directly onto Principled BSDF; transmission, specular/specular_color,
 *  sheen, iridescence, anisotropy, alpha MASK/BLEND modes, and connected
 *  normal/tangent/clearcoat_normal have no faithful Cycles equivalent yet
 *  and are admitted only at their inert default value. thickness /
 *  attenuation_distance / attenuation_color feed a volume closure that the
 *  real nodedef never wires to its "out" surfaceshader output (dead in the
 *  reference nodegraph itself), so they are accepted unconditionally. */
constexpr const char *gltf_pbr_id = "ND_gltf_pbr_surfaceshader";
constexpr const char *standard_surface_id = "ND_standard_surface_surfaceshader";
/** Real semantic lowerer for Disney's classic Principled BSDF, verified
 *  against the bundled libraries/bxdf/disney_principled.mtlx
 *  ND_disney_principled nodedef (14 real inputs; all 14 lowered here, none
 *  admitted-only-inert). All fourteen map onto Cycles' PrincipledBsdfNode,
 *  which itself already has the same physically-based-F0 / native
 *  coat-weight / native sheen-weight / native transmission-weight design as
 *  gltf_pbr_id/usd_preview_surface_id use above -- so, like those two, this
 *  is a direct field-by-field lowering (see the node-creation block below),
 *  not the standard_surface_id closure-composition style:
 *   baseColor -> Base Color; metallic -> Metallic; roughness -> Roughness;
 *   anisotropic -> Anisotropic; ior -> IOR; subsurface -> Subsurface
 *   Weight; subsurfaceDistance -> Subsurface Radius (both are literally the
 *   same "radius" concept -- NG_disney_principled's subsurface_bsdf wires
 *   subsurfaceDistance straight into its own "radius" input); specTrans ->
 *   Transmission Weight; sheen -> Sheen Weight; clearcoat -> Coat Weight.
 *   specular is disney_principled's own 0..1 F0-intensity knob, with the
 *   same real default (0.5) as Principled's own "Specular IOR Level" --
 *   both scale the same physically-based F0 derived from IOR (see
 *   kernel/svm/closure.h's `f0 *= 2.0f * specular_ior_level`), so this is a
 *   direct, not scaled, correspondence -> Specular IOR Level.
 *   specularTint/sheenTint are each a real 0..1 blend factor between white
 *   and baseColor (NG_disney_principled's dielectric_tint/sheen_color mix
 *   nodes) rather than a 1:1 socket value -- lowered via a real Cycles
 *   MixNode (color, NODE_MIX_BLEND) computing that same
 *   mix(white, baseColor, tint) into Principled's Specular Tint/Sheen Tint
 *   color sockets. clearcoatGloss is disney's inverse-roughness knob
 *   (0=rough, 1=glossy) with no native Cycles equivalent socket; lowered
 *   via a real Cycles MathNode computing 1-clearcoatGloss into Coat
 *   Roughness. */
constexpr const char *disney_principled_id = "ND_disney_principled";

float luminance(const float3 color)
{
  return color.x * 0.2126f + color.y * 0.7152f + color.z * 0.0722f;
}

bool is_supported_standard_surface_float_input(const string &name)
{
  return name == "base" || name == "diffuse_roughness" || name == "metalness" ||
         name == "specular" || name == "specular_roughness" || name == "specular_IOR" ||
         name == "specular_anisotropy" || name == "specular_rotation" ||
         name == "transmission" || name == "subsurface" || name == "subsurface_scale" ||
         name == "subsurface_anisotropy" || name == "sheen" || name == "sheen_roughness" ||
         name == "coat" || name == "coat_roughness" || name == "coat_IOR" ||
         name == "thin_film_thickness" || name == "thin_film_IOR" || name == "emission";
}

bool is_supported_standard_surface_color_input(const string &name)
{
  return name == "base_color" || name == "specular_color" || name == "transmission_color" ||
         name == "subsurface_radius" || name == "sheen_color" || name == "coat_color" ||
         name == "emission_color" || name == "opacity";
}

bool is_supported_standard_surface_vector_input(const string &name)
{
  return name == "normal" || name == "coat_normal" || name == "tangent";
}

bool is_supported_standard_surface_bool_input(const string &name)
{
  return name == "thin_walled";
}

bool has_standard_surface_parameter(const Node &node, const string &name)
{
  return node.inputs.contains(name) || node.color3_inputs.contains(name) ||
         node.vector3_inputs.contains(name) || node.int_inputs.contains(name) ||
         node.links.contains(name);
}

bool has_standard_surface_sheen(const Node &node)
{
  return has_standard_surface_parameter(node, "sheen") ||
         has_standard_surface_parameter(node, "sheen_color") ||
         has_standard_surface_parameter(node, "sheen_roughness");
}

bool has_standard_surface_transmission(const Node &node)
{
  return has_standard_surface_parameter(node, "transmission") ||
         has_standard_surface_parameter(node, "transmission_color");
}

bool has_standard_surface_coat(const Node &node)
{
  return has_standard_surface_parameter(node, "coat") ||
         has_standard_surface_parameter(node, "coat_color") ||
         has_standard_surface_parameter(node, "coat_roughness") ||
         has_standard_surface_parameter(node, "coat_IOR") ||
         has_standard_surface_parameter(node, "coat_normal");
}

/** Real BSDF closure-producer leaves -- see Type::BSDF's comment in graph.h.
 *  ND_burley_diffuse_bsdf is a deliberate, documented boundary: Cycles has
 *  no node-graph-reachable way to select CLOSURE_BSDF_BURLEY_ID (only
 *  DiffuseBsdfNode's fixed CLOSURE_BSDF_DIFFUSE_ID, which itself
 *  interpolates toward Oren-Nayar at the kernel level from `roughness`, is
 *  exposed) -- it is intentionally NOT in this list. */
constexpr const char *oren_nayar_diffuse_bsdf_id = "ND_oren_nayar_diffuse_bsdf";
constexpr const char *translucent_bsdf_id = "ND_translucent_bsdf";
constexpr const char *sheen_bsdf_id = "ND_sheen_bsdf";
constexpr const char *subsurface_bsdf_id = "ND_subsurface_bsdf";
constexpr const char *conductor_bsdf_id = "ND_conductor_bsdf";
constexpr const char *dielectric_bsdf_id = "ND_dielectric_bsdf";
constexpr const char *chiang_hair_bsdf_id = "ND_chiang_hair_bsdf";
constexpr const char *lama_diffuse_id = "ND_lama_diffuse";
constexpr const char *lama_translucent_id = "ND_lama_translucent";
constexpr const char *lama_emission_id = "ND_lama_emission";
constexpr const char *lama_sss_id = "ND_lama_sss";
constexpr const char *lama_conductor_id = "ND_lama_conductor";
constexpr const char *lama_iridescence_id = "ND_lama_iridescence";

bool is_lama_leaf_bsdf(const string &nodedef)
{
  return nodedef == lama_diffuse_id || nodedef == lama_translucent_id || nodedef == lama_sss_id;
}

bool is_lama_microfacet_surface_bsdf(const string &nodedef)
{
  return nodedef == lama_conductor_id || nodedef == lama_iridescence_id;
}

bool is_direct_bsdf_producer(const string &nodedef)
{
  return nodedef == oren_nayar_diffuse_bsdf_id || nodedef == translucent_bsdf_id ||
         nodedef == sheen_bsdf_id || nodedef == subsurface_bsdf_id ||
         nodedef == conductor_bsdf_id || nodedef == dielectric_bsdf_id || is_lama_leaf_bsdf(nodedef);
}

bool is_bsdf_producer(const string &nodedef)
{
  return is_direct_bsdf_producer(nodedef) || nodedef == chiang_hair_bsdf_id;
}

float chiang_longitudinal_variance_from_roughness(const float roughness)
{
  return sqr(0.726f * roughness + 0.812f * sqr(roughness) +
             3.700f * std::pow(roughness, 20.0f));
}

float chiang_azimuthal_scale_from_roughness(const float roughness)
{
  return 0.265f * roughness + 1.194f * sqr(roughness) + 5.372f * std::pow(roughness, 22.0f);
}

float invert_monotonic_roughness(const float target, float (*fn)(float))
{
  float lo = 0.001f;
  float hi = 1.0f;
  for (int i = 0; i < 32; ++i) {
    const float mid = 0.5f * (lo + hi);
    if (fn(mid) < target) {
      lo = mid;
    }
    else {
      hi = mid;
    }
  }
  return 0.5f * (lo + hi);
}

bool finite_float2(const float2 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool approx_equal(const float a, const float b, const float eps = 1.0e-5f)
{
  return fabsf(a - b) <= eps * max(max(fabsf(a), fabsf(b)), 1.0f);
}

bool is_default_white(const float3 &value)
{
  return value.x == 1.0f && value.y == 1.0f && value.z == 1.0f;
}

bool chiang_roughness_subset_ok(const Node &node)
{
  const bool explicit_r = node.vector2_inputs.contains("roughness_R");
  const bool explicit_tt = node.vector2_inputs.contains("roughness_TT");
  const bool explicit_trt = node.vector2_inputs.contains("roughness_TRT");
  if (explicit_r != explicit_tt || explicit_r != explicit_trt) {
    return false;
  }
  if (!explicit_r) {
    /* The MaterialX defaults (0.1, 0.05, 0.2) do not follow Cycles'
     * hard-coded Chiang lobe variance ratios, so absence of all three
     * roughness inputs is not an honest lowering. */
    return false;
  }
  const float2 roughness_r = node.vector2_inputs.at("roughness_R");
  const float2 roughness_tt = node.vector2_inputs.at("roughness_TT");
  const float2 roughness_trt = node.vector2_inputs.at("roughness_TRT");
  if (!finite_float2(roughness_r) || !finite_float2(roughness_tt) || !finite_float2(roughness_trt)) {
    return false;
  }
  /* MaterialX's chiang_hair_bsdf accepts independent post-remap
   * longitudinal/azimuthal roughness per lobe. Cycles' Chiang closure, as
   * exposed through PrincipledHairBsdfNode, has one longitudinal roughness,
   * one radial roughness, and hard-coded lobe variance ratios:
   *   R=m0_roughness, TT=0.25*v, TRT=4*v
   * (kernel/closure/bsdf_principled_hair_chiang.h). The only honest native
   * subset is therefore MaterialX data that already follows those exact
   * ratios and uses one shared azimuthal scale. */
  return approx_equal(roughness_tt.x, 0.25f * roughness_r.x) &&
         approx_equal(roughness_trt.x, 4.0f * roughness_r.x) &&
         approx_equal(roughness_tt.y, roughness_r.y) && approx_equal(roughness_trt.y, roughness_r.y);
}

/** Real BSDF closure *combinators* -- lower onto Cycles' only two real
 *  closure-combining node types, AddClosureNode and MixClosureNode (see
 *  scene/shader_nodes.h ~1186-1206; there is no third combining node in this
 *  codebase, e.g. no per-channel/vector closure-weighting node exists).
 *
 *  ND_layer_bsdf is a deliberate, documented boundary and is NOT in this
 *  list: MaterialX's real layering semantics
 *  (libraries/pbrlib/genglsl/mx_layer_bsdf.glsl) are
 *      result.response  = top.response + base.response * top.throughput
 *      result.throughput = top.throughput * base.throughput
 *  i.e. true energy-conserving vertical layering keyed on the *top* layer's
 *  own throughput (transmittance) -- a per-closure quantity that OSL/full
 *  closure trees carry but Cycles' node-graph (AddClosureNode/
 *  MixClosureNode) has no access to. Neither combining node reproduces this;
 *  approximating it with add/mix would be exactly the "proxy/substitute"
 *  mapping this project forbids, so ND_layer_bsdf is honestly rejected
 *  (falls through to the unsupported-nodedef `return false` below) rather
 *  than implemented. */
constexpr const char *add_bsdf_id = "ND_add_bsdf";
constexpr const char *mix_bsdf_id = "ND_mix_bsdf";
constexpr const char *multiply_bsdff_id = "ND_multiply_bsdfF";
constexpr const char *multiply_bsdfc_id = "ND_multiply_bsdfC";
constexpr const char *lama_add_bsdf_id = "ND_lama_add_bsdf";
constexpr const char *lama_mix_bsdf_id = "ND_lama_mix_bsdf";
constexpr const char *lama_add_edf_id = "ND_lama_add_edf";
constexpr const char *lama_mix_edf_id = "ND_lama_mix_edf";

bool is_bsdf_combinator(const string &nodedef)
{
  return nodedef == add_bsdf_id || nodedef == mix_bsdf_id || nodedef == multiply_bsdff_id ||
         nodedef == multiply_bsdfc_id || nodedef == lama_add_bsdf_id ||
         nodedef == lama_mix_bsdf_id;
}

/* Generic <surface> closure-composition terminal: composes whatever real
 * BSDF/EDF closures are connected to it, rather than an uber-shader with
 * fixed inputs like standard_surface_id above. Its own admitted upstream
 * closure set is deliberately scoped to what has been verified end-to-end
 * (oren_nayar_diffuse_bsdf/uniform_edf plus their mix/add/multiply
 * combinators) -- not the full is_bsdf_producer/is_bsdf_combinator sets
 * above, which include producers (translucent, sheen, subsurface,
 * conductor, dielectric) not yet verified as generic_surface upstream
 * links. */
constexpr const char *generic_surface_id = "ND_surface";
constexpr const char *mix_surfaceshader_id = "ND_mix_surfaceshader";
constexpr const char *lama_surface_id = "ND_lama_surface";
/* MaterialX stdlib_defs.mtlx declares ND_dot_surfaceshader as an
 * organization-only identity wrapper: surfaceshader input "in", uniform string
 * "note", surfaceshader output "out" with defaultinput="in"; the genosl,
 * genglsl, and genmdl implementations are all sourcecode="{{in}}". */
constexpr const char *dot_surfaceshader_id = "ND_dot_surfaceshader";
constexpr const char *uniform_edf_id = "ND_uniform_edf";
constexpr const char *mix_edf_id = "ND_mix_edf";
constexpr const char *add_edf_id = "ND_add_edf";
/* ND_multiply_edfF/ND_multiply_edfC (pbrlib/pbrlib_defs.mtlx): the EDF-typed
 * siblings of multiply_bsdff_id/multiply_bsdfc_id below (and of
 * multiply_vdff_id/multiply_vdfc_id in usdshade_reader.cpp) -- same "in1"
 * (EDF to scale) / "in2" (literal float or literal uniform-channel color3
 * weight) shape. Unlike multiply_bsdff_id/multiply_bsdfc_id, these have no
 * other flavor and are always SurfaceShader-typed at the IR level (mirroring
 * mix_edf_id/add_edf_id above), so they need no Type::SurfaceShader dual-
 * purpose gate. */
constexpr const char *multiply_edff_id = "ND_multiply_edfF";
constexpr const char *multiply_edfc_id = "ND_multiply_edfC";
/* ND_generalized_schlick_edf (pbrlib/pbrlib_defs.mtlx: color0,
 * color90, exponent, base EDF -> EDF) attenuates its base EDF by
 * mx_fresnel_schlick(abs(dot(N,-I)), color0, color90, exponent).  Cycles has
 * no directional EDF closure, but for the exact subset color0==color90 and a
 * uniform-channel factor the directional term is a constant scalar; then this
 * degenerates without loss to the same scalar closure-weighting primitive used
 * for ND_multiply_edfF/C. Non-constant/generalized-directional cases remain an
 * explicit unsupported boundary in validate()/usdshade_reader.cpp. */
constexpr const char *generalized_schlick_edf_id = "ND_generalized_schlick_edf";

bool finite_float3(const float3 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool supported_generic_surface_closure(const string &nodedef)
{
  return nodedef == oren_nayar_diffuse_bsdf_id || nodedef == translucent_bsdf_id ||
         nodedef == sheen_bsdf_id || nodedef == subsurface_bsdf_id ||
         nodedef == conductor_bsdf_id || nodedef == dielectric_bsdf_id ||
         nodedef == chiang_hair_bsdf_id || is_lama_leaf_bsdf(nodedef) ||
         is_lama_microfacet_surface_bsdf(nodedef) || nodedef == uniform_edf_id ||
         nodedef == lama_emission_id || nodedef == dot_surfaceshader_id ||
         nodedef == mix_bsdf_id || nodedef == mix_edf_id || nodedef == add_bsdf_id ||
         nodedef == add_edf_id || nodedef == multiply_bsdff_id || nodedef == multiply_bsdfc_id ||
         nodedef == multiply_edff_id || nodedef == multiply_edfc_id ||
         nodedef == lama_add_bsdf_id || nodedef == lama_mix_bsdf_id ||
         nodedef == lama_add_edf_id || nodedef == lama_mix_edf_id ||
         nodedef == generalized_schlick_edf_id;
}

const char *generic_surface_closure_output_name(const Node &source)
{
  if (source.nodedef == oren_nayar_diffuse_bsdf_id || source.nodedef == translucent_bsdf_id ||
      source.nodedef == sheen_bsdf_id || source.nodedef == conductor_bsdf_id ||
      source.nodedef == dielectric_bsdf_id || source.nodedef == chiang_hair_bsdf_id ||
      source.nodedef == lama_diffuse_id || source.nodedef == lama_translucent_id ||
      is_lama_microfacet_surface_bsdf(source.nodedef)) {
    return "BSDF";
  }
  if (source.nodedef == subsurface_bsdf_id || source.nodedef == lama_sss_id) {
    return "BSSRDF";
  }
  if (source.nodedef == uniform_edf_id || source.nodedef == lama_emission_id) {
    return "Emission";
  }
  if (source.nodedef == generic_surface_id || source.nodedef == mix_surfaceshader_id ||
      source.nodedef == lama_surface_id) {
    return "Closure";
  }
  if (source.nodedef == mix_bsdf_id || source.nodedef == mix_edf_id ||
      source.nodedef == lama_mix_bsdf_id || source.nodedef == lama_mix_edf_id) {
    return "Closure";
  }
  if (source.nodedef == add_bsdf_id || source.nodedef == add_edf_id ||
      source.nodedef == lama_add_bsdf_id || source.nodedef == lama_add_edf_id) {
    return "Closure";
  }
  if (source.nodedef == multiply_bsdff_id || source.nodedef == multiply_bsdfc_id ||
      source.nodedef == multiply_edff_id || source.nodedef == multiply_edfc_id ||
      source.nodedef == generalized_schlick_edf_id) {
    return "Closure";
  }
  return nullptr;
}

Type generic_surface_closure_type(const string &nodedef)
{
  if (nodedef == uniform_edf_id || nodedef == lama_emission_id || nodedef == mix_edf_id ||
      nodedef == add_edf_id || nodedef == lama_mix_edf_id || nodedef == lama_add_edf_id ||
      nodedef == multiply_edff_id || nodedef == multiply_edfc_id ||
      nodedef == generalized_schlick_edf_id) {
    return Type::LightShader;
  }
  return Type::SurfaceShader;
}

bool surface_shader_mix_has_unit_opacity(
    const Node &node,
    const unordered_map<string, const Node *> &nodes_by_name,
    unordered_set<string> *active_nodes)
{
  if (!active_nodes->insert(node.name).second) {
    return false;
  }
  const auto finish = [&](const bool result) {
    active_nodes->erase(node.name);
    return result;
  };

  if (node.nodedef == generic_surface_id) {
    return finish(!node.links.contains("opacity") && !node.inputs.contains("opacity"));
  }
  if (node.nodedef == mix_surfaceshader_id) {
    const auto fg = node.links.find("fg");
    const auto bg = node.links.find("bg");
    if (fg == node.links.end() || bg == node.links.end()) {
      return finish(false);
    }
    const Node *fg_node = nodes_by_name.at(fg->second.source_node);
    const Node *bg_node = nodes_by_name.at(bg->second.source_node);
    return finish(surface_shader_mix_has_unit_opacity(*fg_node, nodes_by_name, active_nodes) &&
                  surface_shader_mix_has_unit_opacity(*bg_node, nodes_by_name, active_nodes));
  }
  if (node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader &&
      supported_generic_surface_closure(node.nodedef)) {
    return finish(true);
  }
  return finish(false);
}

bool surface_shader_mix_has_unit_opacity(const Node &node,
                                         const unordered_map<string, const Node *> &nodes_by_name)
{
  unordered_set<string> active_nodes;
  return surface_shader_mix_has_unit_opacity(node, nodes_by_name, &active_nodes);
}

bool surface_shader_node_has_surface_shader_consumer(const Graph &source, const string &node_name)
{
  for (const Node &node : source.nodes) {
    for (const auto &link : node.links) {
      if (link.second.source_node == node_name && link.second.type == Type::SurfaceShader) {
        return true;
      }
    }
  }
  return false;
}

bool scalar_math_type(const string &nodedef, NodeMathType *math_type)
{
  if (nodedef == add_float_id) {
    *math_type = NODE_MATH_ADD;
  }
  else if (nodedef == subtract_float_id) {
    *math_type = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == multiply_float_id) {
    *math_type = NODE_MATH_MULTIPLY;
  }
  else if (nodedef == power_float_id) {
    *math_type = NODE_MATH_POWER;
  }
  else if (nodedef == modulo_float_id) {
    *math_type = NODE_MATH_MODULO;
  }
  else if (nodedef == divide_float_id) {
    *math_type = NODE_MATH_DIVIDE;
  }
  else if (nodedef == invert_float_id) {
    *math_type = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == absval_float_id) {
    *math_type = NODE_MATH_ABSOLUTE;
  }
  else if (nodedef == floor_float_id) {
    *math_type = NODE_MATH_FLOOR;
  }
  else if (nodedef == ceil_float_id) {
    *math_type = NODE_MATH_CEIL;
  }
  else if (nodedef == round_float_id) {
    *math_type = NODE_MATH_ROUND;
  }
  else if (nodedef == sqrt_float_id) {
    *math_type = NODE_MATH_SQRT;
  }
  else if (nodedef == fract_float_id) {
    *math_type = NODE_MATH_FRACTION;
  }
  else if (nodedef == sign_float_id) {
    *math_type = NODE_MATH_SIGN;
  }
  else if (nodedef == min_float_id) {
    *math_type = NODE_MATH_MINIMUM;
  }
  else if (nodedef == max_float_id) {
    *math_type = NODE_MATH_MAXIMUM;
  }
  else if (nodedef == sin_float_id) {
    *math_type = NODE_MATH_SINE;
  }
  else if (nodedef == cos_float_id) {
    *math_type = NODE_MATH_COSINE;
  }
  else if (nodedef == tan_float_id) {
    *math_type = NODE_MATH_TANGENT;
  }
  else if (nodedef == exp_float_id) {
    *math_type = NODE_MATH_EXPONENT;
  }
  else if (nodedef == acos_float_id) {
    *math_type = NODE_MATH_ARCCOSINE;
  }
  else if (nodedef == asin_float_id) {
    *math_type = NODE_MATH_ARCSINE;
  }
  else if (nodedef == atan2_float_id) {
    *math_type = NODE_MATH_ARCTAN2;
  }
  else if (nodedef == ln_float_id) {
    *math_type = NODE_MATH_LOGARITHM;
  }
  else {
    return false;
  }
  return true;
}

bool scalar_math_is_unary(const string &nodedef)
{
  return nodedef == absval_float_id || nodedef == floor_float_id || nodedef == ceil_float_id ||
         nodedef == round_float_id || nodedef == fract_float_id || nodedef == sign_float_id ||
         nodedef == sqrt_float_id || nodedef == sin_float_id ||
         nodedef == cos_float_id || nodedef == tan_float_id || nodedef == exp_float_id ||
         nodedef == acos_float_id || nodedef == asin_float_id || nodedef == ln_float_id;
}

bool is_safepower_float(const string &nodedef)
{
  return nodedef == safepower_float_id;
}

bool vector_math_type(const string &nodedef, NodeVectorMathType *math_type)
{
  NodeVectorMathType result;
  if (nodedef == "ND_add_vector3" || nodedef == "ND_add_vector3FA") result = NODE_VECTOR_MATH_ADD;
  else if (nodedef == "ND_subtract_vector3" || nodedef == "ND_subtract_vector3FA") result = NODE_VECTOR_MATH_SUBTRACT;
  else if (nodedef == "ND_multiply_vector3") result = NODE_VECTOR_MATH_MULTIPLY;
  else if (nodedef == "ND_multiply_vector3FA") result = NODE_VECTOR_MATH_SCALE;
  else if (nodedef == "ND_divide_vector3") result = NODE_VECTOR_MATH_DIVIDE;
  else if (nodedef == "ND_crossproduct_vector3") result = NODE_VECTOR_MATH_CROSS_PRODUCT;
  else if (nodedef == "ND_reflect_vector3") result = NODE_VECTOR_MATH_REFLECT;
  else if (nodedef == "ND_refract_vector3") result = NODE_VECTOR_MATH_REFRACT;
  else if (nodedef == "ND_dotproduct_vector3") result = NODE_VECTOR_MATH_DOT_PRODUCT;
  else if (nodedef == "ND_distance_vector3") result = NODE_VECTOR_MATH_DISTANCE;
  else if (nodedef == "ND_magnitude_vector3") result = NODE_VECTOR_MATH_LENGTH;
  else if (nodedef == "ND_normalize_vector3") result = NODE_VECTOR_MATH_NORMALIZE;
  else if (nodedef == "ND_absval_vector3") result = NODE_VECTOR_MATH_ABSOLUTE;
  else if (nodedef == "ND_floor_vector3") result = NODE_VECTOR_MATH_FLOOR;
  else if (nodedef == "ND_ceil_vector3") result = NODE_VECTOR_MATH_CEIL;
  else if (nodedef == "ND_fract_vector3") result = NODE_VECTOR_MATH_FRACTION;
  else if (nodedef == "ND_sin_vector3") result = NODE_VECTOR_MATH_SINE;
  else if (nodedef == "ND_cos_vector3") result = NODE_VECTOR_MATH_COSINE;
  else if (nodedef == "ND_tan_vector3") result = NODE_VECTOR_MATH_TANGENT;
  else if (nodedef == "ND_min_vector3") result = NODE_VECTOR_MATH_MINIMUM;
  else if (nodedef == "ND_max_vector3") result = NODE_VECTOR_MATH_MAXIMUM;
  else if (nodedef == "ND_sign_vector3") result = NODE_VECTOR_MATH_SIGN;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool vector2_math_type(const string &nodedef, NodeVectorMathType *math_type)
{
  if (nodedef == "ND_add_vector2" || nodedef == "ND_add_vector2FA") *math_type = NODE_VECTOR_MATH_ADD;
  else if (nodedef == "ND_subtract_vector2" || nodedef == "ND_subtract_vector2FA") *math_type = NODE_VECTOR_MATH_SUBTRACT;
  else if (nodedef == "ND_multiply_vector2") *math_type = NODE_VECTOR_MATH_MULTIPLY;
  else if (nodedef == "ND_multiply_vector2FA") *math_type = NODE_VECTOR_MATH_SCALE;
  else if (nodedef == "ND_min_vector2") *math_type = NODE_VECTOR_MATH_MINIMUM;
  else if (nodedef == "ND_max_vector2") *math_type = NODE_VECTOR_MATH_MAXIMUM;
  else if (nodedef == "ND_divide_vector2") *math_type = NODE_VECTOR_MATH_DIVIDE;
  else if (nodedef == "ND_magnitude_vector2") *math_type = NODE_VECTOR_MATH_LENGTH;
  else if (nodedef == "ND_dotproduct_vector2") *math_type = NODE_VECTOR_MATH_DOT_PRODUCT;
  else if (nodedef == "ND_distance_vector2") *math_type = NODE_VECTOR_MATH_DISTANCE;
  else if (nodedef == "ND_normalize_vector2") *math_type = NODE_VECTOR_MATH_NORMALIZE;
  else if (nodedef == "ND_absval_vector2") *math_type = NODE_VECTOR_MATH_ABSOLUTE;
  else if (nodedef == "ND_floor_vector2") *math_type = NODE_VECTOR_MATH_FLOOR;
  else if (nodedef == "ND_ceil_vector2") *math_type = NODE_VECTOR_MATH_CEIL;
  else if (nodedef == "ND_fract_vector2") *math_type = NODE_VECTOR_MATH_FRACTION;
  else if (nodedef == "ND_sin_vector2") *math_type = NODE_VECTOR_MATH_SINE;
  else if (nodedef == "ND_cos_vector2") *math_type = NODE_VECTOR_MATH_COSINE;
  else if (nodedef == "ND_tan_vector2") *math_type = NODE_VECTOR_MATH_TANGENT;
  else if (nodedef == "ND_sign_vector2") *math_type = NODE_VECTOR_MATH_SIGN;
  else return false;
  return true;
}

bool vector2_domain_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == "ND_acos_vector2") result = NODE_MATH_ARCCOSINE;
  else if (nodedef == "ND_asin_vector2") result = NODE_MATH_ARCSINE;
  else if (nodedef == "ND_exp_vector2") result = NODE_MATH_EXPONENT;
  else if (nodedef == "ND_ln_vector2") result = NODE_MATH_LOGARITHM;
  else if (nodedef == "ND_sqrt_vector2") result = NODE_MATH_SQRT;
  else if (nodedef == "ND_round_vector2") result = NODE_MATH_ROUND;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool vector3_domain_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == "ND_acos_vector3") result = NODE_MATH_ARCCOSINE;
  else if (nodedef == "ND_asin_vector3") result = NODE_MATH_ARCSINE;
  else if (nodedef == "ND_exp_vector3") result = NODE_MATH_EXPONENT;
  else if (nodedef == "ND_ln_vector3") result = NODE_MATH_LOGARITHM;
  else if (nodedef == "ND_sqrt_vector3") result = NODE_MATH_SQRT;
  else if (nodedef == "ND_round_vector3") result = NODE_MATH_ROUND;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool vector2_binary_component_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == "ND_modulo_vector2" || nodedef == "ND_modulo_vector2FA") {
    result = NODE_MATH_MODULO;
  }
  else if (nodedef == "ND_power_vector2" || nodedef == "ND_power_vector2FA") {
    result = NODE_MATH_POWER;
  }
  else if (nodedef == "ND_min_vector2FA") {
    result = NODE_MATH_MINIMUM;
  }
  else if (nodedef == "ND_max_vector2FA") {
    result = NODE_MATH_MAXIMUM;
  }
  else if (nodedef == "ND_divide_vector2FA") {
    result = NODE_MATH_DIVIDE;
  }
  else {
    return false;
  }
  if (math_type) *math_type = result;
  return true;
}

bool vector3_binary_component_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == "ND_modulo_vector3" || nodedef == "ND_modulo_vector3FA") {
    result = NODE_MATH_MODULO;
  }
  else if (nodedef == "ND_power_vector3" || nodedef == "ND_power_vector3FA") {
    result = NODE_MATH_POWER;
  }
  else if (nodedef == "ND_min_vector3FA") {
    result = NODE_MATH_MINIMUM;
  }
  else if (nodedef == "ND_max_vector3FA") {
    result = NODE_MATH_MAXIMUM;
  }
  else if (nodedef == "ND_divide_vector3FA") {
    result = NODE_MATH_DIVIDE;
  }
  else {
    return false;
  }
  if (math_type) *math_type = result;
  return true;
}

bool vector2_binary_component_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_modulo_vector2FA" || nodedef == "ND_power_vector2FA" ||
         nodedef == "ND_min_vector2FA" || nodedef == "ND_max_vector2FA" ||
         nodedef == "ND_divide_vector2FA";
}

bool is_safepower_vector2(const string &nodedef)
{
  return nodedef == safepower_vector2_id || nodedef == safepower_vector2_fa_id;
}

bool safepower_vector2_uses_scalar_second(const string &nodedef)
{
  return nodedef == safepower_vector2_fa_id;
}

bool vector3_binary_component_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_modulo_vector3FA" || nodedef == "ND_power_vector3FA" ||
         nodedef == "ND_min_vector3FA" || nodedef == "ND_max_vector3FA" ||
         nodedef == "ND_divide_vector3FA";
}

bool is_safepower_vector3(const string &nodedef)
{
  return nodedef == safepower_vector3_id || nodedef == safepower_vector3_fa_id;
}

bool safepower_vector3_uses_scalar_second(const string &nodedef)
{
  return nodedef == safepower_vector3_fa_id;
}

bool vector2_atan2_type(const string &nodedef, NodeMathType *math_type)
{
  if (nodedef != "ND_atan2_vector2") return false;
  if (math_type) *math_type = NODE_MATH_ARCTAN2;
  return true;
}

bool vector3_atan2_type(const string &nodedef, NodeMathType *math_type)
{
  if (nodedef != "ND_atan2_vector3") return false;
  if (math_type) *math_type = NODE_MATH_ARCTAN2;
  return true;
}

bool vector2_invert_type(const string &nodedef, bool *scalar_amount)
{
  if (nodedef == invert_vector2_id) {
    if (scalar_amount) *scalar_amount = false;
    return true;
  }
  if (nodedef == invert_vector2_fa_id) {
    if (scalar_amount) *scalar_amount = true;
    return true;
  }
  return false;
}

bool vector3_invert_type(const string &nodedef, bool *scalar_amount)
{
  if (nodedef == invert_vector3_id) {
    if (scalar_amount) *scalar_amount = false;
    return true;
  }
  if (nodedef == invert_vector3_fa_id) {
    if (scalar_amount) *scalar_amount = true;
    return true;
  }
  return false;
}

bool vector2_smoothstep_type(const string &nodedef, bool *scalar_edges)
{
  if (nodedef == smoothstep_vector2_id) {
    if (scalar_edges) *scalar_edges = false;
    return true;
  }
  if (nodedef == smoothstep_vector2_fa_id) {
    if (scalar_edges) *scalar_edges = true;
    return true;
  }
  return false;
}

bool vector3_smoothstep_type(const string &nodedef, bool *scalar_edges)
{
  if (nodedef == smoothstep_vector3_id) {
    if (scalar_edges) *scalar_edges = false;
    return true;
  }
  if (nodedef == smoothstep_vector3_fa_id) {
    if (scalar_edges) *scalar_edges = true;
    return true;
  }
  return false;
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

Type native_noise_or_fractal_output_type(const string &nodedef)
{
  if (native_noise_or_fractal_is_float(nodedef)) {
    return Type::Float;
  }
  if (native_noise_or_fractal_is_color3(nodedef)) {
    return Type::Color3;
  }
  if (native_noise_or_fractal_is_vector2(nodedef)) {
    return Type::Vector2;
  }
  if (native_noise_or_fractal_is_color4(nodedef)) {
    return Type::Color4;
  }
  if (nodedef.find("vector4") != string::npos) {
    return Type::Vector4;
  }
  return Type::Vector3;
}

bool vector2_math_is_unary(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector2" || nodedef == "ND_normalize_vector2" ||
         nodedef == "ND_absval_vector2" || nodedef == "ND_floor_vector2" ||
         nodedef == "ND_ceil_vector2" || nodedef == "ND_fract_vector2" ||
         nodedef == "ND_sin_vector2" || nodedef == "ND_cos_vector2" ||
         nodedef == "ND_tan_vector2" || nodedef == "ND_sign_vector2";
}

bool vector2_math_returns_float(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector2" || nodedef == "ND_dotproduct_vector2" ||
         nodedef == "ND_distance_vector2";
}

bool vector2_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_multiply_vector2FA" || nodedef == "ND_add_vector2FA" ||
         nodedef == "ND_subtract_vector2FA";
}

bool vector2_math_uses_scalar_broadcast_second(const string &nodedef)
{
  return nodedef == "ND_add_vector2FA" || nodedef == "ND_subtract_vector2FA";
}

bool vector_math_returns_float(const string &nodedef)
{
  return nodedef == "ND_dotproduct_vector3" || nodedef == "ND_distance_vector3" ||
         nodedef == "ND_magnitude_vector3";
}

bool vector_math_is_unary(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector3" || nodedef == "ND_normalize_vector3" ||
         nodedef == "ND_absval_vector3" || nodedef == "ND_floor_vector3" ||
         nodedef == "ND_ceil_vector3" || nodedef == "ND_fract_vector3" ||
         nodedef == "ND_sin_vector3" || nodedef == "ND_cos_vector3" ||
         nodedef == "ND_tan_vector3" || nodedef == "ND_sign_vector3";
}

bool vector_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_multiply_vector3FA" || nodedef == "ND_add_vector3FA" ||
         nodedef == "ND_subtract_vector3FA";
}

bool vector_math_uses_scalar_broadcast_second(const string &nodedef)
{
  return nodedef == "ND_add_vector3FA" || nodedef == "ND_subtract_vector3FA";
}

bool color_mix_type(const string &nodedef, NodeMix *mix_type)
{
  if (nodedef == add_color3_id) {
    *mix_type = NODE_MIX_ADD;
  }
  else if (nodedef == subtract_color3_id) {
    *mix_type = NODE_MIX_SUB;
  }
  else if (nodedef == multiply_color3_id) {
    *mix_type = NODE_MIX_MUL;
  }
  else if (nodedef == divide_color3_id) {
    *mix_type = NODE_MIX_DIV;
  }
  else if (nodedef == min_color3_id) {
    *mix_type = NODE_MIX_DARK;
  }
  else if (nodedef == max_color3_id) {
    *mix_type = NODE_MIX_LIGHT;
  }
  else if (nodedef == invert_color3_id || nodedef == invert_color3fa_id) {
    *mix_type = NODE_MIX_SUB;
  }
  else {
    return false;
  }
  return true;
}

bool color_unary_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == absval_color3_id) result = NODE_MATH_ABSOLUTE;
  else if (nodedef == floor_color3_id) result = NODE_MATH_FLOOR;
  else if (nodedef == ceil_color3_id) result = NODE_MATH_CEIL;
  else if (nodedef == fract_color3_id) result = NODE_MATH_FRACTION;
  else if (nodedef == round_color3_id) result = NODE_MATH_ROUND;
  else if (nodedef == sign_color3_id) result = NODE_MATH_SIGN;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool color_binary_component_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == modulo_color3_id || nodedef == modulo_color3fa_id) result = NODE_MATH_MODULO;
  else if (nodedef == power_color3_id || nodedef == power_color3fa_id) result = NODE_MATH_POWER;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool is_safepower_color3(const string &nodedef)
{
  return nodedef == safepower_color3_id || nodedef == safepower_color3fa_id;
}

bool safepower_color3_uses_scalar_exponent(const string &nodedef)
{
  return nodedef == safepower_color3fa_id;
}

bool color4_unary_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == absval_color4_id) {
    result = NODE_MATH_ABSOLUTE;
  }
  else if (nodedef == ceil_color4_id) {
    result = NODE_MATH_CEIL;
  }
  else if (nodedef == floor_color4_id) {
    result = NODE_MATH_FLOOR;
  }
  else if (nodedef == fract_color4_id) {
    result = NODE_MATH_FRACTION;
  }
  else if (nodedef == round_color4_id) {
    result = NODE_MATH_ROUND;
  }
  else if (nodedef == sign_color4_id) {
    result = NODE_MATH_SIGN;
  }
  else {
    return false;
  }
  if (math_type) {
    *math_type = result;
  }
  return true;
}

bool is_color4_invert(const string &nodedef)
{
  return nodedef == invert_color4_id || nodedef == invert_color4fa_id;
}

bool is_safepower_color4(const string &nodedef)
{
  return nodedef == safepower_color4_id || nodedef == safepower_color4fa_id;
}

bool color4_binary_math_type(const string &nodedef, NodeMathType *math_type)
{
  NodeMathType result;
  if (nodedef == add_color4_id || nodedef == add_color4fa_id) {
    result = NODE_MATH_ADD;
  }
  else if (nodedef == subtract_color4_id || nodedef == subtract_color4fa_id) {
    result = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == multiply_color4_id || nodedef == multiply_color4fa_id) {
    result = NODE_MATH_MULTIPLY;
  }
  else if (nodedef == divide_color4_id || nodedef == divide_color4fa_id) {
    result = NODE_MATH_DIVIDE;
  }
  else if (nodedef == min_color4_id || nodedef == min_color4fa_id) {
    result = NODE_MATH_MINIMUM;
  }
  else if (nodedef == max_color4_id || nodedef == max_color4fa_id) {
    result = NODE_MATH_MAXIMUM;
  }
  else if (nodedef == modulo_color4_id || nodedef == modulo_color4fa_id) {
    result = NODE_MATH_MODULO;
  }
  else if (nodedef == power_color4_id || nodedef == power_color4fa_id) {
    result = NODE_MATH_POWER;
  }
  else {
    return false;
  }
  if (math_type) {
    *math_type = result;
  }
  return true;
}

bool color4_binary_uses_scalar_second(const string &nodedef)
{
  return nodedef == add_color4fa_id || nodedef == subtract_color4fa_id ||
         nodedef == multiply_color4fa_id || nodedef == divide_color4fa_id ||
         nodedef == min_color4fa_id || nodedef == max_color4fa_id ||
         nodedef == modulo_color4fa_id || nodedef == power_color4fa_id ||
         nodedef == safepower_color4fa_id || nodedef == invert_color4fa_id;
}

bool color4_binary_uses_identity_second(const string &nodedef)
{
  return nodedef == multiply_color4_id || nodedef == divide_color4_id ||
         nodedef == modulo_color4_id || nodedef == power_color4_id ||
         nodedef == multiply_color4fa_id || nodedef == divide_color4fa_id ||
         nodedef == modulo_color4fa_id || nodedef == power_color4fa_id;
}

bool is_color4_operation(const string &nodedef)
{
  return color4_unary_math_type(nodedef, nullptr) || is_color4_invert(nodedef) ||
         is_safepower_color4(nodedef) || nodedef == clamp_color4_id ||
         nodedef == clamp_color4fa_id ||
         color4_binary_math_type(nodedef, nullptr);
}

bool color4_has_finite_components(const float4 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
         std::isfinite(value.w);
}

bool is_vector4_combine(const string &nodedef)
{
  return nodedef == combine2_vector4vf_id || nodedef == combine2_vector4vv_id ||
         nodedef == combine4_vector4_id;
}

bool vector4_math_type(const string &nodedef, NodeVectorMathType *vector_type, NodeMathType *w_type)
{
  NodeVectorMathType vector_result;
  NodeMathType w_result;
  if (nodedef == add_vector4_id || nodedef == add_vector4fa_id) {
    vector_result = NODE_VECTOR_MATH_ADD;
    w_result = NODE_MATH_ADD;
  }
  else if (nodedef == subtract_vector4_id || nodedef == subtract_vector4fa_id) {
    vector_result = NODE_VECTOR_MATH_SUBTRACT;
    w_result = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == multiply_vector4_id || nodedef == multiply_vector4fa_id) {
    vector_result = NODE_VECTOR_MATH_MULTIPLY;
    w_result = NODE_MATH_MULTIPLY;
  }
  else if (nodedef == divide_vector4_id || nodedef == divide_vector4fa_id) {
    vector_result = NODE_VECTOR_MATH_DIVIDE;
    w_result = NODE_MATH_DIVIDE;
  }
  else {
    return false;
  }
  if (vector_type) {
    *vector_type = vector_result;
  }
  if (w_type) {
    *w_type = w_result;
  }
  return true;
}

bool vector4_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == add_vector4fa_id || nodedef == subtract_vector4fa_id ||
         nodedef == multiply_vector4fa_id || nodedef == divide_vector4fa_id;
}

bool is_vector4_math_or_clamp(const string &nodedef)
{
  return vector4_math_type(nodedef, nullptr, nullptr) || nodedef == clamp_vector4_id ||
         nodedef == clamp_vector4fa_id;
}

bool triplanarprojection_type(const string &nodedef, Type *type)
{
  Type result;
  if (nodedef == triplanarprojection_float_id) {
    result = Type::Float;
  }
  else if (nodedef == triplanarprojection_color3_id) {
    result = Type::Color3;
  }
  else if (nodedef == triplanarprojection_color4_id) {
    result = Type::Color4;
  }
  else if (nodedef == triplanarprojection_vector2_id) {
    result = Type::Vector2;
  }
  else if (nodedef == triplanarprojection_vector3_id) {
    result = Type::Vector3;
  }
  else if (nodedef == triplanarprojection_vector4_id) {
    result = Type::Vector4;
  }
  else {
    return false;
  }
  if (type) {
    *type = result;
  }
  return true;
}

bool triplanarprojection_is_four_component(const string &nodedef)
{
  return nodedef == triplanarprojection_color4_id || nodedef == triplanarprojection_vector4_id;
}

const char *triplanarprojection_component_suffix(const Type type)
{
  return type == Type::Color4 ? ".Alpha" : ".W";
}

InterpolationType triplanarprojection_filter(const string &filtertype)
{
  if (filtertype == "closest") {
    return INTERPOLATION_CLOSEST;
  }
  if (filtertype == "cubic") {
    return INTERPOLATION_CUBIC;
  }
  return INTERPOLATION_LINEAR;
}

bool color_binary_component_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == modulo_color3fa_id || nodedef == power_color3fa_id;
}

bool color_scalar_mix_type(const string &nodedef, NodeMix *mix_type)
{
  NodeMix result;
  if (nodedef == add_color3_fa_id) result = NODE_MIX_ADD;
  else if (nodedef == subtract_color3_fa_id) result = NODE_MIX_SUB;
  else if (nodedef == multiply_color3_fa_id) result = NODE_MIX_MUL;
  else if (nodedef == divide_color3_fa_id) result = NODE_MIX_DIV;
  else if (nodedef == min_color3_fa_id) result = NODE_MIX_DARK;
  else if (nodedef == max_color3_fa_id) result = NODE_MIX_LIGHT;
  else return false;
  if (mix_type) *mix_type = result;
  return true;
}

Type mix_value_type(const string &nodedef)
{
  if (nodedef == mix_float_id) return Type::Float;
  if (nodedef == mix_color3_id || nodedef == mix_color3_color3_id) return Type::Color3;
  return Type::Vector3;
}

bool is_mix(const string &nodedef)
{
  return nodedef == mix_float_id || nodedef == mix_color3_id ||
         nodedef == mix_color3_color3_id || nodedef == mix_vector3_id;
}

bool is_inside_outside(const string &nodedef)
{
  return nodedef == inside_float_id || nodedef == outside_float_id ||
         nodedef == inside_color3_id || nodedef == outside_color3_id ||
         nodedef == inside_color4_id || nodedef == outside_color4_id;
}

bool is_outside(const string &nodedef)
{
  return nodedef == outside_float_id || nodedef == outside_color3_id ||
         nodedef == outside_color4_id;
}

Type inside_outside_type(const string &nodedef)
{
  if (nodedef == inside_color3_id || nodedef == outside_color3_id) {
    return Type::Color3;
  }
  if (nodedef == inside_color4_id || nodedef == outside_color4_id) {
    return Type::Color4;
  }
  return Type::Float;
}

Type mix_factor_type(const string &nodedef)
{
  return nodedef == mix_color3_color3_id ? Type::Color3 : Type::Float;
}

bool scalar_blend_type(const string &nodedef, NodeMix *mix_type)
{
  NodeMix result;
  if (nodedef == plus_float_id) result = NODE_MIX_ADD;
  else if (nodedef == minus_float_id) result = NODE_MIX_SUB;
  else if (nodedef == difference_float_id) result = NODE_MIX_DIFF;
  else if (nodedef == burn_float_id) result = NODE_MIX_BURN;
  else if (nodedef == dodge_float_id) result = NODE_MIX_DODGE;
  else if (nodedef == screen_float_id) result = NODE_MIX_SCREEN;
  else if (nodedef == overlay_float_id) result = NODE_MIX_OVERLAY;
  else return false;
  if (mix_type) *mix_type = result;
  return true;
}

bool color_blend_type(const string &nodedef, NodeMix *mix_type)
{
  NodeMix result;
  if (nodedef == plus_color3_id) result = NODE_MIX_ADD;
  else if (nodedef == minus_color3_id) result = NODE_MIX_SUB;
  else if (nodedef == difference_color3_id) result = NODE_MIX_DIFF;
  else if (nodedef == burn_color3_id) result = NODE_MIX_BURN;
  else if (nodedef == dodge_color3_id) result = NODE_MIX_DODGE;
  else if (nodedef == screen_color3_id) result = NODE_MIX_SCREEN;
  else if (nodedef == overlay_color3_id) result = NODE_MIX_OVERLAY;
  else return false;
  if (mix_type) *mix_type = result;
  return true;
}

bool is_exact_color_burn_dodge(const string &nodedef)
{
  return nodedef == burn_color3_id || nodedef == dodge_color3_id;
}

bool is_float_conditional(const string &nodedef)
{
  return nodedef == ifgreater_float_id || nodedef == ifgreatereq_float_id ||
         nodedef == ifequal_float_id;
}

bool is_color_conditional(const string &nodedef)
{
  return nodedef == ifgreater_color3_id || nodedef == ifgreatereq_color3_id || nodedef == ifequal_color3_id;
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

bool is_vector4_conditional(const string &nodedef)
{
  return nodedef == ifgreater_vector4_id || nodedef == ifgreatereq_vector4_id ||
         nodedef == ifequal_vector4_id;
}

bool is_float_predicate_conditional(const string &nodedef)
{
  return is_float_conditional(nodedef) || is_color_conditional(nodedef) ||
         is_vector2_conditional(nodedef) || is_vector_conditional(nodedef) ||
         is_color4_conditional(nodedef) || is_vector4_conditional(nodedef);
}

Type float_predicate_conditional_output_type(const string &nodedef)
{
  if (is_color_conditional(nodedef)) {
    return Type::Color3;
  }
  if (is_vector2_conditional(nodedef)) {
    return Type::Vector2;
  }
  if (is_vector_conditional(nodedef)) {
    return Type::Vector3;
  }
  if (is_color4_conditional(nodedef)) {
    return Type::Color4;
  }
  if (is_vector4_conditional(nodedef)) {
    return Type::Vector4;
  }
  return Type::Float;
}

bool finite_value(const float2 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool finite_value(const float3 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool finite_value(const float4 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z) && std::isfinite(value.w);
}

bool is_scalar_ramp(const string &nodedef)
{
  return nodedef == ramplr_float_id || nodedef == ramptb_float_id;
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

bool is_split(const string &nodedef)
{
  return is_scalar_split(nodedef) || is_color3_split(nodedef) || is_color4_split(nodedef);
}

bool split_is_top_to_bottom(const string &nodedef)
{
  return nodedef == splittb_float_id || nodedef == splittb_color3_id ||
         nodedef == splittb_color4_id;
}

bool is_smoothstep_float(const string &nodedef)
{
  return nodedef == smoothstep_float_id;
}

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

bool is_linear_range_float(const string &nodedef)
{
  return nodedef == remap_float_id || nodedef == range_float_id;
}

bool is_linear_range_color3(const string &nodedef)
{
  return nodedef == remap_color3_id || nodedef == range_color3_id ||
         nodedef == remap_color3fa_id || nodedef == range_color3fa_id;
}

bool is_linear_range_vector2(const string &nodedef)
{
  return nodedef == remap_vector2_id || nodedef == range_vector2_id ||
         nodedef == remap_vector2fa_id;
}

bool is_linear_range_vector3(const string &nodedef)
{
  return nodedef == remap_vector3_id || nodedef == remap_vector3fa_id ||
         nodedef == range_vector3_id || nodedef == range_vector3fa_id;
}

bool is_linear_range_scalar_bounds(const string &nodedef)
{
  return nodedef == remap_color3fa_id || nodedef == range_color3fa_id ||
         nodedef == remap_vector2fa_id || nodedef == remap_vector3fa_id ||
         nodedef == range_vector3fa_id;
}

bool is_luminance_color3(const string &nodedef)
{
  return nodedef == luminance_color3_id;
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

NodeVectorTransformConvertSpace vector_transform_space(const string &space)
{
  if (space == "object") {
    return NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT;
  }
  if (space == "camera") {
    return NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA;
  }
  return NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD;
}

bool validate_link(const Link &link,
                   const Type expected_type,
                   const unordered_map<string, const Node *> &nodes_by_name)
{
  const auto source_node = nodes_by_name.find(link.source_node);
  if (link.type != expected_type || source_node == nodes_by_name.end()) {
    return false;
  }
  const auto source_output = source_node->second->outputs.find(link.source_output);
  return source_output != source_node->second->outputs.end() &&
         source_output->second == expected_type;
}

bool validate_finite_float_link(
    const Link &link, const unordered_map<string, const Node *> &nodes_by_name)
{
  if (!validate_link(link, Type::Float, nodes_by_name)) {
    return false;
  }
  const Node &source = *nodes_by_name.at(link.source_node);
  if (source.nodedef != constant_float_id) {
    return true;
  }
  const auto value = source.inputs.find("value");
  return value != source.inputs.end() && std::isfinite(value->second);
}

bool value_dot_type(const string &nodedef, Type *type = nullptr)
{
  Type result;
  if (nodedef == dot_float_id) {
    result = Type::Float;
  }
  else if (nodedef == dot_color3_id) {
    result = Type::Color3;
  }
  else if (nodedef == dot_color4_id) {
    result = Type::Color4;
  }
  else if (nodedef == dot_vector2_id) {
    result = Type::Vector2;
  }
  else if (nodedef == dot_vector3_id) {
    result = Type::Vector3;
  }
  else if (nodedef == dot_vector4_id) {
    result = Type::Vector4;
  }
  else if (nodedef == dot_boolean_id) {
    result = Type::Boolean;
  }
  else if (nodedef == dot_integer_id) {
    result = Type::Integer;
  }
  else if (nodedef == dot_matrix33_id) {
    result = Type::Matrix33;
  }
  else if (nodedef == dot_matrix44_id) {
    result = Type::Matrix44;
  }
  else {
    return false;
  }
  if (type) {
    *type = result;
  }
  return true;
}

bool blur_type(const string &nodedef, Type *type = nullptr)
{
  Type result;
  if (nodedef == blur_float_id) {
    result = Type::Float;
  }
  else if (nodedef == blur_color3_id) {
    result = Type::Color3;
  }
  else if (nodedef == blur_color4_id) {
    result = Type::Color4;
  }
  else if (nodedef == blur_vector2_id) {
    result = Type::Vector2;
  }
  else if (nodedef == blur_vector3_id) {
    result = Type::Vector3;
  }
  else if (nodedef == blur_vector4_id) {
    result = Type::Vector4;
  }
  else {
    return false;
  }
  if (type) {
    *type = result;
  }
  return true;
}

bool value_dot_literal_is_finite(const Node &node, const Type type)
{
  switch (type) {
    case Type::Float:
      return node.inputs.contains("in") && std::isfinite(node.inputs.at("in"));
    case Type::Color3:
      return node.color3_inputs.contains("in") && std::isfinite(node.color3_inputs.at("in").x) &&
             std::isfinite(node.color3_inputs.at("in").y) &&
             std::isfinite(node.color3_inputs.at("in").z);
    case Type::Color4:
      return node.float4_inputs.contains("in") && std::isfinite(node.float4_inputs.at("in").x) &&
             std::isfinite(node.float4_inputs.at("in").y) &&
             std::isfinite(node.float4_inputs.at("in").z) &&
             std::isfinite(node.float4_inputs.at("in").w);
    case Type::Vector2:
      return node.vector2_inputs.contains("in") && std::isfinite(node.vector2_inputs.at("in").x) &&
             std::isfinite(node.vector2_inputs.at("in").y);
    case Type::Vector3:
      return node.vector3_inputs.contains("in") && std::isfinite(node.vector3_inputs.at("in").x) &&
             std::isfinite(node.vector3_inputs.at("in").y) &&
             std::isfinite(node.vector3_inputs.at("in").z);
    case Type::Vector4:
      return node.vector4_inputs.contains("in") && std::isfinite(node.vector4_inputs.at("in").x) &&
             std::isfinite(node.vector4_inputs.at("in").y) &&
             std::isfinite(node.vector4_inputs.at("in").z) &&
             std::isfinite(node.vector4_inputs.at("in").w);
    case Type::Boolean:
      return node.int_inputs.contains("in") &&
             (node.int_inputs.at("in") == 0 || node.int_inputs.at("in") == 1);
    case Type::Integer:
      return node.int_inputs.contains("in");
    case Type::Matrix33:
      return node.matrix33_inputs.contains("in") &&
             std::all_of(node.matrix33_inputs.at("in").begin(),
                         node.matrix33_inputs.at("in").end(),
                         [](const float component) { return std::isfinite(component); });
    case Type::Matrix44:
      return node.matrix44_inputs.contains("in") &&
             std::all_of(node.matrix44_inputs.at("in").begin(),
                         node.matrix44_inputs.at("in").end(),
                         [](const float component) { return std::isfinite(component); }) &&
             node.matrix44_inputs.at("in")[12] == 0.0f &&
             node.matrix44_inputs.at("in")[13] == 0.0f &&
             node.matrix44_inputs.at("in")[14] == 0.0f &&
             node.matrix44_inputs.at("in")[15] == 1.0f;
    default:
      return false;
  }
}

bool validate_value_dot(const Node &node,
                        const Type type,
                        const unordered_map<string, const Node *> &nodes_by_name)
{
  const auto output = node.outputs.find("out");
  const auto link = node.links.find("in");
  const bool has_link = link != node.links.end();
  const bool has_literal = value_dot_literal_is_finite(node, type);
  if (output == node.outputs.end() || output->second != type || node.outputs.size() != 1 ||
      has_link == has_literal || (has_link && !validate_link(link->second, type, nodes_by_name)) ||
      (node.string_inputs.size() > 1) ||
      (node.string_inputs.size() == 1 && !node.string_inputs.contains("note")) ||
      !node.asset_inputs.empty())
  {
    return false;
  }
  const size_t expected_float = size_t(type == Type::Float && has_literal);
  const size_t expected_int = size_t((type == Type::Boolean || type == Type::Integer) && has_literal);
  const size_t expected_color3 = size_t(type == Type::Color3 && has_literal);
  const size_t expected_color4 = size_t(type == Type::Color4 && has_literal);
  const size_t expected_vector2 = size_t(type == Type::Vector2 && has_literal);
  const size_t expected_vector3 = size_t(type == Type::Vector3 && has_literal);
  const size_t expected_vector4 = size_t(type == Type::Vector4 && has_literal);
  const size_t expected_matrix33 = size_t(type == Type::Matrix33 && has_literal);
  const size_t expected_matrix44 = size_t(type == Type::Matrix44 && has_literal);
  return node.links.size() == size_t(has_link) && node.inputs.size() == expected_float &&
         node.int_inputs.size() == expected_int && node.color3_inputs.size() == expected_color3 &&
         node.float4_inputs.size() == expected_color4 && node.vector2_inputs.size() == expected_vector2 &&
         node.vector3_inputs.size() == expected_vector3 && node.vector4_inputs.size() == expected_vector4 &&
         node.matrix33_inputs.size() == expected_matrix33 &&
         node.matrix44_inputs.size() == expected_matrix44;
}

enum class VisitState {
  Visiting,
  Visited,
};

bool validate_acyclic(const Node &node,
                      const unordered_map<string, const Node *> &nodes_by_name,
                      unordered_map<string, VisitState> *visit_states)
{
  const auto [state, inserted] = visit_states->emplace(node.name, VisitState::Visiting);
  if (!inserted) {
    return state->second == VisitState::Visited;
  }

  for (const auto &item : node.links) {
    const Link &link = item.second;
    const auto source = nodes_by_name.find(link.source_node);
    if (source == nodes_by_name.end() ||
        !validate_acyclic(*source->second, nodes_by_name, visit_states))
    {
      return false;
    }
  }

  state->second = VisitState::Visited;
  return true;
}

struct CellNoiseSpec {
  const char *nodedef;
  const char *input_name;
  Type input_type;
  int dimensions;
};

const CellNoiseSpec *cellnoise_spec(const string &nodedef)
{
  static const CellNoiseSpec specs[] = {
      {cellnoise2d_float_id, "texcoord", Type::Vector2, 2},
      {cellnoise3d_float_id, "position", Type::Vector3, 3},
  };
  for (const CellNoiseSpec &spec : specs) {
    if (nodedef == spec.nodedef) {
      return &spec;
    }
  }
  return nullptr;
}

bool validate(const Graph &source, unordered_map<string, const Node *> *nodes_by_name)
{
  for (const Node &node : source.nodes) {
    if (node.name.empty() || !nodes_by_name->emplace(node.name, &node).second) {
      return false;
    }
  }

  for (const Node &node : source.nodes) {
    if (!node.float4_inputs.empty() && node.nodedef != image_color4_id &&
        node.nodedef != constant_color4_id && node.nodedef != dot_color4_id &&
        node.nodedef != combine4_color4_id &&
        !is_color4_operation(node.nodedef) && !is_color4_conditional(node.nodedef) &&
        node.nodedef != triplanarprojection_color4_id &&
        node.nodedef != inside_color4_id && node.nodedef != outside_color4_id &&
        !is_color4_ramp(node.nodedef) &&
        !is_color4_split(node.nodedef))
    {
      return false;
    }
    /* Vector4 literal payloads are accepted only on true Vector4-valued nodes
     * or value-dot wrappers; convert/extract consume Vector4 by link so they
     * never carry vector4_inputs directly. */
    if (!node.vector4_inputs.empty() && node.nodedef != constant_vector4_id &&
        node.nodedef != dot_vector4_id && node.nodedef != image_vector4_id &&
        node.nodedef != triplanarprojection_vector4_id &&
        !is_vector4_conditional(node.nodedef) &&
        native_noise_or_fractal_output_type(node.nodedef) != Type::Vector4 &&
        !native_noise_or_fractal_is_color4(node.nodedef) &&
        !is_vector4_math_or_clamp(node.nodedef)) {
      return false;
    }
    if (is_vector4_math_or_clamp(node.nodedef)) {
      const bool scalar_second = vector4_math_uses_scalar_second(node.nodedef);
      const bool full_clamp = node.nodedef == clamp_vector4_id;
      const bool scalar_clamp = node.nodedef == clamp_vector4fa_id;
      const bool clamp = full_clamp || scalar_clamp;
      const auto valid_vector4_operand = [&](const char *name, const float4 default_value) {
        const auto literal = node.vector4_inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.vector4_inputs.end() && link == node.links.end()) {
          return finite_value(default_value);
        }
        return (literal != node.vector4_inputs.end()) != (link != node.links.end()) &&
               (literal != node.vector4_inputs.end() ?
                    finite_value(literal->second) :
                    validate_link(link->second, Type::Vector4, *nodes_by_name));
      };
      const auto valid_float_operand = [&](const char *name, const float default_value) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.inputs.end() && link == node.links.end()) {
          return std::isfinite(default_value);
        }
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ?
                    std::isfinite(literal->second) :
                    validate_link(link->second, Type::Float, *nodes_by_name));
      };
      const float4 zero = zero_float4();
      const float4 one = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
      const bool valid_full_clamp =
          full_clamp && valid_vector4_operand("in", zero) &&
          valid_vector4_operand("low", zero) && valid_vector4_operand("high", one) &&
          node.vector4_inputs.size() + node.inputs.size() + node.links.size() <= 3 &&
          (!node.vector4_inputs.contains("low") || !node.vector4_inputs.contains("high") ||
           (node.vector4_inputs.at("low").x <= node.vector4_inputs.at("high").x &&
            node.vector4_inputs.at("low").y <= node.vector4_inputs.at("high").y &&
            node.vector4_inputs.at("low").z <= node.vector4_inputs.at("high").z &&
            node.vector4_inputs.at("low").w <= node.vector4_inputs.at("high").w));
      const bool valid_scalar_clamp = scalar_clamp && valid_vector4_operand("in", zero) &&
                                      valid_float_operand("low", 0.0f) &&
                                      valid_float_operand("high", 1.0f) &&
                                      node.vector4_inputs.size() + node.inputs.size() +
                                              node.links.size() <=
                                          3 &&
                                      (!node.inputs.contains("low") ||
                                       !node.inputs.contains("high") ||
                                       node.inputs.at("low") <= node.inputs.at("high"));
      const bool valid_math =
          !clamp && valid_vector4_operand("in1", zero) &&
          (scalar_second ? valid_float_operand("in2", 1.0f) : valid_vector4_operand("in2", one)) &&
          node.vector4_inputs.size() + node.inputs.size() + node.links.size() <= 2;
      if ((!valid_math && !valid_full_clamp && !valid_scalar_clamp) ||
          std::any_of(node.vector4_inputs.begin(), node.vector4_inputs.end(), [&](const auto &input) {
            return clamp ? input.first != "in" && input.first != "low" && input.first != "high" :
                           input.first != "in1" && (scalar_second || input.first != "in2");
          }) ||
          std::any_of(node.inputs.begin(), node.inputs.end(), [&](const auto &input) {
            return scalar_clamp ? input.first != "low" && input.first != "high" :
                   scalar_second ? input.first != "in2" :
                                   true;
          }) ||
          std::any_of(node.links.begin(), node.links.end(), [&](const auto &input) {
            return clamp ? input.first != "in" && input.first != "low" && input.first != "high" :
                           input.first != "in1" && input.first != "in2";
          }) ||
          ((node.nodedef == divide_vector4_id) && node.vector4_inputs.contains("in2") &&
           (node.vector4_inputs.at("in2").x == 0.0f ||
            node.vector4_inputs.at("in2").y == 0.0f ||
            node.vector4_inputs.at("in2").z == 0.0f ||
            node.vector4_inputs.at("in2").w == 0.0f)) ||
          (node.nodedef == divide_vector4fa_id && node.inputs.contains("in2") &&
           node.inputs.at("in2") == 0.0f) ||
          node.outputs.size() != 1 || node.outputs.find("out") == node.outputs.end() ||
          node.outputs.at("out") != Type::Vector4 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (is_vector4_combine(node.nodedef)) {
      const auto output = node.outputs.find("out");
      const auto valid_float = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ? std::isfinite(literal->second) :
                                               validate_link(link->second, Type::Float, *nodes_by_name));
      };
      const auto valid_vector2 = [&](const char *name) {
        const auto literal = node.vector2_inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.vector2_inputs.end()) != (link != node.links.end()) &&
               (literal != node.vector2_inputs.end() ?
                    (std::isfinite(literal->second.x) && std::isfinite(literal->second.y)) :
                    validate_link(link->second, Type::Vector2, *nodes_by_name));
      };
      const auto valid_vector3 = [&](const char *name) {
        const auto literal = node.vector3_inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.vector3_inputs.end()) != (link != node.links.end()) &&
               (literal != node.vector3_inputs.end() ?
                    (std::isfinite(literal->second.x) && std::isfinite(literal->second.y) &&
                     std::isfinite(literal->second.z)) :
                    validate_link(link->second, Type::Vector3, *nodes_by_name));
      };
      bool ok = false;
      if (node.nodedef == combine2_vector4vf_id) {
        ok = valid_vector3("in1") && valid_float("in2") &&
             node.vector3_inputs.size() + node.links.size() + node.inputs.size() == 2 &&
             node.vector2_inputs.empty();
      }
      else if (node.nodedef == combine2_vector4vv_id) {
        ok = valid_vector2("in1") && valid_vector2("in2") &&
             node.vector2_inputs.size() + node.links.size() == 2 && node.inputs.empty();
      }
      else {
        ok = valid_float("in1") && valid_float("in2") && valid_float("in3") &&
             valid_float("in4") && node.inputs.size() + node.links.size() == 4;
      }
      if (!ok || output == node.outputs.end() || output->second != Type::Vector4 ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == separate4_vector4_id) {
      const auto input = node.links.find("in");
      /* outw must resolve to a real W value; only sources with a native W
       * sidecar (constants, the scalar/vector broadcast converts, the
       * Vector4 combine family, or a link-free dot passthrough) provide one.
       * Without this gate, lowered_output()'s "<source>.W" lookup for outw
       * would throw at lower() time for e.g. a noise/fractal Vector4 source. */
      const Node &source = input == node.links.end() ? node :
                                                        *nodes_by_name->at(input->second.source_node);
      const bool has_native_w = source.nodedef == constant_vector4_id ||
                                source.nodedef == image_vector4_id ||
                                source.nodedef == convert_vector3_vector4_id ||
                                source.nodedef == convert_color3_vector4_id ||
                                source.nodedef == convert_vector2_vector4_id ||
                                source.nodedef == convert_color4_vector4_id ||
                                source.nodedef == triplanarprojection_vector4_id ||
                                source.nodedef == convert_float_vector4_id ||
                                source.nodedef == convert_boolean_vector4_id ||
                                source.nodedef == convert_integer_vector4_id ||
                                is_vector4_math_or_clamp(source.nodedef) ||
                                is_vector4_conditional(source.nodedef) ||
                                is_vector4_combine(source.nodedef) ||
                                (value_dot_type(source.nodedef, nullptr) && source.links.empty());
      if (input == node.links.end() || !has_native_w ||
          !validate_link(input->second, Type::Vector4, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 4 ||
          node.outputs.find("outx") == node.outputs.end() ||
          node.outputs.find("outy") == node.outputs.end() ||
          node.outputs.find("outz") == node.outputs.end() ||
          node.outputs.find("outw") == node.outputs.end() ||
          node.outputs.at("outx") != Type::Float || node.outputs.at("outy") != Type::Float ||
          node.outputs.at("outz") != Type::Float || node.outputs.at("outw") != Type::Float ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (is_mix(node.nodedef) || scalar_blend_type(node.nodedef, nullptr) ||
        color_blend_type(node.nodedef, nullptr))
    {
      const Type value_type = color_blend_type(node.nodedef, nullptr) ? Type::Color3 :
                              is_mix(node.nodedef) ? mix_value_type(node.nodedef) : Type::Float;
      const Type factor_type = is_mix(node.nodedef) ? mix_factor_type(node.nodedef) : Type::Float;
      const auto output = node.outputs.find("out");
      const auto valid_value = [&](const char *name) {
        const bool literal = value_type == Type::Float ? node.inputs.contains(name) :
                             value_type == Type::Color3 ? node.color3_inputs.contains(name) :
                                                          node.vector3_inputs.contains(name);
        const auto link = node.links.find(name);
        const auto finite_color = [&](const float3 &value) {
          return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        };
        return literal != (link != node.links.end()) &&
               (!literal ||
                (value_type == Type::Float ? std::isfinite(node.inputs.at(name)) :
                 value_type == Type::Color3 ? finite_color(node.color3_inputs.at(name)) :
                                              finite_color(node.vector3_inputs.at(name)))) &&
               (link == node.links.end() || validate_link(link->second, value_type, *nodes_by_name));
      };
      const auto valid_factor = [&]() {
        const bool literal = factor_type == Type::Float ? node.inputs.contains("mix") :
                                                          node.color3_inputs.contains("mix");
        const auto link = node.links.find("mix");
        const auto finite_color = [](const float3 &value) {
          return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        };
        return literal != (link != node.links.end()) &&
               (!literal ||
                (factor_type == Type::Float ? std::isfinite(node.inputs.at("mix")) :
                                              finite_color(node.color3_inputs.at("mix")))) &&
               (link == node.links.end() ||
                validate_link(link->second, factor_type, *nodes_by_name));
      };
      if (!valid_value("bg") || !valid_value("fg") || !valid_factor() ||
          node.links.size() + node.inputs.size() + node.color3_inputs.size() +
                  node.vector3_inputs.size() !=
              3 ||
          !node.int_inputs.empty() || !node.vector2_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != value_type ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }
    if (is_inside_outside(node.nodedef)) {
      const Type value_type = inside_outside_type(node.nodedef);
      const auto output = node.outputs.find("out");
      const auto valid_mask = [&]() {
        const auto literal = node.inputs.find("mask");
        const auto link = node.links.find("mask");
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ? std::isfinite(literal->second) :
                                               validate_link(link->second, Type::Float, *nodes_by_name));
      };
      const auto valid_input = [&]() {
        const auto link = node.links.find("in");
        if (value_type == Type::Float) {
          const auto literal = node.inputs.find("in");
          return (literal != node.inputs.end()) != (link != node.links.end()) &&
                 (literal != node.inputs.end() ? std::isfinite(literal->second) :
                                                 validate_link(link->second, value_type, *nodes_by_name));
        }
        if (value_type == Type::Color3) {
          const auto literal = node.color3_inputs.find("in");
          return (literal != node.color3_inputs.end()) != (link != node.links.end()) &&
                 (literal != node.color3_inputs.end() ? finite_value(literal->second) :
                                                        validate_link(link->second, value_type, *nodes_by_name));
        }
        const auto literal = node.float4_inputs.find("in");
        return (literal != node.float4_inputs.end()) != (link != node.links.end()) &&
               (literal != node.float4_inputs.end() ? finite_value(literal->second) :
                                                      validate_link(link->second, value_type, *nodes_by_name));
      };
      const size_t expected_float_inputs = size_t(node.inputs.contains("mask")) +
                                           size_t(value_type == Type::Float && node.inputs.contains("in"));
      if (!valid_mask() || !valid_input() || node.inputs.size() != expected_float_inputs ||
          node.color3_inputs.size() != size_t(value_type == Type::Color3 && node.color3_inputs.contains("in")) ||
          node.float4_inputs.size() != size_t(value_type == Type::Color4 && node.float4_inputs.contains("in")) ||
          node.links.size() != 2 - expected_float_inputs - node.color3_inputs.size() - node.float4_inputs.size() ||
          !node.int_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != value_type || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    NodeMathType unused_math_type;
    if (scalar_math_type(node.nodedef, &unused_math_type)) {
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      const bool is_invert = node.nodedef == invert_float_id;
      const bool is_atan2 = node.nodedef == atan2_float_id;
      const char *first_input = is_unary ? "in" : (is_invert ? "amount" : (is_atan2 ? "iny" : "in1"));
      const char *second_input = is_invert ? "in" : (is_atan2 ? "inx" : "in2");
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if ((literal != node.inputs.end()) == (link != node.links.end())) {
          return false;
        }
        return literal != node.inputs.end() ?
                   std::isfinite(literal->second) :
                   validate_link(link->second, Type::Float, *nodes_by_name);
      };
      if (!valid_operand(first_input) || (!is_unary && !valid_operand(second_input)) ||
          (node.nodedef == divide_float_id &&
           (node.inputs.find("in2") == node.inputs.end() || node.inputs.at("in2") == 0.0f)) ||
          node.inputs.size() + node.links.size() != (is_unary ? 1 : 2) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_safepower_float(node.nodedef)) {
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ? std::isfinite(literal->second) :
                                               validate_link(link->second, Type::Float, *nodes_by_name));
      };
      if (!valid_operand("in1") || !valid_operand("in2") ||
          node.inputs.size() + node.links.size() != 2 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_float_predicate_conditional(node.nodedef)) {
      const Type output_type = float_predicate_conditional_output_type(node.nodedef);
      const auto output = node.outputs.find("out");
      const auto valid_predicate_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal == node.inputs.end() ? validate_link(link->second, Type::Float, *nodes_by_name) :
                                               std::isfinite(literal->second));
      };
      const auto valid_value_operand = [&](const char *name) {
        const auto link = node.links.find(name);
        if (output_type == Type::Float) {
          const auto literal = node.inputs.find(name);
          return (literal != node.inputs.end()) != (link != node.links.end()) &&
                 (literal == node.inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                 std::isfinite(literal->second));
        }
        if (output_type == Type::Color3) {
          const auto literal = node.color3_inputs.find(name);
          return (literal != node.color3_inputs.end()) != (link != node.links.end()) &&
                 (literal == node.color3_inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                        finite_value(literal->second));
        }
        if (output_type == Type::Vector2) {
          const auto literal = node.vector2_inputs.find(name);
          return (literal != node.vector2_inputs.end()) != (link != node.links.end()) &&
                 (literal == node.vector2_inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                        finite_value(literal->second));
        }
        if (output_type == Type::Vector3) {
          const auto literal = node.vector3_inputs.find(name);
          return (literal != node.vector3_inputs.end()) != (link != node.links.end()) &&
                 (literal == node.vector3_inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                        finite_value(literal->second));
        }
        if (output_type == Type::Vector4) {
          const auto literal = node.vector4_inputs.find(name);
          return (literal != node.vector4_inputs.end()) != (link != node.links.end()) &&
                 (literal == node.vector4_inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                        finite_value(literal->second));
        }
        const auto literal = node.float4_inputs.find(name);
        return (literal != node.float4_inputs.end()) != (link != node.links.end()) &&
               (literal == node.float4_inputs.end() ? validate_link(link->second, output_type, *nodes_by_name) :
                                                      finite_value(literal->second));
      };
      const size_t expected_float_literals =
          size_t(node.inputs.contains("value1")) + size_t(node.inputs.contains("value2")) +
          size_t(output_type == Type::Float && node.inputs.contains("in1")) +
          size_t(output_type == Type::Float && node.inputs.contains("in2"));
      const size_t expected_color3_literals =
          size_t(output_type == Type::Color3 && node.color3_inputs.contains("in1")) +
          size_t(output_type == Type::Color3 && node.color3_inputs.contains("in2"));
      const size_t expected_vector2_literals =
          size_t(output_type == Type::Vector2 && node.vector2_inputs.contains("in1")) +
          size_t(output_type == Type::Vector2 && node.vector2_inputs.contains("in2"));
      const size_t expected_vector3_literals =
          size_t(output_type == Type::Vector3 && node.vector3_inputs.contains("in1")) +
          size_t(output_type == Type::Vector3 && node.vector3_inputs.contains("in2"));
      const size_t expected_color4_literals =
          size_t(output_type == Type::Color4 && node.float4_inputs.contains("in1")) +
          size_t(output_type == Type::Color4 && node.float4_inputs.contains("in2"));
      const size_t expected_vector4_literals =
          size_t(output_type == Type::Vector4 && node.vector4_inputs.contains("in1")) +
          size_t(output_type == Type::Vector4 && node.vector4_inputs.contains("in2"));
      if (!valid_predicate_operand("value1") || !valid_predicate_operand("value2") ||
          !valid_value_operand("in1") || !valid_value_operand("in2") ||
          node.inputs.size() != expected_float_literals ||
          node.color3_inputs.size() != expected_color3_literals ||
          node.vector2_inputs.size() != expected_vector2_literals ||
          node.vector3_inputs.size() != expected_vector3_literals ||
          node.float4_inputs.size() != expected_color4_literals ||
          node.vector4_inputs.size() != expected_vector4_literals ||
          node.links.size() != 4 - expected_float_literals - expected_color3_literals -
                                 expected_vector2_literals - expected_vector3_literals -
                                 expected_color4_literals - expected_vector4_literals ||
          !node.int_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != output_type || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_float_color3_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Float, *nodes_by_name) ||
          node.links.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != Type::Color3 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == rgbtohsv_color3_id || node.nodedef == hsvtorgb_color3_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          node.links.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Color3 || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == clamp_float_id) {
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if ((literal != node.inputs.end()) == (link != node.links.end())) {
          return false;
        }
        return literal != node.inputs.end() ?
                   std::isfinite(literal->second) :
                   validate_link(link->second, Type::Float, *nodes_by_name);
      };
      if (!valid_operand("in") || !valid_operand("low") || !valid_operand("high") ||
          node.inputs.size() + node.links.size() != 3 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_smoothstep_float(node.nodedef)) {
      const auto input = node.inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto low = node.inputs.find("low");
      const auto high = node.inputs.find("high");
      const auto output = node.outputs.find("out");
      /* MaterialX delegates smoothstep to target shader languages, where non-increasing
       * edges are undefined.  Reject them rather than choosing a Cycles-specific result. */
      if ((input == node.inputs.end()) == (input_link == node.links.end()) ||
          (input != node.inputs.end() && !std::isfinite(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Float, *nodes_by_name)) ||
          low == node.inputs.end() || high == node.inputs.end() || !std::isfinite(low->second) ||
          !std::isfinite(high->second) || low->second >= high->second ||
          node.inputs.size() != (input == node.inputs.end() ? 2 : 3) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_contrast_float(node.nodedef)) {
      const auto input = node.inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto pivot = node.inputs.find("pivot");
      const auto amount = node.inputs.find("amount");
      const auto output = node.outputs.find("out");
      if ((input == node.inputs.end()) == (input_link == node.links.end()) ||
          (input != node.inputs.end() && !std::isfinite(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Float, *nodes_by_name)) ||
          pivot == node.inputs.end() || amount == node.inputs.end() ||
          !std::isfinite(pivot->second) || !std::isfinite(amount->second) ||
          node.inputs.size() != (input == node.inputs.end() ? 2 : 3) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_linear_range_float(node.nodedef)) {
      const auto input = node.inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto valid_finite_input = [&](const char *name) {
        const auto value = node.inputs.find(name);
        return value != node.inputs.end() && std::isfinite(value->second);
      };
      if ((input == node.inputs.end()) == (input_link == node.links.end()) ||
          (input != node.inputs.end() && !std::isfinite(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Float, *nodes_by_name)) ||
          !valid_finite_input("inlow") || !valid_finite_input("inhigh") ||
          !valid_finite_input("outlow") || !valid_finite_input("outhigh") ||
          node.inputs.at("inlow") == node.inputs.at("inhigh") ||
          node.inputs.size() != (input == node.inputs.end() ? 4 : 5) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == remap_float_id) {
        if (!node.int_inputs.empty()) return false;
      }
      else {
        const auto doclamp = node.int_inputs.find("doclamp");
        if (doclamp == node.int_inputs.end() || (doclamp->second != 0 && doclamp->second != 1) ||
            node.int_inputs.size() != 1 ||
            (doclamp->second && node.inputs.at("outlow") > node.inputs.at("outhigh")))
        {
          return false;
        }
      }
      continue;
    }

    if (is_contrast_color3(node.nodedef)) {
      const bool scalar_parameters = contrast_uses_scalar_parameters(node.nodedef);
      const auto input = node.color3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const bool has_pivot = scalar_parameters ? node.inputs.contains("pivot") :
                                                  node.color3_inputs.contains("pivot");
      const bool has_amount = scalar_parameters ? node.inputs.contains("amount") :
                                                   node.color3_inputs.contains("amount");
      const auto finite_color = [](const float3 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
      };
      if ((input == node.color3_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.color3_inputs.end() && !finite_color(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Color3, *nodes_by_name)) ||
          !has_pivot || !has_amount ||
          (scalar_parameters &&
           (!std::isfinite(node.inputs.at("pivot")) || !std::isfinite(node.inputs.at("amount")))) ||
          (!scalar_parameters &&
           (!finite_color(node.color3_inputs.at("pivot")) ||
            !finite_color(node.color3_inputs.at("amount")))) ||
          node.inputs.size() != (scalar_parameters ? 2 : 0) ||
          node.color3_inputs.size() != (scalar_parameters ? (input == node.color3_inputs.end() ? 0 : 1) :
                                                           (input == node.color3_inputs.end() ? 2 : 3)) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Color3 || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_linear_range_color3(node.nodedef)) {
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const auto input = node.color3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto valid_finite_input = [&](const char *name) {
        if (scalar_bounds) {
          const auto value = node.inputs.find(name);
          return value != node.inputs.end() && std::isfinite(value->second);
        }
        const auto value = node.color3_inputs.find(name);
        return value != node.color3_inputs.end() && std::isfinite(value->second.x) &&
               std::isfinite(value->second.y) && std::isfinite(value->second.z);
      };
      if ((input == node.color3_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.color3_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y) ||
            !std::isfinite(input->second.z))) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Color3, *nodes_by_name)) ||
          !valid_finite_input("inlow") || !valid_finite_input("inhigh") ||
          !valid_finite_input("outlow") || !valid_finite_input("outhigh") ||
          (scalar_bounds ? node.inputs.at("inlow") == node.inputs.at("inhigh") :
                           (node.color3_inputs.at("inlow").x == node.color3_inputs.at("inhigh").x ||
                            node.color3_inputs.at("inlow").y == node.color3_inputs.at("inhigh").y ||
                            node.color3_inputs.at("inlow").z == node.color3_inputs.at("inhigh").z)) ||
          node.color3_inputs.size() != (scalar_bounds ? (input == node.color3_inputs.end() ? 0 : 1) :
                                                       (input == node.color3_inputs.end() ? 4 : 5)) ||
          node.inputs.size() != (scalar_bounds ? 4 : 0) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Color3 || node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == remap_color3_id || node.nodedef == remap_color3fa_id) {
        if (!node.int_inputs.empty()) return false;
      }
      else {
        const auto doclamp = node.int_inputs.find("doclamp");
        const float3 outlow = scalar_bounds ? make_float3(node.inputs.at("outlow")) :
                                             node.color3_inputs.at("outlow");
        const float3 outhigh = scalar_bounds ? make_float3(node.inputs.at("outhigh")) :
                                              node.color3_inputs.at("outhigh");
        if (doclamp == node.int_inputs.end() || (doclamp->second != 0 && doclamp->second != 1) ||
            node.int_inputs.size() != 1 ||
            (doclamp->second &&
             (outlow.x > outhigh.x || outlow.y > outhigh.y || outlow.z > outhigh.z)))
        {
          return false;
        }
      }
      continue;
    }

    if (is_native_noise_or_fractal_family(node.nodedef)) {
      const bool is_fractal = is_native_fractal2d_family(node.nodedef) ||
                              is_native_fractal3d_family(node.nodedef);
      const bool is_3d = native_noise_or_fractal_is_3d(node.nodedef);
      const bool is_float = native_noise_or_fractal_is_float(node.nodedef);
      const bool is_vector2 = native_noise_or_fractal_is_vector2(node.nodedef);
      const bool scalar_amplitude = native_noise_or_fractal_uses_scalar_amplitude(node.nodedef);
      const auto amplitude_float = node.inputs.find("amplitude");
      const auto amplitude_vector2 = node.vector2_inputs.find("amplitude");
      const auto amplitude_vector3 = node.vector3_inputs.find("amplitude");
      const auto amplitude_vector4 = node.vector4_inputs.find("amplitude");
      const auto pivot = node.inputs.find("pivot");
      const auto octaves = node.int_inputs.find("octaves");
      const auto lacunarity = node.inputs.find("lacunarity");
      const auto diminish = node.inputs.find("diminish");
      const auto coordinate = node.links.find(is_3d ? "position" : "texcoord");
      const auto output = node.outputs.find("out");
      const Type output_type = native_noise_or_fractal_output_type(node.nodedef);
      const bool has_expected_amplitude = scalar_amplitude ?
          amplitude_float != node.inputs.end() && std::isfinite(amplitude_float->second) :
          is_vector2 ?
          amplitude_vector2 != node.vector2_inputs.end() &&
              std::isfinite(amplitude_vector2->second.x) &&
              std::isfinite(amplitude_vector2->second.y) :
          (native_noise_or_fractal_output_type(node.nodedef) == Type::Color4 ||
           native_noise_or_fractal_output_type(node.nodedef) == Type::Vector4) ?
          amplitude_vector4 != node.vector4_inputs.end() &&
              color4_has_finite_components(amplitude_vector4->second) :
          amplitude_vector3 != node.vector3_inputs.end() &&
              std::isfinite(amplitude_vector3->second.x) &&
              std::isfinite(amplitude_vector3->second.y) &&
              std::isfinite(amplitude_vector3->second.z);
      if (coordinate == node.links.end() ||
          !validate_link(coordinate->second, is_3d ? Type::Vector3 : Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != output_type || !has_expected_amplitude ||
          node.links.size() != 1 ||
          node.inputs.size() != size_t(scalar_amplitude) + size_t(!is_fractal) +
                                  size_t(is_fractal) * 2 ||
          node.vector2_inputs.size() != size_t(!scalar_amplitude && is_vector2) ||
          node.vector3_inputs.size() !=
              size_t(!scalar_amplitude && !is_vector2 && !is_float &&
                     output_type != Type::Color4 && output_type != Type::Vector4) ||
          node.vector4_inputs.size() !=
              size_t(!scalar_amplitude &&
                     (output_type == Type::Color4 || output_type == Type::Vector4)) ||
          node.outputs.size() != 1 || !node.color3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      if (is_fractal) {
        if (octaves == node.int_inputs.end() || octaves->second < 1 ||
            lacunarity == node.inputs.end() || !std::isfinite(lacunarity->second) ||
            lacunarity->second <= 0.0f || diminish == node.inputs.end() ||
            !std::isfinite(diminish->second) || node.int_inputs.size() != 1)
        {
          return false;
        }
      }
      else if (pivot == node.inputs.end() || !std::isfinite(pivot->second) ||
               !node.int_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (const CellNoiseSpec *spec = cellnoise_spec(node.nodedef)) {
      const auto coordinate = node.links.find(spec->input_name);
      const auto output = node.outputs.find("out");
      if (coordinate == node.links.end() ||
          !validate_link(coordinate->second, spec->input_type, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float ||
          node.links.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == noise2d_color3fa_id) {
      const auto amplitude = node.inputs.find("amplitude");
      const auto pivot = node.inputs.find("pivot");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (amplitude == node.inputs.end() || pivot == node.inputs.end() ||
          !std::isfinite(amplitude->second) || !std::isfinite(pivot->second) ||
          texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3 ||
          node.links.size() != 1 || node.outputs.size() != 1 || node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_luminance_color3(node.nodedef)) {
      const auto input = node.links.find("in");
      const auto coefficients = node.color3_inputs.find("lumacoeffs");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() ||
          !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          coefficients == node.color3_inputs.end() ||
          !std::isfinite(coefficients->second.x) || !std::isfinite(coefficients->second.y) ||
          !std::isfinite(coefficients->second.z) || node.links.size() != 1 ||
          node.color3_inputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_float_id) {
      const auto value = node.inputs.find("value");
      const auto output = node.outputs.find("out");
      if (value == node.inputs.end() || output == node.outputs.end() ||
          output->second != Type::Float)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_color3_id) {
      const auto value = node.color3_inputs.find("value");
      const auto output = node.outputs.find("out");
      if (value == node.color3_inputs.end() || output == node.outputs.end() ||
          output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_color4_id) {
      const auto value = node.float4_inputs.find("value");
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::Color4 ||
          (value != node.float4_inputs.end() && !color4_has_finite_components(value->second)) ||
          node.float4_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_vector4_id) {
      const auto value = node.vector4_inputs.find("value");
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::Vector4 ||
          (value != node.vector4_inputs.end() && !color4_has_finite_components(value->second)) ||
          node.vector4_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_vector4_id ||
        node.nodedef == usd_primvar_reader_vector4_id)
    {
      const char *name = node.nodedef == geompropvalue_vector4_id ? "geomprop" : "varname";
      const auto geomprop = node.string_inputs.find(name);
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Vector4 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_vector3_vector4_id || node.nodedef == convert_color3_vector4_id ||
        node.nodedef == convert_vector2_vector4_id)
    {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      const Type input_type = node.nodedef == convert_color3_vector4_id ? Type::Color3 :
                              node.nodedef == convert_vector2_vector4_id ? Type::Vector2 :
                                                                            Type::Vector3;
      if (input == node.links.end() || !validate_link(input->second, input_type, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Vector4 || node.links.size() != 1 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_vector4_vector3_id || node.nodedef == convert_vector4_vector2_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Vector4, *nodes_by_name) ||
          output == node.outputs.end() ||
          output->second != (node.nodedef == convert_vector4_vector3_id ? Type::Vector3 : Type::Vector2) ||
          node.links.size() != 1 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_color4_vector2_id || node.nodedef == convert_color4_vector3_id ||
        node.nodedef == convert_color4_vector4_id || node.nodedef == convert_vector4_color4_id)
    {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      const bool from_color4 = node.nodedef == convert_color4_vector2_id ||
                               node.nodedef == convert_color4_vector3_id ||
                               node.nodedef == convert_color4_vector4_id;
      const Type output_type = node.nodedef == convert_color4_vector2_id ? Type::Vector2 :
                               node.nodedef == convert_color4_vector3_id ? Type::Vector3 :
                               node.nodedef == convert_color4_vector4_id ? Type::Vector4 :
                                                                          Type::Color4;
      const Node &source = input == node.links.end() ? node : *nodes_by_name->at(input->second.source_node);
      const bool has_native_w = source.nodedef == constant_vector4_id ||
                                source.nodedef == image_vector4_id ||
                                source.nodedef == convert_vector3_vector4_id ||
                                source.nodedef == convert_color3_vector4_id ||
                                source.nodedef == convert_vector2_vector4_id ||
                                source.nodedef == convert_color4_vector4_id ||
                                source.nodedef == convert_float_vector4_id ||
                                source.nodedef == convert_boolean_vector4_id ||
                                source.nodedef == convert_integer_vector4_id ||
                                is_vector4_math_or_clamp(source.nodedef) ||
                                is_vector4_conditional(source.nodedef) ||
                                is_vector4_combine(source.nodedef) ||
                                (value_dot_type(source.nodedef, nullptr) && source.links.empty());
      if (input == node.links.end() || (!from_color4 && !has_native_w) ||
          !validate_link(input->second, from_color4 ? Type::Color4 : Type::Vector4, *nodes_by_name) ||
          output == node.outputs.end() || output->second != output_type || node.links.size() != 1 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == extract_vector4_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      const Node &source = input == node.links.end() ? node : *nodes_by_name->at(input->second.source_node);
      const bool has_native_w = source.nodedef == constant_vector4_id ||
                                source.nodedef == image_vector4_id ||
                                source.nodedef == convert_vector3_vector4_id ||
                                source.nodedef == convert_color3_vector4_id ||
                                source.nodedef == convert_vector2_vector4_id ||
                                source.nodedef == convert_color4_vector4_id ||
                                source.nodedef == convert_float_vector4_id ||
                                source.nodedef == convert_boolean_vector4_id ||
                                source.nodedef == convert_integer_vector4_id ||
                                is_vector4_math_or_clamp(source.nodedef) ||
                                is_vector4_conditional(source.nodedef) ||
                                is_vector4_combine(source.nodedef) ||
                                (value_dot_type(source.nodedef, nullptr) && source.links.empty());
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 3 ||
          (index != node.int_inputs.end() && index->second == 3 && !has_native_w) ||
          input == node.links.end() || !validate_link(input->second, Type::Vector4, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float || node.int_inputs.size() != 1 ||
          node.links.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    NodeMix unused_mix_type;
    if (NodeMathType unused; color_binary_component_math_type(node.nodedef, &unused)) {
      const bool scalar_second = color_binary_component_math_uses_scalar_second(node.nodedef);
      const auto first = node.links.find("in1"); const auto second = node.links.find("in2");
      if (first == node.links.end() || second == node.links.end() ||
          !validate_link(first->second, Type::Color3, *nodes_by_name) ||
          !validate_link(second->second, scalar_second ? Type::Float : Type::Color3, *nodes_by_name) ||
          node.links.size() != 2 || node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == clamp_color3_id || node.nodedef == clamp_color3fa_id) {
      const bool scalar_bounds = node.nodedef == clamp_color3fa_id;
      const auto input = node.color3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      if ((input == node.color3_inputs.end()) == (input_link == node.links.end()) ||
          (input_link != node.links.end() && !validate_link(input_link->second, Type::Color3, *nodes_by_name)) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          node.color3_inputs.size() != (input == node.color3_inputs.end() ? (scalar_bounds ? 0 : 2) : (scalar_bounds ? 1 : 3)) ||
          node.inputs.size() != (scalar_bounds ? 2 : 0) || output == node.outputs.end() ||
          output->second != Type::Color3 || node.outputs.size() != 1) return false;
      const float3 low = scalar_bounds ? make_float3(node.inputs.at("low")) : node.color3_inputs.at("low");
      const float3 high = scalar_bounds ? make_float3(node.inputs.at("high")) : node.color3_inputs.at("high");
      if (!std::isfinite(low.x) || !std::isfinite(low.y) || !std::isfinite(low.z) ||
          !std::isfinite(high.x) || !std::isfinite(high.y) || !std::isfinite(high.z) ||
          low.x > high.x || low.y > high.y || low.z > high.z) return false;
      continue;
    }
    if (is_safepower_color3(node.nodedef)) {
      const bool scalar_exponent = safepower_color3_uses_scalar_exponent(node.nodedef);
      const auto finite_color = [](const float3 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
      };
      const bool first_literal = node.color3_inputs.contains("in1");
      const bool first_link = node.links.contains("in1");
      const bool second_literal = scalar_exponent ? node.inputs.contains("in2") :
                                                    node.color3_inputs.contains("in2");
      const bool second_link = node.links.contains("in2");
      if (first_literal == first_link || second_literal == second_link ||
          (first_literal && !finite_color(node.color3_inputs.at("in1"))) ||
          (first_link &&
           !validate_link(node.links.at("in1"), Type::Color3, *nodes_by_name)) ||
          (second_literal &&
           (scalar_exponent ? !std::isfinite(node.inputs.at("in2")) :
                              !finite_color(node.color3_inputs.at("in2")))) ||
          (second_link &&
           !(scalar_exponent ?
                 validate_finite_float_link(node.links.at("in2"), *nodes_by_name) :
                 validate_link(node.links.at("in2"), Type::Color3, *nodes_by_name))) ||
          node.links.size() + node.inputs.size() + node.color3_inputs.size() != 2 ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3)
      {
        return false;
      }
      continue;
    }
    if (color_scalar_mix_type(node.nodedef, &unused_mix_type)) {
      const auto color = node.links.find("in1");
      const auto scalar = node.links.find("in2");
      const auto output = node.outputs.find("out");
      if (color == node.links.end() || scalar == node.links.end() ||
          !validate_link(color->second, Type::Color3, *nodes_by_name) ||
          !validate_link(scalar->second, Type::Float, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3 ||
          node.links.size() != 2 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == invert_color3fa_id) {
      const auto finite_color = [](const float3 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
      };
      const bool amount_literal = node.inputs.contains("amount");
      const bool amount_link = node.links.contains("amount");
      const bool input_literal = node.color3_inputs.contains("in");
      const bool input_link = node.links.contains("in");
      if (amount_literal == amount_link || input_literal == input_link ||
          (amount_literal && !std::isfinite(node.inputs.at("amount"))) ||
          (amount_link &&
           !validate_finite_float_link(node.links.at("amount"), *nodes_by_name)) ||
          (input_literal && !finite_color(node.color3_inputs.at("in"))) ||
          (input_link && !validate_link(node.links.at("in"), Type::Color3, *nodes_by_name)) ||
          node.links.size() + node.inputs.size() + node.color3_inputs.size() != 2 ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3)
      {
        return false;
      }
      continue;
    }
    if (color_mix_type(node.nodedef, &unused_mix_type)) {
      const bool is_invert = node.nodedef == invert_color3_id;
      const auto first = node.links.find(is_invert ? "amount" : "in1");
      const auto second = node.links.find(is_invert ? "in" : "in2");
      const auto output = node.outputs.find("out");
      if (first == node.links.end() || second == node.links.end() ||
          !validate_link(first->second, Type::Color3, *nodes_by_name) ||
          !validate_link(second->second, Type::Color3, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == extract_color3_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 2 ||
          input == node.links.end() ||
          !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float)
      {
        return false;
      }
      continue;
    }

    if (NodeMathType unused; color_unary_math_type(node.nodedef, &unused)) {
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (is_color_conditional(node.nodedef)) {
      const auto valid_float = [&](const char *name) {
        return (node.inputs.contains(name) && std::isfinite(node.inputs.at(name))) ||
               (node.links.contains(name) && validate_link(node.links.at(name), Type::Float, *nodes_by_name));
      };
      const auto valid_color = [&](const char *name) {
        return node.color3_inputs.contains(name) ||
               (node.links.contains(name) && validate_link(node.links.at(name), Type::Color3, *nodes_by_name));
      };
      if (!valid_float("value1") || !valid_float("value2") || !valid_color("in1") || !valid_color("in2") ||
          node.inputs.size() + node.color3_inputs.size() + node.links.size() != 4 ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3 || !node.int_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (is_vector_conditional(node.nodedef)) {
      const auto valid_float = [&](const char *name) { return (node.inputs.contains(name) && std::isfinite(node.inputs.at(name))) || (node.links.contains(name) && validate_link(node.links.at(name), Type::Float, *nodes_by_name)); };
      const auto valid_vector = [&](const char *name) { return node.vector3_inputs.contains(name) || (node.links.contains(name) && validate_link(node.links.at(name), Type::Vector3, *nodes_by_name)); };
      if (!valid_float("value1") || !valid_float("value2") || !valid_vector("in1") || !valid_vector("in2") ||
          node.inputs.size() + node.vector3_inputs.size() + node.links.size() != 4 || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Vector3 || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }

    if (node.nodedef == constant_vector2_id) {
      if (node.vector2_inputs.size() != 1 || node.vector2_inputs.find("value") == node.vector2_inputs.end() ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (is_contrast_vector2(node.nodedef)) {
      const bool scalar_parameters = contrast_uses_scalar_parameters(node.nodedef);
      const auto input = node.vector2_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto finite_vector2 = [](const float2 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y);
      };
      if ((input == node.vector2_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector2_inputs.end() && !finite_vector2(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector2, *nodes_by_name)) ||
          (scalar_parameters ?
               (!node.inputs.contains("pivot") || !node.inputs.contains("amount") ||
                !std::isfinite(node.inputs.at("pivot")) || !std::isfinite(node.inputs.at("amount"))) :
               (!node.vector2_inputs.contains("pivot") || !node.vector2_inputs.contains("amount") ||
                !finite_vector2(node.vector2_inputs.at("pivot")) ||
                !finite_vector2(node.vector2_inputs.at("amount")))) ||
          node.inputs.size() != (scalar_parameters ? 2 : 0) ||
          node.vector2_inputs.size() != (scalar_parameters ? (input == node.vector2_inputs.end() ? 0 : 1) :
                                                          (input == node.vector2_inputs.end() ? 2 : 3)) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Vector2 || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_linear_range_vector2(node.nodedef)) {
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const auto input = node.vector2_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto valid_finite_input = [&](const char *name) {
        if (scalar_bounds) {
          const auto value = node.inputs.find(name);
          return value != node.inputs.end() && std::isfinite(value->second);
        }
        const auto value = node.vector2_inputs.find(name);
        return value != node.vector2_inputs.end() && std::isfinite(value->second.x) &&
               std::isfinite(value->second.y);
      };
      if ((input == node.vector2_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector2_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y))) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector2, *nodes_by_name)) ||
          !valid_finite_input("inlow") || !valid_finite_input("inhigh") ||
          !valid_finite_input("outlow") || !valid_finite_input("outhigh") ||
          (!scalar_bounds && (node.vector2_inputs.at("inlow").x == node.vector2_inputs.at("inhigh").x ||
                              node.vector2_inputs.at("inlow").y == node.vector2_inputs.at("inhigh").y)) ||
          (scalar_bounds && node.inputs.at("inlow") == node.inputs.at("inhigh")) ||
          node.vector2_inputs.size() != (scalar_bounds ? (input == node.vector2_inputs.end() ? 0 : 1) :
                                                        (input == node.vector2_inputs.end() ? 4 : 5)) ||
          node.inputs.size() != (scalar_bounds ? 4 : 0) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          (!scalar_bounds && !node.inputs.empty()) || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Vector2 || node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == range_vector2_id) {
        const auto doclamp = node.int_inputs.find("doclamp");
        const float2 &outlow = node.vector2_inputs.at("outlow");
        const float2 &outhigh = node.vector2_inputs.at("outhigh");
        if (doclamp == node.int_inputs.end() || (doclamp->second != 0 && doclamp->second != 1) ||
            node.int_inputs.size() != 1 ||
            (doclamp->second && (outlow.x > outhigh.x || outlow.y > outhigh.y)))
        {
          return false;
        }
      }
      else if (!node.int_inputs.empty()) {
        return false;
      }
      continue;
    }
    if (is_contrast_vector3(node.nodedef)) {
      const bool scalar_parameters = contrast_uses_scalar_parameters(node.nodedef);
      const auto input = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto finite_vector3 = [](const float3 &value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
      };
      if ((input == node.vector3_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector3_inputs.end() && !finite_vector3(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          (scalar_parameters ?
               (!node.inputs.contains("pivot") || !node.inputs.contains("amount") ||
                !std::isfinite(node.inputs.at("pivot")) || !std::isfinite(node.inputs.at("amount"))) :
               (!node.vector3_inputs.contains("pivot") || !node.vector3_inputs.contains("amount") ||
                !finite_vector3(node.vector3_inputs.at("pivot")) ||
                !finite_vector3(node.vector3_inputs.at("amount")))) ||
          node.inputs.size() != (scalar_parameters ? 2 : 0) ||
          node.vector3_inputs.size() != (scalar_parameters ? (input == node.vector3_inputs.end() ? 0 : 1) :
                                                          (input == node.vector3_inputs.end() ? 2 : 3)) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Vector3 || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_linear_range_vector3(node.nodedef)) {
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const auto input = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto finite_bound = [&](const char *name) {
        if (scalar_bounds) {
          const auto value = node.inputs.find(name);
          return value != node.inputs.end() && std::isfinite(value->second);
        }
        const auto value = node.vector3_inputs.find(name);
        return value != node.vector3_inputs.end() && std::isfinite(value->second.x) &&
               std::isfinite(value->second.y) && std::isfinite(value->second.z);
      };
      if ((input == node.vector3_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector3_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y) ||
            !std::isfinite(input->second.z))) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          !finite_bound("inlow") || !finite_bound("inhigh") || !finite_bound("outlow") ||
          !finite_bound("outhigh") ||
          (scalar_bounds ? node.inputs.at("inlow") == node.inputs.at("inhigh") :
                           (node.vector3_inputs.at("inlow").x == node.vector3_inputs.at("inhigh").x ||
                            node.vector3_inputs.at("inlow").y == node.vector3_inputs.at("inhigh").y ||
                            node.vector3_inputs.at("inlow").z == node.vector3_inputs.at("inhigh").z)) ||
          node.vector3_inputs.size() != (scalar_bounds ? (input == node.vector3_inputs.end() ? 0 : 1) :
                                                         (input == node.vector3_inputs.end() ? 4 : 5)) ||
          node.inputs.size() != (scalar_bounds ? 4 : 0) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != Type::Vector3 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == range_vector3_id || node.nodedef == range_vector3fa_id) {
        const auto doclamp = node.int_inputs.find("doclamp");
        const float3 outlow = scalar_bounds ? make_float3(node.inputs.at("outlow")) :
                                             node.vector3_inputs.at("outlow");
        const float3 outhigh = scalar_bounds ? make_float3(node.inputs.at("outhigh")) :
                                              node.vector3_inputs.at("outhigh");
        if (doclamp == node.int_inputs.end() || (doclamp->second != 0 && doclamp->second != 1) ||
            node.int_inputs.size() != 1 ||
            (doclamp->second &&
             (outlow.x > outhigh.x || outlow.y > outhigh.y || outlow.z > outhigh.z)))
        {
          return false;
        }
      }
      else if (!node.int_inputs.empty()) {
        return false;
      }
      continue;
    }
    if (node.nodedef == clamp_vector2_id) {
      const auto input = node.vector2_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto low = node.vector2_inputs.find("low");
      const auto high = node.vector2_inputs.find("high");
      const auto output = node.outputs.find("out");
      if ((input == node.vector2_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector2_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y))) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector2, *nodes_by_name)) ||
          low == node.vector2_inputs.end() || high == node.vector2_inputs.end() ||
          !std::isfinite(low->second.x) || !std::isfinite(low->second.y) ||
          !std::isfinite(high->second.x) || !std::isfinite(high->second.y) ||
          low->second.x > high->second.x || low->second.y > high->second.y ||
          node.vector2_inputs.size() != (input == node.vector2_inputs.end() ? 2 : 3) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Vector2 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == clamp_vector2fa_id) {
      const auto input = node.vector2_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto low = node.inputs.find("low");
      const auto high = node.inputs.find("high");
      const auto output = node.outputs.find("out");
      if ((input == node.vector2_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector2_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y))) ||
          (input_link != node.links.end() && !validate_link(input_link->second, Type::Vector2, *nodes_by_name)) ||
          low == node.inputs.end() || high == node.inputs.end() || !std::isfinite(low->second) ||
          !std::isfinite(high->second) || low->second > high->second ||
          node.vector2_inputs.size() != (input == node.vector2_inputs.end() ? 0 : 1) ||
          node.inputs.size() != 2 || node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Vector2 || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == "ND_clamp_vector3" || node.nodedef == "ND_clamp_vector3FA") {
      const bool scalar_bounds = node.nodedef == "ND_clamp_vector3FA";
      const auto input = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const bool has_bounds = scalar_bounds ?
                                  node.inputs.find("low") != node.inputs.end() && node.inputs.find("high") != node.inputs.end() :
                                  node.vector3_inputs.find("low") != node.vector3_inputs.end() && node.vector3_inputs.find("high") != node.vector3_inputs.end();
      if ((input == node.vector3_inputs.end()) == (input_link == node.links.end()) ||
          (input != node.vector3_inputs.end() &&
           (!std::isfinite(input->second.x) || !std::isfinite(input->second.y) || !std::isfinite(input->second.z))) ||
          (input_link != node.links.end() && !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          !has_bounds || node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          node.vector3_inputs.size() != (input == node.vector3_inputs.end() ? (scalar_bounds ? 0 : 2) : (scalar_bounds ? 1 : 3)) ||
          node.inputs.size() != (scalar_bounds ? 2 : 0) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != Type::Vector3 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      if (scalar_bounds) {
        if (!std::isfinite(node.inputs.at("low")) || !std::isfinite(node.inputs.at("high")) ||
            node.inputs.at("low") > node.inputs.at("high")) return false;
      }
      else {
        const float3 &low = node.vector3_inputs.at("low");
        const float3 &high = node.vector3_inputs.at("high");
        if (!std::isfinite(low.x) || !std::isfinite(low.y) || !std::isfinite(low.z) ||
            !std::isfinite(high.x) || !std::isfinite(high.y) || !std::isfinite(high.z) ||
            low.x > high.x || low.y > high.y || low.z > high.z) return false;
      }
      continue;
    }
    if (node.nodedef == combine2_vector2_id) {
      for (const char *component : {"in1", "in2"}) {
        const bool has_value = node.inputs.find(component) != node.inputs.end();
        const bool has_link = node.links.find(component) != node.links.end();
        if (has_value == has_link || (has_link && !validate_link(node.links.at(component), Type::Float, *nodes_by_name))) return false;
      }
      if (node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 || node.links.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == convert_vector3_vector2_id) {
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, Type::Vector3, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == place2d_vector2_id) {
      const auto texcoord = node.links.find("texcoord");
      const auto pivot = node.vector2_inputs.find("pivot");
      const auto scale = node.vector2_inputs.find("scale");
      const auto offset = node.vector2_inputs.find("offset");
      const auto rotate = node.inputs.find("rotate");
      const auto operationorder = node.inputs.find("operationorder");
      if (texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          pivot == node.vector2_inputs.end() || scale == node.vector2_inputs.end() || offset == node.vector2_inputs.end() ||
          !std::isfinite(pivot->second.x) || !std::isfinite(pivot->second.y) || !std::isfinite(scale->second.x) ||
          !std::isfinite(scale->second.y) || !std::isfinite(offset->second.x) || !std::isfinite(offset->second.y) ||
          scale->second.x == 0.0f || scale->second.y == 0.0f || rotate == node.inputs.end() ||
          operationorder == node.inputs.end() || !std::isfinite(rotate->second) || !std::isfinite(operationorder->second) ||
          node.links.size() != 1 || node.vector2_inputs.size() != 3 || node.inputs.size() != 2 ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == rotate2d_vector2_id) {
      const bool input_value = node.vector2_inputs.find("in") != node.vector2_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      const bool amount_value = node.inputs.find("amount") != node.inputs.end();
      const bool amount_link = node.links.find("amount") != node.links.end();
      if ((input_value && input_link) || (amount_value && amount_link) ||
          (input_value && (!std::isfinite(node.vector2_inputs.at("in").x) ||
                           !std::isfinite(node.vector2_inputs.at("in").y))) ||
          (amount_value && !std::isfinite(node.inputs.at("amount"))) ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector2, *nodes_by_name)) ||
          (amount_link && !validate_link(node.links.at("amount"), Type::Float, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() != size_t(input_link) + size_t(amount_link) ||
          node.vector2_inputs.size() != size_t(input_value) ||
          node.inputs.size() != size_t(amount_value) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == extract_vector2_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 1 || input == node.links.end() ||
          !validate_link(input->second, Type::Vector2, *nodes_by_name) || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Float || node.links.size() != 1 || node.int_inputs.size() != 1 ||
          !node.inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == roughness_anisotropy_id || node.nodedef == glossiness_anisotropy_id) {
      /* See roughness_anisotropy_id's declaration comment above for the real
       * nodedefs/reference implementations this validates against. */
      const char *first_name = node.nodedef == glossiness_anisotropy_id ? "glossiness" : "roughness";
      bool ok = true;
      for (const char *name : {first_name, "anisotropy"}) {
        const bool has_value = node.inputs.find(name) != node.inputs.end();
        const bool has_link = node.links.find(name) != node.links.end();
        if (has_value == has_link || (has_value && !std::isfinite(node.inputs.at(name))) ||
            (has_link && !validate_link(node.links.at(name), Type::Float, *nodes_by_name)))
        {
          ok = false;
          break;
        }
      }
      if (!ok || node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == open_pbr_anisotropy_id) {
      const auto valid_float_input = [&](const char *name) {
        const bool has_value = node.inputs.find(name) != node.inputs.end();
        const bool has_link = node.links.find(name) != node.links.end();
        return has_value != has_link &&
               (has_value ? std::isfinite(node.inputs.at(name)) :
                            validate_link(node.links.at(name), Type::Float, *nodes_by_name));
      };
      if (!valid_float_input("roughness") || !valid_float_input("anisotropy") ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.inputs.size() != 2 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == blackbody_id) {
      const bool has_value = node.inputs.find("temperature") != node.inputs.end();
      const bool has_link = node.links.find("temperature") != node.links.end();
      if (has_value == has_link || (has_value && !std::isfinite(node.inputs.at("temperature"))) ||
          (has_link && !validate_link(node.links.at("temperature"), Type::Float, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Color3 ||
          node.links.size() + node.inputs.size() != 1 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == roughness_dual_id) {
      /* See roughness_dual_id's declaration comment above for the real
       * nodedef/reference implementation this validates against. */
      const bool has_value = node.vector2_inputs.find("roughness") != node.vector2_inputs.end();
      const bool has_link = node.links.find("roughness") != node.links.end();
      if (has_value == has_link ||
          (has_value && (!std::isfinite(node.vector2_inputs.at("roughness").x) ||
                        !std::isfinite(node.vector2_inputs.at("roughness").y))) ||
          (has_link && !validate_link(node.links.at("roughness"), Type::Vector2, *nodes_by_name)))
      {
        return false;
      }
      if (node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused;
        vector2_binary_component_math_type(node.nodedef, &unused) || is_safepower_vector2(node.nodedef))
    {
      const bool scalar_second = vector2_binary_component_math_uses_scalar_second(node.nodedef) ||
                                 safepower_vector2_uses_scalar_second(node.nodedef);
      const bool divide = node.nodedef == "ND_divide_vector2FA";
      const bool first_value = node.vector2_inputs.find("in1") != node.vector2_inputs.end();
      const bool first_link = node.links.find("in1") != node.links.end();
      const bool second_value = scalar_second ? node.inputs.find("in2") != node.inputs.end() :
                                                 node.vector2_inputs.find("in2") != node.vector2_inputs.end();
      const bool second_link = node.links.find("in2") != node.links.end();
      const auto finite_vector2 = [&](const char *name) {
        const auto value = node.vector2_inputs.find(name);
        return value == node.vector2_inputs.end() ||
               (std::isfinite(value->second.x) && std::isfinite(value->second.y));
      };
      if (first_value == first_link || second_value == second_link ||
          !finite_vector2("in1") || (!scalar_second && !finite_vector2("in2")) ||
          (scalar_second && second_value && !std::isfinite(node.inputs.at("in2"))) ||
          (divide && second_value && (!std::isfinite(node.inputs.at("in2")) || node.inputs.at("in2") == 0.0f)) ||
          (first_link && !validate_link(node.links.at("in1"), Type::Vector2, *nodes_by_name)) ||
          (second_link && !validate_link(node.links.at("in2"), scalar_second ? Type::Float : Type::Vector2, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused;
        vector3_binary_component_math_type(node.nodedef, &unused) || is_safepower_vector3(node.nodedef))
    {
      const bool scalar_second = vector3_binary_component_math_uses_scalar_second(node.nodedef) ||
                                 safepower_vector3_uses_scalar_second(node.nodedef);
      const bool divide = node.nodedef == "ND_divide_vector3FA";
      const bool first_value = node.vector3_inputs.find("in1") != node.vector3_inputs.end();
      const bool first_link = node.links.find("in1") != node.links.end();
      const bool second_value = scalar_second ? node.inputs.find("in2") != node.inputs.end() :
                                                 node.vector3_inputs.find("in2") != node.vector3_inputs.end();
      const bool second_link = node.links.find("in2") != node.links.end();
      const auto finite_vector3 = [&](const char *name) {
        const auto value = node.vector3_inputs.find(name);
        return value == node.vector3_inputs.end() ||
               (std::isfinite(value->second.x) && std::isfinite(value->second.y) &&
                std::isfinite(value->second.z));
      };
      if (first_value == first_link || second_value == second_link ||
          !finite_vector3("in1") || (!scalar_second && !finite_vector3("in2")) ||
          (scalar_second && second_value && !std::isfinite(node.inputs.at("in2"))) ||
          (divide && second_value && (!std::isfinite(node.inputs.at("in2")) || node.inputs.at("in2") == 0.0f)) ||
          (first_link && !validate_link(node.links.at("in1"), Type::Vector3, *nodes_by_name)) ||
          (second_link && !validate_link(node.links.at("in2"), scalar_second ? Type::Float : Type::Vector3, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() + node.vector3_inputs.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (bool scalar_amount; vector2_invert_type(node.nodedef, &scalar_amount)) {
      const bool input_value = node.vector2_inputs.find("in") != node.vector2_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      const bool amount_value = scalar_amount ? node.inputs.find("amount") != node.inputs.end() :
                                                node.vector2_inputs.find("amount") != node.vector2_inputs.end();
      const bool amount_link = node.links.find("amount") != node.links.end();
      if (input_value == input_link || amount_value == amount_link ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector2, *nodes_by_name)) ||
          (amount_link && !validate_link(node.links.at("amount"), scalar_amount ? Type::Float : Type::Vector2, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (bool scalar_amount; vector3_invert_type(node.nodedef, &scalar_amount)) {
      const bool input_value = node.vector3_inputs.find("in") != node.vector3_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      const bool amount_value = scalar_amount ? node.inputs.find("amount") != node.inputs.end() :
                                                node.vector3_inputs.find("amount") != node.vector3_inputs.end();
      const bool amount_link = node.links.find("amount") != node.links.end();
      if (input_value == input_link || amount_value == amount_link ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector3, *nodes_by_name)) ||
          (amount_link && !validate_link(node.links.at("amount"), scalar_amount ? Type::Float : Type::Vector3, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() + node.vector3_inputs.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (bool scalar_edges; vector2_smoothstep_type(node.nodedef, &scalar_edges)) {
      const bool input_value = node.vector2_inputs.find("in") != node.vector2_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      bool valid_edges = false;
      if (scalar_edges) {
        const auto low = node.inputs.find("low"), high = node.inputs.find("high");
        valid_edges = low != node.inputs.end() && high != node.inputs.end() &&
                      std::isfinite(low->second) && std::isfinite(high->second) && low->second < high->second;
      }
      else {
        const auto low = node.vector2_inputs.find("low"), high = node.vector2_inputs.find("high");
        valid_edges = low != node.vector2_inputs.end() && high != node.vector2_inputs.end() &&
                      std::isfinite(low->second.x) && std::isfinite(low->second.y) &&
                      std::isfinite(high->second.x) && std::isfinite(high->second.y) &&
                      low->second.x < high->second.x && low->second.y < high->second.y;
      }
      if (input_value == input_link || !valid_edges ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector2, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() + node.inputs.size() != 3 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (bool scalar_edges; vector3_smoothstep_type(node.nodedef, &scalar_edges)) {
      const bool input_value = node.vector3_inputs.find("in") != node.vector3_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      bool valid_edges = false;
      if (scalar_edges) {
        const auto low = node.inputs.find("low"), high = node.inputs.find("high");
        valid_edges = low != node.inputs.end() && high != node.inputs.end() &&
                      std::isfinite(low->second) && std::isfinite(high->second) && low->second < high->second;
      }
      else {
        const auto low = node.vector3_inputs.find("low"), high = node.vector3_inputs.find("high");
        valid_edges = low != node.vector3_inputs.end() && high != node.vector3_inputs.end() &&
                      std::isfinite(low->second.x) && std::isfinite(low->second.y) && std::isfinite(low->second.z) &&
                      std::isfinite(high->second.x) && std::isfinite(high->second.y) && std::isfinite(high->second.z) &&
                      low->second.x < high->second.x && low->second.y < high->second.y && low->second.z < high->second.z;
      }
      if (input_value == input_link || !valid_edges ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector3, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() + node.vector3_inputs.size() + node.inputs.size() != 3 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused; vector2_domain_math_type(node.nodedef, &unused)) {
      const bool has_value = node.vector2_inputs.find("in") != node.vector2_inputs.end();
      const bool has_link = node.links.find("in") != node.links.end();
      if (has_value == has_link ||
          (has_link && !validate_link(node.links.at("in"), Type::Vector2, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused; vector3_domain_math_type(node.nodedef, &unused)) {
      const bool has_value = node.vector3_inputs.find("in") != node.vector3_inputs.end();
      const bool has_link = node.links.find("in") != node.links.end();
      if (has_value == has_link ||
          (has_link && !validate_link(node.links.at("in"), Type::Vector3, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() + node.vector3_inputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused; vector2_atan2_type(node.nodedef, &unused)) {
      for (const char *name : {"iny", "inx"}) {
        const bool has_value = node.vector2_inputs.find(name) != node.vector2_inputs.end();
        const bool has_link = node.links.find(name) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(name), Type::Vector2, *nodes_by_name))) return false;
      }
      if (node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 ||
          node.links.size() + node.vector2_inputs.size() != 2 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (NodeMathType unused; vector3_atan2_type(node.nodedef, &unused)) {
      for (const char *name : {"iny", "inx"}) {
        const bool has_value = node.vector3_inputs.find(name) != node.vector3_inputs.end();
        const bool has_link = node.links.find(name) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(name), Type::Vector3, *nodes_by_name))) return false;
      }
      if (node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() + node.vector3_inputs.size() != 2 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    NodeVectorMathType vector2_type;
    if (vector2_math_type(node.nodedef, &vector2_type)) {
      const bool unary = vector2_math_is_unary(node.nodedef);
      const bool scalar_second = vector2_math_uses_scalar_second(node.nodedef);
      const char *first_name = unary ? "in" : "in1";
      for (const char *input_name : {first_name, unary ? nullptr : "in2"}) {
        if (input_name == nullptr) {
          continue;
        }
        const bool has_value = scalar_second && string(input_name) == "in2" ?
                                   node.inputs.find(input_name) != node.inputs.end() :
                                   node.vector2_inputs.find(input_name) != node.vector2_inputs.end();
        const bool has_link = node.links.find(input_name) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(input_name),
                                        scalar_second && string(input_name) == "in2" ? Type::Float : Type::Vector2,
                                        *nodes_by_name))) return false;
      }
      const size_t input_count = unary ? 1 : 2;
      const Type output_type = vector2_math_returns_float(node.nodedef) ? Type::Float : Type::Vector2;
      if (node.outputs.size() != 1 || node.outputs.at("out") != output_type ||
          node.links.size() + node.vector2_inputs.size() + node.inputs.size() != input_count ||
          (!scalar_second && !node.inputs.empty()) || (scalar_second && node.inputs.size() > 1) ||
          (node.nodedef == "ND_divide_vector2" &&
           (node.links.find("in2") != node.links.end() || node.vector2_inputs.find("in2") == node.vector2_inputs.end() ||
            !std::isfinite(node.vector2_inputs.at("in2").x) ||
            !std::isfinite(node.vector2_inputs.at("in2").y) ||
            node.vector2_inputs.at("in2").x == 0.0f || node.vector2_inputs.at("in2").y == 0.0f)) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == geompropvalue_vector2_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Vector2)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_float_id || node.nodedef == geompropvalue_color3_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      const Type output_type = node.nodedef == geompropvalue_float_id ? Type::Float : Type::Color3;
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != output_type ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_color4_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Color4 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_vector3_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second != "Nworld" ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == normal_vector3_id || node.nodedef == position_vector3_id ||
        node.nodedef == viewdirection_vector3_id) {
      const auto space = node.string_inputs.find("space");
      const auto output = node.outputs.find("out");
      if (space == node.string_inputs.end() || space->second != "world" ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == usdprimvarreader_float_id || node.nodedef == usdprimvarreader_vector2_id ||
        node.nodedef == usdprimvarreader_vector3_id) {
      const auto varname = node.string_inputs.find("varname");
      const auto output = node.outputs.find("out");
      const Type output_type = node.nodedef == usdprimvarreader_float_id ?
                                    Type::Float :
                                node.nodedef == usdprimvarreader_vector2_id ? Type::Vector2 :
                                                                               Type::Vector3;
      if (varname == node.string_inputs.end() || varname->second.empty() ||
          output == node.outputs.end() || output->second != output_type ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == texcoord_vector3_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (is_space_transform(node.nodedef)) {
      const auto input = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto fromspace = node.string_inputs.find("fromspace");
      const auto tospace = node.string_inputs.find("tospace");
      const auto output = node.outputs.find("out");
      if ((input == node.vector3_inputs.end()) == (input_link == node.links.end()) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          fromspace == node.string_inputs.end() || tospace == node.string_inputs.end() ||
          !is_supported_transform_space(fromspace->second) ||
          !is_supported_transform_space(tospace->second) || output == node.outputs.end() ||
          output->second != Type::Vector3 || node.outputs.size() != 1 ||
          node.vector3_inputs.size() != size_t(input != node.vector3_inputs.end()) ||
          node.links.size() != size_t(input_link != node.links.end()) ||
          node.string_inputs.size() != 2 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.float4_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (Type triplanar_type; triplanarprojection_type(node.nodedef, &triplanar_type)) {
      const auto output = node.outputs.find("out");
      const auto position = node.links.find("position");
      const auto normal = node.links.find("normal");
      const auto blend_link = node.links.find("blend");
      const auto blend = node.inputs.find("blend");
      const auto upaxis = node.int_inputs.find("upaxis");
      const auto filtertype = node.string_inputs.find("filtertype");
      const bool is_vector = triplanar_type == Type::Vector2 || triplanar_type == Type::Vector3 ||
                             triplanar_type == Type::Vector4;
      const bool valid_default = triplanar_type == Type::Float ?
                                     (!node.inputs.contains("default") ||
                                      std::isfinite(node.inputs.at("default"))) :
                                 triplanar_type == Type::Color3 ?
                                     (!node.color3_inputs.contains("default") ||
                                      finite_value(node.color3_inputs.at("default"))) :
                                 triplanar_type == Type::Color4 ?
                                     (!node.float4_inputs.contains("default") ||
                                      finite_value(node.float4_inputs.at("default"))) :
                                 triplanar_type == Type::Vector2 ?
                                     (!node.vector2_inputs.contains("default") ||
                                      finite_value(node.vector2_inputs.at("default"))) :
                                 triplanar_type == Type::Vector3 ?
                                     (!node.vector3_inputs.contains("default") ||
                                      finite_value(node.vector3_inputs.at("default"))) :
                                     (!node.vector4_inputs.contains("default") ||
                                      finite_value(node.vector4_inputs.at("default")));
      for (const char *file_input : {"filex", "filey", "filez"}) {
        const auto file = node.asset_inputs.find(file_input);
        if (file == node.asset_inputs.end() || file->second.empty() ||
            path_is_relative(file->second) || !path_is_file(file->second) ||
            path_file_size(file->second) == 0)
        {
          return false;
        }
      }
      const bool ok = position != node.links.end() &&
                      validate_link(position->second, Type::Vector3, *nodes_by_name) &&
                      normal != node.links.end() &&
                      validate_link(normal->second, Type::Vector3, *nodes_by_name) &&
                      ((blend == node.inputs.end()) != (blend_link == node.links.end())) &&
                      (blend != node.inputs.end() ?
                           (std::isfinite(blend->second) && blend->second >= 0.0f) :
                           validate_link(blend_link->second, Type::Float, *nodes_by_name)) &&
                      upaxis != node.int_inputs.end() && upaxis->second == 2 &&
                      filtertype != node.string_inputs.end() &&
                      (filtertype->second == "closest" || filtertype->second == "linear" ||
                       filtertype->second == "cubic") &&
                      output != node.outputs.end() && output->second == triplanar_type &&
                      node.outputs.size() == 1 && valid_default &&
                      node.asset_inputs.size() == 3 &&
                      node.links.size() == 2 + size_t(blend_link != node.links.end()) &&
                      node.inputs.size() == size_t(blend != node.inputs.end()) +
                                              size_t(triplanar_type == Type::Float &&
                                                     node.inputs.contains("default")) &&
                      node.int_inputs.size() == 1 &&
                      node.string_inputs.size() == 1 && node.color3_inputs.size() <= 1 &&
                      node.float4_inputs.size() <= 1 && node.vector2_inputs.size() <= 1 &&
                      node.vector3_inputs.size() <= 1 && node.vector4_inputs.size() <= 1 &&
                      (!node.color3_inputs.contains("default") ||
                       triplanar_type == Type::Color3) &&
                      (!node.float4_inputs.contains("default") ||
                       triplanar_type == Type::Color4) &&
                      (!node.vector2_inputs.contains("default") ||
                       triplanar_type == Type::Vector2) &&
                      (!node.vector3_inputs.contains("default") ||
                       triplanar_type == Type::Vector3) &&
                      (!node.vector4_inputs.contains("default") ||
                       triplanar_type == Type::Vector4);
      if (!ok || (!is_vector && (!node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
                                 !node.vector4_inputs.empty())))
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_color3_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    if (is_color4_operation(node.nodedef)) {
      const bool unary = color4_unary_math_type(node.nodedef, nullptr);
      const bool scalar_invert = node.nodedef == invert_color4fa_id;
      const bool invert = is_color4_invert(node.nodedef);
      const bool safepower = is_safepower_color4(node.nodedef);
      const bool scalar_second = color4_binary_uses_scalar_second(node.nodedef);
      const bool full_clamp = node.nodedef == clamp_color4_id;
      const bool scalar_clamp = node.nodedef == clamp_color4fa_id;
      const bool clamp = full_clamp || scalar_clamp;
      const char *first_name = (unary || scalar_invert || clamp) ?
                                   "in" :
                                   (invert ? "amount" : "in1");
      const char *second_name = scalar_invert ? "amount" : (invert ? "in" : "in2");
      const auto output = node.outputs.find("out");
      const auto valid_color4_operand = [&](const char *name, const float4 &default_value) {
        const auto literal = node.float4_inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.float4_inputs.end() && link == node.links.end()) {
          return color4_has_finite_components(default_value);
        }
        return (literal != node.float4_inputs.end()) != (link != node.links.end()) &&
               (literal != node.float4_inputs.end() ?
                    color4_has_finite_components(literal->second) :
                    validate_link(link->second, Type::Color4, *nodes_by_name));
      };
      const auto valid_float_operand = [&](const char *name, const float default_value) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.inputs.end() && link == node.links.end()) {
          return std::isfinite(default_value);
        }
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ?
                    std::isfinite(literal->second) :
                    validate_link(link->second, Type::Float, *nodes_by_name));
      };
      const float4 first_default = (invert && !scalar_invert) ?
                                       make_float4(1.0f, 1.0f, 1.0f, 1.0f) :
                                       make_float4(0.0f, 0.0f, 0.0f, 0.0f);
      const float4 second_default =
          (safepower || color4_binary_uses_identity_second(node.nodedef)) ?
              make_float4(1.0f, 1.0f, 1.0f, 1.0f) :
              make_float4(0.0f, 0.0f, 0.0f, 0.0f);
      const bool valid_scalar_clamp =
          scalar_clamp && valid_color4_operand("in", first_default) &&
          valid_float_operand("low", 0.0f) && valid_float_operand("high", 1.0f) &&
          node.float4_inputs.size() + node.inputs.size() + node.links.size() <= 3 &&
          (!node.inputs.contains("low") || !node.inputs.contains("high") ||
           node.inputs.at("low") <= node.inputs.at("high"));
      const bool valid_full_clamp =
          full_clamp && valid_color4_operand("in", first_default) &&
          valid_color4_operand("low", make_float4(0.0f, 0.0f, 0.0f, 0.0f)) &&
          valid_color4_operand("high", make_float4(1.0f, 1.0f, 1.0f, 1.0f)) &&
          node.float4_inputs.size() + node.inputs.size() + node.links.size() <= 3 &&
          (!node.float4_inputs.contains("low") || !node.float4_inputs.contains("high") ||
           (node.float4_inputs.at("low").x <= node.float4_inputs.at("high").x &&
            node.float4_inputs.at("low").y <= node.float4_inputs.at("high").y &&
            node.float4_inputs.at("low").z <= node.float4_inputs.at("high").z &&
            node.float4_inputs.at("low").w <= node.float4_inputs.at("high").w));
      const bool valid_non_clamp =
          !clamp && valid_color4_operand(first_name, first_default) &&
          (unary ||
           (scalar_second ? valid_float_operand(second_name, second_default.x) :
                            valid_color4_operand(second_name, second_default))) &&
          node.float4_inputs.size() + node.inputs.size() + node.links.size() <= (unary ? 1 : 2);
      if ((!valid_scalar_clamp && !valid_full_clamp && !valid_non_clamp) ||
          std::any_of(node.float4_inputs.begin(),
                      node.float4_inputs.end(),
                      [&](const auto &input) {
                        return full_clamp ?
                                   input.first != "in" && input.first != "low" && input.first != "high" :
                               scalar_clamp ? input.first != "in" :
                                              input.first != first_name &&
                                                  (unary || scalar_second ||
                                                   input.first != second_name);
                      }) ||
          std::any_of(node.links.begin(), node.links.end(), [&](const auto &input) {
            return clamp ?
                       input.first != "in" && input.first != "low" && input.first != "high" :
                       input.first != first_name && (unary || input.first != second_name);
          }) ||
          std::any_of(node.inputs.begin(), node.inputs.end(), [&](const auto &input) {
            return scalar_clamp ? input.first != "low" && input.first != "high" :
                   full_clamp ? true : (!scalar_second || input.first != second_name);
          }) ||
          ((node.nodedef == divide_color4_id || node.nodedef == modulo_color4_id) &&
           node.float4_inputs.contains(second_name) &&
           (node.float4_inputs.at(second_name).x == 0.0f ||
            node.float4_inputs.at(second_name).y == 0.0f ||
            node.float4_inputs.at(second_name).z == 0.0f ||
            node.float4_inputs.at(second_name).w == 0.0f)) ||
          ((node.nodedef == divide_color4fa_id || node.nodedef == modulo_color4fa_id) &&
           node.inputs.contains(second_name) && node.inputs.at(second_name) == 0.0f) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Color4 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_color4_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      const auto default_value = node.float4_inputs.find("default");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color4 ||
          (default_value != node.float4_inputs.end() &&
           (!std::isfinite(default_value->second.x) || !std::isfinite(default_value->second.y) ||
            !std::isfinite(default_value->second.z) || !std::isfinite(default_value->second.w))) ||
          node.asset_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          node.float4_inputs.size() > 1 ||
          (default_value == node.float4_inputs.end() && !node.float4_inputs.empty()) ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == extract_color4_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 3 ||
          input == node.links.end() || !validate_link(input->second, Type::Color4, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float || node.int_inputs.size() != 1 ||
          node.links.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_color4_color3_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Color4, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3 || node.links.size() != 1 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_color3_color4_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color4 || node.links.size() != 1 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_float_vector4_id || node.nodedef == convert_boolean_vector4_id ||
        node.nodedef == convert_integer_vector4_id || node.nodedef == convert_float_color4_id ||
        node.nodedef == convert_boolean_color4_id || node.nodedef == convert_integer_color4_id)
    {
      const bool vector4 = node.nodedef == convert_float_vector4_id ||
                           node.nodedef == convert_boolean_vector4_id ||
                           node.nodedef == convert_integer_vector4_id;
      const Type input_type = (node.nodedef == convert_float_vector4_id ||
                               node.nodedef == convert_float_color4_id) ?
                                  Type::Float :
                              (node.nodedef == convert_boolean_vector4_id ||
                               node.nodedef == convert_boolean_color4_id) ? Type::Boolean :
                                                                          Type::Integer;
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, input_type, *nodes_by_name) ||
          output == node.outputs.end() || output->second != (vector4 ? Type::Vector4 : Type::Color4) ||
          node.links.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == combine2_color4cf_id) {
      const auto color = node.links.find("in1");
      const auto alpha = node.links.find("in2");
      const auto output = node.outputs.find("out");
      if (color == node.links.end() || alpha == node.links.end() ||
          !validate_link(color->second, Type::Color3, *nodes_by_name) ||
          !validate_link(alpha->second, Type::Float, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color4 || node.links.size() != 2 ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == combine4_color4_id) {
      size_t literal_count = 0;
      size_t link_count = 0;
      for (const char *input_name : {"in1", "in2", "in3", "in4"}) {
        const bool has_literal = node.inputs.contains(input_name);
        const bool has_link = node.links.contains(input_name);
        if (has_literal == has_link || (has_literal && !std::isfinite(node.inputs.at(input_name))) ||
            (has_link && !validate_link(node.links.at(input_name), Type::Float, *nodes_by_name)))
        {
          return false;
        }
        literal_count += size_t(has_literal);
        link_count += size_t(has_link);
      }
      const auto output = node.outputs.find("out");
      if (literal_count + link_count != 4 || node.inputs.size() != literal_count ||
          node.links.size() != link_count || output == node.outputs.end() ||
          output->second != Type::Color4 || node.outputs.size() != 1 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == checkerboard_color3_id) {
      const auto color1 = node.color3_inputs.find("color1");
      const auto color2 = node.color3_inputs.find("color2");
      const auto tiling = node.vector2_inputs.find("uvtiling");
      const auto offset = node.vector2_inputs.find("uvoffset");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (color1 == node.color3_inputs.end() || color2 == node.color3_inputs.end() ||
          tiling == node.vector2_inputs.end() || offset == node.vector2_inputs.end() ||
          texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3 || node.color3_inputs.size() != 2 ||
          node.vector2_inputs.size() != 2 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_float_id || node.nodedef == image_vector2_id ||
        node.nodedef == image_vector4_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      const Type output_type = node.nodedef == image_float_id ? Type::Float :
                               node.nodedef == image_vector2_id ? Type::Vector2 : Type::Vector4;
      const auto default_value = node.vector4_inputs.find("default");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != output_type ||
          (node.nodedef == image_vector4_id && default_value != node.vector4_inputs.end() &&
           !color4_has_finite_components(default_value->second)) ||
          node.asset_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          (node.nodedef == image_vector4_id ? node.vector4_inputs.size() > 1 :
                                               !node.vector4_inputs.empty()) ||
          !node.string_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id ||
        is_color4_ramp(node.nodedef))
    {
      const bool color4 = is_color4_ramp(node.nodedef);
      const bool top_to_bottom = node.nodedef == ramptb_color3_id ||
                                 node.nodedef == ramptb_color4_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      const auto first_color3 = node.color3_inputs.find(first_name);
      const auto second_color3 = node.color3_inputs.find(second_name);
      const auto first_color4 = node.float4_inputs.find(first_name);
      const auto second_color4 = node.float4_inputs.find(second_name);
      const auto first_link = node.links.find(first_name);
      const auto second_link = node.links.find(second_name);
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if ((color4 ?
               ((first_color4 != node.float4_inputs.end() &&
                 !color4_has_finite_components(first_color4->second)) ||
                (second_color4 != node.float4_inputs.end() &&
                 !color4_has_finite_components(second_color4->second)) ||
                (first_link != node.links.end() &&
                 !validate_link(first_link->second, Type::Color4, *nodes_by_name)) ||
                (second_link != node.links.end() &&
                 !validate_link(second_link->second, Type::Color4, *nodes_by_name)) ||
                (first_color4 != node.float4_inputs.end() && first_link != node.links.end()) ||
                (second_color4 != node.float4_inputs.end() && second_link != node.links.end())) :
               (first_color3 == node.color3_inputs.end() ||
                second_color3 == node.color3_inputs.end())) ||
          texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() ||
          output->second != (color4 ? Type::Color4 : Type::Color3) ||
          node.color3_inputs.size() != (color4 ? 0 : 2) ||
          node.float4_inputs.size() > (color4 ? 2 : 0) ||
          node.links.size() != 1 + size_t(color4 && first_link != node.links.end()) +
                                   size_t(color4 && second_link != node.links.end()) ||
          node.outputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      const auto first = node.inputs.find(first_name);
      const auto second = node.inputs.find(second_name);
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (first == node.inputs.end() || second == node.inputs.end() ||
          !std::isfinite(first->second) || !std::isfinite(second->second) ||
          texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float ||
          node.inputs.size() != 2 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_split(node.nodedef)) {
      const bool top_to_bottom = split_is_top_to_bottom(node.nodedef);
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      const Type value_type = is_scalar_split(node.nodedef) ? Type::Float :
                              is_color3_split(node.nodedef) ? Type::Color3 : Type::Color4;
      const auto has_literal = [&](const char *name) {
        return value_type == Type::Float ? node.inputs.contains(name) :
               value_type == Type::Color3 ? node.color3_inputs.contains(name) :
                                            node.float4_inputs.contains(name);
      };
      const auto finite_literal = [&](const char *name) {
        if (!has_literal(name)) {
          return true;
        }
        return value_type == Type::Float ? std::isfinite(node.inputs.at(name)) :
               value_type == Type::Color4 ? color4_has_finite_components(node.float4_inputs.at(name)) :
                                            true;
      };
      const bool first_literal = has_literal(first_name);
      const bool second_literal = has_literal(second_name);
      const bool first_link = node.links.contains(first_name);
      const bool second_link = node.links.contains(second_name);
      const bool center_literal = node.inputs.contains("center");
      const bool center_link = node.links.contains("center");
      if ((first_literal && first_link) || (second_literal && second_link) ||
          (center_literal && center_link) || !finite_literal(first_name) ||
          !finite_literal(second_name) ||
          (center_literal && !std::isfinite(node.inputs.at("center"))) ||
          (first_link && !validate_link(node.links.at(first_name), value_type, *nodes_by_name)) ||
          (second_link && !validate_link(node.links.at(second_name), value_type, *nodes_by_name)) ||
          (center_link && !validate_link(node.links.at("center"), Type::Float, *nodes_by_name)) ||
          texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != value_type ||
          node.inputs.size() != size_t(value_type == Type::Float ? int(first_literal) + int(second_literal) + int(center_literal) : int(center_literal)) ||
          node.links.size() != 1 + size_t(first_link) + size_t(second_link) + size_t(center_link) ||
          node.outputs.size() != 1 || !node.int_inputs.empty() ||
          (value_type != Type::Color3 && !node.color3_inputs.empty()) ||
          (value_type == Type::Color3 && node.color3_inputs.size() != size_t(first_literal) + size_t(second_literal)) ||
          (value_type != Type::Color4 && !node.float4_inputs.empty()) ||
          (value_type == Type::Color4 && node.float4_inputs.size() != size_t(first_literal) + size_t(second_literal)) ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_vector3_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.asset_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == normalmap_float_id) {
      const auto scale = node.inputs.find("scale");
      const auto input_value = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto linked_source = input_link == node.links.end() ?
                                     nodes_by_name->end() :
                                     nodes_by_name->find(input_link->second.source_node);
      const bool linked_image = linked_source != nodes_by_name->end() &&
                                linked_source->second->nodedef == image_vector3_id;
      if ((scale != node.inputs.end() &&
           (!std::isfinite(scale->second) || scale->second != 1.0f)) ||
          ((input_value != node.vector3_inputs.end()) == (input_link != node.links.end())) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          (input_link != node.links.end() && !linked_image &&
           linked_source->second->nodedef != "ND_constant_vector3" &&
           linked_source->second->nodedef != "ND_normalize_vector3" &&
           linked_source->second->nodedef != "ND_add_vector3" &&
           linked_source->second->nodedef != "ND_subtract_vector3" &&
           linked_source->second->nodedef != "ND_multiply_vector3" &&
           linked_source->second->nodedef != "ND_divide_vector3" &&
           linked_source->second->nodedef != "ND_crossproduct_vector3" &&
           linked_source->second->nodedef != "ND_absval_vector3" &&
           linked_source->second->nodedef != "ND_floor_vector3" &&
           linked_source->second->nodedef != "ND_ceil_vector3" &&
           linked_source->second->nodedef != "ND_fract_vector3" &&
           linked_source->second->nodedef != "ND_sin_vector3" &&
           linked_source->second->nodedef != "ND_cos_vector3" &&
           linked_source->second->nodedef != "ND_tan_vector3" &&
           linked_source->second->nodedef != "ND_min_vector3" &&
           linked_source->second->nodedef != "ND_max_vector3" &&
           linked_source->second->nodedef != "ND_sign_vector3" &&
           linked_source->second->nodedef != "ND_multiply_vector3FA" &&
           linked_source->second->nodedef != rotate3d_vector3_id &&
           linked_source->second->nodedef != "ND_add_vector3FA" &&
           linked_source->second->nodedef != "ND_subtract_vector3FA" &&
           linked_source->second->nodedef != mix_vector3_id &&
           !is_contrast_vector3(linked_source->second->nodedef) &&
           !is_linear_range_vector3(linked_source->second->nodedef) &&
           !is_vector_conditional(linked_source->second->nodedef) &&
           linked_source->second->nodedef != convert_vector2_vector3_id &&
           linked_source->second->nodedef != combine3_vector3_id) ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.inputs.size() > 1 || node.vector3_inputs.size() > 1 || node.links.size() > 1 ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == "ND_constant_vector3") {
      if (node.vector3_inputs.size() != 1 || node.vector3_inputs.find("value") == node.vector3_inputs.end() ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == convert_color3_vector3_id || node.nodedef == convert_vector3_color3_id ||
        node.nodedef == convert_float_vector3_id || node.nodedef == convert_float_vector2_id ||
        node.nodedef == convert_color3_vector2_id || node.nodedef == convert_vector2_color3_id ||
        node.nodedef == convert_vector2_vector3_id || node.nodedef == convert_boolean_color3_id ||
        node.nodedef == convert_integer_color3_id || node.nodedef == convert_boolean_float_id ||
        node.nodedef == convert_integer_float_id || node.nodedef == convert_boolean_vector2_id ||
        node.nodedef == convert_integer_vector2_id || node.nodedef == convert_boolean_vector3_id ||
        node.nodedef == convert_integer_vector3_id || node.nodedef == convert_boolean_vector4_id ||
        node.nodedef == convert_integer_vector4_id || node.nodedef == convert_vector4_color3_id ||
        node.nodedef == convert_color4_vector2_id || node.nodedef == convert_color4_vector3_id ||
        node.nodedef == convert_color4_vector4_id || node.nodedef == convert_vector4_color4_id)
    {
      const Type input_type = node.nodedef == convert_boolean_color3_id ||
                                      node.nodedef == convert_boolean_float_id ||
                                      node.nodedef == convert_boolean_vector2_id ||
                                      node.nodedef == convert_boolean_vector3_id ||
                                      node.nodedef == convert_boolean_vector4_id ?
                                  Type::Boolean :
                              node.nodedef == convert_integer_color3_id ||
                                      node.nodedef == convert_integer_float_id ||
                                      node.nodedef == convert_integer_vector2_id ||
                                      node.nodedef == convert_integer_vector3_id ||
                                      node.nodedef == convert_integer_vector4_id ?
                                  Type::Integer :
                              node.nodedef == convert_vector4_color3_id || node.nodedef == convert_vector4_color4_id ? Type::Vector4 :
                              node.nodedef == convert_color4_vector2_id || node.nodedef == convert_color4_vector3_id || node.nodedef == convert_color4_vector4_id ? Type::Color4 :
                              node.nodedef == convert_color3_vector3_id || node.nodedef == convert_color3_vector2_id ? Type::Color3 :
                              node.nodedef == convert_vector3_color3_id ? Type::Vector3 :
                              node.nodedef == convert_vector2_color3_id || node.nodedef == convert_vector2_vector3_id ? Type::Vector2 : Type::Float;
      const Type output_type = node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id ||
                               node.nodedef == convert_boolean_color3_id || node.nodedef == convert_integer_color3_id ||
                               node.nodedef == convert_vector4_color3_id ? Type::Color3 :
                               node.nodedef == convert_boolean_float_id || node.nodedef == convert_integer_float_id ? Type::Float :
                               node.nodedef == convert_float_vector2_id || node.nodedef == convert_color3_vector2_id ||
                               node.nodedef == convert_color4_vector2_id || node.nodedef == convert_boolean_vector2_id ||
                               node.nodedef == convert_integer_vector2_id ? Type::Vector2 :
                               node.nodedef == convert_boolean_vector4_id || node.nodedef == convert_integer_vector4_id ||
                               node.nodedef == convert_color4_vector4_id ? Type::Vector4 :
                               node.nodedef == convert_vector4_color4_id ? Type::Color4 : Type::Vector3;
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, input_type, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 1 || node.outputs.at("out") != output_type ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == combine3_color3_id) {
      for (const char *component : {"in1", "in2", "in3"}) {
        const bool value = node.inputs.contains(component), link = node.links.contains(component);
        if (value == link || (link && !validate_link(node.links.at(component), Type::Float, *nodes_by_name))) return false;
      }
      if (node.inputs.size() + node.links.size() != 3 || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Color3 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == separate3_color3_id) {
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 3 || node.outputs.at("outx") != Type::Float ||
          node.outputs.at("outy") != Type::Float || node.outputs.at("outz") != Type::Float ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == separate4_color4_id) {
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, Type::Color4, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 4 ||
          node.outputs.find("outr") == node.outputs.end() ||
          node.outputs.find("outg") == node.outputs.end() ||
          node.outputs.find("outb") == node.outputs.end() ||
          node.outputs.find("outa") == node.outputs.end() ||
          node.outputs.at("outr") != Type::Float || node.outputs.at("outg") != Type::Float ||
          node.outputs.at("outb") != Type::Float || node.outputs.at("outa") != Type::Float ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == combine3_vector3_id) {
      size_t component_count = 0;
      for (const char *component : {"in1", "in2", "in3"}) {
        const bool has_value = node.inputs.find(component) != node.inputs.end();
        const bool has_link = node.links.find(component) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(component), Type::Float, *nodes_by_name))) {
          return false;
        }
        component_count += size_t(has_value || has_link);
      }
      if (component_count != 3 || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Vector3 || node.links.size() + node.inputs.size() != 3 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == extract_vector3_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 2 ||
          input == node.links.end() || !validate_link(input->second, Type::Vector3, *nodes_by_name) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Float || node.links.size() != 1 ||
          node.int_inputs.size() != 1 || !node.inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == separate3_vector3_id) {
      const auto input = node.links.find("in");
      if (input == node.links.end() || !validate_link(input->second, Type::Vector3, *nodes_by_name) ||
          node.links.size() != 1 || node.outputs.size() != 3 ||
          node.outputs.find("outx") == node.outputs.end() ||
          node.outputs.find("outy") == node.outputs.end() ||
          node.outputs.find("outz") == node.outputs.end() ||
          node.outputs.at("outx") != Type::Float || node.outputs.at("outy") != Type::Float ||
          node.outputs.at("outz") != Type::Float || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    if (node.nodedef == rotate3d_vector3_id) {
      const bool input_value = node.vector3_inputs.find("in") != node.vector3_inputs.end();
      const bool input_link = node.links.find("in") != node.links.end();
      const bool amount_value = node.inputs.find("amount") != node.inputs.end();
      const bool amount_link = node.links.find("amount") != node.links.end();
      const auto axis = node.vector3_inputs.find("axis");
      const bool axis_value = axis != node.vector3_inputs.end();
      const bool valid_axis = !axis_value || (std::isfinite(axis->second.x) &&
                                             std::isfinite(axis->second.y) &&
                                             std::isfinite(axis->second.z) &&
                                             len(axis->second) != 0.0f);
      if ((input_value && input_link) || (amount_value && amount_link) || !valid_axis ||
          (input_value && (!std::isfinite(node.vector3_inputs.at("in").x) ||
                           !std::isfinite(node.vector3_inputs.at("in").y) ||
                           !std::isfinite(node.vector3_inputs.at("in").z))) ||
          (amount_value && !std::isfinite(node.inputs.at("amount"))) ||
          (input_link && !validate_link(node.links.at("in"), Type::Vector3, *nodes_by_name)) ||
          (amount_link && !validate_link(node.links.at("amount"), Type::Float, *nodes_by_name)) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
          node.links.size() != size_t(input_link) + size_t(amount_link) ||
          node.vector3_inputs.size() != size_t(input_value) + size_t(axis_value) ||
          node.inputs.size() != size_t(amount_value) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }
    NodeVectorMathType vector_type;
    if (vector_math_type(node.nodedef, &vector_type)) {
      if (node.nodedef == "ND_refract_vector3") {
        const auto valid_vector = [&](const char *name) {
          const bool has_link = node.links.find(name) != node.links.end();
          const bool has_value = node.vector3_inputs.find(name) != node.vector3_inputs.end();
          return has_link != has_value &&
                 (!has_link || validate_link(node.links.at(name), Type::Vector3, *nodes_by_name));
        };
        const bool has_scale_link = node.links.find("scale") != node.links.end();
        const auto scale = node.inputs.find("scale");
        const bool has_scale_value = scale != node.inputs.end();
        if (!valid_vector("in1") || !valid_vector("in2") || has_scale_link == has_scale_value ||
            (has_scale_link && !validate_link(node.links.at("scale"), Type::Float, *nodes_by_name)) ||
            (has_scale_value && !std::isfinite(scale->second)) ||
            node.links.size() != size_t(node.links.contains("in1")) +
                                     size_t(node.links.contains("in2")) + size_t(has_scale_link) ||
            node.vector3_inputs.size() != size_t(node.vector3_inputs.contains("in1")) +
                                              size_t(node.vector3_inputs.contains("in2")) ||
            node.inputs.size() != size_t(has_scale_value) ||
            node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 ||
            !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.string_inputs.empty() ||
            !node.asset_inputs.empty())
        {
          return false;
        }
        continue;
      }
      const bool unary = vector_math_is_unary(node.nodedef);
      const bool scalar_second = vector_math_uses_scalar_second(node.nodedef);
      const char *first = unary ? "in" : "in1";
      const Type output_type = vector_math_returns_float(node.nodedef) ? Type::Float : Type::Vector3;
      const bool has_first_link = node.links.find(first) != node.links.end();
      const bool has_first_value = node.vector3_inputs.find(first) != node.vector3_inputs.end();
      const bool needs_second = !unary;
      const bool has_second_link = node.links.find("in2") != node.links.end();
      const bool has_second_value = scalar_second ? node.inputs.find("in2") != node.inputs.end() :
                                                   node.vector3_inputs.find("in2") != node.vector3_inputs.end();
      if (node.outputs.size() != 1 || node.outputs.at("out") != output_type ||
          has_first_link == has_first_value || (needs_second && has_second_link == has_second_value) ||
          (!needs_second && (has_second_link || has_second_value)) ||
          (has_first_link && !validate_link(node.links.at(first), Type::Vector3, *nodes_by_name)) ||
          (has_second_link && !validate_link(node.links.at("in2"), scalar_second ? Type::Float : Type::Vector3, *nodes_by_name)) ||
          node.links.size() != size_t(has_first_link) + size_t(has_second_link) ||
          node.vector3_inputs.size() != size_t(has_first_value) +
                                            size_t(!scalar_second && has_second_value) ||
          node.inputs.size() != size_t(scalar_second && has_second_value) || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          (node.nodedef == "ND_divide_vector3" &&
           (has_second_link || !has_second_value || node.vector3_inputs.at("in2").x == 0.0f ||
            !std::isfinite(node.vector3_inputs.at("in2").x) ||
            !std::isfinite(node.vector3_inputs.at("in2").y) ||
            !std::isfinite(node.vector3_inputs.at("in2").z) ||
            node.vector3_inputs.at("in2").y == 0.0f || node.vector3_inputs.at("in2").z == 0.0f)) ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }

    if (node.nodedef == open_pbr_surface_id) {
      const auto output = node.outputs.find("out");
      const auto base_color = node.links.find("base_color");
      const auto base_weight = node.links.find("base_weight");
      const auto metalness = node.links.find("base_metalness");
      const auto roughness = node.links.find("specular_roughness");
      const auto ior = node.links.find("specular_ior");
      const auto opacity = node.links.find("geometry_opacity");
      const auto emission_color = node.links.find("emission_color");
      const auto emission_luminance = node.links.find("emission_luminance");
      const auto normal = node.links.find("geometry_normal");
      const auto coat_normal = node.links.find("geometry_coat_normal");
      const auto coat_weight = node.links.find("coat_weight");
      const auto coat_color = node.links.find("coat_color");
      const auto coat_roughness = node.links.find("coat_roughness");
      const auto coat_ior = node.links.find("coat_ior");
      const auto fuzz_weight = node.links.find("fuzz_weight");
      const auto fuzz_color = node.links.find("fuzz_color");
      const auto fuzz_roughness = node.links.find("fuzz_roughness");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader) {
        return false;
      }

      const bool has_base_color_value = node.color3_inputs.find("base_color") !=
                                        node.color3_inputs.end();
      const bool has_base_weight_value = node.inputs.find("base_weight") != node.inputs.end();
      const bool has_metalness_value = node.inputs.find("base_metalness") != node.inputs.end();
      const bool has_roughness_value = node.inputs.find("specular_roughness") != node.inputs.end();
      const bool has_ior_value = node.inputs.find("specular_ior") != node.inputs.end();
      const bool has_opacity_value = node.inputs.find("geometry_opacity") != node.inputs.end();
      const bool has_emission_color_value = node.color3_inputs.find("emission_color") !=
                                            node.color3_inputs.end();
      const bool has_emission_luminance_value = node.inputs.find("emission_luminance") !=
                                                node.inputs.end();
      const bool has_coat_weight_value = node.inputs.find("coat_weight") != node.inputs.end();
      const bool has_coat_color_value = node.color3_inputs.find("coat_color") !=
                                        node.color3_inputs.end();
      const bool has_coat_roughness_value = node.inputs.find("coat_roughness") != node.inputs.end();
      const bool has_coat_ior_value = node.inputs.find("coat_ior") != node.inputs.end();
      const bool has_fuzz_weight_value = node.inputs.find("fuzz_weight") != node.inputs.end();
      const bool has_fuzz_color_value = node.color3_inputs.find("fuzz_color") !=
                                        node.color3_inputs.end();
      const bool has_fuzz_roughness_value = node.inputs.find("fuzz_roughness") != node.inputs.end();
      if (base_color == node.links.end() && base_weight == node.links.end() &&
          metalness == node.links.end() && roughness == node.links.end() &&
          ior == node.links.end() && opacity == node.links.end() &&
          emission_color == node.links.end() && emission_luminance == node.links.end() &&
          normal == node.links.end() && coat_normal == node.links.end() && !has_base_color_value &&
          !has_base_weight_value && !has_metalness_value && !has_roughness_value &&
          !has_ior_value && !has_opacity_value && !has_emission_color_value &&
          !has_emission_luminance_value && !has_coat_weight_value && !has_coat_color_value &&
          !has_coat_roughness_value && !has_coat_ior_value && !has_fuzz_weight_value &&
          !has_fuzz_color_value && !has_fuzz_roughness_value)
      {
        return false;
      }

      for (const auto link :
           {base_color == node.links.end() ? nullptr : &base_color->second,
            base_weight == node.links.end() ? nullptr : &base_weight->second,
            metalness == node.links.end() ? nullptr : &metalness->second,
            roughness == node.links.end() ? nullptr : &roughness->second,
            ior == node.links.end() ? nullptr : &ior->second,
            opacity == node.links.end() ? nullptr : &opacity->second,
            emission_color == node.links.end() ? nullptr : &emission_color->second,
            emission_luminance == node.links.end() ? nullptr : &emission_luminance->second,
            normal == node.links.end() ? nullptr : &normal->second,
            coat_normal == node.links.end() ? nullptr : &coat_normal->second,
            coat_weight == node.links.end() ? nullptr : &coat_weight->second,
            coat_color == node.links.end() ? nullptr : &coat_color->second,
            coat_roughness == node.links.end() ? nullptr : &coat_roughness->second,
            coat_ior == node.links.end() ? nullptr : &coat_ior->second,
            fuzz_weight == node.links.end() ? nullptr : &fuzz_weight->second,
            fuzz_color == node.links.end() ? nullptr : &fuzz_color->second,
            fuzz_roughness == node.links.end() ? nullptr : &fuzz_roughness->second})
      {
        if (link == nullptr) {
          continue;
        }
        if (!validate_link(*link, link->type, *nodes_by_name)) {
          return false;
        }
      }
      if ((base_color != node.links.end() && base_color->second.type != Type::Color3) ||
          (base_weight != node.links.end() && base_weight->second.type != Type::Float) ||
          (metalness != node.links.end() && metalness->second.type != Type::Float) ||
          (roughness != node.links.end() && roughness->second.type != Type::Float) ||
          (ior != node.links.end() && ior->second.type != Type::Float) ||
          (opacity != node.links.end() && opacity->second.type != Type::Float) ||
          (emission_color != node.links.end() && emission_color->second.type != Type::Color3) ||
          (emission_luminance != node.links.end() &&
           emission_luminance->second.type != Type::Float) ||
          (normal != node.links.end() && normal->second.type != Type::Vector3) ||
          (coat_normal != node.links.end() && coat_normal->second.type != Type::Vector3) ||
          (coat_weight != node.links.end() && coat_weight->second.type != Type::Float) ||
          (coat_color != node.links.end() && coat_color->second.type != Type::Color3) ||
          (coat_roughness != node.links.end() && coat_roughness->second.type != Type::Float) ||
          (coat_ior != node.links.end() && coat_ior->second.type != Type::Float) ||
          (fuzz_weight != node.links.end() && fuzz_weight->second.type != Type::Float) ||
          (fuzz_color != node.links.end() && fuzz_color->second.type != Type::Color3) ||
          (fuzz_roughness != node.links.end() && fuzz_roughness->second.type != Type::Float) ||
          (base_color != node.links.end() && has_base_color_value) ||
          (base_weight != node.links.end() && has_base_weight_value) ||
          (metalness != node.links.end() && has_metalness_value) ||
          (roughness != node.links.end() && has_roughness_value) ||
          (ior != node.links.end() && has_ior_value) ||
          (opacity != node.links.end() && has_opacity_value) ||
          (emission_color != node.links.end() && has_emission_color_value) ||
          (emission_luminance != node.links.end() && has_emission_luminance_value) ||
          (coat_weight != node.links.end() && has_coat_weight_value) ||
          (coat_color != node.links.end() && has_coat_color_value) ||
          (coat_roughness != node.links.end() && has_coat_roughness_value) ||
          (coat_ior != node.links.end() && has_coat_ior_value) ||
          (fuzz_weight != node.links.end() && has_fuzz_weight_value) ||
          (fuzz_color != node.links.end() && has_fuzz_color_value) ||
          (fuzz_roughness != node.links.end() && has_fuzz_roughness_value) ||
          node.links.size() != size_t(base_color != node.links.end()) +
                                   size_t(base_weight != node.links.end()) +
                                   size_t(metalness != node.links.end()) +
                                   size_t(roughness != node.links.end()) + size_t(ior != node.links.end()) +
                                   size_t(opacity != node.links.end()) +
                                   size_t(emission_color != node.links.end()) +
                                   size_t(emission_luminance != node.links.end()) +
                                   size_t(normal != node.links.end()) +
                                   size_t(coat_normal != node.links.end()) +
                                   size_t(coat_weight != node.links.end()) +
                                   size_t(coat_color != node.links.end()) +
                                   size_t(coat_roughness != node.links.end()) +
                                   size_t(coat_ior != node.links.end()) +
                                   size_t(fuzz_weight != node.links.end()) +
                                   size_t(fuzz_color != node.links.end()) +
                                   size_t(fuzz_roughness != node.links.end()) ||
          node.inputs.size() != size_t(has_base_weight_value) + size_t(has_metalness_value) +
                                    size_t(has_roughness_value) + size_t(has_ior_value) +
                                    size_t(has_opacity_value) + size_t(has_emission_luminance_value) +
                                    size_t(has_coat_weight_value) + size_t(has_coat_roughness_value) +
                                    size_t(has_coat_ior_value) + size_t(has_fuzz_weight_value) +
                                    size_t(has_fuzz_roughness_value) ||
          node.color3_inputs.size() != size_t(has_base_color_value) +
                                         size_t(has_emission_color_value) + size_t(has_coat_color_value) +
                                         size_t(has_fuzz_color_value) ||
          !node.int_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* Real ND_UsdPreviewSurface_surfaceshader validation. usdshade_reader.cpp
     * only ever stores the seven fields listed here (diffuseColor, metallic,
     * roughness, clearcoat, clearcoatRoughness, ior, emissiveColor) into a
     * node of this nodedef -- every other real input on the nodedef
     * (useSpecularWorkflow, specularColor, opacity, opacityMode,
     * opacityThreshold, normal, displacement, occlusion) is checked at
     * admission time to be absent or at its inert default value and is
     * never stored, so this validator does not need to special-case them. */
    if (node.nodedef == usd_preview_surface_id) {
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.float4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty())
      {
        return false;
      }
      static const char *const color_fields[] = {"diffuseColor", "emissiveColor"};
      static const char *const float_fields[] = {
          "metallic", "roughness", "clearcoat", "clearcoatRoughness", "ior"};
      size_t link_count = 0, color_val_count = 0, float_val_count = 0;
      for (const char *field : color_fields) {
        const auto link = node.links.find(field);
        const auto val = node.color3_inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.color3_inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Color3, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          color_val_count++;
        }
      }
      for (const char *field : float_fields) {
        const auto link = node.links.find(field);
        const auto val = node.inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Float, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          float_val_count++;
        }
      }
      if (node.links.size() != link_count || node.color3_inputs.size() != color_val_count ||
          node.inputs.size() != float_val_count)
      {
        return false;
      }
      continue;
    }

    /* Real ND_gltf_pbr_surfaceshader validation. Same delivery-phase
     * rationale as ND_UsdPreviewSurface_surfaceshader above:
     * usdshade_reader.cpp only ever stores the eight fields listed here
     * into a node of this nodedef; every other real input on the 24-input
     * gltf_pbr nodedef is checked at admission time to be absent or inert
     * and is never stored. */
    if (node.nodedef == gltf_pbr_id) {
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.float4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty())
      {
        return false;
      }
      static const char *const color_fields[] = {"base_color", "emissive"};
      static const char *const float_fields[] = {
          "metallic", "roughness", "clearcoat", "clearcoat_roughness", "ior", "emissive_strength"};
      size_t link_count = 0, color_val_count = 0, float_val_count = 0;
      for (const char *field : color_fields) {
        const auto link = node.links.find(field);
        const auto val = node.color3_inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.color3_inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Color3, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          color_val_count++;
        }
      }
      for (const char *field : float_fields) {
        const auto link = node.links.find(field);
        const auto val = node.inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Float, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          float_val_count++;
        }
      }
      if (node.links.size() != link_count || node.color3_inputs.size() != color_val_count ||
          node.inputs.size() != float_val_count)
      {
        return false;
      }
      continue;
    }

    /* Real ND_disney_principled validation. Same delivery-phase rationale
     * as ND_UsdPreviewSurface_surfaceshader/ND_gltf_pbr_surfaceshader above:
     * usdshade_reader.cpp only ever stores the fourteen real
     * ND_disney_principled inputs (see disney_principled_id's comment
     * below for the field-by-field Cycles mapping) into a node of this
     * nodedef -- there is no admitted-but-inert field for this nodedef, so
     * every stored input must be one of the two color3 or twelve float
     * fields. */
    if (node.nodedef == disney_principled_id) {
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.float4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty())
      {
        return false;
      }
      static const char *const color_fields[] = {"baseColor", "subsurfaceDistance"};
      static const char *const float_fields[] = {"metallic",
                                                  "roughness",
                                                  "anisotropic",
                                                  "specular",
                                                  "specularTint",
                                                  "sheen",
                                                  "sheenTint",
                                                  "clearcoat",
                                                  "clearcoatGloss",
                                                  "specTrans",
                                                  "ior",
                                                  "subsurface"};
      size_t link_count = 0, color_val_count = 0, float_val_count = 0;
      for (const char *field : color_fields) {
        const auto link = node.links.find(field);
        const auto val = node.color3_inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.color3_inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Color3, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          color_val_count++;
        }
      }
      for (const char *field : float_fields) {
        const auto link = node.links.find(field);
        const auto val = node.inputs.find(field);
        const bool has_link = link != node.links.end();
        const bool has_val = val != node.inputs.end();
        if (has_link && has_val) return false;
        if (has_link) {
          if (!validate_link(link->second, Type::Float, *nodes_by_name)) return false;
          link_count++;
        }
        else if (has_val) {
          float_val_count++;
        }
      }
      if (node.links.size() != link_count || node.color3_inputs.size() != color_val_count ||
          node.inputs.size() != float_val_count)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == standard_surface_id) {
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          node.outputs.size() != 1 || !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      bool has_supported_input = false;
      for (const auto &[name, value] : node.inputs) {
        if (!is_supported_standard_surface_float_input(name) || !std::isfinite(value)) {
          return false;
        }
        has_supported_input = true;
      }
      for (const auto &[name, value] : node.color3_inputs) {
        if (!is_supported_standard_surface_color_input(name) || !std::isfinite(value.x) ||
            !std::isfinite(value.y) || !std::isfinite(value.z))
        {
          return false;
        }
        has_supported_input = true;
      }
      for (const auto &[name, value] : node.vector3_inputs) {
        if (!is_supported_standard_surface_vector_input(name) || !std::isfinite(value.x) ||
            !std::isfinite(value.y) || !std::isfinite(value.z))
        {
          return false;
        }
        has_supported_input = true;
      }
      for (const auto &[name, value] : node.int_inputs) {
        if (!is_supported_standard_surface_bool_input(name) || (value != 0 && value != 1)) {
          return false;
        }
        has_supported_input = true;
      }
      for (const auto &[name, link] : node.links) {
        if (is_supported_standard_surface_float_input(name)) {
          if (link.type != Type::Float || node.inputs.contains(name)) return false;
        }
        else if (is_supported_standard_surface_color_input(name)) {
          if (link.type != Type::Color3 || node.color3_inputs.contains(name)) return false;
        }
        else if (is_supported_standard_surface_vector_input(name)) {
          if (link.type != Type::Vector3 || node.vector3_inputs.contains(name)) return false;
        }
        else {
          return false;
        }
        if (!validate_link(link, link.type, *nodes_by_name)) {
          return false;
        }
        has_supported_input = true;
      }
      if (!has_supported_input) {
        return false;
      }
      continue;
    }

    /* Real ND_surface_unlit semantic lowerer validation.
     * Field names/defaults are the five real ND_surface_unlit inputs from
     * the bundled libraries/stdlib/stdlib_defs.mtlx nodedef -- distinct
     * from, and not reused from, the open_pbr_surface_id block above. */
    if (node.nodedef == surface_unlit_id) {
      const auto output = node.outputs.find("out");
      const auto emission = node.links.find("emission");
      const auto emission_color = node.links.find("emission_color");
      const auto transmission = node.links.find("transmission");
      const auto transmission_color = node.links.find("transmission_color");
      const auto opacity = node.links.find("opacity");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader) {
        return false;
      }

      const bool has_emission_value = node.inputs.find("emission") != node.inputs.end();
      const bool has_emission_color_value = node.color3_inputs.find("emission_color") !=
                                            node.color3_inputs.end();
      const bool has_transmission_value = node.inputs.find("transmission") != node.inputs.end();
      const bool has_transmission_color_value = node.color3_inputs.find(
                                                    "transmission_color") !=
                                                node.color3_inputs.end();
      const bool has_opacity_value = node.inputs.find("opacity") != node.inputs.end();
      if (emission == node.links.end() && emission_color == node.links.end() &&
          transmission == node.links.end() && transmission_color == node.links.end() &&
          opacity == node.links.end() && !has_emission_value && !has_emission_color_value &&
          !has_transmission_value && !has_transmission_color_value && !has_opacity_value)
      {
        return false;
      }

      for (const auto link :
           {emission == node.links.end() ? nullptr : &emission->second,
            emission_color == node.links.end() ? nullptr : &emission_color->second,
            transmission == node.links.end() ? nullptr : &transmission->second,
            transmission_color == node.links.end() ? nullptr : &transmission_color->second,
            opacity == node.links.end() ? nullptr : &opacity->second})
      {
        if (link == nullptr) {
          continue;
        }
        if (!validate_link(*link, link->type, *nodes_by_name)) {
          return false;
        }
      }
      if ((emission != node.links.end() && emission->second.type != Type::Float) ||
          (emission_color != node.links.end() && emission_color->second.type != Type::Color3) ||
          (transmission != node.links.end() && transmission->second.type != Type::Float) ||
          (transmission_color != node.links.end() &&
           transmission_color->second.type != Type::Color3) ||
          (opacity != node.links.end() && opacity->second.type != Type::Float) ||
          (emission != node.links.end() && has_emission_value) ||
          (emission_color != node.links.end() && has_emission_color_value) ||
          (transmission != node.links.end() && has_transmission_value) ||
          (transmission_color != node.links.end() && has_transmission_color_value) ||
          (opacity != node.links.end() && has_opacity_value) ||
          node.links.size() != size_t(emission != node.links.end()) +
                                   size_t(emission_color != node.links.end()) +
                                   size_t(transmission != node.links.end()) +
                                   size_t(transmission_color != node.links.end()) +
                                   size_t(opacity != node.links.end()) ||
          node.inputs.size() != size_t(has_emission_value) + size_t(has_transmission_value) +
                                    size_t(has_opacity_value) ||
          node.color3_inputs.size() != size_t(has_emission_color_value) +
                                         size_t(has_transmission_color_value) ||
          !node.int_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == chiang_hair_bsdf_id && node.outputs.count("out") &&
        node.outputs.at("out") == Type::SurfaceShader)
    {
      const auto output = node.outputs.find("out");
      const bool has_ior = node.inputs.contains("ior");
      const bool has_cuticle = node.inputs.contains("cuticle_angle");
      const bool has_absorption = node.vector3_inputs.contains("absorption_coefficient");
      const bool has_normal = node.vector3_inputs.contains("normal") || node.links.contains("normal");
      const float3 tint_r = node.color3_inputs.contains("tint_R") ?
                                node.color3_inputs.at("tint_R") :
                                make_float3(1.0f, 1.0f, 1.0f);
      const float3 tint_tt = node.color3_inputs.contains("tint_TT") ?
                                 node.color3_inputs.at("tint_TT") :
                                 make_float3(1.0f, 1.0f, 1.0f);
      const float3 tint_trt = node.color3_inputs.contains("tint_TRT") ?
                                  node.color3_inputs.at("tint_TRT") :
                                  make_float3(1.0f, 1.0f, 1.0f);
      const auto normal = node.links.find("normal");
      const auto absorption = node.links.find("absorption_coefficient");
      const bool ok = output != node.outputs.end() && output->second == Type::SurfaceShader &&
                      node.outputs.size() == 1 &&
                      (!has_ior || std::isfinite(node.inputs.at("ior"))) &&
                      (!has_cuticle || std::isfinite(node.inputs.at("cuticle_angle"))) &&
                      (!has_absorption || finite_float3(node.vector3_inputs.at("absorption_coefficient"))) &&
                      (normal == node.links.end() ||
                       (normal->second.type == Type::Vector3 &&
                        validate_link(normal->second, Type::Vector3, *nodes_by_name))) &&
                      (absorption == node.links.end() ||
                       (absorption->second.type == Type::Vector3 &&
                        validate_link(absorption->second, Type::Vector3, *nodes_by_name))) &&
                      !node.links.contains("curve_direction") &&
                      !node.links.contains("tint_R") && !node.links.contains("tint_TT") &&
                      !node.links.contains("tint_TRT") && !node.links.contains("roughness_R") &&
                      !node.links.contains("roughness_TT") && !node.links.contains("roughness_TRT") &&
                      !node.links.contains("cuticle_angle") && chiang_roughness_subset_ok(node) &&
                      is_default_white(tint_r) && is_default_white(tint_tt) &&
                      is_default_white(tint_trt) &&
                      node.links.size() == size_t(normal != node.links.end()) +
                                               size_t(absorption != node.links.end()) &&
                      node.inputs.size() == size_t(has_ior) + size_t(has_cuticle) &&
                      node.color3_inputs.size() == size_t(node.color3_inputs.contains("tint_R")) +
                                                     size_t(node.color3_inputs.contains("tint_TT")) +
                                                     size_t(node.color3_inputs.contains("tint_TRT")) &&
                      node.vector3_inputs.size() == size_t(has_absorption) + size_t(has_normal && normal == node.links.end()) &&
                      node.vector2_inputs.size() == 3 && node.int_inputs.empty() &&
                      node.float4_inputs.empty() && node.vector4_inputs.empty() &&
                      node.matrix33_inputs.empty() && node.matrix44_inputs.empty() &&
                      node.string_inputs.empty() && node.asset_inputs.empty();
      if (!ok) {
        return false;
      }
      continue;
    }

    /* oren_nayar_diffuse_bsdf_id and the directly lowered LAMA leaf BSDFs
     * are dual-purpose: plain Type::BSDF closure-producer leaves when used
     * standalone, versus this Type::SurfaceShader-typed flavor when
     * read_connected_surface_closure() constructs them as upstream closures
     * for a generic <surface>'s bsdf socket. */
    if (is_direct_bsdf_producer(node.nodedef) && node.outputs.count("out") &&
        node.outputs.at("out") == Type::SurfaceShader)
    {
      const auto output = node.outputs.find("out");
      const auto color = node.links.find("color");
      const auto roughness = node.links.find("roughness");
      const auto radius = node.links.find("radius");
      const auto ior = node.links.find("ior");
      const auto extinction = node.links.find("extinction");
      const auto tint = node.links.find("tint");
      const auto thinfilm_thickness = node.links.find("thinfilm_thickness");
      const auto thinfilm_ior = node.links.find("thinfilm_ior");
      const auto sss_radius = node.links.find("sssRadius");
      const auto sss_scale = node.links.find("sssScale");
      const auto sss_unit_length = node.links.find("sssUnitLength");
      const auto sss_anisotropy = node.links.find("sssAnisotropy");
      const auto weight = node.links.find("weight");
      const auto normal = node.links.find("normal");
      const auto tangent = node.links.find("tangent");
      const bool has_color_value = node.color3_inputs.find("color") != node.color3_inputs.end();
      const bool has_roughness_value = node.inputs.find("roughness") != node.inputs.end();
      const bool has_weight_value = node.inputs.find("weight") != node.inputs.end();
      const bool has_energy_compensation_value = node.int_inputs.find("energy_compensation") !=
                                                node.int_inputs.end();
      const bool has_lama_energy_value = node.inputs.find("energyCompensation") != node.inputs.end();
      const bool has_radius_value = node.color3_inputs.find("radius") != node.color3_inputs.end();
      const bool has_ior_color_value = node.color3_inputs.find("ior") != node.color3_inputs.end();
      const bool has_extinction_value = node.color3_inputs.find("extinction") != node.color3_inputs.end();
      const bool has_tint_value = node.color3_inputs.find("tint") != node.color3_inputs.end();
      const bool has_sss_radius_value = node.color3_inputs.find("sssRadius") != node.color3_inputs.end();
      const bool has_sss_scale_value = node.inputs.find("sssScale") != node.inputs.end();
      const bool has_sss_unit_length_value = node.inputs.find("sssUnitLength") != node.inputs.end();
      const bool has_sss_anisotropy_value = node.inputs.find("sssAnisotropy") != node.inputs.end();
      const bool has_anisotropy_value = node.inputs.find("anisotropy") != node.inputs.end();
      const bool has_ior_float_value = node.inputs.find("ior") != node.inputs.end();
      const bool has_thinfilm_thickness_value = node.inputs.find("thinfilm_thickness") !=
                                                node.inputs.end();
      const bool has_thinfilm_ior_value = node.inputs.find("thinfilm_ior") != node.inputs.end();
      const bool has_mode_value = node.string_inputs.find("mode") != node.string_inputs.end();
      const bool has_distribution_value = node.string_inputs.find("distribution") !=
                                          node.string_inputs.end();
      const bool has_scatter_mode_value = node.string_inputs.find("scatter_mode") !=
                                          node.string_inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          (color != node.links.end() &&
           (has_color_value || color->second.type != Type::Color3 ||
            !validate_link(color->second, Type::Color3, *nodes_by_name))) ||
          (roughness != node.links.end() &&
           (has_roughness_value || roughness->second.type != Type::Float ||
            !validate_link(roughness->second, Type::Float, *nodes_by_name))) ||
          (sss_radius != node.links.end() &&
           (has_sss_radius_value || sss_radius->second.type != Type::Color3 ||
            !validate_link(sss_radius->second, Type::Color3, *nodes_by_name))) ||
          (radius != node.links.end() &&
           (has_radius_value || radius->second.type != Type::Color3 ||
            !validate_link(radius->second, Type::Color3, *nodes_by_name))) ||
          (ior != node.links.end() &&
           (has_ior_float_value || has_ior_color_value ||
            (node.nodedef == conductor_bsdf_id ? ior->second.type != Type::Color3 :
                                                ior->second.type != Type::Float) ||
            !validate_link(ior->second,
                           node.nodedef == conductor_bsdf_id ? Type::Color3 : Type::Float,
                           *nodes_by_name))) ||
          (extinction != node.links.end() &&
           (has_extinction_value || extinction->second.type != Type::Color3 ||
            !validate_link(extinction->second, Type::Color3, *nodes_by_name))) ||
          (tint != node.links.end() &&
           (has_tint_value || tint->second.type != Type::Color3 ||
            !validate_link(tint->second, Type::Color3, *nodes_by_name))) ||
          (thinfilm_thickness != node.links.end() &&
           (has_thinfilm_thickness_value || thinfilm_thickness->second.type != Type::Float ||
            !validate_link(thinfilm_thickness->second, Type::Float, *nodes_by_name))) ||
          (thinfilm_ior != node.links.end() &&
           (has_thinfilm_ior_value || thinfilm_ior->second.type != Type::Float ||
            !validate_link(thinfilm_ior->second, Type::Float, *nodes_by_name))) ||
          (sss_scale != node.links.end() &&
           (has_sss_scale_value || sss_scale->second.type != Type::Float ||
            !validate_link(sss_scale->second, Type::Float, *nodes_by_name))) ||
          (sss_unit_length != node.links.end() &&
           (has_sss_unit_length_value || sss_unit_length->second.type != Type::Float ||
            !validate_link(sss_unit_length->second, Type::Float, *nodes_by_name))) ||
          (sss_anisotropy != node.links.end() &&
           (has_sss_anisotropy_value || sss_anisotropy->second.type != Type::Float ||
            !validate_link(sss_anisotropy->second, Type::Float, *nodes_by_name))) ||
          (weight != node.links.end() &&
           (has_weight_value || weight->second.type != Type::Float ||
            !validate_link(weight->second, Type::Float, *nodes_by_name))) ||
          (normal != node.links.end() &&
           (normal->second.type != Type::Vector3 ||
            !validate_link(normal->second, Type::Vector3, *nodes_by_name))) ||
          (tangent != node.links.end() &&
           (tangent->second.type != Type::Vector3 ||
            !validate_link(tangent->second, Type::Vector3, *nodes_by_name))) ||
          (has_color_value && !finite_float3(node.color3_inputs.at("color"))) ||
          (has_radius_value && !finite_float3(node.color3_inputs.at("radius"))) ||
          (has_ior_color_value && !finite_float3(node.color3_inputs.at("ior"))) ||
          (has_extinction_value && !finite_float3(node.color3_inputs.at("extinction"))) ||
          (has_tint_value && !finite_float3(node.color3_inputs.at("tint"))) ||
          (has_roughness_value && !std::isfinite(node.inputs.at("roughness"))) ||
          (has_weight_value && !std::isfinite(node.inputs.at("weight"))) ||
          (has_anisotropy_value && !std::isfinite(node.inputs.at("anisotropy"))) ||
          (has_ior_float_value && !std::isfinite(node.inputs.at("ior"))) ||
          (has_thinfilm_thickness_value &&
           !std::isfinite(node.inputs.at("thinfilm_thickness"))) ||
          (has_thinfilm_ior_value && !std::isfinite(node.inputs.at("thinfilm_ior"))) ||
          (has_mode_value && node.string_inputs.at("mode") != "zeltner") ||
          (has_distribution_value && node.string_inputs.at("distribution") != "ggx") ||
          (node.nodedef == dielectric_bsdf_id &&
           (!has_scatter_mode_value || node.string_inputs.at("scatter_mode") != "RT")) ||
          (node.nodedef == lama_diffuse_id &&
           (!has_lama_energy_value || roughness != node.links.end())) ||
          (has_lama_energy_value && (!std::isfinite(node.inputs.at("energyCompensation")) ||
                                     node.inputs.at("energyCompensation") != 0.0f)) ||
          (node.nodedef == lama_sss_id && (sss_radius != node.links.end() ||
                                           sss_scale != node.links.end() ||
                                           sss_unit_length != node.links.end())) ||
          (has_sss_radius_value && !finite_float3(node.color3_inputs.at("sssRadius"))) ||
          (has_sss_scale_value && !std::isfinite(node.inputs.at("sssScale"))) ||
          (has_sss_unit_length_value && !std::isfinite(node.inputs.at("sssUnitLength"))) ||
          (has_sss_anisotropy_value && !std::isfinite(node.inputs.at("sssAnisotropy"))) ||
          (has_energy_compensation_value && node.int_inputs.at("energy_compensation") != 0) ||
          node.links.size() != size_t(color != node.links.end()) +
                                   size_t(roughness != node.links.end()) +
                                   size_t(radius != node.links.end()) +
                                   size_t(ior != node.links.end()) +
                                   size_t(extinction != node.links.end()) +
                                   size_t(tint != node.links.end()) +
                                   size_t(thinfilm_thickness != node.links.end()) +
                                   size_t(thinfilm_ior != node.links.end()) +
                                   size_t(sss_radius != node.links.end()) +
                                   size_t(sss_scale != node.links.end()) +
                                   size_t(sss_unit_length != node.links.end()) +
                                   size_t(sss_anisotropy != node.links.end()) +
                                   size_t(weight != node.links.end()) +
                                   size_t(normal != node.links.end()) +
                                   size_t(tangent != node.links.end()) ||
          node.color3_inputs.size() != size_t(has_color_value) + size_t(has_radius_value) +
                                           size_t(has_ior_color_value) +
                                           size_t(has_extinction_value) + size_t(has_tint_value) +
                                           size_t(has_sss_radius_value) ||
          node.inputs.size() != size_t(has_roughness_value) + size_t(has_weight_value) +
                                    size_t(has_lama_energy_value) + size_t(has_sss_scale_value) +
                                    size_t(has_sss_unit_length_value) +
                                    size_t(has_sss_anisotropy_value) +
                                    size_t(has_anisotropy_value) + size_t(has_ior_float_value) +
                                    size_t(has_thinfilm_thickness_value) +
                                    size_t(has_thinfilm_ior_value) ||
          node.int_inputs.size() != size_t(has_energy_compensation_value) ||
          !node.float4_inputs.empty() ||
          (!node.vector2_inputs.empty() &&
           !((node.nodedef == conductor_bsdf_id || node.nodedef == dielectric_bsdf_id) &&
             node.vector2_inputs.size() == 1 && node.vector2_inputs.contains("roughness"))) ||
          (node.vector2_inputs.contains("roughness") &&
           (!std::isfinite(node.vector2_inputs.at("roughness").x) ||
            node.vector2_inputs.at("roughness").x != node.vector2_inputs.at("roughness").y)) ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          node.string_inputs.size() != size_t(has_mode_value) + size_t(has_distribution_value) +
                                           size_t(has_scatter_mode_value) ||
          !node.asset_inputs.empty() || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_lama_microfacet_surface_bsdf(node.nodedef) &&
        node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader)
    {
      const auto output = node.outputs.find("out");
      const auto roughness = node.vector2_inputs.find("roughness");
      const auto normal = node.links.find("normal");
      const auto ior = node.color3_inputs.find("ior");
      const auto extinction = node.color3_inputs.find("extinction");
      const auto thinfilm_thickness = node.inputs.find("thinfilm_thickness");
      const auto thinfilm_ior = node.inputs.find("thinfilm_ior");
      const bool conductor = node.nodedef == lama_conductor_id;
      const bool glass_like = node.nodedef == lama_iridescence_id;
      const bool roughness_ok = roughness == node.vector2_inputs.end() ||
                                (std::isfinite(roughness->second.x) &&
                                 std::isfinite(roughness->second.y) &&
                                 roughness->second.x == roughness->second.y);
      const bool normal_ok = normal == node.links.end() ||
                             (normal->second.type == Type::Vector3 &&
                              validate_link(normal->second, Type::Vector3, *nodes_by_name));
      bool ok = output != node.outputs.end() && output->second == Type::SurfaceShader &&
                node.outputs.size() == 1 && roughness_ok && normal_ok &&
                node.links.size() == size_t(normal != node.links.end()) &&
                node.vector2_inputs.size() == size_t(roughness != node.vector2_inputs.end()) &&
                node.vector3_inputs.empty() && node.int_inputs.empty() && node.float4_inputs.empty() &&
                node.vector4_inputs.empty() && node.matrix33_inputs.empty() &&
                node.matrix44_inputs.empty() && node.string_inputs.empty() &&
                node.asset_inputs.empty();
      if (conductor) {
        ok = ok && finite_float3(ior == node.color3_inputs.end() ?
                                     make_float3(0.180000007153f, 0.419999986887f, 1.37000000477f) :
                                     ior->second) &&
             finite_float3(extinction == node.color3_inputs.end() ?
                               make_float3(3.42000007629f, 2.34999990463f, 1.76999998093f) :
                               extinction->second) &&
             node.color3_inputs.size() == size_t(ior != node.color3_inputs.end()) +
                                            size_t(extinction != node.color3_inputs.end()) &&
             node.inputs.empty();
      }
      else if (glass_like) {
        ok = ok &&
             (thinfilm_thickness == node.inputs.end() || std::isfinite(thinfilm_thickness->second)) &&
             (thinfilm_ior == node.inputs.end() || std::isfinite(thinfilm_ior->second)) &&
             node.color3_inputs.empty() &&
             node.inputs.size() == size_t(thinfilm_thickness != node.inputs.end()) +
                                      size_t(thinfilm_ior != node.inputs.end());
      }
      else {
        ok = false;
      }
      if (!ok) {
        return false;
      }
      continue;
    }

    if (node.nodedef == uniform_edf_id || node.nodedef == lama_emission_id) {
      const auto output = node.outputs.find("out");
      const auto color = node.links.find("color");
      const bool has_color_value = node.color3_inputs.find("color") !=
                                       node.color3_inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          (color != node.links.end() &&
           (has_color_value || color->second.type != Type::Color3 ||
            !validate_link(color->second, Type::Color3, *nodes_by_name))) ||
          (has_color_value && !finite_float3(node.color3_inputs.at("color"))) ||
          node.links.size() != size_t(color != node.links.end()) ||
          node.color3_inputs.size() != size_t(has_color_value) || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == generic_surface_id) {
      const auto output = node.outputs.find("out");
      const auto bsdf = node.links.find("bsdf");
      const auto edf = node.links.find("edf");
      const auto opacity = node.links.find("opacity");
      const bool has_opacity_value = node.inputs.find("opacity") != node.inputs.end();
      const bool has_thin_walled_value = node.int_inputs.find("thin_walled") !=
                                         node.int_inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          (bsdf == node.links.end() && edf == node.links.end()) ||
          (bsdf != node.links.end() &&
           (bsdf->second.type != Type::SurfaceShader ||
            !validate_link(bsdf->second, Type::SurfaceShader, *nodes_by_name) ||
            !supported_generic_surface_closure(nodes_by_name->at(bsdf->second.source_node)->nodedef) ||
            generic_surface_closure_type(nodes_by_name->at(bsdf->second.source_node)->nodedef) !=
                Type::SurfaceShader)) ||
          (edf != node.links.end() &&
           (edf->second.type != Type::SurfaceShader ||
            !validate_link(edf->second, Type::SurfaceShader, *nodes_by_name) ||
            !supported_generic_surface_closure(nodes_by_name->at(edf->second.source_node)->nodedef) ||
            generic_surface_closure_type(nodes_by_name->at(edf->second.source_node)->nodedef) !=
                Type::LightShader)) ||
          (opacity != node.links.end() &&
           (has_opacity_value || opacity->second.type != Type::Float ||
            !validate_link(opacity->second, Type::Float, *nodes_by_name))) ||
          (has_opacity_value && !std::isfinite(node.inputs.at("opacity"))) ||
          (has_thin_walled_value && node.int_inputs.at("thin_walled") != 0) ||
          node.links.size() != size_t(bsdf != node.links.end()) + size_t(edf != node.links.end()) +
                                   size_t(opacity != node.links.end()) ||
          node.inputs.size() != size_t(has_opacity_value) ||
          node.int_inputs.size() != size_t(has_thin_walled_value) ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == mix_surfaceshader_id) {
      const auto output = node.outputs.find("out");
      const auto fg = node.links.find("fg");
      const auto bg = node.links.find("bg");
      const auto mix = node.links.find("mix");
      const bool has_mix_value = node.inputs.find("mix") != node.inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          fg == node.links.end() || bg == node.links.end() ||
          !validate_link(fg->second, Type::SurfaceShader, *nodes_by_name) ||
          !validate_link(bg->second, Type::SurfaceShader, *nodes_by_name) ||
          !surface_shader_mix_has_unit_opacity(*nodes_by_name->at(fg->second.source_node),
                                               *nodes_by_name) ||
          !surface_shader_mix_has_unit_opacity(*nodes_by_name->at(bg->second.source_node),
                                               *nodes_by_name) ||
          (mix != node.links.end() &&
           (has_mix_value || mix->second.type != Type::Float ||
            !validate_link(mix->second, Type::Float, *nodes_by_name))) ||
          (has_mix_value && !std::isfinite(node.inputs.at("mix"))) ||
          node.links.size() != size_t(2 + (mix != node.links.end())) ||
          node.inputs.size() != size_t(has_mix_value) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == dot_surfaceshader_id) {
      const auto output = node.outputs.find("out");
      const auto input = node.links.find("in");
      const bool has_note = node.string_inputs.find("note") != node.string_inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          input == node.links.end() ||
          !validate_link(input->second, Type::SurfaceShader, *nodes_by_name) ||
          node.links.size() != 1 || node.string_inputs.size() != size_t(has_note) ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.asset_inputs.empty() || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == lama_surface_id) {
      const auto output = node.outputs.find("out");
      const auto front = node.links.find("materialFront");
      const auto back = node.links.find("materialBack");
      const auto presence = node.links.find("presence");
      const bool has_presence_value = node.inputs.find("presence") != node.inputs.end();
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          front == node.links.end() ||
          !validate_link(front->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(front->second.source_node)->nodedef) !=
              Type::SurfaceShader ||
          !supported_generic_surface_closure(nodes_by_name->at(front->second.source_node)->nodedef) ||
          (back != node.links.end() &&
           (!validate_link(back->second, Type::SurfaceShader, *nodes_by_name) ||
            generic_surface_closure_type(nodes_by_name->at(back->second.source_node)->nodedef) !=
                Type::SurfaceShader ||
            !supported_generic_surface_closure(nodes_by_name->at(back->second.source_node)->nodedef))) ||
          (presence != node.links.end() &&
           (has_presence_value || presence->second.type != Type::Float ||
            !validate_link(presence->second, Type::Float, *nodes_by_name))) ||
          (has_presence_value && !std::isfinite(node.inputs.at("presence"))) ||
          node.links.size() != size_t(1 + (back != node.links.end()) + (presence != node.links.end())) ||
          node.inputs.size() != size_t(has_presence_value) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* mix_bsdf_id/lama_mix_bsdf_id are likewise dual-purpose (see the
     * closure-producer comment above) -- mix_edf_id/lama_mix_edf_id have no
     * other flavor and are always SurfaceShader-typed. */
    if (((node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) &&
         node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
        node.nodedef == mix_edf_id || node.nodedef == lama_mix_edf_id)
    {
      const auto output = node.outputs.find("out");
      const auto fg = node.links.find("fg");
      const auto bg = node.links.find("bg");
      const auto mix = node.links.find("mix");
      const bool has_mix_value = node.inputs.find("mix") != node.inputs.end();
      const Type closure_type = generic_surface_closure_type(node.nodedef);
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          fg == node.links.end() || bg == node.links.end() ||
          !validate_link(fg->second, Type::SurfaceShader, *nodes_by_name) ||
          !validate_link(bg->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(fg->second.source_node)->nodedef) !=
              closure_type ||
          generic_surface_closure_type(nodes_by_name->at(bg->second.source_node)->nodedef) !=
              closure_type ||
          !supported_generic_surface_closure(nodes_by_name->at(fg->second.source_node)->nodedef) ||
          !supported_generic_surface_closure(nodes_by_name->at(bg->second.source_node)->nodedef) ||
          (mix != node.links.end() &&
           (has_mix_value || mix->second.type != Type::Float ||
            !validate_link(mix->second, Type::Float, *nodes_by_name))) ||
          (has_mix_value && !std::isfinite(node.inputs.at("mix"))) ||
          node.links.size() != size_t(2 + (mix != node.links.end())) ||
          node.inputs.size() != size_t(has_mix_value) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == generalized_schlick_edf_id) {
      const auto output = node.outputs.find("out");
      const auto base = node.links.find("base");
      const Type closure_type = generic_surface_closure_type(node.nodedef);
      const auto color0 = node.color3_inputs.find("color0");
      const auto color90 = node.color3_inputs.find("color90");
      const auto exponent = node.inputs.find("exponent");
      const float3 color0_value = color0 != node.color3_inputs.end() ? color0->second :
                                                                          make_float3(1.0f);
      const float3 color90_value = color90 != node.color3_inputs.end() ? color90->second :
                                                                            make_float3(1.0f);
      const bool same_color = color0_value == color90_value;
      const bool uniform_color = same_color && std::isfinite(color0_value.x) &&
                                 std::isfinite(color0_value.y) && std::isfinite(color0_value.z) &&
                                 color0_value.x == color0_value.y && color0_value.y == color0_value.z;
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          base == node.links.end() || !validate_link(base->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(base->second.source_node)->nodedef) !=
              closure_type ||
          !supported_generic_surface_closure(nodes_by_name->at(base->second.source_node)->nodedef) ||
          !uniform_color || (exponent != node.inputs.end() && !std::isfinite(exponent->second)) ||
          node.links.size() != 1 ||
          node.color3_inputs.size() != size_t(color0 != node.color3_inputs.end()) +
                                          size_t(color90 != node.color3_inputs.end()) ||
          node.inputs.size() != size_t(exponent != node.inputs.end()) ||
          !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* add_bsdf_id/lama_add_bsdf_id are likewise dual-purpose (see the
     * closure-producer comment above) -- add_edf_id/lama_add_edf_id have no
     * other flavor and are always SurfaceShader-typed. */
    if (((node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) &&
         node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
        node.nodedef == add_edf_id || node.nodedef == lama_add_edf_id)
    {
      const auto output = node.outputs.find("out");
      const auto in1 = node.links.find("in1");
      const auto in2 = node.links.find("in2");
      const Type closure_type = generic_surface_closure_type(node.nodedef);
      const bool lama_add = node.nodedef == lama_add_bsdf_id || node.nodedef == lama_add_edf_id;
      const auto weight1 = node.inputs.find("weight1");
      const auto weight2 = node.inputs.find("weight2");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader ||
          in1 == node.links.end() || in2 == node.links.end() ||
          !validate_link(in1->second, Type::SurfaceShader, *nodes_by_name) ||
          !validate_link(in2->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(in1->second.source_node)->nodedef) !=
              closure_type ||
          generic_surface_closure_type(nodes_by_name->at(in2->second.source_node)->nodedef) !=
              closure_type ||
          !supported_generic_surface_closure(nodes_by_name->at(in1->second.source_node)->nodedef) ||
          !supported_generic_surface_closure(nodes_by_name->at(in2->second.source_node)->nodedef) ||
          node.links.size() != 2 ||
          (lama_add ?
               (weight1 == node.inputs.end() || weight2 == node.inputs.end() ||
                !std::isfinite(weight1->second) || !std::isfinite(weight2->second) ||
                node.inputs.size() != 2) :
               !node.inputs.empty()) ||
          !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* ND_multiply_edfF/ND_multiply_edfC -- see the constant declarations
     * above. Unlike multiply_bsdff_id/multiply_bsdfc_id just below, these
     * have no BSDF-typed flavor (mirrors mix_edf_id/add_edf_id further
     * above), so there is no Type::SurfaceShader dual-purpose gate: any node
     * with this nodedef is admitted here directly. */
    if (node.nodedef == multiply_edff_id || node.nodedef == multiply_edfc_id) {
      const auto output = node.outputs.find("out");
      const auto in1 = node.links.find("in1");
      const Type closure_type = generic_surface_closure_type(node.nodedef);
      bool ok = false;
      if (node.nodedef == multiply_edff_id) {
        const auto in2_literal = node.inputs.find("in2");
        ok = in2_literal != node.inputs.end() && !node.links.contains("in2") &&
             std::isfinite(in2_literal->second) && node.inputs.size() == 1 &&
             node.color3_inputs.empty();
      }
      else {
        /* Only a literal, uniform-channel (R==G==B) tint is admitted here --
         * same genuine Cycles limitation (MixClosureNode.fac is a single
         * scalar) as multiply_bsdfc_id below. */
        const auto in2_literal = node.color3_inputs.find("in2");
        ok = in2_literal != node.color3_inputs.end() && !node.links.contains("in2") &&
             std::isfinite(in2_literal->second.x) && std::isfinite(in2_literal->second.y) &&
             std::isfinite(in2_literal->second.z) && in2_literal->second.x == in2_literal->second.y &&
             in2_literal->second.y == in2_literal->second.z && node.color3_inputs.size() == 1 &&
             node.inputs.empty();
      }
      if (!ok || output == node.outputs.end() || output->second != Type::SurfaceShader ||
          in1 == node.links.end() || !validate_link(in1->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(in1->second.source_node)->nodedef) !=
              closure_type ||
          !supported_generic_surface_closure(nodes_by_name->at(in1->second.source_node)->nodedef) ||
          node.links.size() != 1 || !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* multiply_bsdff_id/multiply_bsdfc_id are dual-purpose like
     * add_bsdf_id/mix_bsdf_id above -- when SurfaceShader-typed (generic
     * <surface> closure-graph flavor, produced by
     * read_connected_surface_closure()'s multiply_bsdfF/C branch in
     * usdshade_reader.cpp) they need this dedicated admission; when
     * BSDF-typed they fall through to the generic is_bsdf_combinator
     * dispatch further below unchanged. Unlike add_bsdf_id/mix_bsdf_id
     * there is no edf flavor (ND_multiply_edfF/C are distinct nodedefs,
     * validated in the dedicated block just above), so this is gated on
     * multiply_bsdff_id/multiply_bsdfc_id specifically. */
    if ((node.nodedef == multiply_bsdff_id || node.nodedef == multiply_bsdfc_id) &&
        node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader)
    {
      const auto output = node.outputs.find("out");
      const auto in1 = node.links.find("in1");
      const Type closure_type = generic_surface_closure_type(node.nodedef);
      bool ok = false;
      if (node.nodedef == multiply_bsdff_id) {
        const auto in2_literal = node.inputs.find("in2");
        ok = in2_literal != node.inputs.end() && !node.links.contains("in2") &&
             std::isfinite(in2_literal->second) && node.inputs.size() == 1 &&
             node.color3_inputs.empty();
      }
      else {
        /* Only a literal, uniform-channel (R==G==B) tint is admitted here --
         * see the matching BSDF-typed multiply_bsdfc_id comment further
         * below (is_bsdf_combinator's validate case) for why. */
        const auto in2_literal = node.color3_inputs.find("in2");
        ok = in2_literal != node.color3_inputs.end() && !node.links.contains("in2") &&
             std::isfinite(in2_literal->second.x) && std::isfinite(in2_literal->second.y) &&
             std::isfinite(in2_literal->second.z) && in2_literal->second.x == in2_literal->second.y &&
             in2_literal->second.y == in2_literal->second.z && node.color3_inputs.size() == 1 &&
             node.inputs.empty();
      }
      if (!ok || output == node.outputs.end() || output->second != Type::SurfaceShader ||
          in1 == node.links.end() || !validate_link(in1->second, Type::SurfaceShader, *nodes_by_name) ||
          generic_surface_closure_type(nodes_by_name->at(in1->second.source_node)->nodedef) !=
              closure_type ||
          !supported_generic_surface_closure(nodes_by_name->at(in1->second.source_node)->nodedef) ||
          node.links.size() != 1 || !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    /* Task 5: boolean/integer exact-domain observation. Both share the
     * existing `int_inputs` storage (already used elsewhere for internal
     * node parameters like "octaves"/"doclamp"/"index") -- distinguished
     * by the node's own nodedef/output tag, exactly like the Color3/Color4
     * float4_inputs-sharing precedent. */
    if (node.nodedef == geompropvalue_boolean_id ||
        node.nodedef == usd_primvar_reader_boolean_id)
    {
      const char *name = node.nodedef == geompropvalue_boolean_id ? "geomprop" : "varname";
      const auto geomprop = node.string_inputs.find(name);
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Boolean ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_boolean_id) {
      const auto value = node.int_inputs.find("value");
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::Boolean ||
          (value != node.int_inputs.end() && value->second != 0 && value->second != 1) ||
          node.int_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_integer_id ||
        node.nodedef == usd_primvar_reader_integer_id)
    {
      const char *name = node.nodedef == geompropvalue_integer_id ? "geomprop" : "varname";
      const auto geomprop = node.string_inputs.find(name);
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Integer ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_integer_id) {
      /* MaterialX's `integer` domain is a full signed 32-bit int -- no
       * value-range restriction beyond the shape/tag checks below (unlike
       * boolean's exact {0, 1} domain). */
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::Integer ||
          node.int_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.vector4_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    /* Task 6: matrix boundary. Both share the general shape/finiteness
     * pattern established for constant_color4/constant_vector4, plus a
     * matrix-specific structural constraint: matrix44's stored last row
     * must be exactly {0, 0, 0, 1} (a genuinely affine 4x4), the exact
     * subset for which Cycles' native `Transform` device representation
     * (see lower()) is a zero-loss encoding, not a lossy truncation. */
    if (node.nodedef == constant_matrix33_id) {
      const auto value = node.matrix33_inputs.find("value");
      const auto output = node.outputs.find("out");
      const bool finite = value == node.matrix33_inputs.end() ||
                          std::all_of(value->second.begin(),
                                      value->second.end(),
                                      [](const float component) { return std::isfinite(component); });
      if (output == node.outputs.end() || output->second != Type::Matrix33 || !finite ||
          node.matrix33_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_matrix44_id) {
      const auto value = node.matrix44_inputs.find("value");
      const auto output = node.outputs.find("out");
      const bool finite = value == node.matrix44_inputs.end() ||
                          std::all_of(value->second.begin(),
                                      value->second.end(),
                                      [](const float component) { return std::isfinite(component); });
      const bool affine = value == node.matrix44_inputs.end() ||
                          (value->second[12] == 0.0f && value->second[13] == 0.0f &&
                           value->second[14] == 0.0f && value->second[15] == 1.0f);
      if (output == node.outputs.end() || output->second != Type::Matrix44 || !finite || !affine ||
          node.matrix44_inputs.size() > 1 || node.outputs.size() != 1 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.float4_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (Type dot_type; value_dot_type(node.nodedef, &dot_type)) {
      if (!validate_value_dot(node, dot_type, *nodes_by_name)) {
        return false;
      }
      continue;
    }

    if (Type output_type; blur_type(node.nodedef, &output_type)) {
      const auto size = node.inputs.find("size");
      const auto filtertype = node.string_inputs.find("filtertype");
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      const bool valid_filter = filtertype != node.string_inputs.end() &&
                                (filtertype->second == "box" || filtertype->second == "gaussian");
      if (size == node.inputs.end() || size->second != 0.0f || !valid_filter ||
          input == node.links.end() || !validate_link(input->second, output_type, *nodes_by_name) ||
          output == node.outputs.end() || output->second != output_type || node.inputs.size() != 1 ||
          node.string_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.float4_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_bsdf_producer(node.nodedef)) {
      const auto output = node.outputs.find("out");
      const bool allows_roughness_vector2 = node.nodedef == conductor_bsdf_id ||
                                            node.nodedef == dielectric_bsdf_id ||
                                            node.nodedef == chiang_hair_bsdf_id;
      const bool valid_vector2_inputs = node.vector2_inputs.empty() ||
                                        (node.nodedef == chiang_hair_bsdf_id ?
                                             std::all_of(node.vector2_inputs.begin(),
                                                         node.vector2_inputs.end(),
                                                         [](const auto &input) {
                                                           return input.first == "roughness_R" ||
                                                                  input.first == "roughness_TT" ||
                                                                  input.first == "roughness_TRT";
                                                         }) :
                                             (node.vector2_inputs.size() == 1 &&
                                              node.vector2_inputs.contains("roughness")));
      if (output == node.outputs.end() || output->second != Type::BSDF ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          (!node.vector2_inputs.empty() && (!allows_roughness_vector2 || !valid_vector2_inputs)) ||
          !node.vector4_inputs.empty() || !node.matrix33_inputs.empty() ||
          !node.matrix44_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      const auto valid_float = [&](const char *name, const float default_value) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.inputs.end() && link == node.links.end()) {
          return std::isfinite(default_value);
        }
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal != node.inputs.end() ? std::isfinite(literal->second) :
                                               validate_link(link->second, Type::Float, *nodes_by_name));
      };
      const auto valid_color3 = [&](const char *name, const float3 &default_value) {
        const auto literal = node.color3_inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.color3_inputs.end() && link == node.links.end()) {
          return std::isfinite(default_value.x) && std::isfinite(default_value.y) &&
                 std::isfinite(default_value.z);
        }
        return (literal != node.color3_inputs.end()) != (link != node.links.end()) &&
               (literal != node.color3_inputs.end() ?
                    (std::isfinite(literal->second.x) && std::isfinite(literal->second.y) &&
                     std::isfinite(literal->second.z)) :
                    validate_link(link->second, Type::Color3, *nodes_by_name));
      };
      const auto valid_vector3_opt = [&](const char *name) {
        /* "normal"/"tangent": optional, defaultgeomprop-driven in MaterialX
         * (Nworld/Tworld). Absent is always valid (Cycles' auto shading
         * normal/generated-tangent attribute takes over). */
        const auto literal = node.vector3_inputs.find(name);
        const auto link = node.links.find(name);
        if (literal == node.vector3_inputs.end() && link == node.links.end()) {
          return true;
        }
        return (literal != node.vector3_inputs.end()) != (link != node.links.end()) &&
               (literal != node.vector3_inputs.end() ?
                    (std::isfinite(literal->second.x) && std::isfinite(literal->second.y) &&
                     std::isfinite(literal->second.z)) :
                    validate_link(link->second, Type::Vector3, *nodes_by_name));
      };

      /* `weight` is folded directly into the color-role socket (color/tint)
       * at lower() time as a literal multiply -- it must therefore be a
       * literal itself (not a link) everywhere it is used below, exactly
       * like conductor_bsdf's stricter, fully-literal-only requirement. */
      const auto weight_literal_ok = [&](const float default_value) {
        const auto literal = node.inputs.find("weight");
        return !node.links.contains("weight") &&
               (literal == node.inputs.end() ? std::isfinite(default_value) :
                                               std::isfinite(literal->second));
      };

      bool ok = false;
      unordered_set<string> allowed_float;
      unordered_set<string> allowed_color3;
      unordered_set<string> allowed_vector3 = {"normal"};
      unordered_set<string> allowed_string;

      if (node.nodedef == oren_nayar_diffuse_bsdf_id || node.nodedef == lama_diffuse_id) {
        allowed_float = node.nodedef == lama_diffuse_id ?
                            unordered_set<string>{"weight", "roughness", "energyCompensation"} :
                            unordered_set<string>{"weight", "roughness"};
        allowed_color3 = {"color"};
        /* energy_compensation has no Cycles equivalent (DiffuseBsdfNode
         * doesn't model it) -- the common top-of-function guard already
         * rejects any authored int_inputs (booleans are stored there, see
         * ND_constant_boolean above), so only the MaterialX default
         * (false, i.e. entirely absent) is admitted here. */
        ok = weight_literal_ok(1.0f) && valid_float("roughness", 0.0f) &&
             valid_color3("color", make_float3(0.18f, 0.18f, 0.18f)) && valid_vector3_opt("normal") &&
             (node.nodedef != lama_diffuse_id ||
              (node.inputs.contains("energyCompensation") &&
               node.inputs.at("energyCompensation") == 0.0f &&
               !node.links.contains("roughness")));
      }
      else if (node.nodedef == translucent_bsdf_id || node.nodedef == lama_translucent_id) {
        allowed_float = {"weight"};
        allowed_color3 = {"color"};
        ok = weight_literal_ok(1.0f) &&
             valid_color3("color", node.nodedef == lama_translucent_id ?
                                       make_float3(0.18f, 0.18f, 0.18f) :
                                       make_float3(1.0f, 1.0f, 1.0f)) &&
             valid_vector3_opt("normal");
      }
      else if (node.nodedef == sheen_bsdf_id) {
        allowed_float = {"weight", "roughness"};
        allowed_color3 = {"color"};
        allowed_string = {"mode"};
        const auto mode = node.string_inputs.find("mode");
        /* Cycles' only sheen closure (CLOSURE_BSDF_SHEEN_ID, bsdf_sheen.h)
         * is Zeltner et al. 2022's microfiber model -- it is not the
         * Conty-Kulla model MaterialX's default `mode="conty_kulla"`
         * names, so only the explicit `mode="zeltner"` case is admitted. */
        ok = mode != node.string_inputs.end() && mode->second == "zeltner" &&
             weight_literal_ok(1.0f) && valid_float("roughness", 0.3f) &&
             valid_color3("color", make_float3(1.0f, 1.0f, 1.0f)) && valid_vector3_opt("normal");
      }
      else if (node.nodedef == subsurface_bsdf_id || node.nodedef == lama_sss_id) {
        allowed_float = node.nodedef == lama_sss_id ?
                            unordered_set<string>{"weight", "sssScale", "sssUnitLength", "sssAnisotropy"} :
                            unordered_set<string>{"weight", "anisotropy"};
        allowed_color3 = node.nodedef == lama_sss_id ? unordered_set<string>{"color", "sssRadius"} :
                                                       unordered_set<string>{"color", "radius"};
        ok = weight_literal_ok(1.0f) &&
             (node.nodedef == lama_sss_id ?
                  (valid_float("sssScale", 1.0f) && valid_float("sssUnitLength", 0.00328f) &&
                   valid_float("sssAnisotropy", 0.0f) &&
                   valid_color3("sssRadius", make_float3(0.0f, 0.0f, 0.0f)) &&
                   !node.links.contains("sssRadius") && !node.links.contains("sssScale") &&
                   !node.links.contains("sssUnitLength")) :
                  (valid_float("anisotropy", 0.0f) &&
                   valid_color3("radius", make_float3(1.0f, 1.0f, 1.0f)))) &&
             valid_color3("color", make_float3(0.18f, 0.18f, 0.18f)) && valid_vector3_opt("normal");
      }
      else if (node.nodedef == conductor_bsdf_id) {
        allowed_float = {"weight", "thinfilm_thickness", "thinfilm_ior"};
        allowed_color3 = {"ior", "extinction"};
        allowed_vector3 = {"normal", "tangent"};
        allowed_string = {"distribution"};
        const auto distribution = node.string_inputs.find("distribution");
        const auto roughness = node.vector2_inputs.find("roughness");
        /* MetallicBsdfNode has no per-graph blend-weight socket to fold a
         * non-default `weight` into (unlike the color-role BSDFs above,
         * `ior`/`extinction` are physical constants, not a reflectance
         * color -- scaling them by weight would misrepresent the metal) --
         * only weight=1 (the MaterialX default) is admitted. Roughness is
         * restricted to the isotropic case (x == y): Cycles' MetallicBsdfNode
         * anisotropy comes from a separate scalar + rotation, not a second
         * roughness component, and no verified conversion from MaterialX's
         * (roughness_x, roughness_y, tangent) triple to that representation
         * exists in this codebase. */
        ok = !node.inputs.contains("weight") &&
             (distribution == node.string_inputs.end() || distribution->second == "ggx") &&
             valid_float("thinfilm_thickness", 0.0f) && valid_float("thinfilm_ior", 1.5f) &&
             valid_color3("ior", make_float3(0.183f, 0.421f, 1.373f)) &&
             valid_color3("extinction", make_float3(3.424f, 2.346f, 1.770f)) &&
             valid_vector3_opt("normal") && valid_vector3_opt("tangent") &&
             (roughness == node.vector2_inputs.end() ||
              (std::isfinite(roughness->second.x) && roughness->second.x == roughness->second.y)) &&
             !node.links.contains("roughness");
      }
      else if (node.nodedef == dielectric_bsdf_id) {
        allowed_float = {"weight", "ior", "thinfilm_thickness", "thinfilm_ior"};
        allowed_color3 = {"tint"};
        allowed_vector3 = {"normal", "tangent"};
        allowed_string = {"distribution", "scatter_mode"};
        const auto distribution = node.string_inputs.find("distribution");
        const auto scatter_mode = node.string_inputs.find("scatter_mode");
        const auto roughness = node.vector2_inputs.find("roughness");
        /* Only the explicit scatter_mode="RT" (full glass: reflection +
         * transmission via a single Fresnel-weighted closure) maps onto a
         * real Cycles closure (GlassBsdfNode). scatter_mode="R" (the
         * MaterialX default) has no equivalent: GlossyBsdfNode has no IOR/
         * Fresnel input at all (only a flat color tint), so a dielectric
         * reflection-only lobe with a physical IOR can't be represented.
         * scatter_mode="T" (RefractionBsdfNode) is a separate, narrower
         * closure this pass does not attempt. Roughness is isotropic-only
         * for the same reason as conductor_bsdf above. */
        ok = scatter_mode != node.string_inputs.end() && scatter_mode->second == "RT" &&
             (distribution == node.string_inputs.end() || distribution->second == "ggx") &&
             weight_literal_ok(1.0f) && valid_float("ior", 1.5f) &&
             valid_float("thinfilm_thickness", 0.0f) && valid_float("thinfilm_ior", 1.5f) &&
             valid_color3("tint", make_float3(1.0f, 1.0f, 1.0f)) &&
             valid_vector3_opt("normal") && valid_vector3_opt("tangent") &&
             (roughness == node.vector2_inputs.end() ||
              (std::isfinite(roughness->second.x) && roughness->second.x == roughness->second.y)) &&
             !node.links.contains("roughness");
      }
      else if (node.nodedef == chiang_hair_bsdf_id) {
        allowed_float = {"ior", "cuticle_angle"};
        allowed_color3 = {"tint_R", "tint_TT", "tint_TRT"};
        allowed_vector3 = {"normal", "curve_direction", "absorption_coefficient"};
        const float3 tint_r = node.color3_inputs.contains("tint_R") ?
                                  node.color3_inputs.at("tint_R") :
                                  make_float3(1.0f, 1.0f, 1.0f);
        const float3 tint_tt = node.color3_inputs.contains("tint_TT") ?
                                   node.color3_inputs.at("tint_TT") :
                                   make_float3(1.0f, 1.0f, 1.0f);
        const float3 tint_trt = node.color3_inputs.contains("tint_TRT") ?
                                    node.color3_inputs.at("tint_TRT") :
                                    make_float3(1.0f, 1.0f, 1.0f);
        ok = valid_float("ior", 1.55f) && valid_float("cuticle_angle", 0.5f) &&
             valid_color3("tint_R", make_float3(1.0f, 1.0f, 1.0f)) &&
             valid_color3("tint_TT", make_float3(1.0f, 1.0f, 1.0f)) &&
             valid_color3("tint_TRT", make_float3(1.0f, 1.0f, 1.0f)) &&
             valid_vector3_opt("normal") && valid_vector3_opt("curve_direction") &&
             valid_vector3_opt("absorption_coefficient") &&
             !node.vector3_inputs.contains("curve_direction") &&
             !node.links.contains("tint_R") && !node.links.contains("tint_TT") &&
             !node.links.contains("tint_TRT") && !node.links.contains("roughness_R") &&
             !node.links.contains("roughness_TT") && !node.links.contains("roughness_TRT") &&
             !node.links.contains("cuticle_angle") && !node.links.contains("curve_direction") &&
             chiang_roughness_subset_ok(node) && is_default_white(tint_r) &&
             is_default_white(tint_tt) && is_default_white(tint_trt);
      }

      if (!ok ||
          std::any_of(node.inputs.begin(),
                      node.inputs.end(),
                      [&](const auto &input) { return !allowed_float.contains(input.first); }) ||
          std::any_of(node.color3_inputs.begin(),
                      node.color3_inputs.end(),
                      [&](const auto &input) { return !allowed_color3.contains(input.first); }) ||
          std::any_of(node.vector3_inputs.begin(),
                      node.vector3_inputs.end(),
                      [&](const auto &input) { return !allowed_vector3.contains(input.first); }) ||
          std::any_of(node.string_inputs.begin(),
                      node.string_inputs.end(),
                      [&](const auto &input) { return !allowed_string.contains(input.first); }) ||
          std::any_of(node.links.begin(), node.links.end(), [&](const auto &input) {
            return !allowed_float.contains(input.first) && !allowed_color3.contains(input.first) &&
                   !allowed_vector3.contains(input.first);
          }))
      {
        return false;
      }
      continue;
    }

    /* Real BSDF closure combinators: ND_add_bsdf, ND_mix_bsdf,
     * ND_multiply_bsdfF, ND_multiply_bsdfC. `in1`/`in2` (or `fg`/`bg` for
     * mix_bsdf) must themselves resolve to a lowerable Type::BSDF subgraph
     * -- validate_link() checks the link's own type/shape, and the
     * surrounding per-node loop over source.nodes already validates every
     * other node in the graph (including that link's source node) against
     * this same dispatch, so a source node that is neither a bsdf-producer
     * leaf nor another combinator is rejected there, not here. */
    if (is_bsdf_combinator(node.nodedef)) {
      const auto output = node.outputs.find("out");
      if (output == node.outputs.end() || output->second != Type::BSDF ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.vector4_inputs.empty() ||
          !node.float4_inputs.empty() || !node.string_inputs.empty() ||
          !node.matrix33_inputs.empty() || !node.matrix44_inputs.empty() ||
          !node.asset_inputs.empty())
      {
        return false;
      }

      bool ok = false;
      if (node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) {
        const auto in1 = node.links.find("in1");
        const auto in2 = node.links.find("in2");
        const auto weight1 = node.inputs.find("weight1");
        const auto weight2 = node.inputs.find("weight2");
        const bool lama_add = node.nodedef == lama_add_bsdf_id;
        ok = in1 != node.links.end() && in2 != node.links.end() &&
             validate_link(in1->second, Type::BSDF, *nodes_by_name) &&
             validate_link(in2->second, Type::BSDF, *nodes_by_name) && node.links.size() == 2 &&
             (lama_add ?
                  (weight1 != node.inputs.end() && weight2 != node.inputs.end() &&
                   std::isfinite(weight1->second) && std::isfinite(weight2->second) &&
                   node.inputs.size() == 2) :
                  node.inputs.empty()) &&
             node.color3_inputs.empty();
      }
      else if (node.nodedef == multiply_bsdff_id) {
        /* Scale a BSDF by a literal float weight. `in2` must be literal --
         * it is lowered as MixClosureNode.fac, see the mix_bsdf comment
         * below for why a linked float is real capability there; here it is
         * kept literal-only to match multiply_bsdfC's genuine literal-only
         * limitation (see that branch's comment) rather than diverging
         * without reason between the two multiply_bsdf* siblings. */
        const auto in1 = node.links.find("in1");
        const auto in2_literal = node.inputs.find("in2");
        ok = in1 != node.links.end() && validate_link(in1->second, Type::BSDF, *nodes_by_name) &&
             in2_literal != node.inputs.end() && !node.links.contains("in2") &&
             std::isfinite(in2_literal->second) && node.links.size() == 1 &&
             node.inputs.size() == 1 && node.color3_inputs.empty();
      }
      else if (node.nodedef == multiply_bsdfc_id) {
        /* Scale a BSDF by a color3 tint. Cycles has no per-channel closure
         * weighting primitive (MixClosureNode.fac is a single scalar, and
         * there is no other closure-combining node in shader_nodes.h/.cpp)
         * -- so only a literal, uniform-channel (R==G==B) tint is admitted,
         * since that degenerates exactly to a scalar MixClosureNode.fac
         * with no loss. A non-uniform tint, or one fed by a link, is
         * rejected here rather than approximated.
         *
         * Rejected alternative: recursively folding a non-uniform literal
         * tint into the underlying bsdf-producer leaf's own literal
         * color-role socket (mirroring the VDF coefficient folding in
         * usdshade_reader.cpp's read_vdf_coefficients()). That was not
         * implemented: `in1`'s source node can be referenced by more than
         * one downstream link (this file's Node graph shares nodes by
         * name), so mutating its stored literal color in place to bake in
         * the tint would silently corrupt any other consumer of that same
         * bsdf-producer node -- correctly doing so would require detecting
         * single-use and/or cloning the producer node, which is
         * disproportionate complexity for representing content that could
         * already just author the tint directly into the producer's own
         * color-role literal. */
        const auto in1 = node.links.find("in1");
        const auto in2_literal = node.color3_inputs.find("in2");
        const bool uniform = in2_literal != node.color3_inputs.end() &&
                             std::isfinite(in2_literal->second.x) &&
                             std::isfinite(in2_literal->second.y) &&
                             std::isfinite(in2_literal->second.z) &&
                             in2_literal->second.x == in2_literal->second.y &&
                             in2_literal->second.y == in2_literal->second.z;
        ok = in1 != node.links.end() && validate_link(in1->second, Type::BSDF, *nodes_by_name) &&
             uniform && !node.links.contains("in2") && node.links.size() == 1 &&
             node.inputs.empty() && node.color3_inputs.size() == 1;
      }
      else if (node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) {
        /* `mix` may be literal or linked: this file's own ND_mix_float/
         * ND_mix_color3 lowering already connects a linked "mix" factor
         * straight into a Mix*Node's Fac-equivalent socket (see is_mix()
         * handling above and its `mix_link`/`connect_if_linked("mix", ...)`
         * connect-phase counterpart) -- MixClosureNode.fac is the same kind
         * of plain SVM float socket, so there is no genuine capability gap
         * here; restricting it to literal-only would be arbitrary caution,
         * not a real limitation. */
        const auto fg = node.links.find("fg");
        const auto bg = node.links.find("bg");
        const auto mix_literal = node.inputs.find("mix");
        const auto mix_link = node.links.find("mix");
        ok = fg != node.links.end() && bg != node.links.end() &&
             validate_link(fg->second, Type::BSDF, *nodes_by_name) &&
             validate_link(bg->second, Type::BSDF, *nodes_by_name) &&
             ((mix_literal != node.inputs.end()) != (mix_link != node.links.end())) &&
             (mix_literal == node.inputs.end() || std::isfinite(mix_literal->second)) &&
             (mix_link == node.links.end() ||
              validate_link(mix_link->second, Type::Float, *nodes_by_name)) &&
             node.links.size() == size_t(2 + (mix_link != node.links.end())) &&
             node.inputs.size() == size_t(mix_literal != node.inputs.end()) &&
             node.color3_inputs.empty();
      }

      if (!ok) {
        return false;
      }
      continue;
    }

    return false;
  }

  unordered_map<string, VisitState> visit_states;
  for (const Node &node : source.nodes) {
    if (!validate_acyclic(node, *nodes_by_name, &visit_states)) {
      return false;
    }
  }
  if (source.has_displacement &&
      ((!source.displacement_is_vector3 && source.displacement.is_linked &&
        (source.displacement.link.type != Type::Float ||
         !validate_link(source.displacement.link, Type::Float, *nodes_by_name))) ||
       (source.displacement_is_vector3 && source.displacement_vector3.is_linked &&
        (source.displacement_vector3.link.type != Type::Vector3 ||
         !validate_link(source.displacement_vector3.link, Type::Vector3, *nodes_by_name))) ||
       (source.displacement_scale.is_linked &&
        (source.displacement_scale.link.type != Type::Float ||
         !validate_link(source.displacement_scale.link, Type::Float, *nodes_by_name)))))
  {
    return false;
  }

  /* Task 3: volume terminal is preserved atomically with any co-authored
   * surface/displacement terminals -- validate its links exactly like
   * displacement's, so a malformed volume link fails the whole material
   * before any terminal (surface, volume, or displacement) is committed.
   *
   * 'absorption'/'scattering' are MaterialX vector3 (Type::Vector3), not
   * color3 -- see read_volume_color_input()'s comment in
   * usdshade_reader.cpp for why. */
  if (source.has_volume &&
      ((source.volume_absorption.is_linked &&
        (source.volume_absorption.link.type != Type::Vector3 ||
         !validate_link(source.volume_absorption.link, Type::Vector3, *nodes_by_name))) ||
       (source.volume_scattering.is_linked &&
        (source.volume_scattering.link.type != Type::Vector3 ||
         !validate_link(source.volume_scattering.link, Type::Vector3, *nodes_by_name))) ||
       (source.volume_anisotropy.is_linked &&
        (source.volume_anisotropy.link.type != Type::Float ||
         !validate_link(source.volume_anisotropy.link, Type::Float, *nodes_by_name))) ||
       (source.volume_emission.is_linked &&
        (source.volume_emission.link.type != Type::Color3 ||
         !validate_link(source.volume_emission.link, Type::Color3, *nodes_by_name)))))
  {
    return false;
  }

  return true;
}

ShaderOutput *lowered_color4_alpha_output(
    const Link &link,
    const unordered_map<string, const Node *> &nodes_by_name,
    const unordered_map<string, ShaderNode *> &lowered_nodes);

ShaderOutput *lowered_vector4_w_output(
    const Link &link,
    const unordered_map<string, const Node *> &nodes_by_name,
    const unordered_map<string, ShaderNode *> &lowered_nodes);

ShaderOutput *lowered_output(const Link &link,
                             const unordered_map<string, const Node *> &nodes_by_name,
                             const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  ShaderNode *lowered = lowered_nodes.at(link.source_node);
  if (source.nodedef == dot_surfaceshader_id) {
    return lowered_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (value_dot_type(source.nodedef, nullptr) && source.links.contains("in")) {
    return lowered_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (blur_type(source.nodedef, nullptr)) {
    return lowered_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (scalar_blend_type(source.nodedef, nullptr) ||
      (color_blend_type(source.nodedef, nullptr) &&
       !is_exact_color_burn_dodge(source.nodedef)))
  {
    return lowered->output("Result");
  }
  if (source.nodedef == extract_color3_id) {
    static const char *channels[] = {"Red", "Green", "Blue"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (source.nodedef == extract_color4_id) {
    const int index = source.int_inputs.at("index");
    if (index == 3) {
      const Node &color4_source = *nodes_by_name.at(source.links.at("in").source_node);
      return color4_source.nodedef == image_color4_id ? lowered->output("Alpha") :
                                                        lowered->output("Value");
    }
    static const char *channels[] = {"Red", "Green", "Blue"};
    return lowered->output(channels[index]);
  }
  if (source.nodedef == extract_vector3_id ||
      (source.nodedef == extract_vector4_id && source.int_inputs.at("index") < 3)) {
    static const char *channels[] = {"X", "Y", "Z"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (source.nodedef == extract_vector4_id) {
    return lowered->output("Value");
  }
  if (source.nodedef == separate3_vector3_id) {
    if (link.source_output == "outx") return lowered->output("X");
    if (link.source_output == "outy") return lowered->output("Y");
    if (link.source_output == "outz") return lowered->output("Z");
    return nullptr;
  }
  if (source.nodedef == separate4_vector4_id) {
    if (link.source_output == "outx") return lowered->output("X");
    if (link.source_output == "outy") return lowered->output("Y");
    if (link.source_output == "outz") return lowered->output("Z");
    if (link.source_output == "outw") {
      return lowered_nodes.at(source.links.at("in").source_node + ".W")->output("Value");
    }
    return nullptr;
  }
  if (source.nodedef == separate3_color3_id) {
    if (link.source_output == "outx") return lowered->output("Red");
    if (link.source_output == "outy") return lowered->output("Green");
    if (link.source_output == "outz") return lowered->output("Blue");
    return nullptr;
  }
  if (source.nodedef == separate4_color4_id) {
    if (link.source_output == "outr") return lowered->output("Red");
    if (link.source_output == "outg") return lowered->output("Green");
    if (link.source_output == "outb") return lowered->output("Blue");
    if (link.source_output == "outa") {
      return lowered_color4_alpha_output(source.links.at("in"), nodes_by_name, lowered_nodes);
    }
    return nullptr;
  }
  if (source.nodedef == extract_vector2_id) {
    static const char *channels[] = {"X", "Y"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (link.type == Type::Color3) {
    return lowered->output("Color");
  }
  if (link.type == Type::Float) {
    if (triplanarprojection_type(source.nodedef, nullptr)) {
      return lowered->output("Value");
    }
    if (source.nodedef == clamp_float_id || is_smoothstep_float(source.nodedef) ||
        is_linear_range_float(source.nodedef))
    {
      return lowered->output("Result");
    }
    if (source.nodedef == convert_float_color3_id) {
      return lowered->output("Color");
    }
    if (source.nodedef == geompropvalue_float_id) {
      return lowered->output("Fac");
    }
    if (source.nodedef == usdprimvarreader_float_id) {
      return lowered->output("Fac");
    }
    if (source.nodedef == image_float_id) {
      return lowered->output("Red");
    }
    return lowered->output("Value");
  }
  if (link.type == Type::Vector2) {
    if (is_vector2_conditional(source.nodedef)) {
      return lowered->output("Result");
    }
    if (source.nodedef == convert_color4_vector2_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == clamp_vector2fa_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == geompropvalue_vector2_id) {
      return lowered->output("UV");
    }
    if (source.nodedef == usdprimvarreader_vector2_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == place2d_vector2_id) {
      return lowered->output("Result");
    }
    if (source.nodedef == rotate2d_vector2_id) {
      return lowered->output("Vector");
    }
    if (vector2_invert_type(source.nodedef, nullptr)) {
      return lowered->output("Vector");
    }
    if (vector2_smoothstep_type(source.nodedef, nullptr)) {
      return lowered->output("Vector");
    }
    if (is_contrast_vector2(source.nodedef)) {
      return lowered->output("Vector");
    }
    return lowered->output("Vector");
  }
  if (link.type == Type::Vector3) {
    if (is_vector_conditional(source.nodedef)) {
      return lowered->output("Result");
    }
    if (source.nodedef == normalmap_float_id) {
      return lowered->output("Normal");
    }
    if (source.nodedef == "ND_constant_vector3" || source.nodedef == combine3_vector3_id ||
        source.nodedef == convert_color3_vector3_id || source.nodedef == convert_float_vector3_id ||
        source.nodedef == convert_boolean_vector3_id || source.nodedef == convert_integer_vector3_id ||
        source.nodedef == convert_vector2_vector3_id || source.nodedef == convert_vector4_vector3_id ||
        source.nodedef == convert_vector4_vector2_id ||
        is_space_transform(source.nodedef) || is_native_fractal2d_family(source.nodedef) ||
        source.nodedef == mix_vector3_id || vector_math_type(source.nodedef, nullptr) ||
        vector3_binary_component_math_type(source.nodedef, nullptr) || is_safepower_vector3(source.nodedef) ||
        vector3_domain_math_type(source.nodedef, nullptr) ||
        vector3_atan2_type(source.nodedef, nullptr) || vector3_invert_type(source.nodedef, nullptr) ||
        vector3_smoothstep_type(source.nodedef, nullptr) || source.nodedef == clamp_vector3_id ||
        source.nodedef == clamp_vector3fa_id || source.nodedef == convert_color4_vector3_id ||
        is_contrast_vector3(source.nodedef) || is_linear_range_vector3(source.nodedef) ||
        source.nodedef == rotate3d_vector3_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == noise2d_vector3_id || source.nodedef == noise2d_vector3fa_id ||
        source.nodedef == noise3d_vector3_id || source.nodedef == noise3d_vector3fa_id ||
        source.nodedef == fractal2d_vector3_id || source.nodedef == fractal2d_vector3fa_id ||
        source.nodedef == fractal3d_vector3_id || source.nodedef == fractal3d_vector3fa_id)
    {
      return lowered->output("Vector");
    }
    if (source.nodedef == image_vector3_id) {
      return lowered->output("Color");
    }
    if (source.nodedef == triplanarprojection_vector3_id) {
      return lowered->output("Color");
    }
    if (source.nodedef == geompropvalue_vector3_id) {
      return lowered->output("Normal");
    }
    if (source.nodedef == normal_vector3_id) {
      return lowered->output("Normal");
    }
    if (source.nodedef == position_vector3_id) {
      return lowered->output("Position");
    }
    if (source.nodedef == viewdirection_vector3_id) {
      return lowered->output("Incoming");
    }
    if (source.nodedef == usdprimvarreader_vector3_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == texcoord_vector3_id) {
      return lowered->output("UV");
    }
  }
  if (link.type == Type::Color4) {
    if (source.nodedef == triplanarprojection_color4_id) {
      return lowered->output("Color");
    }
    if (source.nodedef == convert_color3_color4_id) {
      return lowered_output(source.links.at("in"), nodes_by_name, lowered_nodes);
    }
    if (native_noise_or_fractal_is_color4(source.nodedef)) {
      return lowered_nodes.at(link.source_node)->output("Vector");
    }
    if (source.nodedef == image_color4_id || source.nodedef == constant_color4_id ||
        source.nodedef == geompropvalue_color4_id ||
        source.nodedef == convert_float_color4_id || source.nodedef == convert_boolean_color4_id ||
        source.nodedef == convert_integer_color4_id || source.nodedef == convert_vector4_color4_id ||
        source.nodedef == combine2_color4cf_id || source.nodedef == combine4_color4_id ||
        is_color4_operation(source.nodedef) || is_color4_conditional(source.nodedef) ||
        source.nodedef == inside_color4_id || source.nodedef == outside_color4_id ||
        is_color4_ramp(source.nodedef) || is_color4_split(source.nodedef)) {
      return lowered->output("Color");
    }
  }
  if (link.type == Type::Vector4) {
    if (source.nodedef == triplanarprojection_vector4_id) {
      return lowered->output("Color");
    }
    if (is_vector4_conditional(source.nodedef)) {
      return lowered->output("Result");
    }
    if (native_noise_or_fractal_output_type(source.nodedef) == Type::Vector4) {
      return lowered->output("Vector");
    }
    if (source.nodedef == constant_vector4_id || source.nodedef == image_vector4_id ||
        source.nodedef == geompropvalue_vector4_id ||
        source.nodedef == usd_primvar_reader_vector4_id ||
        source.nodedef == convert_vector3_vector4_id || source.nodedef == convert_color3_vector4_id ||
        source.nodedef == convert_vector2_vector4_id || source.nodedef == convert_color4_vector4_id ||
        source.nodedef == convert_float_vector4_id || source.nodedef == convert_boolean_vector4_id ||
        source.nodedef == convert_integer_vector4_id || is_vector4_combine(source.nodedef) ||
        is_vector4_math_or_clamp(source.nodedef)) {
      return lowered->output("Vector");
    }
  }
  if (link.type == Type::Boolean) {
    if (source.nodedef == constant_boolean_id || source.nodedef == geompropvalue_boolean_id ||
        source.nodedef == usd_primvar_reader_boolean_id) {
      return source.nodedef == constant_boolean_id ? lowered_nodes.at(link.source_node + ".float")->output("Value") :
                                                     lowered->output("Fac");
    }
  }
  if (link.type == Type::Integer) {
    if (source.nodedef == constant_integer_id || source.nodedef == geompropvalue_integer_id ||
        source.nodedef == usd_primvar_reader_integer_id) {
      return source.nodedef == constant_integer_id ? lowered_nodes.at(link.source_node + ".float")->output("Value") :
                                                     lowered->output("Fac");
    }
  }
  if (link.type == Type::BSDF) {
    /* AddClosureNode/MixClosureNode's output socket is "Closure", not
     * "BSDF"/"BSSRDF" like the bsdf-producer leaves' BsdfNode subclasses. */
    if (is_bsdf_combinator(source.nodedef)) {
      return lowered->output("Closure");
    }
    /* SubsurfaceScatteringNode's closure output socket is "BSSRDF", not
     * "BSDF" like every other BsdfNode subclass. */
    return lowered->output(source.nodedef == subsurface_bsdf_id ? "BSSRDF" : "BSDF");
  }
  if (link.type == Type::SurfaceShader) {
    if (const char *output_name = generic_surface_closure_output_name(source)) {
      return lowered->output(output_name);
    }
  }
  return nullptr;
}

ShaderOutput *lowered_color4_alpha_output(
    const Link &link,
    const unordered_map<string, const Node *> &nodes_by_name,
    const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  if (source.nodedef == dot_color4_id && source.links.contains("in")) {
    return lowered_color4_alpha_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == blur_color4_id) {
    return lowered_color4_alpha_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == convert_color3_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (source.nodedef == convert_vector4_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (source.nodedef == convert_float_color4_id || source.nodedef == convert_boolean_color4_id ||
      source.nodedef == convert_integer_color4_id)
  {
    return lowered_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == combine2_color4cf_id) {
    return lowered_output(source.links.at("in2"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == combine4_color4_id && source.links.contains("in4")) {
    return lowered_output(source.links.at("in4"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == combine4_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (source.nodedef == image_color4_id) {
    return lowered_nodes.at(link.source_node)->output("Alpha");
  }
  if (source.nodedef == triplanarprojection_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (source.nodedef == geompropvalue_color4_id) {
    /* AttributeNode's "Alpha" output is a genuine per-element read of the
     * same named attribute as "Color", not a fabricated constant -- see the
     * geompropvalue_color4_id comment above and its lowering below. */
    return lowered_nodes.at(link.source_node)->output("Alpha");
  }
  if (source.nodedef == constant_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (native_noise_or_fractal_is_color4(source.nodedef)) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (is_color4_operation(source.nodedef)) {
    return lowered_nodes
        .at(link.source_node +
            (source.nodedef == clamp_color4_id || source.nodedef == clamp_color4fa_id ?
                 ".Alpha.maximum" :
                 (is_safepower_color4(source.nodedef) ? ".Alpha.multiply" : ".Alpha")))
        ->output("Value");
  }
  if (is_color4_ramp(source.nodedef) || is_color4_split(source.nodedef) ||
      is_color4_conditional(source.nodedef) || source.nodedef == inside_color4_id ||
      source.nodedef == outside_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  return nullptr;
}

ShaderOutput *lowered_vector4_w_output(
    const Link &link,
    const unordered_map<string, const Node *> &nodes_by_name,
    const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  if (source.nodedef == dot_vector4_id && source.links.contains("in")) {
    return lowered_vector4_w_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == blur_vector4_id) {
    return lowered_vector4_w_output(source.links.at("in"), nodes_by_name, lowered_nodes);
  }
  if (source.nodedef == constant_vector4_id || source.nodedef == image_vector4_id ||
      source.nodedef == convert_vector3_vector4_id || source.nodedef == convert_color3_vector4_id ||
      source.nodedef == convert_vector2_vector4_id || source.nodedef == convert_color4_vector4_id ||
      source.nodedef == convert_float_vector4_id || source.nodedef == convert_boolean_vector4_id ||
      source.nodedef == convert_integer_vector4_id || is_vector4_combine(source.nodedef) ||
      is_vector4_conditional(source.nodedef) ||
      native_noise_or_fractal_output_type(source.nodedef) == Type::Vector4 ||
      source.nodedef == triplanarprojection_vector4_id)
  {
    return lowered_nodes.at(link.source_node + ".W")->output("Value");
  }
  return nullptr;
}

float color4_channel_value(const float4 &value, const char *channel)
{
  if (channel[0] == 'R') {
    return value.x;
  }
  if (channel[0] == 'G') {
    return value.y;
  }
  if (channel[0] == 'B') {
    return value.z;
  }
  return value.w;
}

}  // namespace

bool validate(const Graph &source)
{
  unordered_map<string, const Node *> nodes_by_name;
  return validate(source, &nodes_by_name);
}

bool lower(const Graph &source, ShaderGraph *graph)
{
  if (graph == nullptr) {
    return false;
  }

  const size_t original_node_count = graph->nodes.size();
  const auto rollback = [&]() {
    graph->nodes.resize(original_node_count);
    return false;
  };

  unordered_map<string, const Node *> nodes_by_name;
  if (!validate(source, &nodes_by_name)) {
    return rollback();
  }

  /* Color4 values are represented internally as RGB plus a parallel alpha scalar. */

  unordered_map<string, ShaderNode *> lowered_nodes;
  for (const Node &node : source.nodes) {
    ShaderNode *lowered = nullptr;
    bool preserve_lowered_name = false;
    /* These Vector4/Color4 adapter branches are hoisted out of the main
     * else-if chain below (each `continue`s immediately after the shared
     * name/registration tail that the chain's fallthrough performs at the
     * bottom of the loop) purely to keep the chain's nesting depth under
     * MSVC's internal block-nesting limit (C1061); they are otherwise
     * ordinary members of that dispatch and must stay mutually exclusive
     * with every nodedef checked below. */
    if (is_inside_outside(node.nodedef)) {
      const Type value_type = inside_outside_type(node.nodedef);
      const bool outside = is_outside(node.nodedef);
      MathNode *one_minus_mask = nullptr;
      if (outside) {
        one_minus_mask = graph->create_node<MathNode>();
        one_minus_mask->name = node.name + ".mask";
        one_minus_mask->set_math_type(NODE_MATH_SUBTRACT);
        one_minus_mask->set_value1(1.0f);
        if (const auto mask = node.inputs.find("mask"); mask != node.inputs.end()) {
          one_minus_mask->set_value2(mask->second);
        }
        lowered_nodes.emplace(one_minus_mask->name, one_minus_mask);
      }
      if (value_type == Type::Float) {
        MathNode *multiply = graph->create_node<MathNode>();
        multiply->set_math_type(NODE_MATH_MULTIPLY);
        if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
          multiply->set_value1(input->second);
        }
        if (!outside) {
          if (const auto mask = node.inputs.find("mask"); mask != node.inputs.end()) {
            multiply->set_value2(mask->second);
          }
        }
        lowered = multiply;
      }
      else if (value_type == Type::Color3) {
        CombineColorNode *mask = graph->create_node<CombineColorNode>();
        mask->name = node.name + ".mask_color";
        mask->set_color_type(NODE_COMBSEP_COLOR_RGB);
        if (!outside) {
          if (const auto mask_value = node.inputs.find("mask"); mask_value != node.inputs.end()) {
            mask->set_r(mask_value->second);
            mask->set_g(mask_value->second);
            mask->set_b(mask_value->second);
          }
        }
        MixNode *multiply = graph->create_node<MixNode>();
        multiply->set_mix_type(NODE_MIX_MUL);
        multiply->set_fac(1.0f);
        if (const auto input = node.color3_inputs.find("in"); input != node.color3_inputs.end()) {
          multiply->set_color1(input->second);
        }
        lowered_nodes.emplace(mask->name, mask);
        lowered = multiply;
      }
      else {
        CombineColorNode *mask = graph->create_node<CombineColorNode>();
        mask->name = node.name + ".mask_color";
        mask->set_color_type(NODE_COMBSEP_COLOR_RGB);
        if (!outside) {
          if (const auto mask_value = node.inputs.find("mask"); mask_value != node.inputs.end()) {
            mask->set_r(mask_value->second);
            mask->set_g(mask_value->second);
            mask->set_b(mask_value->second);
          }
        }
        MixNode *multiply = graph->create_node<MixNode>();
        multiply->set_mix_type(NODE_MIX_MUL);
        multiply->set_fac(1.0f);
        const float4 value = node.float4_inputs.contains("in") ? node.float4_inputs.at("in") : zero_float4();
        multiply->set_color1(make_float3(value.x, value.y, value.z));
        MathNode *alpha = graph->create_node<MathNode>();
        alpha->name = node.name + ".Alpha";
        alpha->set_math_type(NODE_MATH_MULTIPLY);
        alpha->set_value1(value.w);
        if (!outside) {
          if (const auto mask_value = node.inputs.find("mask"); mask_value != node.inputs.end()) {
            alpha->set_value2(mask_value->second);
          }
        }
        lowered_nodes.emplace(mask->name, mask);
        lowered_nodes.emplace(alpha->name, alpha);
        lowered = multiply;
      }
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (Type triplanar_type; triplanarprojection_type(node.nodedef, &triplanar_type)) {
      SeparateXYZNode *position = graph->create_node<SeparateXYZNode>();
      position->name = node.name + ".position";
      CombineXYZNode *uv_x = graph->create_node<CombineXYZNode>();
      uv_x->name = node.name + ".uv_x";
      uv_x->set_z(0.0f);
      CombineXYZNode *uv_y = graph->create_node<CombineXYZNode>();
      uv_y->name = node.name + ".uv_y";
      uv_y->set_z(0.0f);
      CombineXYZNode *uv_z = graph->create_node<CombineXYZNode>();
      uv_z->name = node.name + ".uv_z";
      uv_z->set_z(0.0f);
      ImageTextureNode *image_x = graph->create_node<ImageTextureNode>();
      image_x->name = node.name + ".image_x";
      image_x->set_filename(ustring(node.asset_inputs.at("filex")));
      image_x->set_interpolation(triplanarprojection_filter(node.string_inputs.at("filtertype")));
      ImageTextureNode *image_y = graph->create_node<ImageTextureNode>();
      image_y->name = node.name + ".image_y";
      image_y->set_filename(ustring(node.asset_inputs.at("filey")));
      image_y->set_interpolation(triplanarprojection_filter(node.string_inputs.at("filtertype")));
      ImageTextureNode *image_z = graph->create_node<ImageTextureNode>();
      image_z->name = node.name + ".image_z";
      image_z->set_filename(ustring(node.asset_inputs.at("filez")));
      image_z->set_interpolation(triplanarprojection_filter(node.string_inputs.at("filtertype")));
      if (triplanar_type == Type::Vector2 || triplanar_type == Type::Vector3 ||
          triplanar_type == Type::Vector4)
      {
        image_x->set_colorspace(u_colorspace_data);
        image_y->set_colorspace(u_colorspace_data);
        image_z->set_colorspace(u_colorspace_data);
      }

      VectorMathNode *normal_normalize = graph->create_node<VectorMathNode>();
      normal_normalize->name = node.name + ".normal.normalize";
      normal_normalize->set_math_type(NODE_VECTOR_MATH_NORMALIZE);
      VectorMathNode *normal_abs = graph->create_node<VectorMathNode>();
      normal_abs->name = node.name + ".normal.abs";
      normal_abs->set_math_type(NODE_VECTOR_MATH_ABSOLUTE);
      VectorMathNode *normal_dot = graph->create_node<VectorMathNode>();
      normal_dot->name = node.name + ".normal.dot";
      normal_dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
      normal_dot->set_vector2(make_float3(1.0f, 1.0f, 1.0f));
      MathNode *normal_dot_inverse = graph->create_node<MathNode>();
      normal_dot_inverse->name = node.name + ".normal.inverse";
      normal_dot_inverse->set_math_type(NODE_MATH_DIVIDE);
      normal_dot_inverse->set_value1(1.0f);
      VectorMathNode *normal_weights = graph->create_node<VectorMathNode>();
      normal_weights->name = node.name + ".weights";
      normal_weights->set_math_type(NODE_VECTOR_MATH_SCALE);
      ClampNode *blend_clamp = graph->create_node<ClampNode>();
      blend_clamp->name = node.name + ".blend.clamp";
      blend_clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      blend_clamp->set_min(0.03f);
      blend_clamp->set_max(1.0e20f);
      if (const auto blend = node.inputs.find("blend"); blend != node.inputs.end()) {
        blend_clamp->set_value(blend->second);
      }
      MathNode *one_over_blend = graph->create_node<MathNode>();
      one_over_blend->name = node.name + ".blend.inverse";
      one_over_blend->set_math_type(NODE_MATH_DIVIDE);
      one_over_blend->set_value1(1.0f);
      VectorMathNode *blend_power = graph->create_node<VectorMathNode>();
      blend_power->name = node.name + ".blend.power";
      blend_power->set_math_type(NODE_VECTOR_MATH_POWER);
      CombineXYZNode *blend_exponent = graph->create_node<CombineXYZNode>();
      blend_exponent->name = node.name + ".blend.exponent";
      VectorMathNode *blend_dot = graph->create_node<VectorMathNode>();
      blend_dot->name = node.name + ".blend.dot";
      blend_dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
      blend_dot->set_vector2(make_float3(1.0f, 1.0f, 1.0f));
      MathNode *blend_dot_inverse = graph->create_node<MathNode>();
      blend_dot_inverse->name = node.name + ".blend.weight_inverse";
      blend_dot_inverse->set_math_type(NODE_MATH_DIVIDE);
      blend_dot_inverse->set_value1(1.0f);
      VectorMathNode *blend_weights = graph->create_node<VectorMathNode>();
      blend_weights->name = node.name + ".blend.weights";
      blend_weights->set_math_type(NODE_VECTOR_MATH_SCALE);
      SeparateXYZNode *separate_weights = graph->create_node<SeparateXYZNode>();
      separate_weights->name = node.name + ".separate_weights";

      lowered_nodes.emplace(position->name, position);
      lowered_nodes.emplace(uv_x->name, uv_x);
      lowered_nodes.emplace(uv_y->name, uv_y);
      lowered_nodes.emplace(uv_z->name, uv_z);
      lowered_nodes.emplace(image_x->name, image_x);
      lowered_nodes.emplace(image_y->name, image_y);
      lowered_nodes.emplace(image_z->name, image_z);
      lowered_nodes.emplace(normal_normalize->name, normal_normalize);
      lowered_nodes.emplace(normal_abs->name, normal_abs);
      lowered_nodes.emplace(normal_dot->name, normal_dot);
      lowered_nodes.emplace(normal_dot_inverse->name, normal_dot_inverse);
      lowered_nodes.emplace(normal_weights->name, normal_weights);
      lowered_nodes.emplace(blend_clamp->name, blend_clamp);
      lowered_nodes.emplace(one_over_blend->name, one_over_blend);
      lowered_nodes.emplace(blend_power->name, blend_power);
      lowered_nodes.emplace(blend_exponent->name, blend_exponent);
      lowered_nodes.emplace(blend_dot->name, blend_dot);
      lowered_nodes.emplace(blend_dot_inverse->name, blend_dot_inverse);
      lowered_nodes.emplace(blend_weights->name, blend_weights);
      lowered_nodes.emplace(separate_weights->name, separate_weights);

      const char *axis_names[] = {"x", "y", "z"};
      if (triplanar_type == Type::Float) {
        for (const char *axis : axis_names) {
          SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
          separate->name = node.name + "." + axis + ".separate";
          separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
          MathNode *multiply = graph->create_node<MathNode>();
          multiply->name = node.name + "." + axis + ".weighted";
          multiply->set_math_type(NODE_MATH_MULTIPLY);
          lowered_nodes.emplace(separate->name, separate);
          lowered_nodes.emplace(multiply->name, multiply);
        }
        MathNode *sum_xy = graph->create_node<MathNode>();
        sum_xy->name = node.name + ".sum_xy";
        sum_xy->set_math_type(NODE_MATH_ADD);
        lowered_nodes.emplace(sum_xy->name, sum_xy);
        lowered = graph->create_node<MathNode>();
        static_cast<MathNode *>(lowered)->set_math_type(NODE_MATH_ADD);
      }
      else if (triplanar_type == Type::Color3 || triplanar_type == Type::Color4 ||
               triplanar_type == Type::Vector3 || triplanar_type == Type::Vector4)
      {
        for (const char *axis : axis_names) {
          CombineColorNode *weight = graph->create_node<CombineColorNode>();
          weight->name = node.name + "." + axis + ".weight";
          weight->set_color_type(NODE_COMBSEP_COLOR_RGB);
          MixNode *multiply = graph->create_node<MixNode>();
          multiply->name = node.name + "." + axis + ".weighted";
          multiply->set_mix_type(NODE_MIX_MUL);
          multiply->set_fac(1.0f);
          lowered_nodes.emplace(weight->name, weight);
          lowered_nodes.emplace(multiply->name, multiply);
        }
        MixNode *sum_xy = graph->create_node<MixNode>();
        sum_xy->name = node.name + ".sum_xy";
        sum_xy->set_mix_type(NODE_MIX_ADD);
        sum_xy->set_fac(1.0f);
        lowered_nodes.emplace(sum_xy->name, sum_xy);
        lowered = graph->create_node<MixNode>();
        static_cast<MixNode *>(lowered)->set_mix_type(NODE_MIX_ADD);
        static_cast<MixNode *>(lowered)->set_fac(1.0f);
        if (triplanarprojection_is_four_component(node.nodedef)) {
          for (const char *axis : axis_names) {
            MathNode *multiply = graph->create_node<MathNode>();
            multiply->name = node.name + "." + axis +
                             triplanarprojection_component_suffix(triplanar_type) + ".weighted";
            multiply->set_math_type(NODE_MATH_MULTIPLY);
            lowered_nodes.emplace(multiply->name, multiply);
          }
          MathNode *sum_xy_w = graph->create_node<MathNode>();
          sum_xy_w->name = node.name + triplanarprojection_component_suffix(triplanar_type) +
                           ".sum_xy";
          sum_xy_w->set_math_type(NODE_MATH_ADD);
          MathNode *sum_w = graph->create_node<MathNode>();
          sum_w->name = node.name + triplanarprojection_component_suffix(triplanar_type);
          sum_w->set_math_type(NODE_MATH_ADD);
          lowered_nodes.emplace(sum_xy_w->name, sum_xy_w);
          lowered_nodes.emplace(sum_w->name, sum_w);
        }
      }
      else {
        for (const char *axis : axis_names) {
          SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
          separate->name = node.name + "." + axis + ".separate";
          separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
          MathNode *x = graph->create_node<MathNode>();
          x->name = node.name + "." + axis + ".x.weighted";
          x->set_math_type(NODE_MATH_MULTIPLY);
          MathNode *y = graph->create_node<MathNode>();
          y->name = node.name + "." + axis + ".y.weighted";
          y->set_math_type(NODE_MATH_MULTIPLY);
          lowered_nodes.emplace(separate->name, separate);
          lowered_nodes.emplace(x->name, x);
          lowered_nodes.emplace(y->name, y);
        }
        MathNode *sum_x_xy = graph->create_node<MathNode>();
        sum_x_xy->name = node.name + ".x.sum_xy";
        sum_x_xy->set_math_type(NODE_MATH_ADD);
        MathNode *sum_y_xy = graph->create_node<MathNode>();
        sum_y_xy->name = node.name + ".y.sum_xy";
        sum_y_xy->set_math_type(NODE_MATH_ADD);
        MathNode *sum_x = graph->create_node<MathNode>();
        sum_x->name = node.name + ".x.sum";
        sum_x->set_math_type(NODE_MATH_ADD);
        MathNode *sum_y = graph->create_node<MathNode>();
        sum_y->name = node.name + ".y.sum";
        sum_y->set_math_type(NODE_MATH_ADD);
        lowered_nodes.emplace(sum_x_xy->name, sum_x_xy);
        lowered_nodes.emplace(sum_y_xy->name, sum_y_xy);
        lowered_nodes.emplace(sum_x->name, sum_x);
        lowered_nodes.emplace(sum_y->name, sum_y);
        lowered = graph->create_node<CombineXYZNode>();
        static_cast<CombineXYZNode *>(lowered)->set_z(0.0f);
      }
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (is_contrast_color3(node.nodedef)) {
      const bool scalar_parameters = contrast_uses_scalar_parameters(node.nodedef);
      SeparateColorNode *input = graph->create_node<SeparateColorNode>();
      input->name = node.name + ".input";
      input->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(input->name, input);
      for (const char *channel : {"Red", "Green", "Blue"}) {
        const auto component = [channel](const float3 &value) {
          return channel[0] == 'R' ? value.x : channel[0] == 'G' ? value.y : value.z;
        };
        const float pivot = scalar_parameters ? node.inputs.at("pivot") :
                                                component(node.color3_inputs.at("pivot"));
        const float amount = scalar_parameters ? node.inputs.at("amount") :
                                                 component(node.color3_inputs.at("amount"));
        MathNode *subtract = graph->create_node<MathNode>();
        subtract->name = node.name + "." + channel + ".subtract";
        subtract->set_math_type(NODE_MATH_SUBTRACT);
        subtract->set_value2(pivot);
        if (const auto value = node.color3_inputs.find("in"); value != node.color3_inputs.end()) {
          subtract->set_value1(component(value->second));
        }
        MathNode *multiply = graph->create_node<MathNode>();
        multiply->name = node.name + "." + channel + ".multiply";
        multiply->set_math_type(NODE_MATH_MULTIPLY);
        multiply->set_value2(amount);
        MathNode *add = graph->create_node<MathNode>();
        add->name = node.name + "." + channel;
        add->set_math_type(NODE_MATH_ADD);
        add->set_value2(pivot);
        lowered_nodes.emplace(subtract->name, subtract);
        lowered_nodes.emplace(multiply->name, multiply);
        lowered_nodes.emplace(add->name, add);
      }
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    else if (is_vector2_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>();
      condition->name = node.name + ".condition";
      MathNode *greater = nullptr;
      MathNode *equal = nullptr;
      if (node.nodedef == ifgreater_vector2_id) {
        condition->set_math_type(NODE_MATH_GREATER_THAN);
      }
      else if (node.nodedef == ifequal_vector2_id) {
        condition->set_math_type(NODE_MATH_COMPARE);
        condition->set_value3(0.0f);
      }
      else {
        greater = graph->create_node<MathNode>();
        greater->name = node.name + ".greater";
        greater->set_math_type(NODE_MATH_GREATER_THAN);
        equal = graph->create_node<MathNode>();
        equal->name = node.name + ".equal";
        equal->set_math_type(NODE_MATH_COMPARE);
        equal->set_value3(0.0f);
        condition->set_math_type(NODE_MATH_MAXIMUM);
        lowered_nodes.emplace(greater->name, greater);
        lowered_nodes.emplace(equal->name, equal);
      }
      for (const auto &[input_name, value] : node.inputs) {
        if (input_name == "value1") {
          if (node.nodedef == ifgreatereq_vector2_id) {
            greater->set_value1(value);
            equal->set_value1(value);
          }
          else {
            condition->set_value1(value);
          }
        }
        else if (input_name == "value2") {
          if (node.nodedef == ifgreatereq_vector2_id) {
            greater->set_value2(value);
            equal->set_value2(value);
          }
          else {
            condition->set_value2(value);
          }
        }
      }
      MixVectorNode *mix = graph->create_node<MixVectorNode>();
      mix->set_fac(0.0f);
      if (const auto value = node.vector2_inputs.find("in2"); value != node.vector2_inputs.end()) {
        mix->set_a(make_float3(value->second.x, value->second.y, 0.0f));
      }
      if (const auto value = node.vector2_inputs.find("in1"); value != node.vector2_inputs.end()) {
        mix->set_b(make_float3(value->second.x, value->second.y, 0.0f));
      }
      mix->name = node.name;
      lowered_nodes.emplace(condition->name, condition);
      lowered_nodes.emplace(node.name, mix);
      continue;
    }
    if (is_vector4_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>();
      condition->name = node.name + ".condition";
      MathNode *greater = nullptr;
      MathNode *equal = nullptr;
      if (node.nodedef == ifgreater_vector4_id) {
        condition->set_math_type(NODE_MATH_GREATER_THAN);
      }
      else if (node.nodedef == ifequal_vector4_id) {
        condition->set_math_type(NODE_MATH_COMPARE);
        condition->set_value3(0.0f);
      }
      else {
        greater = graph->create_node<MathNode>();
        greater->name = node.name + ".greater";
        greater->set_math_type(NODE_MATH_GREATER_THAN);
        equal = graph->create_node<MathNode>();
        equal->name = node.name + ".equal";
        equal->set_math_type(NODE_MATH_COMPARE);
        equal->set_value3(0.0f);
        condition->set_math_type(NODE_MATH_MAXIMUM);
        lowered_nodes.emplace(greater->name, greater);
        lowered_nodes.emplace(equal->name, equal);
      }
      for (const auto &[input_name, value] : node.inputs) {
        if (input_name == "value1") {
          if (node.nodedef == ifgreatereq_vector4_id) {
            greater->set_value1(value);
            equal->set_value1(value);
          }
          else {
            condition->set_value1(value);
          }
        }
        else if (input_name == "value2") {
          if (node.nodedef == ifgreatereq_vector4_id) {
            greater->set_value2(value);
            equal->set_value2(value);
          }
          else {
            condition->set_value2(value);
          }
        }
      }
      MixVectorNode *mix = graph->create_node<MixVectorNode>();
      mix->set_fac(0.0f);
      if (const auto value = node.vector4_inputs.find("in2"); value != node.vector4_inputs.end()) {
        mix->set_a(make_float3(value->second.x, value->second.y, value->second.z));
      }
      if (const auto value = node.vector4_inputs.find("in1"); value != node.vector4_inputs.end()) {
        mix->set_b(make_float3(value->second.x, value->second.y, value->second.z));
      }
      MathNode *w_delta = graph->create_node<MathNode>();
      w_delta->name = node.name + ".W.delta";
      w_delta->set_math_type(NODE_MATH_SUBTRACT);
      w_delta->set_value1(node.vector4_inputs.contains("in1") ? node.vector4_inputs.at("in1").w : 0.0f);
      w_delta->set_value2(node.vector4_inputs.contains("in2") ? node.vector4_inputs.at("in2").w : 0.0f);
      MathNode *w_product = graph->create_node<MathNode>();
      w_product->name = node.name + ".W.product";
      w_product->set_math_type(NODE_MATH_MULTIPLY);
      MathNode *w_sum = graph->create_node<MathNode>();
      w_sum->name = node.name + ".W";
      w_sum->set_math_type(NODE_MATH_ADD);
      w_sum->set_value1(node.vector4_inputs.contains("in2") ? node.vector4_inputs.at("in2").w : 0.0f);
      mix->name = node.name;
      lowered_nodes.emplace(condition->name, condition);
      lowered_nodes.emplace(w_delta->name, w_delta);
      lowered_nodes.emplace(w_product->name, w_product);
      lowered_nodes.emplace(w_sum->name, w_sum);
      lowered_nodes.emplace(node.name, mix);
      continue;
    }
    if (is_color4_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>();
      condition->name = node.name + ".condition";
      MathNode *greater = nullptr;
      MathNode *equal = nullptr;
      if (node.nodedef == ifgreater_color4_id) {
        condition->set_math_type(NODE_MATH_GREATER_THAN);
      }
      else if (node.nodedef == ifequal_color4_id) {
        condition->set_math_type(NODE_MATH_COMPARE);
        condition->set_value3(0.0f);
      }
      else {
        greater = graph->create_node<MathNode>();
        greater->name = node.name + ".greater";
        greater->set_math_type(NODE_MATH_GREATER_THAN);
        equal = graph->create_node<MathNode>();
        equal->name = node.name + ".equal";
        equal->set_math_type(NODE_MATH_COMPARE);
        equal->set_value3(0.0f);
        condition->set_math_type(NODE_MATH_MAXIMUM);
        lowered_nodes.emplace(greater->name, greater);
        lowered_nodes.emplace(equal->name, equal);
      }
      for (const auto &[input_name, value] : node.inputs) {
        if (input_name == "value1") {
          if (node.nodedef == ifgreatereq_color4_id) {
            greater->set_value1(value);
            equal->set_value1(value);
          }
          else {
            condition->set_value1(value);
          }
        }
        else if (input_name == "value2") {
          if (node.nodedef == ifgreatereq_color4_id) {
            greater->set_value2(value);
            equal->set_value2(value);
          }
          else {
            condition->set_value2(value);
          }
        }
      }
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(NODE_MIX_BLEND);
      const float4 bg = node.float4_inputs.contains("in2") ? node.float4_inputs.at("in2") : zero_float4();
      const float4 fg = node.float4_inputs.contains("in1") ? node.float4_inputs.at("in1") : zero_float4();
      mix->set_color1(make_float3(bg.x, bg.y, bg.z));
      mix->set_color2(make_float3(fg.x, fg.y, fg.z));
      MathNode *alpha_delta = graph->create_node<MathNode>();
      alpha_delta->name = node.name + ".Alpha.delta";
      alpha_delta->set_math_type(NODE_MATH_SUBTRACT);
      alpha_delta->set_value1(fg.w);
      alpha_delta->set_value2(bg.w);
      MathNode *alpha_product = graph->create_node<MathNode>();
      alpha_product->name = node.name + ".Alpha.product";
      alpha_product->set_math_type(NODE_MATH_MULTIPLY);
      MathNode *alpha_sum = graph->create_node<MathNode>();
      alpha_sum->name = node.name + ".Alpha";
      alpha_sum->set_math_type(NODE_MATH_ADD);
      alpha_sum->set_value1(bg.w);
      mix->name = node.name;
      lowered_nodes.emplace(condition->name, condition);
      lowered_nodes.emplace(alpha_delta->name, alpha_delta);
      lowered_nodes.emplace(alpha_product->name, alpha_product);
      lowered_nodes.emplace(alpha_sum->name, alpha_sum);
      lowered_nodes.emplace(node.name, mix);
      continue;
    }
    if (is_contrast_vector2(node.nodedef) || is_contrast_vector3(node.nodedef)) {
      const bool vector2 = is_contrast_vector2(node.nodedef);
      const bool scalar_parameters = contrast_uses_scalar_parameters(node.nodedef);
      SeparateXYZNode *input = graph->create_node<SeparateXYZNode>();
      input->name = node.name + ".input";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (vector2) {
        combine->set_z(0.0f);
      }
      lowered_nodes.emplace(input->name, input);
      for (const char *channel : {"X", "Y", "Z"}) {
        if (vector2 && channel[0] == 'Z') {
          continue;
        }
        const auto component = [channel](const float3 &value) {
          return channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z;
        };
        const auto component2 = [channel](const float2 &value) {
          return channel[0] == 'X' ? value.x : value.y;
        };
        const float pivot = scalar_parameters ? node.inputs.at("pivot") :
                            vector2 ? component2(node.vector2_inputs.at("pivot")) :
                                      component(node.vector3_inputs.at("pivot"));
        const float amount = scalar_parameters ? node.inputs.at("amount") :
                             vector2 ? component2(node.vector2_inputs.at("amount")) :
                                       component(node.vector3_inputs.at("amount"));
        MathNode *subtract = graph->create_node<MathNode>();
        subtract->name = node.name + "." + channel + ".subtract";
        subtract->set_math_type(NODE_MATH_SUBTRACT);
        subtract->set_value2(pivot);
        if (vector2) {
          if (const auto value = node.vector2_inputs.find("in"); value != node.vector2_inputs.end()) {
            subtract->set_value1(component2(value->second));
          }
        }
        else if (const auto value = node.vector3_inputs.find("in"); value != node.vector3_inputs.end()) {
          subtract->set_value1(component(value->second));
        }
        MathNode *multiply = graph->create_node<MathNode>();
        multiply->name = node.name + "." + channel + ".multiply";
        multiply->set_math_type(NODE_MATH_MULTIPLY);
        multiply->set_value2(amount);
        MathNode *add = graph->create_node<MathNode>();
        add->name = node.name + "." + channel;
        add->set_math_type(NODE_MATH_ADD);
        add->set_value2(pivot);
        lowered_nodes.emplace(subtract->name, subtract);
        lowered_nodes.emplace(multiply->name, multiply);
        lowered_nodes.emplace(add->name, add);
      }
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_vector3_vector4_id) {
      /* stdlib_ng.mtlx NG_convert_vector3_vector4 copies XYZ and fixes W to 1.0. */
      ValueNode *w = graph->create_node<ValueNode>();
      w->name = node.name + ".W";
      w->set_value(1.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = lowered_nodes.at(node.links.at("in").source_node);
      preserve_lowered_name = true;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (is_contrast_float(node.nodedef)) {
      MathNode *subtract = graph->create_node<MathNode>();
      subtract->name = node.name + ".subtract";
      subtract->set_math_type(NODE_MATH_SUBTRACT);
      MathNode *multiply = graph->create_node<MathNode>();
      multiply->name = node.name + ".multiply";
      multiply->set_math_type(NODE_MATH_MULTIPLY);
      multiply->set_value2(node.inputs.at("amount"));
      MathNode *add = graph->create_node<MathNode>();
      add->set_math_type(NODE_MATH_ADD);
      add->set_value2(node.inputs.at("pivot"));
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        subtract->set_value1(input->second);
      }
      subtract->set_value2(node.inputs.at("pivot"));
      lowered_nodes.emplace(subtract->name, subtract);
      lowered_nodes.emplace(multiply->name, multiply);
      lowered = add;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == combine2_vector4vf_id) {
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      lowered_nodes.emplace(first->name, first);
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      if (const auto input = node.vector3_inputs.find("in1"); input != node.vector3_inputs.end()) {
        vector->set_x(input->second.x);
        vector->set_y(input->second.y);
        vector->set_z(input->second.z);
      }
      MathNode *w = graph->create_node<MathNode>();
      w->name = node.name + ".W";
      w->set_math_type(NODE_MATH_ADD);
      w->set_value1(node.inputs.contains("in2") ? node.inputs.at("in2") : 0.0f);
      w->set_value2(0.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == combine2_vector4vv_id) {
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      SeparateXYZNode *second = graph->create_node<SeparateXYZNode>();
      second->name = node.name + ".second";
      lowered_nodes.emplace(first->name, first);
      lowered_nodes.emplace(second->name, second);
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      if (const auto input = node.vector2_inputs.find("in1"); input != node.vector2_inputs.end()) {
        vector->set_x(input->second.x);
        vector->set_y(input->second.y);
      }
      if (const auto input = node.vector2_inputs.find("in2"); input != node.vector2_inputs.end()) {
        vector->set_z(input->second.x);
      }
      MathNode *w = graph->create_node<MathNode>();
      w->name = node.name + ".W";
      w->set_math_type(NODE_MATH_ADD);
      w->set_value1(node.vector2_inputs.contains("in2") ? node.vector2_inputs.at("in2").y : 0.0f);
      w->set_value2(0.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == combine4_vector4_id) {
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) {
        vector->set_x(input->second);
      }
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
        vector->set_y(input->second);
      }
      if (const auto input = node.inputs.find("in3"); input != node.inputs.end()) {
        vector->set_z(input->second);
      }
      MathNode *w = graph->create_node<MathNode>();
      w->name = node.name + ".W";
      w->set_math_type(NODE_MATH_ADD);
      w->set_value1(node.inputs.contains("in4") ? node.inputs.at("in4") : 0.0f);
      w->set_value2(0.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == separate4_vector4_id) {
      lowered = graph->create_node<SeparateXYZNode>();
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (is_vector4_math_or_clamp(node.nodedef)) {
      const bool clamp = node.nodedef == clamp_vector4_id || node.nodedef == clamp_vector4fa_id;
      const bool scalar_second = vector4_math_uses_scalar_second(node.nodedef) ||
                                 node.nodedef == clamp_vector4fa_id;
      auto vector3_from_vector4 = [](const float4 value) {
        return make_float3(value.x, value.y, value.z);
      };
      if (clamp) {
        const bool scalar_bounds = node.nodedef == clamp_vector4fa_id;
        const float4 low = scalar_bounds ? make_float4(node.inputs.contains("low") ?
                                                           node.inputs.at("low") :
                                                           0.0f) :
                                           (node.vector4_inputs.contains("low") ?
                                                node.vector4_inputs.at("low") :
                                                zero_float4());
        const float4 high = scalar_bounds ? make_float4(node.inputs.contains("high") ?
                                                            node.inputs.at("high") :
                                                            1.0f) :
                                            (node.vector4_inputs.contains("high") ?
                                                 node.vector4_inputs.at("high") :
                                                 make_float4(1.0f, 1.0f, 1.0f, 1.0f));
        VectorMathNode *minimum = graph->create_node<VectorMathNode>();
        minimum->name = node.name + ".minimum";
        minimum->set_math_type(NODE_VECTOR_MATH_MINIMUM);
        minimum->set_vector2(vector3_from_vector4(high));
        if (const auto input = node.vector4_inputs.find("in"); input != node.vector4_inputs.end()) {
          minimum->set_vector1(vector3_from_vector4(input->second));
        }
        VectorMathNode *maximum = graph->create_node<VectorMathNode>();
        maximum->set_math_type(NODE_VECTOR_MATH_MAXIMUM);
        maximum->set_vector2(vector3_from_vector4(low));
        MathNode *w_minimum = graph->create_node<MathNode>();
        w_minimum->name = node.name + ".W.minimum";
        w_minimum->set_math_type(NODE_MATH_MINIMUM);
        w_minimum->set_value2(high.w);
        MathNode *w_maximum = graph->create_node<MathNode>();
        w_maximum->name = node.name + ".W";
        w_maximum->set_math_type(NODE_MATH_MAXIMUM);
        w_maximum->set_value2(low.w);
        lowered_nodes.emplace(minimum->name, minimum);
        lowered_nodes.emplace(w_minimum->name, w_minimum);
        lowered_nodes.emplace(w_maximum->name, w_maximum);
        lowered = maximum;
      }
      else {
        NodeVectorMathType vector_type;
        NodeMathType w_type;
        vector4_math_type(node.nodedef, &vector_type, &w_type);
        VectorMathNode *math = graph->create_node<VectorMathNode>();
        math->set_math_type(vector_type);
        if (const auto input = node.vector4_inputs.find("in1"); input != node.vector4_inputs.end()) {
          math->set_vector1(vector3_from_vector4(input->second));
        }
        const float4 second = scalar_second ? make_float4(node.inputs.contains("in2") ?
                                                              node.inputs.at("in2") :
                                                              1.0f) :
                                      (node.vector4_inputs.contains("in2") ?
                                           node.vector4_inputs.at("in2") :
                                           make_float4(1.0f, 1.0f, 1.0f, 1.0f));
        math->set_vector2(vector3_from_vector4(second));
        MathNode *w = graph->create_node<MathNode>();
        w->name = node.name + ".W";
        w->set_math_type(w_type);
        if (const auto input = node.vector4_inputs.find("in1"); input != node.vector4_inputs.end()) {
          w->set_value1(input->second.w);
        }
        w->set_value2(second.w);
        lowered_nodes.emplace(w->name, w);
        lowered = math;
      }
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_color3_vector4_id || node.nodedef == convert_vector2_vector4_id) {
      if (node.nodedef == convert_color3_vector4_id) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->name = node.name + ".separate";
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        lowered_nodes.emplace(separate->name, separate);
      }
      else {
        SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
        separate->name = node.name + ".separate";
        lowered_nodes.emplace(separate->name, separate);
      }
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      if (node.nodedef == convert_vector2_vector4_id) {
        vector->set_z(0.0f);
      }
      ValueNode *w = graph->create_node<ValueNode>();
      w->name = node.name + ".W";
      w->set_value(1.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_color4_vector4_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      MathNode *w = graph->create_node<MathNode>();
      w->name = node.name + ".W";
      w->set_math_type(NODE_MATH_ADD);
      w->set_value2(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_float_vector4_id || node.nodedef == convert_boolean_vector4_id ||
        node.nodedef == convert_integer_vector4_id)
    {
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      ValueNode *w = graph->create_node<ValueNode>();
      w->name = node.name + ".W";
      w->set_value(0.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_vector4_vector3_id || node.nodedef == convert_vector4_vector2_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      lowered_nodes.emplace(separate->name, separate);
      lowered = node.nodedef == convert_vector4_vector3_id ? static_cast<ShaderNode *>(separate) :
                                                             static_cast<ShaderNode *>(graph->create_node<CombineXYZNode>());
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_color4_vector3_id || node.nodedef == convert_color4_vector2_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (node.nodedef == convert_color4_vector2_id) {
        combine->set_z(0.0f);
      }
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_vector4_color4_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineColorNode *color = graph->create_node<CombineColorNode>();
      color->set_color_type(NODE_COMBSEP_COLOR_RGB);
      MathNode *alpha = graph->create_node<MathNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_math_type(NODE_MATH_ADD);
      alpha->set_value2(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = color;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == extract_vector4_id) {
      if (node.int_inputs.at("index") == 3) {
        lowered = lowered_nodes.at(node.links.at("in").source_node + ".W");
      }
      else {
        lowered = graph->create_node<SeparateXYZNode>();
        lowered->name = node.name;
      }
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == convert_float_color4_id || node.nodedef == convert_boolean_color4_id ||
        node.nodedef == convert_integer_color4_id)
    {
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      ValueNode *alpha = graph->create_node<ValueNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_value(0.0f);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == combine2_color4cf_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".input";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      ValueNode *alpha = graph->create_node<ValueNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_value(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == combine4_color4_id) {
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) {
        combine->set_r(input->second);
      }
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
        combine->set_g(input->second);
      }
      if (const auto input = node.inputs.find("in3"); input != node.inputs.end()) {
        combine->set_b(input->second);
      }
      ValueNode *alpha = graph->create_node<ValueNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_value(node.inputs.contains("in4") ? node.inputs.at("in4") : 0.0f);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = combine;
      lowered->name = node.name;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (node.nodedef == dot_surfaceshader_id) {
      lowered = lowered_nodes.at(node.links.at("in").source_node);
      preserve_lowered_name = true;
      lowered_nodes.emplace(node.name, lowered);
      continue;
    }
    if (is_exact_color_burn_dodge(node.nodedef)) {
      const bool burn = node.nodedef == burn_color3_id;
      SeparateColorNode *foreground = graph->create_node<SeparateColorNode>();
      foreground->name = node.name + ".foreground";
      foreground->set_color_type(NODE_COMBSEP_COLOR_RGB);
      SeparateColorNode *background = graph->create_node<SeparateColorNode>();
      background->name = node.name + ".background";
      background->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(foreground->name, foreground);
      lowered_nodes.emplace(background->name, background);
      const auto component = [](const float3 &value, const char *channel) {
        return channel[0] == 'R' ? value.x : channel[0] == 'G' ? value.y : value.z;
      };
      for (const char *channel : {"Red", "Green", "Blue"}) {
        const string prefix = node.name + "." + channel + ".";
        const auto create_math = [&](const char *suffix, const NodeMathType type) {
          MathNode *math = graph->create_node<MathNode>();
          math->name = prefix + suffix;
          math->set_math_type(type);
          lowered_nodes.emplace(math->name, math);
          return math;
        };
        MathNode *foreground_term = create_math(
            burn ? "foreground_abs" : "denominator", burn ? NODE_MATH_ABSOLUTE :
                                                            NODE_MATH_SUBTRACT);
        MathNode *denominator_abs = burn ? nullptr :
                                           create_math("denominator_abs", NODE_MATH_ABSOLUTE);
        MathNode *condition = create_math("condition", NODE_MATH_LESS_THAN);
        condition->set_value2(1.0e-8f);
        MathNode *safe_denominator = burn ?
                                         create_math("safe_denominator", NODE_MATH_ADD) :
                                         nullptr;
        MathNode *one_minus_background = burn ?
                                             create_math("one_minus_background",
                                                         NODE_MATH_SUBTRACT) :
                                             nullptr;
        if (one_minus_background) {
          one_minus_background->set_value1(1.0f);
        }
        if (!burn) {
          foreground_term->set_value1(1.0f);
        }
        MathNode *divide = create_math("divide", NODE_MATH_DIVIDE);
        MathNode *blend = burn ? create_math("blend", NODE_MATH_SUBTRACT) : divide;
        if (burn) {
          blend->set_value1(1.0f);
        }
        MathNode *mix_product = create_math("mix_product", NODE_MATH_MULTIPLY);
        MathNode *one_minus_mix = create_math("one_minus_mix", NODE_MATH_SUBTRACT);
        one_minus_mix->set_value1(1.0f);
        MathNode *background_product = create_math("background_product", NODE_MATH_MULTIPLY);
        MathNode *sum = create_math("sum", NODE_MATH_ADD);
        MathNode *inverse_condition = create_math("inverse_condition", NODE_MATH_SUBTRACT);
        inverse_condition->set_value1(1.0f);
        create_math("result", NODE_MATH_MULTIPLY);

        if (const auto fg = node.color3_inputs.find("fg"); fg != node.color3_inputs.end()) {
          const float value = component(fg->second, channel);
          foreground_term->set_value2(value);
          if (burn) {
            foreground_term->set_value1(value);
            safe_denominator->set_value1(value);
          }
        }
        if (const auto bg = node.color3_inputs.find("bg"); bg != node.color3_inputs.end()) {
          const float value = component(bg->second, channel);
          if (burn) {
            one_minus_background->set_value2(value);
          }
          else {
            divide->set_value1(value);
          }
          background_product->set_value2(value);
        }
        if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
          mix_product->set_value2(mix->second);
          one_minus_mix->set_value2(mix->second);
        }
        (void)denominator_abs;
        (void)safe_denominator;
        (void)sum;
      }
      lowered = combine;
    }
    else if (NodeMix blend_type; scalar_blend_type(node.nodedef, &blend_type) ||
                                 color_blend_type(node.nodedef, &blend_type))
    {
      MixColorNode *blend = graph->create_node<MixColorNode>();
      blend->set_blend_type(blend_type);
      blend->set_use_clamp(false);
      blend->set_use_clamp_result(false);
      if (const auto background = node.inputs.find("bg"); background != node.inputs.end()) {
        blend->set_a(make_float3(background->second, background->second, background->second));
      }
      if (const auto foreground = node.inputs.find("fg"); foreground != node.inputs.end()) {
        blend->set_b(make_float3(foreground->second, foreground->second, foreground->second));
      }
      if (const auto background = node.color3_inputs.find("bg");
          background != node.color3_inputs.end())
      {
        blend->set_a(background->second);
      }
      if (const auto foreground = node.color3_inputs.find("fg");
          foreground != node.color3_inputs.end())
      {
        blend->set_b(foreground->second);
      }
      if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
        blend->set_fac(mix->second);
      }
      lowered = blend;
    }
    else if (is_mix(node.nodedef)) {
      const Type value_type = mix_value_type(node.nodedef);
      if (value_type == Type::Float) {
        MathNode *delta = graph->create_node<MathNode>();
        delta->name = node.name + ".delta";
        delta->set_math_type(NODE_MATH_SUBTRACT);
        if (const auto foreground = node.inputs.find("fg"); foreground != node.inputs.end()) delta->set_value1(foreground->second);
        if (const auto background = node.inputs.find("bg"); background != node.inputs.end()) delta->set_value2(background->second);
        MathNode *product = graph->create_node<MathNode>();
        product->name = node.name + ".product";
        product->set_math_type(NODE_MATH_MULTIPLY);
        if (const auto factor = node.inputs.find("mix"); factor != node.inputs.end()) {
          product->set_value2(factor->second);
        }
        MathNode *sum = graph->create_node<MathNode>();
        sum->set_math_type(NODE_MATH_ADD);
        if (const auto background = node.inputs.find("bg"); background != node.inputs.end()) sum->set_value1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
      else if (value_type == Type::Color3) {
        MixNode *delta = graph->create_node<MixNode>();
        delta->name = node.name + ".delta";
        delta->set_mix_type(NODE_MIX_SUB);
        delta->set_fac(1.0f);
        if (const auto foreground = node.color3_inputs.find("fg"); foreground != node.color3_inputs.end()) delta->set_color1(foreground->second);
        if (const auto background = node.color3_inputs.find("bg"); background != node.color3_inputs.end()) delta->set_color2(background->second);
        CombineColorNode *factor = nullptr;
        if (mix_factor_type(node.nodedef) == Type::Float) {
          factor = graph->create_node<CombineColorNode>();
          factor->name = node.name + ".factor";
          factor->set_color_type(NODE_COMBSEP_COLOR_RGB);
          if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
            factor->set_r(mix->second);
            factor->set_g(mix->second);
            factor->set_b(mix->second);
          }
        }
        MixNode *product = graph->create_node<MixNode>();
        product->name = node.name + ".product";
        product->set_mix_type(NODE_MIX_MUL);
        product->set_fac(1.0f);
        if (const auto mix = node.color3_inputs.find("mix");
            mix != node.color3_inputs.end())
        {
          product->set_color2(mix->second);
        }
        MixNode *sum = graph->create_node<MixNode>();
        sum->set_mix_type(NODE_MIX_ADD);
        sum->set_fac(1.0f);
        if (const auto background = node.color3_inputs.find("bg"); background != node.color3_inputs.end()) sum->set_color1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        if (factor) {
          lowered_nodes.emplace(factor->name, factor);
        }
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
      else {
        VectorMathNode *delta = graph->create_node<VectorMathNode>();
        delta->name = node.name + ".delta";
        delta->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
        if (const auto foreground = node.vector3_inputs.find("fg"); foreground != node.vector3_inputs.end()) delta->set_vector1(foreground->second);
        if (const auto background = node.vector3_inputs.find("bg"); background != node.vector3_inputs.end()) delta->set_vector2(background->second);
        CombineXYZNode *factor = graph->create_node<CombineXYZNode>();
        factor->name = node.name + ".factor";
        if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
          factor->set_x(mix->second); factor->set_y(mix->second); factor->set_z(mix->second);
        }
        VectorMathNode *product = graph->create_node<VectorMathNode>();
        product->name = node.name + ".product";
        product->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
        VectorMathNode *sum = graph->create_node<VectorMathNode>();
        sum->set_math_type(NODE_VECTOR_MATH_ADD);
        if (const auto background = node.vector3_inputs.find("bg"); background != node.vector3_inputs.end()) sum->set_vector1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(factor->name, factor);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
    }
    else if (is_safepower_float(node.nodedef)) {
      for (const auto &[suffix, type] : {std::pair{"abs", NODE_MATH_ABSOLUTE},
                                        std::pair{"sign", NODE_MATH_SIGN},
                                        std::pair{"power", NODE_MATH_POWER},
                                        std::pair{"multiply", NODE_MATH_MULTIPLY}})
      {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + suffix;
        math->set_math_type(type);
        lowered_nodes.emplace(math->name, math);
      }
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) {
        static_cast<MathNode *>(lowered_nodes.at(node.name + ".abs"))->set_value1(input->second);
        static_cast<MathNode *>(lowered_nodes.at(node.name + ".sign"))->set_value1(input->second);
      }
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
        static_cast<MathNode *>(lowered_nodes.at(node.name + ".power"))->set_value2(input->second);
      }
      lowered = lowered_nodes.at(node.name + ".multiply");
      preserve_lowered_name = true;
    }
    else if (NodeMathType math_type; scalar_math_type(node.nodedef, &math_type)) {
      MathNode *math = graph->create_node<MathNode>();
      math->set_math_type(math_type);
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      const bool is_atan2 = node.nodedef == atan2_float_id;
      if (node.nodedef == ln_float_id) {
        math->set_value2(M_E);
      }
      if (const auto input = node.inputs.find(
              is_unary ? "in" : (node.nodedef == invert_float_id ? "amount" : (is_atan2 ? "iny" : "in1")));
          input != node.inputs.end()) {
        math->set_value1(input->second);
      }
      if (!is_unary) {
        if (const auto input = node.inputs.find(node.nodedef == invert_float_id ? "in" : (is_atan2 ? "inx" : "in2"));
            input != node.inputs.end())
        {
          math->set_value2(input->second);
        }
      }
      lowered = math;
    }
    else if (is_float_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>();
      condition->name = node.name + ".condition";
      MathNode *greater = nullptr;
      MathNode *equal = nullptr;
      if (node.nodedef == ifgreater_float_id) {
        condition->set_math_type(NODE_MATH_GREATER_THAN);
      }
      else if (node.nodedef == ifequal_float_id) {
        condition->set_math_type(NODE_MATH_COMPARE);
        condition->set_value3(0.0f);
      }
      else {
        greater = graph->create_node<MathNode>();
        greater->name = node.name + ".greater";
        greater->set_math_type(NODE_MATH_GREATER_THAN);
        equal = graph->create_node<MathNode>();
        equal->name = node.name + ".equal";
        equal->set_math_type(NODE_MATH_COMPARE);
        equal->set_value3(0.0f);
        condition->set_math_type(NODE_MATH_MAXIMUM);
        lowered_nodes.emplace(greater->name, greater);
        lowered_nodes.emplace(equal->name, equal);
      }
      MathNode *delta = graph->create_node<MathNode>();
      delta->name = node.name + ".delta";
      delta->set_math_type(NODE_MATH_SUBTRACT);
      MathNode *product = graph->create_node<MathNode>();
      product->name = node.name + ".product";
      product->set_math_type(NODE_MATH_MULTIPLY);
      MathNode *sum = graph->create_node<MathNode>();
      sum->set_math_type(NODE_MATH_ADD);
      for (const auto &[input_name, value] : node.inputs) {
        if (input_name == "value1") {
          if (node.nodedef == ifgreatereq_float_id) {
            greater->set_value1(value);
            equal->set_value1(value);
          }
          else {
            condition->set_value1(value);
          }
        }
        else if (input_name == "value2") {
          if (node.nodedef == ifgreatereq_float_id) {
            greater->set_value2(value);
            equal->set_value2(value);
          }
          else {
            condition->set_value2(value);
          }
        }
        else if (input_name == "in1") {
          delta->set_value1(value);
        }
        else if (input_name == "in2") {
          delta->set_value2(value);
          sum->set_value1(value);
        }
      }
      lowered_nodes.emplace(condition->name, condition);
      lowered_nodes.emplace(delta->name, delta);
      lowered_nodes.emplace(product->name, product);
      lowered = sum;
    }
    else if (node.nodedef == convert_boolean_float_id || node.nodedef == convert_integer_float_id) {
      /* MaterialX stdlib's boolean/integer-to-float converts are exact
       * numeric adapters (false/true -> 0/1, integer -> same numeric value).
       * The boolean/integer source lowerers already expose that authenticated
       * numeric value through their sidecar float output; this MathNode adds
       * zero so the conversion has a real float-typed Cycles node without
       * changing the value. */
      MathNode *math = graph->create_node<MathNode>();
      math->set_math_type(NODE_MATH_ADD);
      math->set_value2(0.0f);
      lowered = math;
    }
    else if (node.nodedef == convert_float_color3_id || node.nodedef == convert_boolean_color3_id ||
             node.nodedef == convert_integer_color3_id)
    {
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = combine;
    }
    else if (node.nodedef == convert_color3_vector3_id || node.nodedef == convert_color3_vector2_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (node.nodedef == convert_color3_vector2_id) combine->set_z(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id ||
             node.nodedef == convert_vector4_color3_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      if (node.nodedef == convert_vector2_color3_id) combine->set_b(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (node.nodedef == convert_color3_color4_id) {
      ValueNode *alpha = graph->create_node<ValueNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_value(1.0f);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = lowered_nodes.at(node.links.at("in").source_node);
      preserve_lowered_name = true;
    }
    else if (node.nodedef == convert_float_vector3_id || node.nodedef == convert_float_vector2_id ||
             node.nodedef == convert_boolean_vector3_id || node.nodedef == convert_boolean_vector2_id ||
             node.nodedef == convert_integer_vector3_id || node.nodedef == convert_integer_vector2_id)
    {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (node.nodedef == convert_float_vector2_id || node.nodedef == convert_boolean_vector2_id ||
          node.nodedef == convert_integer_vector2_id)
      {
        combine->set_z(0.0f);
      }
      lowered = combine;
    }
    else if (node.nodedef == convert_vector2_vector3_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>(); separate->name = node.name + ".separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>(); combine->set_z(0.0f);
      lowered_nodes.emplace(separate->name, separate); lowered = combine;
    }
    else if (is_color4_operation(node.nodedef)) {
      NodeMathType math_type = NODE_MATH_ADD;
      if (!color4_unary_math_type(node.nodedef, &math_type)) {
        color4_binary_math_type(node.nodedef, &math_type);
      }
      const bool scalar_invert = node.nodedef == invert_color4fa_id;
      const bool invert = is_color4_invert(node.nodedef);
      const bool safepower = is_safepower_color4(node.nodedef);
      const bool unary = color4_unary_math_type(node.nodedef, nullptr);
      const bool scalar_second = color4_binary_uses_scalar_second(node.nodedef);
      const bool clamp = node.nodedef == clamp_color4_id || node.nodedef == clamp_color4fa_id;
      SeparateColorNode *first = graph->create_node<SeparateColorNode>();
      first->name = node.name +
                    ((unary || scalar_invert || clamp) ?
                         ".input" :
                         (invert ? ".amount" : ".first"));
      first->set_color_type(NODE_COMBSEP_COLOR_RGB);
      SeparateColorNode *second = nullptr;
      if (!unary && !scalar_second && !clamp) {
        second = graph->create_node<SeparateColorNode>();
        second->name = node.name + (invert ? ".input" : ".second");
        second->set_color_type(NODE_COMBSEP_COLOR_RGB);
      }
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(first->name, first);
      if (second) {
        lowered_nodes.emplace(second->name, second);
      }
      if (node.nodedef == clamp_color4_id) {
        for (const char *bound : {"low", "high"}) {
          if (node.links.contains(bound)) {
            SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
            separate->name = node.name + "." + bound;
            separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
            lowered_nodes.emplace(separate->name, separate);
          }
        }
      }
      for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
        if (clamp) {
          MathNode *minimum = graph->create_node<MathNode>();
          minimum->name = node.name + "." + channel + ".minimum";
          minimum->set_math_type(NODE_MATH_MINIMUM);
          MathNode *maximum = graph->create_node<MathNode>();
          maximum->name = node.name + "." + channel + ".maximum";
          maximum->set_math_type(NODE_MATH_MAXIMUM);
          lowered_nodes.emplace(minimum->name, minimum);
          lowered_nodes.emplace(maximum->name, maximum);
        }
        else if (safepower) {
          const std::pair<const char *, NodeMathType> stages[] = {
              {"abs", NODE_MATH_ABSOLUTE},
              {"sign", NODE_MATH_SIGN},
              {"power", NODE_MATH_POWER},
              {"multiply", NODE_MATH_MULTIPLY}};
          for (const auto &[suffix, type] : stages) {
            MathNode *math = graph->create_node<MathNode>();
            math->name = node.name + "." + channel + "." + suffix;
            math->set_math_type(type);
            lowered_nodes.emplace(math->name, math);
          }
        }
        else {
          MathNode *math = graph->create_node<MathNode>();
          math->name = node.name + "." + channel;
          math->set_math_type(invert ? NODE_MATH_SUBTRACT : math_type);
          lowered_nodes.emplace(math->name, math);
        }
      }
      lowered = combine;
    }
    else if (node.nodedef == combine3_color3_id) {
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      for (const auto &[input, channel] : {std::pair{"in1", 0}, std::pair{"in2", 1}, std::pair{"in3", 2}}) {
        if (const auto value = node.inputs.find(input); value != node.inputs.end()) {
          if (channel == 0) combine->set_r(value->second);
          else if (channel == 1) combine->set_g(value->second);
          else combine->set_b(value->second);
        }
      }
      lowered = combine;
    }
    else if (node.nodedef == separate3_color3_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = separate;
    }
    else if (node.nodedef == separate4_color4_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = separate;
    }
    else if (NodeMathType math_type; color_unary_math_type(node.nodedef, &math_type)) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate"; separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(separate->name, separate);
      for (const char *channel : {"Red", "Green", "Blue"}) {
        MathNode *math = graph->create_node<MathNode>(); math->name = node.name + "." + channel; math->set_math_type(math_type);
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (is_color_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>(); condition->name = node.name + ".condition";
      if (node.nodedef == ifgreater_color3_id) condition->set_math_type(NODE_MATH_GREATER_THAN);
      else if (node.nodedef == ifequal_color3_id) { condition->set_math_type(NODE_MATH_COMPARE); condition->set_value3(0.0f); }
      else {
        MathNode *greater = graph->create_node<MathNode>(); greater->name = node.name + ".greater"; greater->set_math_type(NODE_MATH_GREATER_THAN);
        MathNode *equal = graph->create_node<MathNode>(); equal->name = node.name + ".equal"; equal->set_math_type(NODE_MATH_COMPARE); equal->set_value3(0.0f);
        if (const auto value = node.inputs.find("value1"); value != node.inputs.end()) { greater->set_value1(value->second); equal->set_value1(value->second); }
        if (const auto value = node.inputs.find("value2"); value != node.inputs.end()) { greater->set_value2(value->second); equal->set_value2(value->second); }
        condition->set_math_type(NODE_MATH_MAXIMUM); lowered_nodes.emplace(greater->name, greater); lowered_nodes.emplace(equal->name, equal);
      }
      if (node.nodedef != ifgreatereq_color3_id) {
        if (const auto value = node.inputs.find("value1"); value != node.inputs.end()) condition->set_value1(value->second);
        if (const auto value = node.inputs.find("value2"); value != node.inputs.end()) condition->set_value2(value->second);
      }
      MixNode *mix = graph->create_node<MixNode>(); mix->set_mix_type(NODE_MIX_BLEND);
      if (const auto value = node.color3_inputs.find("in2"); value != node.color3_inputs.end()) mix->set_color1(value->second);
      if (const auto value = node.color3_inputs.find("in1"); value != node.color3_inputs.end()) mix->set_color2(value->second);
      lowered_nodes.emplace(condition->name, condition); lowered = mix;
    }
    else if (is_vector_conditional(node.nodedef)) {
      MathNode *condition = graph->create_node<MathNode>(); condition->name = node.name + ".condition";
      if (node.nodedef == ifgreater_vector3_id) condition->set_math_type(NODE_MATH_GREATER_THAN);
      else if (node.nodedef == ifequal_vector3_id) { condition->set_math_type(NODE_MATH_COMPARE); condition->set_value3(0.0f); }
      else {
        MathNode *greater = graph->create_node<MathNode>(); greater->name = node.name + ".greater"; greater->set_math_type(NODE_MATH_GREATER_THAN);
        MathNode *equal = graph->create_node<MathNode>(); equal->name = node.name + ".equal"; equal->set_math_type(NODE_MATH_COMPARE); equal->set_value3(0.0f);
        if (const auto v=node.inputs.find("value1"); v!=node.inputs.end()) { greater->set_value1(v->second); equal->set_value1(v->second); }
        if (const auto v=node.inputs.find("value2"); v!=node.inputs.end()) { greater->set_value2(v->second); equal->set_value2(v->second); }
        condition->set_math_type(NODE_MATH_MAXIMUM); lowered_nodes.emplace(greater->name, greater); lowered_nodes.emplace(equal->name, equal);
      }
      if (node.nodedef != ifgreatereq_vector3_id) { if (const auto v=node.inputs.find("value1"); v!=node.inputs.end()) condition->set_value1(v->second); if (const auto v=node.inputs.find("value2"); v!=node.inputs.end()) condition->set_value2(v->second); }
      MixVectorNode *mix = graph->create_node<MixVectorNode>();
      if (const auto v=node.vector3_inputs.find("in2"); v!=node.vector3_inputs.end()) mix->set_a(v->second);
      if (const auto v=node.vector3_inputs.find("in1"); v!=node.vector3_inputs.end()) mix->set_b(v->second);
      lowered_nodes.emplace(condition->name, condition); lowered = mix;
    }
    else if (NodeMathType math_type; color_binary_component_math_type(node.nodedef, &math_type)) {
      const bool scalar_second = color_binary_component_math_uses_scalar_second(node.nodedef);
      SeparateColorNode *first = graph->create_node<SeparateColorNode>(); first->name = node.name + ".first"; first->set_color_type(NODE_COMBSEP_COLOR_RGB);
      SeparateColorNode *second = scalar_second ? nullptr : graph->create_node<SeparateColorNode>(); if (second) { second->name = node.name + ".second"; second->set_color_type(NODE_COMBSEP_COLOR_RGB); }
      CombineColorNode *combine = graph->create_node<CombineColorNode>(); combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(first->name, first); if (second) lowered_nodes.emplace(second->name, second);
      for (const char *channel : {"Red", "Green", "Blue"}) { MathNode *math = graph->create_node<MathNode>(); math->name = node.name + "." + channel; math->set_math_type(math_type); lowered_nodes.emplace(math->name, math); }
      lowered = combine;
    }
    else if (node.nodedef == clamp_color3_id || node.nodedef == clamp_color3fa_id) {
      const bool scalar_bounds = node.nodedef == clamp_color3fa_id;
      const float3 low = scalar_bounds ? make_float3(node.inputs.at("low")) : node.color3_inputs.at("low");
      const float3 high = scalar_bounds ? make_float3(node.inputs.at("high")) : node.color3_inputs.at("high");
      SeparateColorNode *input = graph->create_node<SeparateColorNode>(); input->name = node.name + ".input"; input->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>(); combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(input->name, input);
      for (const char *channel : {"Red", "Green", "Blue"}) {
        const float lo = channel[0] == 'R' ? low.x : channel[0] == 'G' ? low.y : low.z;
        const float hi = channel[0] == 'R' ? high.x : channel[0] == 'G' ? high.y : high.z;
        MathNode *minimum = graph->create_node<MathNode>(); minimum->name = node.name + "." + channel + ".minimum"; minimum->set_math_type(NODE_MATH_MINIMUM); minimum->set_value2(hi);
        MathNode *maximum = graph->create_node<MathNode>(); maximum->name = node.name + "." + channel + ".maximum"; maximum->set_math_type(NODE_MATH_MAXIMUM); maximum->set_value2(lo);
        lowered_nodes.emplace(minimum->name, minimum); lowered_nodes.emplace(maximum->name, maximum);
      }
      lowered = combine;
    }
    else if (is_safepower_color3(node.nodedef)) {
      const bool scalar_exponent = safepower_color3_uses_scalar_exponent(node.nodedef);
      SeparateColorNode *first = graph->create_node<SeparateColorNode>(); first->name = node.name + ".first"; first->set_color_type(NODE_COMBSEP_COLOR_RGB);
      SeparateColorNode *second = scalar_exponent ? nullptr : graph->create_node<SeparateColorNode>(); if (second) { second->name = node.name + ".second"; second->set_color_type(NODE_COMBSEP_COLOR_RGB); }
      CombineColorNode *combine = graph->create_node<CombineColorNode>(); combine->set_color_type(NODE_COMBSEP_COLOR_RGB); lowered_nodes.emplace(first->name, first); if (second) lowered_nodes.emplace(second->name, second);
      for (const char *c : {"Red","Green","Blue"}) {
        for (const auto &[suffix,type] : {std::pair{"abs",NODE_MATH_ABSOLUTE},std::pair{"sign",NODE_MATH_SIGN},std::pair{"power",NODE_MATH_POWER},std::pair{"multiply",NODE_MATH_MULTIPLY}}) {
          MathNode *m=graph->create_node<MathNode>(); m->name=node.name+"."+c+"."+suffix; m->set_math_type(type); lowered_nodes.emplace(m->name,m);
        }
        const auto component = [c](const float3 &v) { return c[0] == 'R' ? v.x : c[0] == 'G' ? v.y : v.z; };
        if (const auto value=node.color3_inputs.find("in1"); value!=node.color3_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name+"."+c+".abs"))->set_value1(component(value->second));
          static_cast<MathNode *>(lowered_nodes.at(node.name+"."+c+".sign"))->set_value1(component(value->second));
        }
        if (scalar_exponent) {
          if (const auto value=node.inputs.find("in2"); value!=node.inputs.end()) static_cast<MathNode *>(lowered_nodes.at(node.name+"."+c+".power"))->set_value2(value->second);
        }
        else if (const auto value=node.color3_inputs.find("in2"); value!=node.color3_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name+"."+c+".power"))->set_value2(component(value->second));
        }
      }
      lowered=combine;
    }
    else if (node.nodedef == rgbtohsv_color3_id || node.nodedef == hsvtorgb_color3_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(node.nodedef == rgbtohsv_color3_id ? NODE_COMBSEP_COLOR_HSV :
                                                                  NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(node.nodedef == rgbtohsv_color3_id ? NODE_COMBSEP_COLOR_RGB :
                                                                  NODE_COMBSEP_COLOR_HSV);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (is_smoothstep_float(node.nodedef)) {
      MapRangeNode *range = graph->create_node<MapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_SMOOTHSTEP);
      range->set_clamp(false);
      range->set_from_min(node.inputs.at("low"));
      range->set_from_max(node.inputs.at("high"));
      range->set_to_min(0.0f);
      range->set_to_max(1.0f);
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        range->set_value(input->second);
      }
      lowered = range;
    }
    else if (is_linear_range_float(node.nodedef)) {
      MapRangeNode *range = graph->create_node<MapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_LINEAR);
      range->set_clamp(node.nodedef == range_float_id && node.int_inputs.at("doclamp") != 0);
      range->set_from_min(node.inputs.at("inlow"));
      range->set_from_max(node.inputs.at("inhigh"));
      range->set_to_min(node.inputs.at("outlow"));
      range->set_to_max(node.inputs.at("outhigh"));
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        range->set_value(input->second);
      }
      lowered = range;
    }
    else if (is_linear_range_color3(node.nodedef)) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(separate->name, separate);
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const float3 inlow = scalar_bounds ? make_float3(node.inputs.at("inlow")) :
                                           node.color3_inputs.at("inlow");
      const float3 inhigh = scalar_bounds ? make_float3(node.inputs.at("inhigh")) :
                                            node.color3_inputs.at("inhigh");
      const float3 outlow = scalar_bounds ? make_float3(node.inputs.at("outlow")) :
                                            node.color3_inputs.at("outlow");
      const float3 outhigh = scalar_bounds ? make_float3(node.inputs.at("outhigh")) :
                                             node.color3_inputs.at("outhigh");
      const float3 input = node.color3_inputs.count("in") ? node.color3_inputs.at("in") : zero_float3();
      for (const auto &[channel, from_min, from_max, to_min, to_max, value] :
           {std::tuple{"Red", inlow.x, inhigh.x, outlow.x, outhigh.x, input.x},
            std::tuple{"Green", inlow.y, inhigh.y, outlow.y, outhigh.y, input.y},
            std::tuple{"Blue", inlow.z, inhigh.z, outlow.z, outhigh.z, input.z}})
      {
        MapRangeNode *range = graph->create_node<MapRangeNode>();
        range->name = node.name + "." + channel;
        range->set_range_type(NODE_MAP_RANGE_LINEAR);
        range->set_clamp((node.nodedef == range_color3_id || node.nodedef == range_color3fa_id) &&
                         node.int_inputs.at("doclamp") != 0);
        range->set_from_min(from_min);
        range->set_from_max(from_max);
        range->set_to_min(to_min);
        range->set_to_max(to_max);
        range->set_value(value);
        lowered_nodes.emplace(range->name, range);
      }
      lowered = combine;
    }
    else if (is_linear_range_vector2(node.nodedef)) {
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const float2 inlow = scalar_bounds ? make_float2(node.inputs.at("inlow")) :
                                           node.vector2_inputs.at("inlow");
      const float2 inhigh = scalar_bounds ? make_float2(node.inputs.at("inhigh")) :
                                            node.vector2_inputs.at("inhigh");
      const float2 outlow = scalar_bounds ? make_float2(node.inputs.at("outlow")) :
                                            node.vector2_inputs.at("outlow");
      const float2 outhigh = scalar_bounds ? make_float2(node.inputs.at("outhigh")) :
                                             node.vector2_inputs.at("outhigh");
      VectorMapRangeNode *range = graph->create_node<VectorMapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_LINEAR);
      range->set_use_clamp(node.nodedef == range_vector2_id &&
                           node.int_inputs.at("doclamp") != 0);
      range->set_from_min(make_float3(inlow.x, inlow.y, 0.0f));
      range->set_from_max(make_float3(inhigh.x, inhigh.y, 1.0f));
      range->set_to_min(make_float3(outlow.x, outlow.y, 0.0f));
      range->set_to_max(make_float3(outhigh.x, outhigh.y, 1.0f));
      if (const auto input = node.vector2_inputs.find("in"); input != node.vector2_inputs.end()) {
        range->set_vector(make_float3(input->second.x, input->second.y, 0.0f));
      }
      lowered = range;
    }
    else if (is_linear_range_vector3(node.nodedef)) {
      const bool scalar_bounds = is_linear_range_scalar_bounds(node.nodedef);
      const float3 inlow = scalar_bounds ? make_float3(node.inputs.at("inlow")) :
                                           node.vector3_inputs.at("inlow");
      const float3 inhigh = scalar_bounds ? make_float3(node.inputs.at("inhigh")) :
                                            node.vector3_inputs.at("inhigh");
      const float3 outlow = scalar_bounds ? make_float3(node.inputs.at("outlow")) :
                                            node.vector3_inputs.at("outlow");
      const float3 outhigh = scalar_bounds ? make_float3(node.inputs.at("outhigh")) :
                                             node.vector3_inputs.at("outhigh");
      VectorMapRangeNode *range = graph->create_node<VectorMapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_LINEAR);
      range->set_use_clamp((node.nodedef == range_vector3_id || node.nodedef == range_vector3fa_id) &&
                           node.int_inputs.at("doclamp") != 0);
      range->set_from_min(inlow);
      range->set_from_max(inhigh);
      range->set_to_min(outlow);
      range->set_to_max(outhigh);
      if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
        range->set_vector(input->second);
      }
      lowered = range;
    }
    else if (node.nodedef == clamp_vector2_id) {
      VectorMathNode *minimum = graph->create_node<VectorMathNode>();
      minimum->name = node.name + ".minimum";
      minimum->set_math_type(NODE_VECTOR_MATH_MINIMUM);
      minimum->set_vector2(make_float3(node.vector2_inputs.at("high").x,
                                       node.vector2_inputs.at("high").y,
                                       0.0f));
      if (const auto input = node.vector2_inputs.find("in"); input != node.vector2_inputs.end()) {
        minimum->set_vector1(make_float3(input->second.x, input->second.y, 0.0f));
      }
      VectorMathNode *maximum = graph->create_node<VectorMathNode>();
      maximum->set_math_type(NODE_VECTOR_MATH_MAXIMUM);
      maximum->set_vector2(make_float3(node.vector2_inputs.at("low").x,
                                       node.vector2_inputs.at("low").y,
                                       0.0f));
      lowered_nodes.emplace(minimum->name, minimum);
      lowered = maximum;
    }
    else if (node.nodedef == clamp_vector2fa_id) {
      VectorMathNode *minimum = graph->create_node<VectorMathNode>();
      minimum->name = node.name + ".minimum";
      minimum->set_math_type(NODE_VECTOR_MATH_MINIMUM);
      minimum->set_vector2(make_float3(node.inputs.at("high")));
      if (const auto input = node.vector2_inputs.find("in"); input != node.vector2_inputs.end()) {
        minimum->set_vector1(make_float3(input->second.x, input->second.y, 0.0f));
      }
      VectorMathNode *maximum = graph->create_node<VectorMathNode>();
      maximum->set_math_type(NODE_VECTOR_MATH_MAXIMUM);
      maximum->set_vector2(make_float3(node.inputs.at("low")));
      lowered_nodes.emplace(minimum->name, minimum);
      lowered = maximum;
    }
    else if (node.nodedef == clamp_vector3_id || node.nodedef == clamp_vector3fa_id) {
      const bool scalar_bounds = node.nodedef == clamp_vector3fa_id;
      const float3 low = scalar_bounds ? make_float3(node.inputs.at("low")) :
                                         node.vector3_inputs.at("low");
      const float3 high = scalar_bounds ? make_float3(node.inputs.at("high")) :
                                          node.vector3_inputs.at("high");
      VectorMathNode *minimum = graph->create_node<VectorMathNode>();
      minimum->name = node.name + ".minimum";
      minimum->set_math_type(NODE_VECTOR_MATH_MINIMUM);
      minimum->set_vector2(high);
      if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
        minimum->set_vector1(input->second);
      }
      VectorMathNode *maximum = graph->create_node<VectorMathNode>();
      maximum->set_math_type(NODE_VECTOR_MATH_MAXIMUM);
      maximum->set_vector2(low);
      lowered_nodes.emplace(minimum->name, minimum);
      lowered = maximum;
    }
    else if (is_native_noise_or_fractal_family(node.nodedef)) {
      const bool vector2 = native_noise_or_fractal_is_vector2(node.nodedef);
      const bool is_float = native_noise_or_fractal_is_float(node.nodedef);
      const bool is_color3 = native_noise_or_fractal_is_color3(node.nodedef);
      const bool is_color4 = native_noise_or_fractal_is_color4(node.nodedef);
      const bool scalar_amplitude = native_noise_or_fractal_uses_scalar_amplitude(node.nodedef);
      NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
      noise->name = node.name + ".noise";
      noise->set_dimensions(native_noise_or_fractal_is_3d(node.nodedef) ? 3 : 2);
      if (is_native_fractal2d_family(node.nodedef) || is_native_fractal3d_family(node.nodedef)) {
        noise->set_type(NODE_NOISE_FBM);
        noise->set_detail(float(node.int_inputs.at("octaves")));
        noise->set_lacunarity(node.inputs.at("lacunarity"));
        noise->set_roughness(node.inputs.at("diminish"));
      }
      if (is_float) {
        MathNode *amplitude = graph->create_node<MathNode>();
        amplitude->name = node.name + ".amplitude";
        amplitude->set_math_type(NODE_MATH_MULTIPLY);
        amplitude->set_value2(node.inputs.at("amplitude"));
        lowered_nodes.emplace(noise->name, noise);
        lowered_nodes.emplace(amplitude->name, amplitude);
        if (is_native_fractal2d_family(node.nodedef) || is_native_fractal3d_family(node.nodedef)) {
          lowered = amplitude;
        }
        else {
          MathNode *pivot = graph->create_node<MathNode>();
          pivot->set_math_type(NODE_MATH_ADD);
          pivot->set_value2(node.inputs.at("pivot"));
          lowered = pivot;
        }
      }
      else if (is_color3) {
        MixNode *multiply = graph->create_node<MixNode>();
        multiply->name = node.name + ".amplitude";
        multiply->set_mix_type(NODE_MIX_MUL);
        multiply->set_fac(1.0f);
        if (scalar_amplitude) {
          multiply->set_color2(make_float3(node.inputs.at("amplitude")));
        }
        else {
          multiply->set_color2(node.vector3_inputs.at("amplitude"));
        }
        lowered_nodes.emplace(noise->name, noise);
        lowered_nodes.emplace(multiply->name, multiply);
        if (is_native_fractal2d_family(node.nodedef) || is_native_fractal3d_family(node.nodedef)) {
          lowered = multiply;
        }
        else {
          MixNode *pivot = graph->create_node<MixNode>();
          pivot->set_mix_type(NODE_MIX_ADD);
          pivot->set_fac(1.0f);
          pivot->set_color2(make_float3(node.inputs.at("pivot")));
          lowered = pivot;
        }
      }
      else {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->name = node.name + ".separate";
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        ShaderNode *combine = is_color4 ? static_cast<ShaderNode *>(graph->create_node<CombineColorNode>()) :
                                          static_cast<ShaderNode *>(graph->create_node<CombineXYZNode>());
        if (is_color4) {
          static_cast<CombineColorNode *>(combine)->set_color_type(NODE_COMBSEP_COLOR_RGB);
        }
        const bool is_vector4 = native_noise_or_fractal_output_type(node.nodedef) == Type::Vector4;
        if (is_color4 || is_vector4) {
          SeparateXYZNode *offset = graph->create_node<SeparateXYZNode>();
          offset->name = node.name + ".offset.separate";
          NoiseTextureNode *fourth_noise = graph->create_node<NoiseTextureNode>();
          fourth_noise->name = node.name + ".W.noise";
          fourth_noise->set_dimensions(native_noise_or_fractal_is_3d(node.nodedef) ? 3 : 2);
          if (is_native_fractal2d_family(node.nodedef) || is_native_fractal3d_family(node.nodedef)) {
            fourth_noise->set_type(NODE_NOISE_FBM);
            fourth_noise->set_detail(float(node.int_inputs.at("octaves")));
            fourth_noise->set_lacunarity(node.inputs.at("lacunarity"));
            fourth_noise->set_roughness(node.inputs.at("diminish"));
          }
          MathNode *w_amplitude = graph->create_node<MathNode>();
          w_amplitude->name = node.name + ".W.amplitude";
          w_amplitude->set_math_type(NODE_MATH_MULTIPLY);
          w_amplitude->set_value2(scalar_amplitude ? node.inputs.at("amplitude") :
                                                     node.vector4_inputs.at("amplitude").w);
          lowered_nodes.emplace(offset->name, offset);
          lowered_nodes.emplace(fourth_noise->name, fourth_noise);
          lowered_nodes.emplace(w_amplitude->name, w_amplitude);
          if (is_native_fractal2d_family(node.nodedef) || is_native_fractal3d_family(node.nodedef)) {
            lowered_nodes.emplace(node.name + (is_color4 ? ".Alpha" : ".W"), w_amplitude);
          }
          else {
            MathNode *w_pivot = graph->create_node<MathNode>();
            w_pivot->name = node.name + (is_color4 ? ".Alpha" : ".W");
            w_pivot->set_math_type(NODE_MATH_ADD);
            w_pivot->set_value2(node.inputs.at("pivot"));
            lowered_nodes.emplace(w_pivot->name, w_pivot);
          }
        }
        if (!is_color4) {
          static_cast<CombineXYZNode *>(combine)->set_z(0.0f);
        }
        for (const char *channel : {"X", "Y", "Z"}) {
          if (vector2 && channel[0] == 'Z') {
            continue;
          }
          MathNode *amplitude = graph->create_node<MathNode>();
          amplitude->name = node.name + "." + channel + ".amplitude";
          amplitude->set_math_type(NODE_MATH_MULTIPLY);
          float amplitude_value = scalar_amplitude ? node.inputs.at("amplitude") : 0.0f;
          if (!scalar_amplitude) {
            if (vector2) {
              amplitude_value = channel[0] == 'X' ? node.vector2_inputs.at("amplitude").x :
                                                    node.vector2_inputs.at("amplitude").y;
            }
            else if (is_color4 || is_vector4) {
              const float4 value = node.vector4_inputs.at("amplitude");
              amplitude_value = channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z;
            }
            else {
              const float3 value = node.vector3_inputs.at("amplitude");
              amplitude_value = channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z;
            }
          }
          amplitude->set_value2(amplitude_value);
          lowered_nodes.emplace(amplitude->name, amplitude);
          if (!is_native_fractal2d_family(node.nodedef) && !is_native_fractal3d_family(node.nodedef)) {
            MathNode *pivot = graph->create_node<MathNode>();
            pivot->name = node.name + "." + channel;
            pivot->set_math_type(NODE_MATH_ADD);
            pivot->set_value2(node.inputs.at("pivot"));
            lowered_nodes.emplace(pivot->name, pivot);
          }
        }
        lowered_nodes.emplace(noise->name, noise);
        lowered_nodes.emplace(separate->name, separate);
        lowered = combine;
      }
    }
    else if (const CellNoiseSpec *spec = cellnoise_spec(node.nodedef)) {
      VectorMathNode *floor = graph->create_node<VectorMathNode>();
      floor->name = node.name + ".floor";
      floor->set_math_type(NODE_VECTOR_MATH_FLOOR);
      WhiteNoiseTextureNode *noise = graph->create_node<WhiteNoiseTextureNode>();
      noise->set_dimensions(spec->dimensions);
      lowered_nodes.emplace(floor->name, floor);
      lowered = noise;
    }
    else if (node.nodedef == checkerboard_color3_id) {
      CheckerTextureNode *checker = graph->create_node<CheckerTextureNode>();
      checker->set_color1(node.color3_inputs.at("color1"));
      checker->set_color2(node.color3_inputs.at("color2"));
      checker->set_scale(node.vector2_inputs.at("uvtiling").x);
      lowered = checker;
    }
    else if (is_luminance_color3(node.nodedef)) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->name = node.name + ".vector";
      VectorMathNode *dot = graph->create_node<VectorMathNode>();
      dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
      dot->set_vector2(node.color3_inputs.at("lumacoeffs"));
      lowered_nodes.emplace(separate->name, separate);
      lowered_nodes.emplace(combine->name, combine);
      lowered = dot;
    }
    else if (node.nodedef == clamp_float_id) {
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        clamp->set_value(input->second);
      }
      if (const auto input = node.inputs.find("low"); input != node.inputs.end()) {
        clamp->set_min(input->second);
      }
      if (const auto input = node.inputs.find("high"); input != node.inputs.end()) {
        clamp->set_max(input->second);
      }
      lowered = clamp;
    }
    else if (Type dot_type; value_dot_type(node.nodedef, &dot_type)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        lowered = lowered_nodes.at(input->second.source_node);
        preserve_lowered_name = true;
      }
      else if (dot_type == Type::Float) {
        ValueNode *value = graph->create_node<ValueNode>();
        value->set_value(node.inputs.at("in"));
        lowered = value;
      }
      else if (dot_type == Type::Color3) {
        ColorNode *color = graph->create_node<ColorNode>();
        color->set_value(node.color3_inputs.at("in"));
        lowered = color;
      }
      else if (dot_type == Type::Color4) {
        const float4 value = node.float4_inputs.at("in");
        CombineColorNode *color = graph->create_node<CombineColorNode>();
        color->set_color_type(NODE_COMBSEP_COLOR_RGB);
        color->set_r(value.x);
        color->set_g(value.y);
        color->set_b(value.z);
        ValueNode *alpha = graph->create_node<ValueNode>();
        alpha->name = node.name + ".Alpha";
        alpha->set_value(value.w);
        lowered_nodes.emplace(alpha->name, alpha);
        lowered = color;
      }
      else if (dot_type == Type::Vector2) {
        const float2 value = node.vector2_inputs.at("in");
        CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
        vector->set_x(value.x);
        vector->set_y(value.y);
        vector->set_z(0.0f);
        lowered = vector;
      }
      else if (dot_type == Type::Vector3) {
        const float3 value = node.vector3_inputs.at("in");
        CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
        vector->set_x(value.x);
        vector->set_y(value.y);
        vector->set_z(value.z);
        lowered = vector;
      }
      else if (dot_type == Type::Vector4) {
        const float4 value = node.vector4_inputs.at("in");
        CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
        vector->set_x(value.x);
        vector->set_y(value.y);
        vector->set_z(value.z);
        ValueNode *w = graph->create_node<ValueNode>();
        w->name = node.name + ".W";
        w->set_value(value.w);
        lowered_nodes.emplace(w->name, w);
        lowered = vector;
      }
      else if (dot_type == Type::Boolean) {
        const int value = node.int_inputs.at("in");
        MixNode *boolean = graph->create_node<MixNode>();
        boolean->set_use_clamp(value != 0);
        ValueNode *as_float = graph->create_node<ValueNode>();
        as_float->name = node.name + ".float";
        as_float->set_value(value != 0 ? 1.0f : 0.0f);
        lowered_nodes.emplace(as_float->name, as_float);
        lowered = boolean;
      }
      else if (dot_type == Type::Integer) {
        const int value = node.int_inputs.at("in");
        MagicTextureNode *integer = graph->create_node<MagicTextureNode>();
        integer->set_depth(value);
        ValueNode *as_float = graph->create_node<ValueNode>();
        as_float->name = node.name + ".float";
        as_float->set_value(float(value));
        lowered_nodes.emplace(as_float->name, as_float);
        lowered = integer;
      }
      else if (dot_type == Type::Matrix33) {
        const std::array<float, 9> value = node.matrix33_inputs.at("in");
        Transform transform;
        transform.x = make_float4(value[0], value[1], value[2], 0.0f);
        transform.y = make_float4(value[3], value[4], value[5], 0.0f);
        transform.z = make_float4(value[6], value[7], value[8], 0.0f);
        TextureCoordinateNode *matrix = graph->create_node<TextureCoordinateNode>();
        matrix->set_ob_tfm(transform);
        lowered = matrix;
      }
      else if (dot_type == Type::Matrix44) {
        const std::array<float, 16> value = node.matrix44_inputs.at("in");
        Transform transform;
        transform.x = make_float4(value[0], value[1], value[2], value[3]);
        transform.y = make_float4(value[4], value[5], value[6], value[7]);
        transform.z = make_float4(value[8], value[9], value[10], value[11]);
        TextureCoordinateNode *matrix = graph->create_node<TextureCoordinateNode>();
        matrix->set_ob_tfm(transform);
        lowered = matrix;
      }
    }
    else if (blur_type(node.nodedef, nullptr)) {
      lowered = lowered_nodes.at(node.links.at("in").source_node);
      preserve_lowered_name = true;
    }
    else if (node.nodedef == constant_float_id) {
      ValueNode *value = graph->create_node<ValueNode>();
      value->set_value(node.inputs.at("value"));
      lowered = value;
    }
    else if (node.nodedef == constant_color4_id) {
      const float4 value = node.float4_inputs.contains("value") ? node.float4_inputs.at("value") :
                                                                  zero_float4();
      CombineColorNode *color = graph->create_node<CombineColorNode>();
      color->set_color_type(NODE_COMBSEP_COLOR_RGB);
      color->set_r(value.x);
      color->set_g(value.y);
      color->set_b(value.z);
      ValueNode *alpha = graph->create_node<ValueNode>();
      alpha->name = node.name + ".Alpha";
      alpha->set_value(value.w);
      lowered_nodes.emplace(alpha->name, alpha);
      lowered = color;
    }
    else if (node.nodedef == constant_vector4_id) {
      /* Task 4: Vector4 values are represented internally as Vector3 (via a
       * native CombineXYZNode) plus a parallel W scalar ValueNode -- the
       * same "N components + parallel scalar" device ABI already used for
       * Color4's alpha, so the fourth component is a genuine, distinct,
       * preserved native payload rather than dropped or folded into the
       * three-component node. */
      const float4 value = node.vector4_inputs.contains("value") ?
                               node.vector4_inputs.at("value") :
                               zero_float4();
      CombineXYZNode *vector = graph->create_node<CombineXYZNode>();
      vector->set_x(value.x);
      vector->set_y(value.y);
      vector->set_z(value.z);
      ValueNode *w = graph->create_node<ValueNode>();
      w->name = node.name + ".W";
      w->set_value(value.w);
      lowered_nodes.emplace(w->name, w);
      lowered = vector;
    }
    else if (node.nodedef == geompropvalue_boolean_id || node.nodedef == geompropvalue_integer_id ||
             node.nodedef == geompropvalue_vector4_id || node.nodedef == usd_primvar_reader_boolean_id ||
             node.nodedef == usd_primvar_reader_integer_id || node.nodedef == usd_primvar_reader_vector4_id) {
      AttributeNode *attribute = graph->create_node<AttributeNode>();
      const char *input_name = (node.nodedef == geompropvalue_boolean_id ||
                                node.nodedef == geompropvalue_integer_id ||
                                node.nodedef == geompropvalue_vector4_id) ? "geomprop" : "varname";
      attribute->set_attribute(ustring(node.string_inputs.at(input_name)));
      lowered = attribute;
    }
    else if (node.nodedef == constant_boolean_id) {
      /* Task 5: boolean exact-domain observation. `MixNode::use_clamp` is
       * a genuine native `SocketType::BOOLEAN` field (declared with the
       * `SOCKET_BOOLEAN` macro, stored and read back as `bool`, never
       * routed through a float socket) -- reused here purely as a vehicle
       * to carry the observed boolean value with no float coercion. The
       * rest of the node's sockets are left at their defaults; only
       * `use_clamp` is meaningful for this observation. */
      const int value = node.int_inputs.contains("value") ? node.int_inputs.at("value") : 0;
      MixNode *boolean = graph->create_node<MixNode>();
      boolean->set_use_clamp(value != 0);
      ValueNode *as_float = graph->create_node<ValueNode>();
      as_float->name = node.name + ".float";
      as_float->set_value(value != 0 ? 1.0f : 0.0f);
      lowered_nodes.emplace(as_float->name, as_float);
      lowered = boolean;
    }
    else if (node.nodedef == constant_integer_id) {
      /* Task 5: integer exact-domain observation. `MagicTextureNode::depth`
       * is a genuine native `SocketType::INT` field, stored and read back
       * as `int` with no float coercion -- reused here purely as a vehicle
       * to carry the observed integer value. */
      const int value = node.int_inputs.contains("value") ? node.int_inputs.at("value") : 0;
      MagicTextureNode *integer = graph->create_node<MagicTextureNode>();
      integer->set_depth(value);
      ValueNode *as_float = graph->create_node<ValueNode>();
      as_float->name = node.name + ".float";
      as_float->set_value(float(value));
      lowered_nodes.emplace(as_float->name, as_float);
      lowered = integer;
    }
    else if (node.nodedef == constant_matrix33_id) {
      /* Task 6: matrix boundary. `TextureCoordinateNode::ob_tfm` is a
       * genuine native `SocketType::TRANSFORM` field (Cycles' real affine
       * 4x3 matrix representation) -- reused here purely as a value
       * vehicle, exactly like Task 5's MixNode/MagicTextureNode reuse.
       * Matrix33's 9 linear components map directly into Transform's
       * upper-left 3x3 block; translation (the 4th column) is forced to
       * zero, which is exact (not lossy) because Matrix33 has no
       * translation component to begin with. */
      const std::array<float, 9> value = node.matrix33_inputs.contains("value") ?
                                             node.matrix33_inputs.at("value") :
                                             std::array<float, 9>{};
      Transform transform;
      transform.x = make_float4(value[0], value[1], value[2], 0.0f);
      transform.y = make_float4(value[3], value[4], value[5], 0.0f);
      transform.z = make_float4(value[6], value[7], value[8], 0.0f);
      TextureCoordinateNode *matrix = graph->create_node<TextureCoordinateNode>();
      matrix->set_ob_tfm(transform);
      lowered = matrix;
    }
    else if (node.nodedef == constant_matrix44_id) {
      /* Task 6: matrix boundary, Matrix44 side. Only genuinely affine
       * 4x4 matrices reach `lower()` at all -- validate() rejects any
       * Matrix44 whose last row is not exactly {0, 0, 0, 1} before this
       * point is ever reached, so the 12 components mapped here are a
       * complete, zero-loss encoding of the authenticated value, not a
       * truncation of a general projective matrix. */
      const std::array<float, 16> value = node.matrix44_inputs.contains("value") ?
                                              node.matrix44_inputs.at("value") :
                                              std::array<float, 16>{0, 0, 0, 0, 0, 0, 0, 0,
                                                                     0, 0, 0, 0, 0, 0, 0, 1};
      Transform transform;
      transform.x = make_float4(value[0], value[1], value[2], value[3]);
      transform.y = make_float4(value[4], value[5], value[6], value[7]);
      transform.z = make_float4(value[8], value[9], value[10], value[11]);
      TextureCoordinateNode *matrix = graph->create_node<TextureCoordinateNode>();
      matrix->set_ob_tfm(transform);
      lowered = matrix;
    }
    else if (node.nodedef == constant_color3_id) {
      ColorNode *color = graph->create_node<ColorNode>();
      color->set_value(node.color3_inputs.at("value"));
      lowered = color;
    }
    else if (NodeMix mix_type; color_mix_type(node.nodedef, &mix_type)) {
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(mix_type);
      mix->set_fac(1.0f);
      if (node.nodedef == invert_color3fa_id) {
        CombineColorNode *broadcast = graph->create_node<CombineColorNode>();
        broadcast->name = node.name + ".scalar";
        broadcast->set_color_type(NODE_COMBSEP_COLOR_RGB);
        if (const auto amount=node.inputs.find("amount"); amount!=node.inputs.end()) {
          broadcast->set_r(amount->second); broadcast->set_g(amount->second); broadcast->set_b(amount->second);
        }
        if (const auto input=node.color3_inputs.find("in"); input!=node.color3_inputs.end()) mix->set_color2(input->second);
        lowered_nodes.emplace(broadcast->name, broadcast);
      }
      lowered = mix;
    }
    else if (NodeMix mix_type; color_scalar_mix_type(node.nodedef, &mix_type)) {
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(mix_type);
      mix->set_fac(1.0f);
      CombineColorNode *broadcast = graph->create_node<CombineColorNode>();
      broadcast->name = node.name + ".scalar";
      broadcast->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(broadcast->name, broadcast);
      lowered = mix;
    }
    else if (node.nodedef == extract_color3_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = separate;
    }
    else if (node.nodedef == extract_vector3_id) {
      lowered = graph->create_node<SeparateXYZNode>();
    }
    else if (node.nodedef == separate3_vector3_id) {
      lowered = graph->create_node<SeparateXYZNode>();
    }
    else if (node.nodedef == extract_vector2_id) {
      lowered = graph->create_node<SeparateXYZNode>();
    }
    else if (node.nodedef == geompropvalue_vector2_id) {
      UVMapNode *uv_map = graph->create_node<UVMapNode>();
      uv_map->set_attribute(ustring(node.string_inputs.at("geomprop")));
      lowered = uv_map;
    }
    else if (node.nodedef == geompropvalue_float_id || node.nodedef == geompropvalue_color3_id ||
             node.nodedef == geompropvalue_color4_id) {
      /* AttributeNode natively exposes both "Color" and "Alpha" on the one
       * named attribute (see NODE_DEFINE(AttributeNode), scene/shader_nodes.cpp)
       * -- Color4's alpha channel is resolved straight off this same node by
       * lowered_color4_alpha_output(), no separate literal/CombineColorNode
       * needed. */
      AttributeNode *attribute = graph->create_node<AttributeNode>();
      attribute->set_attribute(ustring(node.string_inputs.at("geomprop")));
      lowered = attribute;
    }
    else if (node.nodedef == geompropvalue_vector3_id) {
      lowered = graph->create_node<GeometryNode>();
    }
    else if (node.nodedef == normal_vector3_id || node.nodedef == position_vector3_id) {
      /* Geometric-source observation (real gap closed). validate() only
       * admits space="world" -- GeometryNode's Position/Normal outputs are
       * always world space (see the declaration comment). */
      lowered = graph->create_node<GeometryNode>();
    }
    else if (node.nodedef == viewdirection_vector3_id) {
      /* Geometric-source observation (real gap closed). validate() only
       * admits space="world" -- GeometryNode's "Incoming" output is the
       * shading-point-space (world) incident ray direction (see the
       * declaration comment for the genosl reference confirming no sign
       * flip is needed). */
      lowered = graph->create_node<GeometryNode>();
    }
    else if (node.nodedef == usdprimvarreader_float_id || node.nodedef == usdprimvarreader_vector2_id ||
             node.nodedef == usdprimvarreader_vector3_id) {
      /* Generic named-primvar read -- the same AttributeNode used for
       * ND_geompropvalue_float/_color3/_color4 above, just addressed by the
       * nodedef's own literal 'varname' rather than 'geomprop'. */
      AttributeNode *attribute = graph->create_node<AttributeNode>();
      attribute->set_attribute(ustring(node.string_inputs.at("varname")));
      lowered = attribute;
    }
    else if (node.nodedef == texcoord_vector3_id) {
      UVMapNode *uv_map = graph->create_node<UVMapNode>();
      uv_map->set_attribute(ustring(node.string_inputs.at("geomprop")));
      lowered = uv_map;
    }
    else if (is_space_transform(node.nodedef)) {
      VectorTransformNode *transform = graph->create_node<VectorTransformNode>();
      transform->set_transform_type(node.nodedef == transformpoint_vector3_id ?
                                        NODE_VECTOR_TRANSFORM_TYPE_POINT :
                                    node.nodedef == transformnormal_vector3_id ?
                                        NODE_VECTOR_TRANSFORM_TYPE_NORMAL :
                                        NODE_VECTOR_TRANSFORM_TYPE_VECTOR);
      transform->set_convert_from(vector_transform_space(node.string_inputs.at("fromspace")));
      transform->set_convert_to(vector_transform_space(node.string_inputs.at("tospace")));
      if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
        transform->set_vector(input->second);
      }
      lowered = transform;
    }
    else if (node.nodedef == image_color3_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      lowered = image;
    }
    else if (node.nodedef == image_color4_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      lowered = image;
    }
    else if (node.nodedef == extract_color4_id) {
      if (node.int_inputs.at("index") == 3) {
        const Node &color4_source = *nodes_by_name.at(node.links.at("in").source_node);
        lowered = color4_source.nodedef == image_color4_id ?
                      lowered_nodes.at(node.links.at("in").source_node) :
                      lowered_nodes.at(node.links.at("in").source_node +
                                       (color4_source.nodedef == clamp_color4_id ||
                                               color4_source.nodedef == clamp_color4fa_id ?
                                            ".Alpha.maximum" :
                                            (is_safepower_color4(color4_source.nodedef) ?
                                                 ".Alpha.multiply" :
                                                 ".Alpha")));
        preserve_lowered_name = true;
      }
      else {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        lowered = separate;
      }
    }
    else if (node.nodedef == convert_color4_color3_id) {
      lowered = lowered_nodes.at(node.links.at("in").source_node);
      preserve_lowered_name = true;
    }
    else if (node.nodedef == image_float_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->name = node.name + ".image";
      image->set_filename(ustring(node.asset_inputs.at("file")));
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered_nodes.emplace(image->name, image);
      lowered = separate;
    }
    else if (node.nodedef == image_vector2_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->name = node.name + ".image";
      image->set_filename(ustring(node.asset_inputs.at("file")));
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(image->name, image);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (node.nodedef == image_vector4_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      image->set_colorspace(u_colorspace_data);
      MathNode *w = graph->create_node<MathNode>();
      w->name = node.name + ".W";
      w->set_math_type(NODE_MATH_ADD);
      w->set_value2(0.0f);
      lowered_nodes.emplace(w->name, w);
      lowered = image;
    }
    else if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id ||
             is_color4_ramp(node.nodedef))
    {
      const bool color4 = is_color4_ramp(node.nodedef);
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(NODE_MIX_BLEND);
      const bool top_to_bottom = node.nodedef == ramptb_color3_id ||
                                 node.nodedef == ramptb_color4_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      if (color4) {
        const float4 first = node.float4_inputs.contains(first_name) ?
                                 node.float4_inputs.at(first_name) :
                                 zero_float4();
        const float4 second = node.float4_inputs.contains(second_name) ?
                                  node.float4_inputs.at(second_name) :
                                  zero_float4();
        mix->set_color1(make_float3(first.x, first.y, first.z));
        mix->set_color2(make_float3(second.x, second.y, second.z));
        MathNode *alpha_delta = graph->create_node<MathNode>();
        alpha_delta->name = node.name + ".Alpha.delta";
        alpha_delta->set_math_type(NODE_MATH_SUBTRACT);
        alpha_delta->set_value1(second.w);
        alpha_delta->set_value2(first.w);
        MathNode *alpha_product = graph->create_node<MathNode>();
        alpha_product->name = node.name + ".Alpha.product";
        alpha_product->set_math_type(NODE_MATH_MULTIPLY);
        MathNode *alpha_sum = graph->create_node<MathNode>();
        alpha_sum->name = node.name + ".Alpha";
        alpha_sum->set_math_type(NODE_MATH_ADD);
        alpha_sum->set_value1(first.w);
        lowered_nodes.emplace(alpha_delta->name, alpha_delta);
        lowered_nodes.emplace(alpha_product->name, alpha_product);
        lowered_nodes.emplace(alpha_sum->name, alpha_sum);
      }
      else {
        mix->set_color1(node.color3_inputs.at(first_name));
        mix->set_color2(node.color3_inputs.at(second_name));
      }
      SeparateXYZNode *coordinate = graph->create_node<SeparateXYZNode>();
      coordinate->name = node.name + ".coordinate";
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->name = node.name + ".factor";
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      clamp->set_min(0.0f);
      clamp->set_max(1.0f);
      lowered_nodes.emplace(coordinate->name, coordinate);
      lowered_nodes.emplace(clamp->name, clamp);
      lowered = mix;
    }
    else if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      SeparateXYZNode *coordinate = graph->create_node<SeparateXYZNode>();
      coordinate->name = node.name + ".coordinate";
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->name = node.name + ".factor";
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      clamp->set_min(0.0f);
      clamp->set_max(1.0f);
      MathNode *delta = graph->create_node<MathNode>();
      delta->name = node.name + ".delta";
      delta->set_math_type(NODE_MATH_SUBTRACT);
      delta->set_value1(node.inputs.at(second_name));
      delta->set_value2(node.inputs.at(first_name));
      MathNode *product = graph->create_node<MathNode>();
      product->name = node.name + ".product";
      product->set_math_type(NODE_MATH_MULTIPLY);
      MathNode *sum = graph->create_node<MathNode>();
      sum->set_math_type(NODE_MATH_ADD);
      sum->set_value1(node.inputs.at(first_name));
      lowered_nodes.emplace(coordinate->name, coordinate);
      lowered_nodes.emplace(clamp->name, clamp);
      lowered_nodes.emplace(delta->name, delta);
      lowered_nodes.emplace(product->name, product);
      lowered = sum;
    }
    else if (is_split(node.nodedef)) {
      const bool top_to_bottom = split_is_top_to_bottom(node.nodedef);
      const bool color4 = is_color4_split(node.nodedef);
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      SeparateXYZNode *coordinate = graph->create_node<SeparateXYZNode>();
      coordinate->name = node.name + ".coordinate";
      MathNode *factor = graph->create_node<MathNode>();
      factor->name = node.name + ".factor";
      factor->set_math_type(NODE_MATH_GREATER_THAN);
      factor->set_value2(node.inputs.contains("center") ? node.inputs.at("center") : 0.5f);
      lowered_nodes.emplace(coordinate->name, coordinate);
      lowered_nodes.emplace(factor->name, factor);
      if (is_scalar_split(node.nodedef)) {
        const float first = node.inputs.contains(first_name) ? node.inputs.at(first_name) : 0.0f;
        const float second = node.inputs.contains(second_name) ? node.inputs.at(second_name) : 0.0f;
        MathNode *delta = graph->create_node<MathNode>();
        delta->name = node.name + ".delta";
        delta->set_math_type(NODE_MATH_SUBTRACT);
        delta->set_value1(second);
        delta->set_value2(first);
        MathNode *product = graph->create_node<MathNode>();
        product->name = node.name + ".product";
        product->set_math_type(NODE_MATH_MULTIPLY);
        MathNode *sum = graph->create_node<MathNode>();
        sum->set_math_type(NODE_MATH_ADD);
        sum->set_value1(first);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
      else {
        MixNode *mix = graph->create_node<MixNode>();
        mix->set_mix_type(NODE_MIX_BLEND);
        if (color4) {
          const float4 first = node.float4_inputs.contains(first_name) ?
                                   node.float4_inputs.at(first_name) :
                                   zero_float4();
          const float4 second = node.float4_inputs.contains(second_name) ?
                                    node.float4_inputs.at(second_name) :
                                    zero_float4();
          mix->set_color1(make_float3(first.x, first.y, first.z));
          mix->set_color2(make_float3(second.x, second.y, second.z));
          MathNode *alpha_delta = graph->create_node<MathNode>();
          alpha_delta->name = node.name + ".Alpha.delta";
          alpha_delta->set_math_type(NODE_MATH_SUBTRACT);
          alpha_delta->set_value1(second.w);
          alpha_delta->set_value2(first.w);
          MathNode *alpha_product = graph->create_node<MathNode>();
          alpha_product->name = node.name + ".Alpha.product";
          alpha_product->set_math_type(NODE_MATH_MULTIPLY);
          MathNode *alpha_sum = graph->create_node<MathNode>();
          alpha_sum->name = node.name + ".Alpha";
          alpha_sum->set_math_type(NODE_MATH_ADD);
          alpha_sum->set_value1(first.w);
          lowered_nodes.emplace(alpha_delta->name, alpha_delta);
          lowered_nodes.emplace(alpha_product->name, alpha_product);
          lowered_nodes.emplace(alpha_sum->name, alpha_sum);
        }
        else {
          const float3 first = node.color3_inputs.contains(first_name) ?
                                   node.color3_inputs.at(first_name) : make_float3(0.0f);
          const float3 second = node.color3_inputs.contains(second_name) ?
                                    node.color3_inputs.at(second_name) : make_float3(0.0f);
          mix->set_color1(first);
          mix->set_color2(second);
        }
        lowered = mix;
      }
    }
    else if (node.nodedef == image_vector3_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      image->set_colorspace(u_colorspace_data);
      lowered = image;
    }
    else if (node.nodedef == normalmap_float_id) {
      NormalMapNode *normalmap = graph->create_node<NormalMapNode>();
      normalmap->set_space(NODE_NORMAL_MAP_TANGENT);
      normalmap->set_convention(NODE_NORMAL_MAP_CONVENTION_OPENGL);
      normalmap->set_base(NODE_NORMAL_MAP_BASE_DISPLACED);
      normalmap->set_strength(1.0f);
      if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
        normalmap->set_color(input->second);
      }
      lowered = normalmap;
    }
    else if (node.nodedef == "ND_constant_vector3") {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_x(node.vector3_inputs.at("value").x);
      combine->set_y(node.vector3_inputs.at("value").y);
      combine->set_z(node.vector3_inputs.at("value").z);
      lowered = combine;
    }
    else if (node.nodedef == constant_vector2_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      const float2 value = node.vector2_inputs.at("value");
      combine->set_x(value.x);
      combine->set_y(value.y);
      combine->set_z(0.0f);
      lowered = combine;
    }
    else if (node.nodedef == combine3_vector3_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) {
        combine->set_x(input->second);
      }
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
        combine->set_y(input->second);
      }
      if (const auto input = node.inputs.find("in3"); input != node.inputs.end()) {
        combine->set_z(input->second);
      }
      lowered = combine;
    }
    else if (node.nodedef == combine2_vector2_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) combine->set_x(input->second);
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) combine->set_y(input->second);
      combine->set_z(0.0f);
      lowered = combine;
    }
    else if (node.nodedef == roughness_anisotropy_id || node.nodedef == glossiness_anisotropy_id) {
      /* See roughness_anisotropy_id's declaration comment above for the real
       * formula (mx_roughness_anisotropy.osl) this builds, and why the
       * "if (anisotropy > 0.0)" branch collapses into one unconditional
       * expression via clamp(anisotropy, 0.0, 0.98). glossiness_anisotropy
       * (pbrlib_ng.mtlx IMP_glossiness_anisotropy) prepends invert(glossiness)
       * = 1.0 - glossiness feeding the identical roughness_anisotropy chain. */
      const bool is_glossiness = node.nodedef == glossiness_anisotropy_id;
      const char *first_name = is_glossiness ? "glossiness" : "roughness";
      if (is_glossiness) {
        MathNode *invert = graph->create_node<MathNode>();
        invert->name = node.name + ".invert1";
        invert->set_math_type(NODE_MATH_SUBTRACT);
        invert->set_value1(1.0f);
        if (const auto v = node.inputs.find(first_name); v != node.inputs.end()) invert->set_value2(v->second);
        lowered_nodes.emplace(invert->name, invert);
      }
      MathNode *roughness_sqr_mul = graph->create_node<MathNode>();
      roughness_sqr_mul->name = node.name + ".roughness_sqr.multiply";
      roughness_sqr_mul->set_math_type(NODE_MATH_MULTIPLY);
      if (!is_glossiness) {
        if (const auto v = node.inputs.find(first_name); v != node.inputs.end()) {
          roughness_sqr_mul->set_value1(v->second);
          roughness_sqr_mul->set_value2(v->second);
        }
      }
      lowered_nodes.emplace(roughness_sqr_mul->name, roughness_sqr_mul);

      ClampNode *roughness_sqr = graph->create_node<ClampNode>();
      roughness_sqr->name = node.name + ".roughness_sqr";
      roughness_sqr->set_clamp_type(NODE_CLAMP_MINMAX);
      roughness_sqr->set_min(1e-8f); /* MaterialX shadergen's M_FLOAT_EPS. */
      roughness_sqr->set_max(1.0f);
      lowered_nodes.emplace(roughness_sqr->name, roughness_sqr);

      ClampNode *anisotropy_clamped = graph->create_node<ClampNode>();
      anisotropy_clamped->name = node.name + ".anisotropy_clamped";
      anisotropy_clamped->set_clamp_type(NODE_CLAMP_MINMAX);
      anisotropy_clamped->set_min(0.0f);
      anisotropy_clamped->set_max(0.98f);
      if (const auto v = node.inputs.find("anisotropy"); v != node.inputs.end()) {
        anisotropy_clamped->set_value(v->second);
      }
      lowered_nodes.emplace(anisotropy_clamped->name, anisotropy_clamped);

      MathNode *one_minus_anisotropy = graph->create_node<MathNode>();
      one_minus_anisotropy->name = node.name + ".one_minus_anisotropy";
      one_minus_anisotropy->set_math_type(NODE_MATH_SUBTRACT);
      one_minus_anisotropy->set_value1(1.0f);
      lowered_nodes.emplace(one_minus_anisotropy->name, one_minus_anisotropy);

      MathNode *aspect = graph->create_node<MathNode>();
      aspect->name = node.name + ".aspect";
      aspect->set_math_type(NODE_MATH_SQRT);
      lowered_nodes.emplace(aspect->name, aspect);

      MathNode *x_divide = graph->create_node<MathNode>();
      x_divide->name = node.name + ".x.divide";
      x_divide->set_math_type(NODE_MATH_DIVIDE);
      lowered_nodes.emplace(x_divide->name, x_divide);

      MathNode *x_min = graph->create_node<MathNode>();
      x_min->name = node.name + ".x";
      x_min->set_math_type(NODE_MATH_MINIMUM);
      x_min->set_value2(1.0f);
      lowered_nodes.emplace(x_min->name, x_min);

      MathNode *y_multiply = graph->create_node<MathNode>();
      y_multiply->name = node.name + ".y";
      y_multiply->set_math_type(NODE_MATH_MULTIPLY);
      lowered_nodes.emplace(y_multiply->name, y_multiply);

      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered = combine;
    }
    else if (node.nodedef == open_pbr_anisotropy_id) {
      MathNode *aniso_invert = graph->create_node<MathNode>();
      aniso_invert->name = node.name + ".aniso_invert";
      aniso_invert->set_math_type(NODE_MATH_SUBTRACT);
      aniso_invert->set_value1(1.0f);
      if (const auto v = node.inputs.find("anisotropy"); v != node.inputs.end()) {
        aniso_invert->set_value2(v->second);
      }
      lowered_nodes.emplace(aniso_invert->name, aniso_invert);

      MathNode *aniso_invert_sq = graph->create_node<MathNode>();
      aniso_invert_sq->name = node.name + ".aniso_invert_sq";
      aniso_invert_sq->set_math_type(NODE_MATH_MULTIPLY);
      lowered_nodes.emplace(aniso_invert_sq->name, aniso_invert_sq);

      MathNode *denom = graph->create_node<MathNode>();
      denom->name = node.name + ".denom";
      denom->set_math_type(NODE_MATH_ADD);
      denom->set_value2(1.0f);
      lowered_nodes.emplace(denom->name, denom);

      MathNode *fraction = graph->create_node<MathNode>();
      fraction->name = node.name + ".fraction";
      fraction->set_math_type(NODE_MATH_DIVIDE);
      fraction->set_value1(2.0f);
      lowered_nodes.emplace(fraction->name, fraction);

      MathNode *sqrt = graph->create_node<MathNode>();
      sqrt->name = node.name + ".sqrt";
      sqrt->set_math_type(NODE_MATH_SQRT);
      lowered_nodes.emplace(sqrt->name, sqrt);

      MathNode *rough_sq = graph->create_node<MathNode>();
      rough_sq->name = node.name + ".rough_sq";
      rough_sq->set_math_type(NODE_MATH_MULTIPLY);
      if (const auto v = node.inputs.find("roughness"); v != node.inputs.end()) {
        rough_sq->set_value1(v->second);
        rough_sq->set_value2(v->second);
      }
      lowered_nodes.emplace(rough_sq->name, rough_sq);

      MathNode *alpha_x = graph->create_node<MathNode>();
      alpha_x->name = node.name + ".alpha_x";
      alpha_x->set_math_type(NODE_MATH_MULTIPLY);
      lowered_nodes.emplace(alpha_x->name, alpha_x);

      MathNode *alpha_y = graph->create_node<MathNode>();
      alpha_y->name = node.name + ".alpha_y";
      alpha_y->set_math_type(NODE_MATH_MULTIPLY);
      lowered_nodes.emplace(alpha_y->name, alpha_y);

      lowered = graph->create_node<CombineXYZNode>();
      static_cast<CombineXYZNode *>(lowered)->set_z(0.0f);
    }
    else if (node.nodedef == blackbody_id) {
      BlackbodyNode *blackbody = graph->create_node<BlackbodyNode>();
      if (const auto temperature = node.inputs.find("temperature"); temperature != node.inputs.end()) {
        blackbody->set_temperature(temperature->second);
      }
      lowered = blackbody;
    }
    else if (node.nodedef == roughness_dual_id) {
      /* See roughness_dual_id's declaration comment above for the real
       * formula/citation and why the select needs a real runtime compare,
       * unlike roughness_anisotropy's collapsible branch. */
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      lowered_nodes.emplace(separate->name, separate);

      const auto default_it = node.vector2_inputs.find("roughness");
      const float2 default_roughness = default_it != node.vector2_inputs.end() ? default_it->second :
                                                                                  make_float2(0.0f, 0.0f);

      MathNode *x_sqr = graph->create_node<MathNode>();
      x_sqr->name = node.name + ".x.multiply";
      x_sqr->set_math_type(NODE_MATH_MULTIPLY);
      x_sqr->set_value1(default_roughness.x);
      x_sqr->set_value2(default_roughness.x);
      lowered_nodes.emplace(x_sqr->name, x_sqr);

      ClampNode *x_clamp = graph->create_node<ClampNode>();
      x_clamp->name = node.name + ".x";
      x_clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      x_clamp->set_min(1e-8f); /* MaterialX shadergen's M_FLOAT_EPS. */
      x_clamp->set_max(1.0f);
      lowered_nodes.emplace(x_clamp->name, x_clamp);

      MathNode *y_sqr = graph->create_node<MathNode>();
      y_sqr->name = node.name + ".y.multiply";
      y_sqr->set_math_type(NODE_MATH_MULTIPLY);
      y_sqr->set_value1(default_roughness.y);
      y_sqr->set_value2(default_roughness.y);
      lowered_nodes.emplace(y_sqr->name, y_sqr);

      ClampNode *y_clamp = graph->create_node<ClampNode>();
      y_clamp->name = node.name + ".y_clamp";
      y_clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      y_clamp->set_min(1e-8f);
      y_clamp->set_max(1.0f);
      lowered_nodes.emplace(y_clamp->name, y_clamp);

      MathNode *condition = graph->create_node<MathNode>();
      condition->name = node.name + ".condition";
      condition->set_math_type(NODE_MATH_LESS_THAN);
      condition->set_value1(default_roughness.y);
      condition->set_value2(0.0f);
      lowered_nodes.emplace(condition->name, condition);

      MathNode *delta = graph->create_node<MathNode>();
      delta->name = node.name + ".y.delta";
      delta->set_math_type(NODE_MATH_SUBTRACT);
      lowered_nodes.emplace(delta->name, delta);

      MathNode *product = graph->create_node<MathNode>();
      product->name = node.name + ".y.product";
      product->set_math_type(NODE_MATH_MULTIPLY);
      lowered_nodes.emplace(product->name, product);

      MathNode *y_select = graph->create_node<MathNode>();
      y_select->name = node.name + ".y";
      y_select->set_math_type(NODE_MATH_ADD);
      lowered_nodes.emplace(y_select->name, y_select);

      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered = combine;
    }
    else if (node.nodedef == convert_vector3_vector2_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (node.nodedef == place2d_vector2_id) {
      const auto vector = [&](const char *suffix, const float2 &value) {
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->name = node.name + suffix;
        combine->set_x(value.x); combine->set_y(value.y); combine->set_z(0.0f);
        lowered_nodes.emplace(combine->name, combine);
        return combine;
      };
      CombineXYZNode *pivot = vector(".pivot", node.vector2_inputs.at("pivot"));
      CombineXYZNode *scale = vector(".scale", node.vector2_inputs.at("scale"));
      CombineXYZNode *offset = vector(".offset", node.vector2_inputs.at("offset"));
      VectorMathNode *subpivot = graph->create_node<VectorMathNode>(); subpivot->name = node.name + ".subpivot"; subpivot->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
      VectorMathNode *applyscale = graph->create_node<VectorMathNode>(); applyscale->name = node.name + ".applyscale"; applyscale->set_math_type(NODE_VECTOR_MATH_DIVIDE);
      VectorMathNode *applyoffset = graph->create_node<VectorMathNode>(); applyoffset->name = node.name + ".applyoffset"; applyoffset->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
      VectorMathNode *applyoffset2 = graph->create_node<VectorMathNode>(); applyoffset2->name = node.name + ".applyoffset2"; applyoffset2->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
      VectorMathNode *applyscale2 = graph->create_node<VectorMathNode>(); applyscale2->name = node.name + ".applyscale2"; applyscale2->set_math_type(NODE_VECTOR_MATH_DIVIDE);
      VectorMathNode *addpivot = graph->create_node<VectorMathNode>(); addpivot->name = node.name + ".addpivot"; addpivot->set_math_type(NODE_VECTOR_MATH_ADD);
      VectorMathNode *addpivot2 = graph->create_node<VectorMathNode>(); addpivot2->name = node.name + ".addpivot2"; addpivot2->set_math_type(NODE_VECTOR_MATH_ADD);
      MathNode *radians = graph->create_node<MathNode>(); radians->name = node.name + ".radians"; radians->set_math_type(NODE_MATH_RADIANS); radians->set_value1(node.inputs.at("rotate"));
      VectorRotateNode *rotate = graph->create_node<VectorRotateNode>(); rotate->name = node.name + ".rotate"; rotate->set_rotate_type(NODE_VECTOR_ROTATE_TYPE_AXIS_Z); rotate->set_invert(true);
      VectorRotateNode *rotate2 = graph->create_node<VectorRotateNode>(); rotate2->name = node.name + ".rotate2"; rotate2->set_rotate_type(NODE_VECTOR_ROTATE_TYPE_AXIS_Z); rotate2->set_invert(true);
      MixVectorNode *operation = graph->create_node<MixVectorNode>(); operation->set_fac(node.inputs.at("operationorder"));
      for (ShaderNode *aux : {static_cast<ShaderNode *>(pivot), static_cast<ShaderNode *>(scale), static_cast<ShaderNode *>(offset), static_cast<ShaderNode *>(subpivot), static_cast<ShaderNode *>(applyscale), static_cast<ShaderNode *>(applyoffset), static_cast<ShaderNode *>(applyoffset2), static_cast<ShaderNode *>(applyscale2), static_cast<ShaderNode *>(addpivot), static_cast<ShaderNode *>(addpivot2), static_cast<ShaderNode *>(radians), static_cast<ShaderNode *>(rotate), static_cast<ShaderNode *>(rotate2)}) lowered_nodes.emplace(aux->name, aux);
      lowered = operation;
    }
    else if (is_safepower_vector2(node.nodedef)) {
      const bool scalar_second = safepower_vector2_uses_scalar_second(node.nodedef);
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      SeparateXYZNode *second = scalar_second ? nullptr : graph->create_node<SeparateXYZNode>();
      if (second) second->name = node.name + ".second";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(first->name, first);
      if (second) lowered_nodes.emplace(second->name, second);
      for (const char *channel : {"X", "Y"}) {
        for (const auto &[suffix, type] : {std::pair{"abs", NODE_MATH_ABSOLUTE},
                                          std::pair{"sign", NODE_MATH_SIGN},
                                          std::pair{"power", NODE_MATH_POWER},
                                          std::pair{"multiply", NODE_MATH_MULTIPLY}})
        {
          MathNode *math = graph->create_node<MathNode>();
          math->name = node.name + "." + channel + "." + suffix;
          math->set_math_type(type);
          lowered_nodes.emplace(math->name, math);
        }
        const auto component = [channel](const float2 &value) { return channel[0] == 'X' ? value.x : value.y; };
        if (const auto input = node.vector2_inputs.find("in1"); input != node.vector2_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".abs"))->set_value1(component(input->second));
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".sign"))->set_value1(component(input->second));
        }
        if (scalar_second) {
          if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".power"))->set_value2(input->second);
          }
        }
        else if (const auto input = node.vector2_inputs.find("in2"); input != node.vector2_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".power"))->set_value2(component(input->second));
        }
      }
      lowered = combine;
    }
    else if (node.nodedef == rotate2d_vector2_id || node.nodedef == rotate3d_vector3_id) {
      MathNode *radians = graph->create_node<MathNode>();
      radians->name = node.name + ".radians";
      radians->set_math_type(NODE_MATH_RADIANS);
      if (const auto amount = node.inputs.find("amount"); amount != node.inputs.end()) {
        radians->set_value1(amount->second);
      }
      else {
        radians->set_value1(0.0f);
      }

      VectorRotateNode *rotate = graph->create_node<VectorRotateNode>();
      rotate->set_rotate_type(node.nodedef == rotate2d_vector2_id ? NODE_VECTOR_ROTATE_TYPE_AXIS_Z :
                                                              NODE_VECTOR_ROTATE_TYPE_AXIS);
      rotate->set_invert(node.nodedef == rotate2d_vector2_id);
      if (node.nodedef == rotate2d_vector2_id) {
        if (const auto input = node.vector2_inputs.find("in"); input != node.vector2_inputs.end()) {
          rotate->set_vector(make_float3(input->second.x, input->second.y, 0.0f));
        }
      }
      else {
        if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
          rotate->set_vector(input->second);
        }
        const auto axis = node.vector3_inputs.find("axis");
        rotate->set_axis(axis == node.vector3_inputs.end() ? make_float3(0.0f, 1.0f, 0.0f) :
                                                        axis->second);
      }
      lowered_nodes.emplace(radians->name, radians);
      lowered = rotate;
    }
    else if (NodeMathType math_type; vector2_binary_component_math_type(node.nodedef, &math_type)) {
      const bool scalar_second = vector2_binary_component_math_uses_scalar_second(node.nodedef);
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      SeparateXYZNode *second = scalar_second ? nullptr : graph->create_node<SeparateXYZNode>();
      if (second) second->name = node.name + ".second";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(first->name, first);
      if (second) lowered_nodes.emplace(second->name, second);
      for (const char *channel : {"X", "Y"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        if (const auto input = node.vector2_inputs.find("in1"); input != node.vector2_inputs.end()) {
          math->set_value1(channel[0] == 'X' ? input->second.x : input->second.y);
        }
        if (scalar_second) {
          if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) math->set_value2(input->second);
        }
        else if (const auto input = node.vector2_inputs.find("in2"); input != node.vector2_inputs.end()) {
          math->set_value2(channel[0] == 'X' ? input->second.x : input->second.y);
        }
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (is_safepower_vector3(node.nodedef)) {
      const bool scalar_second = safepower_vector3_uses_scalar_second(node.nodedef);
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      SeparateXYZNode *second = scalar_second ? nullptr : graph->create_node<SeparateXYZNode>();
      if (second) second->name = node.name + ".second";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(first->name, first);
      if (second) lowered_nodes.emplace(second->name, second);
      for (const char *channel : {"X", "Y", "Z"}) {
        for (const auto &[suffix, type] : {std::pair{"abs", NODE_MATH_ABSOLUTE},
                                          std::pair{"sign", NODE_MATH_SIGN},
                                          std::pair{"power", NODE_MATH_POWER},
                                          std::pair{"multiply", NODE_MATH_MULTIPLY}})
        {
          MathNode *math = graph->create_node<MathNode>();
          math->name = node.name + "." + channel + "." + suffix;
          math->set_math_type(type);
          lowered_nodes.emplace(math->name, math);
        }
        const auto component = [channel](const float3 &value) {
          return channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z;
        };
        if (const auto input = node.vector3_inputs.find("in1"); input != node.vector3_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".abs"))->set_value1(component(input->second));
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".sign"))->set_value1(component(input->second));
        }
        if (scalar_second) {
          if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".power"))->set_value2(input->second);
          }
        }
        else if (const auto input = node.vector3_inputs.find("in2"); input != node.vector3_inputs.end()) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".power"))->set_value2(component(input->second));
        }
      }
      lowered = combine;
    }
    else if (NodeMathType math_type; vector3_binary_component_math_type(node.nodedef, &math_type)) {
      const bool scalar_second = vector3_binary_component_math_uses_scalar_second(node.nodedef);
      SeparateXYZNode *first = graph->create_node<SeparateXYZNode>();
      first->name = node.name + ".first";
      SeparateXYZNode *second = scalar_second ? nullptr : graph->create_node<SeparateXYZNode>();
      if (second) second->name = node.name + ".second";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(first->name, first);
      if (second) lowered_nodes.emplace(second->name, second);
      for (const char *channel : {"X", "Y", "Z"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        const auto component = [channel](const float3 &value) {
          return channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z;
        };
        if (const auto input = node.vector3_inputs.find("in1"); input != node.vector3_inputs.end()) math->set_value1(component(input->second));
        if (scalar_second) {
          if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) math->set_value2(input->second);
        }
        else if (const auto input = node.vector3_inputs.find("in2"); input != node.vector3_inputs.end()) math->set_value2(component(input->second));
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (bool scalar_amount; vector2_invert_type(node.nodedef, &scalar_amount)) {
      SeparateXYZNode *input = graph->create_node<SeparateXYZNode>();
      input->name = node.name + ".in.separate";
      SeparateXYZNode *amount = scalar_amount ? nullptr : graph->create_node<SeparateXYZNode>();
      if (amount) amount->name = node.name + ".amount.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(input->name, input);
      if (amount) lowered_nodes.emplace(amount->name, amount);
      for (const char *channel : {"X", "Y"}) {
        MathNode *subtract = graph->create_node<MathNode>();
        subtract->name = node.name + "." + channel;
        subtract->set_math_type(NODE_MATH_SUBTRACT);
        if (scalar_amount) {
          if (const auto value = node.inputs.find("amount"); value != node.inputs.end()) subtract->set_value1(value->second);
        }
        else if (const auto value = node.vector2_inputs.find("amount"); value != node.vector2_inputs.end()) {
          subtract->set_value1(channel[0] == 'X' ? value->second.x : value->second.y);
        }
        lowered_nodes.emplace(subtract->name, subtract);
      }
      lowered = combine;
    }
    else if (bool scalar_amount; vector3_invert_type(node.nodedef, &scalar_amount)) {
      SeparateXYZNode *input = graph->create_node<SeparateXYZNode>();
      input->name = node.name + ".in.separate";
      SeparateXYZNode *amount = scalar_amount ? nullptr : graph->create_node<SeparateXYZNode>();
      if (amount) amount->name = node.name + ".amount.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(input->name, input);
      if (amount) lowered_nodes.emplace(amount->name, amount);
      for (const char *channel : {"X", "Y", "Z"}) {
        MathNode *subtract = graph->create_node<MathNode>();
        subtract->name = node.name + "." + channel;
        subtract->set_math_type(NODE_MATH_SUBTRACT);
        if (scalar_amount) {
          if (const auto value = node.inputs.find("amount"); value != node.inputs.end()) subtract->set_value1(value->second);
        }
        else if (const auto value = node.vector3_inputs.find("amount"); value != node.vector3_inputs.end()) {
          subtract->set_value1(channel[0] == 'X' ? value->second.x : channel[0] == 'Y' ? value->second.y : value->second.z);
        }
        lowered_nodes.emplace(subtract->name, subtract);
      }
      lowered = combine;
    }
    else if (bool scalar_edges; vector2_smoothstep_type(node.nodedef, &scalar_edges)) {
      SeparateXYZNode *input = graph->create_node<SeparateXYZNode>(); input->name = node.name + ".in.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>(); combine->set_z(0.0f);
      lowered_nodes.emplace(input->name, input);
      for (const char *channel : {"X", "Y"}) {
        const auto value = [channel](const float2 &v) { return channel[0] == 'X' ? v.x : v.y; };
        MathNode *numerator = graph->create_node<MathNode>(); numerator->name = node.name + "." + channel + ".numerator"; numerator->set_math_type(NODE_MATH_SUBTRACT);
        MathNode *denominator = graph->create_node<MathNode>(); denominator->name = node.name + "." + channel + ".denominator"; denominator->set_math_type(NODE_MATH_SUBTRACT);
        MathNode *divide = graph->create_node<MathNode>(); divide->name = node.name + "." + channel + ".divide"; divide->set_math_type(NODE_MATH_DIVIDE);
        MathNode *maximum = graph->create_node<MathNode>(); maximum->name = node.name + "." + channel + ".maximum"; maximum->set_math_type(NODE_MATH_MAXIMUM); maximum->set_value2(0.0f);
        MathNode *minimum = graph->create_node<MathNode>(); minimum->name = node.name + "." + channel + ".minimum"; minimum->set_math_type(NODE_MATH_MINIMUM); minimum->set_value2(1.0f);
        MathNode *square = graph->create_node<MathNode>(); square->name = node.name + "." + channel + ".square"; square->set_math_type(NODE_MATH_MULTIPLY);
        MathNode *twice = graph->create_node<MathNode>(); twice->name = node.name + "." + channel + ".twice"; twice->set_math_type(NODE_MATH_MULTIPLY); twice->set_value2(2.0f);
        MathNode *cubic = graph->create_node<MathNode>(); cubic->name = node.name + "." + channel + ".cubic"; cubic->set_math_type(NODE_MATH_SUBTRACT); cubic->set_value1(3.0f);
        MathNode *result = graph->create_node<MathNode>(); result->name = node.name + "." + channel + ".result"; result->set_math_type(NODE_MATH_MULTIPLY);
        if (const auto in = node.vector2_inputs.find("in"); in != node.vector2_inputs.end()) numerator->set_value1(value(in->second));
        const float low = scalar_edges ? node.inputs.at("low") : value(node.vector2_inputs.at("low"));
        const float high = scalar_edges ? node.inputs.at("high") : value(node.vector2_inputs.at("high"));
        numerator->set_value2(low); denominator->set_value1(high); denominator->set_value2(low);
        for (ShaderNode *part : {static_cast<ShaderNode *>(numerator), static_cast<ShaderNode *>(denominator), static_cast<ShaderNode *>(divide), static_cast<ShaderNode *>(maximum), static_cast<ShaderNode *>(minimum), static_cast<ShaderNode *>(square), static_cast<ShaderNode *>(twice), static_cast<ShaderNode *>(cubic), static_cast<ShaderNode *>(result)}) lowered_nodes.emplace(part->name, part);
      }
      lowered = combine;
    }
    else if (bool scalar_edges; vector3_smoothstep_type(node.nodedef, &scalar_edges)) {
      SeparateXYZNode *input = graph->create_node<SeparateXYZNode>(); input->name = node.name + ".in.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(input->name, input);
      for (const char *channel : {"X", "Y", "Z"}) {
        const auto value = [channel](const float3 &v) { return channel[0] == 'X' ? v.x : channel[0] == 'Y' ? v.y : v.z; };
        MathNode *numerator = graph->create_node<MathNode>(); numerator->name = node.name + "." + channel + ".numerator"; numerator->set_math_type(NODE_MATH_SUBTRACT);
        MathNode *denominator = graph->create_node<MathNode>(); denominator->name = node.name + "." + channel + ".denominator"; denominator->set_math_type(NODE_MATH_SUBTRACT);
        MathNode *divide = graph->create_node<MathNode>(); divide->name = node.name + "." + channel + ".divide"; divide->set_math_type(NODE_MATH_DIVIDE);
        MathNode *maximum = graph->create_node<MathNode>(); maximum->name = node.name + "." + channel + ".maximum"; maximum->set_math_type(NODE_MATH_MAXIMUM); maximum->set_value2(0.0f);
        MathNode *minimum = graph->create_node<MathNode>(); minimum->name = node.name + "." + channel + ".minimum"; minimum->set_math_type(NODE_MATH_MINIMUM); minimum->set_value2(1.0f);
        MathNode *square = graph->create_node<MathNode>(); square->name = node.name + "." + channel + ".square"; square->set_math_type(NODE_MATH_MULTIPLY);
        MathNode *twice = graph->create_node<MathNode>(); twice->name = node.name + "." + channel + ".twice"; twice->set_math_type(NODE_MATH_MULTIPLY); twice->set_value2(2.0f);
        MathNode *cubic = graph->create_node<MathNode>(); cubic->name = node.name + "." + channel + ".cubic"; cubic->set_math_type(NODE_MATH_SUBTRACT); cubic->set_value1(3.0f);
        MathNode *result = graph->create_node<MathNode>(); result->name = node.name + "." + channel + ".result"; result->set_math_type(NODE_MATH_MULTIPLY);
        if (const auto in = node.vector3_inputs.find("in"); in != node.vector3_inputs.end()) numerator->set_value1(value(in->second));
        const float low = scalar_edges ? node.inputs.at("low") : value(node.vector3_inputs.at("low"));
        const float high = scalar_edges ? node.inputs.at("high") : value(node.vector3_inputs.at("high"));
        numerator->set_value2(low); denominator->set_value1(high); denominator->set_value2(low);
        for (ShaderNode *part : {static_cast<ShaderNode *>(numerator), static_cast<ShaderNode *>(denominator), static_cast<ShaderNode *>(divide), static_cast<ShaderNode *>(maximum), static_cast<ShaderNode *>(minimum), static_cast<ShaderNode *>(square), static_cast<ShaderNode *>(twice), static_cast<ShaderNode *>(cubic), static_cast<ShaderNode *>(result)}) lowered_nodes.emplace(part->name, part);
      }
      lowered = combine;
    }
    else if (NodeMathType math_type; vector2_domain_math_type(node.nodedef, &math_type)) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      for (const char *channel : {"X", "Y"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        if (node.nodedef == "ND_ln_vector2") math->set_value2(M_E);
        if (const auto input = node.vector2_inputs.find("in"); input != node.vector2_inputs.end()) {
          math->set_value1(channel[0] == 'X' ? input->second.x : input->second.y);
        }
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (NodeMathType math_type; vector3_domain_math_type(node.nodedef, &math_type)) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(separate->name, separate);
      for (const char *channel : {"X", "Y", "Z"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        if (node.nodedef == "ND_ln_vector3") math->set_value2(M_E);
        if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
          const float3 value = input->second;
          math->set_value1(channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z);
        }
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (NodeMathType math_type; vector2_atan2_type(node.nodedef, &math_type)) {
      SeparateXYZNode *separate_y = graph->create_node<SeparateXYZNode>();
      separate_y->name = node.name + ".iny.separate";
      SeparateXYZNode *separate_x = graph->create_node<SeparateXYZNode>();
      separate_x->name = node.name + ".inx.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_z(0.0f);
      lowered_nodes.emplace(separate_y->name, separate_y);
      lowered_nodes.emplace(separate_x->name, separate_x);
      for (const char *channel : {"X", "Y"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        if (const auto iny = node.vector2_inputs.find("iny"); iny != node.vector2_inputs.end()) {
          math->set_value1(channel[0] == 'X' ? iny->second.x : iny->second.y);
        }
        if (const auto inx = node.vector2_inputs.find("inx"); inx != node.vector2_inputs.end()) {
          math->set_value2(channel[0] == 'X' ? inx->second.x : inx->second.y);
        }
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else if (NodeMathType math_type; vector3_atan2_type(node.nodedef, &math_type)) {
      SeparateXYZNode *separate_y = graph->create_node<SeparateXYZNode>();
      separate_y->name = node.name + ".iny.separate";
      SeparateXYZNode *separate_x = graph->create_node<SeparateXYZNode>();
      separate_x->name = node.name + ".inx.separate";
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      lowered_nodes.emplace(separate_y->name, separate_y);
      lowered_nodes.emplace(separate_x->name, separate_x);
      for (const char *channel : {"X", "Y", "Z"}) {
        MathNode *math = graph->create_node<MathNode>();
        math->name = node.name + "." + channel;
        math->set_math_type(math_type);
        if (const auto iny = node.vector3_inputs.find("iny"); iny != node.vector3_inputs.end()) {
          const float3 value = iny->second;
          math->set_value1(channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z);
        }
        if (const auto inx = node.vector3_inputs.find("inx"); inx != node.vector3_inputs.end()) {
          const float3 value = inx->second;
          math->set_value2(channel[0] == 'X' ? value.x : channel[0] == 'Y' ? value.y : value.z);
        }
        lowered_nodes.emplace(math->name, math);
      }
      lowered = combine;
    }
    else {
      NodeVectorMathType vector_type;
      if (vector_math_type(node.nodedef, &vector_type)) {
        VectorMathNode *math = graph->create_node<VectorMathNode>();
        math->set_math_type(vector_type);
        if (const auto input = node.vector3_inputs.find(
                vector_math_is_unary(node.nodedef) ? "in" : "in1");
            input != node.vector3_inputs.end())
        {
          math->set_vector1(input->second);
        }
        if (const auto input = node.vector3_inputs.find("in2");
            input != node.vector3_inputs.end())
        {
          math->set_vector2(input->second);
        }
        if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
          if (vector_math_uses_scalar_broadcast_second(node.nodedef)) {
            math->set_vector2(make_float3(input->second, input->second, input->second));
          }
          else {
            math->set_scale(input->second);
          }
        }
        if (node.nodedef == "ND_refract_vector3") {
          if (const auto scale = node.inputs.find("scale"); scale != node.inputs.end()) {
            math->set_scale(scale->second);
          }
        }
        if (vector_math_uses_scalar_broadcast_second(node.nodedef) &&
            node.links.find("in2") != node.links.end())
        {
          CombineXYZNode *broadcast = graph->create_node<CombineXYZNode>();
          broadcast->name = node.name + ".broadcast";
          lowered_nodes.emplace(broadcast->name, broadcast);
        }
        lowered = math;
      }
      else if (vector2_math_type(node.nodedef, &vector_type)) {
        VectorMathNode *math = graph->create_node<VectorMathNode>();
        math->set_math_type(vector_type);
        const char *first_name = vector2_math_is_unary(node.nodedef) ? "in" : "in1";
        if (const auto input = node.vector2_inputs.find(first_name); input != node.vector2_inputs.end()) {
          math->set_vector1(make_float3(input->second.x, input->second.y, 0.0f));
        }
        if (const auto input = node.vector2_inputs.find("in2"); input != node.vector2_inputs.end()) {
          math->set_vector2(make_float3(input->second.x, input->second.y, 0.0f));
        }
        if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
          if (vector2_math_uses_scalar_broadcast_second(node.nodedef)) {
            math->set_vector2(make_float3(input->second, input->second, 0.0f));
          }
          else {
            math->set_scale(input->second);
          }
        }
        if (vector2_math_uses_scalar_broadcast_second(node.nodedef) &&
            node.links.find("in2") != node.links.end())
        {
          CombineXYZNode *broadcast = graph->create_node<CombineXYZNode>();
          broadcast->name = node.name + ".broadcast";
          broadcast->set_z(0.0f);
          lowered_nodes.emplace(broadcast->name, broadcast);
        }
        if (vector2_math_returns_float(node.nodedef)) {
          lowered = math;
        }
        else {
          math->name = node.name;
          SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
          separate->name = node.name + ".vector2.separate";
          CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
          combine->name = node.name + ".vector2";
          combine->set_z(0.0f);
          lowered_nodes.emplace(node.name + ".vector2.math", math);
          lowered_nodes.emplace(separate->name, separate);
          lowered = combine;
          preserve_lowered_name = true;
        }
      }
      else if (node.nodedef == standard_surface_id) {
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
        if (const auto input = node.color3_inputs.find("base_color");
            input != node.color3_inputs.end())
        {
          principled->set_base_color(input->second);
        }
        if (const auto input = node.inputs.find("base"); input != node.inputs.end()) {
          /* ShaderGraph::finalize() adds its implicit unit closure weight. Store the delta so
           * the finalized closure weight equals Standard Surface's base weight. */
          principled->set_surface_mix_weight(input->second - 1.0f);
        }
        if (const auto input = node.inputs.find("metalness"); input != node.inputs.end()) {
          principled->set_metallic(input->second);
        }
        if (const auto input = node.inputs.find("diffuse_roughness"); input != node.inputs.end()) {
          principled->set_diffuse_roughness(input->second);
        }
        if (const auto input = node.inputs.find("specular"); input != node.inputs.end()) {
          principled->set_specular_ior_level(input->second * 2.0f);
        }
        if (const auto input = node.color3_inputs.find("specular_color");
            input != node.color3_inputs.end())
        {
          principled->set_specular_tint(input->second);
        }
        if (const auto input = node.inputs.find("specular_roughness"); input != node.inputs.end()) {
          principled->set_roughness(input->second);
        }
        if (const auto input = node.inputs.find("specular_IOR"); input != node.inputs.end()) {
          principled->set_ior(input->second);
        }
        if (const auto input = node.inputs.find("specular_anisotropy");
            input != node.inputs.end())
        {
          principled->set_anisotropic(input->second);
        }
        if (const auto input = node.inputs.find("specular_rotation"); input != node.inputs.end()) {
          principled->set_anisotropic_rotation(input->second);
        }
        if (const auto input = node.inputs.find("subsurface"); input != node.inputs.end()) {
          principled->set_subsurface_weight(input->second);
        }
        if (const auto input = node.color3_inputs.find("subsurface_radius");
            input != node.color3_inputs.end())
        {
          principled->set_subsurface_radius(input->second);
        }
        if (const auto input = node.inputs.find("subsurface_scale"); input != node.inputs.end()) {
          principled->set_subsurface_scale(input->second);
        }
        if (const auto input = node.inputs.find("subsurface_anisotropy");
            input != node.inputs.end())
        {
          principled->set_subsurface_anisotropy(input->second);
        }
        if (const auto input = node.inputs.find("thin_film_thickness");
            input != node.inputs.end())
        {
          principled->set_thin_film_thickness(input->second);
        }
        if (const auto input = node.inputs.find("thin_film_IOR"); input != node.inputs.end()) {
          principled->set_thin_film_ior(input->second);
        }
        if (const auto input = node.inputs.find("emission"); input != node.inputs.end()) {
          principled->set_emission_strength(input->second);
        }
        if (const auto input = node.color3_inputs.find("emission_color");
            input != node.color3_inputs.end())
        {
          principled->set_emission_color(input->second);
        }
        if (const auto input = node.color3_inputs.find("opacity");
            input != node.color3_inputs.end())
        {
          principled->set_alpha(luminance(input->second));
        }
        if (const auto input = node.int_inputs.find("thin_walled"); input != node.int_inputs.end()) {
          principled->set_thin_wall(input->second != 0);
        }
        if (const auto input = node.vector3_inputs.find("normal"); input != node.vector3_inputs.end()) {
          principled->set_normal(input->second);
        }
        if (const auto input = node.vector3_inputs.find("tangent"); input != node.vector3_inputs.end()) {
          principled->set_tangent(input->second);
        }

        ShaderOutput *closure = principled->output("BSDF");
        lowered_nodes.emplace(node.name + ".standard_surface_base", principled);

        if (has_standard_surface_sheen(node)) {
          SheenBsdfNode *sheen = graph->create_node<SheenBsdfNode>();
          sheen->name = node.name + ".standard_surface_sheen";
          if (const auto input = node.color3_inputs.find("sheen_color");
              input != node.color3_inputs.end())
          {
            sheen->set_color(input->second);
          }
          if (const auto input = node.inputs.find("sheen"); input != node.inputs.end()) {
            sheen->set_surface_mix_weight(input->second);
          }
          if (const auto input = node.inputs.find("sheen_roughness"); input != node.inputs.end()) {
            sheen->set_roughness(input->second);
          }
          if (const auto input = node.vector3_inputs.find("normal");
              input != node.vector3_inputs.end())
          {
            sheen->set_normal(input->second);
          }
          AddClosureNode *sum = graph->create_node<AddClosureNode>();
          sum->name = node.name + ".standard_surface_sheen_sum";
          lowered_nodes.emplace(sheen->name, sheen);
          lowered_nodes.emplace(sum->name, sum);
          closure = sum->output("Closure");
        }

        if (has_standard_surface_transmission(node)) {
          GlassBsdfNode *transmission = graph->create_node<GlassBsdfNode>();
          transmission->name = node.name + ".standard_surface_transmission";
          if (const auto input = node.color3_inputs.find("transmission_color");
              input != node.color3_inputs.end())
          {
            transmission->set_color(input->second);
          }
          if (const auto input = node.inputs.find("specular_roughness");
              input != node.inputs.end())
          {
            transmission->set_roughness(input->second);
          }
          if (const auto input = node.inputs.find("specular_IOR"); input != node.inputs.end()) {
            transmission->set_IOR(input->second);
          }
          if (const auto input = node.inputs.find("thin_film_thickness");
              input != node.inputs.end())
          {
            transmission->set_thin_film_thickness(input->second);
          }
          if (const auto input = node.inputs.find("thin_film_IOR"); input != node.inputs.end()) {
            transmission->set_thin_film_ior(input->second);
          }
          if (const auto input = node.vector3_inputs.find("normal");
              input != node.vector3_inputs.end())
          {
            transmission->set_normal(input->second);
          }
          MixClosureNode *mix = graph->create_node<MixClosureNode>();
          mix->name = node.name + ".standard_surface_transmission_mix";
          if (const auto input = node.inputs.find("transmission"); input != node.inputs.end()) {
            mix->set_fac(input->second);
          }
          lowered_nodes.emplace(transmission->name, transmission);
          lowered_nodes.emplace(mix->name, mix);
          closure = mix->output("Closure");
        }

        if (has_standard_surface_coat(node)) {
          GlossyBsdfNode *coat = graph->create_node<GlossyBsdfNode>();
          coat->name = node.name + ".standard_surface_coat";
          if (const auto input = node.color3_inputs.find("coat_color");
              input != node.color3_inputs.end())
          {
            coat->set_color(input->second);
          }
          if (const auto input = node.inputs.find("coat"); input != node.inputs.end()) {
            coat->set_surface_mix_weight(input->second);
          }
          if (const auto input = node.inputs.find("coat_roughness"); input != node.inputs.end()) {
            coat->set_roughness(input->second);
          }
          if (const auto input = node.vector3_inputs.find("coat_normal");
              input != node.vector3_inputs.end())
          {
            coat->set_normal(input->second);
          }
          else if (const auto input = node.vector3_inputs.find("normal");
                   input != node.vector3_inputs.end())
          {
            coat->set_normal(input->second);
          }
          AddClosureNode *sum = graph->create_node<AddClosureNode>();
          sum->name = node.name + ".standard_surface_coat_sum";
          lowered_nodes.emplace(coat->name, coat);
          lowered_nodes.emplace(sum->name, sum);
          closure = sum->output("Closure");
        }

        if (closure == principled->output("BSDF")) {
          lowered = principled;
          lowered_nodes.erase(node.name + ".standard_surface_base");
        }
        else {
          lowered = static_cast<ShaderNode *>(closure->parent);
        }
      }
      /* oren_nayar_diffuse_bsdf_id and direct LAMA leaf BSDFs are dual-purpose
       * -- see validate()'s matching comment. SurfaceShader-typed LAMA leaves
       * are native closures fed into ND_surface; plain Type::BSDF leaves still
       * fall through to the generic is_bsdf_producer dispatch below. */
      else if ((is_bsdf_producer(node.nodedef) || is_lama_microfacet_surface_bsdf(node.nodedef)) &&
               node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader)
      {
        if (node.nodedef == chiang_hair_bsdf_id) {
          PrincipledHairBsdfNode *hair = graph->create_node<PrincipledHairBsdfNode>();
          hair->set_model(NODE_PRINCIPLED_HAIR_CHIANG);
          hair->set_parametrization(NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
          const float2 roughness_r = node.vector2_inputs.contains("roughness_R") ?
                                         node.vector2_inputs.at("roughness_R") :
                                         make_float2(0.1f, 0.1f);
          const float longitudinal = invert_monotonic_roughness(
              roughness_r.x, chiang_longitudinal_variance_from_roughness);
          const float radial = invert_monotonic_roughness(
              roughness_r.y, chiang_azimuthal_scale_from_roughness);
          hair->set_roughness(longitudinal);
          hair->set_radial_roughness(radial);
          hair->set_coat(0.0f);
          hair->set_tint(make_float3(1.0f, 1.0f, 1.0f));
          hair->set_absorption_coefficient(node.vector3_inputs.contains("absorption_coefficient") ?
                                               node.vector3_inputs.at("absorption_coefficient") :
                                               make_float3(0.0f, 0.0f, 0.0f));
          hair->set_offset(node.inputs.contains("cuticle_angle") ?
                               node.inputs.at("cuticle_angle") * M_PI_F - (M_PI_F * 0.5f) :
                               0.0f);
          if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
            hair->set_ior(input->second);
          }
          lowered = hair;
        }
        else if (node.nodedef == translucent_bsdf_id || node.nodedef == lama_translucent_id) {
          TranslucentBsdfNode *translucent = graph->create_node<TranslucentBsdfNode>();
          if (const auto input = node.color3_inputs.find("color"); input != node.color3_inputs.end()) {
            translucent->set_color(input->second);
          }
          lowered = translucent;
        }
        else if (node.nodedef == subsurface_bsdf_id || node.nodedef == lama_sss_id) {
          SubsurfaceScatteringNode *sss = graph->create_node<SubsurfaceScatteringNode>();
          sss->set_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
          if (const auto input = node.color3_inputs.find("color"); input != node.color3_inputs.end()) {
            sss->set_color(input->second);
          }
          if (node.nodedef == lama_sss_id) {
            if (!node.links.contains("sssRadius") && !node.links.contains("sssScale") &&
                !node.links.contains("sssUnitLength"))
            {
              const float3 radius = node.color3_inputs.contains("sssRadius") ?
                                        node.color3_inputs.at("sssRadius") :
                                        make_float3(0.0f, 0.0f, 0.0f);
              const float scale = node.inputs.contains("sssScale") ? node.inputs.at("sssScale") : 1.0f;
              const float unit_length = node.inputs.contains("sssUnitLength") ?
                                            node.inputs.at("sssUnitLength") :
                                            0.00328f;
              sss->set_radius(radius * scale * unit_length);
            }
            if (const auto input = node.inputs.find("sssAnisotropy"); input != node.inputs.end()) {
              sss->set_subsurface_anisotropy(input->second);
            }
          }
          else {
            if (const auto input = node.color3_inputs.find("radius"); input != node.color3_inputs.end()) {
              sss->set_radius(input->second);
            }
            if (const auto input = node.inputs.find("anisotropy"); input != node.inputs.end()) {
              sss->set_subsurface_anisotropy(input->second);
            }
          }
          lowered = sss;
        }
        else if (node.nodedef == conductor_bsdf_id) {
          MetallicBsdfNode *metallic = graph->create_node<MetallicBsdfNode>();
          metallic->set_fresnel_type(CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
          metallic->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
          if (const auto input = node.color3_inputs.find("ior"); input != node.color3_inputs.end()) {
            metallic->set_ior(input->second);
          }
          if (const auto input = node.color3_inputs.find("extinction");
              input != node.color3_inputs.end())
          {
            metallic->set_k(input->second);
          }
          if (const auto input = node.vector2_inputs.find("roughness"); input != node.vector2_inputs.end()) {
            metallic->set_roughness(input->second.x);
          }
          if (const auto input = node.inputs.find("thinfilm_thickness"); input != node.inputs.end()) {
            metallic->set_thin_film_thickness(input->second);
          }
          if (const auto input = node.inputs.find("thinfilm_ior"); input != node.inputs.end()) {
            metallic->set_thin_film_ior(input->second);
          }
          lowered = metallic;
        }
        else if (node.nodedef == dielectric_bsdf_id) {
          GlassBsdfNode *glass = graph->create_node<GlassBsdfNode>();
          glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
          if (const auto input = node.color3_inputs.find("tint"); input != node.color3_inputs.end()) {
            glass->set_color(input->second);
          }
          if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
            glass->set_IOR(input->second);
          }
          if (const auto input = node.vector2_inputs.find("roughness"); input != node.vector2_inputs.end()) {
            glass->set_roughness(input->second.x);
          }
          if (const auto input = node.inputs.find("thinfilm_thickness"); input != node.inputs.end()) {
            glass->set_thin_film_thickness(input->second);
          }
          if (const auto input = node.inputs.find("thinfilm_ior"); input != node.inputs.end()) {
            glass->set_thin_film_ior(input->second);
          }
          lowered = glass;
        }
        else if (node.nodedef == sheen_bsdf_id) {
          SheenBsdfNode *sheen = graph->create_node<SheenBsdfNode>();
          sheen->set_distribution(CLOSURE_BSDF_SHEEN_ID);
          if (const auto input = node.color3_inputs.find("color"); input != node.color3_inputs.end()) {
            sheen->set_color(input->second);
          }
          if (const auto input = node.inputs.find("roughness"); input != node.inputs.end()) {
            sheen->set_roughness(input->second);
          }
          lowered = sheen;
        }
        else if (node.nodedef == lama_conductor_id) {
          MetallicBsdfNode *metallic = graph->create_node<MetallicBsdfNode>();
          metallic->set_fresnel_type(CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
          metallic->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
          metallic->set_ior(node.color3_inputs.contains("ior") ?
                                node.color3_inputs.at("ior") :
                                make_float3(0.180000007153f, 0.419999986887f, 1.37000000477f));
          metallic->set_k(node.color3_inputs.contains("extinction") ?
                              node.color3_inputs.at("extinction") :
                              make_float3(3.42000007629f, 2.34999990463f, 1.76999998093f));
          const float roughness = node.vector2_inputs.contains("roughness") ?
                                      node.vector2_inputs.at("roughness").x :
                                      0.01f;
          metallic->set_roughness(roughness);
          metallic->set_anisotropy(0.0f);
          metallic->set_rotation(0.0f);
          lowered = metallic;
        }
        else if (node.nodedef == lama_iridescence_id) {
          GlassBsdfNode *glass = graph->create_node<GlassBsdfNode>();
          glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
          glass->set_color(make_float3(1.0f, 1.0f, 1.0f));
          glass->set_IOR(1.0f);
          const float roughness = node.vector2_inputs.contains("roughness") ?
                                      node.vector2_inputs.at("roughness").x :
                                      0.0001f;
          glass->set_roughness(roughness);
          glass->set_thin_film_thickness(node.inputs.contains("thinfilm_thickness") ?
                                             node.inputs.at("thinfilm_thickness") :
                                             600.0f);
          glass->set_thin_film_ior(node.inputs.contains("thinfilm_ior") ?
                                       node.inputs.at("thinfilm_ior") :
                                       1.3f);
          lowered = glass;
        }
        else {
          DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
          if (const auto input = node.color3_inputs.find("color"); input != node.color3_inputs.end()) {
            diffuse->set_color(input->second);
          }
          if (const auto input = node.inputs.find("roughness"); input != node.inputs.end()) {
            const float roughness = node.nodedef == lama_diffuse_id ?
                                        input->second * input->second * 0.5f :
                                        input->second;
            diffuse->set_roughness(roughness);
          }
          if (const auto input = node.inputs.find("weight"); input != node.inputs.end()) {
            diffuse->set_surface_mix_weight(input->second - 1.0f);
          }
          lowered = diffuse;
        }
      }
      else if (node.nodedef == uniform_edf_id || node.nodedef == lama_emission_id) {
        EmissionNode *emission = graph->create_node<EmissionNode>();
        if (const auto input = node.color3_inputs.find("color");
            input != node.color3_inputs.end())
        {
          emission->set_color(input->second);
        }
        emission->set_strength(1.0f);
        lowered = emission;
      }
      else if (node.nodedef == generic_surface_id) {
        ShaderNode *composed = nullptr;
        if (node.links.find("bsdf") != node.links.end() && node.links.find("edf") != node.links.end()) {
          AddClosureNode *add = graph->create_node<AddClosureNode>();
          add->name = node.name + ".add";
          lowered_nodes.emplace(add->name, add);
          composed = add;
        }
        if (node.links.find("opacity") != node.links.end() || node.inputs.find("opacity") != node.inputs.end()) {
          TransparentBsdfNode *transparent = graph->create_node<TransparentBsdfNode>();
          transparent->name = node.name + ".transparent";
          MixClosureNode *mix = graph->create_node<MixClosureNode>();
          mix->name = node.name + ".opacity";
          if (const auto opacity = node.inputs.find("opacity"); opacity != node.inputs.end()) {
            mix->set_fac(opacity->second);
          }
          lowered_nodes.emplace(transparent->name, transparent);
          lowered = mix;
        }
        else if (composed) {
          lowered = composed;
        }
        else {
          lowered = graph->create_node<AddClosureNode>();
        }
        preserve_lowered_name = true;
      }
      else if (node.nodedef == mix_surfaceshader_id) {
        MixClosureNode *mix = graph->create_node<MixClosureNode>();
        mix->name = node.name;
        preserve_lowered_name = true;
        if (const auto input = node.inputs.find("mix"); input != node.inputs.end()) {
          mix->set_fac(input->second);
        }
        lowered = mix;
      }
      else if (node.nodedef == lama_surface_id) {
        if (node.links.contains("materialBack")) {
          GeometryNode *geometry = graph->create_node<GeometryNode>();
          geometry->name = node.name + ".geometry";
          MixClosureNode *side = graph->create_node<MixClosureNode>();
          side->name = node.name + ".side";
          lowered_nodes.emplace(geometry->name, geometry);
          lowered_nodes.emplace(side->name, side);
        }
        if (node.links.find("presence") != node.links.end() ||
            node.inputs.find("presence") != node.inputs.end())
        {
          TransparentBsdfNode *transparent = graph->create_node<TransparentBsdfNode>();
          transparent->name = node.name + ".transparent";
          MixClosureNode *presence = graph->create_node<MixClosureNode>();
          presence->name = node.name;
          if (const auto input = node.inputs.find("presence"); input != node.inputs.end()) {
            presence->set_fac(input->second);
          }
          lowered_nodes.emplace(transparent->name, transparent);
          lowered = presence;
        }
        else if (node.links.contains("materialBack")) {
          lowered = lowered_nodes.at(node.name + ".side");
        }
        else {
          lowered = graph->create_node<AddClosureNode>();
        }
        preserve_lowered_name = true;
      }
      /* mix_bsdf_id/add_bsdf_id are dual-purpose like oren_nayar_diffuse_bsdf_id
       * above -- when SurfaceShader-typed (generic <surface> closure-graph
       * flavor) they need this dedicated lowering; when BSDF-typed they fall
       * through to the generic is_bsdf_combinator dispatch below unchanged.
       * mix_edf_id/add_edf_id have no other flavor and are always
       * SurfaceShader-typed. */
      else if (((node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) &&
                node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
               node.nodedef == mix_edf_id || node.nodedef == lama_mix_edf_id)
      {
        MixClosureNode *mix = graph->create_node<MixClosureNode>();
        mix->name = node.name;
        preserve_lowered_name = true;
        if (const auto input = node.inputs.find("mix"); input != node.inputs.end()) {
          mix->set_fac(input->second);
        }
        lowered = mix;
      }
      else if (((node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) &&
                node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
               node.nodedef == add_edf_id || node.nodedef == lama_add_edf_id)
      {
        if (node.nodedef == lama_add_bsdf_id || node.nodedef == lama_add_edf_id) {
          MixClosureNode *mul1 = graph->create_node<MixClosureNode>();
          mul1->name = node.name + ".weight1";
          mul1->set_fac(node.inputs.at("weight1"));
          TransparentBsdfNode *null1 = graph->create_node<TransparentBsdfNode>();
          null1->name = node.name + ".weight1_null";
          null1->set_color(make_float3(0.0f, 0.0f, 0.0f));
          MixClosureNode *mul2 = graph->create_node<MixClosureNode>();
          mul2->name = node.name + ".weight2";
          mul2->set_fac(node.inputs.at("weight2"));
          TransparentBsdfNode *null2 = graph->create_node<TransparentBsdfNode>();
          null2->name = node.name + ".weight2_null";
          null2->set_color(make_float3(0.0f, 0.0f, 0.0f));
          lowered_nodes.emplace(mul1->name, mul1);
          lowered_nodes.emplace(null1->name, null1);
          lowered_nodes.emplace(mul2->name, mul2);
          lowered_nodes.emplace(null2->name, null2);
        }
        lowered = graph->create_node<AddClosureNode>();
        lowered->name = node.name;
        preserve_lowered_name = true;
      }
      /* multiply_bsdff_id/multiply_bsdfc_id are dual-purpose like
       * mix_bsdf_id/add_bsdf_id above -- see the matching validate() comment.
       * Same MixClosureNode + zero-contribution TransparentBsdfNode idiom as
       * the BSDF-typed is_bsdf_combinator flavor below (w*bsdf via
       * Closure1*(1-fac) + Closure2*fac with Closure1 a null closure and
       * fac=w). multiply_edff_id/multiply_edfc_id have no other flavor (see
       * their validate() comment) and reuse this exact lowering. */
      else if (((node.nodedef == multiply_bsdff_id || node.nodedef == multiply_bsdfc_id) &&
                node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
               node.nodedef == multiply_edff_id || node.nodedef == multiply_edfc_id)
      {
        MixClosureNode *mix = graph->create_node<MixClosureNode>();
        mix->name = node.name;
        preserve_lowered_name = true;
        const bool float_weight = node.nodedef == multiply_bsdff_id ||
                                   node.nodedef == multiply_edff_id;
        mix->set_fac(float_weight ? node.inputs.at("in2") : node.color3_inputs.at("in2").x);
        TransparentBsdfNode *null_bsdf = graph->create_node<TransparentBsdfNode>();
        null_bsdf->name = node.name + ".multiply_null";
        null_bsdf->set_color(make_float3(0.0f, 0.0f, 0.0f));
        lowered_nodes.emplace(null_bsdf->name, null_bsdf);
        lowered = mix;
      }
      else if (node.nodedef == generalized_schlick_edf_id) {
        MixClosureNode *mix = graph->create_node<MixClosureNode>();
        mix->name = node.name;
        preserve_lowered_name = true;
        mix->set_fac(node.color3_inputs.contains("color0") ? node.color3_inputs.at("color0").x : 1.0f);
        TransparentBsdfNode *null_bsdf = graph->create_node<TransparentBsdfNode>();
        null_bsdf->name = node.name + ".directional_null";
        null_bsdf->set_color(make_float3(0.0f, 0.0f, 0.0f));
        lowered_nodes.emplace(null_bsdf->name, null_bsdf);
        lowered = mix;
      }
      else if (node.nodedef == surface_unlit_id) {
        /* Real ND_surface_unlit lowering, mirroring the reference
         * implementation in libraries/stdlib/genosl/mx_surface_unlit.osl:
         *   trans = clamp(transmission, 0, 1)
         *   bsdf = trans * transmission_color * transparent()
         *   edf  = (1 - trans) * emission * emission_color * emission()
         *   opacity = clamp(opacity, 0, 1)
         * composed as add(bsdf, edf), then mixed with a fully-weighted
         * transparent BSDF using MixClosureNode.Fac = opacity -- Cycles'
         * standard closure-tree idiom for cutout alpha (Fac=1 keeps the
         * composed closure fully; Fac=0 yields full transparency).
         * `transmission`/`opacity` are literal-only in this delivery
         * phase: a connected source for either is rejected at admission
         * (usdshade_reader.cpp's surface_unlit branch), not silently
         * mishandled here. */
        const float trans = std::min(
            1.0f,
            std::max(0.0f,
                     node.inputs.count("transmission") ? node.inputs.at("transmission") : 0.0f));
        const float opacity_value = std::min(
            1.0f,
            std::max(0.0f, node.inputs.count("opacity") ? node.inputs.at("opacity") : 1.0f));

        TransparentBsdfNode *transmission_bsdf = graph->create_node<TransparentBsdfNode>();
        transmission_bsdf->name = node.name + ".unlit_transmission";
        if (!node.links.count("transmission_color")) {
          const float3 transmission_color = node.color3_inputs.count("transmission_color") ?
                                                node.color3_inputs.at("transmission_color") :
                                                make_float3(1.0f, 1.0f, 1.0f);
          transmission_bsdf->set_color(transmission_color * trans);
        }
        lowered_nodes.emplace(transmission_bsdf->name, transmission_bsdf);

        EmissionNode *emission_node = graph->create_node<EmissionNode>();
        emission_node->name = node.name + ".unlit_emission";
        if (!node.links.count("emission")) {
          const float emission_weight = node.inputs.count("emission") ?
                                            node.inputs.at("emission") :
                                            1.0f;
          emission_node->set_strength(emission_weight * (1.0f - trans));
        }
        if (!node.links.count("emission_color")) {
          emission_node->set_color(node.color3_inputs.count("emission_color") ?
                                       node.color3_inputs.at("emission_color") :
                                       make_float3(1.0f, 1.0f, 1.0f));
        }
        lowered_nodes.emplace(emission_node->name, emission_node);

        AddClosureNode *sum = graph->create_node<AddClosureNode>();
        sum->name = node.name + ".unlit_sum";
        lowered_nodes.emplace(sum->name, sum);

        TransparentBsdfNode *cutout = graph->create_node<TransparentBsdfNode>();
        cutout->name = node.name + ".unlit_cutout";
        cutout->set_color(make_float3(1.0f, 1.0f, 1.0f));
        lowered_nodes.emplace(cutout->name, cutout);

        MixClosureNode *mix = graph->create_node<MixClosureNode>();
        mix->set_fac(opacity_value);
        lowered = mix;
      }
      else if (is_bsdf_producer(node.nodedef)) {
        /* `weight` is validated literal-only above and folded directly into
         * the color-role socket as a plain multiply -- only when that socket
         * itself is a literal too (a linked color/tint is wired verbatim in
         * the phase-2 connect pass below; scaling a *linked* color by a
         * literal weight would need a real multiply node, which none of
         * these nodedefs need in practice since `weight` defaults to 1). */
        const float weight = node.inputs.contains("weight") ? node.inputs.at("weight") : 1.0f;
        if (node.nodedef == chiang_hair_bsdf_id) {
          PrincipledHairBsdfNode *hair = graph->create_node<PrincipledHairBsdfNode>();
          hair->set_model(NODE_PRINCIPLED_HAIR_CHIANG);
          hair->set_parametrization(NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
          const float2 roughness_r = node.vector2_inputs.contains("roughness_R") ?
                                         node.vector2_inputs.at("roughness_R") :
                                         make_float2(0.1f, 0.1f);
          const float longitudinal = invert_monotonic_roughness(
              roughness_r.x, chiang_longitudinal_variance_from_roughness);
          const float radial = invert_monotonic_roughness(
              roughness_r.y, chiang_azimuthal_scale_from_roughness);
          hair->set_roughness(longitudinal);
          hair->set_radial_roughness(radial);
          hair->set_coat(0.0f);
          hair->set_tint(make_float3(1.0f, 1.0f, 1.0f));
          hair->set_absorption_coefficient(node.vector3_inputs.contains("absorption_coefficient") ?
                                               node.vector3_inputs.at("absorption_coefficient") :
                                               make_float3(0.0f, 0.0f, 0.0f));
          hair->set_offset(node.inputs.contains("cuticle_angle") ?
                               node.inputs.at("cuticle_angle") * M_PI_F - (M_PI_F * 0.5f) :
                               0.0f);
          if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
            hair->set_ior(input->second);
          }
          lowered = hair;
        }
        else if (node.nodedef == oren_nayar_diffuse_bsdf_id || node.nodedef == lama_diffuse_id) {
          DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(0.18f, 0.18f, 0.18f);
            diffuse->set_color(color * weight);
          }
          if (!node.links.contains("roughness")) {
            const float roughness = node.inputs.contains("roughness") ? node.inputs.at("roughness") :
                                                                        0.0f;
            diffuse->set_roughness(node.nodedef == lama_diffuse_id ? roughness * roughness * 0.5f :
                                                                   roughness);
          }
          lowered = diffuse;
        }
        else if (node.nodedef == translucent_bsdf_id || node.nodedef == lama_translucent_id) {
          TranslucentBsdfNode *translucent = graph->create_node<TranslucentBsdfNode>();
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     (node.nodedef == lama_translucent_id ?
                                          make_float3(0.18f, 0.18f, 0.18f) :
                                          make_float3(1.0f, 1.0f, 1.0f));
            translucent->set_color(color * weight);
          }
          lowered = translucent;
        }
        else if (node.nodedef == sheen_bsdf_id) {
          SheenBsdfNode *sheen = graph->create_node<SheenBsdfNode>();
          /* validate() only admits mode="zeltner", Cycles' CLOSURE_BSDF_SHEEN_ID
           * "microfiber" distribution. */
          sheen->set_distribution(CLOSURE_BSDF_SHEEN_ID);
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(1.0f, 1.0f, 1.0f);
            sheen->set_color(color * weight);
          }
          if (!node.links.contains("roughness")) {
            sheen->set_roughness(
                node.inputs.contains("roughness") ? node.inputs.at("roughness") : 0.3f);
          }
          lowered = sheen;
        }
        else if (node.nodedef == subsurface_bsdf_id || node.nodedef == lama_sss_id) {
          SubsurfaceScatteringNode *sss = graph->create_node<SubsurfaceScatteringNode>();
          sss->set_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(0.18f, 0.18f, 0.18f);
            sss->set_color(color * weight);
          }
          if (node.nodedef == lama_sss_id) {
            if (!node.links.contains("sssRadius") && !node.links.contains("sssScale") &&
                !node.links.contains("sssUnitLength"))
            {
              const float3 radius = node.color3_inputs.contains("sssRadius") ?
                                        node.color3_inputs.at("sssRadius") :
                                        make_float3(0.0f, 0.0f, 0.0f);
              const float scale = node.inputs.contains("sssScale") ? node.inputs.at("sssScale") :
                                                                    1.0f;
              const float unit_length = node.inputs.contains("sssUnitLength") ?
                                            node.inputs.at("sssUnitLength") :
                                            0.00328f;
              sss->set_radius(radius * scale * unit_length);
            }
            if (!node.links.contains("sssAnisotropy")) {
              sss->set_subsurface_anisotropy(node.inputs.contains("sssAnisotropy") ?
                                                 node.inputs.at("sssAnisotropy") :
                                                 0.0f);
            }
          }
          else {
            if (!node.links.contains("radius")) {
              sss->set_radius(node.color3_inputs.contains("radius") ?
                                  node.color3_inputs.at("radius") :
                                  make_float3(1.0f, 1.0f, 1.0f));
            }
            if (!node.links.contains("anisotropy")) {
              sss->set_subsurface_anisotropy(
                  node.inputs.contains("anisotropy") ? node.inputs.at("anisotropy") : 0.0f);
            }
          }
          lowered = sss;
        }
        else if (node.nodedef == conductor_bsdf_id) {
          MetallicBsdfNode *metallic = graph->create_node<MetallicBsdfNode>();
          metallic->set_fresnel_type(CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
          metallic->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
          if (!node.links.contains("ior")) {
            metallic->set_ior(node.color3_inputs.contains("ior") ?
                                  node.color3_inputs.at("ior") :
                                  make_float3(0.183f, 0.421f, 1.373f));
          }
          if (!node.links.contains("extinction")) {
            metallic->set_k(node.color3_inputs.contains("extinction") ?
                                node.color3_inputs.at("extinction") :
                                make_float3(3.424f, 2.346f, 1.770f));
          }
          /* Isotropic-only: validate() rejects roughness_x != roughness_y. */
          const float roughness = node.vector2_inputs.contains("roughness") ?
                                      node.vector2_inputs.at("roughness").x :
                                      0.05f;
          metallic->set_roughness(roughness);
          metallic->set_anisotropy(0.0f);
          metallic->set_rotation(0.0f);
          if (!node.links.contains("thinfilm_thickness")) {
            metallic->set_thin_film_thickness(node.inputs.contains("thinfilm_thickness") ?
                                                  node.inputs.at("thinfilm_thickness") :
                                                  0.0f);
          }
          if (!node.links.contains("thinfilm_ior")) {
            metallic->set_thin_film_ior(
                node.inputs.contains("thinfilm_ior") ? node.inputs.at("thinfilm_ior") : 1.5f);
          }
          lowered = metallic;
        }
        else if (node.nodedef == dielectric_bsdf_id) {
          GlassBsdfNode *glass = graph->create_node<GlassBsdfNode>();
          glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
          if (!node.links.contains("tint")) {
            const float3 tint = node.color3_inputs.contains("tint") ?
                                    node.color3_inputs.at("tint") :
                                    make_float3(1.0f, 1.0f, 1.0f);
            glass->set_color(tint * weight);
          }
          if (!node.links.contains("ior")) {
            glass->set_IOR(node.inputs.contains("ior") ? node.inputs.at("ior") : 1.5f);
          }
          /* Isotropic-only: validate() rejects roughness_x != roughness_y. */
          const float roughness = node.vector2_inputs.contains("roughness") ?
                                      node.vector2_inputs.at("roughness").x :
                                      0.05f;
          glass->set_roughness(roughness);
          if (!node.links.contains("thinfilm_thickness")) {
            glass->set_thin_film_thickness(node.inputs.contains("thinfilm_thickness") ?
                                               node.inputs.at("thinfilm_thickness") :
                                               0.0f);
          }
          if (!node.links.contains("thinfilm_ior")) {
            glass->set_thin_film_ior(
                node.inputs.contains("thinfilm_ior") ? node.inputs.at("thinfilm_ior") : 1.5f);
          }
          lowered = glass;
        }
      }
      else if (node.nodedef == usd_preview_surface_id) {
        /* Real ND_UsdPreviewSurface_surfaceshader lowering onto Cycles'
         * PrincipledBsdfNode. Field names are the real
         * ND_UsdPreviewSurface_surfaceshader inputs from the bundled
         * libraries/bxdf/usd_preview_surface.mtlx nodedef -- not reused
         * from open_pbr_surface_id. See usd_preview_surface_id's comment
         * above for which inputs are lowered here versus admitted only at
         * their inert default by usdshade_reader.cpp. */
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
        if (const auto input = node.color3_inputs.find("diffuseColor");
            input != node.color3_inputs.end())
        {
          principled->set_base_color(input->second);
        }
        if (const auto input = node.inputs.find("metallic"); input != node.inputs.end()) {
          principled->set_metallic(input->second);
        }
        if (const auto input = node.inputs.find("roughness"); input != node.inputs.end()) {
          principled->set_roughness(input->second);
        }
        if (const auto input = node.inputs.find("clearcoat"); input != node.inputs.end()) {
          principled->set_coat_weight(input->second);
        }
        if (const auto input = node.inputs.find("clearcoatRoughness");
            input != node.inputs.end())
        {
          principled->set_coat_roughness(input->second);
        }
        if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
          /* The reference nodegraph derives the clearcoat's own F0 from the
           * same base `ior` (via R = (1-ior)/(1+ior), R_sq) rather than a
           * second, independent coat IOR -- ND_UsdPreviewSurface_surfaceshader
           * has no separate clearcoat-IOR input, so reusing `ior` for
           * Principled's Coat IOR is the real, non-proxy correspondence. */
          principled->set_ior(input->second);
          principled->set_coat_ior(input->second);
        }
        if (const auto input = node.color3_inputs.find("emissiveColor");
            input != node.color3_inputs.end())
        {
          /* ND_UsdPreviewSurface_surfaceshader's emissiveColor is direct
           * radiance (fed straight into a uniform_edf in the reference
           * implementation), unlike Principled's separate
           * color * strength product. Emission Strength = 1 makes
           * Principled's product reduce to exactly that radiance. */
          principled->set_emission_color(input->second);
          principled->set_emission_strength(1.0f);
        }
        lowered = principled;
      }
      else if (node.nodedef == gltf_pbr_id) {
        /* Real ND_gltf_pbr_surfaceshader lowering onto Cycles'
         * PrincipledBsdfNode. Field names are the real
         * ND_gltf_pbr_surfaceshader inputs from the bundled
         * libraries/bxdf/gltf_pbr.mtlx nodedef. See gltf_pbr_id's comment
         * above for scope. */
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
        if (const auto input = node.color3_inputs.find("base_color");
            input != node.color3_inputs.end())
        {
          principled->set_base_color(input->second);
        }
        if (const auto input = node.inputs.find("metallic"); input != node.inputs.end()) {
          principled->set_metallic(input->second);
        }
        if (const auto input = node.inputs.find("roughness"); input != node.inputs.end()) {
          principled->set_roughness(input->second);
        }
        if (const auto input = node.inputs.find("clearcoat"); input != node.inputs.end()) {
          principled->set_coat_weight(input->second);
        }
        if (const auto input = node.inputs.find("clearcoat_roughness");
            input != node.inputs.end())
        {
          principled->set_coat_roughness(input->second);
        }
        if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
          /* The reference nodegraph's clearcoat_bsdf uses a fixed literal
           * ior=1.5 (not the base `ior` input) for the clearcoat lobe
           * itself, but derives the base dielectric F0 from `ior`. Cycles'
           * Principled has a single shared Coat IOR; reusing the base
           * `ior` here (rather than hardcoding 1.5) keeps the base layer's
           * real IOR faithful and only diverges from the reference for the
           * coat lobe when a document authors a non-default `ior` together
           * with a non-zero `clearcoat` -- documented, not silently forced. */
          principled->set_ior(input->second);
          principled->set_coat_ior(input->second);
        }
        if (const auto input = node.color3_inputs.find("emissive");
            input != node.color3_inputs.end())
        {
          principled->set_emission_color(input->second);
          /* ND_gltf_pbr_surfaceshader's real default for emissive_strength
           * is 1.0 (not Principled's own socket default of 0.0) -- set it
           * explicitly so an authored `emissive` with an unauthored
           * `emissive_strength` still reproduces the real default product,
           * then let an authored value below override it. */
          principled->set_emission_strength(1.0f);
        }
        if (const auto input = node.inputs.find("emissive_strength"); input != node.inputs.end())
        {
          principled->set_emission_strength(input->second);
        }
        lowered = principled;
      }
      else if (node.nodedef == disney_principled_id) {
        /* Real ND_disney_principled lowering onto Cycles' PrincipledBsdfNode.
         * Field names are the real ND_disney_principled inputs from the
         * bundled libraries/bxdf/disney_principled.mtlx nodedef. See
         * disney_principled_id's comment above for the field-by-field
         * correspondence. */
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
        if (const auto input = node.color3_inputs.find("baseColor");
            input != node.color3_inputs.end())
        {
          principled->set_base_color(input->second);
        }
        if (const auto input = node.inputs.find("metallic"); input != node.inputs.end()) {
          principled->set_metallic(input->second);
        }
        if (const auto input = node.inputs.find("roughness"); input != node.inputs.end()) {
          principled->set_roughness(input->second);
        }
        if (const auto input = node.inputs.find("anisotropic"); input != node.inputs.end()) {
          principled->set_anisotropic(input->second);
        }
        if (const auto input = node.inputs.find("specular"); input != node.inputs.end()) {
          principled->set_specular_ior_level(input->second);
        }
        if (const auto input = node.inputs.find("ior"); input != node.inputs.end()) {
          principled->set_ior(input->second);
        }
        if (const auto input = node.inputs.find("sheen"); input != node.inputs.end()) {
          principled->set_sheen_weight(input->second);
        }
        if (const auto input = node.inputs.find("clearcoat"); input != node.inputs.end()) {
          principled->set_coat_weight(input->second);
        }
        if (const auto input = node.inputs.find("specTrans"); input != node.inputs.end()) {
          principled->set_transmission_weight(input->second);
        }
        if (const auto input = node.inputs.find("subsurface"); input != node.inputs.end()) {
          principled->set_subsurface_weight(input->second);
        }
        if (const auto input = node.color3_inputs.find("subsurfaceDistance");
            input != node.color3_inputs.end())
        {
          principled->set_subsurface_radius(input->second);
        }

        /* specularTint/sheenTint: NG_disney_principled's dielectric_tint and
         * sheen_color nodes are each a real mix(white, baseColor, tint) --
         * lowered here via a real Cycles MixNode rather than approximated
         * as a 1:1 socket value. Both auxiliary MixNodes are registered in
         * lowered_nodes under their own name so the connect phase below can
         * still wire a connected baseColor/specularTint/sheenTint source
         * into them. */
        MixNode *specular_tint = graph->create_node<MixNode>();
        specular_tint->name = node.name + ".disney_specular_tint";
        specular_tint->set_mix_type(NODE_MIX_BLEND);
        specular_tint->set_color1(make_float3(1.0f, 1.0f, 1.0f));
        if (const auto input = node.color3_inputs.find("baseColor");
            input != node.color3_inputs.end())
        {
          specular_tint->set_color2(input->second);
        }
        if (const auto input = node.inputs.find("specularTint"); input != node.inputs.end()) {
          specular_tint->set_fac(input->second);
        }
        graph->connect(specular_tint->output("Color"), principled->input("Specular Tint"));
        lowered_nodes.emplace(specular_tint->name, specular_tint);

        MixNode *sheen_tint = graph->create_node<MixNode>();
        sheen_tint->name = node.name + ".disney_sheen_tint";
        sheen_tint->set_mix_type(NODE_MIX_BLEND);
        sheen_tint->set_color1(make_float3(1.0f, 1.0f, 1.0f));
        if (const auto input = node.color3_inputs.find("baseColor");
            input != node.color3_inputs.end())
        {
          sheen_tint->set_color2(input->second);
        }
        if (const auto input = node.inputs.find("sheenTint"); input != node.inputs.end()) {
          sheen_tint->set_fac(input->second);
        }
        graph->connect(sheen_tint->output("Color"), principled->input("Sheen Tint"));
        lowered_nodes.emplace(sheen_tint->name, sheen_tint);

        /* clearcoatGloss is disney's inverse-roughness knob (0=rough,
         * 1=glossy; NG_disney_principled's coat_roughness = invert(
         * clearcoatGloss)), with no native Cycles equivalent socket --
         * lowered via a real Cycles MathNode computing 1-clearcoatGloss
         * into Coat Roughness. */
        MathNode *coat_roughness = graph->create_node<MathNode>();
        coat_roughness->name = node.name + ".disney_coat_roughness";
        coat_roughness->set_math_type(NODE_MATH_SUBTRACT);
        coat_roughness->set_value1(1.0f);
        if (const auto input = node.inputs.find("clearcoatGloss"); input != node.inputs.end()) {
          coat_roughness->set_value2(input->second);
        }
        graph->connect(coat_roughness->output("Value"), principled->input("Coat Roughness"));
        lowered_nodes.emplace(coat_roughness->name, coat_roughness);

        lowered = principled;
      }
      else if (is_bsdf_combinator(node.nodedef)) {
        if (node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) {
          if (node.nodedef == lama_add_bsdf_id) {
            MixClosureNode *mul1 = graph->create_node<MixClosureNode>();
            mul1->name = node.name + ".weight1";
            mul1->set_fac(node.inputs.at("weight1"));
            TransparentBsdfNode *null1 = graph->create_node<TransparentBsdfNode>();
            null1->name = node.name + ".weight1_null";
            null1->set_color(make_float3(0.0f, 0.0f, 0.0f));
            MixClosureNode *mul2 = graph->create_node<MixClosureNode>();
            mul2->name = node.name + ".weight2";
            mul2->set_fac(node.inputs.at("weight2"));
            TransparentBsdfNode *null2 = graph->create_node<TransparentBsdfNode>();
            null2->name = node.name + ".weight2_null";
            null2->set_color(make_float3(0.0f, 0.0f, 0.0f));
            lowered_nodes.emplace(mul1->name, mul1);
            lowered_nodes.emplace(null1->name, null1);
            lowered_nodes.emplace(mul2->name, mul2);
            lowered_nodes.emplace(null2->name, null2);
          }
          AddClosureNode *add = graph->create_node<AddClosureNode>();
          lowered = add;
        }
        else if (node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) {
          /* mix(bg, fg, mixValue) per
           * libraries/pbrlib/genglsl/mx_mix_bsdf.glsl: result = mix(bg, fg,
           * mixValue) = bg*(1-mixValue) + fg*mixValue. Cycles'
           * svm_node_mix_closure() weights Closure1 by (1-fac) and Closure2
           * by fac -- so Closure1=bg, Closure2=fg, Fac=mix (wired in the
           * connect phase below). */
          MixClosureNode *mix = graph->create_node<MixClosureNode>();
          if (!node.links.contains("mix")) {
            mix->set_fac(node.inputs.at("mix"));
          }
          lowered = mix;
        }
        else if (node.nodedef == multiply_bsdff_id || node.nodedef == multiply_bsdfc_id) {
          /* Scale a BSDF by a scalar weight w via
           * MixClosureNode: Closure1*(1-fac) + Closure2*fac. With Closure1
           * a physically-zero closure (a TransparentBsdfNode with color
           * (0,0,0) -- the same "zero-contribution closure" idiom already
           * used for ND_surface_unlit's transmission_bsdf above) and fac=w,
           * this is exactly 0*(1-w) + bsdf*w = w*bsdf. multiply_bsdfC's
           * literal uniform-channel tint is validated equal to a scalar
           * (R==G==B) and used the same way. */
          MixClosureNode *mix = graph->create_node<MixClosureNode>();
          mix->set_fac(node.nodedef == multiply_bsdff_id ?
                          node.inputs.at("in2") :
                          node.color3_inputs.at("in2").x);
          TransparentBsdfNode *null_bsdf = graph->create_node<TransparentBsdfNode>();
          null_bsdf->name = node.name + ".multiply_null";
          null_bsdf->set_color(make_float3(0.0f, 0.0f, 0.0f));
          lowered_nodes.emplace(null_bsdf->name, null_bsdf);
          lowered = mix;
        }
      }
      else {
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
      if (const auto input = node.color3_inputs.find("base_color");
          input != node.color3_inputs.end())
      {
        principled->set_base_color(input->second);
      }
      if (const auto input = node.inputs.find("base_weight"); input != node.inputs.end()) {
        /* ShaderGraph::finalize() adds its implicit unit closure weight. Store the delta so
         * the finalized closure weight equals the MaterialX base_weight. */
        principled->set_surface_mix_weight(input->second - 1.0f);
      }
      if (const auto input = node.inputs.find("base_metalness"); input != node.inputs.end()) {
        principled->set_metallic(input->second);
      }
      if (const auto input = node.inputs.find("specular_roughness"); input != node.inputs.end()) {
        principled->set_roughness(input->second);
      }
      if (const auto input = node.inputs.find("specular_ior"); input != node.inputs.end()) {
        principled->set_ior(input->second);
      }
      if (const auto input = node.inputs.find("geometry_opacity"); input != node.inputs.end()) {
        principled->set_alpha(input->second);
      }
      if (const auto input = node.color3_inputs.find("emission_color");
          input != node.color3_inputs.end())
      {
        principled->set_emission_color(input->second);
      }
      if (const auto input = node.inputs.find("emission_luminance"); input != node.inputs.end()) {
        principled->set_emission_strength(input->second);
      }
      if (const auto input = node.inputs.find("coat_weight"); input != node.inputs.end()) {
        principled->set_coat_weight(input->second);
      }
      if (const auto input = node.color3_inputs.find("coat_color");
          input != node.color3_inputs.end())
      {
        principled->set_coat_tint(input->second);
      }
      if (const auto input = node.inputs.find("coat_roughness"); input != node.inputs.end()) {
        principled->set_coat_roughness(input->second);
      }
      if (const auto input = node.inputs.find("coat_ior"); input != node.inputs.end()) {
        principled->set_coat_ior(input->second);
      }
      if (const auto input = node.inputs.find("fuzz_weight"); input != node.inputs.end()) {
        principled->set_sheen_weight(input->second);
      }
      if (const auto input = node.color3_inputs.find("fuzz_color");
          input != node.color3_inputs.end())
      {
        principled->set_sheen_tint(input->second);
      }
      if (const auto input = node.inputs.find("fuzz_roughness"); input != node.inputs.end()) {
        principled->set_sheen_roughness(input->second);
      }
      lowered = principled;
      }
    }
    if (!preserve_lowered_name) {
      lowered->name = node.name;
    }
    lowered_nodes.emplace(node.name, lowered);
  }

  for (const Node &node : source.nodes) {
    if (is_exact_color_burn_dodge(node.nodedef)) {
      const bool burn = node.nodedef == burn_color3_id;
      ShaderNode *foreground = lowered_nodes.at(node.name + ".foreground");
      ShaderNode *background = lowered_nodes.at(node.name + ".background");
      ShaderNode *combine = lowered_nodes.at(node.name);
      const auto fg_link = node.links.find("fg");
      const auto bg_link = node.links.find("bg");
      const auto mix_link = node.links.find("mix");
      if (fg_link != node.links.end()) {
        graph->connect(lowered_output(fg_link->second, nodes_by_name, lowered_nodes),
                       foreground->input("Color"));
      }
      if (bg_link != node.links.end()) {
        graph->connect(lowered_output(bg_link->second, nodes_by_name, lowered_nodes),
                       background->input("Color"));
      }
      for (const char *channel : {"Red", "Green", "Blue"}) {
        const string prefix = node.name + "." + channel + ".";
        ShaderOutput *fg = fg_link == node.links.end() ? nullptr : foreground->output(channel);
        ShaderOutput *bg = bg_link == node.links.end() ? nullptr : background->output(channel);
        ShaderNode *foreground_term = lowered_nodes.at(
            prefix + (burn ? "foreground_abs" : "denominator"));
        ShaderNode *denominator_abs = burn ? nullptr :
                                             lowered_nodes.at(prefix + "denominator_abs");
        ShaderNode *condition = lowered_nodes.at(prefix + "condition");
        ShaderNode *safe_denominator = burn ?
                                           lowered_nodes.at(prefix + "safe_denominator") :
                                           nullptr;
        ShaderNode *one_minus_background = burn ?
                                               lowered_nodes.at(prefix +
                                                                "one_minus_background") :
                                               nullptr;
        ShaderNode *divide = lowered_nodes.at(prefix + "divide");
        ShaderNode *blend = burn ? lowered_nodes.at(prefix + "blend") : divide;
        ShaderNode *mix_product = lowered_nodes.at(prefix + "mix_product");
        ShaderNode *one_minus_mix = lowered_nodes.at(prefix + "one_minus_mix");
        ShaderNode *background_product = lowered_nodes.at(prefix + "background_product");
        ShaderNode *sum = lowered_nodes.at(prefix + "sum");
        ShaderNode *inverse_condition = lowered_nodes.at(prefix + "inverse_condition");
        ShaderNode *result = lowered_nodes.at(prefix + "result");

        if (burn) {
          if (fg) {
            graph->connect(fg, foreground_term->input("Value1"));
            graph->connect(fg, safe_denominator->input("Value1"));
          }
          if (bg) {
            graph->connect(bg, one_minus_background->input("Value2"));
          }
          graph->connect(foreground_term->output("Value"), condition->input("Value1"));
          graph->connect(condition->output("Value"), safe_denominator->input("Value2"));
          graph->connect(safe_denominator->output("Value"), divide->input("Value2"));
          graph->connect(one_minus_background->output("Value"), divide->input("Value1"));
          graph->connect(divide->output("Value"), blend->input("Value2"));
        }
        else {
          if (fg) {
            graph->connect(fg, foreground_term->input("Value2"));
          }
          if (bg) {
            graph->connect(bg, divide->input("Value1"));
          }
          graph->connect(foreground_term->output("Value"), denominator_abs->input("Value1"));
          graph->connect(denominator_abs->output("Value"), condition->input("Value1"));
          graph->connect(foreground_term->output("Value"), divide->input("Value2"));
        }
        if (bg) {
          graph->connect(bg, background_product->input("Value2"));
        }
        if (mix_link != node.links.end()) {
          ShaderOutput *mix = lowered_output(mix_link->second, nodes_by_name, lowered_nodes);
          graph->connect(mix, mix_product->input("Value2"));
          graph->connect(mix, one_minus_mix->input("Value2"));
        }
        graph->connect(blend->output("Value"), mix_product->input("Value1"));
        graph->connect(one_minus_mix->output("Value"), background_product->input("Value1"));
        graph->connect(mix_product->output("Value"), sum->input("Value1"));
        graph->connect(background_product->output("Value"), sum->input("Value2"));
        graph->connect(condition->output("Value"), inverse_condition->input("Value2"));
        graph->connect(sum->output("Value"), result->input("Value1"));
        graph->connect(inverse_condition->output("Value"), result->input("Value2"));
        graph->connect(result->output("Value"), combine->input(channel));
      }
      continue;
    }
    if (scalar_blend_type(node.nodedef, nullptr) || color_blend_type(node.nodedef, nullptr)) {
      MixColorNode *blend = static_cast<MixColorNode *>(lowered_nodes.at(node.name));
      const auto connect_if_linked = [&](const char *input, ShaderInput *socket) {
        if (const auto link = node.links.find(input); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      connect_if_linked("bg", blend->input("A"));
      connect_if_linked("fg", blend->input("B"));
      connect_if_linked("mix", blend->input("Factor"));
      continue;
    }
    if (is_mix(node.nodedef)) {
      const Type value_type = mix_value_type(node.nodedef);
      ShaderNode *delta = lowered_nodes.at(node.name + ".delta");
      ShaderNode *factor = value_type == Type::Float ||
                                   mix_factor_type(node.nodedef) == Type::Color3 ?
                               nullptr :
                               lowered_nodes.at(node.name + ".factor");
      ShaderNode *product = lowered_nodes.at(node.name + ".product");
      ShaderNode *sum = lowered_nodes.at(node.name);
      const auto connect_if_linked = [&](const char *input, ShaderInput *socket) {
        if (const auto link = node.links.find(input); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (value_type == Type::Float) {
        connect_if_linked("fg", delta->input("Value1"));
        connect_if_linked("bg", delta->input("Value2"));
        graph->connect(delta->output("Value"), product->input("Value1"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) graph->connect(lowered_output(mix->second, nodes_by_name, lowered_nodes), product->input("Value2"));
        connect_if_linked("bg", sum->input("Value1"));
        graph->connect(product->output("Value"), sum->input("Value2"));
      }
      else if (value_type == Type::Color3) {
        connect_if_linked("fg", delta->input("Color1")); connect_if_linked("bg", delta->input("Color2"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) {
          ShaderOutput *mix_output = lowered_output(mix->second, nodes_by_name, lowered_nodes);
          if (mix_factor_type(node.nodedef) == Type::Color3) {
            graph->connect(mix_output, product->input("Color2"));
          }
          else {
            graph->connect(mix_output, factor->input("Red"));
            graph->connect(mix_output, factor->input("Green"));
            graph->connect(mix_output, factor->input("Blue"));
          }
        }
        graph->connect(delta->output("Color"), product->input("Color1"));
        if (mix_factor_type(node.nodedef) == Type::Float) {
          graph->connect(factor->output("Color"), product->input("Color2"));
        }
        connect_if_linked("bg", sum->input("Color1")); graph->connect(product->output("Color"), sum->input("Color2"));
      }
      else {
        connect_if_linked("fg", delta->input("Vector1")); connect_if_linked("bg", delta->input("Vector2"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) {
          ShaderOutput *mix_output = lowered_output(mix->second, nodes_by_name, lowered_nodes);
          graph->connect(mix_output, factor->input("X")); graph->connect(mix_output, factor->input("Y")); graph->connect(mix_output, factor->input("Z"));
        }
        graph->connect(delta->output("Vector"), product->input("Vector1")); graph->connect(factor->output("Vector"), product->input("Vector2"));
        connect_if_linked("bg", sum->input("Vector1")); graph->connect(product->output("Vector"), sum->input("Vector2"));
      }
      continue;
    }
    NodeMathType unused_math_type;
    if (is_safepower_float(node.nodedef)) {
      ShaderNode *absolute = lowered_nodes.at(node.name + ".abs");
      ShaderNode *sign = lowered_nodes.at(node.name + ".sign");
      ShaderNode *power = lowered_nodes.at(node.name + ".power");
      ShaderNode *multiply = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in1"); input != node.links.end()) {
        ShaderOutput *source = lowered_output(input->second, nodes_by_name, lowered_nodes);
        graph->connect(source, absolute->input("Value1"));
        graph->connect(source, sign->input("Value1"));
      }
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), power->input("Value2"));
      }
      graph->connect(absolute->output("Value"), power->input("Value1"));
      graph->connect(sign->output("Value"), multiply->input("Value1"));
      graph->connect(power->output("Value"), multiply->input("Value2"));
      continue;
    }

    if (scalar_math_type(node.nodedef, &unused_math_type)) {
      ShaderNode *math = lowered_nodes.at(node.name);
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      const bool is_atan2 = node.nodedef == atan2_float_id;
      if (const auto input = node.links.find(
              is_unary ? "in" : (node.nodedef == invert_float_id ? "amount" : (is_atan2 ? "iny" : "in1")));
          input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       math->input("Value1"));
      }
      if (!is_unary) {
        if (const auto input = node.links.find(node.nodedef == invert_float_id ? "in" : (is_atan2 ? "inx" : "in2"));
            input != node.links.end())
        {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         math->input("Value2"));
        }
      }
      continue;
    }

    if (is_inside_outside(node.nodedef)) {
      const Type value_type = inside_outside_type(node.nodedef);
      const bool outside = is_outside(node.nodedef);
      ShaderOutput *mask = nullptr;
      if (outside) {
        MathNode *one_minus_mask = static_cast<MathNode *>(lowered_nodes.at(node.name + ".mask"));
        if (const auto link = node.links.find("mask"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), one_minus_mask->input("Value2"));
        }
        mask = one_minus_mask->output("Value");
      }
      else if (const auto link = node.links.find("mask"); link != node.links.end()) {
        mask = lowered_output(link->second, nodes_by_name, lowered_nodes);
      }
      if (value_type == Type::Float) {
        MathNode *multiply = static_cast<MathNode *>(lowered_nodes.at(node.name));
        if (const auto link = node.links.find("in"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), multiply->input("Value1"));
        }
        if (mask) {
          graph->connect(mask, multiply->input("Value2"));
        }
      }
      else {
        ShaderNode *mask_color = lowered_nodes.at(node.name + ".mask_color");
        ShaderNode *multiply = lowered_nodes.at(node.name);
        if (mask) {
          for (const char *channel : {"Red", "Green", "Blue"}) {
            graph->connect(mask, mask_color->input(channel));
          }
        }
        graph->connect(mask_color->output("Color"), multiply->input("Color2"));
        if (const auto link = node.links.find("in"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), multiply->input("Color1"));
          if (value_type == Type::Color4) {
            graph->connect(lowered_color4_alpha_output(link->second, nodes_by_name, lowered_nodes),
                           lowered_nodes.at(node.name + ".Alpha")->input("Value1"));
          }
        }
        if (value_type == Type::Color4) {
          if (mask) {
            graph->connect(mask, lowered_nodes.at(node.name + ".Alpha")->input("Value2"));
          }
        }
      }
      continue;
    }

    if (is_vector2_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MixVectorNode *mix = static_cast<MixVectorNode *>(lowered_nodes.at(node.name));
      const auto connect_predicate = [&](const char *input_name, ShaderInput *socket) {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (node.nodedef == ifgreatereq_vector2_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        connect_predicate("value1", greater->input("Value1"));
        connect_predicate("value1", equal->input("Value1"));
        connect_predicate("value2", greater->input("Value2"));
        connect_predicate("value2", equal->input("Value2"));
        graph->connect(greater->output("Value"), condition->input("Value1"));
        graph->connect(equal->output("Value"), condition->input("Value2"));
      }
      else {
        connect_predicate("value1", condition->input("Value1"));
        connect_predicate("value2", condition->input("Value2"));
      }
      if (const auto link = node.links.find("in1"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("B"));
      }
      if (const auto link = node.links.find("in2"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("A"));
      }
      graph->connect(condition->output("Value"), mix->input("Factor"));
      continue;
    }

    if (is_vector4_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MixVectorNode *mix = static_cast<MixVectorNode *>(lowered_nodes.at(node.name));
      MathNode *w_delta = static_cast<MathNode *>(lowered_nodes.at(node.name + ".W.delta"));
      MathNode *w_product = static_cast<MathNode *>(lowered_nodes.at(node.name + ".W.product"));
      MathNode *w_sum = static_cast<MathNode *>(lowered_nodes.at(node.name + ".W"));
      const auto connect_predicate = [&](const char *input_name, ShaderInput *socket) {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (node.nodedef == ifgreatereq_vector4_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        connect_predicate("value1", greater->input("Value1"));
        connect_predicate("value1", equal->input("Value1"));
        connect_predicate("value2", greater->input("Value2"));
        connect_predicate("value2", equal->input("Value2"));
        graph->connect(greater->output("Value"), condition->input("Value1"));
        graph->connect(equal->output("Value"), condition->input("Value2"));
      }
      else {
        connect_predicate("value1", condition->input("Value1"));
        connect_predicate("value2", condition->input("Value2"));
      }
      if (const auto link = node.links.find("in1"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("B"));
        graph->connect(lowered_vector4_w_output(link->second, nodes_by_name, lowered_nodes),
                       w_delta->input("Value1"));
      }
      if (const auto link = node.links.find("in2"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("A"));
        graph->connect(lowered_vector4_w_output(link->second, nodes_by_name, lowered_nodes),
                       w_delta->input("Value2"));
        graph->connect(lowered_vector4_w_output(link->second, nodes_by_name, lowered_nodes),
                       w_sum->input("Value1"));
      }
      graph->connect(condition->output("Value"), mix->input("Factor"));
      graph->connect(w_delta->output("Value"), w_product->input("Value1"));
      graph->connect(condition->output("Value"), w_product->input("Value2"));
      graph->connect(w_product->output("Value"), w_sum->input("Value2"));
      continue;
    }

    if (is_color4_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MixNode *mix = static_cast<MixNode *>(lowered_nodes.at(node.name));
      MathNode *alpha_delta = static_cast<MathNode *>(lowered_nodes.at(node.name + ".Alpha.delta"));
      MathNode *alpha_product = static_cast<MathNode *>(lowered_nodes.at(node.name + ".Alpha.product"));
      MathNode *alpha_sum = static_cast<MathNode *>(lowered_nodes.at(node.name + ".Alpha"));
      const auto connect_predicate = [&](const char *input_name, ShaderInput *socket) {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (node.nodedef == ifgreatereq_color4_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        connect_predicate("value1", greater->input("Value1"));
        connect_predicate("value1", equal->input("Value1"));
        connect_predicate("value2", greater->input("Value2"));
        connect_predicate("value2", equal->input("Value2"));
        graph->connect(greater->output("Value"), condition->input("Value1"));
        graph->connect(equal->output("Value"), condition->input("Value2"));
      }
      else {
        connect_predicate("value1", condition->input("Value1"));
        connect_predicate("value2", condition->input("Value2"));
      }
      if (const auto link = node.links.find("in1"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("Color2"));
        graph->connect(lowered_color4_alpha_output(link->second, nodes_by_name, lowered_nodes),
                       alpha_delta->input("Value1"));
      }
      if (const auto link = node.links.find("in2"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("Color1"));
        graph->connect(lowered_color4_alpha_output(link->second, nodes_by_name, lowered_nodes),
                       alpha_delta->input("Value2"));
        graph->connect(lowered_color4_alpha_output(link->second, nodes_by_name, lowered_nodes),
                       alpha_sum->input("Value1"));
      }
      graph->connect(condition->output("Value"), mix->input("Fac"));
      graph->connect(alpha_delta->output("Value"), alpha_product->input("Value1"));
      graph->connect(condition->output("Value"), alpha_product->input("Value2"));
      graph->connect(alpha_product->output("Value"), alpha_sum->input("Value2"));
      continue;
    }

    if (is_float_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MathNode *delta = static_cast<MathNode *>(lowered_nodes.at(node.name + ".delta"));
      MathNode *product = static_cast<MathNode *>(lowered_nodes.at(node.name + ".product"));
      MathNode *sum = static_cast<MathNode *>(lowered_nodes.at(node.name));
      const auto connect_operand = [&](const char *input_name, ShaderInput *socket) {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (node.nodedef == ifgreatereq_float_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        connect_operand("value1", greater->input("Value1"));
        connect_operand("value1", equal->input("Value1"));
        connect_operand("value2", greater->input("Value2"));
        connect_operand("value2", equal->input("Value2"));
        graph->connect(greater->output("Value"), condition->input("Value1"));
        graph->connect(equal->output("Value"), condition->input("Value2"));
      }
      else {
        connect_operand("value1", condition->input("Value1"));
        connect_operand("value2", condition->input("Value2"));
      }
      connect_operand("in1", delta->input("Value1"));
      connect_operand("in2", delta->input("Value2"));
      connect_operand("in2", sum->input("Value1"));
      graph->connect(delta->output("Value"), product->input("Value1"));
      graph->connect(condition->output("Value"), product->input("Value2"));
      graph->connect(product->output("Value"), sum->input("Value2"));
      continue;
    }

    if (node.nodedef == convert_float_color3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      graph->connect(input, combine->input("Red"));
      graph->connect(input, combine->input("Green"));
      graph->connect(input, combine->input("Blue"));
      continue;
    }

    if (is_contrast_float(node.nodedef)) {
      ShaderNode *subtract = lowered_nodes.at(node.name + ".subtract");
      ShaderNode *multiply = lowered_nodes.at(node.name + ".multiply");
      ShaderNode *add = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       subtract->input("Value1"));
      }
      graph->connect(subtract->output("Value"), multiply->input("Value1"));
      graph->connect(multiply->output("Value"), add->input("Value1"));
      continue;
    }

    if (node.nodedef == convert_boolean_float_id || node.nodedef == convert_integer_float_id) {
      ShaderNode *convert = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     convert->input("Value1"));
      continue;
    }

    if (node.nodedef == rgbtohsv_color3_id || node.nodedef == hsvtorgb_color3_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      for (const char *channel : {"Red", "Green", "Blue"}) {
        graph->connect(separate->output(channel), combine->input(channel));
      }
      continue;
    }

    if (node.nodedef == clamp_float_id) {
      ShaderNode *clamp = lowered_nodes.at(node.name);
      for (const auto &[input_name, socket] :
           {std::pair{"in", "Value"}, std::pair{"low", "Min"}, std::pair{"high", "Max"}})
      {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         clamp->input(socket));
        }
      }
      continue;
    }

    if (is_smoothstep_float(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Value"));
      }
      continue;
    }

    if (is_linear_range_float(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Value"));
      }
      continue;
    }

    if (is_linear_range_color3(node.nodedef)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       separate->input("Color"));
        for (const char *channel : {"Red", "Green", "Blue"}) {
          graph->connect(separate->output(channel),
                         lowered_nodes.at(node.name + "." + channel)->input("Value"));
        }
      }
      for (const char *channel : {"Red", "Green", "Blue"}) {
        graph->connect(lowered_nodes.at(node.name + "." + channel)->output("Result"),
                       combine->input(channel));
      }
      continue;
    }

    if (is_contrast_vector2(node.nodedef) || is_contrast_vector3(node.nodedef)) {
      const bool vector2 = is_contrast_vector2(node.nodedef);
      ShaderNode *input = lowered_nodes.at(node.name + ".input");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto source = node.links.find("in"); source != node.links.end()) {
        graph->connect(lowered_output(source->second, nodes_by_name, lowered_nodes),
                       input->input("Vector"));
      }
      for (const char *channel : {"X", "Y", "Z"}) {
        if (vector2 && channel[0] == 'Z') {
          continue;
        }
        ShaderNode *subtract = lowered_nodes.at(node.name + "." + channel + ".subtract");
        ShaderNode *multiply = lowered_nodes.at(node.name + "." + channel + ".multiply");
        ShaderNode *add = lowered_nodes.at(node.name + "." + channel);
        graph->connect(input->output(channel), subtract->input("Value1"));
        graph->connect(subtract->output("Value"), multiply->input("Value1"));
        graph->connect(multiply->output("Value"), add->input("Value1"));
        graph->connect(add->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (is_linear_range_vector2(node.nodedef) || is_linear_range_vector3(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Vector"));
      }
      continue;
    }

    if (node.nodedef == clamp_vector2_id) {
      ShaderNode *minimum = lowered_nodes.at(node.name + ".minimum");
      ShaderNode *maximum = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       minimum->input("Vector1"));
      }
      graph->connect(minimum->output("Vector"), maximum->input("Vector1"));
      continue;
    }

    if (node.nodedef == clamp_vector2fa_id) {
      ShaderNode *minimum = lowered_nodes.at(node.name + ".minimum");
      ShaderNode *maximum = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       minimum->input("Vector1"));
      }
      graph->connect(minimum->output("Vector"), maximum->input("Vector1"));
      continue;
    }

    if (node.nodedef == clamp_vector3_id || node.nodedef == clamp_vector3fa_id) {
      ShaderNode *minimum = lowered_nodes.at(node.name + ".minimum");
      ShaderNode *maximum = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       minimum->input("Vector1"));
      }
      graph->connect(minimum->output("Vector"), maximum->input("Vector1"));
      continue;
    }

    if (is_native_noise_or_fractal_family(node.nodedef)) {
      const bool is_fractal = is_native_fractal2d_family(node.nodedef) ||
                              is_native_fractal3d_family(node.nodedef);
      const bool vector2 = native_noise_or_fractal_is_vector2(node.nodedef);
      const bool is_color4 = native_noise_or_fractal_is_color4(node.nodedef);
      const bool is_vector4 = native_noise_or_fractal_output_type(node.nodedef) == Type::Vector4;
      ShaderNode *noise = lowered_nodes.at(node.name + ".noise");
      const char *coordinate_name = native_noise_or_fractal_is_3d(node.nodedef) ? "position" :
                                                                             "texcoord";
      ShaderOutput *coordinate = lowered_output(
          node.links.at(coordinate_name), nodes_by_name, lowered_nodes);
      graph->connect(coordinate, noise->input("Vector"));
      if (native_noise_or_fractal_is_float(node.nodedef)) {
        ShaderNode *amplitude = lowered_nodes.at(node.name + ".amplitude");
        graph->connect(noise->output("Fac"), amplitude->input("Value1"));
        if (!is_fractal) {
          ShaderNode *pivot = lowered_nodes.at(node.name);
          graph->connect(amplitude->output("Value"), pivot->input("Value1"));
        }
        continue;
      }
      if (native_noise_or_fractal_is_color3(node.nodedef)) {
        ShaderNode *amplitude = lowered_nodes.at(node.name + ".amplitude");
        graph->connect(noise->output("Color"), amplitude->input("Color1"));
        if (!is_fractal) {
          ShaderNode *pivot = lowered_nodes.at(node.name);
          graph->connect(amplitude->output("Color"), pivot->input("Color1"));
        }
        continue;
      }
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(noise->output("Color"), separate->input("Color"));
      if (is_color4 || is_vector4) {
        ShaderNode *offset = lowered_nodes.at(node.name + ".offset.separate");
        ShaderNode *fourth_noise = lowered_nodes.at(node.name + ".W.noise");
        ShaderNode *w_amplitude = lowered_nodes.at(node.name + ".W.amplitude");
        graph->connect(coordinate, offset->input("Vector"));
        graph->connect(offset->output("X"), fourth_noise->input("Vector"));
        graph->connect(fourth_noise->output("Fac"), w_amplitude->input("Value1"));
        if (!is_fractal) {
          ShaderNode *w_pivot = lowered_nodes.at(node.name + (is_color4 ? ".Alpha" : ".W"));
          graph->connect(w_amplitude->output("Value"), w_pivot->input("Value1"));
        }
      }
      for (const auto &[channel, source] : {std::pair{"X", "Red"},
                                           std::pair{"Y", "Green"},
                                           std::pair{"Z", "Blue"}})
      {
        if (vector2 && channel[0] == 'Z') {
          continue;
        }
        ShaderNode *amplitude = lowered_nodes.at(node.name + "." + channel + ".amplitude");
        graph->connect(separate->output(source), amplitude->input("Value1"));
        if (is_fractal) {
          graph->connect(amplitude->output("Value"),
                         combine->input(is_color4 ? source : channel));
        }
        else {
          ShaderNode *pivot = lowered_nodes.at(node.name + "." + channel);
          graph->connect(amplitude->output("Value"), pivot->input("Value1"));
          graph->connect(pivot->output("Value"), combine->input(is_color4 ? source : channel));
        }
      }
      continue;
    }

    if (node.nodedef == checkerboard_color3_id) {
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     lowered_nodes.at(node.name)->input("Vector"));
      continue;
    }

    if (const CellNoiseSpec *spec = cellnoise_spec(node.nodedef)) {
      ShaderNode *floor = lowered_nodes.at(node.name + ".floor");
      graph->connect(lowered_output(node.links.at(spec->input_name), nodes_by_name, lowered_nodes),
                     floor->input("Vector1"));
      graph->connect(floor->output("Vector"), lowered_nodes.at(node.name)->input("Vector"));
      continue;
    }

    if (is_luminance_color3(node.nodedef)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name + ".vector");
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("X"));
      graph->connect(separate->output("Green"), combine->input("Y"));
      graph->connect(separate->output("Blue"), combine->input("Z"));
      graph->connect(combine->output("Vector"), lowered_nodes.at(node.name)->input("Vector1"));
      continue;
    }

    if (value_dot_type(node.nodedef, nullptr)) {
      continue;
    }

    if (is_space_transform(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Vector"));
      }
      continue;
    }

    NodeMix unused_mix_type;
    if (color_scalar_mix_type(node.nodedef, &unused_mix_type)) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      ShaderNode *broadcast = lowered_nodes.at(node.name + ".scalar");
      graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                     mix->input("Color1"));
      ShaderOutput *scalar = lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes);
      for (const char *channel : {"Red", "Green", "Blue"}) {
        graph->connect(scalar, broadcast->input(channel));
      }
      graph->connect(broadcast->output("Color"), mix->input("Color2"));
      continue;
    }
    if (color_mix_type(node.nodedef, &unused_mix_type)) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      const bool is_invert = node.nodedef == invert_color3_id ||
                             node.nodedef == invert_color3fa_id;
      if (node.nodedef == invert_color3fa_id) {
        ShaderNode *broadcast = lowered_nodes.at(node.name + ".scalar");
        if (const auto amount=node.links.find("amount"); amount!=node.links.end()) {
          ShaderOutput *output=lowered_output(amount->second,nodes_by_name,lowered_nodes);
          for (const char *channel:{"Red","Green","Blue"}) graph->connect(output,broadcast->input(channel));
        }
        graph->connect(broadcast->output("Color"),mix->input("Color1"));
      }
      else {
        graph->connect(lowered_output(node.links.at(is_invert ? "amount" : "in1"), nodes_by_name, lowered_nodes),
                       mix->input("Color1"));
      }
      if (const auto input=node.links.find(is_invert ? "in" : "in2"); input!=node.links.end()) graph->connect(lowered_output(input->second,nodes_by_name,lowered_nodes),mix->input("Color2"));
      continue;
    }

    if (node.nodedef == extract_color3_id) {
      ShaderNode *separate = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      continue;
    }

    if (color_unary_math_type(node.nodedef, nullptr)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), separate->input("Color"));
      for (const char *channel : {"Red", "Green", "Blue"}) {
        ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
        graph->connect(separate->output(channel), math->input("Value1"));
        graph->connect(math->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (is_color_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MixNode *mix = static_cast<MixNode *>(lowered_nodes.at(node.name));
      if (node.nodedef == ifgreatereq_color3_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        graph->connect(greater->output("Value"), condition->input("Value1"));
        graph->connect(equal->output("Value"), condition->input("Value2"));
        for (const char *input : {"value1", "value2"}) {
          if (const auto link = node.links.find(input); link != node.links.end()) {
            graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), greater->input(input == string("value1") ? "Value1" : "Value2"));
            graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), equal->input(input == string("value1") ? "Value1" : "Value2"));
          }
        }
      }
      else {
        if (const auto link = node.links.find("value1"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), condition->input("Value1"));
        if (const auto link = node.links.find("value2"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), condition->input("Value2"));
      }
      if (const auto link = node.links.find("in1"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("Color2"));
      if (const auto link = node.links.find("in2"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), mix->input("Color1"));
      graph->connect(condition->output("Value"), mix->input("Fac"));
      continue;
    }

    if (is_vector_conditional(node.nodedef)) {
      MathNode *condition = static_cast<MathNode *>(lowered_nodes.at(node.name + ".condition"));
      MixVectorNode *mix = static_cast<MixVectorNode *>(lowered_nodes.at(node.name));
      if (node.nodedef == ifgreatereq_vector3_id) {
        MathNode *greater = static_cast<MathNode *>(lowered_nodes.at(node.name + ".greater"));
        MathNode *equal = static_cast<MathNode *>(lowered_nodes.at(node.name + ".equal"));
        graph->connect(greater->output("Value"), condition->input("Value1")); graph->connect(equal->output("Value"), condition->input("Value2"));
        for (const char *name : {"value1", "value2"}) if (const auto link=node.links.find(name); link!=node.links.end()) { graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), greater->input(name == string("value1") ? "Value1" : "Value2")); graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), equal->input(name == string("value1") ? "Value1" : "Value2")); }
      }
      else { if (const auto link=node.links.find("value1"); link!=node.links.end()) graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), condition->input("Value1")); if (const auto link=node.links.find("value2"); link!=node.links.end()) graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), condition->input("Value2")); }
      if (const auto link=node.links.find("in1"); link!=node.links.end()) graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), mix->input("B"));
      if (const auto link=node.links.find("in2"); link!=node.links.end()) graph->connect(lowered_output(link->second,nodes_by_name,lowered_nodes), mix->input("A"));
      graph->connect(condition->output("Value"), mix->input("Factor"));
      continue;
    }

    if (color_binary_component_math_type(node.nodedef, nullptr)) {
      const bool scalar_second = color_binary_component_math_uses_scalar_second(node.nodedef);
      ShaderNode *first = lowered_nodes.at(node.name + ".first"); ShaderNode *second = scalar_second ? nullptr : lowered_nodes.at(node.name + ".second"); ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes), first->input("Color"));
      if (second) graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes), second->input("Color"));
      for (const char *channel : {"Red", "Green", "Blue"}) { ShaderNode *math = lowered_nodes.at(node.name + "." + channel); graph->connect(first->output(channel), math->input("Value1")); if (second) graph->connect(second->output(channel), math->input("Value2")); else graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes), math->input("Value2")); graph->connect(math->output("Value"), combine->input(channel)); }
      continue;
    }
    if (node.nodedef == clamp_color3_id || node.nodedef == clamp_color3fa_id) {
      ShaderNode *input = lowered_nodes.at(node.name + ".input"); ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto source = node.links.find("in"); source != node.links.end()) graph->connect(lowered_output(source->second, nodes_by_name, lowered_nodes), input->input("Color"));
      for (const char *channel : {"Red", "Green", "Blue"}) { ShaderNode *minimum=lowered_nodes.at(node.name+"."+channel+".minimum"), *maximum=lowered_nodes.at(node.name+"."+channel+".maximum"); graph->connect(input->output(channel),minimum->input("Value1")); graph->connect(minimum->output("Value"),maximum->input("Value1")); graph->connect(maximum->output("Value"),combine->input(channel)); }
      continue;
    }
    if (is_safepower_color3(node.nodedef)) {
      const bool scalar_exponent=safepower_color3_uses_scalar_exponent(node.nodedef);
      ShaderNode *first=lowered_nodes.at(node.name+".first"),*second=scalar_exponent?nullptr:lowered_nodes.at(node.name+".second"),*combine=lowered_nodes.at(node.name);
      const bool first_link=node.links.contains("in1"),second_link=node.links.contains("in2");
      if(first_link) graph->connect(lowered_output(node.links.at("in1"),nodes_by_name,lowered_nodes),first->input("Color"));
      if(second && second_link) graph->connect(lowered_output(node.links.at("in2"),nodes_by_name,lowered_nodes),second->input("Color"));
      for(const char *c:{"Red","Green","Blue"}) { ShaderNode *abs=lowered_nodes.at(node.name+"."+c+".abs"),*sign=lowered_nodes.at(node.name+"."+c+".sign"),*power=lowered_nodes.at(node.name+"."+c+".power"),*multiply=lowered_nodes.at(node.name+"."+c+".multiply"); if(first_link){graph->connect(first->output(c),abs->input("Value1"));graph->connect(first->output(c),sign->input("Value1"));} graph->connect(abs->output("Value"),power->input("Value1")); if(second_link) graph->connect(scalar_exponent?lowered_output(node.links.at("in2"),nodes_by_name,lowered_nodes):second->output(c),power->input("Value2")); graph->connect(sign->output("Value"),multiply->input("Value1")); graph->connect(power->output("Value"),multiply->input("Value2")); graph->connect(multiply->output("Value"),combine->input(c)); }
      continue;
    }

    if (node.nodedef == convert_color3_vector3_id || node.nodedef == convert_color3_vector2_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("X"));
      graph->connect(separate->output("Green"), combine->input("Y"));
      if (node.nodedef == convert_color3_vector3_id) graph->connect(separate->output("Blue"), combine->input("Z"));
      continue;
    }
    if (node.nodedef == convert_vector4_vector3_id) {
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     lowered_nodes.at(node.name + ".separate")->input("Vector"));
      continue;
    }
    if (node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id ||
        node.nodedef == convert_vector4_color3_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), separate->input("Vector"));
      graph->connect(separate->output("X"), combine->input("Red"));
      graph->connect(separate->output("Y"), combine->input("Green"));
      if (node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector4_color3_id) graph->connect(separate->output("Z"), combine->input("Blue"));
      continue;
    }
    if (node.nodedef == convert_boolean_color3_id || node.nodedef == convert_integer_color3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      graph->connect(input, combine->input("Red"));
      graph->connect(input, combine->input("Green"));
      graph->connect(input, combine->input("Blue"));
      continue;
    }
    if (node.nodedef == convert_float_color4_id || node.nodedef == convert_boolean_color4_id ||
        node.nodedef == convert_integer_color4_id || node.nodedef == convert_float_vector4_id ||
        node.nodedef == convert_boolean_vector4_id || node.nodedef == convert_integer_vector4_id)
    {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      const bool vector4 = node.nodedef == convert_float_vector4_id ||
                           node.nodedef == convert_boolean_vector4_id ||
                           node.nodedef == convert_integer_vector4_id;
      graph->connect(input, combine->input(vector4 ? "X" : "Red"));
      graph->connect(input, combine->input(vector4 ? "Y" : "Green"));
      graph->connect(input, combine->input(vector4 ? "Z" : "Blue"));
      if (vector4) {
        const Node &scalar_source = *nodes_by_name.at(node.links.at("in").source_node);
        static_cast<ValueNode *>(lowered_nodes.at(node.name + ".W"))->set_value(
            scalar_source.nodedef == constant_float_id ?
                static_cast<ValueNode *>(lowered_nodes.at(node.links.at("in").source_node))->get_value() :
                (scalar_source.nodedef == constant_boolean_id ?
                     (static_cast<MixNode *>(lowered_nodes.at(node.links.at("in").source_node))
                              ->get_use_clamp() ?
                          1.0f :
                          0.0f) :
                     float(static_cast<MagicTextureNode *>(
                               lowered_nodes.at(node.links.at("in").source_node))
                               ->get_depth())));
      }
      else {
        /* Color4 scalar adapters preserve their existing sidecar-alpha lowering. */
      }
      continue;
    }
    if (node.nodedef == combine2_color4cf_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".input");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("Red"));
      graph->connect(separate->output("Green"), combine->input("Green"));
      graph->connect(separate->output("Blue"), combine->input("Blue"));
      continue;
    }
    if (node.nodedef == combine4_color4_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderNode *alpha = lowered_nodes.at(node.name + ".Alpha");
      for (const auto &[input_name, socket_name] : {std::pair{"in1", "Red"},
                                                    std::pair{"in2", "Green"},
                                                    std::pair{"in3", "Blue"}})
      {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         combine->input(socket_name));
        }
      }
      if (const auto input = node.links.find("in4"); input != node.links.end()) {
        (void)alpha;
      }
      continue;
    }
    if (node.nodedef == convert_float_vector3_id || node.nodedef == convert_float_vector2_id ||
        node.nodedef == convert_boolean_vector3_id || node.nodedef == convert_boolean_vector2_id ||
        node.nodedef == convert_integer_vector3_id || node.nodedef == convert_integer_vector2_id)
    {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      graph->connect(input, combine->input("X"));
      graph->connect(input, combine->input("Y"));
      if (node.nodedef == convert_float_vector3_id || node.nodedef == convert_boolean_vector3_id ||
          node.nodedef == convert_integer_vector3_id)
      {
        graph->connect(input, combine->input("Z"));
      }
      continue;
    }
    if (node.nodedef == convert_vector2_vector3_id || node.nodedef == convert_vector4_vector2_id) {
      ShaderNode *separate=lowered_nodes.at(node.name+".separate"), *combine=lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"),nodes_by_name,lowered_nodes),separate->input("Vector")); graph->connect(separate->output("X"),combine->input("X")); graph->connect(separate->output("Y"),combine->input("Y")); continue;
    }
    if (node.nodedef == combine2_vector4vf_id) {
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in1"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       first->input("Vector"));
      }
      graph->connect(first->output("X"), combine->input("X"));
      graph->connect(first->output("Y"), combine->input("Y"));
      graph->connect(first->output("Z"), combine->input("Z"));
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name + ".W")->input("Value1"));
      }
      continue;
    }
    if (node.nodedef == combine2_vector4vv_id) {
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *second = lowered_nodes.at(node.name + ".second");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in1"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       first->input("Vector"));
      }
      graph->connect(first->output("X"), combine->input("X"));
      graph->connect(first->output("Y"), combine->input("Y"));
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       second->input("Vector"));
      }
      graph->connect(second->output("X"), combine->input("Z"));
      graph->connect(second->output("Y"), lowered_nodes.at(node.name + ".W")->input("Value1"));
      continue;
    }
    if (node.nodedef == combine4_vector4_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[input_name, socket_name] : {std::pair{"in1", "X"},
                                                    std::pair{"in2", "Y"},
                                                    std::pair{"in3", "Z"}})
      {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         combine->input(socket_name));
        }
      }
      if (const auto input = node.links.find("in4"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name + ".W")->input("Value1"));
      }
      continue;
    }
    if (is_vector4_math_or_clamp(node.nodedef)) {
      const bool clamp = node.nodedef == clamp_vector4_id || node.nodedef == clamp_vector4fa_id;
      const bool scalar_second = vector4_math_uses_scalar_second(node.nodedef);
      if (clamp) {
        ShaderNode *minimum = lowered_nodes.at(node.name + ".minimum");
        ShaderNode *maximum = lowered_nodes.at(node.name);
        ShaderNode *w_minimum = lowered_nodes.at(node.name + ".W.minimum");
        ShaderNode *w_maximum = lowered_nodes.at(node.name + ".W");
        if (const auto input = node.links.find("in"); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         minimum->input("Vector1"));
          graph->connect(lowered_nodes.at(input->second.source_node + ".W")->output("Value"),
                         w_minimum->input("Value1"));
        }
        graph->connect(minimum->output("Vector"), maximum->input("Vector1"));
        graph->connect(w_minimum->output("Value"), w_maximum->input("Value1"));
      }
      else {
        ShaderNode *math = lowered_nodes.at(node.name);
        ShaderNode *w = lowered_nodes.at(node.name + ".W");
        if (const auto input = node.links.find("in1"); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         math->input("Vector1"));
          graph->connect(lowered_nodes.at(input->second.source_node + ".W")->output("Value"),
                         w->input("Value1"));
        }
        if (const auto input = node.links.find("in2"); input != node.links.end()) {
          if (scalar_second) {
            ShaderOutput *value = lowered_output(input->second, nodes_by_name, lowered_nodes);
            graph->connect(value, math->input("Vector2"));
            graph->connect(value, w->input("Value2"));
          }
          else {
            graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                           math->input("Vector2"));
            graph->connect(lowered_nodes.at(input->second.source_node + ".W")->output("Value"),
                           w->input("Value2"));
          }
        }
      }
      continue;
    }
    if (node.nodedef == separate4_vector4_id) {
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     lowered_nodes.at(node.name)->input("Vector"));
      continue;
    }
    if (node.nodedef == convert_color3_vector4_id || node.nodedef == convert_vector2_vector4_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      const bool color3_source = node.nodedef == convert_color3_vector4_id;
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input(color3_source ? "Color" : "Vector"));
      graph->connect(separate->output(color3_source ? "Red" : "X"), combine->input("X"));
      graph->connect(separate->output(color3_source ? "Green" : "Y"), combine->input("Y"));
      if (color3_source) {
        graph->connect(separate->output("Blue"), combine->input("Z"));
      }
      continue;
    }
    if (node.nodedef == convert_color4_vector2_id || node.nodedef == convert_color4_vector3_id ||
        node.nodedef == convert_color4_vector4_id)
    {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("X"));
      graph->connect(separate->output("Green"), combine->input("Y"));
      if (node.nodedef != convert_color4_vector2_id) {
        graph->connect(separate->output("Blue"), combine->input("Z"));
      }
      if (node.nodedef == convert_color4_vector4_id) {
        ShaderNode *w = lowered_nodes.at(node.name + ".W");
        graph->connect(lowered_color4_alpha_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                       w->input("Value1"));
      }
      continue;
    }
    if (node.nodedef == convert_vector4_color4_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderNode *alpha = lowered_nodes.at(node.name + ".Alpha");
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Vector"));
      graph->connect(separate->output("X"), combine->input("Red"));
      graph->connect(separate->output("Y"), combine->input("Green"));
      graph->connect(separate->output("Z"), combine->input("Blue"));
      graph->connect(lowered_nodes.at(node.links.at("in").source_node + ".W")->output("Value"),
                     alpha->input("Value1"));
      continue;
    }
    if (is_color4_operation(node.nodedef)) {
      const bool unary = color4_unary_math_type(node.nodedef, nullptr);
      const bool scalar_invert = node.nodedef == invert_color4fa_id;
      const bool invert = is_color4_invert(node.nodedef);
      const bool safepower = is_safepower_color4(node.nodedef);
      const bool scalar_second = color4_binary_uses_scalar_second(node.nodedef);
      const bool full_clamp = node.nodedef == clamp_color4_id;
      const bool scalar_clamp = node.nodedef == clamp_color4fa_id;
      const bool clamp = full_clamp || scalar_clamp;
      const char *first_name = (unary || scalar_invert || clamp) ?
                                   "in" :
                                   (invert ? "amount" : "in1");
      const char *second_name = scalar_invert ? "amount" : (invert ? "in" : "in2");
      ShaderNode *first = lowered_nodes.at(
          node.name + ((unary || scalar_invert || clamp) ?
                           ".input" :
                           (invert ? ".amount" : ".first")));
      ShaderNode *second = (unary || scalar_second || clamp) ?
                               nullptr :
                               lowered_nodes.at(node.name +
                                                (invert ? ".input" : ".second"));
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find(first_name); link != node.links.end()) {
        graph->connect(
            lowered_output(link->second, nodes_by_name, lowered_nodes), first->input("Color"));
      }
      if (node.nodedef == clamp_color4_id) {
        for (const char *bound : {"low", "high"}) {
          if (const auto link = node.links.find(bound); link != node.links.end()) {
            graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                           lowered_nodes.at(node.name + "." + bound)->input("Color"));
          }
        }
      }
      if (!unary && !clamp) {
        if (const auto link = node.links.find(second_name); link != node.links.end() && second) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         second->input("Color"));
        }
      }
      for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
        const bool alpha = channel[0] == 'A';
        ShaderOutput *first_output = nullptr;
        ShaderOutput *second_output = nullptr;
        if (const auto value = node.float4_inputs.find(first_name);
            value != node.float4_inputs.end())
        {
          if (clamp) {
            static_cast<MathNode *>(
                lowered_nodes.at(node.name + "." + channel + ".minimum"))
                ->set_value1(color4_channel_value(value->second, channel));
          }
          else if (scalar_invert) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                ->set_value2(color4_channel_value(value->second, channel));
          }
          else if (!safepower) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                ->set_value1(color4_channel_value(value->second, channel));
          }
          else {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".abs"))
                ->set_value1(color4_channel_value(value->second, channel));
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".sign"))
                ->set_value1(color4_channel_value(value->second, channel));
          }
        }
        else if (const auto link = node.links.find(first_name); link != node.links.end()) {
          first_output = alpha ?
                             lowered_color4_alpha_output(
                                 link->second, nodes_by_name, lowered_nodes) :
                             first->output(channel);
        }
        else if (clamp) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".minimum"))
              ->set_value1(0.0f);
        }
        else if (scalar_invert) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
              ->set_value2(0.0f);
        }
        else if (!safepower) {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
              ->set_value1(invert ? 1.0f : 0.0f);
        }
        else {
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".abs"))
              ->set_value1(0.0f);
          static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".sign"))
              ->set_value1(0.0f);
        }
        if (!unary && !clamp) {
          if (const auto scalar = node.inputs.find(second_name); scalar != node.inputs.end()) {
            if (scalar_invert) {
              static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                  ->set_value1(scalar->second);
            }
            else if (safepower) {
              static_cast<MathNode *>(
                  lowered_nodes.at(node.name + "." + channel + ".power"))
                  ->set_value2(scalar->second);
            }
            else {
              static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                  ->set_value2(scalar->second);
            }
          }
          else if (const auto value = node.float4_inputs.find(second_name);
              value != node.float4_inputs.end())
          {
            if (!safepower) {
              static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                  ->set_value2(color4_channel_value(value->second, channel));
            }
            else {
              static_cast<MathNode *>(
                  lowered_nodes.at(node.name + "." + channel + ".power"))
                  ->set_value2(color4_channel_value(value->second, channel));
            }
          }
          else if (const auto link = node.links.find(second_name); link != node.links.end()) {
            second_output = scalar_second ?
                                lowered_output(link->second, nodes_by_name, lowered_nodes) :
                                (alpha ?
                                     lowered_color4_alpha_output(
                                         link->second, nodes_by_name, lowered_nodes) :
                                     second->output(channel));
          }
          else if (safepower) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel + ".power"))
                ->set_value2(1.0f);
          }
          else if (scalar_invert) {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                ->set_value1(1.0f);
          }
          else {
            static_cast<MathNode *>(lowered_nodes.at(node.name + "." + channel))
                ->set_value2(color4_binary_uses_identity_second(node.nodedef) ? 1.0f : 0.0f);
          }
        }
        if (clamp) {
          ShaderNode *minimum = lowered_nodes.at(node.name + "." + channel + ".minimum");
          ShaderNode *maximum = lowered_nodes.at(node.name + "." + channel + ".maximum");
          if (first_output) {
            graph->connect(first_output, minimum->input("Value1"));
          }
          if (const auto high = node.inputs.find("high"); high != node.inputs.end()) {
            static_cast<MathNode *>(minimum)->set_value2(high->second);
          }
          else if (const auto high_color = node.float4_inputs.find("high");
                   high_color != node.float4_inputs.end())
          {
            static_cast<MathNode *>(minimum)->set_value2(
                color4_channel_value(high_color->second, channel));
          }
          else if (const auto high_link = node.links.find("high");
                   high_link != node.links.end())
          {
            graph->connect(alpha ? lowered_color4_alpha_output(high_link->second,
                                                               nodes_by_name,
                                                               lowered_nodes) :
                                   lowered_nodes.at(node.name + ".high")->output(channel),
                           minimum->input("Value2"));
          }
          else {
            static_cast<MathNode *>(minimum)->set_value2(1.0f);
          }
          graph->connect(minimum->output("Value"), maximum->input("Value1"));
          if (const auto low = node.inputs.find("low"); low != node.inputs.end()) {
            static_cast<MathNode *>(maximum)->set_value2(low->second);
          }
          else if (const auto low_color = node.float4_inputs.find("low");
                   low_color != node.float4_inputs.end())
          {
            static_cast<MathNode *>(maximum)->set_value2(
                color4_channel_value(low_color->second, channel));
          }
          else if (const auto low_link = node.links.find("low"); low_link != node.links.end()) {
            graph->connect(alpha ? lowered_color4_alpha_output(low_link->second,
                                                              nodes_by_name,
                                                              lowered_nodes) :
                                  lowered_nodes.at(node.name + ".low")->output(channel),
                           maximum->input("Value2"));
          }
          else {
            static_cast<MathNode *>(maximum)->set_value2(0.0f);
          }
          if (alpha) {
            continue;
          }
          graph->connect(maximum->output("Value"), combine->input(channel));
        }
        else if (safepower) {
          ShaderNode *abs = lowered_nodes.at(node.name + "." + channel + ".abs");
          ShaderNode *sign = lowered_nodes.at(node.name + "." + channel + ".sign");
          ShaderNode *power = lowered_nodes.at(node.name + "." + channel + ".power");
          ShaderNode *multiply = lowered_nodes.at(node.name + "." + channel + ".multiply");
          if (first_output) {
            graph->connect(first_output, abs->input("Value1"));
            graph->connect(first_output, sign->input("Value1"));
          }
          if (second_output) {
            graph->connect(second_output, power->input("Value2"));
          }
          graph->connect(abs->output("Value"), power->input("Value1"));
          graph->connect(sign->output("Value"), multiply->input("Value1"));
          graph->connect(power->output("Value"), multiply->input("Value2"));
          if (alpha) {
            continue;
          }
          graph->connect(multiply->output("Value"), combine->input(channel));
        }
        else {
          ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
          if (first_output) {
            graph->connect(first_output,
                           math->input(scalar_invert ? "Value2" : "Value1"));
          }
          if (second_output) {
            graph->connect(second_output,
                           math->input(scalar_invert ? "Value1" : "Value2"));
          }
          if (alpha) {
            continue;
          }
          graph->connect(math->output("Value"), combine->input(channel));
        }
      }
      continue;
    }

    if (node.nodedef == combine3_color3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[input, channel] : {std::pair{"in1", "Red"}, std::pair{"in2", "Green"}, std::pair{"in3", "Blue"}}) {
        if (const auto link = node.links.find(input); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), combine->input(channel));
      }
      continue;
    }
    if (node.nodedef == separate3_color3_id) {
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), lowered_nodes.at(node.name)->input("Color"));
      continue;
    }
    if (node.nodedef == separate4_color4_id) {
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     lowered_nodes.at(node.name)->input("Color"));
      continue;
    }

    if (node.nodedef == extract_vector3_id || node.nodedef == extract_vector2_id) {
      ShaderNode *separate = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Vector"));
      continue;
    }

    if (node.nodedef == separate3_vector3_id) {
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     lowered_nodes.at(node.name)->input("Vector"));
      continue;
    }

    if (node.nodedef == combine3_vector3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[component, socket] :
           {std::pair{"in1", "X"}, std::pair{"in2", "Y"}, std::pair{"in3", "Z"}})
      {
        if (const auto input = node.links.find(component); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         combine->input(socket));
        }
      }
      continue;
    }

    if (node.nodedef == combine2_vector2_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[component, socket] : {std::pair{"in1", "X"}, std::pair{"in2", "Y"}}) {
        if (const auto input = node.links.find(component); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), combine->input(socket));
        }
      }
      continue;
    }

    if (node.nodedef == roughness_anisotropy_id || node.nodedef == glossiness_anisotropy_id) {
      /* Wires the chain built above -- see roughness_anisotropy_id's
       * declaration comment for the real formula/citation. */
      const bool is_glossiness = node.nodedef == glossiness_anisotropy_id;
      const char *first_name = is_glossiness ? "glossiness" : "roughness";
      ShaderNode *roughness_sqr_mul = lowered_nodes.at(node.name + ".roughness_sqr.multiply");
      ShaderNode *roughness_sqr = lowered_nodes.at(node.name + ".roughness_sqr");
      ShaderNode *anisotropy_clamped = lowered_nodes.at(node.name + ".anisotropy_clamped");
      ShaderNode *one_minus_anisotropy = lowered_nodes.at(node.name + ".one_minus_anisotropy");
      ShaderNode *aspect = lowered_nodes.at(node.name + ".aspect");
      ShaderNode *x_divide = lowered_nodes.at(node.name + ".x.divide");
      ShaderNode *x_min = lowered_nodes.at(node.name + ".x");
      ShaderNode *y_multiply = lowered_nodes.at(node.name + ".y");
      ShaderNode *combine = lowered_nodes.at(node.name);

      if (is_glossiness) {
        ShaderNode *invert = lowered_nodes.at(node.name + ".invert1");
        if (const auto link = node.links.find(first_name); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         invert->input("Value2"));
        }
        graph->connect(invert->output("Value"), roughness_sqr_mul->input("Value1"));
        graph->connect(invert->output("Value"), roughness_sqr_mul->input("Value2"));
      }
      else if (const auto link = node.links.find(first_name); link != node.links.end()) {
        ShaderOutput *roughness = lowered_output(link->second, nodes_by_name, lowered_nodes);
        graph->connect(roughness, roughness_sqr_mul->input("Value1"));
        graph->connect(roughness, roughness_sqr_mul->input("Value2"));
      }
      graph->connect(roughness_sqr_mul->output("Value"), roughness_sqr->input("Value"));

      if (const auto link = node.links.find("anisotropy"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       anisotropy_clamped->input("Value"));
      }
      graph->connect(anisotropy_clamped->output("Result"), one_minus_anisotropy->input("Value2"));
      graph->connect(one_minus_anisotropy->output("Value"), aspect->input("Value1"));
      graph->connect(roughness_sqr->output("Result"), x_divide->input("Value1"));
      graph->connect(aspect->output("Value"), x_divide->input("Value2"));
      graph->connect(x_divide->output("Value"), x_min->input("Value1"));
      graph->connect(roughness_sqr->output("Result"), y_multiply->input("Value1"));
      graph->connect(aspect->output("Value"), y_multiply->input("Value2"));
      graph->connect(x_min->output("Value"), combine->input("X"));
      graph->connect(y_multiply->output("Value"), combine->input("Y"));
      continue;
    }

    if (node.nodedef == open_pbr_anisotropy_id) {
      ShaderNode *aniso_invert = lowered_nodes.at(node.name + ".aniso_invert");
      ShaderNode *aniso_invert_sq = lowered_nodes.at(node.name + ".aniso_invert_sq");
      ShaderNode *denom = lowered_nodes.at(node.name + ".denom");
      ShaderNode *fraction = lowered_nodes.at(node.name + ".fraction");
      ShaderNode *sqrt = lowered_nodes.at(node.name + ".sqrt");
      ShaderNode *rough_sq = lowered_nodes.at(node.name + ".rough_sq");
      ShaderNode *alpha_x = lowered_nodes.at(node.name + ".alpha_x");
      ShaderNode *alpha_y = lowered_nodes.at(node.name + ".alpha_y");
      ShaderNode *combine = lowered_nodes.at(node.name);

      if (const auto link = node.links.find("anisotropy"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       aniso_invert->input("Value2"));
      }
      graph->connect(aniso_invert->output("Value"), aniso_invert_sq->input("Value1"));
      graph->connect(aniso_invert->output("Value"), aniso_invert_sq->input("Value2"));
      graph->connect(aniso_invert_sq->output("Value"), denom->input("Value1"));
      graph->connect(denom->output("Value"), fraction->input("Value2"));
      graph->connect(fraction->output("Value"), sqrt->input("Value1"));

      if (const auto link = node.links.find("roughness"); link != node.links.end()) {
        ShaderOutput *roughness = lowered_output(link->second, nodes_by_name, lowered_nodes);
        graph->connect(roughness, rough_sq->input("Value1"));
        graph->connect(roughness, rough_sq->input("Value2"));
      }
      graph->connect(rough_sq->output("Value"), alpha_x->input("Value1"));
      graph->connect(sqrt->output("Value"), alpha_x->input("Value2"));
      graph->connect(aniso_invert->output("Value"), alpha_y->input("Value1"));
      graph->connect(alpha_x->output("Value"), alpha_y->input("Value2"));
      graph->connect(alpha_x->output("Value"), combine->input("X"));
      graph->connect(alpha_y->output("Value"), combine->input("Y"));
      continue;
    }

    if (node.nodedef == blackbody_id) {
      if (const auto temperature = node.links.find("temperature"); temperature != node.links.end()) {
        graph->connect(lowered_output(temperature->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Temperature"));
      }
      continue;
    }

    if (node.nodedef == roughness_dual_id) {
      /* Wires the chain built above -- see roughness_dual_id's declaration
       * comment for the real formula/citation. */
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *x_sqr = lowered_nodes.at(node.name + ".x.multiply");
      ShaderNode *x_clamp = lowered_nodes.at(node.name + ".x");
      ShaderNode *y_sqr = lowered_nodes.at(node.name + ".y.multiply");
      ShaderNode *y_clamp = lowered_nodes.at(node.name + ".y_clamp");
      ShaderNode *condition = lowered_nodes.at(node.name + ".condition");
      ShaderNode *delta = lowered_nodes.at(node.name + ".y.delta");
      ShaderNode *product = lowered_nodes.at(node.name + ".y.product");
      ShaderNode *y_select = lowered_nodes.at(node.name + ".y");
      ShaderNode *combine = lowered_nodes.at(node.name);

      if (const auto link = node.links.find("roughness"); link != node.links.end()) {
        ShaderOutput *roughness = lowered_output(link->second, nodes_by_name, lowered_nodes);
        graph->connect(roughness, separate->input("Vector"));
        graph->connect(separate->output("X"), x_sqr->input("Value1"));
        graph->connect(separate->output("X"), x_sqr->input("Value2"));
        graph->connect(separate->output("Y"), y_sqr->input("Value1"));
        graph->connect(separate->output("Y"), y_sqr->input("Value2"));
        graph->connect(separate->output("Y"), condition->input("Value1"));
      }

      graph->connect(x_sqr->output("Value"), x_clamp->input("Value"));
      graph->connect(y_sqr->output("Value"), y_clamp->input("Value"));
      graph->connect(x_clamp->output("Result"), delta->input("Value1"));
      graph->connect(y_clamp->output("Result"), delta->input("Value2"));
      graph->connect(condition->output("Value"), product->input("Value1"));
      graph->connect(delta->output("Value"), product->input("Value2"));
      graph->connect(product->output("Value"), y_select->input("Value1"));
      graph->connect(y_clamp->output("Result"), y_select->input("Value2"));
      graph->connect(x_clamp->output("Result"), combine->input("X"));
      graph->connect(y_select->output("Value"), combine->input("Y"));
      continue;
    }

    if (node.nodedef == convert_vector3_vector2_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), separate->input("Vector"));
      graph->connect(separate->output("X"), combine->input("X"));
      graph->connect(separate->output("Y"), combine->input("Y"));
      continue;
    }

    if (node.nodedef == place2d_vector2_id) {
      ShaderNode *pivot = lowered_nodes.at(node.name + ".pivot");
      ShaderNode *scale = lowered_nodes.at(node.name + ".scale");
      ShaderNode *offset = lowered_nodes.at(node.name + ".offset");
      ShaderNode *subpivot = lowered_nodes.at(node.name + ".subpivot");
      ShaderNode *applyscale = lowered_nodes.at(node.name + ".applyscale");
      ShaderNode *applyoffset = lowered_nodes.at(node.name + ".applyoffset");
      ShaderNode *applyoffset2 = lowered_nodes.at(node.name + ".applyoffset2");
      ShaderNode *applyscale2 = lowered_nodes.at(node.name + ".applyscale2");
      ShaderNode *addpivot = lowered_nodes.at(node.name + ".addpivot");
      ShaderNode *addpivot2 = lowered_nodes.at(node.name + ".addpivot2");
      ShaderNode *radians = lowered_nodes.at(node.name + ".radians");
      ShaderNode *rotate = lowered_nodes.at(node.name + ".rotate");
      ShaderNode *rotate2 = lowered_nodes.at(node.name + ".rotate2");
      ShaderNode *operation = lowered_nodes.at(node.name);
      ShaderOutput *texcoord = lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes);
      graph->connect(texcoord, subpivot->input("Vector1")); graph->connect(pivot->output("Vector"), subpivot->input("Vector2"));
      graph->connect(subpivot->output("Vector"), applyscale->input("Vector1")); graph->connect(scale->output("Vector"), applyscale->input("Vector2"));
      graph->connect(applyscale->output("Vector"), rotate->input("Vector")); graph->connect(radians->output("Value"), rotate->input("Angle"));
      graph->connect(rotate->output("Vector"), applyoffset->input("Vector1")); graph->connect(offset->output("Vector"), applyoffset->input("Vector2"));
      graph->connect(applyoffset->output("Vector"), addpivot->input("Vector1")); graph->connect(pivot->output("Vector"), addpivot->input("Vector2"));
      graph->connect(subpivot->output("Vector"), applyoffset2->input("Vector1")); graph->connect(offset->output("Vector"), applyoffset2->input("Vector2"));
      graph->connect(applyoffset2->output("Vector"), rotate2->input("Vector")); graph->connect(radians->output("Value"), rotate2->input("Angle"));
      graph->connect(rotate2->output("Vector"), applyscale2->input("Vector1")); graph->connect(scale->output("Vector"), applyscale2->input("Vector2"));
      graph->connect(applyscale2->output("Vector"), addpivot2->input("Vector1")); graph->connect(pivot->output("Vector"), addpivot2->input("Vector2"));
      graph->connect(addpivot->output("Vector"), operation->input("A")); graph->connect(addpivot2->output("Vector"), operation->input("B"));
      continue;
    }

    if (node.nodedef == rotate2d_vector2_id || node.nodedef == rotate3d_vector3_id) {
      ShaderNode *rotate = lowered_nodes.at(node.name);
      ShaderNode *radians = lowered_nodes.at(node.name + ".radians");
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       rotate->input("Vector"));
      }
      if (const auto amount = node.links.find("amount"); amount != node.links.end()) {
        graph->connect(lowered_output(amount->second, nodes_by_name, lowered_nodes),
                       radians->input("Value1"));
      }
      graph->connect(radians->output("Value"), rotate->input("Angle"));
      continue;
    }

    if (node.nodedef == image_color3_id || node.nodedef == image_color4_id ||
        node.nodedef == image_vector3_id || node.nodedef == image_vector4_id)
    {
      ShaderNode *image_node = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     image_node->input("Vector"));
      if (node.nodedef == image_vector4_id) {
        graph->connect(image_node->output("Alpha"), lowered_nodes.at(node.name + ".W")->input("Value1"));
      }
      continue;
    }
    if (Type triplanar_type; triplanarprojection_type(node.nodedef, &triplanar_type)) {
      ShaderNode *position = lowered_nodes.at(node.name + ".position");
      ShaderNode *uv_x = lowered_nodes.at(node.name + ".uv_x");
      ShaderNode *uv_y = lowered_nodes.at(node.name + ".uv_y");
      ShaderNode *uv_z = lowered_nodes.at(node.name + ".uv_z");
      ShaderNode *image_x = lowered_nodes.at(node.name + ".image_x");
      ShaderNode *image_y = lowered_nodes.at(node.name + ".image_y");
      ShaderNode *image_z = lowered_nodes.at(node.name + ".image_z");
      ShaderNode *normal_normalize = lowered_nodes.at(node.name + ".normal.normalize");
      ShaderNode *normal_abs = lowered_nodes.at(node.name + ".normal.abs");
      ShaderNode *normal_dot = lowered_nodes.at(node.name + ".normal.dot");
      ShaderNode *normal_dot_inverse = lowered_nodes.at(node.name + ".normal.inverse");
      ShaderNode *normal_weights = lowered_nodes.at(node.name + ".weights");
      ShaderNode *blend_clamp = lowered_nodes.at(node.name + ".blend.clamp");
      ShaderNode *one_over_blend = lowered_nodes.at(node.name + ".blend.inverse");
      ShaderNode *blend_power = lowered_nodes.at(node.name + ".blend.power");
      ShaderNode *blend_exponent = lowered_nodes.at(node.name + ".blend.exponent");
      ShaderNode *blend_dot = lowered_nodes.at(node.name + ".blend.dot");
      ShaderNode *blend_dot_inverse = lowered_nodes.at(node.name + ".blend.weight_inverse");
      ShaderNode *blend_weights = lowered_nodes.at(node.name + ".blend.weights");
      ShaderNode *separate_weights = lowered_nodes.at(node.name + ".separate_weights");

      graph->connect(lowered_output(node.links.at("position"), nodes_by_name, lowered_nodes),
                     position->input("Vector"));
      graph->connect(position->output("Y"), uv_x->input("X"));
      graph->connect(position->output("Z"), uv_x->input("Y"));
      graph->connect(position->output("X"), uv_y->input("X"));
      graph->connect(position->output("Z"), uv_y->input("Y"));
      graph->connect(position->output("X"), uv_z->input("X"));
      graph->connect(position->output("Y"), uv_z->input("Y"));
      graph->connect(uv_x->output("Vector"), image_x->input("Vector"));
      graph->connect(uv_y->output("Vector"), image_y->input("Vector"));
      graph->connect(uv_z->output("Vector"), image_z->input("Vector"));

      graph->connect(lowered_output(node.links.at("normal"), nodes_by_name, lowered_nodes),
                     normal_normalize->input("Vector1"));
      graph->connect(normal_normalize->output("Vector"), normal_abs->input("Vector1"));
      graph->connect(normal_abs->output("Vector"), normal_dot->input("Vector1"));
      graph->connect(normal_dot->output("Value"), normal_dot_inverse->input("Value2"));
      graph->connect(normal_abs->output("Vector"), normal_weights->input("Vector1"));
      graph->connect(normal_dot_inverse->output("Value"), normal_weights->input("Scale"));
      if (const auto blend = node.links.find("blend"); blend != node.links.end()) {
        graph->connect(lowered_output(blend->second, nodes_by_name, lowered_nodes),
                       blend_clamp->input("Value"));
      }
      graph->connect(blend_clamp->output("Result"), one_over_blend->input("Value2"));
      graph->connect(normal_weights->output("Vector"), blend_power->input("Vector1"));
      graph->connect(one_over_blend->output("Value"), blend_exponent->input("X"));
      graph->connect(one_over_blend->output("Value"), blend_exponent->input("Y"));
      graph->connect(one_over_blend->output("Value"), blend_exponent->input("Z"));
      graph->connect(blend_exponent->output("Vector"), blend_power->input("Vector2"));
      graph->connect(blend_power->output("Vector"), blend_dot->input("Vector1"));
      graph->connect(blend_dot->output("Value"), blend_dot_inverse->input("Value2"));
      graph->connect(blend_power->output("Vector"), blend_weights->input("Vector1"));
      graph->connect(blend_dot_inverse->output("Value"), blend_weights->input("Scale"));
      graph->connect(blend_weights->output("Vector"), separate_weights->input("Vector"));

      const std::pair<const char *, const char *> axes[] = {{"x", "X"}, {"y", "Y"}, {"z", "Z"}};
      if (triplanar_type == Type::Float) {
        for (const auto &[axis, weight_channel] : axes) {
          ShaderNode *image = lowered_nodes.at(node.name + ".image_" + axis);
          ShaderNode *separate = lowered_nodes.at(node.name + "." + axis + ".separate");
          ShaderNode *weighted = lowered_nodes.at(node.name + "." + axis + ".weighted");
          graph->connect(image->output("Color"), separate->input("Color"));
          graph->connect(separate->output("Red"), weighted->input("Value1"));
          graph->connect(separate_weights->output(weight_channel), weighted->input("Value2"));
        }
        ShaderNode *sum_xy = lowered_nodes.at(node.name + ".sum_xy");
        ShaderNode *sum = lowered_nodes.at(node.name);
        graph->connect(lowered_nodes.at(node.name + ".x.weighted")->output("Value"), sum_xy->input("Value1"));
        graph->connect(lowered_nodes.at(node.name + ".y.weighted")->output("Value"), sum_xy->input("Value2"));
        graph->connect(sum_xy->output("Value"), sum->input("Value1"));
        graph->connect(lowered_nodes.at(node.name + ".z.weighted")->output("Value"), sum->input("Value2"));
      }
      else if (triplanar_type == Type::Vector2) {
        for (const auto &[axis, weight_channel] : axes) {
          ShaderNode *image = lowered_nodes.at(node.name + ".image_" + axis);
          ShaderNode *separate = lowered_nodes.at(node.name + "." + axis + ".separate");
          ShaderNode *weighted_x = lowered_nodes.at(node.name + "." + axis + ".x.weighted");
          ShaderNode *weighted_y = lowered_nodes.at(node.name + "." + axis + ".y.weighted");
          graph->connect(image->output("Color"), separate->input("Color"));
          graph->connect(separate->output("Red"), weighted_x->input("Value1"));
          graph->connect(separate->output("Green"), weighted_y->input("Value1"));
          graph->connect(separate_weights->output(weight_channel), weighted_x->input("Value2"));
          graph->connect(separate_weights->output(weight_channel), weighted_y->input("Value2"));
        }
        ShaderNode *sum_x_xy = lowered_nodes.at(node.name + ".x.sum_xy");
        ShaderNode *sum_y_xy = lowered_nodes.at(node.name + ".y.sum_xy");
        ShaderNode *sum_x = lowered_nodes.at(node.name + ".x.sum");
        ShaderNode *sum_y = lowered_nodes.at(node.name + ".y.sum");
        ShaderNode *sum = lowered_nodes.at(node.name);
        graph->connect(lowered_nodes.at(node.name + ".x.x.weighted")->output("Value"), sum_x_xy->input("Value1"));
        graph->connect(lowered_nodes.at(node.name + ".y.x.weighted")->output("Value"), sum_x_xy->input("Value2"));
        graph->connect(lowered_nodes.at(node.name + ".x.y.weighted")->output("Value"), sum_y_xy->input("Value1"));
        graph->connect(lowered_nodes.at(node.name + ".y.y.weighted")->output("Value"), sum_y_xy->input("Value2"));
        graph->connect(sum_x_xy->output("Value"), sum_x->input("Value1"));
        graph->connect(sum_y_xy->output("Value"), sum_y->input("Value1"));
        graph->connect(lowered_nodes.at(node.name + ".z.x.weighted")->output("Value"), sum_x->input("Value2"));
        graph->connect(lowered_nodes.at(node.name + ".z.y.weighted")->output("Value"), sum_y->input("Value2"));
        graph->connect(sum_x->output("Value"), sum->input("X"));
        graph->connect(sum_y->output("Value"), sum->input("Y"));
      }
      else {
        for (const auto &[axis, weight_channel] : axes) {
          ShaderNode *weight = lowered_nodes.at(node.name + "." + axis + ".weight");
          graph->connect(separate_weights->output(weight_channel), weight->input("Red"));
          graph->connect(separate_weights->output(weight_channel), weight->input("Green"));
          graph->connect(separate_weights->output(weight_channel), weight->input("Blue"));
          ShaderNode *weighted = lowered_nodes.at(node.name + "." + axis + ".weighted");
          graph->connect(lowered_nodes.at(node.name + ".image_" + axis)->output("Color"), weighted->input("Color1"));
          graph->connect(weight->output("Color"), weighted->input("Color2"));
          if (triplanarprojection_is_four_component(node.nodedef)) {
            ShaderNode *weighted_w = lowered_nodes.at(node.name + "." + axis + triplanarprojection_component_suffix(triplanar_type) + ".weighted");
            graph->connect(lowered_nodes.at(node.name + ".image_" + axis)->output("Alpha"), weighted_w->input("Value1"));
            graph->connect(separate_weights->output(weight_channel), weighted_w->input("Value2"));
          }
        }
        ShaderNode *sum_xy = lowered_nodes.at(node.name + ".sum_xy");
        ShaderNode *sum = lowered_nodes.at(node.name);
        graph->connect(lowered_nodes.at(node.name + ".x.weighted")->output("Color"), sum_xy->input("Color1"));
        graph->connect(lowered_nodes.at(node.name + ".y.weighted")->output("Color"), sum_xy->input("Color2"));
        graph->connect(sum_xy->output("Color"), sum->input("Color1"));
        graph->connect(lowered_nodes.at(node.name + ".z.weighted")->output("Color"), sum->input("Color2"));
        if (triplanarprojection_is_four_component(node.nodedef)) {
          const char *suffix = triplanarprojection_component_suffix(triplanar_type);
          ShaderNode *sum_xy_w = lowered_nodes.at(node.name + suffix + ".sum_xy");
          ShaderNode *sum_w = lowered_nodes.at(node.name + suffix);
          graph->connect(lowered_nodes.at(node.name + ".x" + suffix + ".weighted")->output("Value"), sum_xy_w->input("Value1"));
          graph->connect(lowered_nodes.at(node.name + ".y" + suffix + ".weighted")->output("Value"), sum_xy_w->input("Value2"));
          graph->connect(sum_xy_w->output("Value"), sum_w->input("Value1"));
          graph->connect(lowered_nodes.at(node.name + ".z" + suffix + ".weighted")->output("Value"), sum_w->input("Value2"));
        }
      }
      continue;
    }


    if (node.nodedef == image_float_id) {
      ShaderNode *image = lowered_nodes.at(node.name + ".image");
      ShaderNode *separate = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     image->input("Vector"));
      graph->connect(image->output("Color"), separate->input("Color"));
      continue;
    }

    if (node.nodedef == extract_color4_id) {
      if (node.int_inputs.at("index") != 3) {
        ShaderNode *separate = lowered_nodes.at(node.name);
        ShaderNode *image = lowered_nodes.at(node.links.at("in").source_node);
        graph->connect(image->output("Color"), separate->input("Color"));
      }
      continue;
    }


    if (node.nodedef == image_vector2_id) {
      ShaderNode *image = lowered_nodes.at(node.name + ".image");
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     image->input("Vector"));
      graph->connect(image->output("Color"), separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("X"));
      graph->connect(separate->output("Green"), combine->input("Y"));
      continue;
    }

    if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id ||
        is_color4_ramp(node.nodedef))
    {
      ShaderNode *mix = lowered_nodes.at(node.name);
      ShaderNode *coordinate = lowered_nodes.at(node.name + ".coordinate");
      ShaderNode *clamp = lowered_nodes.at(node.name + ".factor");
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     coordinate->input("Vector"));
      graph->connect(
          coordinate->output(
              node.nodedef == ramptb_color3_id || node.nodedef == ramptb_color4_id ? "Y" : "X"),
          clamp->input("Value"));
      graph->connect(clamp->output("Result"), mix->input("Fac"));
      if (is_color4_ramp(node.nodedef)) {
        ShaderNode *alpha_delta = lowered_nodes.at(node.name + ".Alpha.delta");
        ShaderNode *alpha_product = lowered_nodes.at(node.name + ".Alpha.product");
        ShaderNode *alpha_sum = lowered_nodes.at(node.name + ".Alpha");
        const bool top_to_bottom = node.nodedef == ramptb_color4_id;
        const char *first_name = top_to_bottom ? "valuet" : "valuel";
        const char *second_name = top_to_bottom ? "valueb" : "valuer";
        if (const auto first = node.links.find(first_name); first != node.links.end()) {
          graph->connect(lowered_output(first->second, nodes_by_name, lowered_nodes),
                         mix->input("Color1"));
          graph->connect(lowered_color4_alpha_output(first->second, nodes_by_name, lowered_nodes),
                         alpha_delta->input("Value2"));
          graph->connect(lowered_color4_alpha_output(first->second, nodes_by_name, lowered_nodes),
                         alpha_sum->input("Value1"));
        }
        if (const auto second = node.links.find(second_name); second != node.links.end()) {
          graph->connect(lowered_output(second->second, nodes_by_name, lowered_nodes),
                         mix->input("Color2"));
          graph->connect(lowered_color4_alpha_output(second->second, nodes_by_name, lowered_nodes),
                         alpha_delta->input("Value1"));
        }
        graph->connect(clamp->output("Result"), alpha_product->input("Value2"));
        graph->connect(alpha_delta->output("Value"), alpha_product->input("Value1"));
        graph->connect(alpha_product->output("Value"), alpha_sum->input("Value2"));
      }
      continue;
    }
    if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      ShaderNode *coordinate = lowered_nodes.at(node.name + ".coordinate");
      ShaderNode *clamp = lowered_nodes.at(node.name + ".factor");
      ShaderNode *delta = lowered_nodes.at(node.name + ".delta");
      ShaderNode *product = lowered_nodes.at(node.name + ".product");
      ShaderNode *sum = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     coordinate->input("Vector"));
      graph->connect(coordinate->output(top_to_bottom ? "Y" : "X"), clamp->input("Value"));
      graph->connect(clamp->output("Result"), product->input("Value2"));
      graph->connect(delta->output("Value"), product->input("Value1"));
      graph->connect(product->output("Value"), sum->input("Value2"));
      continue;
    }

    if (is_split(node.nodedef)) {
      const bool top_to_bottom = split_is_top_to_bottom(node.nodedef);
      const bool color4 = is_color4_split(node.nodedef);
      ShaderNode *coordinate = lowered_nodes.at(node.name + ".coordinate");
      ShaderNode *factor = lowered_nodes.at(node.name + ".factor");
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     coordinate->input("Vector"));
      graph->connect(coordinate->output(top_to_bottom ? "Y" : "X"), factor->input("Value1"));
      if (const auto center = node.links.find("center"); center != node.links.end()) {
        graph->connect(lowered_output(center->second, nodes_by_name, lowered_nodes),
                       factor->input("Value2"));
      }
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      if (is_scalar_split(node.nodedef)) {
        ShaderNode *delta = lowered_nodes.at(node.name + ".delta");
        ShaderNode *product = lowered_nodes.at(node.name + ".product");
        ShaderNode *sum = lowered_nodes.at(node.name);
        if (const auto first = node.links.find(first_name); first != node.links.end()) {
          ShaderOutput *first_output = lowered_output(first->second, nodes_by_name, lowered_nodes);
          graph->connect(first_output, delta->input("Value2"));
          graph->connect(first_output, sum->input("Value1"));
        }
        if (const auto second = node.links.find(second_name); second != node.links.end()) {
          graph->connect(lowered_output(second->second, nodes_by_name, lowered_nodes),
                         delta->input("Value1"));
        }
        graph->connect(factor->output("Value"), product->input("Value2"));
        graph->connect(delta->output("Value"), product->input("Value1"));
        graph->connect(product->output("Value"), sum->input("Value2"));
      }
      else {
        ShaderNode *mix = lowered_nodes.at(node.name);
        graph->connect(factor->output("Value"), mix->input("Fac"));
        if (const auto first = node.links.find(first_name); first != node.links.end()) {
          graph->connect(lowered_output(first->second, nodes_by_name, lowered_nodes),
                         mix->input("Color1"));
        }
        if (const auto second = node.links.find(second_name); second != node.links.end()) {
          graph->connect(lowered_output(second->second, nodes_by_name, lowered_nodes),
                         mix->input("Color2"));
        }
        if (color4) {
          ShaderNode *alpha_delta = lowered_nodes.at(node.name + ".Alpha.delta");
          ShaderNode *alpha_product = lowered_nodes.at(node.name + ".Alpha.product");
          ShaderNode *alpha_sum = lowered_nodes.at(node.name + ".Alpha");
          if (const auto first = node.links.find(first_name); first != node.links.end()) {
            ShaderOutput *first_alpha = lowered_color4_alpha_output(
                first->second, nodes_by_name, lowered_nodes);
            graph->connect(first_alpha, alpha_delta->input("Value2"));
            graph->connect(first_alpha, alpha_sum->input("Value1"));
          }
          if (const auto second = node.links.find(second_name); second != node.links.end()) {
            graph->connect(lowered_color4_alpha_output(second->second, nodes_by_name, lowered_nodes),
                           alpha_delta->input("Value1"));
          }
          graph->connect(factor->output("Value"), alpha_product->input("Value2"));
          graph->connect(alpha_delta->output("Value"), alpha_product->input("Value1"));
          graph->connect(alpha_product->output("Value"), alpha_sum->input("Value2"));
        }
      }
      continue;
    }

    if (node.nodedef == normalmap_float_id) {
      ShaderNode *normalmap = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       normalmap->input("Color"));
      }
      continue;
    }
    NodeVectorMathType vector_type;
    if (vector_math_type(node.nodedef, &vector_type)) {
      ShaderNode *math = lowered_nodes.at(node.name);
      const char *first = vector_math_is_unary(node.nodedef) ? "in" : "in1";
      if (const auto input = node.links.find(first); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Vector1"));
      }
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        ShaderOutput *output = lowered_output(input->second, nodes_by_name, lowered_nodes);
        if (vector_math_uses_scalar_broadcast_second(node.nodedef)) {
          ShaderNode *broadcast = lowered_nodes.at(node.name + ".broadcast");
          graph->connect(output, broadcast->input("X"));
          graph->connect(output, broadcast->input("Y"));
          graph->connect(output, broadcast->input("Z"));
          graph->connect(broadcast->output("Vector"), math->input("Vector2"));
        }
        else {
          graph->connect(output, math->input(vector_math_uses_scalar_second(node.nodedef) ? "Scale" : "Vector2"));
        }
      }
      if (node.nodedef == "ND_refract_vector3") {
        if (const auto input = node.links.find("scale"); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         math->input("Scale"));
        }
      }
      continue;
    }
    if (vector2_math_type(node.nodedef, &vector_type)) {
      const bool returns_float = vector2_math_returns_float(node.nodedef);
      ShaderNode *math = returns_float ? lowered_nodes.at(node.name) :
                                        lowered_nodes.at(node.name + ".vector2.math");
      const char *first_name = vector2_math_is_unary(node.nodedef) ? "in" : "in1";
      if (const auto input = node.links.find(first_name); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Vector1"));
      }
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        ShaderOutput *output = lowered_output(input->second, nodes_by_name, lowered_nodes);
        if (vector2_math_uses_scalar_broadcast_second(node.nodedef)) {
          ShaderNode *broadcast = lowered_nodes.at(node.name + ".broadcast");
          graph->connect(output, broadcast->input("X"));
          graph->connect(output, broadcast->input("Y"));
          graph->connect(broadcast->output("Vector"), math->input("Vector2"));
        }
        else {
          graph->connect(output,
                         math->input(vector2_math_uses_scalar_second(node.nodedef) ? "Scale" : "Vector2"));
        }
      }
      if (!returns_float) {
        ShaderNode *separate = lowered_nodes.at(node.name + ".vector2.separate");
        ShaderNode *combine = lowered_nodes.at(node.name);
        graph->connect(math->output("Vector"), separate->input("Vector"));
        graph->connect(separate->output("X"), combine->input("X"));
        graph->connect(separate->output("Y"), combine->input("Y"));
      }
      continue;
    }

    if (is_safepower_vector2(node.nodedef)) {
      const bool scalar_second = safepower_vector2_uses_scalar_second(node.nodedef);
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *second = scalar_second ? nullptr : lowered_nodes.at(node.name + ".second");
      ShaderNode *combine = lowered_nodes.at(node.name);
      const auto first_link = node.links.find("in1");
      const auto second_link = node.links.find("in2");
      if (first_link != node.links.end()) {
        graph->connect(
            lowered_output(first_link->second, nodes_by_name, lowered_nodes), first->input("Vector"));
      }
      if (second && second_link != node.links.end()) {
        graph->connect(
            lowered_output(second_link->second, nodes_by_name, lowered_nodes), second->input("Vector"));
      }
      for (const char *channel : {"X", "Y"}) {
        ShaderNode *absolute = lowered_nodes.at(node.name + "." + channel + ".abs");
        ShaderNode *sign = lowered_nodes.at(node.name + "." + channel + ".sign");
        ShaderNode *power = lowered_nodes.at(node.name + "." + channel + ".power");
        ShaderNode *multiply = lowered_nodes.at(node.name + "." + channel + ".multiply");
        if (first_link != node.links.end()) {
          graph->connect(first->output(channel), absolute->input("Value1"));
          graph->connect(first->output(channel), sign->input("Value1"));
        }
        graph->connect(absolute->output("Value"), power->input("Value1"));
        if (second && second_link != node.links.end()) {
          graph->connect(second->output(channel), power->input("Value2"));
        }
        else if (!second && second_link != node.links.end()) {
          graph->connect(lowered_output(second_link->second, nodes_by_name, lowered_nodes),
                         power->input("Value2"));
        }
        graph->connect(sign->output("Value"), multiply->input("Value1"));
        graph->connect(power->output("Value"), multiply->input("Value2"));
        graph->connect(multiply->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector2_binary_component_math_type(node.nodedef, nullptr)) {
      const bool scalar_second = vector2_binary_component_math_uses_scalar_second(node.nodedef);
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in1"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), first->input("Vector"));
      }
      ShaderNode *second = scalar_second ? nullptr : lowered_nodes.at(node.name + ".second");
      if (second) {
        if (const auto input = node.links.find("in2"); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), second->input("Vector"));
        }
      }
      for (const char *channel : {"X", "Y"}) {
        ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
        graph->connect(first->output(channel), math->input("Value1"));
        if (second) graph->connect(second->output(channel), math->input("Value2"));
        else if (const auto input = node.links.find("in2"); input != node.links.end()) graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Value2"));
        graph->connect(math->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (is_safepower_vector3(node.nodedef)) {
      const bool scalar_second = safepower_vector3_uses_scalar_second(node.nodedef);
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *second = scalar_second ? nullptr : lowered_nodes.at(node.name + ".second");
      ShaderNode *combine = lowered_nodes.at(node.name);
      const auto first_link = node.links.find("in1");
      const auto second_link = node.links.find("in2");
      if (first_link != node.links.end()) {
        graph->connect(
            lowered_output(first_link->second, nodes_by_name, lowered_nodes), first->input("Vector"));
      }
      if (second && second_link != node.links.end()) {
        graph->connect(
            lowered_output(second_link->second, nodes_by_name, lowered_nodes), second->input("Vector"));
      }
      for (const char *channel : {"X", "Y", "Z"}) {
        ShaderNode *absolute = lowered_nodes.at(node.name + "." + channel + ".abs");
        ShaderNode *sign = lowered_nodes.at(node.name + "." + channel + ".sign");
        ShaderNode *power = lowered_nodes.at(node.name + "." + channel + ".power");
        ShaderNode *multiply = lowered_nodes.at(node.name + "." + channel + ".multiply");
        if (first_link != node.links.end()) {
          graph->connect(first->output(channel), absolute->input("Value1"));
          graph->connect(first->output(channel), sign->input("Value1"));
        }
        graph->connect(absolute->output("Value"), power->input("Value1"));
        if (second && second_link != node.links.end()) {
          graph->connect(second->output(channel), power->input("Value2"));
        }
        else if (!second && second_link != node.links.end()) {
          graph->connect(lowered_output(second_link->second, nodes_by_name, lowered_nodes),
                         power->input("Value2"));
        }
        graph->connect(sign->output("Value"), multiply->input("Value1"));
        graph->connect(power->output("Value"), multiply->input("Value2"));
        graph->connect(multiply->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector3_binary_component_math_type(node.nodedef, nullptr)) {
      const bool scalar_second = vector3_binary_component_math_uses_scalar_second(node.nodedef);
      ShaderNode *first = lowered_nodes.at(node.name + ".first");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in1"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), first->input("Vector"));
      }
      ShaderNode *second = scalar_second ? nullptr : lowered_nodes.at(node.name + ".second");
      if (second) {
        if (const auto input = node.links.find("in2"); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), second->input("Vector"));
        }
      }
      for (const char *channel : {"X", "Y", "Z"}) {
        ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
        graph->connect(first->output(channel), math->input("Value1"));
        if (second) graph->connect(second->output(channel), math->input("Value2"));
        else if (const auto input = node.links.find("in2"); input != node.links.end()) graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Value2"));
        graph->connect(math->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (bool scalar_amount; vector2_invert_type(node.nodedef, &scalar_amount)) {
      ShaderNode *input = lowered_nodes.at(node.name + ".in.separate");
      ShaderNode *amount = scalar_amount ? nullptr : lowered_nodes.at(node.name + ".amount.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("in"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), input->input("Vector"));
      }
      if (amount) {
        if (const auto link = node.links.find("amount"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), amount->input("Vector"));
        }
      }
      for (const char *channel : {"X", "Y"}) {
        ShaderNode *subtract = lowered_nodes.at(node.name + "." + channel);
        if (amount) graph->connect(amount->output(channel), subtract->input("Value1"));
        else if (const auto link = node.links.find("amount"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), subtract->input("Value1"));
        graph->connect(input->output(channel), subtract->input("Value2"));
        graph->connect(subtract->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (bool scalar_amount; vector3_invert_type(node.nodedef, &scalar_amount)) {
      ShaderNode *input = lowered_nodes.at(node.name + ".in.separate");
      ShaderNode *amount = scalar_amount ? nullptr : lowered_nodes.at(node.name + ".amount.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("in"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), input->input("Vector"));
      }
      if (amount) {
        if (const auto link = node.links.find("amount"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), amount->input("Vector"));
        }
      }
      for (const char *channel : {"X", "Y", "Z"}) {
        ShaderNode *subtract = lowered_nodes.at(node.name + "." + channel);
        if (amount) graph->connect(amount->output(channel), subtract->input("Value1"));
        else if (const auto link = node.links.find("amount"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), subtract->input("Value1"));
        graph->connect(input->output(channel), subtract->input("Value2"));
        graph->connect(subtract->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector2_smoothstep_type(node.nodedef, nullptr)) {
      ShaderNode *input = lowered_nodes.at(node.name + ".in.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("in"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), input->input("Vector"));
      for (const char *channel : {"X", "Y"}) {
        ShaderNode *numerator = lowered_nodes.at(node.name + "." + channel + ".numerator");
        ShaderNode *denominator = lowered_nodes.at(node.name + "." + channel + ".denominator");
        ShaderNode *divide = lowered_nodes.at(node.name + "." + channel + ".divide");
        ShaderNode *maximum = lowered_nodes.at(node.name + "." + channel + ".maximum");
        ShaderNode *minimum = lowered_nodes.at(node.name + "." + channel + ".minimum");
        ShaderNode *square = lowered_nodes.at(node.name + "." + channel + ".square");
        ShaderNode *twice = lowered_nodes.at(node.name + "." + channel + ".twice");
        ShaderNode *cubic = lowered_nodes.at(node.name + "." + channel + ".cubic");
        ShaderNode *result = lowered_nodes.at(node.name + "." + channel + ".result");
        if (node.links.find("in") != node.links.end()) graph->connect(input->output(channel), numerator->input("Value1"));
        graph->connect(numerator->output("Value"), divide->input("Value1")); graph->connect(denominator->output("Value"), divide->input("Value2"));
        graph->connect(divide->output("Value"), maximum->input("Value1")); graph->connect(maximum->output("Value"), minimum->input("Value1"));
        graph->connect(minimum->output("Value"), square->input("Value1")); graph->connect(minimum->output("Value"), square->input("Value2"));
        graph->connect(minimum->output("Value"), twice->input("Value1")); graph->connect(twice->output("Value"), cubic->input("Value2"));
        graph->connect(square->output("Value"), result->input("Value1")); graph->connect(cubic->output("Value"), result->input("Value2"));
        graph->connect(result->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector3_smoothstep_type(node.nodedef, nullptr)) {
      ShaderNode *input = lowered_nodes.at(node.name + ".in.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("in"); link != node.links.end()) graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), input->input("Vector"));
      for (const char *channel : {"X", "Y", "Z"}) {
        ShaderNode *numerator = lowered_nodes.at(node.name + "." + channel + ".numerator");
        ShaderNode *denominator = lowered_nodes.at(node.name + "." + channel + ".denominator");
        ShaderNode *divide = lowered_nodes.at(node.name + "." + channel + ".divide");
        ShaderNode *maximum = lowered_nodes.at(node.name + "." + channel + ".maximum");
        ShaderNode *minimum = lowered_nodes.at(node.name + "." + channel + ".minimum");
        ShaderNode *square = lowered_nodes.at(node.name + "." + channel + ".square");
        ShaderNode *twice = lowered_nodes.at(node.name + "." + channel + ".twice");
        ShaderNode *cubic = lowered_nodes.at(node.name + "." + channel + ".cubic");
        ShaderNode *result = lowered_nodes.at(node.name + "." + channel + ".result");
        if (node.links.find("in") != node.links.end()) graph->connect(input->output(channel), numerator->input("Value1"));
        graph->connect(numerator->output("Value"), divide->input("Value1")); graph->connect(denominator->output("Value"), divide->input("Value2"));
        graph->connect(divide->output("Value"), maximum->input("Value1")); graph->connect(maximum->output("Value"), minimum->input("Value1"));
        graph->connect(minimum->output("Value"), square->input("Value1")); graph->connect(minimum->output("Value"), square->input("Value2"));
        graph->connect(minimum->output("Value"), twice->input("Value1")); graph->connect(twice->output("Value"), cubic->input("Value2"));
        graph->connect(square->output("Value"), result->input("Value1")); graph->connect(cubic->output("Value"), result->input("Value2"));
        graph->connect(result->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector2_domain_math_type(node.nodedef, nullptr)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       separate->input("Vector"));
        for (const char *channel : {"X", "Y"}) {
          ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
          graph->connect(separate->output(channel), math->input("Value1"));
          graph->connect(math->output("Value"), combine->input(channel));
        }
      }
      else {
        for (const char *channel : {"X", "Y"}) {
          ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
          graph->connect(math->output("Value"), combine->input(channel));
        }
      }
      continue;
    }

    if (vector3_domain_math_type(node.nodedef, nullptr)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       separate->input("Vector"));
        for (const char *channel : {"X", "Y", "Z"}) {
          ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
          graph->connect(separate->output(channel), math->input("Value1"));
          graph->connect(math->output("Value"), combine->input(channel));
        }
      }
      else {
        for (const char *channel : {"X", "Y", "Z"}) {
          ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
          graph->connect(math->output("Value"), combine->input(channel));
        }
      }
      continue;
    }

    if (vector2_atan2_type(node.nodedef, nullptr)) {
      ShaderNode *separate_y = lowered_nodes.at(node.name + ".iny.separate");
      ShaderNode *separate_x = lowered_nodes.at(node.name + ".inx.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto iny = node.links.find("iny"); iny != node.links.end()) {
        graph->connect(lowered_output(iny->second, nodes_by_name, lowered_nodes),
                       separate_y->input("Vector"));
      }
      if (const auto inx = node.links.find("inx"); inx != node.links.end()) {
        graph->connect(lowered_output(inx->second, nodes_by_name, lowered_nodes),
                       separate_x->input("Vector"));
      }
      for (const char *channel : {"X", "Y"}) {
        ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
        if (node.links.find("iny") != node.links.end()) {
          graph->connect(separate_y->output(channel), math->input("Value1"));
        }
        if (node.links.find("inx") != node.links.end()) {
          graph->connect(separate_x->output(channel), math->input("Value2"));
        }
        graph->connect(math->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (vector3_atan2_type(node.nodedef, nullptr)) {
      ShaderNode *separate_y = lowered_nodes.at(node.name + ".iny.separate");
      ShaderNode *separate_x = lowered_nodes.at(node.name + ".inx.separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto iny = node.links.find("iny"); iny != node.links.end()) {
        graph->connect(lowered_output(iny->second, nodes_by_name, lowered_nodes),
                       separate_y->input("Vector"));
      }
      if (const auto inx = node.links.find("inx"); inx != node.links.end()) {
        graph->connect(lowered_output(inx->second, nodes_by_name, lowered_nodes),
                       separate_x->input("Vector"));
      }
      for (const char *channel : {"X", "Y", "Z"}) {
        ShaderNode *math = lowered_nodes.at(node.name + "." + channel);
        if (node.links.find("iny") != node.links.end()) {
          graph->connect(separate_y->output(channel), math->input("Value1"));
        }
        if (node.links.find("inx") != node.links.end()) {
          graph->connect(separate_x->output(channel), math->input("Value2"));
        }
        graph->connect(math->output("Value"), combine->input(channel));
      }
      continue;
    }

    if (node.nodedef == standard_surface_id) {
      ShaderNode *surface_node = lowered_nodes.count(node.name + ".standard_surface_base") ?
                                     lowered_nodes.at(node.name + ".standard_surface_base") :
                                     lowered_nodes.at(node.name);
      const auto connect_if_linked = [&](const char *input, const char *socket) {
        if (const auto link = node.links.find(input); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         surface_node->input(socket));
        }
      };
      connect_if_linked("base_color", "Base Color");
      connect_if_linked("metalness", "Metallic");
      connect_if_linked("diffuse_roughness", "Diffuse Roughness");
      connect_if_linked("specular_color", "Specular Tint");
      connect_if_linked("specular_anisotropy", "Anisotropic");
      connect_if_linked("specular_rotation", "Anisotropic Rotation");
      connect_if_linked("subsurface", "Subsurface Weight");
      connect_if_linked("subsurface_radius", "Subsurface Radius");
      connect_if_linked("subsurface_scale", "Subsurface Scale");
      connect_if_linked("subsurface_anisotropy", "Subsurface Anisotropy");
      connect_if_linked("emission", "Emission Strength");
      connect_if_linked("emission_color", "Emission Color");
      connect_if_linked("normal", "Normal");
      connect_if_linked("tangent", "Tangent");
      if (const auto base = node.links.find("base"); base != node.links.end()) {
        MathNode *weight_delta = graph->create_node<MathNode>();
        weight_delta->name = node.name + ".base_delta";
        weight_delta->set_math_type(NODE_MATH_SUBTRACT);
        weight_delta->set_value2(1.0f);
        graph->connect(lowered_output(base->second, nodes_by_name, lowered_nodes),
                       weight_delta->input("Value1"));
        graph->connect(weight_delta->output("Value"), surface_node->input("SurfaceMixWeight"));
      }
      if (const auto specular = node.links.find("specular"); specular != node.links.end()) {
        MathNode *scale = graph->create_node<MathNode>();
        scale->name = node.name + ".specular_scale";
        scale->set_math_type(NODE_MATH_MULTIPLY);
        scale->set_value2(2.0f);
        graph->connect(lowered_output(specular->second, nodes_by_name, lowered_nodes),
                       scale->input("Value1"));
        graph->connect(scale->output("Value"), surface_node->input("Specular IOR Level"));
      }
      if (const auto opacity = node.links.find("opacity"); opacity != node.links.end()) {
        SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
        separate->name = node.name + ".opacity_luminance.separate";
        separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
        CombineXYZNode *rgb = graph->create_node<CombineXYZNode>();
        rgb->name = node.name + ".opacity_luminance.rgb";
        CombineXYZNode *coefficients = graph->create_node<CombineXYZNode>();
        coefficients->name = node.name + ".opacity_luminance.coefficients";
        coefficients->set_x(0.2126f);
        coefficients->set_y(0.7152f);
        coefficients->set_z(0.0722f);
        VectorMathNode *dot = graph->create_node<VectorMathNode>();
        dot->name = node.name + ".opacity_luminance";
        dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
        graph->connect(lowered_output(opacity->second, nodes_by_name, lowered_nodes),
                       separate->input("Color"));
        graph->connect(separate->output("Red"), rgb->input("X"));
        graph->connect(separate->output("Green"), rgb->input("Y"));
        graph->connect(separate->output("Blue"), rgb->input("Z"));
        graph->connect(rgb->output("Vector"), dot->input("Vector1"));
        graph->connect(coefficients->output("Vector"), dot->input("Vector2"));
        graph->connect(dot->output("Value"), surface_node->input("Alpha"));
      }

      ShaderOutput *closure = surface_node->output("BSDF");
      if (has_standard_surface_sheen(node)) {
        ShaderNode *sheen = lowered_nodes.at(node.name + ".standard_surface_sheen");
        ShaderNode *sum = lowered_nodes.at(node.name + ".standard_surface_sheen_sum");
        if (const auto link = node.links.find("sheen_color"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         sheen->input("Color"));
        }
        if (const auto link = node.links.find("sheen"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         sheen->input("SurfaceMixWeight"));
        }
        if (const auto link = node.links.find("sheen_roughness"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         sheen->input("Roughness"));
        }
        if (const auto link = node.links.find("normal"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         sheen->input("Normal"));
        }
        graph->connect(closure, sum->input("Closure1"));
        graph->connect(sheen->output("BSDF"), sum->input("Closure2"));
        closure = sum->output("Closure");
      }

      if (const auto specular_roughness = node.links.find("specular_roughness");
          specular_roughness != node.links.end())
      {
        graph->connect(lowered_output(specular_roughness->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Roughness"));
      }
      if (const auto specular_IOR = node.links.find("specular_IOR");
          specular_IOR != node.links.end())
      {
        graph->connect(lowered_output(specular_IOR->second, nodes_by_name, lowered_nodes),
                       surface_node->input("IOR"));
      }
      if (const auto thin_film_thickness = node.links.find("thin_film_thickness");
          thin_film_thickness != node.links.end())
      {
        graph->connect(lowered_output(thin_film_thickness->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Thin Film Thickness"));
      }
      if (const auto thin_film_IOR = node.links.find("thin_film_IOR");
          thin_film_IOR != node.links.end())
      {
        graph->connect(lowered_output(thin_film_IOR->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Thin Film IOR"));
      }
      if (has_standard_surface_transmission(node)) {
        ShaderNode *transmission = lowered_nodes.at(node.name + ".standard_surface_transmission");
        ShaderNode *mix = lowered_nodes.at(node.name + ".standard_surface_transmission_mix");
        if (const auto link = node.links.find("transmission_color"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("Color"));
        }
        if (const auto link = node.links.find("specular_roughness"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("Roughness"));
        }
        if (const auto link = node.links.find("specular_IOR"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("IOR"));
        }
        if (const auto link = node.links.find("thin_film_thickness"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("Thin Film Thickness"));
        }
        if (const auto link = node.links.find("thin_film_IOR"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("Thin Film IOR"));
        }
        if (const auto link = node.links.find("normal"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         transmission->input("Normal"));
        }
        if (const auto link = node.links.find("transmission"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         mix->input("Fac"));
        }
        graph->connect(closure, mix->input("Closure1"));
        graph->connect(transmission->output("BSDF"), mix->input("Closure2"));
        closure = mix->output("Closure");
      }

      if (has_standard_surface_coat(node)) {
        ShaderNode *coat = lowered_nodes.at(node.name + ".standard_surface_coat");
        ShaderNode *sum = lowered_nodes.at(node.name);
        if (const auto link = node.links.find("coat_color"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         coat->input("Color"));
        }
        if (const auto link = node.links.find("coat"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         coat->input("SurfaceMixWeight"));
        }
        if (const auto link = node.links.find("coat_roughness"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         coat->input("Roughness"));
        }
        if (const auto link = node.links.find("coat_normal"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         coat->input("Normal"));
        }
        else if (const auto link = node.links.find("normal"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         coat->input("Normal"));
        }
        graph->connect(closure, sum->input("Closure1"));
        graph->connect(coat->output("BSDF"), sum->input("Closure2"));
        closure = sum->output("Closure");
      }
      graph->connect(closure, graph->output()->input("Surface"));
      continue;
    }

    /* Direct pbrlib/LAMA BSDF leaf dual-purpose connect wiring -- see the
     * matching comments in validate() and in the create-phase loop. Only the
     * SurfaceShader-typed (generic <surface> closure-graph) flavor is wired
     * here, including a *linked* weight (via a SurfaceMixWeight delta) for
     * the oren_nayar_diffuse_bsdf_id flavor; the BSDF-typed flavor falls
     * through to the generic is_bsdf_producer connect dispatch below. */
    if ((is_bsdf_producer(node.nodedef) || is_lama_microfacet_surface_bsdf(node.nodedef)) &&
        node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader)
    {
      ShaderNode *bsdf = lowered_nodes.at(node.name);
      if (const auto color = node.links.find("color"); color != node.links.end()) {
        graph->connect(lowered_output(color->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Color"));
      }
      if (const auto tint = node.links.find("tint"); tint != node.links.end()) {
        graph->connect(lowered_output(tint->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Color"));
      }
      if (const auto roughness = node.links.find("roughness"); roughness != node.links.end()) {
        graph->connect(lowered_output(roughness->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Roughness"));
      }
      const char *radius_input = node.nodedef == lama_sss_id ? "sssRadius" : "radius";
      if (const auto radius = node.links.find(radius_input); radius != node.links.end()) {
        graph->connect(lowered_output(radius->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Radius"));
      }
      const char *anisotropy_input = node.nodedef == lama_sss_id ? "sssAnisotropy" : "anisotropy";
      if (const auto anisotropy = node.links.find(anisotropy_input); anisotropy != node.links.end()) {
        graph->connect(lowered_output(anisotropy->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Anisotropy"));
      }
      if (const auto ior = node.links.find("ior"); ior != node.links.end()) {
        graph->connect(lowered_output(ior->second, nodes_by_name, lowered_nodes), bsdf->input("IOR"));
      }
      if (const auto extinction = node.links.find("extinction"); extinction != node.links.end()) {
        graph->connect(lowered_output(extinction->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Extinction"));
      }
      if (const auto thinfilm_thickness = node.links.find("thinfilm_thickness");
          thinfilm_thickness != node.links.end())
      {
        graph->connect(lowered_output(thinfilm_thickness->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Thin Film Thickness"));
      }
      if (const auto thinfilm_ior = node.links.find("thinfilm_ior");
          thinfilm_ior != node.links.end())
      {
        graph->connect(lowered_output(thinfilm_ior->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Thin Film IOR"));
      }
      if (const auto normal = node.links.find("normal"); normal != node.links.end()) {
        graph->connect(lowered_output(normal->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Normal"));
      }
      if (const auto weight = node.links.find("weight"); weight != node.links.end()) {
        MathNode *weight_delta = graph->create_node<MathNode>();
        weight_delta->name = node.name + ".weight_delta";
        weight_delta->set_math_type(NODE_MATH_SUBTRACT);
        weight_delta->set_value2(1.0f);
        graph->connect(lowered_output(weight->second, nodes_by_name, lowered_nodes),
                       weight_delta->input("Value1"));
        graph->connect(weight_delta->output("Value"), bsdf->input("SurfaceMixWeight"));
      }
      continue;
    }

    if (node.nodedef == uniform_edf_id || node.nodedef == lama_emission_id) {
      ShaderNode *emission = lowered_nodes.at(node.name);
      if (const auto color = node.links.find("color"); color != node.links.end()) {
        graph->connect(lowered_output(color->second, nodes_by_name, lowered_nodes),
                       emission->input("Color"));
      }
      continue;
    }

    if (node.nodedef == generic_surface_id) {
      ShaderOutput *closure = nullptr;
      const auto bsdf = node.links.find("bsdf");
      const auto edf = node.links.find("edf");
      if (bsdf != node.links.end() && edf != node.links.end()) {
        ShaderNode *add = lowered_nodes.at(node.name + ".add");
        graph->connect(lowered_output(bsdf->second, nodes_by_name, lowered_nodes),
                       add->input("Closure1"));
        graph->connect(lowered_output(edf->second, nodes_by_name, lowered_nodes),
                       add->input("Closure2"));
        closure = add->output("Closure");
      }
      else if (bsdf != node.links.end()) {
        closure = lowered_output(bsdf->second, nodes_by_name, lowered_nodes);
      }
      else if (edf != node.links.end()) {
        closure = lowered_output(edf->second, nodes_by_name, lowered_nodes);
      }
      if (node.links.find("opacity") != node.links.end() ||
          node.inputs.find("opacity") != node.inputs.end())
      {
        ShaderNode *transparent = lowered_nodes.at(node.name + ".transparent");
        ShaderNode *mix = lowered_nodes.at(node.name);
        graph->connect(transparent->output("BSDF"), mix->input("Closure1"));
        graph->connect(closure, mix->input("Closure2"));
        if (const auto opacity = node.links.find("opacity"); opacity != node.links.end()) {
          graph->connect(lowered_output(opacity->second, nodes_by_name, lowered_nodes),
                         mix->input("Fac"));
        }
        closure = mix->output("Closure");
      }
      if (!surface_shader_node_has_surface_shader_consumer(source, node.name)) {
        graph->connect(closure, graph->output()->input("Surface"));
      }
      continue;
    }

    if (node.nodedef == mix_surfaceshader_id) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      const auto surface_shader_closure = [&](const Link &link) -> ShaderOutput * {
        const Node &source = *nodes_by_name.at(link.source_node);
        if (source.nodedef == generic_surface_id && !source.links.contains("opacity") &&
            !source.inputs.contains("opacity"))
        {
          const auto bsdf = source.links.find("bsdf");
          const auto edf = source.links.find("edf");
          if ((bsdf != source.links.end()) != (edf != source.links.end())) {
            return lowered_output(bsdf != source.links.end() ? bsdf->second : edf->second,
                                  nodes_by_name,
                                  lowered_nodes);
          }
        }
        return lowered_output(link, nodes_by_name, lowered_nodes);
      };
      graph->connect(surface_shader_closure(node.links.at("bg")), mix->input("Closure1"));
      graph->connect(surface_shader_closure(node.links.at("fg")), mix->input("Closure2"));
      if (const auto mix_amount = node.links.find("mix"); mix_amount != node.links.end()) {
        graph->connect(lowered_output(mix_amount->second, nodes_by_name, lowered_nodes),
                       mix->input("Fac"));
      }
      if (!surface_shader_node_has_surface_shader_consumer(source, node.name)) {
        graph->connect(mix->output("Closure"), graph->output()->input("Surface"));
      }
      continue;
    }

    if (node.nodedef == dot_surfaceshader_id) {
      if (!surface_shader_node_has_surface_shader_consumer(source, node.name)) {
        graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                       graph->output()->input("Surface"));
      }
      continue;
    }

    if (node.nodedef == lama_surface_id) {
      ShaderOutput *closure = lowered_output(node.links.at("materialFront"), nodes_by_name, lowered_nodes);
      if (const auto back = node.links.find("materialBack"); back != node.links.end()) {
        ShaderNode *geometry = lowered_nodes.at(node.name + ".geometry");
        ShaderNode *side = lowered_nodes.at(node.name + ".side");
        graph->connect(closure, side->input("Closure1"));
        graph->connect(lowered_output(back->second, nodes_by_name, lowered_nodes),
                       side->input("Closure2"));
        graph->connect(geometry->output("Backfacing"), side->input("Fac"));
        closure = side->output("Closure");
      }
      if (node.links.find("presence") != node.links.end() ||
          node.inputs.find("presence") != node.inputs.end())
      {
        ShaderNode *transparent = lowered_nodes.at(node.name + ".transparent");
        ShaderNode *presence = lowered_nodes.at(node.name);
        graph->connect(transparent->output("BSDF"), presence->input("Closure1"));
        graph->connect(closure, presence->input("Closure2"));
        if (const auto link = node.links.find("presence"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         presence->input("Fac"));
        }
        closure = presence->output("Closure");
      }
      if (!surface_shader_node_has_surface_shader_consumer(source, node.name)) {
        graph->connect(closure, graph->output()->input("Surface"));
      }
      continue;
    }

    /* mix_bsdf_id/add_bsdf_id dual-purpose connect wiring -- see the
     * matching comment in the create-phase loop. Only the SurfaceShader-
     * typed flavor is wired here; the BSDF-typed flavor falls through to
     * the generic is_bsdf_combinator connect dispatch below. mix_edf_id/
     * add_edf_id have no other flavor. */
    if (((node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) &&
         node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
        node.nodedef == mix_edf_id || node.nodedef == lama_mix_edf_id)
    {
      ShaderNode *mix = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("bg"), nodes_by_name, lowered_nodes),
                     mix->input("Closure1"));
      graph->connect(lowered_output(node.links.at("fg"), nodes_by_name, lowered_nodes),
                     mix->input("Closure2"));
      if (const auto mix_amount = node.links.find("mix"); mix_amount != node.links.end()) {
        graph->connect(lowered_output(mix_amount->second, nodes_by_name, lowered_nodes),
                       mix->input("Fac"));
      }
      continue;
    }

    if (((node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) &&
         node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
        node.nodedef == add_edf_id || node.nodedef == lama_add_edf_id)
    {
      ShaderNode *add = lowered_nodes.at(node.name);
      if (node.nodedef == lama_add_bsdf_id || node.nodedef == lama_add_edf_id) {
        ShaderNode *mul1 = lowered_nodes.at(node.name + ".weight1");
        ShaderNode *null1 = lowered_nodes.at(node.name + ".weight1_null");
        ShaderNode *mul2 = lowered_nodes.at(node.name + ".weight2");
        ShaderNode *null2 = lowered_nodes.at(node.name + ".weight2_null");
        graph->connect(null1->output("BSDF"), mul1->input("Closure1"));
        graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                       mul1->input("Closure2"));
        graph->connect(null2->output("BSDF"), mul2->input("Closure1"));
        graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes),
                       mul2->input("Closure2"));
        graph->connect(mul1->output("Closure"), add->input("Closure1"));
        graph->connect(mul2->output("Closure"), add->input("Closure2"));
      }
      else {
        graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                       add->input("Closure1"));
        graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes),
                       add->input("Closure2"));
      }
      continue;
    }

    /* multiply_bsdff_id/multiply_bsdfc_id dual-purpose connect wiring --
     * see the matching comment in the create-phase loop. Same wiring as the
     * BSDF-typed is_bsdf_combinator connect dispatch below: the null
     * closure into Closure1, the real 'in1' subgraph into Closure2, 'fac'
     * already set to the literal weight at create time. multiply_edff_id/
     * multiply_edfc_id have no other flavor and reuse this exact wiring. */
    if (((node.nodedef == multiply_bsdff_id || node.nodedef == multiply_bsdfc_id) &&
         node.outputs.count("out") && node.outputs.at("out") == Type::SurfaceShader) ||
        node.nodedef == multiply_edff_id || node.nodedef == multiply_edfc_id)
    {
      ShaderNode *mix = lowered_nodes.at(node.name);
      ShaderNode *null_bsdf = lowered_nodes.at(node.name + ".multiply_null");
      graph->connect(null_bsdf->output("BSDF"), mix->input("Closure1"));
      graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                     mix->input("Closure2"));
      continue;
    }

    if (node.nodedef == generalized_schlick_edf_id) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      ShaderNode *null_bsdf = lowered_nodes.at(node.name + ".directional_null");
      graph->connect(null_bsdf->output("BSDF"), mix->input("Closure1"));
      graph->connect(lowered_output(node.links.at("base"), nodes_by_name, lowered_nodes),
                     mix->input("Closure2"));
      continue;
    }

    if (node.nodedef == surface_unlit_id) {
      ShaderNode *transmission_bsdf = lowered_nodes.at(node.name + ".unlit_transmission");
      ShaderNode *emission_node = lowered_nodes.at(node.name + ".unlit_emission");
      ShaderNode *sum = lowered_nodes.at(node.name + ".unlit_sum");
      ShaderNode *cutout = lowered_nodes.at(node.name + ".unlit_cutout");
      ShaderNode *mix = lowered_nodes.at(node.name);

      const float trans = std::min(
          1.0f,
          std::max(0.0f,
                   node.inputs.count("transmission") ? node.inputs.at("transmission") : 0.0f));

      if (const auto link = node.links.find("transmission_color"); link != node.links.end()) {
        VectorMathNode *scale = graph->create_node<VectorMathNode>();
        scale->name = node.name + ".unlit_transmission_color_scale";
        scale->set_math_type(NODE_VECTOR_MATH_SCALE);
        scale->set_scale(trans);
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       scale->input("Vector1"));
        graph->connect(scale->output("Vector"), transmission_bsdf->input("Color"));
      }
      if (const auto link = node.links.find("emission_color"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       emission_node->input("Color"));
      }
      if (const auto link = node.links.find("emission"); link != node.links.end()) {
        MathNode *scale = graph->create_node<MathNode>();
        scale->name = node.name + ".unlit_emission_weight_scale";
        scale->set_math_type(NODE_MATH_MULTIPLY);
        scale->set_value2(1.0f - trans);
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       scale->input("Value1"));
        graph->connect(scale->output("Value"), emission_node->input("Strength"));
      }

      graph->connect(transmission_bsdf->output("BSDF"), sum->input("Closure1"));
      graph->connect(emission_node->output("Emission"), sum->input("Closure2"));
      graph->connect(sum->output("Closure"), mix->input("Closure1"));
      graph->connect(cutout->output("BSDF"), mix->input("Closure2"));
      graph->connect(mix->output("Closure"), graph->output()->input("Surface"));
      continue;
    }

    if (is_bsdf_producer(node.nodedef)) {
      ShaderNode *bsdf = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("color"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Color"));
      }
      if (const auto link = node.links.find("tint"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Color"));
      }
      if (const auto link = node.links.find("radius"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Radius"));
      }
      if (const auto link = node.links.find("ior"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("IOR"));
      }
      if (const auto link = node.links.find("extinction"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Extinction"));
      }
      if (const auto link = node.links.find("thinfilm_thickness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Thin Film Thickness"));
      }
      if (const auto link = node.links.find("thinfilm_ior"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Thin Film IOR"));
      }
      if (const auto link = node.links.find("anisotropy"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Anisotropy"));
      }
      if (node.nodedef == oren_nayar_diffuse_bsdf_id || node.nodedef == sheen_bsdf_id) {
        /* Only these two admit a linked (non-literal) "roughness" -- the
         * others' "roughness" is the vector2 anisotropic-roughness input,
         * which validate() requires to be a literal, isotropic value. */
        if (const auto link = node.links.find("roughness"); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                         bsdf->input("Roughness"));
        }
      }
      if (const auto link = node.links.find("absorption_coefficient");
          link != node.links.end())
      {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Absorption Coefficient"));
      }
      if (const auto link = node.links.find("normal"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Normal"));
      }
      continue;
    }

    if (node.nodedef == usd_preview_surface_id) {
      ShaderNode *surface_node = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("diffuseColor"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Base Color"));
      }
      if (const auto link = node.links.find("metallic"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Metallic"));
      }
      if (const auto link = node.links.find("roughness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Roughness"));
      }
      if (const auto link = node.links.find("clearcoat"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat Weight"));
      }
      if (const auto link = node.links.find("clearcoatRoughness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat Roughness"));
      }
      if (const auto link = node.links.find("ior"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("IOR"));
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat IOR"));
      }
      if (const auto link = node.links.find("emissiveColor"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Emission Color"));
      }
      graph->connect(surface_node->output("BSDF"), graph->output()->input("Surface"));
      continue;
    }

    if (node.nodedef == gltf_pbr_id) {
      ShaderNode *surface_node = lowered_nodes.at(node.name);
      if (const auto link = node.links.find("base_color"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Base Color"));
      }
      if (const auto link = node.links.find("metallic"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Metallic"));
      }
      if (const auto link = node.links.find("roughness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Roughness"));
      }
      if (const auto link = node.links.find("clearcoat"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat Weight"));
      }
      if (const auto link = node.links.find("clearcoat_roughness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat Roughness"));
      }
      if (const auto link = node.links.find("ior"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("IOR"));
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat IOR"));
      }
      if (const auto link = node.links.find("emissive"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Emission Color"));
      }
      if (const auto link = node.links.find("emissive_strength"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Emission Strength"));
      }
      graph->connect(surface_node->output("BSDF"), graph->output()->input("Surface"));
      continue;
    }

    if (node.nodedef == disney_principled_id) {
      ShaderNode *surface_node = lowered_nodes.at(node.name);
      ShaderNode *specular_tint = lowered_nodes.at(node.name + ".disney_specular_tint");
      ShaderNode *sheen_tint = lowered_nodes.at(node.name + ".disney_sheen_tint");
      ShaderNode *coat_roughness = lowered_nodes.at(node.name + ".disney_coat_roughness");
      if (const auto link = node.links.find("baseColor"); link != node.links.end()) {
        ShaderOutput *base_color = lowered_output(link->second, nodes_by_name, lowered_nodes);
        graph->connect(base_color, surface_node->input("Base Color"));
        graph->connect(base_color, specular_tint->input("Color2"));
        graph->connect(base_color, sheen_tint->input("Color2"));
      }
      if (const auto link = node.links.find("metallic"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Metallic"));
      }
      if (const auto link = node.links.find("roughness"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Roughness"));
      }
      if (const auto link = node.links.find("anisotropic"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Anisotropic"));
      }
      if (const auto link = node.links.find("specular"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Specular IOR Level"));
      }
      if (const auto link = node.links.find("specularTint"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       specular_tint->input("Fac"));
      }
      if (const auto link = node.links.find("sheen"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Sheen Weight"));
      }
      if (const auto link = node.links.find("sheenTint"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       sheen_tint->input("Fac"));
      }
      if (const auto link = node.links.find("clearcoat"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Coat Weight"));
      }
      if (const auto link = node.links.find("clearcoatGloss"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       coat_roughness->input("Value2"));
      }
      if (const auto link = node.links.find("specTrans"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Transmission Weight"));
      }
      if (const auto link = node.links.find("ior"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("IOR"));
      }
      if (const auto link = node.links.find("subsurface"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Subsurface Weight"));
      }
      if (const auto link = node.links.find("subsurfaceDistance"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       surface_node->input("Subsurface Radius"));
      }
      graph->connect(surface_node->output("BSDF"), graph->output()->input("Surface"));
      continue;
    }

    if (is_bsdf_combinator(node.nodedef)) {
      ShaderNode *combinator = lowered_nodes.at(node.name);
      if (node.nodedef == add_bsdf_id || node.nodedef == lama_add_bsdf_id) {
        if (node.nodedef == lama_add_bsdf_id) {
          ShaderNode *mul1 = lowered_nodes.at(node.name + ".weight1");
          ShaderNode *null1 = lowered_nodes.at(node.name + ".weight1_null");
          ShaderNode *mul2 = lowered_nodes.at(node.name + ".weight2");
          ShaderNode *null2 = lowered_nodes.at(node.name + ".weight2_null");
          graph->connect(null1->output("BSDF"), mul1->input("Closure1"));
          graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                         mul1->input("Closure2"));
          graph->connect(null2->output("BSDF"), mul2->input("Closure1"));
          graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes),
                         mul2->input("Closure2"));
          graph->connect(mul1->output("Closure"), combinator->input("Closure1"));
          graph->connect(mul2->output("Closure"), combinator->input("Closure2"));
        }
        else {
          graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                         combinator->input("Closure1"));
          graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes),
                         combinator->input("Closure2"));
        }
      }
      else if (node.nodedef == mix_bsdf_id || node.nodedef == lama_mix_bsdf_id) {
        graph->connect(lowered_output(node.links.at("bg"), nodes_by_name, lowered_nodes),
                       combinator->input("Closure1"));
        graph->connect(lowered_output(node.links.at("fg"), nodes_by_name, lowered_nodes),
                       combinator->input("Closure2"));
        if (const auto mix_link = node.links.find("mix"); mix_link != node.links.end()) {
          graph->connect(lowered_output(mix_link->second, nodes_by_name, lowered_nodes),
                         combinator->input("Fac"));
        }
      }
      else if (node.nodedef == multiply_bsdff_id || node.nodedef == multiply_bsdfc_id) {
        ShaderNode *null_bsdf = lowered_nodes.at(node.name + ".multiply_null");
        graph->connect(null_bsdf->output("BSDF"), combinator->input("Closure1"));
        graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                       combinator->input("Closure2"));
      }
      continue;
    }

    if (node.nodedef != open_pbr_surface_id) {
      continue;
    }

    ShaderNode *surface_node = lowered_nodes.at(node.name);
    if (const auto base_color = node.links.find("base_color"); base_color != node.links.end()) {
      graph->connect(lowered_output(base_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Base Color"));
    }
    if (const auto base_weight = node.links.find("base_weight"); base_weight != node.links.end()) {
      MathNode *weight_delta = graph->create_node<MathNode>();
      weight_delta->name = node.name + ".base_weight_delta";
      weight_delta->set_math_type(NODE_MATH_SUBTRACT);
      weight_delta->set_value2(1.0f);
      graph->connect(lowered_output(base_weight->second, nodes_by_name, lowered_nodes),
                     weight_delta->input("Value1"));
      graph->connect(weight_delta->output("Value"), surface_node->input("SurfaceMixWeight"));
    }
    if (const auto metalness = node.links.find("base_metalness"); metalness != node.links.end()) {
      graph->connect(lowered_output(metalness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Metallic"));
    }
    if (const auto roughness = node.links.find("specular_roughness");
        roughness != node.links.end())
    {
      graph->connect(lowered_output(roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Roughness"));
    }
    if (const auto ior = node.links.find("specular_ior"); ior != node.links.end()) {
      graph->connect(lowered_output(ior->second, nodes_by_name, lowered_nodes),
                     surface_node->input("IOR"));
    }
    if (const auto opacity = node.links.find("geometry_opacity"); opacity != node.links.end()) {
      graph->connect(lowered_output(opacity->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Alpha"));
    }
    if (const auto emission_color = node.links.find("emission_color");
        emission_color != node.links.end())
    {
      graph->connect(lowered_output(emission_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Emission Color"));
    }
    if (const auto emission_luminance = node.links.find("emission_luminance");
        emission_luminance != node.links.end())
    {
      graph->connect(lowered_output(emission_luminance->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Emission Strength"));
    }
    if (const auto coat_weight = node.links.find("coat_weight"); coat_weight != node.links.end()) {
      graph->connect(lowered_output(coat_weight->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Weight"));
    }
    if (const auto coat_color = node.links.find("coat_color"); coat_color != node.links.end()) {
      graph->connect(lowered_output(coat_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Tint"));
    }
    if (const auto coat_roughness = node.links.find("coat_roughness");
        coat_roughness != node.links.end())
    {
      graph->connect(lowered_output(coat_roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Roughness"));
    }
    if (const auto coat_ior = node.links.find("coat_ior"); coat_ior != node.links.end()) {
      graph->connect(lowered_output(coat_ior->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat IOR"));
    }
    if (const auto fuzz_weight = node.links.find("fuzz_weight"); fuzz_weight != node.links.end()) {
      graph->connect(lowered_output(fuzz_weight->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Weight"));
    }
    if (const auto fuzz_color = node.links.find("fuzz_color"); fuzz_color != node.links.end()) {
      graph->connect(lowered_output(fuzz_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Tint"));
    }
    if (const auto fuzz_roughness = node.links.find("fuzz_roughness");
        fuzz_roughness != node.links.end())
    {
      graph->connect(lowered_output(fuzz_roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Roughness"));
    }
    if (const auto normal = node.links.find("geometry_normal"); normal != node.links.end()) {
      graph->connect(lowered_output(normal->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Normal"));
    }
    if (const auto coat_normal = node.links.find("geometry_coat_normal");
        coat_normal != node.links.end())
    {
      graph->connect(lowered_output(coat_normal->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Normal"));
    }
    graph->connect(surface_node->output("BSDF"), graph->output()->input("Surface"));
  }

  if (source.has_displacement && !source.displacement_is_vector3) {
    /* ND_displacement_float: scalar height displacement along the surface
     * normal -- DisplacementNode is the direct, honest Cycles equivalent. */
    DisplacementNode *displacement = graph->create_node<DisplacementNode>();
    displacement->name = "Displacement";
    displacement->set_midlevel(0.0f);
    if (source.displacement.is_linked) {
      graph->connect(lowered_output(source.displacement.link, nodes_by_name, lowered_nodes),
                     displacement->input("Height"));
    }
    else {
      displacement->set_height(source.displacement.value);
    }
    if (source.displacement_scale.is_linked) {
      graph->connect(lowered_output(source.displacement_scale.link, nodes_by_name, lowered_nodes),
                     displacement->input("Scale"));
    }
    else {
      displacement->set_scale(source.displacement_scale.value);
    }
    graph->connect(displacement->output("Displacement"), graph->output()->input("Displacement"));
  }
  else if (source.has_displacement && source.displacement_is_vector3) {
    /* ND_displacement_vector3: "Vector displacement in (dPdu, dPdv, N)
     * tangent/normal space" (pbrlib/pbrlib_defs.mtlx) -- VectorDisplacementNode
     * defaults to exactly that tangent-space convention, so this is a direct,
     * honest Cycles equivalent, not a substitute mapping. */
    VectorDisplacementNode *displacement = graph->create_node<VectorDisplacementNode>();
    displacement->name = "Displacement";
    displacement->set_space(NODE_NORMAL_MAP_TANGENT);
    displacement->set_midlevel(0.0f);
    if (source.displacement_vector3.is_linked) {
      graph->connect(
          lowered_output(source.displacement_vector3.link, nodes_by_name, lowered_nodes),
          displacement->input("Vector"));
    }
    else {
      displacement->set_vector(source.displacement_vector3.value);
    }
    if (source.displacement_scale.is_linked) {
      graph->connect(lowered_output(source.displacement_scale.link, nodes_by_name, lowered_nodes),
                     displacement->input("Scale"));
    }
    else {
      displacement->set_scale(source.displacement_scale.value);
    }
    graph->connect(displacement->output("Displacement"), graph->output()->input("Displacement"));
  }

  /* Task 3: volume terminal. Previously read_usdshade_graph() never queried
   * the material's volume output at all, so a co-authored ND_volume closure
   * was silently dropped no matter what the surface/displacement terminals
   * contained. VolumeCoefficientsNode is the native Cycles node whose
   * scatter_coeffs/absorption_coeffs/anisotropy fields are a direct,
   * physically-based match for MaterialX's ND_anisotropic_vdf
   * (scattering/absorption/anisotropy) and, with scatter_coeffs left at
   * zero, for ND_absorption_vdf (absorption only). ND_volume's optional
   * 'edf' input (when it resolves to ND_uniform_edf) maps onto this same
   * node's "Emission Coefficients" socket -- previously always hardcoded
   * to zero here regardless of what read_usdshade_graph() discovered. */
  if (source.has_volume) {
    VolumeCoefficientsNode *volume = graph->create_node<VolumeCoefficientsNode>();
    volume->name = "Volume";
    volume->set_absorption_coeffs(source.volume_absorption.value);
    volume->set_scatter_coeffs(source.volume_scattering.value);
    volume->set_anisotropy(source.volume_anisotropy.value);
    volume->set_emission_coeffs(source.volume_emission.value);
    if (source.volume_absorption.is_linked) {
      graph->connect(lowered_output(source.volume_absorption.link, nodes_by_name, lowered_nodes),
                     volume->input("Absorption Coefficients"));
    }
    if (source.volume_scattering.is_linked) {
      graph->connect(lowered_output(source.volume_scattering.link, nodes_by_name, lowered_nodes),
                     volume->input("Scatter Coefficients"));
    }
    if (source.volume_anisotropy.is_linked) {
      graph->connect(lowered_output(source.volume_anisotropy.link, nodes_by_name, lowered_nodes),
                     volume->input("Anisotropy"));
    }
    if (source.volume_emission.is_linked) {
      graph->connect(lowered_output(source.volume_emission.link, nodes_by_name, lowered_nodes),
                     volume->input("Emission Coefficients"));
    }
    graph->connect(volume->output("Volume"), graph->output()->input("Volume"));
  }

  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
