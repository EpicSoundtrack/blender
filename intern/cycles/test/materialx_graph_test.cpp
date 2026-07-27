/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <filesystem>
#include <fstream>

#include "materialx/authority.h"
#include "materialx/graph.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"

CCL_NAMESPACE_BEGIN

namespace {

class TemporaryImage {
 public:
  TemporaryImage()
      : path_(std::filesystem::temp_directory_path() / "cycles_materialx_graph_test.ppm")
  {
    std::ofstream file(path_, std::ios::binary);
    file << "P6\n1 1\n255\n";
    const char pixel[] = {0, static_cast<char>(255), 0};
    file.write(pixel, sizeof(pixel));
  }

  ~TemporaryImage()
  {
    std::error_code error;
    std::filesystem::remove(path_, error);
  }

  string path() const
  {
    return path_.string();
  }

 private:
  std::filesystem::path path_;
};

}  // namespace

TEST(materialx_graph, lowers_multiply_float_to_math_multiply)
{
  materialx::Graph source;
  source.nodes = {{"multiply", "ND_multiply_float", {{"in1", 0.25f}, {"in2", 0.5f}}}};
  source.nodes[0].outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  EXPECT_TRUE(materialx::lower(source, &graph));

  MathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->type == MathNode::get_node_type()) {
      math = static_cast<MathNode *>(node);
      break;
    }
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(math->get_value1(), 0.25f);
  EXPECT_FLOAT_EQ(math->get_value2(), 0.5f);
}

TEST(materialx_graph, lowers_unclamped_mix_float_color3_and_vector3_to_native_arithmetic)
{
  materialx::Node scalar_mix;
  scalar_mix.name = "ScalarMix";
  scalar_mix.nodedef = "ND_mix_float";
  scalar_mix.inputs = {{"bg", 2.0f}, {"fg", 6.0f}, {"mix", -0.5f}};
  scalar_mix.outputs["out"] = materialx::Type::Float;

  materialx::Node background;
  background.name = "Background";
  background.nodedef = "ND_constant_color3";
  background.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  background.outputs["out"] = materialx::Type::Color3;
  materialx::Node foreground = background;
  foreground.name = "Foreground";
  foreground.color3_inputs["value"] = make_float3(0.4f, 0.5f, 0.6f);
  materialx::Node color_mix;
  color_mix.name = "ColorMix";
  color_mix.nodedef = "ND_mix_color3";
  color_mix.links["bg"] = {"Background", "out", materialx::Type::Color3};
  color_mix.links["fg"] = {"Foreground", "out", materialx::Type::Color3};
  color_mix.links["mix"] = {"ScalarMix", "out", materialx::Type::Float};
  color_mix.outputs["out"] = materialx::Type::Color3;

  materialx::Node vector_background;
  vector_background.name = "VectorBackground";
  vector_background.nodedef = "ND_constant_vector3";
  vector_background.vector3_inputs["value"] = make_float3(1.0f, 2.0f, 3.0f);
  vector_background.outputs["out"] = materialx::Type::Vector3;
  materialx::Node vector_foreground = vector_background;
  vector_foreground.name = "VectorForeground";
  vector_foreground.vector3_inputs["value"] = make_float3(4.0f, 5.0f, 6.0f);
  materialx::Node vector_mix;
  vector_mix.name = "VectorMix";
  vector_mix.nodedef = "ND_mix_vector3";
  vector_mix.links["bg"] = {"VectorBackground", "out", materialx::Type::Vector3};
  vector_mix.links["fg"] = {"VectorForeground", "out", materialx::Type::Vector3};
  vector_mix.links["mix"] = {"ScalarMix", "out", materialx::Type::Float};
  vector_mix.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{scalar_mix, background, foreground, color_mix, vector_background, vector_foreground, vector_mix}},
      &graph));

  MathNode *scalar_delta = nullptr;
  MathNode *scalar_product = nullptr;
  MathNode *scalar_result = nullptr;
  CombineColorNode *color_factor = nullptr;
  CombineXYZNode *vector_factor = nullptr;
  for (ShaderNode *node : graph.nodes) {
    scalar_delta = node->name == "ScalarMix.delta" ? dynamic_cast<MathNode *>(node) : scalar_delta;
    scalar_product = node->name == "ScalarMix.product" ? dynamic_cast<MathNode *>(node) : scalar_product;
    scalar_result = node->name == "ScalarMix" ? dynamic_cast<MathNode *>(node) : scalar_result;
    color_factor = node->name == "ColorMix.factor" ? dynamic_cast<CombineColorNode *>(node) : color_factor;
    vector_factor = node->name == "VectorMix.factor" ? dynamic_cast<CombineXYZNode *>(node) : vector_factor;
  }
  ASSERT_NE(scalar_delta, nullptr);
  ASSERT_NE(scalar_product, nullptr);
  ASSERT_NE(scalar_result, nullptr);
  EXPECT_EQ(scalar_delta->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(scalar_product->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(scalar_result->get_math_type(), NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(scalar_product->get_value2(), -0.5f);
  ASSERT_NE(color_factor, nullptr);
  ASSERT_NE(vector_factor, nullptr);
  EXPECT_EQ(color_factor->input("Red")->link, scalar_result->output("Value"));
  EXPECT_EQ(vector_factor->input("X")->link, scalar_result->output("Value"));
}

TEST(materialx_graph, lowers_nested_vector2_uv_utilities_to_native_vector_routing)
{
  materialx::Node constant;
  constant.name = "UV";
  constant.nodedef = "ND_constant_vector2";
  constant.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  constant.outputs["out"] = materialx::Type::Vector2;

  materialx::Node combine;
  combine.name = "Offset";
  combine.nodedef = "ND_combine2_vector2";
  combine.inputs["in1"] = 0.5f;
  combine.inputs["in2"] = 0.25f;
  combine.outputs["out"] = materialx::Type::Vector2;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector2";
  add.links["in1"] = {"UV", "out", materialx::Type::Vector2};
  add.links["in2"] = {"Offset", "out", materialx::Type::Vector2};
  add.outputs["out"] = materialx::Type::Vector2;

  materialx::Node extract;
  extract.name = "Extract";
  extract.nodedef = "ND_extract_vector2";
  extract.int_inputs["index"] = 1;
  extract.links["in"] = {"Add", "out", materialx::Type::Vector2};
  extract.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Extract", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant, combine, add, extract, surface}}, &graph));

  CombineXYZNode *uv = nullptr;
  CombineXYZNode *offset = nullptr;
  VectorMathNode *math = nullptr;
  SeparateXYZNode *separate = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "UV") uv = dynamic_cast<CombineXYZNode *>(node);
    if (node->name == "Offset") offset = dynamic_cast<CombineXYZNode *>(node);
    if (node->name == "Add") math = dynamic_cast<VectorMathNode *>(node);
    if (node->name == "Extract") separate = dynamic_cast<SeparateXYZNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(uv, nullptr);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(math, nullptr);
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(uv->get_x(), 0.25f);
  EXPECT_FLOAT_EQ(uv->get_y(), 0.75f);
  EXPECT_FLOAT_EQ(uv->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(offset->get_x(), 0.5f);
  EXPECT_FLOAT_EQ(offset->get_y(), 0.25f);
  EXPECT_FLOAT_EQ(offset->get_z(), 0.0f);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_ADD);
  EXPECT_EQ(math->input("Vector1")->link, uv->output("Vector"));
  EXPECT_EQ(math->input("Vector2")->link, offset->output("Vector"));
  ASSERT_NE(separate->input("Vector")->link, nullptr);
  EXPECT_FALSE(math->output("Vector")->links.empty());
  EXPECT_EQ(principled->input("Roughness")->link, separate->output("Y"));
}

TEST(materialx_graph, lowers_exact_vector2_magnitude_and_dotproduct_to_scalar_outputs)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector2";
  first.vector2_inputs["value"] = make_float2(3.0f, 4.0f);
  first.outputs["out"] = materialx::Type::Vector2;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector2";
  second.vector2_inputs["value"] = make_float2(1.0f, 2.0f);
  second.outputs["out"] = materialx::Type::Vector2;

  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector2";
  magnitude.links["in"] = {"First", "out", materialx::Type::Vector2};
  magnitude.outputs["out"] = materialx::Type::Float;

  materialx::Node dotproduct;
  dotproduct.name = "DotProduct";
  dotproduct.nodedef = "ND_dotproduct_vector2";
  dotproduct.links["in1"] = {"First", "out", materialx::Type::Vector2};
  dotproduct.links["in2"] = {"Second", "out", materialx::Type::Vector2};
  dotproduct.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"DotProduct", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, magnitude, dotproduct, surface}}, &graph));

  VectorMathNode *lowered_magnitude = nullptr;
  VectorMathNode *lowered_dotproduct = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    lowered_magnitude = node->name == "Magnitude" ? dynamic_cast<VectorMathNode *>(node) :
                                                      lowered_magnitude;
    lowered_dotproduct = node->name == "DotProduct" ? dynamic_cast<VectorMathNode *>(node) :
                                                        lowered_dotproduct;
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(lowered_magnitude, nullptr);
  ASSERT_NE(lowered_dotproduct, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(lowered_magnitude->get_math_type(), NODE_VECTOR_MATH_LENGTH);
  EXPECT_EQ(lowered_dotproduct->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(principled->input("Roughness")->link, lowered_dotproduct->output("Value"));
}

TEST(materialx_graph, lowers_exact_vector2_distance_to_scalar_output)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector2";
  first.vector2_inputs["value"] = make_float2(3.0f, 4.0f);
  first.outputs["out"] = materialx::Type::Vector2;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector2";
  second.vector2_inputs["value"] = make_float2(0.0f, 0.0f);
  second.outputs["out"] = materialx::Type::Vector2;

  materialx::Node distance;
  distance.name = "Distance";
  distance.nodedef = "ND_distance_vector2";
  distance.links["in1"] = {"First", "out", materialx::Type::Vector2};
  distance.links["in2"] = {"Second", "out", materialx::Type::Vector2};
  distance.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, distance}}, &graph));

  VectorMathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = node->name == "Distance" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
}

