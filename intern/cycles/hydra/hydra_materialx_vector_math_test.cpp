/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0-or-later
 */

#include <gtest/gtest.h>

#include <array>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>

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

HDCYCLES_NAMESPACE_CLOSE_SCOPE
