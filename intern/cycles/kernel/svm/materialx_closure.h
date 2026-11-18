/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"

#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* NODE_MATERIALX_CLOSURE_BSDF: MaterialX-only closure leaves. The header and
 * the closure color weight are those of the stock NODE_CLOSURE_BSDF; the data
 * that follows is MaterialX-only, so the stock closure node and its data layouts
 * stay as upstream. Only the SVM glue is here; the closures are set up by the
 * stock setup functions. */

ccl_device_inline int svm_node_materialx_closure_bsdf_skip(int offset, const uint /*type*/)
{
  /* CLOSURE_BSDF_DIFFUSE_ID, CLOSURE_BSDF_BURLEY_ID. */
  return offset + sizeof(SVMNodeMaterialXDiffuseBsdfData) / sizeof(uint);
}

template<uint node_feature_mask, ShaderType shader_type>
ccl_device_noinline int svm_node_materialx_closure_bsdf(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    ccl_private float *ccl_restrict stack,
    const Spectrum closure_weight,
    const ccl_global SVMNodeClosureBsdf &ccl_restrict node,
    int offset)
{
  const ClosureType type = node.closure_type;
  const float mix_weight = stack_load_float_default(stack, node.mix_weight_offset, 1.0f);

  /* Surface BSDFs only, as the stock closure node (none of these has emission). */
  if constexpr (shader_type != SHADER_TYPE_SURFACE) {
    return svm_node_materialx_closure_bsdf_skip(offset, type);
  }
  IF_KERNEL_NODES_FEATURE(BSDF)
  {
    if (mix_weight == 0.0f) {
      return svm_node_materialx_closure_bsdf_skip(offset, type);
    }
  }
  else {
    return svm_node_materialx_closure_bsdf_skip(offset, type);
  }

  const ccl_global SVMNodeMaterialXDiffuseBsdfData &bsdf_data =
      svm_node_get<SVMNodeMaterialXDiffuseBsdfData>(kg, &offset);
  float3 N = stack_load(stack, bsdf_data.normal);
  N = safe_normalize_fallback(N, sd->N);

  const Spectrum weight = closure_weight * stack_load(stack, bsdf_data.closure_weight) *
                          mix_weight;
  const float roughness = stack_load(stack, bsdf_data.roughness);
  if (type == CLOSURE_BSDF_BURLEY_ID) {
    ccl_private BurleyBsdf *bsdf = (ccl_private BurleyBsdf *)bsdf_alloc(
        sd, sizeof(BurleyBsdf), weight);
    if (bsdf) {
      bsdf->N = N;
      sd->flag |= bsdf_burley_setup(bsdf, roughness);
      /* MaterialX burley_diffuse_bsdf does not clamp roughness. */
      bsdf->roughness = roughness;
    }
  }
  else if (diffuse_roughness_is_almost_zero(roughness)) {
    bsdf_diffuse_setup(sd, N, weight);
  }
  else {
    const Spectrum color = saturate(rgb_to_spectrum(stack_load(stack, bsdf_data.color)));
    bsdf_oren_nayar_setup(sd, N, weight, roughness, color);
  }
  return offset;
}

CCL_NAMESPACE_END
