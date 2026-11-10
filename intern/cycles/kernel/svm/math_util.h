/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/types.h"
#include "kernel/tables.h"

#include "util/math.h"
#include "util/types.h"

CCL_NAMESPACE_BEGIN

/* MaterialX 1.39.4 Standard Nodes: division by either signed zero returns NaN.
 * Keep this separate from Blender safe_divide and its constant-fold identities. */
ccl_device_inline float materialx_divide(const float a, const float b)
{
  return b == 0.0f ? __uint_as_float(0x7fc00000u) : a / b;
}

ccl_device_inline float3 materialx_divide(const float3 a, const float3 b)
{
  return make_float3(materialx_divide(a.x, b.x),
                     materialx_divide(a.y, b.y),
                     materialx_divide(a.z, b.z));
}

ccl_device_inline dual3 materialx_divide(const dual3 a, const dual3 b)
{
  /* As in safe_divide, zero-denominator derivatives use the zero fallback.
   * The MaterialX value remains NaN; no derivative contract is inferred there. */
  return select(component_is_zero(b.val),
                dual3(make_float3(__uint_as_float(0x7fc00000u))),
                a / b);
}

/* MaterialX consumers must retain NaN even when the other operand is zero.
 * Integer classification survives finite-math assumptions on floating predicates.
 * Ordinary Blender multiply and scale keep their existing kernels/identities. */
ccl_device_inline float materialx_multiply(const float a, const float b)
{
  const uint aa = __float_as_uint(a) & 0x7fffffffu;
  const uint bb = __float_as_uint(b) & 0x7fffffffu;
  if (aa > 0x7f800000u || bb > 0x7f800000u ||
      (aa == 0u && bb == 0x7f800000u) || (bb == 0u && aa == 0x7f800000u))
  {
    return __uint_as_float(0x7fc00000u);
  }
  return a * b;
}

ccl_device_inline float3 materialx_multiply(const float3 a, const float3 b)
{
  return make_float3(materialx_multiply(a.x, b.x),
                     materialx_multiply(a.y, b.y),
                     materialx_multiply(a.z, b.z));
}

ccl_device_inline dual3 materialx_multiply(const dual3 a, const dual3 b)
{
  /* Keep the existing product-rule derivatives; undefined NaN derivatives are
   * not qualified here. The value uses the same explicit per-channel policy. */
  dual3 result = a * b;
  result.val = materialx_multiply(a.val, b.val);
  return result;
}

/* Preserve an upstream NaN only for the MaterialX Sqrt consumer. Negative
 * finite inputs keep the existing source-extension fallback pending policy.
 * Integer classification follows the admitted private multiply convention. */
ccl_device_inline float materialx_sqrt(const float a)
{
  if ((__float_as_uint(a) & 0x7fffffffu) > 0x7f800000u) {
    return __uint_as_float(0x7fc00000u);
  }
  return safe_sqrtf(a);
}