TEST(materialx_graph, lowers_exact_vector3_distance_to_scalar_output)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector3";
  first.vector3_inputs["value"] = make_float3(3.0f, 4.0f, 0.0f);
  first.outputs["out"] = materialx::Type::Vector3;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector3";
  second.vector3_inputs["value"] = make_float3(0.0f, 0.0f, 0.0f);
  second.outputs["out"] = materialx::Type::Vector3;

  materialx::Node distance;
  distance.name = "Distance";
  distance.nodedef = "ND_distance_vector3";
  distance.links["in1"] = {"First", "out", materialx::Type::Vector3};
  distance.links["in2"] = {"Second", "out", materialx::Type::Vector3};
  distance.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, distance}}, &graph));

  VectorMathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = node->name == "Distance" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
}

TEST(materialx_graph, lowers_exact_unary_vector3_utilities)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(-1.25f, 2.75f, -3.5f);
  input.outputs["out"] = materialx::Type::Vector3;

  materialx::Node absval;
  absval.name = "Abs";
  absval.nodedef = "ND_absval_vector3";
  absval.links["in"] = {"Input", "out", materialx::Type::Vector3};
  absval.outputs["out"] = materialx::Type::Vector3;

  materialx::Node floor;
  floor.name = "Floor";
  floor.nodedef = "ND_floor_vector3";
  floor.links["in"] = {"Abs", "out", materialx::Type::Vector3};
  floor.outputs["out"] = materialx::Type::Vector3;

  materialx::Node ceil;
  ceil.name = "Ceil";
  ceil.nodedef = "ND_ceil_vector3";
  ceil.links["in"] = {"Floor", "out", materialx::Type::Vector3};
  ceil.outputs["out"] = materialx::Type::Vector3;

  materialx::Node fract;
  fract.name = "Fract";
  fract.nodedef = "ND_fract_vector3";
  fract.links["in"] = {"Ceil", "out", materialx::Type::Vector3};
  fract.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, absval, floor, ceil, fract}}, &graph));

  bool found_absval = false;
  bool found_floor = false;
  bool found_ceil = false;
  bool found_fract = false;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Abs") found_absval = dynamic_cast<VectorMathNode *>(node) != nullptr &&
                                            static_cast<VectorMathNode *>(node)->get_math_type() == NODE_VECTOR_MATH_ABSOLUTE;
    if (node->name == "Floor") found_floor = dynamic_cast<VectorMathNode *>(node) != nullptr &&
                                              static_cast<VectorMathNode *>(node)->get_math_type() == NODE_VECTOR_MATH_FLOOR;
    if (node->name == "Ceil") found_ceil = dynamic_cast<VectorMathNode *>(node) != nullptr &&
                                            static_cast<VectorMathNode *>(node)->get_math_type() == NODE_VECTOR_MATH_CEIL;
    if (node->name == "Fract") found_fract = dynamic_cast<VectorMathNode *>(node) != nullptr &&
                                              static_cast<VectorMathNode *>(node)->get_math_type() == NODE_VECTOR_MATH_FRACTION;
  }
  EXPECT_TRUE(found_absval);
  EXPECT_TRUE(found_floor);
  EXPECT_TRUE(found_ceil);
  EXPECT_TRUE(found_fract);
}

TEST(materialx_graph, lowers_exact_trigonometric_vector3_nodes)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  input.outputs["out"] = materialx::Type::Vector3;

  materialx::Node sine;
  sine.name = "Sine";
  sine.nodedef = "ND_sin_vector3";
  sine.links["in"] = {"Input", "out", materialx::Type::Vector3};
  sine.outputs["out"] = materialx::Type::Vector3;

  materialx::Node cosine;
  cosine.name = "Cosine";
  cosine.nodedef = "ND_cos_vector3";
  cosine.links["in"] = {"Sine", "out", materialx::Type::Vector3};
  cosine.outputs["out"] = materialx::Type::Vector3;

  materialx::Node tangent;
  tangent.name = "Tangent";
  tangent.nodedef = "ND_tan_vector3";
  tangent.links["in"] = {"Cosine", "out", materialx::Type::Vector3};
  tangent.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, sine, cosine, tangent}}, &graph));

  bool found_sine = false;
  bool found_cosine = false;
  bool found_tangent = false;
  for (ShaderNode *node : graph.nodes) {
    const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
    if (!math) continue;
    found_sine |= math->get_math_type() == NODE_VECTOR_MATH_SINE;
    found_cosine |= math->get_math_type() == NODE_VECTOR_MATH_COSINE;
    found_tangent |= math->get_math_type() == NODE_VECTOR_MATH_TANGENT;
  }
  EXPECT_TRUE(found_sine);
  EXPECT_TRUE(found_cosine);
  EXPECT_TRUE(found_tangent);
}

TEST(materialx_graph, lowers_exact_minimum_and_maximum_vector3_nodes)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector3";
  first.vector3_inputs["value"] = make_float3(-1.0f, 2.0f, 5.0f);
  first.outputs["out"] = materialx::Type::Vector3;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector3";
  second.vector3_inputs["value"] = make_float3(3.0f, 1.0f, 4.0f);
  second.outputs["out"] = materialx::Type::Vector3;

  materialx::Node minimum;
  minimum.name = "Minimum";
  minimum.nodedef = "ND_min_vector3";
  minimum.links["in1"] = {"First", "out", materialx::Type::Vector3};
  minimum.links["in2"] = {"Second", "out", materialx::Type::Vector3};
  minimum.outputs["out"] = materialx::Type::Vector3;

  materialx::Node maximum;
  maximum.name = "Maximum";
  maximum.nodedef = "ND_max_vector3";
  maximum.links["in1"] = {"Minimum", "out", materialx::Type::Vector3};
  maximum.links["in2"] = {"Second", "out", materialx::Type::Vector3};
  maximum.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, minimum, maximum}}, &graph));

  bool found_minimum = false;
  bool found_maximum = false;
  for (ShaderNode *node : graph.nodes) {
    const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
    if (!math) continue;
    found_minimum |= math->get_math_type() == NODE_VECTOR_MATH_MINIMUM;
    found_maximum |= math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM;
  }
  EXPECT_TRUE(found_minimum);
  EXPECT_TRUE(found_maximum);
}

TEST(materialx_graph, lowers_exact_sign_vector3_node)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(-1.0f, 0.0f, 1.0f);
  input.outputs["out"] = materialx::Type::Vector3;

  materialx::Node sign;
  sign.name = "Sign";
  sign.nodedef = "ND_sign_vector3";
  sign.links["in"] = {"Input", "out", materialx::Type::Vector3};
  sign.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, sign}}, &graph));

  VectorMathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = node->name == "Sign" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SIGN);
}

TEST(materialx_graph, lowers_exact_vector3_float_multiply_to_scale)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(1.0f, 2.0f, 3.0f);
  input.outputs["out"] = materialx::Type::Vector3;

  materialx::Node multiply;
  multiply.name = "Multiply";
  multiply.nodedef = "ND_multiply_vector3FA";
  multiply.links["in1"] = {"Input", "out", materialx::Type::Vector3};
  multiply.inputs["in2"] = 2.5f;
  multiply.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, multiply}}, &graph));

  VectorMathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = node->name == "Multiply" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_EQ(math->get_scale(), 2.5f);
}

TEST(materialx_graph, lowers_exact_vector3_float_add_and_subtract_with_scalar_broadcast)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(1.0f, 2.0f, 3.0f);
  input.outputs["out"] = materialx::Type::Vector3;

  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 2.5f;
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector3FA";
  add.links["in1"] = {"Input", "out", materialx::Type::Vector3};
  add.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  add.outputs["out"] = materialx::Type::Vector3;

  materialx::Node subtract;
  subtract.name = "Subtract";
  subtract.nodedef = "ND_subtract_vector3FA";
  subtract.links["in1"] = {"Add", "out", materialx::Type::Vector3};
  subtract.inputs["in2"] = 0.5f;
  subtract.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, scalar, add, subtract}}, &graph));

  VectorMathNode *add_math = nullptr;
  VectorMathNode *subtract_math = nullptr;
  CombineXYZNode *broadcast = nullptr;
  for (ShaderNode *node : graph.nodes) {
    add_math = node->name == "Add" ? dynamic_cast<VectorMathNode *>(node) : add_math;
    subtract_math = node->name == "Subtract" ? dynamic_cast<VectorMathNode *>(node) : subtract_math;
    broadcast = node->name == "Add.broadcast" ? dynamic_cast<CombineXYZNode *>(node) : broadcast;
  }
  ASSERT_NE(add_math, nullptr);
  ASSERT_NE(subtract_math, nullptr);
  ASSERT_NE(broadcast, nullptr);
  EXPECT_EQ(add_math->get_math_type(), NODE_VECTOR_MATH_ADD);
  EXPECT_EQ(subtract_math->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(subtract_math->get_vector2(), make_float3(0.5f, 0.5f, 0.5f));
}

