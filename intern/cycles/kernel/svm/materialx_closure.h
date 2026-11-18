/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/closure/alloc.h"
#include "kernel/closure/bsdf.h"
#include "kernel/closure/bsdf_util.h"
#include "kernel/closure/bssrdf.h"

#include "kernel/svm/math_util.h"
#include "kernel/svm/node_types.h"
#include "kernel/svm/util.h"

CCL_NAMESPACE_BEGIN

/* NODE_MATERIALX_CLOSURE_BSDF: MaterialX-only closure leaves. The header and
 * the closure color weight are those of the stock NODE_CLOSURE_BSDF; the data
 * that follows is MaterialX-only, so the stock closure node and its data layouts
 * stay as upstream. Only the SVM glue is here; the closures are set up by the
 * stock setup functions. */

ccl_device_inline int svm_node_materialx_closure_bsdf_skip(int offset, const uint type)
{
  switch (type) {
    case CLOSURE_BSDF_PHYSICAL_CONDUCTOR:
    case CLOSURE_BSDF_F82_CONDUCTOR:
      return offset + sizeof(SVMNodeMaterialXMetallicBsdfData) / sizeof(uint);
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_LEGACY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID:
      return offset + sizeof(SVMNodeMaterialXBssrdfData) / sizeof(uint);
    default: /* CLOSURE_BSDF_DIFFUSE_ID, CLOSURE_BSDF_BURLEY_ID. */
      return offset + sizeof(SVMNodeMaterialXDiffuseBsdfData) / sizeof(uint);
  }
}

