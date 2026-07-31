/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/usdshade_reader.h"

#include <cmath>
#include <unordered_map>
#include <unordered_set>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/types.h>
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
constexpr const char *noise3d_float_id = "ND_noise3d_float";
constexpr const char *noise3d_color3_id = "ND_noise3d_color3";
constexpr const char *noise3d_color3fa_id = "ND_noise3d_color3FA";
constexpr const char *noise3d_vector2_id = "ND_noise3d_vector2";
constexpr const char *noise3d_vector2fa_id = "ND_noise3d_vector2FA";
constexpr const char *noise3d_vector3_id = "ND_noise3d_vector3";
constexpr const char *noise3d_vector3fa_id = "ND_noise3d_vector3FA";
constexpr const char *fractal2d_float_id = "ND_fractal2d_float";
constexpr const char *fractal2d_color3_id = "ND_fractal2d_color3";
constexpr const char *fractal2d_color3fa_id = "ND_fractal2d_color3FA";
constexpr const char *fractal2d_vector2_id = "ND_fractal2d_vector2";
constexpr const char *fractal2d_vector2fa_id = "ND_fractal2d_vector2FA";
constexpr const char *fractal2d_vector3_id = "ND_fractal2d_vector3";
constexpr const char *fractal2d_vector3fa_id = "ND_fractal2d_vector3FA";
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
constexpr const char *geompropvalue_vector3_id = "ND_geompropvalue_vector3";
constexpr const char *image_float_id = "ND_image_float";
constexpr const char *image_color3_id = "ND_image_color3";
constexpr const char *image_color4_id = "ND_image_color4";
constexpr const char *absval_color4_id = "ND_absval_color4";
constexpr const char *ceil_color4_id = "ND_ceil_color4";
constexpr const char *floor_color4_id = "ND_floor_color4";
constexpr const char *fract_color4_id = "ND_fract_color4";
constexpr const char *round_color4_id = "ND_round_color4";
constexpr const char *sign_color4_id = "ND_sign_color4";
constexpr const char *invert_color4_id = "ND_invert_color4";
constexpr const char *safepower_color4_id = "ND_safepower_color4";
constexpr const char *add_color4_id = "ND_add_color4";
constexpr const char *subtract_color4_id = "ND_subtract_color4";
constexpr const char *multiply_color4_id = "ND_multiply_color4";
constexpr const char *divide_color4_id = "ND_divide_color4";
constexpr const char *min_color4_id = "ND_min_color4";
constexpr const char *max_color4_id = "ND_max_color4";
constexpr const char *modulo_color4_id = "ND_modulo_color4";
constexpr const char *power_color4_id = "ND_power_color4";
constexpr const char *image_vector2_id = "ND_image_vector2";
constexpr const char *image_vector3_id = "ND_image_vector3";
constexpr const char *extract_color4_id = "ND_extract_color4";
constexpr const char *convert_color4_color3_id = "ND_convert_color4_color3";
constexpr const char *normalmap_float_id = "ND_normalmap_float";
constexpr const char *constant_vector3_id = "ND_constant_vector3";
constexpr const char *constant_vector2_id = "ND_constant_vector2";
constexpr const char *combine2_vector2_id = "ND_combine2_vector2";
constexpr const char *convert_vector3_vector2_id = "ND_convert_vector3_vector2";
constexpr const char *place2d_vector2_id = "ND_place2d_vector2";
constexpr const char *extract_vector2_id = "ND_extract_vector2";
constexpr const char *ramplr_color3_id = "ND_ramplr_color3";
constexpr const char *ramptb_color3_id = "ND_ramptb_color3";
constexpr const char *ramplr_float_id = "ND_ramplr_float";
constexpr const char *ramptb_float_id = "ND_ramptb_float";
constexpr const char *combine3_vector3_id = "ND_combine3_vector3";
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
constexpr const char *displacement_shader_id = "ND_displacementshader";
const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);