TEST(materialx_graph, lowers_exact_vector2_float_multiply_to_scale)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector2";
  input.vector2_inputs["value"] = make_float2(1.0f, 2.0f);
  input.outputs["out"] = materialx::Type::Vector2;
  materialx::Node multiply;
  multiply.name = "Multiply";
  multiply.nodedef = "ND_multiply_vector2FA";
  multiply.links["in1"] = {"Input", "out", materialx::Type::Vector2};
  multiply.inputs["in2"] = 2.5f;
  multiply.outputs["out"] = materialx::Type::Vector2;
  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, multiply}}, &graph));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : graph.nodes) math = node->name == "Multiply" ? dynamic_cast<VectorMathNode *>(node) : math;
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_EQ(math->get_scale(), 2.5f);
}

TEST(materialx_graph, lowers_exact_vector2_float_add_and_subtract_with_xy_broadcast)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector2";
  input.vector2_inputs["value"] = make_float2(1.0f, 2.0f);
  input.outputs["out"] = materialx::Type::Vector2;
  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 2.5f;
  scalar.outputs["out"] = materialx::Type::Float;
  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector2FA";
  add.links["in1"] = {"Input", "out", materialx::Type::Vector2};
  add.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  add.outputs["out"] = materialx::Type::Vector2;
  materialx::Node subtract;
  subtract.name = "Subtract";
  subtract.nodedef = "ND_subtract_vector2FA";
  subtract.links["in1"] = {"Add", "out", materialx::Type::Vector2};
  subtract.inputs["in2"] = 0.5f;
  subtract.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, scalar, add, subtract}}, &graph));
  VectorMathNode *add_math = nullptr;
  VectorMathNode *subtract_math = nullptr;
  CombineXYZNode *broadcast = nullptr;
  CombineXYZNode *subtract_repack = nullptr;
  for (ShaderNode *node : graph.nodes) {
    add_math = node->name == "Add" ? dynamic_cast<VectorMathNode *>(node) : add_math;
    subtract_math = node->name == "Subtract" ? dynamic_cast<VectorMathNode *>(node) : subtract_math;
    broadcast = node->name == "Add.broadcast" ? dynamic_cast<CombineXYZNode *>(node) : broadcast;
    subtract_repack = node->name == "Subtract.vector2" ? dynamic_cast<CombineXYZNode *>(node) : subtract_repack;
  }
  ASSERT_NE(add_math, nullptr);
  ASSERT_NE(subtract_math, nullptr);
  ASSERT_NE(broadcast, nullptr);
  ASSERT_NE(subtract_repack, nullptr);
  EXPECT_EQ(add_math->get_math_type(), NODE_VECTOR_MATH_ADD);
  EXPECT_EQ(subtract_math->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(subtract_math->get_vector2(), make_float3(0.5f, 0.5f, 0.0f));
}

TEST(materialx_graph, lowers_exact_minimum_and_maximum_vector2_nodes)
{
  materialx::Node a; a.name = "A"; a.nodedef = "ND_constant_vector2"; a.vector2_inputs["value"] = make_float2(-1.0f, 2.0f); a.outputs["out"] = materialx::Type::Vector2;
  materialx::Node b; b.name = "B"; b.nodedef = "ND_constant_vector2"; b.vector2_inputs["value"] = make_float2(3.0f, 1.0f); b.outputs["out"] = materialx::Type::Vector2;
  materialx::Node minimum; minimum.name = "Min"; minimum.nodedef = "ND_min_vector2"; minimum.links["in1"] = {"A", "out", materialx::Type::Vector2}; minimum.links["in2"] = {"B", "out", materialx::Type::Vector2}; minimum.outputs["out"] = materialx::Type::Vector2;
  materialx::Node maximum; maximum.name = "Max"; maximum.nodedef = "ND_max_vector2"; maximum.links["in1"] = {"Min", "out", materialx::Type::Vector2}; maximum.links["in2"] = {"B", "out", materialx::Type::Vector2}; maximum.outputs["out"] = materialx::Type::Vector2;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{a, b, minimum, maximum}}, &graph));
  bool found_min = false, found_max = false;
  for (ShaderNode *node : graph.nodes) { const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node); if (math) { found_min |= math->get_math_type() == NODE_VECTOR_MATH_MINIMUM; found_max |= math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM; } }
  EXPECT_TRUE(found_min); EXPECT_TRUE(found_max);
}

TEST(materialx_graph, lowers_exact_unary_vector2_utilities)
{
  materialx::Node input; input.name = "Input"; input.nodedef = "ND_constant_vector2"; input.vector2_inputs["value"] = make_float2(-1.25f, 2.75f); input.outputs["out"] = materialx::Type::Vector2;
  materialx::Node absval; absval.name = "Abs"; absval.nodedef = "ND_absval_vector2"; absval.links["in"] = {"Input", "out", materialx::Type::Vector2}; absval.outputs["out"] = materialx::Type::Vector2;
  materialx::Node floor; floor.name = "Floor"; floor.nodedef = "ND_floor_vector2"; floor.links["in"] = {"Abs", "out", materialx::Type::Vector2}; floor.outputs["out"] = materialx::Type::Vector2;
  materialx::Node ceil; ceil.name = "Ceil"; ceil.nodedef = "ND_ceil_vector2"; ceil.links["in"] = {"Floor", "out", materialx::Type::Vector2}; ceil.outputs["out"] = materialx::Type::Vector2;
  materialx::Node fract; fract.name = "Fract"; fract.nodedef = "ND_fract_vector2"; fract.links["in"] = {"Ceil", "out", materialx::Type::Vector2}; fract.outputs["out"] = materialx::Type::Vector2;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{input, absval, floor, ceil, fract}}, &graph));
  bool a=false,f=false,c=false,r=false; for (ShaderNode *node : graph.nodes) { const VectorMathNode *m=dynamic_cast<VectorMathNode *>(node); if(m){a|=m->get_math_type()==NODE_VECTOR_MATH_ABSOLUTE;f|=m->get_math_type()==NODE_VECTOR_MATH_FLOOR;c|=m->get_math_type()==NODE_VECTOR_MATH_CEIL;r|=m->get_math_type()==NODE_VECTOR_MATH_FRACTION;}} EXPECT_TRUE(a); EXPECT_TRUE(f); EXPECT_TRUE(c); EXPECT_TRUE(r);
}

TEST(materialx_graph, lowers_exact_trigonometric_and_sign_vector2_nodes)
{
  materialx::Node input; input.name="Input"; input.nodedef="ND_constant_vector2"; input.vector2_inputs["value"]=make_float2(0.25f, -0.5f); input.outputs["out"]=materialx::Type::Vector2;
  materialx::Node sine; sine.name="Sine"; sine.nodedef="ND_sin_vector2"; sine.links["in"]={"Input","out",materialx::Type::Vector2}; sine.outputs["out"]=materialx::Type::Vector2;
  materialx::Node cosine; cosine.name="Cosine"; cosine.nodedef="ND_cos_vector2"; cosine.links["in"]={"Sine","out",materialx::Type::Vector2}; cosine.outputs["out"]=materialx::Type::Vector2;
  materialx::Node tangent; tangent.name="Tangent"; tangent.nodedef="ND_tan_vector2"; tangent.links["in"]={"Cosine","out",materialx::Type::Vector2}; tangent.outputs["out"]=materialx::Type::Vector2;
  materialx::Node sign; sign.name="Sign"; sign.nodedef="ND_sign_vector2"; sign.vector2_inputs["in"]=make_float2(0.0f,-0.0f); sign.outputs["out"]=materialx::Type::Vector2;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{input,sine,cosine,tangent,sign}},&graph)); bool s=false,c=false,t=false,g=false; for(ShaderNode *n:graph.nodes){const VectorMathNode *m=dynamic_cast<VectorMathNode *>(n);if(m){s|=m->get_math_type()==NODE_VECTOR_MATH_SINE;c|=m->get_math_type()==NODE_VECTOR_MATH_COSINE;t|=m->get_math_type()==NODE_VECTOR_MATH_TANGENT;g|=m->get_math_type()==NODE_VECTOR_MATH_SIGN;}} EXPECT_TRUE(s);EXPECT_TRUE(c);EXPECT_TRUE(t);EXPECT_TRUE(g);
}

TEST(materialx_graph, keeps_vector2_cosine_z_zero_before_magnitude)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector2";
  input.vector2_inputs["value"] = make_float2(0.0f, 0.0f);
  input.outputs["out"] = materialx::Type::Vector2;
  materialx::Node cosine;
  cosine.name = "Cosine";
  cosine.nodedef = "ND_cos_vector2";
  cosine.links["in"] = {"Input", "out", materialx::Type::Vector2};
  cosine.outputs["out"] = materialx::Type::Vector2;
  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector2";
  magnitude.links["in"] = {"Cosine", "out", materialx::Type::Vector2};
  magnitude.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, cosine, magnitude}}, &graph));
  CombineXYZNode *mask = nullptr;
  VectorMathNode *magnitude_math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    mask = node->name == "Cosine.vector2" ? dynamic_cast<CombineXYZNode *>(node) : mask;
    magnitude_math = node->name == "Magnitude" ? dynamic_cast<VectorMathNode *>(node) : magnitude_math;
  }
  ASSERT_NE(mask, nullptr);
  ASSERT_NE(magnitude_math, nullptr);
  ASSERT_NE(magnitude_math->input("Vector1")->link, nullptr);
  EXPECT_EQ(magnitude_math->input("Vector1")->link->parent, mask);
}

