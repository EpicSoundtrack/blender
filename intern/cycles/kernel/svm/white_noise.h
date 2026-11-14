/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"
#include "util/hash.h"

CCL_NAMESPACE_BEGIN

/* MaterialX mx_noise.glsl cell noise and Worley, exact (MaterialX lowering only).
 *
 * mx_hash_int(x, y[, z[, w]]) is Bob Jenkins' lookup3 with the same seeds, mix
 * and final as Cycles' hash_uint2/3/4 (hash_uint4 is mx_hash_int(x, y, z, w):
 * seed len 4, mix, a += w, final). mx_bits_to_01 is float(bits) /
 * float(0xffffffff), and float(0xffffffff) rounds to 2^32, so the divide is an
 * exact multiply by 2^-32. Blender's White Noise and Voronoi instead hash the
 * float bits of the coordinate and their own feature points, so they never
 * reproduce MaterialX's cells. */

ccl_device_inline float mtlx_bits_to_01(const uint bits)
{
  return float(bits) * (1.0f / 4294967296.0f);
}

ccl_device_inline uint mtlx_floor_bits(const float x)
{
  return uint(float_to_int(floorf(x)));
}

/* mx_cell_noise_float(vec2 / vec3). */
ccl_device_inline float mtlx_cell_noise_float(const float2 p)
{
  return mtlx_bits_to_01(hash_uint2(mtlx_floor_bits(p.x), mtlx_floor_bits(p.y)));
}

ccl_device_inline float mtlx_cell_noise_float(const float3 p)
{
  return mtlx_bits_to_01(
      hash_uint3(mtlx_floor_bits(p.x), mtlx_floor_bits(p.y), mtlx_floor_bits(p.z)));
}

/* mx_cell_noise_vec3(vec2): mx_hash_int(ix, iy, 0|1|2). */
ccl_device_inline float3 mtlx_cell_noise_float3_int(const uint ix, const uint iy)
{
  return make_float3(mtlx_bits_to_01(hash_uint3(ix, iy, 0u)),
                     mtlx_bits_to_01(hash_uint3(ix, iy, 1u)),
                     mtlx_bits_to_01(hash_uint3(ix, iy, 2u)));
}

/* mx_cell_noise_vec3(vec3): mx_hash_int(ix, iy, iz, 0|1|2). */
ccl_device_inline float3 mtlx_cell_noise_float3_int(const uint ix, const uint iy, const uint iz)
{
  return make_float3(mtlx_bits_to_01(hash_uint4(ix, iy, iz, 0u)),
                     mtlx_bits_to_01(hash_uint4(ix, iy, iz, 1u)),
                     mtlx_bits_to_01(hash_uint4(ix, iy, iz, 2u)));
}

ccl_device_inline float3 mtlx_cell_noise_float3(const float2 p)
{
  return mtlx_cell_noise_float3_int(mtlx_floor_bits(p.x), mtlx_floor_bits(p.y));
}

ccl_device_inline float3 mtlx_cell_noise_float3(const float3 p)
{
  return mtlx_cell_noise_float3_int(
      mtlx_floor_bits(p.x), mtlx_floor_bits(p.y), mtlx_floor_bits(p.z));
}

/* mx_worley_distance: 2 Manhattan, 3 Chebyshev, otherwise dot(diff, diff)
 * (0 Euclidean, square-rooted at the end; 1 distance squared). */
ccl_device_inline float mtlx_worley_metric(const float2 diff, const int metric)
{
  if (metric == 2) {
    return fabsf(diff.x) + fabsf(diff.y);
  }
  if (metric == 3) {
    return fmaxf(fabsf(diff.x), fabsf(diff.y));
  }
  return diff.x * diff.x + diff.y * diff.y;
}

ccl_device_inline float mtlx_worley_metric(const float3 diff, const int metric)
{
  if (metric == 2) {
    return fabsf(diff.x) + fabsf(diff.y) + fabsf(diff.z);
  }
  if (metric == 3) {
    return fmaxf(fmaxf(fabsf(diff.x), fabsf(diff.y)), fabsf(diff.z));
  }
  return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
}

/* Three-nearest update of mx_worley_noise_vec3. Its x component and minpos are
 * exactly mx_worley_noise_float's and _vec2's (minpos only moves with x). */
ccl_device_inline bool mtlx_worley_insert(const float dist, ccl_private float3 *sqdist)
{
  if (dist < sqdist->x) {
    sqdist->z = sqdist->y;
    sqdist->y = sqdist->x;
    sqdist->x = dist;
    return true;
  }
  if (dist < sqdist->y) {
    sqdist->z = sqdist->y;
    sqdist->y = dist;
  }
  else if (dist < sqdist->z) {
    sqdist->z = dist;
  }
  return false;
}

/* mx_worley_noise_{float,vec2,vec3}(vec2 p, jitter, style, metric). Returns
 * (F1, F2, F3) for style 0; for style 1 (solid) mx_cell_noise_vec3 of the
 * nearest feature point when vector_output, else (mx_cell_noise_float, 0, 0). */
