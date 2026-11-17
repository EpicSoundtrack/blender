/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

#include "kernel/util/colorspace.h"

CCL_NAMESPACE_BEGIN

/* MaterialX 1.39 pbrlib genglsl mx_blackbody, transcribed in the same
 * operation order: clamp to [1667, 25000] K, the Planckian-locus cubic
 * spline for (xc, yc), XYZ = (xc/yc, 1, (1-xc-yc)/yc), the Rec.709
 * XYZ_to_RGB matrix and max(rgb, 0); yc <= 0 returns (1, 1, 1). The result
 * is linear Rec.709. Used only by the MaterialX Blackbody node. */
ccl_device float3 svm_math_materialx_blackbody_rec709(float temperature)
{
  temperature = clamp(temperature, 1667.0f, 25000.0f);
  const float t = 1000.0f / temperature;
  const float t2 = t * t;
  const float t3 = t * t * t;
  float xc;
  if (temperature < 4000.0f) {
    xc = -0.2661239f * t3 - 0.2343580f * t2 + 0.8776956f * t + 0.179910f;
  }
  else {
    xc = -3.0258469f * t3 + 2.1070379f * t2 + 0.2226347f * t + 0.240390f;
  }
  const float xc2 = xc * xc;
  const float xc3 = xc * xc * xc;
  float yc;
  if (temperature < 2222.0f) {
    yc = -1.1063814f * xc3 - 1.34811020f * xc2 + 2.18555832f * xc - 0.20219683f;
  }
  else if (temperature < 4000.0f) {
    yc = -0.9549476f * xc3 - 1.37418593f * xc2 + 2.09137015f * xc - 0.16748867f;
  }
  else {
    yc = 3.0817580f * xc3 - 5.87338670f * xc2 + 3.75112997f * xc - 0.37001483f;
  }
  if (yc <= 0.0f) {
    return make_float3(1.0f, 1.0f, 1.0f);
  }
  const float X = xc / yc;
  const float Y = 1.0f;
  const float Z = (1.0f - xc - yc) / yc;
  const float3 rgb = make_float3(3.2406f * X + -1.5372f * Y + -0.4986f * Z,
                                 -0.9689f * X + 1.8758f * Y + 0.0415f * Z,
                                 0.0557f * X + -0.2040f * Y + 1.0570f * Z);
  return max(rgb, zero_float3());
}

ccl_device_noinline void svm_node_materialx_blackbody(
    KernelGlobals kg,
    ccl_private float *ccl_restrict stack,
    const ccl_global SVMNodeBlackbody &ccl_restrict node)
{
  const float temperature = stack_load(stack, node.temperature);
  float3 color_rgb = rec709_to_rgb(kg, svm_math_materialx_blackbody_rec709(temperature));
  color_rgb = max(color_rgb, zero_float3());
  stack_store_float3(stack, node.color_offset, color_rgb);
}

CCL_NAMESPACE_END
