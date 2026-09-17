/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/fractal_noise.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* The following offset functions generate random offsets to be added to texture
 * coordinates to act as a seed since the noise functions don't have seed values.
 * A seed value is needed for generating distortion textures and color outputs.
 * The offset's components are in the range [100, 200], not too high to cause
 * bad precision and not too small to be noticeable. We use float seed because
 * OSL only support float hashes.
 */

ccl_device_inline float random_float_offset(const float seed)
{
  return 100.0f + hash_float_to_float(seed) * 100.0f;
}

ccl_device_inline float2 random_float2_offset(const float seed)
{
  return make_float2(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f);
}

ccl_device_inline float3 random_float3_offset(const float seed)
{
  return make_float3(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f);
}

ccl_device_inline float4 random_float4_offset(const float seed)
{
  return make_float4(100.0f + hash_float2_to_float(make_float2(seed, 0.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 1.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 2.0f)) * 100.0f,
                     100.0f + hash_float2_to_float(make_float2(seed, 3.0f)) * 100.0f);
}

template<typename T>
ccl_device float noise_select(T p,
                              const float detail,
                              const float roughness,
                              const float lacunarity,
                              const float offset,
                              const float gain,
                              const int type,
                              bool normalize)
{
  switch ((NodeNoiseType)type) {
    case NODE_NOISE_MULTIFRACTAL: {
      return noise_multi_fractal(p, detail, roughness, lacunarity);
    }
    case NODE_NOISE_FBM: {
      return noise_fbm(p, detail, roughness, lacunarity, normalize);
    }
    case NODE_NOISE_HYBRID_MULTIFRACTAL: {
      return noise_hybrid_multi_fractal(p, detail, roughness, lacunarity, offset, gain);
    }
    case NODE_NOISE_RIDGED_MULTIFRACTAL: {
      return noise_ridged_multi_fractal(p, detail, roughness, lacunarity, offset, gain);
    }
    case NODE_NOISE_HETERO_TERRAIN: {
      return noise_hetero_terrain(p, detail, roughness, lacunarity, offset);
    }
    default: {
      kernel_assert(0);
      return 0.0;
    }
  }
}

ccl_device_inline float mtlx_noise_grad2(const uint hash, const float x, const float y)
{
  const uint h = hash & 7u;
  const float u = h < 4u ? x : y;
  const float v = 2.0f * (h < 4u ? y : x);
  return negate_if(u, int(h & 1u)) + negate_if(v, int(h & 2u));
}

ccl_device_inline float mtlx_noise_grad3(const uint hash,
                                         const float x,
                                         const float y,
                                         const float z)
{
  const uint h = hash & 15u;
  const float u = h < 8u ? x : y;
  const float vt = (h == 12u || h == 14u) ? x : z;
  const float v = h < 4u ? y : vt;
  return negate_if(u, int(h & 1u)) + negate_if(v, int(h & 2u));
}

ccl_device_inline float3 mtlx_noise_grad3_from_hash2(const uint hash,
                                                     const float x,
                                                     const float y)
{
  return make_float3(mtlx_noise_grad2(hash & 0xFFu, x, y),
                     mtlx_noise_grad2((hash >> 8u) & 0xFFu, x, y),
                     mtlx_noise_grad2((hash >> 16u) & 0xFFu, x, y));
}

ccl_device_inline float3 mtlx_noise_grad3_from_hash3(const uint hash,
                                                     const float x,
                                                     const float y,
                                                     const float z)
{
  return make_float3(mtlx_noise_grad3(hash & 0xFFu, x, y, z),
                     mtlx_noise_grad3((hash >> 8u) & 0xFFu, x, y, z),
                     mtlx_noise_grad3((hash >> 16u) & 0xFFu, x, y, z));
}

ccl_device_inline float3 mtlx_noise_bi_mix(const float3 v0,
                                           const float3 v1,
                                           const float3 v2,
                                           const float3 v3,
                                           const float x,
                                           const float y)
{
  const float x1 = 1.0f - x;
  return (1.0f - y) * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x);
}

ccl_device_inline float3 mtlx_noise_tri_mix(const float3 v0,
                                            const float3 v1,
                                            const float3 v2,
                                            const float3 v3,
                                            const float3 v4,
                                            const float3 v5,
                                            const float3 v6,
                                            const float3 v7,
                                            const float x,
                                            const float y,
                                            const float z)
{
  const float x1 = 1.0f - x;
  const float y1 = 1.0f - y;
  const float z1 = 1.0f - z;
  return z1 * (y1 * (v0 * x1 + v1 * x) + y * (v2 * x1 + v3 * x)) +
         z * (y1 * (v4 * x1 + v5 * x) + y * (v6 * x1 + v7 * x));
}