ccl_device float3 mtlx_worley_noise(
    const float2 p, const float jitter, const int style, const int metric, const bool vector_output)
{
  int X, Y;
  const float2 localpos = make_float2(floorfrac(p.x, &X), floorfrac(p.y, &Y));
  float3 sqdist = make_float3(1e6f, 1e6f, 1e6f);
  float2 minpos = zero_float2();
  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      const float3 tmp = mtlx_cell_noise_float3_int(uint(x + X), uint(y + Y));
      float2 off = make_float2(tmp.x, tmp.y);
      off = off - make_float2(0.5f, 0.5f);
      off = off * jitter;
      off = off + make_float2(0.5f, 0.5f);
      const float2 cellpos = make_float2(float(x), float(y)) + off;
      const float2 diff = cellpos - localpos;
      if (mtlx_worley_insert(mtlx_worley_metric(diff, metric), &sqdist)) {
        minpos = diff;
      }
    }
  }
  if (style == 1) {
    const float2 q = minpos + p;
    return vector_output ? mtlx_cell_noise_float3(q) :
                           make_float3(mtlx_cell_noise_float(q), 0.0f, 0.0f);
  }
  if (metric == 0) {
    sqdist = make_float3(sqrtf(sqdist.x), sqrtf(sqdist.y), sqrtf(sqdist.z));
  }
  return sqdist;
}

ccl_device float3 mtlx_worley_noise(
    const float3 p, const float jitter, const int style, const int metric, const bool vector_output)
{
  int X, Y, Z;
  const float3 localpos = make_float3(floorfrac(p.x, &X), floorfrac(p.y, &Y), floorfrac(p.z, &Z));
  float3 sqdist = make_float3(1e6f, 1e6f, 1e6f);
  float3 minpos = zero_float3();
  for (int x = -1; x <= 1; ++x) {
    for (int y = -1; y <= 1; ++y) {
      for (int z = -1; z <= 1; ++z) {
        float3 off = mtlx_cell_noise_float3_int(uint(x + X), uint(y + Y), uint(z + Z));
        off -= make_float3(0.5f, 0.5f, 0.5f);
        off *= jitter;
        off += make_float3(0.5f, 0.5f, 0.5f);
        const float3 cellpos = make_float3(float(x), float(y), float(z)) + off;
        const float3 diff = cellpos - localpos;
        if (mtlx_worley_insert(mtlx_worley_metric(diff, metric), &sqdist)) {
          minpos = diff;
        }
      }
    }
  }
  if (style == 1) {
    const float3 q = minpos + p;
    return vector_output ? mtlx_cell_noise_float3(q) :
                           make_float3(mtlx_cell_noise_float(q), 0.0f, 0.0f);
  }
  if (metric == 0) {
    sqdist = make_float3(sqrtf(sqdist.x), sqrtf(sqdist.y), sqrtf(sqdist.z));
  }
  return sqdist;
}

ccl_device_noinline void svm_node_tex_white_noise(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeTexWhiteNoise &ccl_restrict node)
{
  const float3 vector = stack_load(stack, node.vector);
  const float w = stack_load(stack, node.w);

  if (node.materialx_cellnoise && (node.dimensions == 2 || node.dimensions == 3)) {
    /* MaterialX cellnoise2d/3d (and the cells of randomfloat/randomcolor and
     * unifiednoise type 1): Value mx_cell_noise_float, Color mx_cell_noise_vec3. */
    const float2 p2 = make_float2(vector.x, vector.y);
    if (stack_valid(node.color_offset)) {
      stack_store_float3(stack,
                         node.color_offset,
                         node.dimensions == 2 ? mtlx_cell_noise_float3(p2) :
                                                mtlx_cell_noise_float3(vector));
    }
    if (stack_valid(node.value_offset)) {
      stack_store_float(stack,
                        node.value_offset,
                        node.dimensions == 2 ? mtlx_cell_noise_float(p2) :
                                               mtlx_cell_noise_float(vector));
    }
    return;
  }

  if (stack_valid(node.color_offset)) {
    float3 color;
    switch (node.dimensions) {
      case 1:
        color = hash_float_to_float3(w);
        break;
      case 2:
        color = hash_float2_to_float3(make_float2(vector.x, vector.y));
        break;
      case 3:
        color = hash_float3_to_float3(vector);
        break;
      case 4:
        color = hash_float4_to_float3(make_float4(vector, w));
        break;
      default:
        color = make_float3(1.0f, 0.0f, 1.0f);
        kernel_assert(0);
        break;
    }
    stack_store_float3(stack, node.color_offset, color);
  }

  if (stack_valid(node.value_offset)) {
    float value;
    switch (node.dimensions) {
      case 1:
        value = hash_float_to_float(w);
        break;
      case 2:
        value = hash_float2_to_float(make_float2(vector.x, vector.y));
        break;
      case 3:
        value = hash_float3_to_float(vector);
        break;
      case 4:
        value = hash_float4_to_float(make_float4(vector, w));
        break;
      default:
        value = 0.0f;
        kernel_assert(0);
        break;
    }
    stack_store_float(stack, node.value_offset, value);
  }
}

CCL_NAMESPACE_END
