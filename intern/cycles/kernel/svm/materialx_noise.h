/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/noise.h"
#include "kernel/svm/noisetex.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* MaterialX noise2d/3d, fractal2d/3d and the unifiednoise Perlin/fractal
 * branches: MaterialX's own mx_noise.glsl Perlin (no fmod lattice wrap),
 * fractals that rescale p every octave with an unclamped diminish, and one
 * vector-valued Perlin call for vector/color outputs. Blender's Noise Texture
 * is not used for these. */

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

ccl_device_inline float3 mtlx_noise_grad3_from_hash2(const uint hash, const float x, const float y)
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

/* Scalar MaterialX Perlin noise, transcribed from mx_perlin_noise_float in
 * MaterialX 1.39 libraries/stdlib/genglsl/lib/mx_noise.glsl: Jenkins lookup3
 * lattice hash, quintic fade, bilinear/trilinear blend and the 0.6616 / 0.9820
 * gradient scale, with no lattice wrap. Blender's snoise_2d/3d share the
 * formula but take an fmod(100000) wrap and, on CPU, an SSE evaluation order;
 * the MaterialX lowering calls these instead so every target evaluates the
 * same scalar expression. */
ccl_device_inline float mtlx_noise_bi_mix(
    const float v0, const float v1, const float v2, const float v3, const float s, const float t)
{
  const float s1 = 1.0f - s;
  return (1.0f - t) * (v0 * s1 + v1 * s) + t * (v2 * s1 + v3 * s);
}

ccl_device_inline float mtlx_noise_tri_mix(const float v0,
                                           const float v1,
                                           const float v2,
                                           const float v3,
                                           const float v4,
                                           const float v5,
                                           const float v6,
                                           const float v7,
                                           const float s,
                                           const float t,
                                           const float r)
{
  const float s1 = 1.0f - s;
  const float t1 = 1.0f - t;
  const float r1 = 1.0f - r;
  return (r1 * (t1 * (v0 * s1 + v1 * s) + t * (v2 * s1 + v3 * s)) +
          r * (t1 * (v4 * s1 + v5 * s) + t * (v6 * s1 + v7 * s)));
}

ccl_device float mtlx_perlin_noise_float(const float2 p)
{
  int X, Y;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float u = fade(fx);
  const float v = fade(fy);
  const float result = mtlx_noise_bi_mix(
      mtlx_noise_grad2(hash_uint2(X, Y), fx, fy),
      mtlx_noise_grad2(hash_uint2(X + 1, Y), fx - 1.0f, fy),
      mtlx_noise_grad2(hash_uint2(X, Y + 1), fx, fy - 1.0f),
      mtlx_noise_grad2(hash_uint2(X + 1, Y + 1), fx - 1.0f, fy - 1.0f),
      u,
      v);
  return 0.6616f * result;
}

