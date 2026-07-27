/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/vectorSchema.h>

#include "hydra/material.h"
#include "hydra/session.h"
#include "scene/shader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "session/session.h"

HDCYCLES_NAMESPACE_OPEN_SCOPE

class HdCyclesMaterialTestAccess {
 public:
  static void Populate(HdCyclesMaterial *material,
                       HdCyclesSession *session,
                       const HdMaterialNetworkSchema &network)
  {
    material->Initialize(session);
    material->PopulateShaderGraph(network);
  }
};

namespace {

HdContainerDataSourceHandle float_parameter(const float value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<float>::New(value))
      .Build();
}

HdDataSourceBaseHandle connection(const TfToken &upstream_node, const TfToken &upstream_output)
{
  const HdDataSourceBaseHandle source = HdMaterialConnectionSchema::Builder()
                                      .SetUpstreamNodePath(
                                          HdRetainedTypedSampledDataSource<TfToken>::New(upstream_node))
                                      .SetUpstreamNodeOutputName(
                                          HdRetainedTypedSampledDataSource<TfToken>::New(upstream_output))
                                      .Build();
  return HdVectorSchema::BuildRetained(1, &source);
}

HdContainerDataSourceHandle node(const char *identifier,
                                  const float value,
                                  const HdContainerDataSourceHandle &connections = nullptr)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(value)))
      .SetInputConnections(connections)
      .Build();
}

}  // namespace

TEST(HdCyclesMaterialXMapping, lowers_exact_unary_math_with_materialx_socket_aliases)
{
  const HdContainerDataSourceHandle cos_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sin"), TfToken("out")));
  const HdContainerDataSourceHandle tan_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Cos"), TfToken("out")));
  const HdContainerDataSourceHandle exp_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Tan"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Sin"), node("ND_sin_float", 0.25f),
      TfToken("Cos"), node("ND_cos_float", 0.5f, cos_connections),
      TfToken("Tan"), node("ND_tan_float", 0.75f, tan_connections),
      TfToken("Exp"), node("ND_exp_float", 1.0f, exp_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXUnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *sin = nullptr;
  MathNode *cos = nullptr;
  MathNode *tan = nullptr;
  MathNode *exp = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    MathNode *math = dynamic_cast<MathNode *>(node);
    if (!math) {
      continue;
    }
    switch (math->get_math_type()) {
      case NODE_MATH_SINE: sin = math; break;
      case NODE_MATH_COSINE: cos = math; break;
      case NODE_MATH_TANGENT: tan = math; break;
      case NODE_MATH_EXPONENT: exp = math; break;
      default: break;
    }
  }
  ASSERT_NE(sin, nullptr);
  ASSERT_NE(cos, nullptr);
  ASSERT_NE(tan, nullptr);
  ASSERT_NE(exp, nullptr);
  EXPECT_EQ(sin->get_math_type(), NODE_MATH_SINE);
  EXPECT_EQ(cos->get_math_type(), NODE_MATH_COSINE);
  EXPECT_EQ(tan->get_math_type(), NODE_MATH_TANGENT);
  EXPECT_EQ(exp->get_math_type(), NODE_MATH_EXPONENT);
  EXPECT_FLOAT_EQ(sin->get_value1(), 0.25f);
  EXPECT_EQ(cos->input("Value1")->link, sin->output("Value"));
  EXPECT_EQ(tan->input("Value1")->link, cos->output("Value"));
  EXPECT_EQ(exp->input("Value1")->link, tan->output("Value"));

  material.Finalize(&session);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
