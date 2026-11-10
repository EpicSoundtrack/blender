/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/math.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* MaterialX genosl/mx_heighttonormal_vector3.osl. Derivatives are per screen
 * pixel, and the cross product is formed in texture/height coordinates. */
ccl_device_inline float3 materialx_heighttonormal(const float2 height_derivative,
                                                   const float2 uv_dx,
                                                   const float2 uv_dy,
                                                   const float scale)
{
  const float2 dH = height_derivative * (scale * (1.0f / 16.0f));
  const float3 tangent = make_float3(uv_dx.x, uv_dx.y, dH.x);
  const float3 bitangent = make_float3(uv_dy.x, uv_dy.y, dH.y);
  float3 normal = cross(tangent, bitangent);
  /* MaterialX GenOSL defines M_FLOAT_EPS as 1e-8. */
  constexpr float materialx_float_eps = 1.0e-8f;
  if (dot(normal, normal) < materialx_float_eps * materialx_float_eps) {
    normal = make_float3(0.0f, 0.0f, 1.0f);
  }
  else if (normal.z < 0.0f) {
    normal = -normal;
  }
  return normalize(normal) * 0.5f + make_float3(0.5f);
}

/* OSL mx_normalmap_float orthogonalizes dPdu/dPdv against the shading normal
 * before decoding the tangent-space normal. */
ccl_device_inline float3 materialx_normalmap_world(const float3 decoded,
                                                   const float3 normal,
                                                   const float3 dPdu,
                                                   const float3 dPdv,
                                                   const float scale)
{
  const float3 tangent = normalize(dPdu - dot(dPdu, normal) * normal);
  const float3 bitangent = normalize(dPdv - dot(dPdv, normal) * normal -
                                     dot(dPdv, tangent) * tangent);
  return normalize(tangent * decoded.x * scale + bitangent * decoded.y * scale +
                   normal * decoded.z);
}

ccl_device_noinline void svm_node_materialx_heighttonormal(
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeMaterialXHeightToNormal &ccl_restrict node)
{
  const dual1 height = stack_load<dual1>(stack, node.height);
  const dual3 uv = stack_load<dual3>(stack, node.texcoord);
  const float scale = stack_load(stack, node.scale);
  stack_store_float3(stack,
                     node.out_offset,
                     materialx_heighttonormal(make_float2(height.dx, height.dy),
                                              make_float2(uv.dx.x, uv.dx.y),
                                              make_float2(uv.dy.x, uv.dy.y),
                                              scale));
}

CCL_NAMESPACE_END