ccl_device float mtlx_perlin_noise_float(const float3 p)
{
  int X, Y, Z;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float fz = floorfrac(p.z, &Z);
  const float u = fade(fx);
  const float v = fade(fy);
  const float w = fade(fz);
  const float result = mtlx_noise_tri_mix(
      mtlx_noise_grad3(hash_uint3(X, Y, Z), fx, fy, fz),
      mtlx_noise_grad3(hash_uint3(X + 1, Y, Z), fx - 1.0f, fy, fz),
      mtlx_noise_grad3(hash_uint3(X, Y + 1, Z), fx, fy - 1.0f, fz),
      mtlx_noise_grad3(hash_uint3(X + 1, Y + 1, Z), fx - 1.0f, fy - 1.0f, fz),
      mtlx_noise_grad3(hash_uint3(X, Y, Z + 1), fx, fy, fz - 1.0f),
      mtlx_noise_grad3(hash_uint3(X + 1, Y, Z + 1), fx - 1.0f, fy, fz - 1.0f),
      mtlx_noise_grad3(hash_uint3(X, Y + 1, Z + 1), fx, fy - 1.0f, fz - 1.0f),
      mtlx_noise_grad3(hash_uint3(X + 1, Y + 1, Z + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f),
      u,
      v,
      w);
  return 0.9820f * result;
}

/* mx_fractal2d/3d_noise_float: the coordinate itself is rescaled each octave
 * (p *= lacunarity), NOT evaluated at lacunarity^i * p as Blender's noise_fbm
 * does -- the two round differently once lacunarity is not a power of two
 * (measured 1.6e-5 at (24.433, 195.719), octaves 4, lacunarity 2.37) -- and
 * diminish is not clamped. `detail` is octaves - 1, the lowering's one
 * convention; detail -1 is zero octaves. */
template<typename T>
ccl_device float mtlx_fractal_noise_float(T p,
                                          const float detail,
                                          const float diminish,
                                          const float lacunarity)
{
  float result = 0.0f;
  float amplitude = 1.0f;
  const int octaves = float_to_int(detail) + 1;
  for (int i = 0; i < octaves; i++) {
    result += amplitude * mtlx_perlin_noise_float(p);
    amplitude *= diminish;
    p *= lacunarity;
  }
  return result;
}

/* mx_perlin_noise_vec3: no lattice wrap, as in MaterialX. Channel x hashes
 * byte 0 of the same lattice hash, so it equals mtlx_perlin_noise_float. */
ccl_device float3 mtlx_perlin_noise_float3(const float2 p)
{
  int X, Y;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float u = fade(fx);
  const float v = fade(fy);
  return 0.6616f * mtlx_noise_bi_mix(
                       mtlx_noise_grad3_from_hash2(hash_uint2(X, Y), fx, fy),
                       mtlx_noise_grad3_from_hash2(hash_uint2(X + 1, Y), fx - 1.0f, fy),
                       mtlx_noise_grad3_from_hash2(hash_uint2(X, Y + 1), fx, fy - 1.0f),
                       mtlx_noise_grad3_from_hash2(hash_uint2(X + 1, Y + 1), fx - 1.0f, fy - 1.0f),
                       u,
                       v);
}

ccl_device float3 mtlx_perlin_noise_float3(const float3 p)
{
  int X, Y, Z;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float fz = floorfrac(p.z, &Z);
  const float u = fade(fx);
  const float v = fade(fy);
  const float w = fade(fz);
  return 0.9820f *
         mtlx_noise_tri_mix(
             mtlx_noise_grad3_from_hash3(hash_uint3(X, Y, Z), fx, fy, fz),
             mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y, Z), fx - 1.0f, fy, fz),
             mtlx_noise_grad3_from_hash3(hash_uint3(X, Y + 1, Z), fx, fy - 1.0f, fz),
             mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y + 1, Z), fx - 1.0f, fy - 1.0f, fz),
             mtlx_noise_grad3_from_hash3(hash_uint3(X, Y, Z + 1), fx, fy, fz - 1.0f),
             mtlx_noise_grad3_from_hash3(hash_uint3(X + 1, Y, Z + 1), fx - 1.0f, fy, fz - 1.0f),
             mtlx_noise_grad3_from_hash3(hash_uint3(X, Y + 1, Z + 1), fx, fy - 1.0f, fz - 1.0f),
             mtlx_noise_grad3_from_hash3(
                 hash_uint3(X + 1, Y + 1, Z + 1), fx - 1.0f, fy - 1.0f, fz - 1.0f),
             u,
             v,
             w);
}

template<typename T>
ccl_device float3
mtlx_fbm_float3(T p, const float detail, const float diminish, const float lacunarity)
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

/* mx_fractal2d/3d_noise_vec2: two scalar fractals, the second at p + (19, 193)
 * or p + (19, 193, 17). */
ccl_device float3 mtlx_fbm_vector2_float3(const float2 p,
                                          const float detail,
                                          const float diminish,
                                          const float lacunarity)
{
  return make_float3(
      mtlx_fractal_noise_float(p, detail, diminish, lacunarity),
      mtlx_fractal_noise_float(p + make_float2(19.0f, 193.0f), detail, diminish, lacunarity),
      0.0f);
}

ccl_device float3 mtlx_fbm_vector2_float3(const float3 p,
                                          const float detail,
                                          const float diminish,
                                          const float lacunarity)
{
  return make_float3(mtlx_fractal_noise_float(p, detail, diminish, lacunarity),
                     mtlx_fractal_noise_float(
                         p + make_float3(19.0f, 193.0f, 17.0f), detail, diminish, lacunarity),
                     0.0f);
}

