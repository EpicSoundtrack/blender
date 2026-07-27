/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0-or-later
 */

#include <gtest/gtest.h>

#include <array>

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

HdContainerDataSourceHandle float_parameter(const float value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<float>::New(value))
      .Build();
}

HdContainerDataSourceHandle scalar_math_node(const char *identifier, const bool binary)
{
  HdContainerDataSourceHandle parameters;
  if (binary) {
    parameters = HdRetainedContainerDataSource::New(TfToken("in1"),
                                                    float_parameter(0.25f),
                                                    TfToken("in2"),
                                                    float_parameter(0.75f));
  }
  else {
    parameters = HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.25f));
  }
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .Build();
}

}  // namespace

TEST(HdCyclesMaterialXScalarMath, lowers_exact_scalar_math_nodedefs)
{
  struct Case {
    const char *identifier;
    NodeMathType math_type;
    bool binary;
  };
  const std::array<Case, 17> cases = {{{"ND_absval_float", NODE_MATH_ABSOLUTE, false},
                                       {"ND_acos_float", NODE_MATH_ARCCOSINE, false},
                                       {"ND_add_float", NODE_MATH_ADD, true},
                                       {"ND_asin_float", NODE_MATH_ARCSINE, false},
                                       {"ND_ceil_float", NODE_MATH_CEIL, false},
                                       {"ND_cos_float", NODE_MATH_COSINE, false},
                                       {"ND_divide_float", NODE_MATH_DIVIDE, true},
                                       {"ND_exp_float", NODE_MATH_EXPONENT, false},
                                       {"ND_floor_float", NODE_MATH_FLOOR, false},
                                       {"ND_fract_float", NODE_MATH_FRACTION, false},
                                       {"ND_multiply_float", NODE_MATH_MULTIPLY, true},
                                       {"ND_round_float", NODE_MATH_ROUND, false},
                                       {"ND_sign_float", NODE_MATH_SIGN, false},
                                       {"ND_sin_float", NODE_MATH_SINE, false},
                                       {"ND_sqrt_float", NODE_MATH_SQRT, false},
                                       {"ND_subtract_float", NODE_MATH_SUBTRACT, true},
                                       {"ND_tan_float", NODE_MATH_TANGENT, false}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
        TfToken("Math"), scalar_math_node(test.identifier, test.binary));
    const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXScalarMath"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, network);

    MathNode *math = nullptr;
    for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
      math = dynamic_cast<MathNode *>(node);
      if (math) {
        break;
      }
    }
    ASSERT_NE(math, nullptr);
    EXPECT_EQ(math->get_math_type(), test.math_type);
    EXPECT_FLOAT_EQ(math->get_value1(), 0.25f);
    if (test.binary) {
      EXPECT_FLOAT_EQ(math->get_value2(), 0.75f);
    }

    material.Finalize(&session);
  }
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