ccl_device float3 mtlx_perlin_noise_float3(float2 p)
{
  const float2 precision_correction = 0.5f *
                                      mask(fabs(p) >= make_float2(1000000.0f), one_float2());
  p = fmod(p, 100000.0f) + precision_correction;

  int X, Y;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float u = fade(fx);
  const float v = fade(fy);
  return 0.6616f * mtlx_noise_bi_mix(mtlx_noise_grad3_from_hash2(hash_uint2(X, Y), fx, fy),
                                     mtlx_noise_grad3_from_hash2(hash_uint2(X + 1, Y),
                                                                 fx - 1.0f,
                                                                 fy),
                                     mtlx_noise_grad3_from_hash2(hash_uint2(X, Y + 1),
                                                                 fx,
                                                                 fy - 1.0f),
                                     mtlx_noise_grad3_from_hash2(hash_uint2(X + 1, Y + 1),
                                                                 fx - 1.0f,
                                                                 fy - 1.0f),
                                     u,
                                     v);
}

ccl_device float3 mtlx_perlin_noise_float3(float3 p)
{
  const float3 precision_correction = 0.5f *
                                      mask(fabs(p) >= make_float3(1000000.0f), one_float3());
  p = fmod(p, 100000.0f) + precision_correction;

  int X, Y, Z;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float fz = floorfrac(p.z, &Z);
  const float u = fade(fx);
  const float v = fade(fy);
  const float w = fade(fz);
  return 0.9820f * mtlx_noise_tri_mix(
      mtlx_noise_grad3_from_hash3(hash_uint3(X, Y, Z), fx, fy, fz),
      mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y, Z), fx - 1.0f, fy, fz),
      mtlx_noise_grad3_from_hash3(hash_uint3(X, Y + 1, Z), fx, fy - 1.0f, fz),
      mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y + 1, Z), fx - 1.0f, fy - 1.0f, fz),
      mtlx_noise_grad3_from_hash3(hash_uint3(X, Y, Z + 1), fx, fy, fz - 1.0f),
      mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y, Z + 1), fx - 1.0f, fy, fz - 1.0f),
      mtlx_noise_grad3_from_hash3(hash_uint3(X, Y + 1, Z + 1), fx, fy - 1.0f, fz - 1.0f),
      mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y + 1, Z + 1),
                                  fx - 1.0f,
                                  fy - 1.0f,
                                  fz - 1.0f),
      u,
      v,
      w);
}

template<typename T>
ccl_device float3 mtlx_fbm_float3(T p,
                                  const float detail,
                                  const float diminish,
                                  const float lacunarity)
{
  float3 sum = zero_float3();
  float amplitude = 1.0f;

  /* INCLUSIVE, like Cycles' own fBM: detail 0 is one octave.  MaterialX's
   * fractal runs `octaves` times, so the lowering seeds detail with
   * octaves - 1 and that one convention holds for the scalar and the vector
   * path alike -- `detail` must not mean two different things depending on a
   * flag. */
  for (int i = 0; i <= float_to_int(detail); i++) {
    sum += amplitude * mtlx_perlin_noise_float3(p);
    amplitude *= diminish;
    p *= lacunarity;
  }

  return sum;
}