/* Analytic gradient of the same hashed Perlin lattice used by snoise_3d.
 * Keep the scalar value on the regular snoise path so normal appearance is
 * unchanged when a caller starts requesting derivatives. */
ccl_device_inline float3 snoise_3d_gradient(float3 p)
{
  const float3 precision_correction = 0.5f *
                                      mask(fabs(p) >= make_float3(1000000.0f), one_float3());
  p = fmod(p, 100000.0f) + precision_correction;
  int X, Y, Z;
  const float fx = floorfrac(p.x, &X);
  const float fy = floorfrac(p.y, &Y);
  const float fz = floorfrac(p.z, &Z);
  const float u = fade(fx), v = fade(fy), w = fade(fz);
  const float du = 30.0f * fx * fx * (fx - 1.0f) * (fx - 1.0f);
  const float dv = 30.0f * fy * fy * (fy - 1.0f) * (fy - 1.0f);
  const float dw = 30.0f * fz * fz * (fz - 1.0f) * (fz - 1.0f);
  float3 derivative = zero_float3();
  for (int iz = 0; iz != 2; ++iz) {
    for (int iy = 0; iy != 2; ++iy) {
      for (int ix = 0; ix != 2; ++ix) {
        const int hash = hash_uint3(X + ix, Y + iy, Z + iz);
        const float x = fx - float(ix), y = fy - float(iy), z = fz - float(iz);
        const int h = hash & 15;
        const float s1 = (h & 1) ? -1.0f : 1.0f;
        const float s2 = (h & 2) ? -1.0f : 1.0f;
        /* grad3's hash table in scalar and SSE kernels has this same slope. */
        const float3 gradient = make_float3((h < 8 ? s1 : 0.0f) + (h == 12 || h == 14 ? s2 : 0.0f),
                                            (h >= 8 ? s1 : 0.0f) + (h < 4 ? s2 : 0.0f),
                                            (h >= 4 && h != 12 && h != 14 ? s2 : 0.0f));
        const float sample = dot(gradient, make_float3(x, y, z));
        const float wx = ix ? u : 1.0f - u;
        const float wy = iy ? v : 1.0f - v;
        const float wz = iz ? w : 1.0f - w;
        const float dwx = ix ? du : -du;
        const float dwy = iy ? dv : -dv;
        const float dwz = iz ? dw : -dw;
        derivative += gradient * (wx * wy * wz) +
                      sample * make_float3(dwx * wy * wz, wx * dwy * wz, wx * wy * dwz);
      }
    }
  }
  return derivative * 0.9820f;
}

ccl_device void materialx_noise_texture(const ccl_global SVMNodeMaterialXNoise &ccl_restrict node,
                                        float3 vector,
                                        const float scale,
                                        float detail,
                                        const float roughness,
                                        const float lacunarity,
                                        const float distortion,
                                        ccl_private float *value,
                                        ccl_private float3 *color)
{
  /* MaterialX octaves are detail + 1 with no upper limit of 16, zero octaves
   * are legal (detail -1), and diminish is not clamped. The bound only stops
   * a runaway loop; the coordinate overflows to infinity long before it. */
  detail = clamp(detail, -1.0f, 255.0f);
  vector *= scale;
  const bool fractal = node.vector_fbm || node.vector2_fbm;
  const bool color_is_needed = stack_valid(node.color_offset);
  if (node.dimensions == 2) {
    float2 p = make_float2(vector.x, vector.y);
    if (distortion != 0.0f) {
      p += make_float2(snoise_2d(p + random_float2_offset(0.0f)) * distortion,
                       snoise_2d(p + random_float2_offset(1.0f)) * distortion);
    }
    *value = fractal ? mtlx_fractal_noise_float(p, detail, roughness, lacunarity) :
                       mtlx_perlin_noise_float(p);
    if (color_is_needed) {
      *color = node.vector2_fbm ? mtlx_fbm_vector2_float3(p, detail, roughness, lacunarity) :
               node.vector_fbm  ? mtlx_fbm_float3(p, detail, roughness, lacunarity) :
                                  mtlx_perlin_noise_float3(p);
    }
  }
  else {
    float3 p = vector;
    if (distortion != 0.0f) {
      p += make_float3(snoise_3d(p + random_float3_offset(0.0f)) * distortion,
                       snoise_3d(p + random_float3_offset(1.0f)) * distortion,
                       snoise_3d(p + random_float3_offset(2.0f)) * distortion);
    }
    *value = fractal ? mtlx_fractal_noise_float(p, detail, roughness, lacunarity) :
                       mtlx_perlin_noise_float(p);
    if (color_is_needed) {
      *color = node.vector2_fbm ? mtlx_fbm_vector2_float3(p, detail, roughness, lacunarity) :
               node.vector_fbm  ? mtlx_fbm_float3(p, detail, roughness, lacunarity) :
                                  mtlx_perlin_noise_float3(p);
    }
  }
}