TEST(materialx_graph, keeps_supported_vector2_math_chains_dimension_safe)
{
  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 0.5f;
  scalar.outputs["out"] = materialx::Type::Float;
  materialx::Node combine;
  combine.name = "Combine";
  combine.nodedef = "ND_combine2_vector2";
  combine.links["in1"] = {"Scalar", "out", materialx::Type::Float};
  combine.inputs["in2"] = 0.25f;
  combine.outputs["out"] = materialx::Type::Vector2;
  materialx::Node cosine;
  cosine.name = "Cosine";
  cosine.nodedef = "ND_cos_vector2";
  cosine.links["in"] = {"Combine", "out", materialx::Type::Vector2};
  cosine.outputs["out"] = materialx::Type::Vector2;
  materialx::Node scale;
  scale.name = "Scale";
  scale.nodedef = "ND_multiply_vector2FA";
  scale.links["in1"] = {"Cosine", "out", materialx::Type::Vector2};
  scale.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  scale.outputs["out"] = materialx::Type::Vector2;
  materialx::Node offset;
  offset.name = "Offset";
  offset.nodedef = "ND_constant_vector2";
  offset.vector2_inputs["value"] = make_float2(1.0f, -1.0f);
  offset.outputs["out"] = materialx::Type::Vector2;
  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector2";
  add.links["in1"] = {"Scale", "out", materialx::Type::Vector2};
  add.links["in2"] = {"Offset", "out", materialx::Type::Vector2};
  add.outputs["out"] = materialx::Type::Vector2;
  materialx::Node normalize;
  normalize.name = "Normalize";
  normalize.nodedef = "ND_normalize_vector2";
  normalize.links["in"] = {"Add", "out", materialx::Type::Vector2};
  normalize.outputs["out"] = materialx::Type::Vector2;
  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector2";
  magnitude.links["in"] = {"Normalize", "out", materialx::Type::Vector2};
  magnitude.outputs["out"] = materialx::Type::Float;
  materialx::Node dot;
  dot.name = "Dot";
  dot.nodedef = "ND_dotproduct_vector2";
  dot.links["in1"] = {"Normalize", "out", materialx::Type::Vector2};
  dot.links["in2"] = {"Combine", "out", materialx::Type::Vector2};
  dot.outputs["out"] = materialx::Type::Float;
  materialx::Node distance;
  distance.name = "Distance";
  distance.nodedef = "ND_distance_vector2";
  distance.links["in1"] = {"Normalize", "out", materialx::Type::Vector2};
  distance.links["in2"] = {"Combine", "out", materialx::Type::Vector2};
  distance.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{scalar, combine, cosine, scale, offset, add, normalize, magnitude, dot, distance}}, &graph));
  std::unordered_map<string, CombineXYZNode *> masks;
  std::unordered_map<string, VectorMathNode *> consumers;
  for (ShaderNode *node : graph.nodes) {
    if (const auto *math = dynamic_cast<VectorMathNode *>(node)) {
      consumers.emplace(node->name, const_cast<VectorMathNode *>(math));
    }
    if (const auto *mask = dynamic_cast<CombineXYZNode *>(node)) {
      masks.emplace(node->name, const_cast<CombineXYZNode *>(mask));
    }
  }
  for (const char *name : {"Cosine", "Scale", "Add", "Normalize"}) {
    ASSERT_NE(masks[name + string(".vector2")], nullptr) << name;
  }
  for (const char *name : {"Magnitude", "Dot", "Distance"}) {
    ASSERT_NE(consumers[name], nullptr) << name;
    ASSERT_NE(consumers[name]->input("Vector1")->link, nullptr) << name;
    ShaderNode *parent = consumers[name]->input("Vector1")->link->parent;
    EXPECT_NE(dynamic_cast<CombineXYZNode *>(parent), nullptr) << name;
    EXPECT_NE(parent->name.find(".vector2"), string::npos) << name;
  }
}

TEST(materialx_graph, lowers_exact_vector2_normalize_to_vector_output)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector2";
  input.vector2_inputs["value"] = make_float2(3.0f, 4.0f);
  input.outputs["out"] = materialx::Type::Vector2;

  materialx::Node normalize;
  normalize.name = "Normalize";
  normalize.nodedef = "ND_normalize_vector2";
  normalize.links["in"] = {"Input", "out", materialx::Type::Vector2};
  normalize.outputs["out"] = materialx::Type::Vector2;

  materialx::Node extract;
  extract.name = "Extract";
  extract.nodedef = "ND_extract_vector2";
  extract.int_inputs["index"] = 0;
  extract.links["in"] = {"Normalize", "out", materialx::Type::Vector2};
  extract.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, normalize, extract}}, &graph));

  VectorMathNode *math = nullptr;
  SeparateXYZNode *separate = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = node->name == "Normalize" ? dynamic_cast<VectorMathNode *>(node) : math;
    separate = node->name == "Extract" ? dynamic_cast<SeparateXYZNode *>(node) : separate;
  }
  ASSERT_NE(math, nullptr);
  ASSERT_NE(separate, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  ASSERT_NE(math->input("Vector1")->link, nullptr);
  ASSERT_NE(separate->input("Vector")->link, nullptr);
}

TEST(materialx_graph, lowers_standard_binary_float_literals_to_native_math)
{
  struct MathCase {
    const char *nodedef;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"ND_add_float", NODE_MATH_ADD},
                            {"ND_subtract_float", NODE_MATH_SUBTRACT},
                            {"ND_divide_float", NODE_MATH_DIVIDE}};

  for (const MathCase &math_case : cases) {
    materialx::Node source_node;
    source_node.name = math_case.nodedef;
    source_node.nodedef = math_case.nodedef;
    source_node.inputs = {{"in1", 0.75f}, {"in2", 0.25f}};
    source_node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source_node}}, &graph)) << math_case.nodedef;

    MathNode *math = nullptr;
    for (ShaderNode *node : graph.nodes) {
      math = math ? math : dynamic_cast<MathNode *>(node);
    }
    ASSERT_NE(math, nullptr) << math_case.nodedef;
    EXPECT_EQ(math->get_math_type(), math_case.math_type) << math_case.nodedef;
    EXPECT_FLOAT_EQ(math->get_value1(), 0.75f);
    EXPECT_FLOAT_EQ(math->get_value2(), 0.25f);
  }
}

TEST(materialx_graph, lowers_exact_trigonometric_and_exponential_float_nodes)
{
  struct MathCase {
    const char *nodedef;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"ND_sin_float", NODE_MATH_SINE},
                            {"ND_cos_float", NODE_MATH_COSINE},
                            {"ND_tan_float", NODE_MATH_TANGENT},
                            {"ND_exp_float", NODE_MATH_EXPONENT}};

  for (const MathCase &math_case : cases) {
    materialx::Node source_node;
    source_node.name = math_case.nodedef;
    source_node.nodedef = math_case.nodedef;
    source_node.inputs["in"] = 0.25f;
    source_node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source_node}}, &graph)) << math_case.nodedef;

    MathNode *math = nullptr;
    for (ShaderNode *node : graph.nodes) {
      math = math ? math : dynamic_cast<MathNode *>(node);
    }
    ASSERT_NE(math, nullptr) << math_case.nodedef;
    EXPECT_EQ(math->get_math_type(), math_case.math_type) << math_case.nodedef;
    EXPECT_FLOAT_EQ(math->get_value1(), 0.25f);
  }
}

TEST(materialx_graph, rejects_domain_sensitive_scalar_math_before_mutating_destination)
{
  for (const char *nodedef : {"ND_sqrt_float",
                               "ND_ln_float",
                               "ND_asin_float",
                               "ND_acos_float",
                               "ND_round_float"})
  {
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.inputs["in"] = 0.5f;
    node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower({{node}}, &graph)) << nodedef;
    EXPECT_EQ(graph.nodes.size(), original_node_count) << nodedef;
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link) << nodedef;
  }
}

