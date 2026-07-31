/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0-or-later
 */

#include <gtest/gtest.h>

#include <array>

#include <pxr/base/gf/vec3f.h>
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

HdContainerDataSourceHandle vector3_parameter(const pxr::GfVec3f &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<pxr::GfVec3f>::New(value))
      .Build();
}

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
                                 const HdContainerDataSourceHandle &parameters,
                                 const HdContainerDataSourceHandle &connections = nullptr)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .SetInputConnections(connections)
      .Build();
}

HdContainerDataSourceHandle vector_math_node(const char *identifier, const bool unary)
{
  HdContainerDataSourceHandle parameters;
  if (unary) {
    parameters = HdRetainedContainerDataSource::New(
        TfToken("in"), vector3_parameter(pxr::GfVec3f(-0.25f, 0.5f, 0.75f)));
  }
  else {
    parameters = HdRetainedContainerDataSource::New(
        TfToken("in1"),
        vector3_parameter(pxr::GfVec3f(-0.25f, 0.5f, 0.75f)),
        TfToken("in2"),
        vector3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f)));
  }
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .Build();
}

}  // namespace

TEST(HdCyclesMaterialXVectorMath, lowers_direct_vector3_math_nodedefs)
{
  struct Case {
    const char *identifier;
    NodeVectorMathType math_type;
    bool unary;
  };
  const std::array<Case, 12> cases = {{{"ND_add_vector3", NODE_VECTOR_MATH_ADD, false},
                                        {"ND_subtract_vector3", NODE_VECTOR_MATH_SUBTRACT, false},
                                        {"ND_multiply_vector3", NODE_VECTOR_MATH_MULTIPLY, false},
                                        {"ND_divide_vector3", NODE_VECTOR_MATH_DIVIDE, false},
                                        {"ND_crossproduct_vector3", NODE_VECTOR_MATH_CROSS_PRODUCT, false},
                                        {"ND_dotproduct_vector3", NODE_VECTOR_MATH_DOT_PRODUCT, false},
                                        {"ND_distance_vector3", NODE_VECTOR_MATH_DISTANCE, false},
                                        {"ND_magnitude_vector3", NODE_VECTOR_MATH_LENGTH, true},
                                        {"ND_normalize_vector3", NODE_VECTOR_MATH_NORMALIZE, true},
                                        {"ND_absval_vector3", NODE_VECTOR_MATH_ABSOLUTE, true},
                                        {"ND_min_vector3", NODE_VECTOR_MATH_MINIMUM, false},
                                        {"ND_max_vector3", NODE_VECTOR_MATH_MAXIMUM, false}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
        TfToken("VectorMath"), vector_math_node(test.identifier, test.unary));
    const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXVectorMath"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, network);

    VectorMathNode *math = nullptr;
    for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
      math = dynamic_cast<VectorMathNode *>(node);
      if (math) {
        break;
      }
    }
    ASSERT_NE(math, nullptr);
    EXPECT_EQ(math->get_math_type(), test.math_type);
    EXPECT_EQ(math->get_vector1(), make_float3(-0.25f, 0.5f, 0.75f));
    if (!test.unary) {
      EXPECT_EQ(math->get_vector2(), make_float3(1.0f, 2.0f, 3.0f));
    }

    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXVectorMath, lowers_reflect_and_refract_vector3_literals)
{
  const HdContainerDataSourceHandle reflect_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.3f, -0.4f, -0.5f)),
      TfToken("normal"), vector3_parameter(pxr::GfVec3f(0.0f, 0.0f, 1.0f)));
  const HdContainerDataSourceHandle refract_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.8660254f, 0.0f, -0.5f)),
      TfToken("normal"), vector3_parameter(pxr::GfVec3f(0.0f, 0.0f, 1.0f)),
      TfToken("ior"), float_parameter(1.5f));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Reflect"), node("ND_reflect_vector3", reflect_parameters),
      TfToken("Refract"), node("ND_refract_vector3", refract_parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorMathReflectRefractLiterals"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  VectorMathNode *reflect = nullptr;
  VectorMathNode *refract = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node)) {
      reflect = math->get_math_type() == NODE_VECTOR_MATH_REFLECT ? math : reflect;
      refract = math->get_math_type() == NODE_VECTOR_MATH_REFRACT ? math : refract;
    }
  }

  ASSERT_NE(reflect, nullptr);
  EXPECT_EQ(reflect->get_vector1(), make_float3(0.3f, -0.4f, -0.5f));
  EXPECT_EQ(reflect->get_vector2(), make_float3(0.0f, 0.0f, 1.0f));

  ASSERT_NE(refract, nullptr);
  EXPECT_EQ(refract->get_vector1(), make_float3(0.8660254f, 0.0f, -0.5f));
  EXPECT_EQ(refract->get_vector2(), make_float3(0.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(refract->get_scale(), 1.5f);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXVectorMath, lowers_reflect_and_refract_vector3_links)
{
  const HdContainerDataSourceHandle incident_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), vector3_parameter(pxr::GfVec3f(0.8660254f, 0.0f, -0.5f)));
  const HdContainerDataSourceHandle normal_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), vector3_parameter(pxr::GfVec3f(0.0f, 0.0f, 1.0f)));
  const HdContainerDataSourceHandle ior_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), float_parameter(1.5f));
  const HdContainerDataSourceHandle reflect_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Incident"), TfToken("out")),
      TfToken("normal"), connection(TfToken("Normal"), TfToken("out")));
  const HdContainerDataSourceHandle refract_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Incident"), TfToken("out")),
      TfToken("normal"), connection(TfToken("Normal"), TfToken("out")),
      TfToken("ior"), connection(TfToken("Ior"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Incident"), node("ND_constant_vector3", incident_parameters),
      TfToken("Normal"), node("ND_constant_vector3", normal_parameters),
      TfToken("Ior"), node("ND_constant_float", ior_parameters),
      TfToken("Reflect"), node("ND_reflect_vector3", nullptr, reflect_connections),
      TfToken("Refract"), node("ND_refract_vector3", nullptr, refract_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorMathReflectRefractLinks"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  VectorMathNode *reflect = nullptr;
  VectorMathNode *refract = nullptr;
  ValueNode *ior = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node)) {
      reflect = math->get_math_type() == NODE_VECTOR_MATH_REFLECT ? math : reflect;
      refract = math->get_math_type() == NODE_VECTOR_MATH_REFRACT ? math : refract;
    }
    else if (ValueNode *value = dynamic_cast<ValueNode *>(node)) {
      ior = value;
    }
  }

  ASSERT_NE(reflect, nullptr);
  ASSERT_NE(refract, nullptr);
  ASSERT_NE(ior, nullptr);
  ASSERT_NE(reflect->input("Vector1")->link, nullptr);
  ASSERT_NE(reflect->input("Vector2")->link, nullptr);
  EXPECT_EQ(refract->input("Vector1")->link, reflect->input("Vector1")->link);
  EXPECT_EQ(refract->input("Vector2")->link, reflect->input("Vector2")->link);
  EXPECT_EQ(refract->input("Scale")->link, ior->output("Value"));

  material.Finalize(&session);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
