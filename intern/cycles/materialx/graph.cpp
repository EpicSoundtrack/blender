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
constexpr const char *range_color3_id = "ND_range_color3";
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
constexpr const char *checkerboard_color3_id = "ND_checkerboard_color3";
constexpr const char *rgbtohsv_color3_id = "ND_rgbtohsv_color3";
constexpr const char *hsvtorgb_color3_id = "ND_hsvtorgb_color3";
constexpr const char *remap_vector2_id = "ND_remap_vector2";
constexpr const char *range_vector2_id = "ND_range_vector2";
constexpr const char *remap_vector2fa_id = "ND_remap_vector2FA";
constexpr const char *remap_vector3_id = "ND_remap_vector3";
constexpr const char *remap_vector3fa_id = "ND_remap_vector3FA";
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
constexpr const char *image_float_id = "ND_image_float";
constexpr const char *image_color3_id = "ND_image_color3";
constexpr const char *image_color4_id = "ND_image_color4";
constexpr const char *constant_color4_id = "ND_constant_color4";
/** Task 4: the only native Vector4 lowerer implemented in this pass --
 *  everything else (image_vector4, arithmetic ops, ramps, splits) is a
 *  documented boundary, matching how constant_color4/image_color4/color4
 *  operations were each added incrementally. */
constexpr const char *constant_vector4_id = "ND_constant_vector4";
/** Task 5: boolean/integer exact-domain observation. */
constexpr const char *constant_boolean_id = "ND_constant_boolean";
constexpr const char *constant_integer_id = "ND_constant_integer";
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
constexpr const char *extract_color4_id = "ND_extract_color4";
constexpr const char *convert_color4_color3_id = "ND_convert_color4_color3";
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
constexpr const char *safepower_vector2_id = "ND_safepower_vector2";
constexpr const char *safepower_vector2_fa_id = "ND_safepower_vector2FA";
constexpr const char *safepower_vector3_id = "ND_safepower_vector3";
constexpr const char *safepower_vector3_fa_id = "ND_safepower_vector3FA";
constexpr const char *transformpoint_vector3_id = "ND_transformpoint_vector3";
constexpr const char *transformvector_vector3_id = "ND_transformvector_vector3";
constexpr const char *transformnormal_vector3_id = "ND_transformnormal_vector3";
constexpr const char *open_pbr_surface_id = "ND_open_pbr_surface_surfaceshader";
constexpr const char *surface_unlit_id = "ND_surface_unlit";
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