TEST(materialx_graph, lowers_nested_exact_fraction_sign_minimum_and_maximum_float_links)
{
  const auto constant = [](const char *name, const float value) {
    materialx::Node node;
    node.name = name;
    node.nodedef = "ND_constant_float";
    node.inputs["value"] = value;
    node.outputs["out"] = materialx::Type::Float;
    return node;
  };
  const auto unary = [](const char *name, const char *nodedef, const char *input) {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.links["in"] = {input, "out", materialx::Type::Float};
    node.outputs["out"] = materialx::Type::Float;
    return node;
  };
  const auto binary = [](const char *name, const char *nodedef, const char *first, const char *second) {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.links["in1"] = {first, "out", materialx::Type::Float};
    node.links["in2"] = {second, "out", materialx::Type::Float};
    node.outputs["out"] = materialx::Type::Float;
    return node;
  };

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Maximum", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {constant("Input", -1.25f),
                  constant("Limit", 0.75f),
                  unary("Fraction", "ND_fract_float", "Input"),
                  unary("Sign", "ND_sign_float", "Fraction"),
                  binary("Minimum", "ND_min_float", "Fraction", "Limit"),
                  binary("Maximum", "ND_max_float", "Minimum", "Sign"),
                  surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  MathNode *fraction = nullptr;
  MathNode *sign = nullptr;
  MathNode *minimum = nullptr;
  MathNode *maximum = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Fraction") fraction = dynamic_cast<MathNode *>(node);
    if (node->name == "Sign") sign = dynamic_cast<MathNode *>(node);
    if (node->name == "Minimum") minimum = dynamic_cast<MathNode *>(node);
    if (node->name == "Maximum") maximum = dynamic_cast<MathNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(fraction, nullptr);
  ASSERT_NE(sign, nullptr);
  ASSERT_NE(minimum, nullptr);
  ASSERT_NE(maximum, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(fraction->get_math_type(), NODE_MATH_FRACTION);
  EXPECT_EQ(sign->get_math_type(), NODE_MATH_SIGN);
  EXPECT_EQ(minimum->get_math_type(), NODE_MATH_MINIMUM);
  EXPECT_EQ(maximum->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_EQ(sign->input("Value1")->link, fraction->output("Value"));
  EXPECT_EQ(minimum->input("Value1")->link, fraction->output("Value"));
  EXPECT_EQ(maximum->input("Value1")->link, minimum->output("Value"));
  EXPECT_EQ(principled->input("Roughness")->link, maximum->output("Value"));
}

TEST(materialx_graph, lowers_nested_standard_binary_float_links)
{
  const auto constant = [](const char *name, const float value) {
    materialx::Node node;
    node.name = name;
    node.nodedef = "ND_constant_float";
    node.inputs["value"] = value;
    node.outputs["out"] = materialx::Type::Float;
    return node;
  };
  const auto math =
      [](const char *name, const char *nodedef, const char *first, const char *second) {
        materialx::Node node;
        node.name = name;
        node.nodedef = nodedef;
        node.links["in1"] = {first, "out", materialx::Type::Float};
        node.links["in2"] = {second, "out", materialx::Type::Float};
        node.outputs["out"] = materialx::Type::Float;
        return node;
      };

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Multiply", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {constant("First", 0.5f),
                  constant("Second", 0.25f),
                  constant("Third", 0.1f),
                  constant("Fourth", 2.0f),
                  math("Add", "ND_add_float", "First", "Second"),
                  math("Subtract", "ND_subtract_float", "Add", "Third"),
                  math("Multiply", "ND_multiply_float", "Subtract", "Fourth"),
                  surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  MathNode *add = nullptr;
  MathNode *subtract = nullptr;
  MathNode *multiply = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Add") {
      add = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "Subtract") {
      subtract = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "Multiply") {
      multiply = dynamic_cast<MathNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(add, nullptr);
  ASSERT_NE(subtract, nullptr);
  ASSERT_NE(multiply, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(add->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(subtract->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(multiply->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(subtract->input("Value1")->link, add->output("Value"));
  EXPECT_EQ(multiply->input("Value1")->link, subtract->output("Value"));
  EXPECT_EQ(principled->input("Roughness")->link, multiply->output("Value"));
}

TEST(materialx_graph, rejects_invalid_divide_float_structure_before_mutation)
{
  materialx::Node divide;
  divide.name = "Divide";
  divide.nodedef = "ND_divide_float";
  divide.inputs["in1"] = 1.0f;
  divide.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{divide}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);

  materialx::Node constant;
  constant.name = "Constant";
  constant.nodedef = "ND_constant_float";
  constant.inputs["value"] = 2.0f;
  constant.outputs["out"] = materialx::Type::Float;
  divide.inputs["in2"] = 0.5f;
  divide.links["in2"] = {"Constant", "out", materialx::Type::Float};
  EXPECT_FALSE(materialx::lower({{constant, divide}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_graph, rejects_zero_or_dynamic_division_denominators_before_mutation)
{
  const auto expect_rejected = [](materialx::Graph source) {
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower(source, &graph));
    EXPECT_EQ(graph.nodes.size(), original_node_count);
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
  };
  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_divide_float";
  scalar.inputs["in1"] = 1.0f;
  scalar.inputs["in2"] = 0.0f;
  scalar.outputs["out"] = materialx::Type::Float;
  expect_rejected({{scalar}});
  materialx::Node denominator;
  denominator.name = "Denominator";
  denominator.nodedef = "ND_constant_float";
  denominator.inputs["value"] = 2.0f;
  denominator.outputs["out"] = materialx::Type::Float;
  scalar.inputs.erase("in2");
  scalar.links["in2"] = {"Denominator", "out", materialx::Type::Float};
  expect_rejected({{denominator, scalar}});
  materialx::Node vector;
  vector.name = "Vector";
  vector.nodedef = "ND_divide_vector2";
  vector.vector2_inputs["in1"] = make_float2(1.0f, 2.0f);
  vector.vector2_inputs["in2"] = make_float2(1.0f, 0.0f);
  vector.outputs["out"] = materialx::Type::Vector2;
  expect_rejected({{vector}});
}

TEST(materialx_graph, lowers_vector3_float_add_and_subtract_into_normalmap)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector3";
  input.vector3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  input.outputs["out"] = materialx::Type::Vector3;
  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector3FA";
  add.links["in1"] = {"Input", "out", materialx::Type::Vector3};
  add.inputs["in2"] = 0.5f;
  add.outputs["out"] = materialx::Type::Vector3;
  materialx::Node subtract;
  subtract.name = "Subtract";
  subtract.nodedef = "ND_subtract_vector3FA";
  subtract.links["in1"] = {"Add", "out", materialx::Type::Vector3};
  subtract.inputs["in2"] = 0.25f;
  subtract.outputs["out"] = materialx::Type::Vector3;
  materialx::Node normalmap;
  normalmap.name = "NormalMap";
  normalmap.nodedef = "ND_normalmap_float";
  normalmap.links["in"] = {"Subtract", "out", materialx::Type::Vector3};
  normalmap.outputs["out"] = materialx::Type::Vector3;
  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, add, subtract, normalmap}}, &graph));
  NormalMapNode *native_normalmap = nullptr;
  for (ShaderNode *node : graph.nodes) native_normalmap = node->name == "NormalMap" ? dynamic_cast<NormalMapNode *>(node) : native_normalmap;
  ASSERT_NE(native_normalmap, nullptr);
  ASSERT_NE(native_normalmap->input("Color")->link, nullptr);
}

TEST(materialx_graph, rejects_cyclic_scalar_graph_before_mutation)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_divide_float";
  first.links["in1"] = {"Second", "out", materialx::Type::Float};
  first.inputs["in2"] = 2.0f;
  first.outputs["out"] = materialx::Type::Float;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_add_float";
  second.links["in1"] = {"First", "out", materialx::Type::Float};
  second.inputs["in2"] = 1.0f;
  second.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{first, second}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_graph, lowers_normalmap_float_to_open_pbr_normal_inputs)
{
  materialx::Node normalmap;
  normalmap.name = "NormalMap";
  normalmap.nodedef = "ND_normalmap_float";
  normalmap.vector3_inputs["in"] = make_float3(0.25f, 0.75f, 1.0f);
  /* Omitted scale is MaterialX's unit-strength default. */
  normalmap.outputs["out"] = materialx::Type::Vector3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_normal"] = {"NormalMap", "out", materialx::Type::Vector3};
  surface.links["geometry_coat_normal"] = {"NormalMap", "out", materialx::Type::Vector3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{normalmap, surface}}, &graph));

  NormalMapNode *native_normalmap = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_normalmap = native_normalmap ? native_normalmap : dynamic_cast<NormalMapNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(native_normalmap, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(native_normalmap->get_space(), NODE_NORMAL_MAP_TANGENT);
  EXPECT_EQ(native_normalmap->get_convention(), NODE_NORMAL_MAP_CONVENTION_OPENGL);
  EXPECT_EQ(native_normalmap->get_base(), NODE_NORMAL_MAP_BASE_DISPLACED);
  EXPECT_FLOAT_EQ(native_normalmap->get_strength(), 1.0f);
  EXPECT_EQ(native_normalmap->get_color(), make_float3(0.25f, 0.75f, 1.0f));
  EXPECT_EQ(principled->input("Normal")->link, native_normalmap->output("Normal"));
  EXPECT_EQ(principled->input("Coat Normal")->link, native_normalmap->output("Normal"));
}

