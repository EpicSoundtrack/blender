/* SPDX-FileCopyrightText: 2022 NVIDIA Corporation
 * SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "hydra/config.h"

#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace CCL_NS {
class Shader;
class ShaderGraph;
class ShaderInput;
class ShaderNode;
class ShaderOutput;
}  // namespace CCL_NS

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesMaterialTestAccess;

class HdCyclesMaterial final : public PXR_NS::HdMaterial {
 public:
  HdCyclesMaterial(const PXR_NS::SdfPath &sprimId);
  ~HdCyclesMaterial() override;

  PXR_NS::HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(PXR_NS::HdSceneDelegate *sceneDelegate,
            PXR_NS::HdRenderParam *renderParam,
            PXR_NS::HdDirtyBits *dirtyBits) override;

  void Finalize(PXR_NS::HdRenderParam *renderParam) override;

  CCL_NS::Shader *GetCyclesShader() const
  {
    return _shader;
  }

 private:
  friend class HdCyclesMaterialTestAccess;

  struct NodeDesc {
    CCL_NS::ShaderNode *node;
    const class UsdToCyclesMapping *mapping;
    std::unordered_map<PXR_NS::TfToken,
                       std::vector<CCL_NS::ShaderInput *>,
                       PXR_NS::TfToken::HashFunctor>
        input_endpoints;
    std::unordered_map<PXR_NS::TfToken, CCL_NS::ShaderOutput *, PXR_NS::TfToken::HashFunctor>
        output_endpoints;
    struct Color4InputEndpoint {
      CCL_NS::ShaderInput *color = nullptr;
      std::vector<CCL_NS::ShaderInput *> alpha;
    };
    std::unordered_map<PXR_NS::TfToken, Color4InputEndpoint, PXR_NS::TfToken::HashFunctor>
        color4_input_endpoints;
    std::unordered_map<PXR_NS::TfToken, CCL_NS::ShaderOutput *, PXR_NS::TfToken::HashFunctor>
        color4_alpha_output_endpoints;
    std::unordered_map<PXR_NS::TfToken,
                       std::vector<CCL_NS::ShaderOutput *>,
                       PXR_NS::TfToken::HashFunctor>
        vector4_output_endpoints;
    std::unordered_set<PXR_NS::TfToken, PXR_NS::TfToken::HashFunctor> consumed_parameters;
  };

  void Initialize(PXR_NS::HdRenderParam *renderParam);

  void UpdateParameters(NodeDesc &nodeDesc,
                        PXR_NS::HdMaterialNodeParameterContainerSchema params,
                        const PXR_NS::SdfPath &nodePath);

  void UpdateParameters(PXR_NS::HdMaterialNetworkSchema network);

  void UpdateConnections(NodeDesc &nodeDesc,
                         PXR_NS::HdMaterialNodeSchema nodeSchema,
                         const PXR_NS::SdfPath &nodePath,
                         CCL_NS::ShaderGraph *shaderGraph,
                         const std::unordered_map<PXR_NS::SdfPath, NodeDesc, PXR_NS::SdfPath::Hash>
                             &nodes);

  bool PopulateShaderGraphInternal(PXR_NS::HdMaterialNetworkSchema network,
                                   CCL_NS::ShaderGraph *graph,
                                   std::unordered_map<PXR_NS::SdfPath, NodeDesc, PXR_NS::SdfPath::Hash> &nodes);

  void PopulateShaderGraph(PXR_NS::HdMaterialNetworkSchema network);

  CCL_NS::Shader *_shader = nullptr;
  std::unordered_map<PXR_NS::SdfPath, NodeDesc, PXR_NS::SdfPath::Hash> _nodes;
};

HDCYCLES_NAMESPACE_CLOSE_SCOPE
