/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0-or-later
 */

#include <gtest/gtest.h>

#include <array>

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

HdContainerDataSourceHandle int_parameter(const int value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<int>::New(value))
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

HdContainerDataSourceHandle integer_math_node(const char *identifier,
                                              const bool binary,
                                              const int in1,
                                              const int in2 = 0)
{
  HdContainerDataSourceHandle parameters;
  if (binary) {
    parameters = HdRetainedContainerDataSource::New(TfToken("in1"),
                                                    int_parameter(in1),
                                                    TfToken("in2"),
                                                    int_parameter(in2));
  }
  else {
    parameters = HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(in1));
  }
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .Build();
}

HdContainerDataSourceHandle integer_rounding_node(const char *identifier, const float input)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(
          HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(input)))
      .Build();
}

HdContainerDataSourceHandle integer_math_node_with_connections(
    const char *identifier,
    const HdContainerDataSourceHandle &parameters,
    const HdContainerDataSourceHandle &connections)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .SetInputConnections(connections)
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
  const std::array<Case, 19> cases = {{{"ND_absval_float", NODE_MATH_ABSOLUTE, false},
                                       {"ND_acos_float", NODE_MATH_ARCCOSINE, false},
                                       {"ND_add_float", NODE_MATH_ADD, true},
                                       {"ND_asin_float", NODE_MATH_ARCSINE, false},
                                       {"ND_atan_float", NODE_MATH_ARCTANGENT, false},
                                       {"ND_ceil_float", NODE_MATH_CEIL, false},
                                       {"ND_cos_float", NODE_MATH_COSINE, false},
                                       {"ND_divide_float", NODE_MATH_DIVIDE, true},
                                       {"ND_exp_float", NODE_MATH_EXPONENT, false},
                                       {"ND_floor_float", NODE_MATH_FLOOR, false},
                                       {"ND_fract_float", NODE_MATH_FRACTION, false},
                                       {"ND_multiply_float", NODE_MATH_MULTIPLY, true},
                                       {"ND_power_float", NODE_MATH_POWER, true},
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

TEST(HdCyclesMaterialXScalarMath, lowers_power_float_with_exact_base_exponent_literals)
{
  const HdContainerDataSourceHandle parameters = HdRetainedContainerDataSource::New(
      TfToken("base"), float_parameter(-2.0f), TfToken("exponent"), float_parameter(2.0f));
  const HdMaterialNetworkSchema network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Power"),
              HdMaterialNodeSchema::Builder()
                  .SetNodeIdentifier(
                      HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("ND_power_float")))
                  .SetParameters(parameters)
                  .Build()))
          .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXPowerFloatLiterals"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *power = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(node);
        math && math->get_math_type() == NODE_MATH_POWER)
    {
      power = math;
      break;
    }
  }
  ASSERT_NE(power, nullptr);
  EXPECT_FLOAT_EQ(power->get_value1(), -2.0f);
  EXPECT_FLOAT_EQ(power->get_value2(), 2.0f);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXScalarMath, lowers_power_float_with_exact_base_exponent_links)
{
  const HdContainerDataSourceHandle base_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), float_parameter(2.0f));
  const HdContainerDataSourceHandle exponent_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), float_parameter(3.0f));
  const HdContainerDataSourceHandle power_connections = HdRetainedContainerDataSource::New(
      TfToken("base"), connection(TfToken("Base"), TfToken("out")),
      TfToken("exponent"), connection(TfToken("Exponent"), TfToken("out")));
  const HdContainerDataSourceHandle power_parameters = HdRetainedContainerDataSource::New(
      TfToken("base"), float_parameter(0.0f), TfToken("exponent"), float_parameter(0.0f));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Base"),
      HdMaterialNodeSchema::Builder()
          .SetNodeIdentifier(
              HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("ND_constant_float")))
          .SetParameters(base_parameters)
          .Build(),
      TfToken("Exponent"),
      HdMaterialNodeSchema::Builder()
          .SetNodeIdentifier(
              HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("ND_constant_float")))
          .SetParameters(exponent_parameters)
          .Build(),
      TfToken("Power"),
      HdMaterialNodeSchema::Builder()
          .SetNodeIdentifier(
              HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("ND_power_float")))
          .SetParameters(power_parameters)
          .SetInputConnections(power_connections)
          .Build());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXPowerFloatLinks"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *power = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(node);
        math && math->get_math_type() == NODE_MATH_POWER)
    {
      power = math;
    }
  }
  ASSERT_NE(power, nullptr);
  ASSERT_NE(power->input("Value1")->link, nullptr);
  ASSERT_NE(power->input("Value2")->link, nullptr);
  EXPECT_NE(power->input("Value1")->link, power->input("Value2")->link);
  EXPECT_FLOAT_EQ(power->get_value1(), 0.0f);
  EXPECT_FLOAT_EQ(power->get_value2(), 0.0f);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXIntegerMath, lowers_literal_integer_batch_as_exact_constants)
{
  struct Case {
    const char *identifier;
    bool binary;
    int in1;
    int in2;
    int expected;
  };
  const std::array<Case, 2> cases = {{{"ND_add_integer", true, 7, 5, 12},
                                      {"ND_subtract_integer", true, 7, 11, -4}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
        TfToken("IntegerMath"),
        integer_math_node(test.identifier, test.binary, test.in1, test.in2));
    const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXIntegerMath"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, network);

    ValueNode *value = nullptr;
    MathNode *math = nullptr;
    for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
      value = value ? value : dynamic_cast<ValueNode *>(node);
      math = math ? math : dynamic_cast<MathNode *>(node);
    }
    ASSERT_NE(value, nullptr);
    EXPECT_FLOAT_EQ(value->get_value(), float(test.expected));
    EXPECT_EQ(value->output("Value")->socket_type.type, SocketType::FLOAT);
    EXPECT_EQ(math, nullptr) << "integer MaterialX must not be approximated with float Math";

    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXIntegerMath, rounds_fractional_float_literals_to_exact_integer_constants)
{
  struct Case {
    const char *identifier;
    float input;
    int expected;
  };
  const std::array<Case, 3> cases = {{{"ND_floor_integer", -2.25f, -3},
                                      {"ND_ceil_integer", 2.25f, 3},
                                      {"ND_round_integer", -2.6f, -3}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
        TfToken("IntegerMath"), integer_rounding_node(test.identifier, test.input));
    const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXIntegerRounding"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, network);

    ValueNode *value = nullptr;
    for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
      value = value ? value : dynamic_cast<ValueNode *>(node);
    }
    ASSERT_NE(value, nullptr);
    EXPECT_FLOAT_EQ(value->get_value(), float(test.expected));
    EXPECT_EQ(value->output("Value")->socket_type.type, SocketType::FLOAT);

    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXIntegerMath, rejects_wrong_typed_integer_links_atomically)
{
  const HdContainerDataSourceHandle source = HdMaterialNodeSchema::Builder()
                                           .SetNodeIdentifier(
                                               HdRetainedTypedSampledDataSource<TfToken>::New(
                                                   TfToken("ND_constant_float")))
                                           .SetParameters(HdRetainedContainerDataSource::New(
                                               TfToken("value"), float_parameter(1.0f)))
                                           .Build();
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Source"),
      source,
      TfToken("IntegerMath"),
      integer_math_node_with_connections(
          "ND_add_integer",
          HdRetainedContainerDataSource::New(TfToken("in2"), int_parameter(2)),
          HdRetainedContainerDataSource::New(TfToken("in1"),
                                             connection(TfToken("Source"), TfToken("out")))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerLinkedRejected"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int value_count = 0;
  int math_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    value_count += dynamic_cast<ValueNode *>(node) != nullptr;
    math_count += dynamic_cast<MathNode *>(node) != nullptr;
  }
  EXPECT_EQ(value_count, 0)
      << "wrong-typed integer links must reject the complete candidate graph";
  EXPECT_EQ(math_count, 0) << "rejected integer lowering must not mutate the graph with Math nodes";

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXIntegerMath, rejects_unsupported_integer_literals_without_graph_mutation)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("MissingOperand"),
      integer_math_node_with_connections(
          "ND_subtract_integer",
          HdRetainedContainerDataSource::New(TfToken("in1"), int_parameter(9)),
          nullptr),
      TfToken("NotExactlyRepresentable"),
      integer_math_node("ND_add_integer", true, 16777216, 1));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerUnsupportedRejected"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int value_count = 0;
  int math_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    value_count += dynamic_cast<ValueNode *>(node) != nullptr;
    math_count += dynamic_cast<MathNode *>(node) != nullptr;
  }
  EXPECT_EQ(value_count, 0);
  EXPECT_EQ(math_count, 0);

  material.Finalize(&session);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