TEST(materialx_graph, lowers_vector_constant_and_normalize_into_normalmap)
{
  materialx::Node constant;
  constant.name = "NormalValue";
  constant.nodedef = "ND_constant_vector3";
  constant.vector3_inputs["value"] = make_float3(0.25f, 0.75f, 1.0f);
  constant.outputs["out"] = materialx::Type::Vector3;
  materialx::Node normalize;
  normalize.name = "Normalize";
  normalize.nodedef = "ND_normalize_vector3";
  normalize.links["in"] = {"NormalValue", "out", materialx::Type::Vector3};
  normalize.outputs["out"] = materialx::Type::Vector3;
  materialx::Node normalmap;
  normalmap.name = "NormalMap";
  normalmap.nodedef = "ND_normalmap_float";
  normalmap.links["in"] = {"Normalize", "out", materialx::Type::Vector3};
  normalmap.outputs["out"] = materialx::Type::Vector3;
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_normal"] = {"NormalMap", "out", materialx::Type::Vector3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;
  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant, normalize, normalmap, surface}}, &graph));
  NormalMapNode *native_normalmap = nullptr;
  VectorMathNode *native_normalize = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_normalmap = native_normalmap ? native_normalmap : dynamic_cast<NormalMapNode *>(node);
    native_normalize = native_normalize ? native_normalize : dynamic_cast<VectorMathNode *>(node);
  }
  ASSERT_NE(native_normalmap, nullptr);
  ASSERT_NE(native_normalize, nullptr);
  EXPECT_EQ(native_normalize->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_NE(native_normalmap->input("Color")->link, nullptr);
  EXPECT_NE(native_normalize->input("Vector1")->link, nullptr);
}

TEST(materialx_graph, lowers_crossproduct_vector3_to_native_cross_product)
{
  materialx::Node crossproduct;
  crossproduct.name = "Cross";
  crossproduct.nodedef = "ND_crossproduct_vector3";
  crossproduct.vector3_inputs["in1"] = make_float3(1.0f, 0.0f, 0.0f);
  crossproduct.vector3_inputs["in2"] = make_float3(0.0f, 1.0f, 0.0f);
  crossproduct.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{crossproduct}}, &graph));

  VectorMathNode *native_crossproduct = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_crossproduct = native_crossproduct ? native_crossproduct :
                                                dynamic_cast<VectorMathNode *>(node);
  }
  ASSERT_NE(native_crossproduct, nullptr);
  EXPECT_EQ(native_crossproduct->get_math_type(), NODE_VECTOR_MATH_CROSS_PRODUCT);
  EXPECT_EQ(native_crossproduct->get_vector1(), make_float3(1.0f, 0.0f, 0.0f));
  EXPECT_EQ(native_crossproduct->get_vector2(), make_float3(0.0f, 1.0f, 0.0f));
}