template<class Float3Type, class FloatType>
ccl_device void svm_vector_math(ccl_private FloatType *value,
                                ccl_private Float3Type *vector,
                                NodeVectorMathType type,
                                const Float3Type a,
                                const Float3Type b,
                                const Float3Type c,
                                const FloatType param1)
{
  switch (type) {
    case NODE_VECTOR_MATH_ADD:
      *vector = a + b;
      break;
    case NODE_VECTOR_MATH_SUBTRACT:
      *vector = a - b;
      break;
    case NODE_VECTOR_MATH_MULTIPLY:
      *vector = a * b;
      break;
    case NODE_VECTOR_MATH_DIVIDE:
      *vector = safe_divide(a, b);
      break;
    case NODE_VECTOR_MATH_MATERIALX_DIVIDE:
      *vector = materialx_divide(a, b);
      break;
    case NODE_VECTOR_MATH_MATERIALX_MULTIPLY:
      *vector = materialx_multiply(a, b);
      break;
    case NODE_VECTOR_MATH_CROSS_PRODUCT:
      *vector = cross(a, b);
      break;
    case NODE_VECTOR_MATH_PROJECT:
      *vector = project(a, b);
      break;
    case NODE_VECTOR_MATH_REFLECT:
      *vector = reflect(a, safe_normalize(b));
      break;
    case NODE_VECTOR_MATH_REFRACT:
      *vector = refract(a, safe_normalize(b), param1);
      break;
    case NODE_VECTOR_MATH_FACEFORWARD:
      *vector = faceforward(a, b, c);
      break;
    case NODE_VECTOR_MATH_MULTIPLY_ADD:
      *vector = a * b + c;
      break;
    case NODE_VECTOR_MATH_DOT_PRODUCT:
      *value = dot(a, b);
      break;
    case NODE_VECTOR_MATH_DISTANCE:
      *value = distance(a, b);
      break;
    case NODE_VECTOR_MATH_LENGTH:
      *value = len(a);
      break;
    case NODE_VECTOR_MATH_SCALE:
      *vector = a * param1;
      break;
    case NODE_VECTOR_MATH_NORMALIZE:
      *vector = safe_normalize(a);
      break;
    case NODE_VECTOR_MATH_SNAP:
      *vector = floor(safe_divide(a, b)) * b;
      break;
    case NODE_VECTOR_MATH_ROUND:
      *vector = floor(a + 0.5f);
      break;
    case NODE_VECTOR_MATH_FLOOR:
      *vector = floor(a);
      break;
    case NODE_VECTOR_MATH_CEIL:
      *vector = ceil(a);
      break;
    case NODE_VECTOR_MATH_MODULO:
      *vector = safe_fmod(a, b);
      break;
    case NODE_VECTOR_MATH_WRAP:
      *vector = wrap(a, b, c);
      break;
    case NODE_VECTOR_MATH_FRACTION:
      *vector = a - floor(a);
      break;
    case NODE_VECTOR_MATH_ABSOLUTE:
      *vector = fabs(a);
      break;
    case NODE_VECTOR_MATH_POWER:
      *vector = safe_pow(a, b);
      break;
    case NODE_VECTOR_MATH_SIGN:
      *vector = compatible_sign(a);
      break;
    case NODE_VECTOR_MATH_MINIMUM:
      *vector = min(a, b);
      break;
    case NODE_VECTOR_MATH_MAXIMUM:
      *vector = max(a, b);
      break;
    case NODE_VECTOR_MATH_SINE:
      *vector = sin(a);
      break;
    case NODE_VECTOR_MATH_COSINE:
      *vector = cos(a);
      break;
    case NODE_VECTOR_MATH_TANGENT:
      *vector = tan(a);
      break;
    default:
      *vector = Float3Type(zero_float3());
      *value = FloatType(0.0f);
  }
}

ccl_device float svm_math(NodeMathType type, const float a, float b, const float c)
{
  switch (type) {
    case NODE_MATH_ADD:
      return a + b;
    case NODE_MATH_SUBTRACT:
      return a - b;
    case NODE_MATH_MULTIPLY:
      return a * b;
    case NODE_MATH_DIVIDE:
      return safe_divide(a, b);
    case NODE_MATH_MATERIALX_DIVIDE:
      return materialx_divide(a, b);
    case NODE_MATH_MATERIALX_MULTIPLY:
      return materialx_multiply(a, b);
    case NODE_MATH_POWER:
      return safe_powf(a, b);
    case NODE_MATH_LOGARITHM:
      return safe_logf(a, b);
    case NODE_MATH_SQRT:
      return safe_sqrtf(a);
    case NODE_MATH_MATERIALX_SQRT:
      return materialx_sqrt(a);
    case NODE_MATH_INV_SQRT:
      return inversesqrtf(a);
    case NODE_MATH_ABSOLUTE:
      return fabsf(a);
    case NODE_MATH_RADIANS:
      return a * (M_PI_F / 180.0f);
    case NODE_MATH_DEGREES:
      return a * (180.0f / M_PI_F);
    case NODE_MATH_MINIMUM:
      return fminf(a, b);
    case NODE_MATH_MAXIMUM:
      return fmaxf(a, b);
    case NODE_MATH_LESS_THAN:
      return a < b;
    case NODE_MATH_GREATER_THAN:
      return a > b;
    case NODE_MATH_ROUND:
      return floorf(a + 0.5f);
    case NODE_MATH_FLOOR:
      return floorf(a);
    case NODE_MATH_CEIL:
      return ceilf(a);
    case NODE_MATH_FRACTION:
      return a - floorf(a);
    case NODE_MATH_MODULO:
      return safe_modulo(a, b);
    case NODE_MATH_FLOORED_MODULO:
      return safe_floored_modulo(a, b);
    case NODE_MATH_TRUNC:
      return a >= 0.0f ? floorf(a) : ceilf(a);
    case NODE_MATH_SNAP:
      return floorf(safe_divide(a, b)) * b;
    case NODE_MATH_WRAP:
      return wrapf(a, b, c);
    case NODE_MATH_PINGPONG:
      return pingpongf(a, b);
    case NODE_MATH_SINE:
      return sinf(a);
    case NODE_MATH_COSINE:
      return cosf(a);
    case NODE_MATH_TANGENT:
      return tanf(a);
    case NODE_MATH_SINH:
      return sinhf(a);
    case NODE_MATH_COSH:
      return coshf(a);
    case NODE_MATH_TANH:
      return tanhf(a);
    case NODE_MATH_ARCSINE:
      return safe_asinf(a);
    case NODE_MATH_ARCCOSINE:
      return safe_acosf(a);
    case NODE_MATH_ARCTANGENT:
      return atanf(a);
    case NODE_MATH_ARCTAN2:
      return compatible_atan2(a, b);
    case NODE_MATH_SIGN:
      return compatible_signf(a);
    case NODE_MATH_EXPONENT:
      return expf(a);
    case NODE_MATH_COMPARE:
      return ((a == b) || (fabsf(a - b) <= fmaxf(c, FLT_EPSILON))) ? 1.0f : 0.0f;
    case NODE_MATH_MULTIPLY_ADD:
      return a * b + c;
    case NODE_MATH_SMOOTH_MIN:
      return smoothminf(a, b, c);
    case NODE_MATH_SMOOTH_MAX:
      return -smoothminf(-a, -b, c);
    default:
      return 0.0f;
  }
}