bool read_vector2_output(const pxr::UsdShadeInput &input,
                         Graph *graph,
                         Link *result,
                         std::unordered_set<string> *active_shaders,
                         int depth,
                         string *error_message);

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
                              string *error_message)
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
                                           error_message));
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
                                           error_message));
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
      (source_name.GetString() != "out" && source_id.GetString() != separate3_vector3_id &&
       source_id.GetString() != separate3_color3_id) ||
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
                      string *error_message)
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
                                error_message))
  {
    if (error_message) {
      *error_message = string("MaterialX input '") + input.GetBaseName().GetString() +
                       "': " + *error_message;
    }
    return false;
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

bool is_color_conditional(const string &nodedef)
{
  return nodedef == ifgreater_color3_id || nodedef == ifgreatereq_color3_id ||
         nodedef == ifequal_color3_id;
}

bool is_vector_conditional(const string &nodedef)
{
  return nodedef == ifgreater_vector3_id || nodedef == ifgreatereq_vector3_id ||
         nodedef == ifequal_vector3_id;
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

bool is_color4_operation(const string &nodedef)
{
  return is_color4_unary_math(nodedef) || nodedef == invert_color4_id ||
         nodedef == safepower_color4_id || is_color4_binary_math(nodedef);
}

bool color4_is_finite(const pxr::GfVec4f &value)
{
  return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]) &&
         std::isfinite(value[3]);
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

bool is_native_noise_or_fractal_family(const string &nodedef)
{
  return is_native_noise_family(nodedef) || is_native_fractal2d_family(nodedef);
}

bool native_noise_or_fractal_is_3d(const string &nodedef)
{
  return nodedef.find("noise3d") != string::npos;
}

bool native_noise_or_fractal_is_float(const string &nodedef)
{
  return nodedef == noise2d_float_id || nodedef == noise3d_float_id ||
         nodedef == fractal2d_float_id;
}

bool native_noise_or_fractal_is_color3(const string &nodedef)
{
  return nodedef == noise2d_color3_id || nodedef == noise2d_color3fa_id ||
         nodedef == noise3d_color3_id || nodedef == noise3d_color3fa_id ||
         nodedef == fractal2d_color3_id || nodedef == fractal2d_color3fa_id;
}

bool native_noise_or_fractal_is_vector2(const string &nodedef)
{
  return nodedef == noise2d_vector2_id || nodedef == noise2d_vector2fa_id ||
         nodedef == noise3d_vector2_id || nodedef == noise3d_vector2fa_id ||
         nodedef == fractal2d_vector2_id || nodedef == fractal2d_vector2fa_id;
}

bool native_noise_or_fractal_uses_scalar_amplitude(const string &nodedef)
{
  return nodedef == noise2d_float_id || nodedef == noise2d_color3fa_id ||
         nodedef == noise2d_vector2fa_id || nodedef == noise2d_vector3fa_id ||
         nodedef == noise3d_float_id || nodedef == noise3d_color3fa_id ||
         nodedef == noise3d_vector2fa_id || nodedef == noise3d_vector3fa_id ||
         nodedef == fractal2d_float_id || nodedef == fractal2d_color3fa_id ||
         nodedef == fractal2d_vector2fa_id || nodedef == fractal2d_vector3fa_id;
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
  return pxr::SdfValueTypeNames->Float3;
}

bool read_native_noise_or_fractal_parameters(const pxr::UsdShadeShader &source,
                                             Node *node,
                                             string *error_message)
{
  const string nodedef = node->nodedef;
  const bool is_fractal = is_native_fractal2d_family(nodedef);
  const bool scalar_amplitude = native_noise_or_fractal_uses_scalar_amplitude(nodedef);
  const bool is_vector2 = native_noise_or_fractal_is_vector2(nodedef);
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
  if (!connected_shader(input, nullptr, &source_shader, error_message)) {
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

  if (is_color4_operation(nodedef)) {
    Node operation;
    operation.name = unique_node_name(
        *graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    operation.nodedef = nodedef;
    const bool unary = is_color4_unary_math(nodedef);
    const bool invert = nodedef == invert_color4_id;
    const char *first_name = unary ? "in" : (invert ? "amount" : "in1");
    const char *second_name = invert ? "in" : "in2";
    for (const char *input_name : {first_name, second_name}) {
      if (unary && input_name == second_name) {
        break;
      }
      const pxr::UsdShadeInput operand = source_shader.GetInput(pxr::TfToken(input_name));
      if (!operand) {
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
    operation.outputs["out"] = Type::Color4;
    *result = {operation.name, "out", Type::Color4};
    emitted_shaders->emplace(shader_path, operation.name);
    graph->nodes.push_back(std::move(operation));
    return finish(true);
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
  if (!connected_shader(input, nullptr, &source_shader, error_message)) {
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

  if (is_native_fractal2d_family(nodedef) && native_noise_or_fractal_is_color3(nodedef)) {
    Node noise;
    noise.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    noise.nodedef = nodedef;
    const pxr::UsdShadeOutput output = source_shader.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source_shader,
                                    is_native_fractal2d_family(nodedef) ?
                                        std::initializer_list<const char *>{"amplitude", "octaves", "lacunarity", "diminish", "texcoord"} :
                                        std::initializer_list<const char *>{"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"},
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

  if (nodedef == ramplr_color3_id || nodedef == ramptb_color3_id) {
    const bool top_to_bottom = nodedef == ramptb_color3_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    Node ramp;
    ramp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    ramp.nodedef = nodedef;
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput color_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec3f color;
      if (!color_input || color_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          color_input.HasConnectedSource() || !color_input.Get(&color))
      {
        set_error(error_message,
                  nodedef + " requires literal color3 input '" + input_name + "'");
        return finish(false);
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

  if (nodedef == ramplr_float_id || nodedef == ramptb_float_id) {
    const bool top_to_bottom = nodedef == ramptb_float_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
    Node ramp;
    ramp.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    ramp.nodedef = nodedef;
    for (const char *input_name : {first_name, second_name}) {
      const pxr::UsdShadeInput value_input = source_shader.GetInput(pxr::TfToken(input_name));
      float value;
      if (!value_input || value_input.GetTypeName() != pxr::SdfValueTypeNames->Float ||
          value_input.HasConnectedSource() || !value_input.Get(&value) || !std::isfinite(value))
      {
        set_error(error_message,
                  nodedef + " requires literal finite float input '" + input_name + "'");
        return finish(false);
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

  if (nodedef == remap_color3_id || nodedef == range_color3_id) {
    Node range;
    range.name = unique_node_name(*graph, source_shader.GetPrim().GetName().GetString(), shader_path);
    range.nodedef = nodedef;
    for (const char *input_name : {"inlow", "inhigh", "outlow", "outhigh"}) {
      const pxr::UsdShadeInput color_input = source_shader.GetInput(pxr::TfToken(input_name));
      pxr::GfVec3f value;
      if (!color_input || color_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          color_input.HasConnectedSource() || !color_input.Get(&value) ||
          !std::isfinite(value[0]) || !std::isfinite(value[1]) || !std::isfinite(value[2]))
      {
        set_error(error_message,
                  nodedef + " requires literal finite color3 input '" + input_name + "'");
        return finish(false);
      }
      range.color3_inputs[input_name] = make_float3(value[0], value[1], value[2]);
    }
    const float3 &inlow = range.color3_inputs.at("inlow");
    const float3 &inhigh = range.color3_inputs.at("inhigh");
    if (inlow.x == inhigh.x || inlow.y == inhigh.y || inlow.z == inhigh.z) {
      set_error(error_message, nodedef + " requires inlow != inhigh in every component");
      return finish(false);
    }
    if (nodedef == range_color3_id) {
      const pxr::UsdShadeInput gamma_input = source_shader.GetInput(pxr::TfToken("gamma"));
      pxr::GfVec3f gamma;
      if (!gamma_input || gamma_input.GetTypeName() != pxr::SdfValueTypeNames->Color3f ||
          gamma_input.HasConnectedSource() || !gamma_input.Get(&gamma) ||
          gamma[0] != 1.0f || gamma[1] != 1.0f || gamma[2] != 1.0f)
      {
        set_error(error_message, "ND_range_color3 requires literal gamma (1, 1, 1)");
        return finish(false);
      }
      const pxr::UsdShadeInput clamp_input = source_shader.GetInput(pxr::TfToken("doclamp"));
      bool do_clamp;
      if (!clamp_input || clamp_input.GetTypeName() != pxr::SdfValueTypeNames->Bool ||
          clamp_input.HasConnectedSource() || !clamp_input.Get(&do_clamp))
      {
        set_error(error_message, "ND_range_color3 requires literal boolean 'doclamp'");
        return finish(false);
      }
      const float3 &outlow = range.color3_inputs.at("outlow");
      const float3 &outhigh = range.color3_inputs.at("outhigh");
      if (do_clamp && (outlow.x > outhigh.x || outlow.y > outhigh.y || outlow.z > outhigh.z)) {
        set_error(error_message, "ND_range_color3 requires outlow <= outhigh in every component when clamped");
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
  if (!connected_shader(input, nullptr, &source, error_message)) return false;
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
  if (is_native_fractal2d_family(nodedef) && native_noise_or_fractal_is_vector2(nodedef)) {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    is_native_fractal2d_family(nodedef) ?
                                        std::initializer_list<const char *>{"amplitude", "octaves", "lacunarity", "diminish", "texcoord"} :
                                        std::initializer_list<const char *>{"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"},
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
  else if (nodedef == convert_vector3_vector2_id) {
    Link value;
    std::unordered_set<string> active_vector3_shaders;
    if (!read_vector3_output(source.GetInput(pxr::TfToken("in")), graph, &value,
                             &active_vector3_shaders, depth + 1, error_message)) {
      return finish(false);
    }
    node.links["in"] = value;
  }
  else if (nodedef == convert_float_vector2_id) {
    Link value; std::unordered_set<string> active_float_shaders; std::unordered_map<string, string> emitted_float_shaders;
    if (!read_float_output(source.GetInput(pxr::TfToken("in")), graph, &value, &active_float_shaders,
                           &emitted_float_shaders, depth + 1, error_message)) return finish(false);
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
  if (!connected_shader(input, nullptr, &source, error_message)) {
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
  if (is_native_noise_or_fractal_family(nodedef) && native_noise_or_fractal_is_float(nodedef)) {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    is_native_fractal2d_family(nodedef) ?
                                        std::initializer_list<const char *>{"amplitude", "octaves", "lacunarity", "diminish", "texcoord"} :
                                        std::initializer_list<const char *>{"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"},
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
  else if (nodedef == ramplr_float_id || nodedef == ramptb_float_id) {
    /* Ramps produce a float, so they must be recognized on the same recursive
     * path as other scalar producers (rather than only at a top-level input). */
    const bool top_to_bottom = nodedef == ramptb_float_id;
    const char *first_name = top_to_bottom ? "valuet" : "valuel";
    const char *second_name = top_to_bottom ? "valueb" : "valuer";
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
  if (!connected_shader(input, nullptr, &source, error_message)) return false;
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
  if (is_native_fractal2d_family(nodedef) && !native_noise_or_fractal_is_float(nodedef) &&
      !native_noise_or_fractal_is_color3(nodedef) && !native_noise_or_fractal_is_vector2(nodedef))
  {
    const pxr::UsdShadeOutput output = source.GetOutput(pxr::TfToken("out"));
    if (!shader_has_exact_signature(source,
                                    is_native_fractal2d_family(nodedef) ?
                                        std::initializer_list<const char *>{"amplitude", "octaves", "lacunarity", "diminish", "texcoord"} :
                                        std::initializer_list<const char *>{"amplitude", "pivot", native_noise_or_fractal_is_3d(nodedef) ? "position" : "texcoord"},
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
  else if (nodedef == convert_color3_vector3_id) {
    Link color;
    std::unordered_set<string> active_color_shaders;
    if (!read_color_output(source.GetInput(pxr::TfToken("in")), graph, &color, &active_color_shaders,
                           depth + 1, error_message)) return finish(false);
    node.links["in"] = color;
  }
  else if (nodedef == convert_float_vector3_id) {
    Link value;
    std::unordered_set<string> active_float_shaders;
    std::unordered_map<string, string> emitted_float_shaders;
    if (!read_float_output(source.GetInput(pxr::TfToken("in")), graph, &value, &active_float_shaders,
                           &emitted_float_shaders, depth + 1, error_message)) return finish(false);
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
  else if (nodedef == remap_vector3_id || nodedef == remap_vector3fa_id) {
    const bool scalar_bounds = nodedef == remap_vector3fa_id;
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
  else if (nodedef == mix_vector3_id) {
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
    if (!factor || factor.GetTypeName() != pxr::SdfValueTypeNames->Float) {
      set_error(error_message, "ND_mix_vector3 requires float input 'mix'");
      return finish(false);
    }
    if (factor.HasConnectedSource()) {
      std::unordered_set<string> active_float_shaders;
      std::unordered_map<string, string> emitted_float_shaders;
      Link source_link;
      if (!read_float_output(factor, graph, &source_link, &active_float_shaders, &emitted_float_shaders, depth + 1, error_message)) return finish(false);
      node.links["mix"] = source_link;
    }
    else if (!factor.Get(&node.inputs["mix"])) {
      set_error(error_message, "ND_mix_vector3 requires literal or connected float input 'mix'");
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
    set_error(error_message,
              string("ND_displacementshader '") + input_name + "' must be a float");
    return false;
  }
  if (!input.HasConnectedSource()) {
    if (!input.Get(&result->value)) {
      set_error(error_message,
                string("ND_displacementshader '") + input_name + "' must be a literal float");
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
  pxr::UsdShadeShader displacement;
  std::unordered_set<string> active_endpoints;
  if (!resolve_connected_shader(sources[0].source,
                                sources[0].sourceName,
                                sources[0].sourceType,
                                displacement_shader_id,
                                output.GetTypeName(),
                                &displacement,
                                &active_endpoints,
                                0,
                                error_message))
  {
    if (error_message) {
      *error_message = string("MaterialX displacement: ") + *error_message;
    }
    return false;
  }
  if (!read_displacement_float_input(displacement,
                                     "displacement",
                                     0.0f,
                                     graph,
                                     &graph->displacement,
                                     emitted_shaders,
                                     error_message) ||
      !read_displacement_float_input(displacement,
                                     "scale",
                                     1.0f,
                                     graph,
                                     &graph->displacement_scale,
                                     emitted_shaders,
                                     error_message))
  {
    return false;
  }
  for (const pxr::UsdShadeInput &input : displacement.GetInputs()) {
    const string name = input.GetBaseName().GetString();
    if (name != "displacement" && name != "scale") {
      set_error(error_message,
                string("ND_displacementshader has no direct Cycles equivalent: ") + name);
      return false;
    }
  }
  graph->has_displacement = true;
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

}  // namespace

bool read_usdshade_graph(const pxr::UsdShadeMaterial &material,
                         Graph *graph,
                         string *error_message)
{
  if (!material || !graph) {
    set_error(error_message, "A valid USDShade material and destination graph are required");
    return false;
  }

  pxr::UsdShadeOutput surface_output = material.GetSurfaceOutput(mtlx_render_context);
  if (!surface_output) {
    surface_output = material.GetSurfaceOutput();
  }
  if (!surface_output || !surface_output.HasConnectedSource()) {
    set_error(error_message, "USDShade material has no connected MaterialX surface output");
    return false;
  }

  const auto surface_sources = surface_output.GetConnectedSources();
  if (surface_sources.size() != 1) {
    set_error(error_message, "USDShade MaterialX surface output has an invalid source");
    return false;
  }

  pxr::UsdShadeShader surface;
  std::unordered_set<string> active_endpoints;
  if (!resolve_connected_shader(surface_sources[0].source,
                                surface_sources[0].sourceName,
                                surface_sources[0].sourceType,
                                open_pbr_surface_id,
                                surface_output.GetTypeName(),
                                &surface,
                                &active_endpoints,
                                0,
                                error_message))
  {
    if (error_message) {
      *error_message = string("USDShade MaterialX surface: ") + *error_message;
    }
    return false;
  }

  Graph parsed;
  Node open_pbr;
  open_pbr.name = surface.GetPrim().GetName().GetString();
  open_pbr.nodedef = open_pbr_surface_id;
  open_pbr.outputs["out"] = Type::SurfaceShader;

  bool has_supported_input = false;
  std::unordered_map<string, string> emitted_float_shaders;
  std::unordered_map<string, string> emitted_color4_shaders;
  std::unordered_map<string, string> emitted_normalmap_shaders;
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

  for (const pxr::UsdShadeInput &input : surface.GetInputs()) {
    if (!is_supported_open_pbr_input(input.GetBaseName().GetString())) {
      set_error(error_message,
                string("OpenPBR input has no direct Cycles equivalent: ") +
                    input.GetBaseName().GetString());
      return false;
    }
  }

  if (!has_supported_input) {
    set_error(error_message, "OpenPBR has no supported inputs");
    return false;
  }

  if (!read_displacement_terminal(material, &parsed, &emitted_float_shaders, error_message)) {
    return false;
  }

  open_pbr.name = unique_node_name(parsed, open_pbr.name, surface.GetPath().GetString());
  parsed.nodes.push_back(std::move(open_pbr));
  *graph = std::move(parsed);
  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