TEST(materialx_graph, rejects_explicit_normalmap_basis_before_mutation)
{
  materialx::Node normalmap;
  normalmap.name = "NormalMap";
  normalmap.nodedef = "ND_normalmap_float";
  normalmap.vector3_inputs["in"] = make_float3(0.5f, 0.5f, 1.0f);
  normalmap.vector3_inputs["normal"] = make_float3(0.0f, 0.0f, 1.0f);
  normalmap.inputs["scale"] = 1.0f;
  normalmap.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{normalmap}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);

  normalmap.vector3_inputs.erase("normal");
  normalmap.inputs["scale"] = 0.5f;
  EXPECT_FALSE(materialx::lower({{normalmap}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);

  normalmap.inputs.erase("scale");
  normalmap.links["scale"] = {"Scale", "out", materialx::Type::Float};
  EXPECT_FALSE(materialx::lower({{normalmap}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_graph, lowers_nworld_geomprop_to_open_pbr_normal)
{
  materialx::Node geomprop;
  geomprop.name = "WorldNormal";
  geomprop.nodedef = "ND_geompropvalue_vector3";
  geomprop.string_inputs["geomprop"] = "Nworld";
  geomprop.outputs["out"] = materialx::Type::Vector3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_normal"] = {"WorldNormal", "out", materialx::Type::Vector3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{geomprop, surface}}, &graph));

  GeometryNode *geometry = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    geometry = geometry ? geometry : dynamic_cast<GeometryNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(geometry, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->input("Normal")->link, geometry->output("Normal"));

  geomprop.string_inputs["geomprop"] = "customNormal";
  ShaderGraph invalid_graph;
  EXPECT_FALSE(materialx::lower({{geomprop, surface}}, &invalid_graph));
}

TEST(materialx_graph, lowers_linked_constant_color3_to_open_pbr_base_color)
{
  materialx::Node constant;
  constant.name = "BaseColor";
  constant.nodedef = "ND_constant_color3";
  constant.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  constant.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"BaseColor", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {constant, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  ColorNode *color = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color = color ? color : dynamic_cast<ColorNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(color, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(color->get_value().x, 0.2f);
  EXPECT_FLOAT_EQ(color->get_value().y, 0.4f);
  EXPECT_FLOAT_EQ(color->get_value().z, 0.6f);
  EXPECT_EQ(principled->input("Base Color")->link, color->output("Color"));
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_open_pbr_base_color_literal)
{
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.color3_inputs["base_color"] = make_float3(0.02f, 0.8f, 0.08f);
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(principled->get_base_color().x, 0.02f);
  EXPECT_FLOAT_EQ(principled->get_base_color().y, 0.8f);
  EXPECT_FLOAT_EQ(principled->get_base_color().z, 0.08f);
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_typed_uv_image_chain_to_open_pbr_base_color)
{
  const TemporaryImage image_asset;

  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node image;
  image.name = "BaseColorImage";
  image.nodedef = "ND_image_color3";
  image.asset_inputs["file"] = image_asset.path();
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"BaseColorImage", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {uv, image, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  UVMapNode *uv_map = nullptr;
  ImageTextureNode *image_texture = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    uv_map = uv_map ? uv_map : dynamic_cast<UVMapNode *>(node);
    image_texture = image_texture ? image_texture : dynamic_cast<ImageTextureNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(uv_map, nullptr);
  ASSERT_NE(image_texture, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(uv_map->get_attribute(), ustring("st"));
  EXPECT_EQ(image_texture->get_filename(), ustring(image_asset.path()));
  EXPECT_EQ(image_texture->input("Vector")->link, uv_map->output("UV"));
  EXPECT_EQ(principled->input("Base Color")->link, image_texture->output("Color"));
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, rejects_missing_image_asset_before_mutating_destination)
{
  const string missing_path =
      (std::filesystem::temp_directory_path() / "cycles_materialx_missing_image.ppm").string();
  std::error_code error;
  std::filesystem::remove(missing_path, error);

  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node image;
  image.name = "BaseColorImage";
  image.nodedef = "ND_image_color3";
  image.asset_inputs["file"] = missing_path;
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"BaseColorImage", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {uv, image, surface};

  ShaderGraph graph;
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower(source, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_linked_multiply_float_to_open_pbr_roughness)
{
  materialx::Node multiply;
  multiply.name = "RoughnessMultiply";
  multiply.nodedef = "ND_multiply_float";
  multiply.inputs = {{"in1", 0.7f}, {"in2", 0.2f}};
  multiply.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"RoughnessMultiply", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {multiply, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  MathNode *math = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    math = math ? math : dynamic_cast<MathNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(math, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->input("Roughness")->link, math->output("Value"));
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_open_pbr_opacity_and_emission_literals)
{
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.inputs["geometry_opacity"] = 0.35f;
  surface.color3_inputs["emission_color"] = make_float3(0.25f, 0.5f, 1.0f);
  surface.inputs["emission_luminance"] = 3.0f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(principled->get_alpha(), 0.35f);
  EXPECT_FLOAT_EQ(principled->get_emission_color().x, 0.25f);
  EXPECT_FLOAT_EQ(principled->get_emission_color().y, 0.5f);
  EXPECT_FLOAT_EQ(principled->get_emission_color().z, 1.0f);
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 3.0f);
  EXPECT_TRUE(principled->has_surface_transparent());
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_direct_open_pbr_coat_and_fuzz_literals_and_links)
{
  materialx::Node fuzz_weight;
  fuzz_weight.name = "FuzzWeight";
  fuzz_weight.nodedef = "ND_constant_float";
  fuzz_weight.inputs["value"] = 0.6f;
  fuzz_weight.outputs["out"] = materialx::Type::Float;

  materialx::Node fuzz_color;
  fuzz_color.name = "FuzzColor";
  fuzz_color.nodedef = "ND_constant_color3";
  fuzz_color.color3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  fuzz_color.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.inputs = {{"coat_weight", 0.4f},
                    {"coat_roughness", 0.2f},
                    {"coat_ior", 1.45f},
                    {"fuzz_roughness", 0.35f}};
  surface.color3_inputs["coat_color"] = make_float3(0.8f, 0.6f, 0.4f);
  surface.links["fuzz_weight"] = {"FuzzWeight", "out", materialx::Type::Float};
  surface.links["fuzz_color"] = {"FuzzColor", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{fuzz_weight, fuzz_color, surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  ValueNode *native_fuzz_weight = nullptr;
  ColorNode *native_fuzz_color = nullptr;
  for (ShaderNode *node : graph.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
    native_fuzz_weight = native_fuzz_weight ? native_fuzz_weight : dynamic_cast<ValueNode *>(node);
    native_fuzz_color = native_fuzz_color ? native_fuzz_color : dynamic_cast<ColorNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  ASSERT_NE(native_fuzz_weight, nullptr);
  ASSERT_NE(native_fuzz_color, nullptr);
  EXPECT_FLOAT_EQ(principled->get_coat_weight(), 0.4f);
  EXPECT_FLOAT_EQ(principled->get_coat_roughness(), 0.2f);
  EXPECT_FLOAT_EQ(principled->get_coat_ior(), 1.45f);
  EXPECT_EQ(principled->get_coat_tint(), make_float3(0.8f, 0.6f, 0.4f));
  EXPECT_FLOAT_EQ(principled->get_sheen_roughness(), 0.35f);
  EXPECT_EQ(principled->input("Sheen Weight")->link, native_fuzz_weight->output("Value"));
  EXPECT_EQ(principled->input("Sheen Tint")->link, native_fuzz_color->output("Color"));
}

TEST(materialx_graph, rejects_non_equivalent_open_pbr_coat_semantics_before_mutation)
{
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.inputs["coat_weight"] = 0.5f;
  surface.inputs["coat_darkening"] = 0.5f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{surface}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_graph, lowers_linked_opacity_and_emission_to_native_principled_inputs)
{
  materialx::Node opacity;
  opacity.name = "Opacity";
  opacity.nodedef = "ND_constant_float";
  opacity.inputs["value"] = 0.4f;
  opacity.outputs["out"] = materialx::Type::Float;

  materialx::Node emission_color;
  emission_color.name = "EmissionColor";
  emission_color.nodedef = "ND_constant_color3";
  emission_color.color3_inputs["value"] = make_float3(1.0f, 0.5f, 0.25f);
  emission_color.outputs["out"] = materialx::Type::Color3;

  materialx::Node emission_strength;
  emission_strength.name = "EmissionStrength";
  emission_strength.nodedef = "ND_multiply_float";
  emission_strength.inputs = {{"in1", 2.0f}, {"in2", 3.0f}};
  emission_strength.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_opacity"] = {"Opacity", "out", materialx::Type::Float};
  surface.links["emission_color"] = {"EmissionColor", "out", materialx::Type::Color3};
  surface.links["emission_luminance"] = {"EmissionStrength", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {opacity, emission_color, emission_strength, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  ValueNode *value = nullptr;
  ColorNode *color = nullptr;
  MathNode *math = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    value = value ? value : dynamic_cast<ValueNode *>(node);
    color = color ? color : dynamic_cast<ColorNode *>(node);
    math = math ? math : dynamic_cast<MathNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(value, nullptr);
  ASSERT_NE(color, nullptr);
  ASSERT_NE(math, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(value->get_value(), 0.4f);
  EXPECT_EQ(principled->input("Alpha")->link, value->output("Value"));
  EXPECT_EQ(principled->input("Emission Color")->link, color->output("Color"));
  EXPECT_EQ(principled->input("Emission Strength")->link, math->output("Value"));
  EXPECT_TRUE(principled->has_surface_transparent());
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_open_pbr_primary_scalar_literals)
{
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.inputs["base_weight"] = 0.8f;
  surface.inputs["base_metalness"] = 0.2f;
  surface.inputs["specular_ior"] = 1.45f;
  surface.inputs["specular_roughness"] = 0.35f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(principled->get_surface_mix_weight(), -0.2f);
  EXPECT_FLOAT_EQ(principled->get_metallic(), 0.2f);
  EXPECT_FLOAT_EQ(principled->get_ior(), 1.45f);
  EXPECT_FLOAT_EQ(principled->get_roughness(), 0.35f);
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_linked_primary_scalars_to_native_principled_inputs)
{
  materialx::Node weight;
  weight.name = "Weight";
  weight.nodedef = "ND_constant_float";
  weight.inputs["value"] = 0.8f;
  weight.outputs["out"] = materialx::Type::Float;

  materialx::Node metalness;
  metalness.name = "Metalness";
  metalness.nodedef = "ND_multiply_float";
  metalness.inputs = {{"in1", 0.4f}, {"in2", 0.5f}};
  metalness.outputs["out"] = materialx::Type::Float;

  materialx::Node ior;
  ior.name = "IOR";
  ior.nodedef = "ND_constant_float";
  ior.inputs["value"] = 1.45f;
  ior.outputs["out"] = materialx::Type::Float;

  materialx::Node roughness;
  roughness.name = "Roughness";
  roughness.nodedef = "ND_multiply_float";
  roughness.inputs = {{"in1", 0.7f}, {"in2", 0.5f}};
  roughness.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_weight"] = {"Weight", "out", materialx::Type::Float};
  surface.links["base_metalness"] = {"Metalness", "out", materialx::Type::Float};
  surface.links["specular_ior"] = {"IOR", "out", materialx::Type::Float};
  surface.links["specular_roughness"] = {"Roughness", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {weight, metalness, ior, roughness, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  ValueNode *weight_value = nullptr;
  ValueNode *ior_value = nullptr;
  MathNode *metalness_math = nullptr;
  MathNode *roughness_math = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Weight") {
      weight_value = dynamic_cast<ValueNode *>(node);
    }
    else if (node->name == "IOR") {
      ior_value = dynamic_cast<ValueNode *>(node);
    }
    else if (node->name == "Metalness") {
      metalness_math = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "Roughness") {
      roughness_math = dynamic_cast<MathNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(weight_value, nullptr);
  ASSERT_NE(ior_value, nullptr);
  ASSERT_NE(metalness_math, nullptr);
  ASSERT_NE(roughness_math, nullptr);
  ASSERT_NE(principled, nullptr);

  ASSERT_NE(principled->input("SurfaceMixWeight")->link, nullptr);
  MathNode *weight_delta = dynamic_cast<MathNode *>(
      principled->input("SurfaceMixWeight")->link->parent);
  ASSERT_NE(weight_delta, nullptr);
  EXPECT_EQ(weight_delta->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(weight_delta->input("Value1")->link, weight_value->output("Value"));
  EXPECT_FLOAT_EQ(weight_delta->get_value2(), 1.0f);
  EXPECT_EQ(principled->input("Metallic")->link, metalness_math->output("Value"));
  EXPECT_EQ(principled->input("IOR")->link, ior_value->output("Value"));
  EXPECT_EQ(principled->input("Roughness")->link, roughness_math->output("Value"));
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_standard_binary_color3_nodes_to_native_mix_nodes)
{
  struct ColorMathCase {
    const char *nodedef;
    NodeMix mix_type;
  };
  const ColorMathCase cases[] = {{"ND_add_color3", NODE_MIX_ADD},
                                 {"ND_subtract_color3", NODE_MIX_SUB},
                                 {"ND_multiply_color3", NODE_MIX_MUL},
                                 {"ND_divide_color3", NODE_MIX_DIV}};

  for (const ColorMathCase &test_case : cases) {
    materialx::Node first;
    first.name = "First";
    first.nodedef = "ND_constant_color3";
    first.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
    first.outputs["out"] = materialx::Type::Color3;

    materialx::Node second;
    second.name = "Second";
    second.nodedef = "ND_constant_color3";
    second.color3_inputs["value"] = make_float3(0.5f, 0.25f, 0.8f);
    second.outputs["out"] = materialx::Type::Color3;

    materialx::Node color_math;
    color_math.name = "ColorMath";
    color_math.nodedef = test_case.nodedef;
    color_math.links["in1"] = {"First", "out", materialx::Type::Color3};
    color_math.links["in2"] = {"Second", "out", materialx::Type::Color3};
    color_math.outputs["out"] = materialx::Type::Color3;

    materialx::Node surface;
    surface.name = "OpenPBR";
    surface.nodedef = "ND_open_pbr_surface_surfaceshader";
    surface.links["base_color"] = {"ColorMath", "out", materialx::Type::Color3};
    surface.outputs["out"] = materialx::Type::SurfaceShader;

    materialx::Graph source;
    source.nodes = {first, second, color_math, surface};

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower(source, &graph)) << test_case.nodedef;

    ColorNode *first_color = nullptr;
    ColorNode *second_color = nullptr;
    MixNode *mix = nullptr;
    PrincipledBsdfNode *principled = nullptr;
    for (ShaderNode *node : graph.nodes) {
      if (node->name == "First") {
        first_color = dynamic_cast<ColorNode *>(node);
      }
      else if (node->name == "Second") {
        second_color = dynamic_cast<ColorNode *>(node);
      }
      mix = mix ? mix : dynamic_cast<MixNode *>(node);
      principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
    }
    ASSERT_NE(first_color, nullptr);
    ASSERT_NE(second_color, nullptr);
    ASSERT_NE(mix, nullptr);
    ASSERT_NE(principled, nullptr);
    EXPECT_EQ(mix->get_mix_type(), test_case.mix_type);
    EXPECT_FLOAT_EQ(mix->get_fac(), 1.0f);
    EXPECT_EQ(mix->input("Color1")->link, first_color->output("Color"));
    EXPECT_EQ(mix->input("Color2")->link, second_color->output("Color"));
    EXPECT_EQ(principled->input("Base Color")->link, mix->output("Color"));
  }
}

TEST(materialx_graph, lowers_extract_color3_to_native_separate_color)
{
  materialx::Node color;
  color.name = "PackedColor";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node extract;
  extract.name = "RoughnessChannel";
  extract.nodedef = "ND_extract_color3";
  extract.int_inputs["index"] = 1;
  extract.links["in"] = {"PackedColor", "out", materialx::Type::Color3};
  extract.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"RoughnessChannel", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {color, extract, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  ColorNode *color_node = nullptr;
  SeparateColorNode *separate = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color_node = color_node ? color_node : dynamic_cast<ColorNode *>(node);
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(color_node, nullptr);
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(separate->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(separate->input("Color")->link, color_node->output("Color"));
  EXPECT_EQ(principled->input("Roughness")->link, separate->output("Green"));
}

TEST(materialx_graph, rejects_invalid_extract_index_before_mutating_destination)
{
  materialx::Node color;
  color.name = "PackedColor";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node extract;
  extract.name = "InvalidChannel";
  extract.nodedef = "ND_extract_color3";
  extract.int_inputs["index"] = 3;
  extract.links["in"] = {"PackedColor", "out", materialx::Type::Color3};
  extract.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"InvalidChannel", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {color, extract, surface};

  ShaderGraph graph;
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower(source, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_top_to_bottom_color_ramp_to_clamped_y_coordinate)
{
  materialx::Node coordinate;
  coordinate.name = "UV";
  coordinate.nodedef = "ND_constant_vector2";
  coordinate.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  coordinate.outputs["out"] = materialx::Type::Vector2;

  materialx::Node ramp;
  ramp.name = "Ramp";
  ramp.nodedef = "ND_ramptb_color3";
  ramp.color3_inputs["valuet"] = make_float3(0.1f, 0.2f, 0.3f);
  ramp.color3_inputs["valueb"] = make_float3(0.7f, 0.8f, 0.9f);
  ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  ramp.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"Ramp", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {coordinate, ramp, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  MixNode *mix = nullptr;
  SeparateXYZNode *separate = nullptr;
  ClampNode *clamp = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Ramp") mix = dynamic_cast<MixNode *>(node);
    if (node->name == "Ramp.coordinate") separate = dynamic_cast<SeparateXYZNode *>(node);
    if (node->name == "Ramp.factor") clamp = dynamic_cast<ClampNode *>(node);
  }
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(clamp, nullptr);
  EXPECT_EQ(mix->get_mix_type(), NODE_MIX_BLEND);
  EXPECT_EQ(mix->get_color1(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(mix->get_color2(), make_float3(0.7f, 0.8f, 0.9f));
  EXPECT_EQ(clamp->get_clamp_type(), NODE_CLAMP_MINMAX);
  EXPECT_FLOAT_EQ(clamp->get_min(), 0.0f);
  EXPECT_FLOAT_EQ(clamp->get_max(), 1.0f);
  EXPECT_EQ(clamp->input("Value")->link, separate->output("Y"));
  EXPECT_EQ(mix->input("Fac")->link, clamp->output("Result"));
}

TEST(materialx_graph, lowers_scalar_ramps_to_explicit_clamped_arithmetic)
{
  materialx::Node coordinate;
  coordinate.name = "UV";
  coordinate.nodedef = "ND_constant_vector2";
  coordinate.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  coordinate.outputs["out"] = materialx::Type::Vector2;

  materialx::Node ramp;
  ramp.name = "TopToBottomRamp";
  ramp.nodedef = "ND_ramptb_float";
  ramp.inputs["valuet"] = 0.1f;
  ramp.inputs["valueb"] = 0.9f;
  ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  ramp.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"TopToBottomRamp", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {coordinate, ramp, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  SeparateXYZNode *lowered_coordinate = nullptr;
  ClampNode *clamp = nullptr;
  MathNode *delta = nullptr;
  MathNode *product = nullptr;
  MathNode *sum = nullptr;
  for (ShaderNode *node : graph.nodes) {
    lowered_coordinate = node->name == "TopToBottomRamp.coordinate" ? dynamic_cast<SeparateXYZNode *>(node) : lowered_coordinate;
    clamp = node->name == "TopToBottomRamp.factor" ? dynamic_cast<ClampNode *>(node) : clamp;
    delta = node->name == "TopToBottomRamp.delta" ? dynamic_cast<MathNode *>(node) : delta;
    product = node->name == "TopToBottomRamp.product" ? dynamic_cast<MathNode *>(node) : product;
    sum = node->name == "TopToBottomRamp" ? dynamic_cast<MathNode *>(node) : sum;
  }
  ASSERT_NE(lowered_coordinate, nullptr);
  ASSERT_NE(clamp, nullptr);
  ASSERT_NE(delta, nullptr);
  ASSERT_NE(product, nullptr);
  ASSERT_NE(sum, nullptr);
  EXPECT_EQ(delta->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(product->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(sum->get_math_type(), NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(delta->get_value1(), 0.9f);
  EXPECT_FLOAT_EQ(delta->get_value2(), 0.1f);
  EXPECT_FLOAT_EQ(sum->get_value1(), 0.1f);
  EXPECT_EQ(clamp->input("Value")->link, lowered_coordinate->output("Y"));
  EXPECT_EQ(product->input("Value2")->link, clamp->output("Result"));
  EXPECT_EQ(product->input("Value1")->link, delta->output("Value"));
  EXPECT_EQ(sum->input("Value2")->link, product->output("Value"));
}

TEST(materialx_graph, lowers_smoothstep_float_with_linked_input_to_clamped_native_range)
{
  materialx::Node source;
  source.name = "Source";
  source.nodedef = "ND_multiply_float";
  source.inputs["in1"] = 0.75f;
  source.inputs["in2"] = 1.0f;
  source.outputs["out"] = materialx::Type::Float;

  materialx::Node smoothstep;
  smoothstep.name = "Smoothstep";
  smoothstep.nodedef = "ND_smoothstep_float";
  smoothstep.inputs["low"] = 0.25f;
  smoothstep.inputs["high"] = 0.75f;
  smoothstep.links["in"] = {"Source", "out", materialx::Type::Float};
  smoothstep.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Smoothstep", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source_graph;
  source_graph.nodes = {source, smoothstep, surface};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source_graph, &graph));

  MapRangeNode *range = nullptr;
  MathNode *source_math = nullptr;
  for (ShaderNode *node : graph.nodes) {
    range = node->name == "Smoothstep" ? dynamic_cast<MapRangeNode *>(node) : range;
    source_math = node->name == "Source" ? dynamic_cast<MathNode *>(node) : source_math;
  }
  ASSERT_NE(range, nullptr);
  ASSERT_NE(source_math, nullptr);
  EXPECT_EQ(range->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FALSE(range->get_clamp());
  EXPECT_FLOAT_EQ(range->get_from_min(), 0.25f);
  EXPECT_FLOAT_EQ(range->get_from_max(), 0.75f);
  EXPECT_FLOAT_EQ(range->get_to_min(), 0.0f);
  EXPECT_FLOAT_EQ(range->get_to_max(), 1.0f);
  ASSERT_NE(range->input("Value")->link, nullptr);
  EXPECT_EQ(range->input("Value")->link->parent, source_math);
}

TEST(materialx_graph, rejects_equal_smoothstep_float_edges_before_mutating_destination)
{
  materialx::Node smoothstep;
  smoothstep.name = "Smoothstep";
  smoothstep.nodedef = "ND_smoothstep_float";
  smoothstep.inputs["in"] = 0.5f;
  smoothstep.inputs["low"] = 0.5f;
  smoothstep.inputs["high"] = 0.5f;
  smoothstep.outputs["out"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes = {smoothstep};

  ShaderGraph graph;
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower(source, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_luminance_color3_with_literal_coefficients_and_nested_color_link)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node luminance;
  luminance.name = "Luminance";
  luminance.nodedef = "ND_luminance_color3";
  luminance.color3_inputs["lumacoeffs"] = make_float3(0.2126f, 0.7152f, 0.0722f);
  luminance.links["in"] = {"Color", "out", materialx::Type::Color3};
  luminance.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Luminance", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, luminance, surface}}, &graph));

  VectorMathNode *dot = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Luminance") dot = dynamic_cast<VectorMathNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(dot, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dot->get_vector2(), make_float3(0.2126f, 0.7152f, 0.0722f));
  ASSERT_NE(dot->input("Vector1")->link, nullptr);
  EXPECT_EQ(dot->input("Vector1")->link->parent->name, "Luminance.vector");
  EXPECT_EQ(principled->input("Roughness")->link, dot->output("Value"));
}

TEST(materialx_authority, accepts_complete_canonical_usdshade_contract)
{
  EXPECT_EQ(materialx::usda_sha256_digest("abc"),
            "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  const materialx::Authority authority = {
      "9c37e82e-63a1-470d-a704-e0daf9cfd814",
      materialx::usda_sha256_digest("#usda 1.0\n\ndef Scope \"Looks\" {}\n"),
      ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "/Looks/Material",
      "#usda 1.0\n\ndef Scope \"Looks\" {}\n",
  };

  EXPECT_TRUE(materialx::is_valid(authority));
}

TEST(materialx_authority, rejects_partial_or_mismatched_contract)
{
  const materialx::Authority valid = {
      "9c37e82e-63a1-470d-a704-e0daf9cfd814",
      materialx::usda_sha256_digest("#usda 1.0\n\ndef Scope \"Looks\" {}\n"),
      ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "/Looks/Material",
      "#usda 1.0\n\ndef Scope \"Looks\" {}\n",
  };

  materialx::Authority authority = valid;
  authority.digest = "not-a-digest";
  EXPECT_FALSE(materialx::is_valid(authority));

  authority = valid;
  authority.usda_text_name = ".materialx_usdshade_other-document";
  EXPECT_FALSE(materialx::is_valid(authority));

  authority = valid;
  authority.material_path = "Looks/Material";
  EXPECT_FALSE(materialx::is_valid(authority));

  authority = valid;
  authority.usda = "<materialx />";
  EXPECT_FALSE(materialx::is_valid(authority));

  authority = valid;
  authority.usda += "# tampered\n";
  EXPECT_FALSE(materialx::is_valid(authority));
}

CCL_NAMESPACE_END
