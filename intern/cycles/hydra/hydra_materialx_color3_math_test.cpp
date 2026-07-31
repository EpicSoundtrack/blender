/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0-or-later
 */

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <vector>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/errorMark.h>
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

HdContainerDataSourceHandle bool_parameter(const bool value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<bool>::New(value))
      .Build();
}

HdContainerDataSourceHandle color3_parameter(const pxr::GfVec3f &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<pxr::GfVec3f>::New(value))
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

HdDataSourceBaseHandle multiple_connections(const TfToken &first_node, const TfToken &second_node)
{
  const std::array<HdDataSourceBaseHandle, 2> sources = {
      HdMaterialConnectionSchema::Builder()
          .SetUpstreamNodePath(HdRetainedTypedSampledDataSource<TfToken>::New(first_node))
          .SetUpstreamNodeOutputName(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("out")))
          .Build(),
      HdMaterialConnectionSchema::Builder()
          .SetUpstreamNodePath(HdRetainedTypedSampledDataSource<TfToken>::New(second_node))
          .SetUpstreamNodeOutputName(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("out")))
          .Build()};
  return HdVectorSchema::BuildRetained(sources.size(), sources.data());
}

HdContainerDataSourceHandle node(const char *identifier,
                                  const HdContainerDataSourceHandle &parameters = nullptr,
                                  const HdContainerDataSourceHandle &connections = nullptr)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .SetInputConnections(connections)
      .Build();
}

HdContainerDataSourceHandle color_constant_node(const pxr::GfVec3f &value)
{
  return node("ND_constant_color3",
              HdRetainedContainerDataSource::New(TfToken("value"), color3_parameter(value)));
}

HdContainerDataSourceHandle float_constant_node(const float value)
{
  return node("ND_constant_float",
              HdRetainedContainerDataSource::New(TfToken("value"), float_parameter(value)));
}

int count_math_nodes(HdCyclesMaterial &material, const NodeMathType type)
{
  int count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      count += math->get_math_type() == type;
    }
  }
  return count;
}

}  // namespace