bool is_bsdf_producer(const string &nodedef)
{
  return nodedef == oren_nayar_diffuse_bsdf_id || nodedef == translucent_bsdf_id ||
         nodedef == sheen_bsdf_id || nodedef == subsurface_bsdf_id ||
         nodedef == conductor_bsdf_id || nodedef == dielectric_bsdf_id;
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

bool is_bsdf_combinator(const string &nodedef)
{
  return nodedef == add_bsdf_id || nodedef == mix_bsdf_id || nodedef == multiply_bsdff_id ||
         nodedef == multiply_bsdfc_id;
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
         nodedef == noise2d_color3fa_id || nodedef == noise2d_vector2_id ||
         nodedef == noise2d_vector2fa_id || nodedef == noise2d_vector3_id ||
         nodedef == noise2d_vector3fa_id || nodedef == noise3d_float_id ||
         nodedef == noise3d_color3_id || nodedef == noise3d_color3fa_id ||
         nodedef == noise3d_vector2_id || nodedef == noise3d_vector2fa_id ||
         nodedef == noise3d_vector3_id || nodedef == noise3d_vector3fa_id;
}

bool is_native_fractal2d_family(const string &nodedef)
{
  return nodedef == fractal2d_float_id || nodedef == fractal2d_color3_id ||
         nodedef == fractal2d_color3fa_id || nodedef == fractal2d_vector2_id ||
         nodedef == fractal2d_vector2fa_id || nodedef == fractal2d_vector3_id ||
         nodedef == fractal2d_vector3fa_id;
}

bool is_native_fractal3d_family(const string &nodedef)
{
  return nodedef == fractal3d_float_id || nodedef == fractal3d_color3_id ||
         nodedef == fractal3d_color3fa_id || nodedef == fractal3d_vector2_id ||
         nodedef == fractal3d_vector2fa_id || nodedef == fractal3d_vector3_id ||
         nodedef == fractal3d_vector3fa_id;
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
         nodedef == noise2d_vector2fa_id || nodedef == noise2d_vector3fa_id ||
         nodedef == noise3d_float_id || nodedef == noise3d_color3fa_id ||
         nodedef == noise3d_vector2fa_id || nodedef == noise3d_vector3fa_id ||
         nodedef == fractal2d_float_id || nodedef == fractal2d_color3fa_id ||
         nodedef == fractal2d_vector2fa_id || nodedef == fractal2d_vector3fa_id ||
         nodedef == fractal3d_float_id || nodedef == fractal3d_color3fa_id ||
         nodedef == fractal3d_vector2fa_id || nodedef == fractal3d_vector3fa_id;
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
         is_safepower_color4(nodedef) || nodedef == clamp_color4fa_id ||
         color4_binary_math_type(nodedef, nullptr);
}

bool color4_has_finite_components(const float4 &value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
         std::isfinite(value.w);
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

bool is_vector_conditional(const string &nodedef)
{
  return nodedef == ifgreater_vector3_id || nodedef == ifgreatereq_vector3_id ||
         nodedef == ifequal_vector3_id;
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

bool is_linear_range_float(const string &nodedef)
{
  return nodedef == remap_float_id || nodedef == range_float_id;
}

bool is_linear_range_color3(const string &nodedef)
{
  return nodedef == remap_color3_id || nodedef == range_color3_id;
}

bool is_linear_range_vector2(const string &nodedef)
{
  return nodedef == remap_vector2_id || nodedef == range_vector2_id ||
         nodedef == remap_vector2fa_id;
}

bool is_linear_range_vector3(const string &nodedef)
{
  return nodedef == remap_vector3_id || nodedef == remap_vector3fa_id;
}

bool is_linear_range_scalar_bounds(const string &nodedef)
{
  return nodedef == remap_vector2fa_id || nodedef == remap_vector3fa_id;
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
        node.nodedef != constant_color4_id &&
        !is_color4_operation(node.nodedef) && !is_color4_ramp(node.nodedef) &&
        !is_color4_split(node.nodedef))
    {
      return false;
    }
    /* Task 4: Vector4 literals only exist on ND_constant_vector4 nodes in
     * this pass -- no other Vector4 lowerer is implemented yet. */
    if (!node.vector4_inputs.empty() && node.nodedef != constant_vector4_id) {
      return false;
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

    if (is_float_conditional(node.nodedef)) {
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal == node.inputs.end() ? validate_link(link->second, Type::Float, *nodes_by_name) :
                                               std::isfinite(literal->second));
      };
      if (!valid_operand("value1") || !valid_operand("value2") || !valid_operand("in1") ||
          !valid_operand("in2") || node.inputs.size() + node.links.size() != 4 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
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

    if (is_linear_range_color3(node.nodedef)) {
      const auto input = node.color3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto valid_finite_input = [&](const char *name) {
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
          node.color3_inputs.at("inlow").x == node.color3_inputs.at("inhigh").x ||
          node.color3_inputs.at("inlow").y == node.color3_inputs.at("inhigh").y ||
          node.color3_inputs.at("inlow").z == node.color3_inputs.at("inhigh").z ||
          node.color3_inputs.size() != (input == node.color3_inputs.end() ? 4 : 5) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Color3 || node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == remap_color3_id) {
        if (!node.int_inputs.empty()) return false;
      }
      else {
        const auto doclamp = node.int_inputs.find("doclamp");
        const float3 &outlow = node.color3_inputs.at("outlow");
        const float3 &outhigh = node.color3_inputs.at("outhigh");
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
          node.vector3_inputs.size() != size_t(!scalar_amplitude && !is_vector2 && !is_float) ||
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
      if (!node.int_inputs.empty()) return false;
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
      const bool scalar_clamp = node.nodedef == clamp_color4fa_id;
      const char *first_name = (unary || scalar_invert || scalar_clamp) ?
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
      const bool valid_non_clamp =
          !scalar_clamp && valid_color4_operand(first_name, first_default) &&
          (unary ||
           (scalar_second ? valid_float_operand(second_name, second_default.x) :
                            valid_color4_operand(second_name, second_default))) &&
          node.float4_inputs.size() + node.inputs.size() + node.links.size() <= (unary ? 1 : 2);
      if ((!valid_scalar_clamp && !valid_non_clamp) ||
          std::any_of(node.float4_inputs.begin(),
                      node.float4_inputs.end(),
                      [&](const auto &input) {
                        return scalar_clamp ? input.first != "in" :
                                              input.first != first_name &&
                                                  (unary || scalar_second ||
                                                   input.first != second_name);
                      }) ||
          std::any_of(node.links.begin(), node.links.end(), [&](const auto &input) {
            return scalar_clamp ?
                       input.first != "in" && input.first != "low" && input.first != "high" :
                       input.first != first_name && (unary || input.first != second_name);
          }) ||
          std::any_of(node.inputs.begin(), node.inputs.end(), [&](const auto &input) {
            return scalar_clamp ? input.first != "low" && input.first != "high" :
                                  (!scalar_second || input.first != second_name);
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

    if (node.nodedef == image_float_id || node.nodedef == image_vector2_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      const Type output_type = node.nodedef == image_float_id ? Type::Float : Type::Vector2;
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != output_type ||
          node.asset_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty())
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
           !is_vector_conditional(linked_source->second->nodedef) &&
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
        node.nodedef == convert_vector2_vector3_id)
    {
      const Type input_type = node.nodedef == convert_color3_vector3_id || node.nodedef == convert_color3_vector2_id ? Type::Color3 :
                              node.nodedef == convert_vector3_color3_id ? Type::Vector3 :
                              node.nodedef == convert_vector2_color3_id || node.nodedef == convert_vector2_vector3_id ? Type::Vector2 : Type::Float;
      const Type output_type = node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id ? Type::Color3 :
                               node.nodedef == convert_float_vector2_id || node.nodedef == convert_color3_vector2_id ? Type::Vector2 : Type::Vector3;
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

    /* Task 5: boolean/integer exact-domain observation. Both share the
     * existing `int_inputs` storage (already used elsewhere for internal
     * node parameters like "octaves"/"doclamp"/"index") -- distinguished
     * by the node's own nodedef/output tag, exactly like the Color3/Color4
     * float4_inputs-sharing precedent. */
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

    if (is_bsdf_producer(node.nodedef)) {
      const auto output = node.outputs.find("out");
      const bool allows_roughness_vector2 = node.nodedef == conductor_bsdf_id ||
                                            node.nodedef == dielectric_bsdf_id;
      if (output == node.outputs.end() || output->second != Type::BSDF ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.float4_inputs.empty() ||
          (!node.vector2_inputs.empty() &&
           (!allows_roughness_vector2 ||
            !(node.vector2_inputs.size() == 1 && node.vector2_inputs.contains("roughness")))) ||
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

      if (node.nodedef == oren_nayar_diffuse_bsdf_id) {
        allowed_float = {"weight", "roughness"};
        allowed_color3 = {"color"};
        /* energy_compensation has no Cycles equivalent (DiffuseBsdfNode
         * doesn't model it) -- the common top-of-function guard already
         * rejects any authored int_inputs (booleans are stored there, see
         * ND_constant_boolean above), so only the MaterialX default
         * (false, i.e. entirely absent) is admitted here. */
        ok = weight_literal_ok(1.0f) && valid_float("roughness", 0.0f) &&
             valid_color3("color", make_float3(0.18f, 0.18f, 0.18f)) && valid_vector3_opt("normal");
      }
      else if (node.nodedef == translucent_bsdf_id) {
        allowed_float = {"weight"};
        allowed_color3 = {"color"};
        ok = weight_literal_ok(1.0f) &&
             valid_color3("color", make_float3(1.0f, 1.0f, 1.0f)) && valid_vector3_opt("normal");
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
      else if (node.nodedef == subsurface_bsdf_id) {
        allowed_float = {"weight", "anisotropy"};
        allowed_color3 = {"color", "radius"};
        ok = weight_literal_ok(1.0f) && valid_float("anisotropy", 0.0f) &&
             valid_color3("color", make_float3(0.18f, 0.18f, 0.18f)) &&
             valid_color3("radius", make_float3(1.0f, 1.0f, 1.0f)) && valid_vector3_opt("normal");
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
      if (node.nodedef == add_bsdf_id) {
        const auto in1 = node.links.find("in1");
        const auto in2 = node.links.find("in2");
        ok = in1 != node.links.end() && in2 != node.links.end() &&
             validate_link(in1->second, Type::BSDF, *nodes_by_name) &&
             validate_link(in2->second, Type::BSDF, *nodes_by_name) && node.links.size() == 2 &&
             node.inputs.empty() && node.color3_inputs.empty();
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
      else if (node.nodedef == mix_bsdf_id) {
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
      ((source.displacement.is_linked &&
        (source.displacement.link.type != Type::Float ||
         !validate_link(source.displacement.link, Type::Float, *nodes_by_name))) ||
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

ShaderOutput *lowered_output(const Link &link,
                             const unordered_map<string, const Node *> &nodes_by_name,
                             const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  ShaderNode *lowered = lowered_nodes.at(link.source_node);
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
  if (source.nodedef == extract_vector3_id) {
    static const char *channels[] = {"X", "Y", "Z"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (source.nodedef == separate3_vector3_id) {
    if (link.source_output == "outx") return lowered->output("X");
    if (link.source_output == "outy") return lowered->output("Y");
    if (link.source_output == "outz") return lowered->output("Z");
    return nullptr;
  }
  if (source.nodedef == separate3_color3_id) {
    if (link.source_output == "outx") return lowered->output("Red");
    if (link.source_output == "outy") return lowered->output("Green");
    if (link.source_output == "outz") return lowered->output("Blue");
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
    if (source.nodedef == image_float_id) {
      return lowered->output("Red");
    }
    return lowered->output("Value");
  }
  if (link.type == Type::Vector2) {
    if (source.nodedef == clamp_vector2fa_id) {
      return lowered->output("Vector");
    }
    if (source.nodedef == geompropvalue_vector2_id) {
      return lowered->output("UV");
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
        source.nodedef == convert_vector2_vector3_id || is_space_transform(source.nodedef) ||
        is_native_fractal2d_family(source.nodedef) ||
        source.nodedef == mix_vector3_id || vector_math_type(source.nodedef, nullptr) ||
        vector3_binary_component_math_type(source.nodedef, nullptr) || is_safepower_vector3(source.nodedef) ||
        vector3_domain_math_type(source.nodedef, nullptr) ||
        vector3_atan2_type(source.nodedef, nullptr) || vector3_invert_type(source.nodedef, nullptr) ||
        vector3_smoothstep_type(source.nodedef, nullptr) || source.nodedef == clamp_vector3_id ||
        source.nodedef == clamp_vector3fa_id || is_linear_range_vector3(source.nodedef) ||
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
    if (source.nodedef == geompropvalue_vector3_id) {
      return lowered->output("Normal");
    }
  }
  if (link.type == Type::Color4) {
    if (source.nodedef == image_color4_id || source.nodedef == constant_color4_id ||
        is_color4_operation(source.nodedef) ||
        is_color4_ramp(source.nodedef) || is_color4_split(source.nodedef)) {
      return lowered->output("Color");
    }
  }
  if (link.type == Type::Vector4) {
    if (source.nodedef == constant_vector4_id) {
      return lowered->output("Vector");
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
  return nullptr;
}

ShaderOutput *lowered_color4_alpha_output(
    const Link &link,
    const unordered_map<string, const Node *> &nodes_by_name,
    const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  if (source.nodedef == image_color4_id) {
    return lowered_nodes.at(link.source_node)->output("Alpha");
  }
  if (source.nodedef == constant_color4_id) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
  }
  if (is_color4_operation(source.nodedef)) {
    return lowered_nodes
        .at(link.source_node +
            (source.nodedef == clamp_color4fa_id ?
                 ".Alpha.maximum" :
                 (is_safepower_color4(source.nodedef) ? ".Alpha.multiply" : ".Alpha")))
        ->output("Value");
  }
  if (is_color4_ramp(source.nodedef) || is_color4_split(source.nodedef)) {
    return lowered_nodes.at(link.source_node + ".Alpha")->output("Value");
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
    else if (node.nodedef == convert_float_color3_id) {
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
    else if (node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id) {
      SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
      separate->name = node.name + ".separate";
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      if (node.nodedef == convert_vector2_color3_id) combine->set_b(0.0f);
      lowered_nodes.emplace(separate->name, separate);
      lowered = combine;
    }
    else if (node.nodedef == convert_float_vector3_id || node.nodedef == convert_float_vector2_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (node.nodedef == convert_float_vector2_id) combine->set_z(0.0f);
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
      const bool scalar_clamp = node.nodedef == clamp_color4fa_id;
      SeparateColorNode *first = graph->create_node<SeparateColorNode>();
      first->name = node.name +
                    ((unary || scalar_invert || scalar_clamp) ?
                         ".input" :
                         (invert ? ".amount" : ".first"));
      first->set_color_type(NODE_COMBSEP_COLOR_RGB);
      SeparateColorNode *second = nullptr;
      if (!unary && !scalar_second && !scalar_clamp) {
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
      for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
        if (scalar_clamp) {
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
      const float3 &inlow = node.color3_inputs.at("inlow");
      const float3 &inhigh = node.color3_inputs.at("inhigh");
      const float3 &outlow = node.color3_inputs.at("outlow");
      const float3 &outhigh = node.color3_inputs.at("outhigh");
      const float3 input = node.color3_inputs.count("in") ? node.color3_inputs.at("in") : zero_float3();
      for (const auto &[channel, from_min, from_max, to_min, to_max, value] :
           {std::tuple{"Red", inlow.x, inhigh.x, outlow.x, outhigh.x, input.x},
            std::tuple{"Green", inlow.y, inhigh.y, outlow.y, outhigh.y, input.y},
            std::tuple{"Blue", inlow.z, inhigh.z, outlow.z, outhigh.z, input.z}})
      {
        MapRangeNode *range = graph->create_node<MapRangeNode>();
        range->name = node.name + "." + channel;
        range->set_range_type(NODE_MAP_RANGE_LINEAR);
        range->set_clamp(node.nodedef == range_color3_id && node.int_inputs.at("doclamp") != 0);
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
      range->set_use_clamp(false);
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
        CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
        combine->set_z(0.0f);
        for (const char *channel : {"X", "Y", "Z"}) {
          if (vector2 && channel[0] == 'Z') {
            continue;
          }
          MathNode *amplitude = graph->create_node<MathNode>();
          amplitude->name = node.name + "." + channel + ".amplitude";
          amplitude->set_math_type(NODE_MATH_MULTIPLY);
          amplitude->set_value2(scalar_amplitude ?
                                    node.inputs.at("amplitude") :
                                    (channel[0] == 'X' ?
                                         (vector2 ? node.vector2_inputs.at("amplitude").x :
                                                    node.vector3_inputs.at("amplitude").x) :
                                     channel[0] == 'Y' ?
                                         (vector2 ? node.vector2_inputs.at("amplitude").y :
                                                    node.vector3_inputs.at("amplitude").y) :
                                         node.vector3_inputs.at("amplitude").z));
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
    else if (node.nodedef == geompropvalue_float_id || node.nodedef == geompropvalue_color3_id) {
      AttributeNode *attribute = graph->create_node<AttributeNode>();
      attribute->set_attribute(ustring(node.string_inputs.at("geomprop")));
      lowered = attribute;
    }
    else if (node.nodedef == geompropvalue_vector3_id) {
      lowered = graph->create_node<GeometryNode>();
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
                                       (color4_source.nodedef == clamp_color4fa_id ?
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
        if (node.nodedef == oren_nayar_diffuse_bsdf_id) {
          DiffuseBsdfNode *diffuse = graph->create_node<DiffuseBsdfNode>();
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(0.18f, 0.18f, 0.18f);
            diffuse->set_color(color * weight);
          }
          if (!node.links.contains("roughness")) {
            diffuse->set_roughness(
                node.inputs.contains("roughness") ? node.inputs.at("roughness") : 0.0f);
          }
          lowered = diffuse;
        }
        else if (node.nodedef == translucent_bsdf_id) {
          TranslucentBsdfNode *translucent = graph->create_node<TranslucentBsdfNode>();
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(1.0f, 1.0f, 1.0f);
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
        else if (node.nodedef == subsurface_bsdf_id) {
          SubsurfaceScatteringNode *sss = graph->create_node<SubsurfaceScatteringNode>();
          sss->set_method(CLOSURE_BSSRDF_RANDOM_WALK_ID);
          if (!node.links.contains("color")) {
            const float3 color = node.color3_inputs.contains("color") ?
                                     node.color3_inputs.at("color") :
                                     make_float3(0.18f, 0.18f, 0.18f);
            sss->set_color(color * weight);
          }
          if (!node.links.contains("radius")) {
            sss->set_radius(node.color3_inputs.contains("radius") ?
                                node.color3_inputs.at("radius") :
                                make_float3(1.0f, 1.0f, 1.0f));
          }
          if (!node.links.contains("anisotropy")) {
            sss->set_subsurface_anisotropy(
                node.inputs.contains("anisotropy") ? node.inputs.at("anisotropy") : 0.0f);
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
      else if (is_bsdf_combinator(node.nodedef)) {
        if (node.nodedef == add_bsdf_id) {
          AddClosureNode *add = graph->create_node<AddClosureNode>();
          lowered = add;
        }
        else if (node.nodedef == mix_bsdf_id) {
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
      ShaderNode *noise = lowered_nodes.at(node.name + ".noise");
      graph->connect(lowered_output(node.links.at(native_noise_or_fractal_is_3d(node.nodedef) ?
                                                     "position" :
                                                     "texcoord"),
                                    nodes_by_name,
                                    lowered_nodes),
                     noise->input("Vector"));
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
          graph->connect(amplitude->output("Value"), combine->input(channel));
        }
        else {
          ShaderNode *pivot = lowered_nodes.at(node.name + "." + channel);
          graph->connect(amplitude->output("Value"), pivot->input("Value1"));
          graph->connect(pivot->output("Value"), combine->input(channel));
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
    if (node.nodedef == convert_vector3_color3_id || node.nodedef == convert_vector2_color3_id) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes), separate->input("Vector"));
      graph->connect(separate->output("X"), combine->input("Red"));
      graph->connect(separate->output("Y"), combine->input("Green"));
      if (node.nodedef == convert_vector3_color3_id) graph->connect(separate->output("Z"), combine->input("Blue"));
      continue;
    }
    if (node.nodedef == convert_float_vector3_id || node.nodedef == convert_float_vector2_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      graph->connect(input, combine->input("X")); graph->connect(input, combine->input("Y")); if (node.nodedef == convert_float_vector3_id) graph->connect(input, combine->input("Z"));
      continue;
    }
    if (node.nodedef == convert_vector2_vector3_id) {
      ShaderNode *separate=lowered_nodes.at(node.name+".separate"), *combine=lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"),nodes_by_name,lowered_nodes),separate->input("Vector")); graph->connect(separate->output("X"),combine->input("X")); graph->connect(separate->output("Y"),combine->input("Y")); continue;
    }
    if (is_color4_operation(node.nodedef)) {
      const bool unary = color4_unary_math_type(node.nodedef, nullptr);
      const bool scalar_invert = node.nodedef == invert_color4fa_id;
      const bool invert = is_color4_invert(node.nodedef);
      const bool safepower = is_safepower_color4(node.nodedef);
      const bool scalar_second = color4_binary_uses_scalar_second(node.nodedef);
      const bool scalar_clamp = node.nodedef == clamp_color4fa_id;
      const char *first_name = (unary || scalar_invert || scalar_clamp) ?
                                   "in" :
                                   (invert ? "amount" : "in1");
      const char *second_name = scalar_invert ? "amount" : (invert ? "in" : "in2");
      ShaderNode *first = lowered_nodes.at(
          node.name + ((unary || scalar_invert || scalar_clamp) ?
                           ".input" :
                           (invert ? ".amount" : ".first")));
      ShaderNode *second = (unary || scalar_second || scalar_clamp) ?
                               nullptr :
                               lowered_nodes.at(node.name +
                                                (invert ? ".input" : ".second"));
      ShaderNode *combine = lowered_nodes.at(node.name);
      if (const auto link = node.links.find(first_name); link != node.links.end()) {
        graph->connect(
            lowered_output(link->second, nodes_by_name, lowered_nodes), first->input("Color"));
      }
      if (!unary && !scalar_clamp) {
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
          if (scalar_clamp) {
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
        else if (scalar_clamp) {
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
        if (!unary && !scalar_clamp) {
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
        if (scalar_clamp) {
          ShaderNode *minimum = lowered_nodes.at(node.name + "." + channel + ".minimum");
          ShaderNode *maximum = lowered_nodes.at(node.name + "." + channel + ".maximum");
          if (first_output) {
            graph->connect(first_output, minimum->input("Value1"));
          }
          if (const auto high = node.inputs.find("high"); high != node.inputs.end()) {
            static_cast<MathNode *>(minimum)->set_value2(high->second);
          }
          else if (const auto high_link = node.links.find("high");
                   high_link != node.links.end())
          {
            graph->connect(lowered_output(high_link->second, nodes_by_name, lowered_nodes),
                           minimum->input("Value2"));
          }
          else {
            static_cast<MathNode *>(minimum)->set_value2(1.0f);
          }
          graph->connect(minimum->output("Value"), maximum->input("Value1"));
          if (const auto low = node.inputs.find("low"); low != node.inputs.end()) {
            static_cast<MathNode *>(maximum)->set_value2(low->second);
          }
          else if (const auto low_link = node.links.find("low"); low_link != node.links.end()) {
            graph->connect(lowered_output(low_link->second, nodes_by_name, lowered_nodes),
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
        node.nodedef == image_vector3_id)
    {
      ShaderNode *image_node = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     image_node->input("Vector"));
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
      if (const auto link = node.links.find("normal"); link != node.links.end()) {
        graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes),
                       bsdf->input("Normal"));
      }
      continue;
    }

    if (is_bsdf_combinator(node.nodedef)) {
      ShaderNode *combinator = lowered_nodes.at(node.name);
      if (node.nodedef == add_bsdf_id) {
        graph->connect(lowered_output(node.links.at("in1"), nodes_by_name, lowered_nodes),
                       combinator->input("Closure1"));
        graph->connect(lowered_output(node.links.at("in2"), nodes_by_name, lowered_nodes),
                       combinator->input("Closure2"));
      }
      else if (node.nodedef == mix_bsdf_id) {
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

  if (source.has_displacement) {
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