ccl_device_noinline void svm_node_materialx_noise(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeMaterialXNoise &ccl_restrict node)
{
  float value = 0.0f;
  float3 color = zero_float3();
  materialx_noise_texture(node,
                          stack_load_float3(stack, node.vector),
                          stack_load(stack, node.scale),
                          stack_load(stack, node.detail),
                          stack_load(stack, node.roughness),
                          stack_load(stack, node.lacunarity),
                          stack_load(stack, node.distortion),
                          &value,
                          &color);
  if (stack_valid(node.value_offset)) {
    stack_store_float(stack, node.value_offset, value);
  }
  if (stack_valid(node.color_offset)) {
    stack_store_float3(stack, node.color_offset, color);
  }
}

/* Derivatives for the heighttonormal chain: the Fac value is the MaterialX
 * noise above, the screen derivatives are those of Blender's 3D fBM lattice
 * (snoise_3d_gradient), as in the fork's flag-gated Noise Texture. Color
 * derivatives stay zero. */
ccl_device_noinline void svm_node_materialx_noise_derivative(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeMaterialXNoise &ccl_restrict node)
{
  const dual3 position = stack_load<dual3>(stack, node.vector);
  svm_node_materialx_noise(stack, node);
  if (stack_valid(node.color_offset)) {
    stack_store_float3(stack, node.color_offset + 3, zero_float3());
    stack_store_float3(stack, node.color_offset + 6, zero_float3());
  }
  if (!stack_valid(node.value_offset)) {
    return;
  }
  float dx = 0.0f, dy = 0.0f;
  if (node.dimensions == 3 && node.noise_type == NODE_NOISE_FBM &&
      stack_load(stack, node.distortion) == 0.0f)
  {
    const float scale = stack_load(stack, node.scale);
    const float detail = clamp(stack_load(stack, node.detail), 0.0f, 15.0f);
    const float roughness = fmaxf(stack_load(stack, node.roughness), 0.0f);
    const float lacunarity = stack_load(stack, node.lacunarity);
    float3 p = position.val * scale;
    float3 dpdx = position.dx * scale;
    float3 dpdy = position.dy * scale;
    float amplitude = 1.0f;
    float max_amplitude = 0.0f;
    for (int i = 0; i <= float_to_int(detail); ++i) {
      const float3 gradient = snoise_3d_gradient(p);
      dx += amplitude * dot(gradient, dpdx);
      dy += amplitude * dot(gradient, dpdy);
      max_amplitude += amplitude;
      amplitude *= roughness;
      p *= lacunarity;
      dpdx *= lacunarity;
      dpdy *= lacunarity;
    }
    const float remainder = detail - floorf(detail);
    if (remainder != 0.0f) {
      const float3 gradient = snoise_3d_gradient(p);
      const float dx_extra = dx + amplitude * dot(gradient, dpdx);
      const float dy_extra = dy + amplitude * dot(gradient, dpdy);
      if (node.normalize) {
        dx = mix(
            0.5f * dx / max_amplitude, 0.5f * dx_extra / (max_amplitude + amplitude), remainder);
        dy = mix(
            0.5f * dy / max_amplitude, 0.5f * dy_extra / (max_amplitude + amplitude), remainder);
      }
      else {
        dx = mix(dx, dx_extra, remainder);
        dy = mix(dy, dy_extra, remainder);
      }
    }
    else if (node.normalize) {
      dx *= 0.5f / max_amplitude;
      dy *= 0.5f / max_amplitude;
    }
  }
  stack_store_float(stack, node.value_offset + 1, dx);
  stack_store_float(stack, node.value_offset + 2, dy);
}

CCL_NAMESPACE_END