TEST(HdCyclesMaterialXColor3Math, lowers_homogeneous_and_scalar_broadcast_arithmetic_batch)
{
  struct Case {
    const char *node_name;
    const char *identifier;
    NodeMathType math_type;
    bool scalar_second;
  };
  const std::array<Case, 18> cases = {{{"Add", "ND_add_color3", NODE_MATH_ADD, false},
                                       {"AddFA", "ND_add_color3FA", NODE_MATH_ADD, true},
                                       {"Subtract", "ND_subtract_color3", NODE_MATH_SUBTRACT, false},
                                       {"SubtractFA", "ND_subtract_color3FA", NODE_MATH_SUBTRACT, true},
                                       {"Multiply", "ND_multiply_color3", NODE_MATH_MULTIPLY, false},
                                       {"MultiplyFA", "ND_multiply_color3FA", NODE_MATH_MULTIPLY, true},
                                       {"Divide", "ND_divide_color3", NODE_MATH_DIVIDE, false},
                                       {"DivideFA", "ND_divide_color3FA", NODE_MATH_DIVIDE, true},
                                       {"Min", "ND_min_color3", NODE_MATH_MINIMUM, false},
                                       {"MinFA", "ND_min_color3FA", NODE_MATH_MINIMUM, true},
                                       {"Max", "ND_max_color3", NODE_MATH_MAXIMUM, false},
                                       {"MaxFA", "ND_max_color3FA", NODE_MATH_MAXIMUM, true},
                                       {"Modulo", "ND_modulo_color3", NODE_MATH_MODULO, false},
                                       {"ModuloFA", "ND_modulo_color3FA", NODE_MATH_MODULO, true},
                                       {"Power", "ND_power_color3", NODE_MATH_POWER, false},
                                       {"PowerFA", "ND_power_color3FA", NODE_MATH_POWER, true},
                                       {"SafePower", "ND_safepower_color3", NODE_MATH_MULTIPLY, false},
                                       {"SafePowerFA", "ND_safepower_color3FA", NODE_MATH_MULTIPLY, true}}};
  std::vector<TfToken> names;
  std::vector<HdDataSourceBaseHandle> values;
  names.reserve(cases.size() + 2);
  values.reserve(cases.size() + 2);
  names.push_back(TfToken("Color"));
  values.push_back(color_constant_node(pxr::GfVec3f(2.0f, 3.0f, 4.0f)));
  names.push_back(TfToken("Scalar"));
  values.push_back(float_constant_node(2.0f));
  for (const Case &test : cases) {
    names.push_back(TfToken(test.node_name));
    values.push_back(node(test.identifier,
                          HdRetainedContainerDataSource::New(
                              TfToken("in1"), color3_parameter(pxr::GfVec3f(5.0f, 7.0f, 11.0f)),
                              TfToken("in2"), test.scalar_second ? float_parameter(2.0f) :
                                                                    color3_parameter(pxr::GfVec3f(2.0f, 3.0f, 4.0f))),
                          HdRetainedContainerDataSource::New(
                              TfToken("in1"), connection(TfToken("Color"), TfToken("out")),
                              TfToken("in2"), connection(test.scalar_second ? TfToken("Scalar") : TfToken("Color"),
                                                         TfToken("out")))));
  }
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                names.size(), names.data(), values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXColor3ArithmeticBatch"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  EXPECT_EQ(count_math_nodes(material, NODE_MATH_ADD), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_SUBTRACT), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_MULTIPLY), 12);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_DIVIDE), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_MINIMUM), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_MAXIMUM), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_MODULO), 6);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_POWER), 12);

  int color_outputs = 0;
  int linked_second_inputs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    color_outputs += dynamic_cast<CombineColorNode *>(shader_node) != nullptr;
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      linked_second_inputs += math->input("Value2")->link != nullptr;
    }
  }
  EXPECT_EQ(color_outputs, 18);
  EXPECT_GE(linked_second_inputs, 21);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXColor3Math, lowers_invert_clamp_compositing_and_conversion_residual_batch)
{
  const std::array<TfToken, 18> names = {
      TfToken("Color"),          TfToken("Mask"),          TfToken("Invert"),
      TfToken("InvertFA"),       TfToken("Clamp"),         TfToken("ClampFA"),
      TfToken("Plus"),           TfToken("Minus"),         TfToken("Difference"),
      TfToken("Burn"),           TfToken("Dodge"),         TfToken("Screen"),
      TfToken("Overlay"),        TfToken("MixColorFactor"), TfToken("Inside"),
      TfToken("Outside"),        TfToken("BooleanToColor"), TfToken("IntegerToColor")};
  const std::array<HdDataSourceBaseHandle, 18> values = {
      color_constant_node(pxr::GfVec3f(0.2f, 0.4f, 0.6f)),
      float_constant_node(0.25f),
      node("ND_invert_color3",
           HdRetainedContainerDataSource::New(TfToken("amount"), color3_parameter(pxr::GfVec3f(1.0f, 0.5f, 0.25f)),
                                              TfToken("in"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f))),
           HdRetainedContainerDataSource::New(TfToken("in"), connection(TfToken("Color"), TfToken("out")))),
      node("ND_invert_color3FA",
           HdRetainedContainerDataSource::New(TfToken("amount"), float_parameter(0.75f)),
           HdRetainedContainerDataSource::New(TfToken("amount"), connection(TfToken("Mask"), TfToken("out")),
                                              TfToken("in"), connection(TfToken("Color"), TfToken("out")))),
      node("ND_clamp_color3",
           HdRetainedContainerDataSource::New(TfToken("in"), color3_parameter(pxr::GfVec3f(-1.0f, 0.5f, 2.0f)),
                                              TfToken("low"), color3_parameter(pxr::GfVec3f(0.0f, 0.25f, 0.5f)),
                                              TfToken("high"), color3_parameter(pxr::GfVec3f(0.75f, 1.0f, 1.25f)))),
      node("ND_clamp_color3FA",
           HdRetainedContainerDataSource::New(TfToken("in"), color3_parameter(pxr::GfVec3f(-1.0f, 0.5f, 2.0f)),
                                              TfToken("low"), float_parameter(0.0f),
                                              TfToken("high"), float_parameter(1.0f))),
      node("ND_plus_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_minus_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_difference_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_burn_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_dodge_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_screen_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_overlay_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), float_parameter(0.25f))),
      node("ND_mix_color3_color3",
           HdRetainedContainerDataSource::New(TfToken("bg"), color3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
                                              TfToken("fg"), color3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)),
                                              TfToken("mix"), color3_parameter(pxr::GfVec3f(0.2f, 0.5f, 0.8f)))),
      node("ND_inside_color3", nullptr,
           HdRetainedContainerDataSource::New(TfToken("in"), connection(TfToken("Color"), TfToken("out")),
                                              TfToken("mask"), connection(TfToken("Mask"), TfToken("out")))),
      node("ND_outside_color3", nullptr,
           HdRetainedContainerDataSource::New(TfToken("in"), connection(TfToken("Color"), TfToken("out")),
                                              TfToken("mask"), connection(TfToken("Mask"), TfToken("out")))),
      node("ND_convert_boolean_color3", HdRetainedContainerDataSource::New(TfToken("in"), bool_parameter(true))),
      node("ND_convert_integer_color3", HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(7)))};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                names.size(), names.data(), values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXColor3ResidualBatch"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int blends = 0;
  int colors = 0;
  int color_combines = 0;
  int subtracts = 0;
  int minimums = 0;
  int maximums = 0;
  int multiplies = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    blends += dynamic_cast<MixColorNode *>(shader_node) != nullptr;
    colors += dynamic_cast<ColorNode *>(shader_node) != nullptr;
    color_combines += dynamic_cast<CombineColorNode *>(shader_node) != nullptr;
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      subtracts += math->get_math_type() == NODE_MATH_SUBTRACT;
      minimums += math->get_math_type() == NODE_MATH_MINIMUM;
      maximums += math->get_math_type() == NODE_MATH_MAXIMUM;
      multiplies += math->get_math_type() == NODE_MATH_MULTIPLY;
    }
  }
  EXPECT_EQ(blends, 7);
  EXPECT_EQ(colors, 3); /* Color source plus boolean and integer conversions. */
  EXPECT_GE(color_combines, 6);
  EXPECT_GE(subtracts, 6);
  EXPECT_GE(minimums, 6);
  EXPECT_GE(maximums, 6);
  EXPECT_GE(multiplies, 9);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXColor3Math, rejects_domain_and_multiplicity_without_partial_math_nodes)
{
  const HdContainerDataSourceHandle malformed_divide = node(
      "ND_divide_color3FA",
      HdRetainedContainerDataSource::New(TfToken("in1"), color3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f))),
      HdRetainedContainerDataSource::New(
          TfToken("in2"), multiple_connections(TfToken("ScalarA"), TfToken("ScalarB"))));
  const HdContainerDataSourceHandle zero_modulo = node(
      "ND_modulo_color3",
      nullptr,
      HdRetainedContainerDataSource::New(TfToken("in1"), connection(TfToken("Color"), TfToken("out")),
                                         TfToken("in2"), connection(TfToken("ZeroColor"), TfToken("out"))));
  const HdContainerDataSourceHandle bad_safepower = node(
      "ND_safepower_color3FA",
      HdRetainedContainerDataSource::New(TfToken("in1"), color3_parameter(pxr::GfVec3f(0.0f, 2.0f, 3.0f)),
                                         TfToken("in2"), float_parameter(-1.0f)));
  const std::array<TfToken, 7> names = {TfToken("Color"),
                                        TfToken("ZeroColor"),
                                        TfToken("ScalarA"),
                                        TfToken("ScalarB"),
                                        TfToken("MalformedDivide"),
                                        TfToken("ZeroModulo"),
                                        TfToken("BadSafePower")};
  const std::array<HdDataSourceBaseHandle, 7> values = {
      color_constant_node(pxr::GfVec3f(1.0f, 2.0f, 3.0f)),
      color_constant_node(pxr::GfVec3f(1.0f, 0.0f, 1.0f)),
      float_constant_node(2.0f),
      float_constant_node(3.0f),
      malformed_divide,
      zero_modulo,
      bad_safepower};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXColor3RejectedBatch"));
  TfErrorMark errors;
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  size_t error_count = 0;
  int multiplicity_count = 0;
  int divisor_count = 0;
  int safepower_count = 0;
  for (auto error = errors.GetBegin(&error_count); error != errors.GetEnd(); ++error) {
    const std::string &commentary = error->GetCommentary();
    multiplicity_count += commentary.find("requires a well-formed connection on input 'in2'") !=
                          std::string::npos;
    divisor_count += commentary.find("rejects literal zero or non-finite divisor components") !=
                     std::string::npos;
    safepower_count += commentary.find("safepower rejects literal zero base with nonpositive exponent") !=
                       std::string::npos;
  }
  EXPECT_EQ(multiplicity_count, 1);
  EXPECT_EQ(divisor_count, 1);
  EXPECT_EQ(safepower_count, 1);
  errors.Clear();

  EXPECT_EQ(count_math_nodes(material, NODE_MATH_DIVIDE), 0);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_MODULO), 0);
  EXPECT_EQ(count_math_nodes(material, NODE_MATH_POWER), 0);

  material.Finalize(&session);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
