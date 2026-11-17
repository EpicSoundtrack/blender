/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/math_util.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* MaterialX rotate2d/rotate3d (and place2d rotation). Same axis-angle form as
 * the stock Vector Rotate node, but the angle's sine/cosine use the MaterialX
 * Sin/Cos path, which stays accurate for |angle| > pi on CUDA. The stock
 * Vector Rotate node keeps the fast intrinsics. */
ccl_device_inline float3 materialx_rotate_around_axis(const float3 p,
                                                      const float3 axis,
                                                      const float angle)
{
  const float costheta = materialx_cosf(angle);
  const float sintheta = materialx_sinf(angle);
  float3 r;

  r.x = ((costheta + (1 - costheta) * axis.x * axis.x) * p.x) +
        (((1 - costheta) * axis.x * axis.y - axis.z * sintheta) * p.y) +
        (((1 - costheta) * axis.x * axis.z + axis.y * sintheta) * p.z);

  r.y = (((1 - costheta) * axis.x * axis.y + axis.z * sintheta) * p.x) +
        ((costheta + (1 - costheta) * axis.y * axis.y) * p.y) +
        (((1 - costheta) * axis.y * axis.z - axis.x * sintheta) * p.z);

  r.z = (((1 - costheta) * axis.x * axis.z - axis.y * sintheta) * p.x) +
        (((1 - costheta) * axis.y * axis.z + axis.x * sintheta) * p.y) +
        ((costheta + (1 - costheta) * axis.z * axis.z) * p.z);

  return r;
}

ccl_device_noinline void svm_node_materialx_vector_rotate(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeVectorRotate &ccl_restrict node)
{
  if (!stack_valid(node.result_offset)) {
    return;
  }

  const float3 vector = stack_load(stack, node.vector);
  const float3 center = stack_load(stack, node.center);
  float3 axis;
  float axis_length;
  switch (node.rotate_type) {
    case NODE_VECTOR_ROTATE_TYPE_AXIS_X:
      axis = make_float3(1.0f, 0.0f, 0.0f);
      axis_length = 1.0f;
      break;
    case NODE_VECTOR_ROTATE_TYPE_AXIS_Y:
      axis = make_float3(0.0f, 1.0f, 0.0f);
      axis_length = 1.0f;
      break;
    case NODE_VECTOR_ROTATE_TYPE_AXIS_Z:
      axis = make_float3(0.0f, 0.0f, 1.0f);
      axis_length = 1.0f;
      break;
    default:
      axis = stack_load(stack, node.axis);
      axis_length = len(axis);
      break;
  }
  float angle = stack_load(stack, node.angle);
  angle = node.invert ? -angle : angle;
  const float3 result = (axis_length != 0.0f) ?
                            materialx_rotate_around_axis(
                                vector - center, axis / axis_length, angle) +
                                center :
                            vector;
  stack_store_float3(stack, node.result_offset, result);
}

CCL_NAMESPACE_END