ccl_device float3 svm_math_blackbody_color_rec709(const float t)
{
  /* Calculate color in range 800..12000 using an approximation
   * a/x+bx+c for R and G and ((at + b)t + c)t + d) for B.
   *
   * The result of this can be negative to support gamut wider than
   * than rec.709, just needs to be clamped. */

  if (t >= 12000.0f) {
    return make_float3(0.8262954810464208f, 0.9945080501520986f, 1.566307710274283f);
  }
  if (t < 800.0f) {
    /* Arbitrary lower limit where light is very dim, matching OSL. */
    return make_float3(5.413294490189271f, -0.20319390035873933f, -0.0822535242887164f);
  }

  const int i = (t >= 6365.0f) ? 6 :
                (t >= 3315.0f) ? 5 :
                (t >= 1902.0f) ? 4 :
                (t >= 1449.0f) ? 3 :
                (t >= 1167.0f) ? 2 :
                (t >= 965.0f)  ? 1 :
                                 0;

  ccl_constant float *r = blackbody_table_r[i];
  ccl_constant float *g = blackbody_table_g[i];
  ccl_constant float *b = blackbody_table_b[i];

  const float t_inv = 1.0f / t;
  return make_float3(r[0] * t_inv + r[1] * t + r[2],
                     g[0] * t_inv + g[1] * t + g[2],
                     ((b[0] * t + b[1]) * t + b[2]) * t + b[3]);
}

ccl_device_inline float3 svm_math_gamma_color(float3 color, const float gamma)
{
  if (gamma == 0.0f) {
    return make_float3(1.0f, 1.0f, 1.0f);
  }

  if (color.x > 0.0f) {
    color.x = powf(color.x, gamma);
  }
  if (color.y > 0.0f) {
    color.y = powf(color.y, gamma);
  }
  if (color.z > 0.0f) {
    color.z = powf(color.z, gamma);
  }

  return color;
}

ccl_device float3 svm_math_wavelength_color_xyz(const float lambda_nm)
{
  float ii = (lambda_nm - 380.0f) * (1.0f / 5.0f);  // scaled 0..80
  const int i = float_to_int(ii);
  float3 color;

  if (i < 0 || i >= 80) {
    color = make_float3(0.0f, 0.0f, 0.0f);
  }
  else {
    ii -= i;
    ccl_constant float *c = cie_color_match[i];
    color = interp(make_float3(c[0], c[1], c[2]), make_float3(c[3], c[4], c[5]), ii);
  }

  return color;
}

CCL_NAMESPACE_END