template<uint node_feature_mask, ShaderType shader_type>
ccl_device_noinline int svm_node_materialx_closure_bsdf(
    KernelGlobals kg,
    ccl_private ShaderData *sd,
    ccl_private float *ccl_restrict stack,
    const Spectrum closure_weight,
    const ccl_global SVMNodeClosureBsdf &ccl_restrict node,
    const PathRayVisibility ray_visibility,
    const uint32_t path_flag,
    int offset)
{
  const ClosureType type = node.closure_type;
  const float mix_weight = stack_load_float_default(stack, node.mix_weight_offset, 1.0f);

  /* Surface closures only, as the stock closure node (none of these has emission). */
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

  switch (type) {
    case CLOSURE_BSDF_PHYSICAL_CONDUCTOR:
    case CLOSURE_BSDF_F82_CONDUCTOR: {
      const ccl_global SVMNodeMaterialXMetallicBsdfData &cdata = svm_node_get<SVMNodeMaterialXMetallicBsdfData>(
          kg, &offset);

#ifdef __CAUSTICS_TRICKS__
      if (!kernel_data.integrator.caustics_reflective &&
          (ray_visibility & PATH_RAY_VISIBILITY_DIFFUSE))
      {
        break;
      }
#endif
      const Spectrum metallic_weight =
          (type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR) ? closure_weight * mix_weight :
                                                    rgb_to_spectrum(make_float3(mix_weight));
      ccl_private MicrofacetBsdf *bsdf = (ccl_private MicrofacetBsdf *)bsdf_alloc(
          sd, sizeof(MicrofacetBsdf), metallic_weight);

      if (bsdf != nullptr) {
        float3 N = stack_load_float3_default(stack, cdata.normal_offset, sd->N);
        N = safe_normalize_fallback(N, sd->N);
        const float3 valid_reflection_N = maybe_ensure_valid_specular_reflection(sd, N);
        const float alpha_x = stack_load(stack, cdata.alpha_x);
        const float alpha_y = stack_load(stack, cdata.alpha_y);
        if (alpha_x >= 0.0f && alpha_y >= 0.0f) {
          bsdf->alpha_x = alpha_x;
          bsdf->alpha_y = alpha_y;
          bsdf->T = zero_float3();
          if (alpha_x != alpha_y) {
            bsdf->T = safe_normalize(stack_load(stack, cdata.tangent));
            const float rotation = stack_load(stack, cdata.rotation);
            if (rotation != 0.0f) {
              bsdf->T = rotate_around_axis(bsdf->T, N, rotation * M_2PI_F);
            }
          }
        }
        else {
          const float anisotropy = saturatef(stack_load(stack, cdata.anisotropy));
          const float roughness = saturatef(stack_load(stack, cdata.roughness));
          bsdf->alpha_x = sqr(roughness);
          bsdf->alpha_y = sqr(roughness);
          if (anisotropy > 0.0f && stack_valid(cdata.tangent_offset)) {
            bsdf->T = stack_load_float3(stack, cdata.tangent_offset);
            const float aspect = sqrtf(1.0f - anisotropy * 0.9f);
            bsdf->alpha_x /= aspect;
            bsdf->alpha_y *= aspect;
            const float anisotropic_rotation = stack_load(stack, cdata.rotation);
            if (anisotropic_rotation != 0.0f) {
              bsdf->T = rotate_around_axis(bsdf->T, N, anisotropic_rotation * M_2PI_F);
            }
          }
          else {
            bsdf->T = zero_float3();
          }

        }

        bsdf->N = valid_reflection_N;
        bsdf->ior = 1.0f;

        const float thin_film_thickness = fmaxf(stack_load(stack, cdata.thin_film_thickness),
                                                1e-5f);
        const float thin_film_ior = fmaxf(stack_load(stack, cdata.thin_film_ior), 1e-5f);

        const ClosureType distribution = cdata.distribution;
        /* Setup BSDF */
        if (distribution == CLOSURE_BSDF_MICROFACET_BECKMANN_ID) {
          sd->flag |= bsdf_microfacet_beckmann_setup(bsdf);
        }
        else {
          sd->flag |= bsdf_microfacet_ggx_setup(bsdf);
        }

        const bool is_multiggx = (distribution == CLOSURE_BSDF_MICROFACET_MULTI_GGX_ID);

        if (type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR) {
          ccl_private FresnelConductor *fresnel = (ccl_private FresnelConductor *)
              closure_alloc_extra(sd, sizeof(FresnelConductor));

          if (!fresnel) {
            break;
          }

          fresnel->thin_film.thickness = thin_film_thickness;
          fresnel->thin_film.ior = thin_film_ior;

          const float3 n = max(stack_load(stack, cdata.base_ior), zero_float3());
          const float3 k = max(stack_load(stack, cdata.edge_tint_k), zero_float3());

          fresnel->ior = {rgb_to_spectrum(n), rgb_to_spectrum(k)};
          bsdf_microfacet_setup_fresnel_conductor(kg, bsdf, sd->wi, fresnel, is_multiggx);
        }
        else {
          ccl_private FresnelF82Tint *fresnel = (ccl_private FresnelF82Tint *)closure_alloc_extra(
              sd, sizeof(FresnelF82Tint));

          if (!fresnel) {
            break;
          }

          fresnel->thin_film.thickness = thin_film_thickness;
          fresnel->thin_film.ior = thin_film_ior;

          const float3 color = saturate(stack_load(stack, cdata.base_ior));
          const float3 tint = saturate(stack_load(stack, cdata.edge_tint_k));

          fresnel->f0 = rgb_to_spectrum(color);
          const Spectrum f82 = rgb_to_spectrum(tint);
          bsdf_microfacet_setup_fresnel_f82_tint(kg, bsdf, sd->wi, fresnel, f82, is_multiggx);
        }
      }
      break;
    }
#ifdef __SUBSURFACE__
    case CLOSURE_BSSRDF_BURLEY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_LEGACY_ID:
    case CLOSURE_BSSRDF_RANDOM_WALK_SKIN_ID: {
      const ccl_global SVMNodeMaterialXBssrdfData &bsdf_data = svm_node_get<SVMNodeMaterialXBssrdfData>(kg, &offset);
      float3 N = stack_load_float3_default(stack, bsdf_data.normal_offset, sd->N);
      N = safe_normalize_fallback(N, sd->N);

      /* Keep BSSRDF albedo independent of the completed closure's scalar weight. */
      const float bssrdf_closure_weight = stack_load(stack, bsdf_data.closure_weight);
      const Spectrum weight = closure_weight * bssrdf_closure_weight * mix_weight;
      ccl_private Bssrdf *bssrdf = bssrdf_alloc(sd, weight);

      if (bssrdf) {
        const float scale = stack_load(stack, bsdf_data.scale);
        bssrdf->radius = max(rgb_to_spectrum(stack_load(stack, bsdf_data.radius) * scale),
                             zero_spectrum());
        bssrdf->albedo = closure_weight;
        bssrdf->N = maybe_ensure_valid_specular_reflection(sd, N);
        bssrdf->ior = stack_load(stack, bsdf_data.ior);
        bssrdf->alpha = saturatef(stack_load(stack, bsdf_data.roughness));
        bssrdf->anisotropy = stack_load(stack, bsdf_data.anisotropy);

        sd->flag |= bssrdf_setup(sd, bssrdf, path_flag, type);
      }

      break;
    }
#endif
    default: {
      /* CLOSURE_BSDF_DIFFUSE_ID, CLOSURE_BSDF_BURLEY_ID. */
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
      break;
    }
  }
  return offset;
}

CCL_NAMESPACE_END
