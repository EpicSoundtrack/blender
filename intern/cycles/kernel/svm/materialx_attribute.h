/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/geom/attribute.h"
#include "kernel/geom/primitive.h"

CCL_NAMESPACE_BEGIN

/* MaterialX geompropvalue / UsdPrimvarReader scalar reads (AttributeNode with
 * use_materialx_exact). A vertex or corner float primvar that holds the same
 * value at all three triangle corners reads back as exactly that value: the
 * barycentric sum (1 - u - v) * f + u * f + v * f rounds to a neighbouring
 * float at some (u, v), more often with FMA contraction on the GPU, and a
 * per-pixel 1-ulp change in a linked input such as a fractal lacunarity is
 * visible in the result. This is the fork's former constant scalar shortcut in
 * triangle_interpolate (removed from the stock kernel in Track B4), applied
 * only to MaterialX reads; the screen derivatives are the stock ones. */
template<typename FloatType>
ccl_device_inline FloatType materialx_surface_attribute_float(KernelGlobals kg,
                                                              const ccl_private ShaderData *sd,
                                                              const AttributeDescriptor desc)
{
  FloatType f = primitive_surface_attribute<FloatType>(kg, sd, desc);
  if (!(sd->type & PRIMITIVE_TRIANGLE) ||
      (desc.element & (ATTR_ELEMENT_OBJECT | ATTR_ELEMENT_MESH)) ||
      !(desc.element & (ATTR_ELEMENT_VERTEX | ATTR_ELEMENT_CORNER)))
  {
    return f;
  }
  int i0, i1, i2;
  if (desc.element & ATTR_ELEMENT_VERTEX) {
    const uint3 tri_vindex = kernel_data_fetch(tri_vindex, sd->prim);
    i0 = tri_vindex.x;
    i1 = tri_vindex.y;
    i2 = tri_vindex.z;
  }
  else {
    const int tri = sd->prim * 3;
    i0 = tri + 0;
    i1 = tri + 1;
    i2 = tri + 2;
  }
  float c[3];
  attribute_data_fetch_3<float>(kg, desc.element, desc.offset, i0, i1, i2, c);
  if (c[0] != 0.0f && c[0] == c[1] && c[0] == c[2] && isfinite_safe(c[0])) {
    if constexpr (is_dual_v<FloatType>) {
      f.val = c[0];
    }
    else {
      f = c[0];
    }
  }
  return f;
}

CCL_NAMESPACE_END
