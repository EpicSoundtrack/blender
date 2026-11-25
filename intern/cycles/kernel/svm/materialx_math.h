/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/svm/map_range.h"
#include "kernel/svm/math.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* Math nodes created by MaterialX lowering (NODE_MATERIALX_MATH): the stock Math
 * evaluation, plus screen derivatives for Add and Multiply in the derivative
 * variant (the MaterialX heighttonormal chain). Stock Math keeps NODE_MATH and
 * zero derivatives. */
ccl_device_noinline void svm_node_materialx_math_derivative(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeMath &ccl_restrict node)
{
  if (node.math_type == NODE_MATH_ADD || node.math_type == NODE_MATH_MULTIPLY) {
    const dual1 a = stack_load<dual1>(stack, node.value1);
    const dual1 b = stack_load<dual1>(stack, node.value2);
    stack_store(stack, node.result_offset, node.math_type == NODE_MATH_ADD ? a + b : a * b);
  }
  else {
    svm_node_math(stack, node);
    stack_store_float(stack, node.result_offset + 1, 0.0f);
    stack_store_float(stack, node.result_offset + 2, 0.0f);
  }
}

/* Linear Map Range with derivatives. Not reached by compiled shaders: Map Range
 * has no derivative node type, so the compiler zeroes its derivatives (the fork
 * behaved the same way). Kept for the kernel tests of the heighttonormal chain. */
ccl_device_noinline void svm_node_materialx_map_range_derivative(
    ccl_private float *ccl_restrict stack, const ccl_global SVMNodeMapRange &ccl_restrict node)
{
  if (node.range_type != NODE_MAP_RANGE_LINEAR) {
    svm_node_map_range(stack, node);
    stack_store_float(stack, node.result_offset + 1, 0.0f);
    stack_store_float(stack, node.result_offset + 2, 0.0f);
    return;
  }

  const dual1 value = stack_load<dual1>(stack, node.value);
  const dual1 from_min = stack_load<dual1>(stack, node.from_min);
  const dual1 from_max = stack_load<dual1>(stack, node.from_max);
  const dual1 to_min = stack_load<dual1>(stack, node.to_min);
  const dual1 to_max = stack_load<dual1>(stack, node.to_max);

  const dual1 result = (from_max.val != from_min.val) ?
                           to_min +
                               (value - from_min) / (from_max - from_min) * (to_max - to_min) :
                           dual1(0.0f);
  stack_store(stack, node.result_offset, result);
}

CCL_NAMESPACE_END