ccl_device void noise_texture_1d(const float co,
                                 const float detail,
                                 const float roughness,
                                 const float lacunarity,
                                 const float offset,
                                 const float gain,
                                 const float distortion,
                                 const int type,
                                 bool normalize,
                                 bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float p = co;
  if (distortion != 0.0f) {
    p += snoise_1d(p + random_float_offset(0.0f)) * distortion;
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float_offset(1.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float_offset(2.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device void noise_texture_2d(const float2 co,
                                 const float detail,
                                 const float roughness,
                                 const float lacunarity,
                                 const float offset,
                                 const float gain,
                                 const float distortion,
                                 const int type,
                                 const bool normalize,
                                 const bool materialx_vector_color,
                                 const bool materialx_vector_fbm,
                                 const bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float2 p = co;
  if (distortion != 0.0f) {
    p += make_float2(snoise_2d(p + random_float2_offset(0.0f)) * distortion,
                     snoise_2d(p + random_float2_offset(1.0f)) * distortion);
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    if (materialx_vector_color) {
      *color = materialx_vector_fbm ? mtlx_fbm_float3(p, detail, roughness, lacunarity) :
                                      mtlx_perlin_noise_float3(p);
    }
    else {
      *color = make_float3(*value,
                           noise_select(p + random_float2_offset(2.0f),
                                        detail,
                                        roughness,
                                        lacunarity,
                                        offset,
                                        gain,
                                        type,
                                        normalize),
                           noise_select(p + random_float2_offset(3.0f),
                                        detail,
                                        roughness,
                                        lacunarity,
                                        offset,
                                        gain,
                                        type,
                                        normalize));
    }
  }
}

ccl_device void noise_texture_3d(const float3 co,
                                 const float detail,
                                 const float roughness,
                                 const float lacunarity,
                                 const float offset,
                                 const float gain,
                                 const float distortion,
                                 const int type,
                                 const bool normalize,
                                 const bool materialx_vector_color,
                                 const bool materialx_vector_fbm,
                                 const bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float3 p = co;
  if (distortion != 0.0f) {
    p += make_float3(snoise_3d(p + random_float3_offset(0.0f)) * distortion,
                     snoise_3d(p + random_float3_offset(1.0f)) * distortion,
                     snoise_3d(p + random_float3_offset(2.0f)) * distortion);
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    if (materialx_vector_color) {
      *color = materialx_vector_fbm ? mtlx_fbm_float3(p, detail, roughness, lacunarity) :
                                      mtlx_perlin_noise_float3(p);
    }
    else {
      *color = make_float3(*value,
                           noise_select(p + random_float3_offset(3.0f),
                                        detail,
                                        roughness,
                                        lacunarity,
                                        offset,
                                        gain,
                                        type,
                                        normalize),
                           noise_select(p + random_float3_offset(4.0f),
                                        detail,
                                        roughness,
                                        lacunarity,
                                        offset,
                                        gain,
                                        type,
                                        normalize));
    }
  }
}

ccl_device void noise_texture_4d(const float4 co,
                                 const float detail,
                                 const float roughness,
                                 const float lacunarity,
                                 const float offset,
                                 const float gain,
                                 const float distortion,
                                 const int type,
                                 const bool normalize,
                                 const bool color_is_needed,
                                 ccl_private float *value,
                                 ccl_private float3 *color)
{
  float4 p = co;
  if (distortion != 0.0f) {
    p += make_float4(snoise_4d(p + random_float4_offset(0.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(1.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(2.0f)) * distortion,
                     snoise_4d(p + random_float4_offset(3.0f)) * distortion);
  }

  *value = noise_select(p, detail, roughness, lacunarity, offset, gain, type, normalize);
  if (color_is_needed) {
    *color = make_float3(*value,
                         noise_select(p + random_float4_offset(4.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize),
                         noise_select(p + random_float4_offset(5.0f),
                                      detail,
                                      roughness,
                                      lacunarity,
                                      offset,
                                      gain,
                                      type,
                                      normalize));
  }
}

ccl_device_noinline void svm_node_tex_noise(ccl_private float *ccl_restrict stack,
                                            const ccl_global SVMNodeTexNoise &ccl_restrict node)
{
  float3 vector = stack_load_float3(stack, node.vector);
  float w = stack_load(stack, node.w);
  const float scale = stack_load(stack, node.scale);
  float detail = stack_load(stack, node.detail);
  float roughness = stack_load(stack, node.roughness);
  const float lacunarity = stack_load(stack, node.lacunarity);
  const float offset = stack_load(stack, node.offset);
  const float gain = stack_load(stack, node.gain);
  const float distortion = stack_load(stack, node.distortion);

  detail = clamp(detail, 0.0f, 15.0f);
  roughness = fmaxf(roughness, 0.0f);

  vector *= scale;
  w *= scale;

  float value;
  float3 color;
  switch (node.dimensions) {
    case 1:
      noise_texture_1d(w,
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       node.noise_type,
                       node.normalize,
                       stack_valid(node.color_offset),
                       &value,
                       &color);
      break;
    case 2:
      noise_texture_2d(make_float2(vector.x, vector.y),
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       node.noise_type,
                       node.normalize,
                       node.materialx_vector_color,
                       node.materialx_vector_fbm,
                       stack_valid(node.color_offset),
                       &value,
                       &color);
      break;
    case 3:
      noise_texture_3d(vector,
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       node.noise_type,
                       node.normalize,
                       node.materialx_vector_color,
                       node.materialx_vector_fbm,
                       stack_valid(node.color_offset),
                       &value,
                       &color);
      break;
    case 4:
      noise_texture_4d(make_float4(vector, w),
                       detail,
                       roughness,
                       lacunarity,
                       offset,
                       gain,
                       distortion,
                       node.noise_type,
                       node.normalize,
                       stack_valid(node.color_offset),
                       &value,
                       &color);
      break;
    default:
      kernel_assert(0);
  }

  if (stack_valid(node.value_offset)) {
    stack_store_float(stack, node.value_offset, value);
  }
  if (stack_valid(node.color_offset)) {
    stack_store_float3(stack, node.color_offset, color);
  }
}

CCL_NAMESPACE_END
