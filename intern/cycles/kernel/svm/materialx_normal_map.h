/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/materialx_heighttonormal.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* MaterialX normalmap_float fed by heighttonormal: decode the encoded sample
 * and build the frame from dPdu/dPdv orthogonalized against the shading
 * normal (OSL mx_normalmap_float). An all-black sample is the flat normal.
 * Uses the SVMNodeNormalMap layout; space, convention, base and the tangent
 * attributes are not used. */
ccl_device_noinline void svm_node_materialx_normal_map(
    ccl_private ShaderData *sd,
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeNormalMap &ccl_restrict node)
{
  float3 color = stack_load(stack, node.color);
  color = 2.0f * make_float3(color.x - 0.5f, color.y - 0.5f, color.z - 0.5f);
  if (color == make_float3(-1.0f)) {
    stack_store_float3(stack, node.normal_offset, sd->N);
    return;
  }
  const float strength = stack_load(stack, node.strength);
  stack_store_float3(stack,
                     node.normal_offset,
                     materialx_normalmap_world(color, sd->N, sd->dPdu, sd->dPdv, strength));
}

CCL_NAMESPACE_END
