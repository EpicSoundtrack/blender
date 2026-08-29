/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>

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

TEST(materialx_graph, lowers_native_materialx_space_transforms_to_vector_transform_nodes)
{
  struct TransformCase {
    const char *name;
    const char *nodedef;
    NodeVectorTransformType transform_type;
    const char *fromspace;
    const char *tospace;
  };
  const TransformCase cases[] = {
      {"Point", "ND_transformpoint_vector3", NODE_VECTOR_TRANSFORM_TYPE_POINT, "object", "world"},
      {"Vector", "ND_transformvector_vector3", NODE_VECTOR_TRANSFORM_TYPE_VECTOR, "world", "camera"},
      {"Normal", "ND_transformnormal_vector3", NODE_VECTOR_TRANSFORM_TYPE_NORMAL, "camera", "object"}};
  materialx::Graph source;
  for (const TransformCase &item : cases) {
    materialx::Node transform;
    transform.name = item.name;
    transform.nodedef = item.nodedef;
    transform.vector3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
    transform.string_inputs["fromspace"] = item.fromspace;
    transform.string_inputs["tospace"] = item.tospace;
    transform.outputs["out"] = materialx::Type::Vector3;
    source.nodes.push_back(std::move(transform));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, VectorTransformNode *> transforms;
  for (ShaderNode *node : graph.nodes) {
    if (node->type == VectorTransformNode::get_node_type()) {
      transforms[string(node->name.c_str())] = static_cast<VectorTransformNode *>(node);
    }
  }
  ASSERT_EQ(transforms.size(), std::size(cases));
  for (const TransformCase &item : cases) {
    ASSERT_NE(transforms[item.name], nullptr) << item.name;
    EXPECT_EQ(transforms[item.name]->get_transform_type(), item.transform_type) << item.name;
    EXPECT_EQ(transforms[item.name]->get_vector(), make_float3(0.25f, 0.5f, 0.75f)) << item.name;
  }
  EXPECT_EQ(transforms["Point"]->get_convert_from(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);
  EXPECT_EQ(transforms["Point"]->get_convert_to(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  EXPECT_EQ(transforms["Vector"]->get_convert_from(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_WORLD);
  EXPECT_EQ(transforms["Vector"]->get_convert_to(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA);
  EXPECT_EQ(transforms["Normal"]->get_convert_from(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_CAMERA);
  EXPECT_EQ(transforms["Normal"]->get_convert_to(), NODE_VECTOR_TRANSFORM_CONVERT_SPACE_OBJECT);
}

TEST(materialx_graph, rejects_malformed_native_materialx_space_transforms_without_mutation)
{
  materialx::Node transform;
  transform.name = "BadTransform";
  transform.nodedef = "ND_transformpoint_vector3";
  transform.vector3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  transform.string_inputs["fromspace"] = "world";
  transform.string_inputs["tospace"] = "model";
  transform.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{transform}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);

  transform.string_inputs["tospace"] = "object";
  transform.string_inputs.erase("fromspace");
  EXPECT_FALSE(materialx::lower({{transform}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}


TEST(materialx_graph, lowers_exact_vector_rotation_utilities_to_native_vector_rotate)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node rotate2d;
  rotate2d.name = "Rotate2D";
  rotate2d.nodedef = "ND_rotate2d_vector2";
  rotate2d.links["in"] = {"UV", "out", materialx::Type::Vector2};
  rotate2d.inputs["amount"] = 90.0f;
  rotate2d.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector;
  vector.name = "Vector";
  vector.nodedef = "ND_constant_vector3";
  vector.vector3_inputs["value"] = make_float3(1.0f, 0.0f, 0.0f);
  vector.outputs["out"] = materialx::Type::Vector3;

  materialx::Node angle;
  angle.name = "Angle";
  angle.nodedef = "ND_constant_float";
  angle.inputs["value"] = 180.0f;
  angle.outputs["out"] = materialx::Type::Float;

  materialx::Node rotate3d;
  rotate3d.name = "Rotate3D";
  rotate3d.nodedef = "ND_rotate3d_vector3";
  rotate3d.links["in"] = {"Vector", "out", materialx::Type::Vector3};
  rotate3d.links["amount"] = {"Angle", "out", materialx::Type::Float};
  rotate3d.vector3_inputs["axis"] = make_float3(0.0f, 1.0f, 0.0f);
  rotate3d.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{uv, rotate2d, vector, angle, rotate3d}}, &graph));

  ShaderNode *uv_node = nullptr;
  ShaderNode *vector_node = nullptr;
  ShaderNode *angle_node = nullptr;
  VectorRotateNode *rotate2d_node = nullptr;
  VectorRotateNode *rotate3d_node = nullptr;
  MathNode *rotate2d_radians = nullptr;
  MathNode *rotate3d_radians = nullptr;
  for (ShaderNode *node : graph.nodes) {
    uv_node = node->name == "UV" ? node : uv_node;
    vector_node = node->name == "Vector" ? node : vector_node;
    angle_node = node->name == "Angle" ? node : angle_node;
    rotate2d_node = node->name == "Rotate2D" ? dynamic_cast<VectorRotateNode *>(node) : rotate2d_node;
    rotate3d_node = node->name == "Rotate3D" ? dynamic_cast<VectorRotateNode *>(node) : rotate3d_node;
    rotate2d_radians = node->name == "Rotate2D.radians" ? dynamic_cast<MathNode *>(node) : rotate2d_radians;
    rotate3d_radians = node->name == "Rotate3D.radians" ? dynamic_cast<MathNode *>(node) : rotate3d_radians;
  }

  ASSERT_NE(rotate2d_node, nullptr);
  EXPECT_EQ(rotate2d_node->get_rotate_type(), NODE_VECTOR_ROTATE_TYPE_AXIS_Z);
  EXPECT_TRUE(rotate2d_node->get_invert());
  ASSERT_NE(rotate2d_radians, nullptr);
  EXPECT_EQ(rotate2d_radians->get_math_type(), NODE_MATH_RADIANS);
  EXPECT_FLOAT_EQ(rotate2d_radians->get_value1(), 90.0f);
  ASSERT_NE(uv_node, nullptr);
  EXPECT_EQ(rotate2d_node->input("Vector")->link, uv_node->output("Vector"));
  EXPECT_EQ(rotate2d_node->input("Angle")->link, rotate2d_radians->output("Value"));

  ASSERT_NE(rotate3d_node, nullptr);
  EXPECT_EQ(rotate3d_node->get_rotate_type(), NODE_VECTOR_ROTATE_TYPE_AXIS);
  EXPECT_FALSE(rotate3d_node->get_invert());
  EXPECT_EQ(rotate3d_node->get_axis(), make_float3(0.0f, 1.0f, 0.0f));
  ASSERT_NE(rotate3d_radians, nullptr);
  EXPECT_EQ(rotate3d_radians->get_math_type(), NODE_MATH_RADIANS);
  ASSERT_NE(vector_node, nullptr);
  ASSERT_NE(angle_node, nullptr);
  EXPECT_EQ(rotate3d_node->input("Vector")->link, vector_node->output("Vector"));
  EXPECT_EQ(rotate3d_node->input("Angle")->link, rotate3d_radians->output("Value"));
  EXPECT_EQ(rotate3d_radians->input("Value1")->link, angle_node->output("Value"));
}


TEST(materialx_graph, lowers_vector_rotation_utilities_with_installed_defaults)
{
  materialx::Node rotate2d;
  rotate2d.name = "Rotate2DDefaults";
  rotate2d.nodedef = "ND_rotate2d_vector2";
  rotate2d.outputs["out"] = materialx::Type::Vector2;

  materialx::Node rotate3d;
  rotate3d.name = "Rotate3DDefaults";
  rotate3d.nodedef = "ND_rotate3d_vector3";
  rotate3d.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{rotate2d, rotate3d}}, &graph));

  VectorRotateNode *rotate2d_node = nullptr;
  VectorRotateNode *rotate3d_node = nullptr;
  MathNode *rotate2d_radians = nullptr;
  MathNode *rotate3d_radians = nullptr;
  for (ShaderNode *node : graph.nodes) {
    rotate2d_node = node->name == "Rotate2DDefaults" ? dynamic_cast<VectorRotateNode *>(node) :
                                                       rotate2d_node;
    rotate3d_node = node->name == "Rotate3DDefaults" ? dynamic_cast<VectorRotateNode *>(node) :
                                                       rotate3d_node;
    rotate2d_radians = node->name == "Rotate2DDefaults.radians" ? dynamic_cast<MathNode *>(node) :
                                                                  rotate2d_radians;
    rotate3d_radians = node->name == "Rotate3DDefaults.radians" ? dynamic_cast<MathNode *>(node) :
                                                                  rotate3d_radians;
  }

  ASSERT_NE(rotate2d_node, nullptr);
  EXPECT_EQ(rotate2d_node->get_vector(), zero_float3());
  ASSERT_NE(rotate2d_radians, nullptr);
  EXPECT_FLOAT_EQ(rotate2d_radians->get_value1(), 0.0f);
  EXPECT_EQ(rotate2d_node->input("Vector")->link, nullptr);
  EXPECT_EQ(rotate2d_radians->input("Value1")->link, nullptr);

  ASSERT_NE(rotate3d_node, nullptr);
  EXPECT_EQ(rotate3d_node->get_vector(), zero_float3());
  EXPECT_EQ(rotate3d_node->get_axis(), make_float3(0.0f, 1.0f, 0.0f));
  ASSERT_NE(rotate3d_radians, nullptr);
  EXPECT_FLOAT_EQ(rotate3d_radians->get_value1(), 0.0f);
  EXPECT_EQ(rotate3d_node->input("Vector")->link, nullptr);
  EXPECT_EQ(rotate3d_radians->input("Value1")->link, nullptr);
}

TEST(materialx_graph, rejects_unsafe_vector_rotation_utilities_without_mutating_graph)
{
  materialx::Node rotate2d;
  rotate2d.name = "Rotate2D";
  rotate2d.nodedef = "ND_rotate2d_vector2";
  rotate2d.vector2_inputs["in"] = make_float2(1.0f, 0.0f);
  rotate2d.inputs["amount"] = std::numeric_limits<float>::infinity();
  rotate2d.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  graph.create_node<MathNode>()->name = "sentinel";
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower({{rotate2d}}, &graph));
  ASSERT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.nodes[original_node_count - 1]->name, "sentinel");

  materialx::Node rotate3d;
  rotate3d.name = "Rotate3D";
  rotate3d.nodedef = "ND_rotate3d_vector3";
  rotate3d.vector3_inputs["in"] = make_float3(1.0f, 0.0f, 0.0f);
  rotate3d.inputs["amount"] = 45.0f;
  rotate3d.vector3_inputs["axis"] = make_float3(0.0f, 0.0f, 0.0f);
  rotate3d.outputs["out"] = materialx::Type::Vector3;

  EXPECT_FALSE(materialx::lower({{rotate3d}}, &graph));
  ASSERT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.nodes[original_node_count - 1]->name, "sentinel");
}

TEST(materialx_graph, lowers_vector_remap_forms_to_unclamped_linear_ranges)
{
  materialx::Node vector2;
  vector2.name = "Vector2";
  vector2.nodedef = "ND_remap_vector2";
  vector2.vector2_inputs = {{"in", make_float2(0.25f, 0.75f)},
                            {"inlow", make_float2(0.0f, 0.0f)},
                            {"inhigh", make_float2(1.0f, 1.0f)},
                            {"outlow", make_float2(-1.0f, -1.0f)},
                            {"outhigh", make_float2(1.0f, 1.0f)}};
  vector2.outputs["out"] = materialx::Type::Vector2;
  materialx::Node vector2fa;
  vector2fa.name = "Vector2FA";
  vector2fa.nodedef = "ND_remap_vector2FA";
  vector2fa.vector2_inputs["in"] = make_float2(0.5f, 0.25f);
  vector2fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  vector2fa.outputs["out"] = materialx::Type::Vector2;
  materialx::Node vector3;
  vector3.name = "Vector3";
  vector3.nodedef = "ND_remap_vector3";
  vector3.vector3_inputs = {{"in", make_float3(0.25f, 0.5f, 0.75f)},
                            {"inlow", make_float3(0.0f)}, {"inhigh", make_float3(1.0f)},
                            {"outlow", make_float3(-1.0f)}, {"outhigh", make_float3(1.0f)}};
  vector3.outputs["out"] = materialx::Type::Vector3;
  materialx::Node vector3fa;
  vector3fa.name = "Vector3FA";
  vector3fa.nodedef = "ND_remap_vector3FA";
  vector3fa.vector3_inputs["in"] = make_float3(0.5f);
  vector3fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  vector3fa.outputs["out"] = materialx::Type::Vector3;
  materialx::Graph source;
  source.nodes = {vector2, vector2fa, vector3, vector3fa};
  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  int count = 0;
  for (ShaderNode *node : graph.nodes) {
    if (node->type == VectorMapRangeNode::get_node_type()) {
      ++count;
      EXPECT_EQ(static_cast<VectorMapRangeNode *>(node)->get_range_type(), NODE_MAP_RANGE_LINEAR);
      EXPECT_FALSE(static_cast<VectorMapRangeNode *>(node)->get_use_clamp());
    }
  }
  EXPECT_EQ(count, 4);
}

TEST(materialx_graph, validates_and_lowers_exact_vector2_range_boundaries)
{
  const auto range_node = [](const char *name, const bool clamp) {
    materialx::Node node{name, "ND_range_vector2"};
    node.vector2_inputs = {{"in", make_float2(0.25f, 0.75f)},
                           {"inlow", make_float2(0.0f, 0.0f)},
                           {"inhigh", make_float2(1.0f, 1.0f)},
                           {"outlow", make_float2(-1.0f, -0.5f)},
                           {"outhigh", make_float2(1.0f, 0.5f)}};
    node.int_inputs["doclamp"] = clamp ? 1 : 0;
    node.outputs["out"] = materialx::Type::Vector2;
    return node;
  };

  materialx::Node input{"Input", "ND_constant_vector2"};
  input.vector2_inputs["value"] = make_float2(0.5f, 0.25f);
  input.outputs["out"] = materialx::Type::Vector2;
  materialx::Node literal = range_node("LiteralRange", false);
  materialx::Node linked = range_node("LinkedRange", true);
  linked.vector2_inputs.erase("in");
  linked.links["in"] = {"Input", "out", materialx::Type::Vector2};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, literal, linked}}, &graph));
  int ranges = 0;
  for (ShaderNode *node : graph.nodes) {
    if (const auto *range = dynamic_cast<VectorMapRangeNode *>(node)) {
      ++ranges;
      EXPECT_EQ(range->get_use_clamp(), node->name == "LinkedRange");
    }
  }
  EXPECT_EQ(ranges, 2);

  std::vector<materialx::Node> invalid;
  materialx::Node invalid_doclamp = range_node("InvalidDoclamp", false);
  invalid_doclamp.int_inputs["doclamp"] = 2;
  invalid.push_back(invalid_doclamp);
  materialx::Node missing_doclamp = range_node("MissingDoclamp", false);
  missing_doclamp.int_inputs.clear();
  invalid.push_back(missing_doclamp);
  materialx::Node nonfinite = range_node("Nonfinite", false);
  nonfinite.vector2_inputs["outlow"] = make_float2(
      std::numeric_limits<float>::quiet_NaN(), 0.0f);
  invalid.push_back(nonfinite);
  materialx::Node degenerate = range_node("Degenerate", false);
  degenerate.vector2_inputs["inhigh"] = make_float2(0.0f, 1.0f);
  invalid.push_back(degenerate);
  materialx::Node inverted_clamp = range_node("InvertedClamp", true);
  inverted_clamp.vector2_inputs["outlow"] = make_float2(2.0f, 0.0f);
  invalid.push_back(inverted_clamp);

  for (const materialx::Node &node : invalid) {
    ShaderGraph destination;
    EmissionNode *sentinel = destination.create_node<EmissionNode>();
    destination.connect(sentinel->output("Emission"), destination.output()->input("Surface"));
    const size_t original_node_count = destination.nodes.size();
    ShaderOutput *const original_surface_link = destination.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower({{node}}, &destination)) << node.name;
    EXPECT_EQ(destination.nodes.size(), original_node_count) << node.name;
    EXPECT_EQ(destination.output()->input("Surface")->link, original_surface_link) << node.name;
  }
}

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

TEST(materialx_graph, lowers_chained_scalar_compositing_blends_to_native_mix_modes)
{
  struct BlendCase {
    const char *name;
    const char *nodedef;
    NodeMix mix_type;
  };
  const BlendCase cases[] = {{"Plus", "ND_plus_float", NODE_MIX_ADD},
                             {"Minus", "ND_minus_float", NODE_MIX_SUB},
                             {"Difference", "ND_difference_float", NODE_MIX_DIFF},
                             {"Burn", "ND_burn_float", NODE_MIX_BURN},
                             {"Dodge", "ND_dodge_float", NODE_MIX_DODGE},
                             {"Screen", "ND_screen_float", NODE_MIX_SCREEN},
                             {"Overlay", "ND_overlay_float", NODE_MIX_OVERLAY}};
  materialx::Graph source;
  for (size_t index = 0; index < std::size(cases); index++) {
    materialx::Node blend;
    blend.name = cases[index].name;
    blend.nodedef = cases[index].nodedef;
    blend.inputs["bg"] = 0.2f + float(index) * 0.01f;
    blend.inputs["mix"] = 0.75f;
    if (index == 0) {
      blend.inputs["fg"] = 0.8f;
    }
    else {
      blend.links["fg"] = {cases[index - 1].name, "out", materialx::Type::Float};
    }
    blend.outputs["out"] = materialx::Type::Float;
    source.nodes.push_back(std::move(blend));
  }
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Overlay", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;
  source.nodes.push_back(std::move(surface));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, MixColorNode *> blends;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *blend = dynamic_cast<MixColorNode *>(node)) {
      blends.emplace(node->name, blend);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  for (const BlendCase &test_case : cases) {
    ASSERT_NE(blends[test_case.name], nullptr) << test_case.nodedef;
    EXPECT_EQ(blends[test_case.name]->get_blend_type(), test_case.mix_type);
    EXPECT_FALSE(blends[test_case.name]->get_use_clamp());
    EXPECT_FALSE(blends[test_case.name]->get_use_clamp_result());
  }
  for (size_t index = 1; index < std::size(cases); index++) {
    EXPECT_EQ(blends[cases[index].name]->input("B")->link,
              blends[cases[index - 1].name]->output("Result"));
  }
  EXPECT_NE(principled->input("Roughness")->link, nullptr);
}

TEST(materialx_graph, lowers_color3_compositing_blends_and_color_factor_mix)
{
  struct BlendCase {
    const char *name;
    const char *nodedef;
    NodeMix mix_type;
  };
  const BlendCase cases[] = {{"Plus", "ND_plus_color3", NODE_MIX_ADD},
                             {"Minus", "ND_minus_color3", NODE_MIX_SUB},
                             {"Difference", "ND_difference_color3", NODE_MIX_DIFF},
                             {"Burn", "ND_burn_color3", NODE_MIX_BURN},
                             {"Dodge", "ND_dodge_color3", NODE_MIX_DODGE},
                             {"Screen", "ND_screen_color3", NODE_MIX_SCREEN},
                             {"Overlay", "ND_overlay_color3", NODE_MIX_OVERLAY}};
  materialx::Graph source;
  for (const auto &[name, value] :
       {std::pair{"Background", make_float3(0.2f, 0.4f, 0.6f)},
        std::pair{"Foreground", make_float3(0.8f, 0.3f, 0.1f)},
        std::pair{"ColorFactor", make_float3(0.2f, 0.5f, 0.8f)}})
  {
    materialx::Node constant;
    constant.name = name;
    constant.nodedef = "ND_constant_color3";
    constant.color3_inputs["value"] = value;
    constant.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(constant));
  }
  for (size_t index = 0; index < std::size(cases); index++) {
    materialx::Node blend;
    blend.name = cases[index].name;
    blend.nodedef = cases[index].nodedef;
    blend.links["bg"] = {index == 0 ? "Background" : cases[index - 1].name,
                         "out",
                         materialx::Type::Color3};
    blend.links["fg"] = {"Foreground", "out", materialx::Type::Color3};
    blend.inputs["mix"] = 0.25f + 0.05f * float(index);
    blend.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(blend));
  }
  materialx::Node mix;
  mix.name = "ColorFactorMix";
  mix.nodedef = "ND_mix_color3_color3";
  mix.links["bg"] = {"Overlay", "out", materialx::Type::Color3};
  mix.links["fg"] = {"Foreground", "out", materialx::Type::Color3};
  mix.links["mix"] = {"ColorFactor", "out", materialx::Type::Color3};
  mix.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(mix);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, MixColorNode *> blends;
  MixNode *product = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *blend = dynamic_cast<MixColorNode *>(node)) {
      blends.emplace(node->name, blend);
    }
    product = node->name == "ColorFactorMix.product" ? dynamic_cast<MixNode *>(node) : product;
  }
  for (size_t index = 0; index < std::size(cases); index++) {
    if (string(cases[index].nodedef) == "ND_burn_color3" ||
        string(cases[index].nodedef) == "ND_dodge_color3")
    {
      EXPECT_EQ(blends.count(cases[index].name), 0);
      continue;
    }
    ASSERT_NE(blends[cases[index].name], nullptr);
    EXPECT_EQ(blends[cases[index].name]->get_blend_type(), cases[index].mix_type);
    EXPECT_FLOAT_EQ(blends[cases[index].name]->get_fac(), 0.25f + 0.05f * float(index));
    EXPECT_FALSE(blends[cases[index].name]->get_use_clamp());
    EXPECT_FALSE(blends[cases[index].name]->get_use_clamp_result());
  }
  ASSERT_NE(product, nullptr);
  EXPECT_EQ(product->get_mix_type(), NODE_MIX_MUL);
  ASSERT_NE(product->input("Color2")->link, nullptr);
  EXPECT_EQ(product->input("Color2")->link->parent->name, "ColorFactor");
  EXPECT_EQ(std::count_if(graph.nodes.begin(),
                          graph.nodes.end(),
                          [](ShaderNode *node) {
                            return node->name == "ColorFactorMix.factor";
                          }),
            0);
}

TEST(materialx_graph, rejects_invalid_color_compositing_literals_without_mutation)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (const auto &[nodedef, color_factor] :
       {std::pair{"ND_plus_color3", false}, std::pair{"ND_mix_color3_color3", true}})
  {
    materialx::Node node;
    node.name = "Invalid";
    node.nodedef = nodedef;
    node.color3_inputs["bg"] = make_float3(nan, 0.2f, 0.3f);
    node.color3_inputs["fg"] = make_float3(0.4f, 0.5f, 0.6f);
    if (color_factor) {
      node.color3_inputs["mix"] = make_float3(0.2f, 0.5f, 0.8f);
    }
    else {
      node.inputs["mix"] = 0.5f;
    }
    node.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t node_count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{node}}, &graph));
    EXPECT_EQ(graph.nodes.size(), node_count);
    EXPECT_EQ(graph.output()->input("Surface")->link, sentinel->output("Emission"));
  }

  for (const auto &[nodedef, wrong_type, nonfinite] :
       {std::tuple{"ND_plus_color3", true, false},
        std::tuple{"ND_plus_color3", false, true},
        std::tuple{"ND_mix_color3_color3", true, false},
        std::tuple{"ND_mix_color3_color3", false, true}})
  {
    materialx::Node node;
    node.name = "InvalidFactor";
    node.nodedef = nodedef;
    node.color3_inputs["bg"] = make_float3(0.1f, 0.2f, 0.3f);
    node.color3_inputs["fg"] = make_float3(0.4f, 0.5f, 0.6f);
    const bool expects_color = string(nodedef) == "ND_mix_color3_color3";
    const bool provide_color = wrong_type ? !expects_color : expects_color;
    if (provide_color) {
      node.color3_inputs["mix"] = make_float3(
          nonfinite ? nan : 0.2f, 0.5f, 0.8f);
    }
    else {
      node.inputs["mix"] = nonfinite ? nan : 0.5f;
    }
    node.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t node_count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{node}}, &graph));
    EXPECT_EQ(graph.nodes.size(), node_count);
    EXPECT_EQ(graph.output()->input("Surface")->link, sentinel->output("Emission"));
  }
}

TEST(materialx_graph, lowers_burn_and_dodge_color3_to_materialx_arithmetic)
{
  materialx::Graph source;
  for (const auto &[name, nodedef] :
       {std::pair{"Burn", "ND_burn_color3"}, std::pair{"Dodge", "ND_dodge_color3"}})
  {
    materialx::Node blend;
    blend.name = name;
    blend.nodedef = nodedef;
    blend.color3_inputs["fg"] = make_float3(0.0f, 0.25f, 1.0f);
    blend.color3_inputs["bg"] = make_float3(0.2f, 0.4f, 0.6f);
    blend.inputs["mix"] = 0.5f;
    blend.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(blend));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  for (const char *name : {"Burn", "Dodge"}) {
    EXPECT_EQ(std::count_if(graph.nodes.begin(),
                            graph.nodes.end(),
                            [&](ShaderNode *node) {
                              return node->name == name &&
                                     dynamic_cast<MixColorNode *>(node) != nullptr;
                            }),
              0)
        << name << " must not use Cycles blend-mode semantics";
    for (const char *channel : {"Red", "Green", "Blue"}) {
      const string prefix = string(name) + "." + channel + ".";
      MathNode *condition = nullptr;
      MathNode *divide = nullptr;
      MathNode *safe_denominator = nullptr;
      MathNode *mix_product = nullptr;
      MathNode *background_product = nullptr;
      MathNode *sum = nullptr;
      MathNode *result = nullptr;
      for (ShaderNode *node : graph.nodes) {
        condition = node->name == prefix + "condition" ? dynamic_cast<MathNode *>(node) :
                                                         condition;
        divide = node->name == prefix + "divide" ? dynamic_cast<MathNode *>(node) : divide;
        safe_denominator = node->name == prefix + "safe_denominator" ?
                               dynamic_cast<MathNode *>(node) :
                               safe_denominator;
        mix_product = node->name == prefix + "mix_product" ?
                          dynamic_cast<MathNode *>(node) :
                          mix_product;
        background_product = node->name == prefix + "background_product" ?
                                 dynamic_cast<MathNode *>(node) :
                                 background_product;
        sum = node->name == prefix + "sum" ? dynamic_cast<MathNode *>(node) : sum;
        result = node->name == prefix + "result" ? dynamic_cast<MathNode *>(node) : result;
      }
      ASSERT_NE(condition, nullptr);
      ASSERT_NE(divide, nullptr);
      ASSERT_NE(mix_product, nullptr);
      ASSERT_NE(background_product, nullptr);
      ASSERT_NE(sum, nullptr);
      ASSERT_NE(result, nullptr);
      EXPECT_EQ(condition->get_math_type(), NODE_MATH_LESS_THAN);
      EXPECT_FLOAT_EQ(condition->get_value2(), 1.0e-8f);
      EXPECT_EQ(divide->get_math_type(), NODE_MATH_DIVIDE);
      if (string(name) == "Burn") {
        ASSERT_NE(safe_denominator, nullptr);
        EXPECT_EQ(safe_denominator->get_math_type(), NODE_MATH_ADD);
        ASSERT_NE(divide->input("Value2")->link, nullptr);
        EXPECT_EQ(divide->input("Value2")->link->parent, safe_denominator);
        ASSERT_NE(safe_denominator->input("Value2")->link, nullptr);
        EXPECT_EQ(safe_denominator->input("Value2")->link->parent, condition);
      }
      EXPECT_EQ(mix_product->get_math_type(), NODE_MATH_MULTIPLY);
      EXPECT_FLOAT_EQ(mix_product->get_value2(), 0.5f);
      ASSERT_NE(background_product->input("Value1")->link, nullptr);
      EXPECT_FLOAT_EQ(
          static_cast<MathNode *>(background_product->input("Value1")->link->parent)->get_value2(),
          0.5f);
      EXPECT_EQ(sum->input("Value1")->link->parent, mix_product);
      EXPECT_EQ(sum->input("Value2")->link->parent, background_product);
      EXPECT_EQ(result->get_math_type(), NODE_MATH_MULTIPLY);
      EXPECT_EQ(result->input("Value1")->link->parent, sum);
      EXPECT_NE(result->input("Value2")->link, nullptr);
      EXPECT_EQ(result->input("Value2")->link->parent->name, prefix + "inverse_condition");
    }
  }

  const auto materialx_burn = [](const float fg, const float bg, const float mix) {
    return std::abs(fg) < 1.0e-8f ? 0.0f :
                                      mix * (1.0f - ((1.0f - bg) / fg)) +
                                          (1.0f - mix) * bg;
  };
  const auto materialx_dodge = [](const float fg, const float bg, const float mix) {
    return std::abs(1.0f - fg) < 1.0e-8f ?
               0.0f :
               mix * (bg / (1.0f - fg)) + (1.0f - mix) * bg;
  };
  EXPECT_FLOAT_EQ(materialx_burn(0.0f, 0.2f, 0.5f), 0.0f);
  EXPECT_FLOAT_EQ(
      materialx_burn(std::numeric_limits<float>::denorm_min(), 0.2f, 0.5f), 0.0f);
  EXPECT_NEAR(materialx_burn(0.25f, 0.4f, 0.5f), -0.5f, 1.0e-6f);
  EXPECT_FLOAT_EQ(materialx_dodge(1.0f, 0.6f, 0.5f), 0.0f);
  EXPECT_NEAR(materialx_dodge(0.25f, 0.4f, 0.5f), 0.46666667f, 1.0e-6f);
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

TEST(materialx_graph, lowers_exact_domain_math_vector2_nodes_componentwise)
{
  struct MathCase {
    const char *nodedef;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"ND_acos_vector2", NODE_MATH_ARCCOSINE},
                            {"ND_asin_vector2", NODE_MATH_ARCSINE},
                            {"ND_exp_vector2", NODE_MATH_EXPONENT},
                            {"ND_ln_vector2", NODE_MATH_LOGARITHM},
                            {"ND_sqrt_vector2", NODE_MATH_SQRT}};

  for (const MathCase &math_case : cases) {
    materialx::Node input;
    input.name = "Input";
    input.nodedef = "ND_constant_vector2";
    input.vector2_inputs["value"] = make_float2(0.25f, 0.5f);
    input.outputs["out"] = materialx::Type::Vector2;
    materialx::Node operation;
    operation.name = math_case.nodedef;
    operation.nodedef = math_case.nodedef;
    operation.links["in"] = {"Input", "out", materialx::Type::Vector2};
    operation.outputs["out"] = materialx::Type::Vector2;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{input, operation}}, &graph)) << math_case.nodedef;
    MathNode *x = nullptr;
    MathNode *y = nullptr;
    for (ShaderNode *node : graph.nodes) {
      x = node->name == operation.name + ".X" ? dynamic_cast<MathNode *>(node) : x;
      y = node->name == operation.name + ".Y" ? dynamic_cast<MathNode *>(node) : y;
    }
    ASSERT_NE(x, nullptr) << math_case.nodedef;
    ASSERT_NE(y, nullptr) << math_case.nodedef;
    EXPECT_EQ(x->get_math_type(), math_case.math_type) << math_case.nodedef;
    EXPECT_EQ(y->get_math_type(), math_case.math_type) << math_case.nodedef;
    if (string(math_case.nodedef) == "ND_ln_vector2") {
      EXPECT_FLOAT_EQ(x->get_value2(), M_E);
      EXPECT_FLOAT_EQ(y->get_value2(), M_E);
    }
  }
}

TEST(materialx_graph, lowers_exact_domain_math_vector3_nodes_componentwise)
{
  struct MathCase {
    const char *nodedef;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"ND_acos_vector3", NODE_MATH_ARCCOSINE},
                            {"ND_asin_vector3", NODE_MATH_ARCSINE},
                            {"ND_exp_vector3", NODE_MATH_EXPONENT},
                            {"ND_ln_vector3", NODE_MATH_LOGARITHM},
                            {"ND_sqrt_vector3", NODE_MATH_SQRT}};

  for (const MathCase &math_case : cases) {
    materialx::Node input;
    input.name = "Input";
    input.nodedef = "ND_constant_vector3";
    input.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
    input.outputs["out"] = materialx::Type::Vector3;
    materialx::Node operation;
    operation.name = math_case.nodedef;
    operation.nodedef = math_case.nodedef;
    operation.links["in"] = {"Input", "out", materialx::Type::Vector3};
    operation.outputs["out"] = materialx::Type::Vector3;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{input, operation}}, &graph)) << math_case.nodedef;
    MathNode *x = nullptr;
    MathNode *y = nullptr;
    MathNode *z = nullptr;
    for (ShaderNode *node : graph.nodes) {
      x = node->name == operation.name + ".X" ? dynamic_cast<MathNode *>(node) : x;
      y = node->name == operation.name + ".Y" ? dynamic_cast<MathNode *>(node) : y;
      z = node->name == operation.name + ".Z" ? dynamic_cast<MathNode *>(node) : z;
    }
    ASSERT_NE(x, nullptr) << math_case.nodedef;
    ASSERT_NE(y, nullptr) << math_case.nodedef;
    ASSERT_NE(z, nullptr) << math_case.nodedef;
    EXPECT_EQ(x->get_math_type(), math_case.math_type) << math_case.nodedef;
    EXPECT_EQ(y->get_math_type(), math_case.math_type) << math_case.nodedef;
    EXPECT_EQ(z->get_math_type(), math_case.math_type) << math_case.nodedef;
    if (string(math_case.nodedef) == "ND_ln_vector3") {
      EXPECT_FLOAT_EQ(x->get_value2(), M_E);
      EXPECT_FLOAT_EQ(y->get_value2(), M_E);
      EXPECT_FLOAT_EQ(z->get_value2(), M_E);
    }
  }
}

TEST(materialx_graph, lowers_atan2_vector2_and_vector3_componentwise_with_mtlx_argument_order)
{
  const auto check = [](const materialx::Type type,
                        const char *nodedef,
                        const float3 iny,
                        const float3 inx,
                        const int components) {
    materialx::Node operation;
    operation.name = nodedef;
    operation.nodedef = nodedef;
    operation.outputs["out"] = type;
    if (type == materialx::Type::Vector2) {
      operation.vector2_inputs["iny"] = make_float2(iny.x, iny.y);
      operation.vector2_inputs["inx"] = make_float2(inx.x, inx.y);
    }
    else {
      operation.vector3_inputs["iny"] = iny;
      operation.vector3_inputs["inx"] = inx;
    }

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{operation}}, &graph)) << nodedef;
    for (int index = 0; index < components; index++) {
      const char *channel = index == 0 ? "X" : index == 1 ? "Y" : "Z";
      MathNode *math = nullptr;
      for (ShaderNode *node : graph.nodes) {
        if (node->name == string(nodedef) + "." + channel) {
          math = dynamic_cast<MathNode *>(node);
        }
      }
      ASSERT_NE(math, nullptr) << nodedef << "." << channel;
      EXPECT_EQ(math->get_math_type(), NODE_MATH_ARCTAN2);
      EXPECT_FLOAT_EQ(math->get_value1(), iny[index]);
      EXPECT_FLOAT_EQ(math->get_value2(), inx[index]);
    }
  };

  check(materialx::Type::Vector2,
        "ND_atan2_vector2",
        make_float3(4.0f, 5.0f, 0.0f),
        make_float3(3.0f, 2.0f, 0.0f),
        2);
  check(materialx::Type::Vector3,
        "ND_atan2_vector3",
        make_float3(4.0f, 5.0f, 6.0f),
        make_float3(3.0f, 2.0f, 1.0f),
        3);
}

TEST(materialx_graph, lowers_round_vector2_and_vector3_componentwise)
{
  const auto check = [](const materialx::Type type, const char *nodedef, const int components) {
    materialx::Node operation;
    operation.name = nodedef;
    operation.nodedef = nodedef;
    operation.outputs["out"] = type;
    if (type == materialx::Type::Vector2) {
      operation.vector2_inputs["in"] = make_float2(0.25f, 1.75f);
    }
    else {
      operation.vector3_inputs["in"] = make_float3(0.25f, 1.75f, -2.5f);
    }
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{operation}}, &graph));
    for (int index = 0; index < components; index++) {
      const char *channel = index == 0 ? "X" : index == 1 ? "Y" : "Z";
      MathNode *math = nullptr;
      for (ShaderNode *node : graph.nodes) {
        if (node->name == string(nodedef) + "." + channel) {
          math = dynamic_cast<MathNode *>(node);
        }
      }
      ASSERT_NE(math, nullptr);
      EXPECT_EQ(math->get_math_type(), NODE_MATH_ROUND);
    }
  };
  check(materialx::Type::Vector2, "ND_round_vector2", 2);
  check(materialx::Type::Vector3, "ND_round_vector3", 3);
}

TEST(materialx_graph, lowers_invert_vector_component_amount_minus_input_with_scalar_broadcast)
{
  const auto check = [](const materialx::Type type, const char *nodedef, const bool scalar_amount) {
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.outputs["out"] = type;
    const int components = type == materialx::Type::Vector2 ? 2 : 3;
    if (type == materialx::Type::Vector2) {
      node.vector2_inputs["in"] = make_float2(0.2f, 0.8f);
      if (scalar_amount) node.inputs["amount"] = 0.25f;
      else node.vector2_inputs["amount"] = make_float2(0.0f, 0.5f);
    }
    else {
      node.vector3_inputs["in"] = make_float3(0.2f, 0.5f, 0.8f);
      if (scalar_amount) node.inputs["amount"] = 0.25f;
      else node.vector3_inputs["amount"] = make_float3(0.0f, 0.5f, 1.0f);
    }
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << nodedef;
    for (int index = 0; index < components; index++) {
      const char *channel = index == 0 ? "X" : index == 1 ? "Y" : "Z";
      MathNode *subtract = nullptr;
      for (ShaderNode *shader : graph.nodes) {
        subtract = shader->name == string(nodedef) + "." + channel ? dynamic_cast<MathNode *>(shader) : subtract;
      }
      ASSERT_NE(subtract, nullptr);
      EXPECT_EQ(subtract->get_math_type(), NODE_MATH_SUBTRACT);
      const float expected_amount = scalar_amount ? 0.25f : (index == 0 ? 0.0f : index == 1 ? 0.5f : 1.0f);
      EXPECT_FLOAT_EQ(subtract->get_value1(), expected_amount);
    }
  };
  check(materialx::Type::Vector2, "ND_invert_vector2", false);
  check(materialx::Type::Vector2, "ND_invert_vector2FA", true);
  check(materialx::Type::Vector3, "ND_invert_vector3", false);
  check(materialx::Type::Vector3, "ND_invert_vector3FA", true);
}

TEST(materialx_graph, lowers_smoothstep_vector_componentwise_with_scalar_edge_broadcast)
{
  const auto check = [](const materialx::Type type, const char *nodedef, const bool scalar_edges) {
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.outputs["out"] = type;
    const int components = type == materialx::Type::Vector2 ? 2 : 3;
    if (type == materialx::Type::Vector2) {
      node.vector2_inputs["in"] = make_float2(0.25f, 0.75f);
      if (scalar_edges) { node.inputs["low"] = 0.0f; node.inputs["high"] = 1.0f; }
      else { node.vector2_inputs["low"] = make_float2(0.0f, 0.25f); node.vector2_inputs["high"] = make_float2(1.0f, 1.25f); }
    }
    else {
      node.vector3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
      if (scalar_edges) { node.inputs["low"] = 0.0f; node.inputs["high"] = 1.0f; }
      else { node.vector3_inputs["low"] = make_float3(0.0f, 0.25f, 0.5f); node.vector3_inputs["high"] = make_float3(1.0f, 1.25f, 1.5f); }
    }
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << nodedef;
    for (int index = 0; index < components; index++) {
      const char *channel = index == 0 ? "X" : index == 1 ? "Y" : "Z";
      MathNode *divide = nullptr, *minimum = nullptr, *square = nullptr, *result = nullptr;
      for (ShaderNode *shader : graph.nodes) {
        divide = shader->name == string(nodedef) + "." + channel + ".divide" ? dynamic_cast<MathNode *>(shader) : divide;
        minimum = shader->name == string(nodedef) + "." + channel + ".minimum" ? dynamic_cast<MathNode *>(shader) : minimum;
        square = shader->name == string(nodedef) + "." + channel + ".square" ? dynamic_cast<MathNode *>(shader) : square;
        result = shader->name == string(nodedef) + "." + channel + ".result" ? dynamic_cast<MathNode *>(shader) : result;
      }
      ASSERT_NE(divide, nullptr); ASSERT_NE(minimum, nullptr); ASSERT_NE(square, nullptr); ASSERT_NE(result, nullptr);
      EXPECT_EQ(divide->get_math_type(), NODE_MATH_DIVIDE);
      EXPECT_EQ(minimum->get_math_type(), NODE_MATH_MINIMUM);
      EXPECT_FLOAT_EQ(minimum->get_value2(), 1.0f);
      EXPECT_EQ(square->get_math_type(), NODE_MATH_MULTIPLY);
      EXPECT_EQ(result->get_math_type(), NODE_MATH_MULTIPLY);
    }
  };
  check(materialx::Type::Vector2, "ND_smoothstep_vector2", false);
  check(materialx::Type::Vector2, "ND_smoothstep_vector2FA", true);
  check(materialx::Type::Vector3, "ND_smoothstep_vector3", false);
  check(materialx::Type::Vector3, "ND_smoothstep_vector3FA", true);
}

TEST(materialx_graph, lowers_modulo_and_power_vector_component_nodes)
{
  for (const char *nodedef : {"ND_modulo_vector2", "ND_modulo_vector3", "ND_power_vector2", "ND_power_vector3",
                              "ND_modulo_vector2FA", "ND_modulo_vector3FA", "ND_power_vector2FA", "ND_power_vector3FA"}) {
    materialx::Node node; node.name = nodedef; node.nodedef = nodedef;
    const bool vector2 = string(nodedef).find("vector2") != string::npos;
    const bool scalar = string(nodedef).find("FA") != string::npos;
    node.outputs["out"] = vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;
    if (vector2) node.vector2_inputs["in1"] = make_float2(5.0f, 7.0f);
    else node.vector3_inputs["in1"] = make_float3(5.0f, 7.0f, 11.0f);
    if (scalar) node.inputs["in2"] = 2.0f;
    else if (vector2) node.vector2_inputs["in2"] = make_float2(2.0f, 3.0f);
    else node.vector3_inputs["in2"] = make_float3(2.0f, 3.0f, 4.0f);
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << nodedef;
    const NodeMathType expected = string(nodedef).find("modulo") != string::npos ?
                                      NODE_MATH_MODULO : NODE_MATH_POWER;
    const int components = vector2 ? 2 : 3;
    int math_count = 0;
    CombineXYZNode *combine = nullptr;
    for (ShaderNode *lowered : graph.nodes) {
      if (MathNode *math = dynamic_cast<MathNode *>(lowered)) {
        if (math->get_math_type() == expected) {
          ++math_count;
          const char channel = math->name[math->name.size() - 1];
          const float first = channel == 'X' ? 5.0f : channel == 'Y' ? 7.0f : 11.0f;
          const float second = scalar ? 2.0f : channel == 'X' ? 2.0f : channel == 'Y' ? 3.0f : 4.0f;
          EXPECT_FLOAT_EQ(math->get_value1(), first) << nodedef;
          EXPECT_FLOAT_EQ(math->get_value2(), second) << nodedef;
        }
      }
      if (lowered->name == nodedef) combine = dynamic_cast<CombineXYZNode *>(lowered);
    }
    ASSERT_NE(combine, nullptr) << nodedef;
    EXPECT_EQ(math_count, components) << nodedef;
    if (vector2) EXPECT_FLOAT_EQ(combine->get_z(), 0.0f) << nodedef;
  }
}

TEST(materialx_graph, lowers_vector3_clamp_and_scalar_bound_broadcast)
{
  for (const char *nodedef : {"ND_clamp_vector3", "ND_clamp_vector3FA"}) {
    const bool scalar_bounds = string(nodedef).find("FA") != string::npos;
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.outputs["out"] = materialx::Type::Vector3;
    node.vector3_inputs["in"] = make_float3(-1.0f, 0.5f, 4.0f);
    if (scalar_bounds) {
      node.inputs["low"] = 0.0f;
      node.inputs["high"] = 2.0f;
    }
    else {
      node.vector3_inputs["low"] = make_float3(0.0f, 0.25f, 1.0f);
      node.vector3_inputs["high"] = make_float3(1.0f, 0.75f, 3.0f);
    }
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << nodedef;
    VectorMathNode *minimum = nullptr;
    VectorMathNode *maximum = nullptr;
    for (ShaderNode *lowered : graph.nodes) {
      if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(lowered)) {
        minimum = math->get_math_type() == NODE_VECTOR_MATH_MINIMUM ? math : minimum;
        maximum = math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM ? math : maximum;
      }
    }
    ASSERT_NE(minimum, nullptr) << nodedef;
    ASSERT_NE(maximum, nullptr) << nodedef;
    EXPECT_EQ(minimum->input("Vector1")->link, nullptr) << nodedef;
    EXPECT_EQ(maximum->input("Vector1")->link, minimum->output("Vector")) << nodedef;
    EXPECT_EQ(minimum->get_vector1(), make_float3(-1.0f, 0.5f, 4.0f)) << nodedef;
    EXPECT_EQ(minimum->get_vector2(), scalar_bounds ? make_float3(2.0f, 2.0f, 2.0f) : make_float3(1.0f, 0.75f, 3.0f)) << nodedef;
    EXPECT_EQ(maximum->get_vector2(), scalar_bounds ? make_float3(0.0f, 0.0f, 0.0f) : make_float3(0.0f, 0.25f, 1.0f)) << nodedef;
  }
}

TEST(materialx_graph, lowers_vector_scalar_broadcast_min_max_divide_and_clamp)
{
  for (const char *nodedef : {"ND_clamp_vector2FA", "ND_min_vector2FA", "ND_max_vector2FA",
                              "ND_min_vector3FA", "ND_max_vector3FA", "ND_divide_vector2FA",
                              "ND_divide_vector3FA"}) {
    const bool vector2 = string(nodedef).find("vector2") != string::npos;
    const bool clamp = string(nodedef).find("clamp") != string::npos;
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.outputs["out"] = vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;
    if (clamp) {
      node.vector2_inputs["in"] = make_float2(-1.0f, 4.0f);
      node.inputs["low"] = 0.0f;
      node.inputs["high"] = 2.0f;
    }
    else {
      if (vector2) node.vector2_inputs["in1"] = make_float2(8.0f, 12.0f);
      else node.vector3_inputs["in1"] = make_float3(8.0f, 12.0f, 16.0f);
      node.inputs["in2"] = 2.0f;
    }
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << nodedef;
    if (clamp) {
      VectorMathNode *minimum = nullptr;
      VectorMathNode *maximum = nullptr;
      for (ShaderNode *lowered : graph.nodes) {
        if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(lowered)) {
          minimum = math->get_math_type() == NODE_VECTOR_MATH_MINIMUM ? math : minimum;
          maximum = math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM ? math : maximum;
        }
      }
      ASSERT_NE(minimum, nullptr);
      ASSERT_NE(maximum, nullptr);
      EXPECT_EQ(minimum->get_vector2(), make_float3(2.0f, 2.0f, 2.0f));
      EXPECT_EQ(maximum->get_vector2(), make_float3(0.0f, 0.0f, 0.0f));
      continue;
    }
    const NodeMathType expected = string(nodedef).find("min_") != string::npos ?
                                      NODE_MATH_MINIMUM :
                                      string(nodedef).find("max_") != string::npos ?
                                          NODE_MATH_MAXIMUM : NODE_MATH_DIVIDE;
    const int components = vector2 ? 2 : 3;
    int count = 0;
    for (ShaderNode *lowered : graph.nodes) {
      if (MathNode *math = dynamic_cast<MathNode *>(lowered); math &&
          math->get_math_type() == expected)
      {
        ++count;
        EXPECT_FLOAT_EQ(math->get_value2(), 2.0f) << nodedef;
      }
    }
    EXPECT_EQ(count, components) << nodedef;
  }
}

TEST(materialx_graph, rejects_zero_vector_scalar_divisors)
{
  for (const char *nodedef : {"ND_divide_vector2FA", "ND_divide_vector3FA"}) {
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    const bool vector2 = string(nodedef).find("vector2") != string::npos;
    node.outputs["out"] = vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;
    if (vector2) node.vector2_inputs["in1"] = make_float2(1.0f, 2.0f);
    else node.vector3_inputs["in1"] = make_float3(1.0f, 2.0f, 3.0f);
    node.inputs["in2"] = 0.0f;
    EXPECT_FALSE(materialx::validate({{node}})) << nodedef;
  }
}

TEST(materialx_graph, rejects_nonfinite_vector_scalar_divisors_before_mutating_destination)
{
  for (const char *nodedef : {"ND_divide_vector2FA", "ND_divide_vector3FA"}) {
    const bool vector2 = string(nodedef).find("vector2") != string::npos;
    for (const float divisor : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity()})
    {
      materialx::Node node;
      node.name = nodedef;
      node.nodedef = nodedef;
      node.outputs["out"] = vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;
      if (vector2) {
        node.vector2_inputs["in1"] = make_float2(1.0f, 2.0f);
      }
      else {
        node.vector3_inputs["in1"] = make_float3(1.0f, 2.0f, 3.0f);
      }
      node.inputs["in2"] = divisor;

      ShaderGraph destination;
      ValueNode *const sentinel = destination.create_node<ValueNode>();
      const size_t initial_node_count = destination.nodes.size();
      ASSERT_FALSE(materialx::lower({{node}}, &destination)) << nodedef;
      ASSERT_EQ(destination.nodes.size(), initial_node_count) << nodedef;
      EXPECT_NE(std::find(destination.nodes.begin(), destination.nodes.end(), sentinel),
                destination.nodes.end())
          << nodedef;
    }
  }
}

TEST(materialx_graph, lowers_linked_vector_scalar_divisor_to_every_component)
{
  for (const char *nodedef : {"ND_divide_vector2FA", "ND_divide_vector3FA"}) {
    const bool vector2 = string(nodedef).find("vector2") != string::npos;

    materialx::Node input;
    input.name = "Input";
    input.nodedef = vector2 ? "ND_constant_vector2" : "ND_constant_vector3";
    input.outputs["out"] = vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;
    if (vector2) {
      input.vector2_inputs["value"] = make_float2(8.0f, 12.0f);
    }
    else {
      input.vector3_inputs["value"] = make_float3(8.0f, 12.0f, 16.0f);
    }

    materialx::Node divisor;
    divisor.name = "Divisor";
    divisor.nodedef = "ND_constant_float";
    divisor.inputs["value"] = 2.0f;
    divisor.outputs["out"] = materialx::Type::Float;

    materialx::Node divide;
    divide.name = "Divide";
    divide.nodedef = nodedef;
    divide.links["in1"] = {"Input", "out", input.outputs["out"]};
    divide.links["in2"] = {"Divisor", "out", materialx::Type::Float};
    divide.outputs["out"] = input.outputs["out"];

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{input, divisor, divide}}, &graph)) << nodedef;

    ValueNode *lowered_divisor = nullptr;
    std::vector<MathNode *> lowered_divides;
    for (ShaderNode *node : graph.nodes) {
      lowered_divisor = node->name == "Divisor" ? dynamic_cast<ValueNode *>(node) : lowered_divisor;
      if (MathNode *math = dynamic_cast<MathNode *>(node);
          math && math->get_math_type() == NODE_MATH_DIVIDE)
      {
        lowered_divides.push_back(math);
      }
    }
    ASSERT_NE(lowered_divisor, nullptr) << nodedef;
    ASSERT_EQ(lowered_divides.size(), vector2 ? 2 : 3) << nodedef;
    for (MathNode *math : lowered_divides) {
      EXPECT_EQ(math->input("Value2")->link, lowered_divisor->output("Value")) << nodedef;
    }
  }
}

TEST(materialx_graph, lowers_color3_clamp_and_scalar_component_math)
{
  for (const char *nodedef : {"ND_clamp_color3", "ND_clamp_color3FA", "ND_modulo_color3FA", "ND_power_color3FA"}) {
    materialx::Node node; node.name = nodedef; node.nodedef = nodedef; node.outputs["out"] = materialx::Type::Color3;
    if (string(nodedef).find("clamp") != string::npos) {
      node.color3_inputs["in"] = make_float3(-1.0f, 0.5f, 4.0f);
      if (string(nodedef).find("FA") != string::npos) { node.inputs["low"] = 0.0f; node.inputs["high"] = 2.0f; }
      else { node.color3_inputs["low"] = make_float3(0.0f, 0.25f, 1.0f); node.color3_inputs["high"] = make_float3(1.0f, 0.75f, 3.0f); }
    }
    materialx::Node color, scalar;
    if (string(nodedef).find("clamp") == string::npos) {
      color.name = string(nodedef) + ".color"; color.nodedef = "ND_constant_color3"; color.color3_inputs["value"] = make_float3(5.0f, 7.0f, 11.0f); color.outputs["out"] = materialx::Type::Color3;
      scalar.name = string(nodedef) + ".scalar"; scalar.nodedef = "ND_constant_float"; scalar.inputs["value"] = 2.0f; scalar.outputs["out"] = materialx::Type::Float;
      node.links["in1"] = {color.name, "out", materialx::Type::Color3}; node.links["in2"] = {scalar.name, "out", materialx::Type::Float};
    }
    materialx::Graph source;
    if (string(nodedef).find("clamp") == string::npos) { source.nodes.push_back(color); source.nodes.push_back(scalar); }
    source.nodes.push_back(node);
    ShaderGraph graph; ASSERT_TRUE(materialx::lower(source, &graph)) << nodedef;
    const NodeMathType type = string(nodedef).find("modulo") != string::npos ? NODE_MATH_MODULO : string(nodedef).find("power") != string::npos ? NODE_MATH_POWER : NODE_MATH_MINIMUM;
    int count=0; for (ShaderNode *lowered : graph.nodes) if (const MathNode *math=dynamic_cast<MathNode *>(lowered)) count += math->get_math_type()==type;
    EXPECT_EQ(count,3) << nodedef;
  }
}

TEST(materialx_graph, lowers_vector2_color3_conversion_adapters)
{
  materialx::Node scalar; scalar.name="Scalar"; scalar.nodedef="ND_constant_float"; scalar.inputs["value"]=0.25f; scalar.outputs["out"]=materialx::Type::Float;
  materialx::Node float_to_v2; float_to_v2.name="FloatToV2"; float_to_v2.nodedef="ND_convert_float_vector2"; float_to_v2.links["in"]={"Scalar","out",materialx::Type::Float}; float_to_v2.outputs["out"]=materialx::Type::Vector2;
  materialx::Node v2_to_v3; v2_to_v3.name="V2ToV3"; v2_to_v3.nodedef="ND_convert_vector2_vector3"; v2_to_v3.links["in"]={"FloatToV2","out",materialx::Type::Vector2}; v2_to_v3.outputs["out"]=materialx::Type::Vector3;
  materialx::Node v3_to_color; v3_to_color.name="V3ToColor"; v3_to_color.nodedef="ND_convert_vector3_color3"; v3_to_color.links["in"]={"V2ToV3","out",materialx::Type::Vector3}; v3_to_color.outputs["out"]=materialx::Type::Color3;
  materialx::Node color_to_v2; color_to_v2.name="ColorToV2"; color_to_v2.nodedef="ND_convert_color3_vector2"; color_to_v2.links["in"]={"V3ToColor","out",materialx::Type::Color3}; color_to_v2.outputs["out"]=materialx::Type::Vector2;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{scalar,float_to_v2,v2_to_v3,v3_to_color,color_to_v2}},&graph));
  CombineXYZNode *float_combine=nullptr,*v3_combine=nullptr,*v2_combine=nullptr; CombineColorNode *color_combine=nullptr;
  for(ShaderNode *node:graph.nodes){float_combine=node->name=="FloatToV2"?dynamic_cast<CombineXYZNode *>(node):float_combine;v3_combine=node->name=="V2ToV3"?dynamic_cast<CombineXYZNode *>(node):v3_combine;color_combine=node->name=="V3ToColor"?dynamic_cast<CombineColorNode *>(node):color_combine;v2_combine=node->name=="ColorToV2"?dynamic_cast<CombineXYZNode *>(node):v2_combine;}
  ASSERT_NE(float_combine,nullptr); ASSERT_NE(v3_combine,nullptr); ASSERT_NE(color_combine,nullptr); ASSERT_NE(v2_combine,nullptr);
  EXPECT_FLOAT_EQ(float_combine->get_z(),0.0f); EXPECT_FLOAT_EQ(v3_combine->get_z(),0.0f); EXPECT_FLOAT_EQ(color_combine->get_b(),0.0f); EXPECT_FLOAT_EQ(v2_combine->get_z(),0.0f);
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

TEST(materialx_graph, lowers_float_conditionals_with_exact_boundary_semantics)
{
  struct ConditionalCase {
    const char *nodedef;
    NodeMathType condition_type;
    float value1;
    float value2;
  };
  const ConditionalCase cases[] = {{"ND_ifgreater_float", NODE_MATH_GREATER_THAN, 1.0f, 1.0f},
                                   {"ND_ifgreatereq_float", NODE_MATH_MAXIMUM, 1.0f, 1.0f},
                                   {"ND_ifequal_float", NODE_MATH_COMPARE, 1.0f, 1.0f}};

  for (const ConditionalCase &conditional_case : cases) {
    materialx::Node source_node;
    source_node.name = conditional_case.nodedef;
    source_node.nodedef = conditional_case.nodedef;
    source_node.inputs = {{"value1", conditional_case.value1},
                          {"value2", conditional_case.value2},
                          {"in1", 0.75f},
                          {"in2", 0.25f}};
    source_node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source_node}}, &graph)) << conditional_case.nodedef;

    MathNode *condition = nullptr;
    MathNode *greater = nullptr;
    MathNode *equal = nullptr;
    MathNode *delta = nullptr;
    MathNode *product = nullptr;
    MathNode *sum = nullptr;
    for (ShaderNode *node : graph.nodes) {
      condition = node->name == string(conditional_case.nodedef) + ".condition" ?
                      dynamic_cast<MathNode *>(node) :
                      condition;
      greater = node->name == string(conditional_case.nodedef) + ".greater" ?
                    dynamic_cast<MathNode *>(node) :
                    greater;
      equal = node->name == string(conditional_case.nodedef) + ".equal" ?
                  dynamic_cast<MathNode *>(node) :
                  equal;
      delta = node->name == string(conditional_case.nodedef) + ".delta" ?
                  dynamic_cast<MathNode *>(node) :
                  delta;
      product = node->name == string(conditional_case.nodedef) + ".product" ?
                    dynamic_cast<MathNode *>(node) :
                    product;
      sum = node->name == conditional_case.nodedef ? dynamic_cast<MathNode *>(node) : sum;
    }
    ASSERT_NE(condition, nullptr) << conditional_case.nodedef;
    ASSERT_NE(delta, nullptr) << conditional_case.nodedef;
    ASSERT_NE(product, nullptr) << conditional_case.nodedef;
    ASSERT_NE(sum, nullptr) << conditional_case.nodedef;
    EXPECT_EQ(condition->get_math_type(), conditional_case.condition_type);
    MathNode *predicate = condition;
    if (string(conditional_case.nodedef) == "ND_ifgreatereq_float") {
      ASSERT_NE(greater, nullptr);
      ASSERT_NE(equal, nullptr);
      EXPECT_EQ(greater->get_math_type(), NODE_MATH_GREATER_THAN);
      EXPECT_EQ(equal->get_math_type(), NODE_MATH_COMPARE);
      predicate = greater;
    }
    EXPECT_FLOAT_EQ(predicate->get_value1(), conditional_case.value1);
    EXPECT_FLOAT_EQ(predicate->get_value2(), conditional_case.value2);
    EXPECT_EQ(delta->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(product->get_math_type(), NODE_MATH_MULTIPLY);
    EXPECT_EQ(sum->get_math_type(), NODE_MATH_ADD);
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

TEST(materialx_graph, lowers_round_float_to_native_math)
{
  materialx::Node source_node;
  source_node.name = "Round";
  source_node.nodedef = "ND_round_float";
  source_node.inputs["in"] = 1.25f;
  source_node.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source_node}}, &graph));

  MathNode *round = nullptr;
  for (ShaderNode *node : graph.nodes) {
    round = node->name == "Round" ? dynamic_cast<MathNode *>(node) : round;
  }
  ASSERT_NE(round, nullptr);
  EXPECT_EQ(round->get_math_type(), NODE_MATH_ROUND);
  EXPECT_FLOAT_EQ(round->get_value1(), 1.25f);
}

TEST(materialx_graph, lowers_sqrt_float_to_native_math)
{
  materialx::Node source_node;
  source_node.name = "Sqrt";
  source_node.nodedef = "ND_sqrt_float";
  source_node.inputs["in"] = 2.25f;
  source_node.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source_node}}, &graph));

  MathNode *sqrt = nullptr;
  for (ShaderNode *node : graph.nodes) {
    sqrt = node->name == "Sqrt" ? dynamic_cast<MathNode *>(node) : sqrt;
  }
  ASSERT_NE(sqrt, nullptr);
  EXPECT_EQ(sqrt->get_math_type(), NODE_MATH_SQRT);
  EXPECT_FLOAT_EQ(sqrt->get_value1(), 2.25f);
}

TEST(materialx_graph, rejects_nonfinite_scalar_math_before_mutating_destination)
{
  for (const char *nodedef : {"ND_ln_float",
                               "ND_asin_float",
                               "ND_acos_float"})
  {
    materialx::Node node;
    node.name = nodedef;
    node.nodedef = nodedef;
    node.inputs["in"] = std::numeric_limits<float>::quiet_NaN();
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

TEST(materialx_graph, lowers_domain_sensitive_scalar_math_to_native_nodes)
{
  struct MathCase {
    const char *name;
    const char *nodedef;
    const char *input_name;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"Ln", "ND_ln_float", "in", NODE_MATH_LOGARITHM},
                            {"Asin", "ND_asin_float", "in", NODE_MATH_ARCSINE},
                            {"Acos", "ND_acos_float", "in", NODE_MATH_ARCCOSINE},
                            {"Atan2", "ND_atan2_float", "iny", NODE_MATH_ARCTAN2}};

  for (const MathCase &test_case : cases) {
    materialx::Node node;
    node.name = test_case.name;
    node.nodedef = test_case.nodedef;
    node.inputs[test_case.input_name] = 0.5f;
    if (string(test_case.nodedef) == "ND_atan2_float") {
      node.inputs["inx"] = 0.25f;
    }
    node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << test_case.nodedef;
    MathNode *math = nullptr;
    for (ShaderNode *shader_node : graph.nodes) {
      math = shader_node->name == test_case.name ? dynamic_cast<MathNode *>(shader_node) : math;
    }
    ASSERT_NE(math, nullptr) << test_case.nodedef;
    EXPECT_EQ(math->get_math_type(), test_case.math_type) << test_case.nodedef;
    EXPECT_FLOAT_EQ(math->get_value1(), 0.5f) << test_case.nodedef;
    if (string(test_case.nodedef) == "ND_ln_float") {
      EXPECT_FLOAT_EQ(math->get_value2(), M_E);
    }
    if (string(test_case.nodedef) == "ND_atan2_float") {
      EXPECT_FLOAT_EQ(math->get_value2(), 0.25f);
    }
  }
}

TEST(materialx_graph, lowers_safepower_scalar_and_vector_forms_componentwise)
{
  materialx::Node scalar;
  scalar.name = "SafeFloat";
  scalar.nodedef = "ND_safepower_float";
  scalar.inputs = {{"in1", -2.0f}, {"in2", 3.0f}};
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node vector2;
  vector2.name = "SafeVector2";
  vector2.nodedef = "ND_safepower_vector2";
  vector2.vector2_inputs["in1"] = make_float2(-2.0f, 3.0f);
  vector2.vector2_inputs["in2"] = make_float2(2.0f, 3.0f);
  vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2fa;
  vector2fa.name = "SafeVector2FA";
  vector2fa.nodedef = "ND_safepower_vector2FA";
  vector2fa.links["in1"] = {"SafeVector2", "out", materialx::Type::Vector2};
  vector2fa.inputs["in2"] = 2.0f;
  vector2fa.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3;
  vector3.name = "SafeVector3";
  vector3.nodedef = "ND_safepower_vector3";
  vector3.vector3_inputs["in1"] = make_float3(-2.0f, 3.0f, -4.0f);
  vector3.vector3_inputs["in2"] = make_float3(2.0f, 3.0f, 0.5f);
  vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector3fa;
  vector3fa.name = "SafeVector3FA";
  vector3fa.nodedef = "ND_safepower_vector3FA";
  vector3fa.links["in1"] = {"SafeVector3", "out", materialx::Type::Vector3};
  vector3fa.inputs["in2"] = 2.0f;
  vector3fa.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{scalar, vector2, vector2fa, vector3, vector3fa}}, &graph));

  const auto math_named = [&](const string &name) -> MathNode * {
    for (ShaderNode *node : graph.nodes) {
      if (node->name == name) return dynamic_cast<MathNode *>(node);
    }
    return nullptr;
  };
  ASSERT_NE(math_named("SafeFloat.abs"), nullptr);
  EXPECT_EQ(math_named("SafeFloat.abs")->get_math_type(), NODE_MATH_ABSOLUTE);
  ASSERT_NE(math_named("SafeFloat.sign"), nullptr);
  EXPECT_EQ(math_named("SafeFloat.sign")->get_math_type(), NODE_MATH_SIGN);
  ASSERT_NE(math_named("SafeFloat.power"), nullptr);
  EXPECT_EQ(math_named("SafeFloat.power")->get_math_type(), NODE_MATH_POWER);
  ASSERT_NE(math_named("SafeFloat.multiply"), nullptr);
  EXPECT_EQ(math_named("SafeFloat.multiply")->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(math_named("SafeFloat.abs")->get_value1(), -2.0f);
  EXPECT_FLOAT_EQ(math_named("SafeFloat.power")->get_value2(), 3.0f);

  for (const string &prefix : {"SafeVector2", "SafeVector2FA"}) {
    ASSERT_NE(math_named(prefix + ".X.abs"), nullptr) << prefix;
    ASSERT_NE(math_named(prefix + ".Y.abs"), nullptr) << prefix;
    EXPECT_EQ(math_named(prefix + ".X.power")->get_math_type(), NODE_MATH_POWER) << prefix;
    EXPECT_FLOAT_EQ(math_named(prefix + ".X.power")->get_value2(), 2.0f) << prefix;
  }
  EXPECT_EQ(math_named("SafeVector2.X.abs")->input("Value1")->link, nullptr);
  EXPECT_EQ(math_named("SafeVector2.X.sign")->input("Value1")->link, nullptr);
  EXPECT_EQ(math_named("SafeVector2.X.power")->input("Value2")->link, nullptr);
  EXPECT_FLOAT_EQ(math_named("SafeVector2.X.abs")->get_value1(), -2.0f);
  EXPECT_FLOAT_EQ(math_named("SafeVector2.X.power")->get_value2(), 2.0f);
  for (const string &prefix : {"SafeVector3", "SafeVector3FA"}) {
    ASSERT_NE(math_named(prefix + ".X.abs"), nullptr) << prefix;
    ASSERT_NE(math_named(prefix + ".Y.abs"), nullptr) << prefix;
    ASSERT_NE(math_named(prefix + ".Z.abs"), nullptr) << prefix;
    EXPECT_EQ(math_named(prefix + ".Z.multiply")->get_math_type(), NODE_MATH_MULTIPLY) << prefix;
  }
  EXPECT_EQ(math_named("SafeVector3.Z.abs")->input("Value1")->link, nullptr);
  EXPECT_EQ(math_named("SafeVector3.Z.sign")->input("Value1")->link, nullptr);
  EXPECT_EQ(math_named("SafeVector3.Z.power")->input("Value2")->link, nullptr);
  EXPECT_FLOAT_EQ(math_named("SafeVector3.Z.abs")->get_value1(), -4.0f);
  EXPECT_FLOAT_EQ(math_named("SafeVector3.Z.power")->get_value2(), 0.5f);
}

TEST(materialx_graph, rejects_nonfinite_safepower_vector_literals_before_mutating_destination)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  std::vector<materialx::Node> invalid;

  materialx::Node vector2_first{"Vector2First", "ND_safepower_vector2"};
  vector2_first.vector2_inputs = {{"in1", make_float2(nan, 2.0f)},
                                  {"in2", make_float2(2.0f, 3.0f)}};
  vector2_first.outputs["out"] = materialx::Type::Vector2;
  invalid.push_back(vector2_first);

  materialx::Node vector2_second{"Vector2Second", "ND_safepower_vector2"};
  vector2_second.vector2_inputs = {{"in1", make_float2(-2.0f, 3.0f)},
                                   {"in2", make_float2(2.0f, infinity)}};
  vector2_second.outputs["out"] = materialx::Type::Vector2;
  invalid.push_back(vector2_second);

  materialx::Node vector2_fa{"Vector2FA", "ND_safepower_vector2FA"};
  vector2_fa.vector2_inputs["in1"] = make_float2(-2.0f, 3.0f);
  vector2_fa.inputs["in2"] = nan;
  vector2_fa.outputs["out"] = materialx::Type::Vector2;
  invalid.push_back(vector2_fa);

  materialx::Node vector3_first{"Vector3First", "ND_safepower_vector3"};
  vector3_first.vector3_inputs = {{"in1", make_float3(-2.0f, nan, 4.0f)},
                                  {"in2", make_float3(2.0f, 3.0f, 0.5f)}};
  vector3_first.outputs["out"] = materialx::Type::Vector3;
  invalid.push_back(vector3_first);

  materialx::Node vector3_second{"Vector3Second", "ND_safepower_vector3"};
  vector3_second.vector3_inputs = {{"in1", make_float3(-2.0f, 3.0f, 4.0f)},
                                   {"in2", make_float3(2.0f, 3.0f, infinity)}};
  vector3_second.outputs["out"] = materialx::Type::Vector3;
  invalid.push_back(vector3_second);

  materialx::Node vector3_fa{"Vector3FA", "ND_safepower_vector3FA"};
  vector3_fa.vector3_inputs["in1"] = make_float3(-2.0f, 3.0f, 4.0f);
  vector3_fa.inputs["in2"] = infinity;
  vector3_fa.outputs["out"] = materialx::Type::Vector3;
  invalid.push_back(vector3_fa);

  for (const materialx::Node &node : invalid) {
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower({{node}}, &graph)) << node.nodedef << ": " << node.name;
    EXPECT_EQ(graph.nodes.size(), original_node_count) << node.nodedef << ": " << node.name;
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link)
        << node.nodedef << ": " << node.name;
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

TEST(materialx_graph, lowers_chained_exact_vector3_math_to_native_nodes)
{
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector3";
  first.vector3_inputs["value"] = make_float3(1.0f, 0.0f, 0.0f);
  first.outputs["out"] = materialx::Type::Vector3;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector3";
  second.vector3_inputs["value"] = make_float3(0.0f, 1.0f, 0.0f);
  second.outputs["out"] = materialx::Type::Vector3;

  materialx::Node cross;
  cross.name = "Cross";
  cross.nodedef = "ND_crossproduct_vector3";
  cross.links["in1"] = {"First", "out", materialx::Type::Vector3};
  cross.links["in2"] = {"Second", "out", materialx::Type::Vector3};
  cross.outputs["out"] = materialx::Type::Vector3;

  materialx::Node normalize;
  normalize.name = "Normalize";
  normalize.nodedef = "ND_normalize_vector3";
  normalize.links["in"] = {"Cross", "out", materialx::Type::Vector3};
  normalize.outputs["out"] = materialx::Type::Vector3;

  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector3";
  magnitude.links["in"] = {"Normalize", "out", materialx::Type::Vector3};
  magnitude.outputs["out"] = materialx::Type::Float;

  materialx::Node dot;
  dot.name = "Dot";
  dot.nodedef = "ND_dotproduct_vector3";
  dot.links["in1"] = {"Normalize", "out", materialx::Type::Vector3};
  dot.links["in2"] = {"Second", "out", materialx::Type::Vector3};
  dot.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, cross, normalize, magnitude, dot}}, &graph));

  std::unordered_map<string, VectorMathNode *> math;
  for (ShaderNode *node : graph.nodes) {
    if (auto *candidate = dynamic_cast<VectorMathNode *>(node)) {
      math.emplace(node->name, candidate);
    }
  }
  ASSERT_NE(math["Cross"], nullptr);
  ASSERT_NE(math["Normalize"], nullptr);
  ASSERT_NE(math["Magnitude"], nullptr);
  ASSERT_NE(math["Dot"], nullptr);
  EXPECT_EQ(math["Cross"]->get_math_type(), NODE_VECTOR_MATH_CROSS_PRODUCT);
  EXPECT_EQ(math["Normalize"]->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(math["Magnitude"]->get_math_type(), NODE_VECTOR_MATH_LENGTH);
  EXPECT_EQ(math["Dot"]->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(math["Normalize"]->input("Vector1")->link, math["Cross"]->output("Vector"));
  EXPECT_EQ(math["Magnitude"]->input("Vector1")->link, math["Normalize"]->output("Vector"));
  EXPECT_EQ(math["Dot"]->input("Vector1")->link, math["Normalize"]->output("Vector"));
}

TEST(materialx_graph, lowers_oblique_refract_vector3_with_direct_ior_scale)
{
  materialx::Node refract;
  refract.name = "Refract";
  refract.nodedef = "ND_refract_vector3";
  /* At a 60 degree incident angle, 1.5 and its reciprocal produce different refractions. */
  refract.vector3_inputs["in1"] = make_float3(0.8660254f, 0.0f, -0.5f);
  refract.vector3_inputs["in2"] = make_float3(0.0f, 0.0f, 1.0f);
  refract.inputs["scale"] = 1.5f;
  refract.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{refract}}, &graph));

  VectorMathNode *native_refract = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_refract = node->name == "Refract" ? dynamic_cast<VectorMathNode *>(node) :
                                                  native_refract;
  }
  ASSERT_NE(native_refract, nullptr);
  EXPECT_EQ(native_refract->get_math_type(), NODE_VECTOR_MATH_REFRACT);
  EXPECT_EQ(native_refract->get_vector1(), make_float3(0.8660254f, 0.0f, -0.5f));
  EXPECT_EQ(native_refract->get_vector2(), make_float3(0.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(native_refract->get_scale(), 1.5f);
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

TEST(materialx_graph, lowers_exact_color4_component_arithmetic_batch_with_linked_operands)
{
  const struct Case {
    const char *nodedef;
    NodeMathType math_type;
    float literal_alpha;
    bool linked_second;
  } cases[] = {{"ND_add_color4", NODE_MATH_ADD, 0.0f, true},
               {"ND_subtract_color4", NODE_MATH_SUBTRACT, 0.0f, true},
               {"ND_multiply_color4", NODE_MATH_MULTIPLY, 0.0f, true},
               {"ND_divide_color4", NODE_MATH_DIVIDE, 0.0f, true},
               {"ND_min_color4", NODE_MATH_MINIMUM, 0.0f, true},
               {"ND_max_color4", NODE_MATH_MAXIMUM, 0.0f, true},
               {"ND_modulo_color4", NODE_MATH_MODULO, 0.0f, true},
               {"ND_power_color4", NODE_MATH_POWER, 2.0f, false}};

  const TemporaryImage image_asset;
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;
  materialx::Node image;
  image.name = "Image";
  image.nodedef = "ND_image_color4";
  image.asset_inputs["file"] = image_asset.path();
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Color4;
  materialx::Node rhs;
  rhs.name = "Right";
  rhs.nodedef = "ND_safepower_color4";
  rhs.float4_inputs["in1"] = make_float4(0.5f, 0.75f, 1.25f, 1.5f);
  rhs.float4_inputs["in2"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  rhs.outputs["out"] = materialx::Type::Color4;

  materialx::Graph source;
  source.nodes = {uv, image, rhs};
  const char *previous = "Image";
  for (const Case &test_case : cases) {
    materialx::Node node;
    node.name = test_case.nodedef;
    node.nodedef = test_case.nodedef;
    node.links["in1"] = {previous, "out", materialx::Type::Color4};
    if (test_case.linked_second) {
      node.links["in2"] = {"Right", "out", materialx::Type::Color4};
    }
    else {
      node.float4_inputs["in2"] = make_float4(2.0f, 3.0f, 4.0f, test_case.literal_alpha);
    }
    node.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(node));
    previous = test_case.nodedef;
  }
  materialx::Node extract;
  extract.name = "Alpha";
  extract.nodedef = "ND_extract_color4";
  extract.int_inputs["index"] = 3;
  extract.links["in"] = {previous, "out", materialx::Type::Color4};
  extract.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(std::move(extract));
  materialx::Node convert;
  convert.name = "RGB";
  convert.nodedef = "ND_convert_color4_color3";
  convert.links["in"] = {previous, "out", materialx::Type::Color4};
  convert.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(convert));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *shader_node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      math_nodes[shader_node->name.string()] = math;
    }
  }
  for (const Case &test_case : cases) {
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      MathNode *math = math_nodes[test_case.nodedef + string(".") + channel];
      ASSERT_NE(math, nullptr) << test_case.nodedef << " " << channel;
      EXPECT_EQ(math->get_math_type(), test_case.math_type) << test_case.nodedef << " " << channel;
      if (test_case.linked_second) {
        EXPECT_NE(math->input("Value2")->link, nullptr) << test_case.nodedef << " " << channel;
      }
    }
  }
  EXPECT_FLOAT_EQ(math_nodes["ND_power_color4.Alpha"]->get_value2(), 2.0f);
  EXPECT_EQ(math_nodes["ND_add_color4.Alpha"]->input("Value2")->link,
            math_nodes["Right.Alpha.multiply"]->output("Value"));
  EXPECT_EQ(math_nodes["ND_subtract_color4.Alpha"]->input("Value1")->link,
            math_nodes["ND_add_color4.Alpha"]->output("Value"));
}

TEST(materialx_graph, rejects_invalid_color4_component_arithmetic_without_mutating_destination)
{
  materialx::Node lhs;
  lhs.name = "Left";
  lhs.nodedef = "ND_safepower_color4";
  lhs.float4_inputs["in1"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  lhs.float4_inputs["in2"] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
  lhs.outputs["out"] = materialx::Type::Color4;
  materialx::Node bad;
  bad.name = "BadDivide";
  bad.nodedef = "ND_divide_color4";
  bad.links["in1"] = {"Left", "out", materialx::Type::Color4};
  bad.float4_inputs["in2"] = make_float4(1.0f, 0.0f, 1.0f, 1.0f);
  bad.outputs["out"] = materialx::Type::Color4;
  EXPECT_FALSE(materialx::validate({{lhs, bad}}));
  bad.nodedef = "ND_modulo_color4";
  bad.float4_inputs["in2"] = make_float4(1.0f, 1.0f, 0.0f, 1.0f);
  EXPECT_FALSE(materialx::validate({{lhs, bad}}));
  bad.nodedef = "ND_power_color4";
  bad.float4_inputs["in2"] =
      make_float4(1.0f, 1.0f, 1.0f, std::numeric_limits<float>::infinity());
  EXPECT_FALSE(materialx::validate({{lhs, bad}}));
  bad.nodedef = "ND_divide_color4";
  bad.float4_inputs["in2"] = make_float4(1.0f, 0.0f, 1.0f, 1.0f);
  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{lhs, bad}}, &graph));
  int principled_count = 0;
  for (ShaderNode *shader_node : graph.nodes) {
    if (shader_node->type == PrincipledBsdfNode::get_node_type()) {
      principled_count++;
    }
  }
  EXPECT_EQ(principled_count, 1);
}

TEST(materialx_graph, lowers_exact_color4_component_arithmetic_defaults)
{
  const struct Case {
    const char *nodedef;
    NodeMathType math_type;
    float second_default;
  } cases[] = {{"ND_add_color4", NODE_MATH_ADD, 0.0f},
               {"ND_subtract_color4", NODE_MATH_SUBTRACT, 0.0f},
               {"ND_multiply_color4", NODE_MATH_MULTIPLY, 1.0f},
               {"ND_divide_color4", NODE_MATH_DIVIDE, 1.0f},
               {"ND_min_color4", NODE_MATH_MINIMUM, 0.0f},
               {"ND_max_color4", NODE_MATH_MAXIMUM, 0.0f},
               {"ND_modulo_color4", NODE_MATH_MODULO, 1.0f},
               {"ND_power_color4", NODE_MATH_POWER, 1.0f}};

  for (const Case &test_case : cases) {
    materialx::Node node;
    node.name = test_case.nodedef;
    node.nodedef = test_case.nodedef;
    node.outputs["out"] = materialx::Type::Color4;
    materialx::Graph source{{node}};

    EXPECT_TRUE(materialx::validate(source)) << test_case.nodedef;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower(source, &graph)) << test_case.nodedef;
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      MathNode *found = nullptr;
      const string expected_name = test_case.nodedef + string(".") + channel;
      for (ShaderNode *shader_node : graph.nodes) {
        if (shader_node->name == expected_name) {
          found = dynamic_cast<MathNode *>(shader_node);
          break;
        }
      }
      ASSERT_NE(found, nullptr) << expected_name;
      EXPECT_EQ(found->get_math_type(), test_case.math_type) << expected_name;
      EXPECT_FLOAT_EQ(found->get_value1(), 0.0f) << expected_name;
      EXPECT_FLOAT_EQ(found->get_value2(), test_case.second_default) << expected_name;
    }
  }
}

TEST(materialx_graph, lowers_exact_color4_math_batch_and_preserves_alpha_channel)
{
  const struct UnaryCase {
    const char *nodedef;
    NodeMathType math_type;
  } unary_cases[] = {{"ND_absval_color4", NODE_MATH_ABSOLUTE},
                     {"ND_ceil_color4", NODE_MATH_CEIL},
                     {"ND_floor_color4", NODE_MATH_FLOOR},
                     {"ND_fract_color4", NODE_MATH_FRACTION},
                     {"ND_round_color4", NODE_MATH_ROUND},
                     {"ND_sign_color4", NODE_MATH_SIGN}};

  const TemporaryImage image_asset;
  materialx::Node image;
  image.name = "Image";
  image.nodedef = "ND_image_color4";
  image.asset_inputs["file"] = image_asset.path();
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Color4;

  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Graph source;
  source.nodes = {uv, image};
  for (const UnaryCase &test_case : unary_cases) {
    materialx::Node node;
    node.name = test_case.nodedef;
    node.nodedef = test_case.nodedef;
    node.links["in"] = {"Image", "out", materialx::Type::Color4};
    node.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(node));
  }
  materialx::Node invert;
  invert.name = "ND_invert_color4";
  invert.nodedef = "ND_invert_color4";
  invert.float4_inputs["amount"] = make_float4(1.0f, 0.5f, 0.25f, 0.75f);
  invert.links["in"] = {"ND_sign_color4", "out", materialx::Type::Color4};
  invert.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(std::move(invert));

  materialx::Node safepower;
  safepower.name = "ND_safepower_color4";
  safepower.nodedef = "ND_safepower_color4";
  safepower.links["in1"] = {"ND_invert_color4", "out", materialx::Type::Color4};
  safepower.float4_inputs["in2"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  safepower.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(std::move(safepower));

  materialx::Node extract;
  extract.name = "Alpha";
  extract.nodedef = "ND_extract_color4";
  extract.int_inputs["index"] = 3;
  extract.links["in"] = {"ND_safepower_color4", "out", materialx::Type::Color4};
  extract.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(std::move(extract));

  materialx::Node convert;
  convert.name = "RGB";
  convert.nodedef = "ND_convert_color4_color3";
  convert.links["in"] = {"ND_safepower_color4", "out", materialx::Type::Color4};
  convert.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(convert));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *shader_node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      math_nodes[shader_node->name.string()] = math;
    }
  }
  for (const UnaryCase &test_case : unary_cases) {
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      ASSERT_NE(math_nodes[test_case.nodedef + string(".") + channel], nullptr)
          << test_case.nodedef << " " << channel;
      EXPECT_EQ(math_nodes[test_case.nodedef + string(".") + channel]->get_math_type(),
                test_case.math_type);
    }
  }
  EXPECT_EQ(math_nodes["ND_invert_color4.Alpha"]->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(math_nodes["ND_invert_color4.Alpha"]->get_value1(), 0.75f);
  EXPECT_EQ(math_nodes["ND_safepower_color4.Alpha.abs"]->get_math_type(), NODE_MATH_ABSOLUTE);
  EXPECT_EQ(math_nodes["ND_safepower_color4.Alpha.sign"]->get_math_type(), NODE_MATH_SIGN);
  EXPECT_EQ(math_nodes["ND_safepower_color4.Alpha.power"]->get_math_type(), NODE_MATH_POWER);
  EXPECT_EQ(math_nodes["ND_safepower_color4.Alpha.multiply"]->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(math_nodes["ND_safepower_color4.Alpha.power"]->get_value2(), 5.0f);
}

TEST(materialx_graph, lowers_color4_math_with_exact_materialx_defaults)
{
  const struct UnaryCase {
    const char *nodedef;
    NodeMathType math_type;
  } unary_cases[] = {{"ND_absval_color4", NODE_MATH_ABSOLUTE},
                     {"ND_ceil_color4", NODE_MATH_CEIL},
                     {"ND_floor_color4", NODE_MATH_FLOOR},
                     {"ND_fract_color4", NODE_MATH_FRACTION},
                     {"ND_round_color4", NODE_MATH_ROUND},
                     {"ND_sign_color4", NODE_MATH_SIGN}};

  for (const UnaryCase &test_case : unary_cases) {
    materialx::Node node;
    node.name = test_case.nodedef;
    node.nodedef = test_case.nodedef;
    node.outputs["out"] = materialx::Type::Color4;

    materialx::Graph source{{node}};
    EXPECT_TRUE(materialx::validate(source));

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower(source, &graph)) << test_case.nodedef;

    std::unordered_map<string, MathNode *> math_nodes;
    for (ShaderNode *shader_node : graph.nodes) {
      if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
        math_nodes[shader_node->name.string()] = math;
      }
    }
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      MathNode *math = math_nodes[test_case.nodedef + string(".") + channel];
      ASSERT_NE(math, nullptr) << test_case.nodedef << " " << channel;
      EXPECT_EQ(math->get_math_type(), test_case.math_type);
      EXPECT_FLOAT_EQ(math->get_value1(), 0.0f);
    }
  }

  materialx::Node invert;
  invert.name = "DefaultInvert";
  invert.nodedef = "ND_invert_color4";
  invert.outputs["out"] = materialx::Type::Color4;
  EXPECT_TRUE(materialx::validate({{invert}}));
  ShaderGraph invert_graph;
  ASSERT_TRUE(materialx::lower({{invert}}, &invert_graph));
  for (ShaderNode *shader_node : invert_graph.nodes) {
    MathNode *math = dynamic_cast<MathNode *>(shader_node);
    const string name = shader_node->name.string();
    if (!math || name.rfind("DefaultInvert.", 0) != 0) {
      continue;
    }
    EXPECT_EQ(math->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_FLOAT_EQ(math->get_value1(), 1.0f);
    EXPECT_FLOAT_EQ(math->get_value2(), 0.0f);
  }

  materialx::Node safepower;
  safepower.name = "DefaultSafePower";
  safepower.nodedef = "ND_safepower_color4";
  safepower.outputs["out"] = materialx::Type::Color4;
  EXPECT_TRUE(materialx::validate({{safepower}}));
  ShaderGraph safepower_graph;
  ASSERT_TRUE(materialx::lower({{safepower}}, &safepower_graph));
  for (ShaderNode *shader_node : safepower_graph.nodes) {
    MathNode *math = dynamic_cast<MathNode *>(shader_node);
    if (!math) {
      continue;
    }
    const string name = shader_node->name.string();
    if (name.ends_with(".abs") || name.ends_with(".sign")) {
      EXPECT_FLOAT_EQ(math->get_value1(), 0.0f) << name;
    }
    else if (name.ends_with(".power")) {
      EXPECT_FLOAT_EQ(math->get_value2(), 1.0f) << name;
    }
  }
}

TEST(materialx_graph, rejects_nonfinite_color4_math_without_mutating_destination)
{
  materialx::Node node;
  node.name = "Bad";
  node.nodedef = "ND_absval_color4";
  node.float4_inputs["in"] = make_float4(
      1.0f, std::numeric_limits<float>::infinity(), 2.0f, 3.0f);
  node.outputs["out"] = materialx::Type::Color4;
  EXPECT_FALSE(materialx::validate({{node}}));

  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{node}}, &graph));
  int principled_count = 0;
  for (ShaderNode *shader_node : graph.nodes) {
    if (shader_node->type == PrincipledBsdfNode::get_node_type()) {
      principled_count++;
    }
  }
  EXPECT_EQ(principled_count, 1);
}

TEST(materialx_graph, color4fa_specials_preserve_scalar_broadcast_rgb_and_alpha_semantics)
{
  const struct Case {
    const char *nodedef;
    float second_value;
  } cases[] = {{"ND_invert_color4FA", 0.75f},
                {"ND_safepower_color4FA", 3.0f},
                {"ND_clamp_color4FA", 0.8f}};

  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_invert_color4";
  input.float4_inputs["amount"] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
  input.float4_inputs["in"] = make_float4(0.1f, -0.2f, 0.3f, -0.4f);
  input.outputs["out"] = materialx::Type::Color4;

  materialx::Graph source;
  source.nodes.push_back(input);
  const char *previous = nullptr;
  for (const Case &test_case : cases) {
    materialx::Node node;
    node.name = test_case.nodedef;
    node.nodedef = test_case.nodedef;
    node.links[test_case.nodedef == string("ND_invert_color4FA") ||
                   test_case.nodedef == string("ND_clamp_color4FA") ?
                   "in" :
                   "in1"] = {previous ? previous : "Input", "out", materialx::Type::Color4};
    if (test_case.nodedef == string("ND_invert_color4FA")) {
      node.inputs["amount"] = test_case.second_value;
    }
    else if (test_case.nodedef == string("ND_clamp_color4FA")) {
      node.inputs["low"] = -0.25f;
      node.inputs["high"] = test_case.second_value;
    }
    else {
      node.inputs["in2"] = test_case.second_value;
    }
    node.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(node));
    previous = test_case.nodedef;
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *shader_node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      math_nodes[shader_node->name.string()] = math;
    }
  }

  for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
    SCOPED_TRACE(channel);
    ASSERT_NE(math_nodes[string("ND_invert_color4FA.") + channel], nullptr);
    EXPECT_EQ(math_nodes[string("ND_invert_color4FA.") + channel]->get_math_type(),
              NODE_MATH_SUBTRACT);
    EXPECT_FLOAT_EQ(math_nodes[string("ND_invert_color4FA.") + channel]->get_value1(), 0.75f);
    ASSERT_NE(math_nodes[string("ND_safepower_color4FA.") + channel + ".power"], nullptr);
    EXPECT_EQ(math_nodes[string("ND_safepower_color4FA.") + channel + ".power"]->get_math_type(),
              NODE_MATH_POWER);
    EXPECT_FLOAT_EQ(math_nodes[string("ND_safepower_color4FA.") + channel + ".power"]->get_value2(),
                    3.0f);
    ASSERT_NE(math_nodes[string("ND_clamp_color4FA.") + channel + ".minimum"], nullptr);
    ASSERT_NE(math_nodes[string("ND_clamp_color4FA.") + channel + ".maximum"], nullptr);
    EXPECT_FLOAT_EQ(math_nodes[string("ND_clamp_color4FA.") + channel + ".minimum"]->get_value2(),
                    0.8f);
    EXPECT_FLOAT_EQ(math_nodes[string("ND_clamp_color4FA.") + channel + ".maximum"]->get_value2(),
                    -0.25f);
  }
}

TEST(materialx_graph, rejects_invalid_color4fa_specials_without_mutating_destination)
{
  materialx::Node good;
  good.name = "Good";
  good.nodedef = "ND_invert_color4FA";
  good.float4_inputs["in"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  good.inputs["amount"] = 1.0f;
  good.outputs["out"] = materialx::Type::Color4;

  materialx::Node bad = good;
  bad.name = "Bad";
  bad.nodedef = "ND_clamp_color4FA";
  bad.links["in"] = {"Good", "out", materialx::Type::Color4};
  bad.float4_inputs.clear();
  bad.inputs = {{"low", 0.0f}, {"high", std::numeric_limits<float>::infinity()}};
  EXPECT_FALSE(materialx::validate({{good, bad}}));

  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{good, bad}}, &graph));
  int principled_count = 0;
  for (ShaderNode *shader_node : graph.nodes) {
    if (shader_node->type == PrincipledBsdfNode::get_node_type()) {
      principled_count++;
    }
  }
  EXPECT_EQ(principled_count, 1);
}

TEST(materialx_graph, lowers_bounded_color4_image_rgb_and_alpha_consumers)
{
  const TemporaryImage image_asset;

  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node image;
  image.name = "Image";
  image.nodedef = "ND_image_color4";
  image.asset_inputs["file"] = image_asset.path();
  image.float4_inputs["default"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract;
  extract.name = "Alpha";
  extract.nodedef = "ND_extract_color4";
  extract.int_inputs["index"] = 3;
  extract.links["in"] = {"Image", "out", materialx::Type::Color4};
  extract.outputs["out"] = materialx::Type::Float;

  materialx::Node green;
  green.name = "Green";
  green.nodedef = "ND_extract_color4";
  green.int_inputs["index"] = 1;
  green.links["in"] = {"Image", "out", materialx::Type::Color4};
  green.outputs["out"] = materialx::Type::Float;

  materialx::Node convert;
  convert.name = "RGB";
  convert.nodedef = "ND_convert_color4_color3";
  convert.links["in"] = {"Image", "out", materialx::Type::Color4};
  convert.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"RGB", "out", materialx::Type::Color3};
  surface.links["specular_roughness"] = {"Alpha", "out", materialx::Type::Float};
  surface.links["base_metalness"] = {"Green", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Graph source;
  source.nodes = {uv, image, extract, green, convert, surface};
  EXPECT_TRUE(materialx::validate(source));
  EXPECT_FLOAT_EQ(image.float4_inputs.at("default").w, 0.4f);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  UVMapNode *uv_map = nullptr;
  ImageTextureNode *image_texture = nullptr;
  SeparateColorNode *rgb = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    uv_map = uv_map ? uv_map : dynamic_cast<UVMapNode *>(node);
    image_texture = image_texture ? image_texture : dynamic_cast<ImageTextureNode *>(node);
    rgb = rgb ? rgb : dynamic_cast<SeparateColorNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(uv_map, nullptr);
  ASSERT_NE(image_texture, nullptr);
  ASSERT_NE(rgb, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(image_texture->get_filename(), ustring(image_asset.path()));
  EXPECT_EQ(image_texture->input("Vector")->link, uv_map->output("UV"));
  EXPECT_EQ(rgb->input("Color")->link, image_texture->output("Color"));
  EXPECT_EQ(graph.nodes.size(), 5);
  EXPECT_EQ(principled->input("Base Color")->link, image_texture->output("Color"));
  EXPECT_EQ(principled->input("Roughness")->link, image_texture->output("Alpha"));
  EXPECT_EQ(principled->input("Metallic")->link, rgb->output("Green"));

  extract.links["in"].type = materialx::Type::Color3;
  source.nodes[2] = extract;
  EXPECT_FALSE(materialx::validate(source));

  extract.links["in"].type = materialx::Type::Color4;
  extract.inputs["unexpected"] = 1.0f;
  source.nodes[2] = extract;
  EXPECT_FALSE(materialx::validate(source));

  materialx::Node non_image;
  non_image.name = "NotAnImage";
  non_image.nodedef = "ND_constant_float";
  non_image.inputs["value"] = 1.0f;
  non_image.float4_inputs["default"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  non_image.outputs["out"] = materialx::Type::Float;
  EXPECT_FALSE(materialx::validate({{non_image}}));

  materialx::Node color3;
  color3.name = "Color3";
  color3.nodedef = "ND_constant_color3";
  color3.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  color3.outputs["out"] = materialx::Type::Color3;
  convert.links["in"] = {"Color3", "out", materialx::Type::Color4};
  source.nodes = {color3, convert};
  EXPECT_FALSE(materialx::validate(source));
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

TEST(materialx_graph, lowers_chained_color3_scalar_math_to_native_mix_nodes)
{
  struct MathCase {
    const char *name;
    const char *nodedef;
    NodeMix mix_type;
  };
  const MathCase cases[] = {{"Add", "ND_add_color3FA", NODE_MIX_ADD},
                            {"Subtract", "ND_subtract_color3FA", NODE_MIX_SUB},
                            {"Multiply", "ND_multiply_color3FA", NODE_MIX_MUL},
                            {"Divide", "ND_divide_color3FA", NODE_MIX_DIV},
                            {"Minimum", "ND_min_color3FA", NODE_MIX_DARK},
                            {"Maximum", "ND_max_color3FA", NODE_MIX_LIGHT}};
  materialx::Graph source;
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(color));
  for (size_t index = 0; index < std::size(cases); index++) {
    materialx::Node scalar;
    scalar.name = string(cases[index].name) + "Scalar";
    scalar.nodedef = "ND_constant_float";
    scalar.inputs["value"] = 0.25f + float(index) * 0.1f;
    scalar.outputs["out"] = materialx::Type::Float;
    source.nodes.push_back(std::move(scalar));
    materialx::Node math;
    math.name = cases[index].name;
    math.nodedef = cases[index].nodedef;
    math.links["in1"] = {index == 0 ? "Color" : cases[index - 1].name,
                           "out",
                           materialx::Type::Color3};
    math.links["in2"] = {string(cases[index].name) + "Scalar", "out", materialx::Type::Float};
    math.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(math));
  }
  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"Maximum", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;
  source.nodes.push_back(std::move(surface));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, MixNode *> math;
  std::unordered_map<string, CombineColorNode *> broadcasts;
  for (ShaderNode *node : graph.nodes) {
    if (auto *candidate = dynamic_cast<MixNode *>(node)) math.emplace(node->name, candidate);
    if (auto *candidate = dynamic_cast<CombineColorNode *>(node)) broadcasts.emplace(node->name, candidate);
  }
  for (size_t index = 0; index < std::size(cases); index++) {
    MixNode *mix = math[cases[index].name];
    CombineColorNode *broadcast = broadcasts[string(cases[index].name) + ".scalar"];
    ASSERT_NE(mix, nullptr) << cases[index].nodedef;
    ASSERT_NE(broadcast, nullptr) << cases[index].nodedef;
    EXPECT_EQ(mix->get_mix_type(), cases[index].mix_type);
    EXPECT_EQ(mix->input("Color2")->link, broadcast->output("Color"));
    if (index > 0) {
      EXPECT_EQ(mix->input("Color1")->link, math[cases[index - 1].name]->output("Color"));
    }
  }
}

TEST(materialx_graph, lowers_chained_color3_modulo_and_power_componentwise)
{
  materialx::Node first{"First", "ND_constant_color3"}; first.color3_inputs["value"] = make_float3(5.5f, 6.5f, 7.5f); first.outputs["out"] = materialx::Type::Color3;
  materialx::Node second{"Second", "ND_constant_color3"}; second.color3_inputs["value"] = make_float3(2.0f, 3.0f, 4.0f); second.outputs["out"] = materialx::Type::Color3;
  materialx::Node modulo; modulo.name = "Modulo"; modulo.nodedef = "ND_modulo_color3"; modulo.links["in1"] = {"First", "out", materialx::Type::Color3}; modulo.links["in2"] = {"Second", "out", materialx::Type::Color3}; modulo.outputs["out"] = materialx::Type::Color3;
  materialx::Node power; power.name = "Power"; power.nodedef = "ND_power_color3"; power.links["in1"] = {"Modulo", "out", materialx::Type::Color3}; power.links["in2"] = {"Second", "out", materialx::Type::Color3}; power.outputs["out"] = materialx::Type::Color3;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{first, second, modulo, power}}, &graph));
  int modulo_count = 0, power_count = 0;
  for (ShaderNode *node : graph.nodes) if (const auto *math = dynamic_cast<MathNode *>(node)) { modulo_count += math->get_math_type() == NODE_MATH_MODULO; power_count += math->get_math_type() == NODE_MATH_POWER; }
  EXPECT_EQ(modulo_count, 3); EXPECT_EQ(power_count, 3);
}

TEST(materialx_graph, lowers_color3_safepower_with_negative_channels)
{
  materialx::Node first{"First", "ND_constant_color3"}; first.color3_inputs["value"] = make_float3(-2.0f, -3.0f, 4.0f); first.outputs["out"] = materialx::Type::Color3;
  materialx::Node exponent{"Exponent", "ND_constant_color3"}; exponent.color3_inputs["value"] = make_float3(2.0f, 3.0f, 0.5f); exponent.outputs["out"] = materialx::Type::Color3;
  materialx::Node safe; safe.name="Safe"; safe.nodedef="ND_safepower_color3"; safe.links["in1"]={"First","out",materialx::Type::Color3}; safe.links["in2"]={"Exponent","out",materialx::Type::Color3}; safe.outputs["out"]=materialx::Type::Color3;
  ShaderGraph graph; ASSERT_TRUE(materialx::lower({{first, exponent, safe}}, &graph));
  int abs_count=0, sign_count=0, power_count=0, multiply_count=0; for(ShaderNode *node:graph.nodes) if(const auto *math=dynamic_cast<MathNode *>(node)){ abs_count+=math->get_math_type()==NODE_MATH_ABSOLUTE; sign_count+=math->get_math_type()==NODE_MATH_SIGN; power_count+=math->get_math_type()==NODE_MATH_POWER; multiply_count+=math->get_math_type()==NODE_MATH_MULTIPLY; }
  EXPECT_EQ(abs_count,3); EXPECT_EQ(sign_count,3); EXPECT_EQ(power_count,3); EXPECT_EQ(multiply_count,3);
}

TEST(materialx_graph, lowers_color3fa_invert_and_safepower_literal_link_boundaries)
{
  materialx::Node color{"Color", "ND_constant_color3"};
  color.color3_inputs["value"] = make_float3(-2.0f, 3.0f, -4.0f);
  color.outputs["out"] = materialx::Type::Color3;
  materialx::Node scalar{"Scalar", "ND_constant_float"};
  scalar.inputs["value"] = 2.25f;
  scalar.outputs["out"] = materialx::Type::Float;
  materialx::Node invert{"Invert", "ND_invert_color3FA"};
  invert.links["in"] = {"Color", "out", materialx::Type::Color3};
  invert.inputs["amount"] = 0.625f;
  invert.outputs["out"] = materialx::Type::Color3;
  materialx::Node safe{"Safe", "ND_safepower_color3FA"};
  safe.color3_inputs["in1"] = make_float3(-2.0f, 3.0f, -4.0f);
  safe.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  safe.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, scalar, invert, safe}}, &graph));
  MixNode *mix = nullptr;
  CombineColorNode *broadcast = nullptr;
  ValueNode *exponent = nullptr;
  for (ShaderNode *node : graph.nodes) {
    mix = node->name == "Invert" ? dynamic_cast<MixNode *>(node) : mix;
    broadcast = node->name == "Invert.scalar" ? dynamic_cast<CombineColorNode *>(node) : broadcast;
    exponent = node->name == "Scalar" ? dynamic_cast<ValueNode *>(node) : exponent;
  }
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(broadcast, nullptr);
  ASSERT_NE(exponent, nullptr);
  EXPECT_FLOAT_EQ(broadcast->get_r(), 0.625f);
  EXPECT_EQ(mix->input("Color1")->link, broadcast->output("Color"));
  for (const char *channel : {"Red", "Green", "Blue"}) {
    MathNode *power = nullptr;
    for (ShaderNode *node : graph.nodes) {
      power = node->name == string("Safe.") + channel + ".power" ?
                  dynamic_cast<MathNode *>(node) :
                  power;
    }
    ASSERT_NE(power, nullptr);
    EXPECT_EQ(power->input("Value2")->link, exponent->output("Value"));
  }
}

TEST(materialx_graph, rejects_nonfinite_color3fa_scalars_without_mutation)
{
  for (const char *nodedef : {"ND_invert_color3FA", "ND_safepower_color3FA"}) {
    materialx::Node node{"Invalid", nodedef};
    if (string(nodedef) == "ND_invert_color3FA") {
      node.color3_inputs["in"] = make_float3(0.1f, 0.2f, 0.3f);
      node.inputs["amount"] = std::numeric_limits<float>::infinity();
    }
    else {
      node.color3_inputs["in1"] = make_float3(-2.0f, 3.0f, -4.0f);
      node.inputs["in2"] = std::numeric_limits<float>::quiet_NaN();
    }
    node.outputs["out"] = materialx::Type::Color3;
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{node}}, &graph));
    EXPECT_EQ(graph.nodes.size(), count);
    EXPECT_EQ(graph.output()->input("Surface")->link, sentinel->output("Emission"));
  }
}

TEST(materialx_graph, rejects_linked_nonfinite_color3fa_scalar_constants_without_mutation)
{
  for (const auto &[nodedef, scalar_input, invalid] :
       {std::tuple{"ND_invert_color3FA",
                   "amount",
                   std::numeric_limits<float>::infinity()},
        std::tuple{"ND_safepower_color3FA",
                   "in2",
                   std::numeric_limits<float>::quiet_NaN()}})
  {
    materialx::Node scalar{"InvalidScalar", "ND_constant_float"};
    scalar.inputs["value"] = invalid;
    scalar.outputs["out"] = materialx::Type::Float;
    materialx::Node node{"Invalid", nodedef};
    node.links[scalar_input] = {"InvalidScalar", "out", materialx::Type::Float};
    if (string(nodedef) == "ND_invert_color3FA") {
      node.color3_inputs["in"] = make_float3(0.1f, 0.2f, 0.3f);
    }
    else {
      node.color3_inputs["in1"] = make_float3(-2.0f, 3.0f, -4.0f);
    }
    node.outputs["out"] = materialx::Type::Color3;
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{scalar, node}}, &graph)) << nodedef;
    EXPECT_EQ(graph.nodes.size(), count) << nodedef;
    EXPECT_EQ(graph.output()->input("Surface")->link, sentinel->output("Emission")) << nodedef;
  }
}

TEST(materialx_graph, credits_binary_vector_and_color_minmax_batch)
{
  struct VectorCase {
    const char *id;
    materialx::Type type;
    NodeVectorMathType operation;
  };
  const VectorCase cases[] = {
      {"ND_subtract_vector2", materialx::Type::Vector2, NODE_VECTOR_MATH_SUBTRACT},
      {"ND_multiply_vector2", materialx::Type::Vector2, NODE_VECTOR_MATH_MULTIPLY},
      {"ND_divide_vector2", materialx::Type::Vector2, NODE_VECTOR_MATH_DIVIDE},
      {"ND_subtract_vector3", materialx::Type::Vector3, NODE_VECTOR_MATH_SUBTRACT},
      {"ND_multiply_vector3", materialx::Type::Vector3, NODE_VECTOR_MATH_MULTIPLY},
      {"ND_divide_vector3", materialx::Type::Vector3, NODE_VECTOR_MATH_DIVIDE}};
  for (const VectorCase &test_case : cases) {
    materialx::Node literal{"Literal", test_case.id};
    literal.outputs["out"] = test_case.type;
    if (test_case.type == materialx::Type::Vector2) {
      literal.vector2_inputs["in1"] = make_float2(8.0f, 12.0f);
      literal.vector2_inputs["in2"] = make_float2(2.0f, 3.0f);
    }
    else {
      literal.vector3_inputs["in1"] = make_float3(8.0f, 12.0f, 16.0f);
      literal.vector3_inputs["in2"] = make_float3(2.0f, 3.0f, 4.0f);
    }
    ShaderGraph literal_graph;
    ASSERT_TRUE(materialx::lower({{literal}}, &literal_graph)) << test_case.id;
    VectorMathNode *literal_math = nullptr;
    for (ShaderNode *node : literal_graph.nodes) {
      literal_math = node->name == "Literal" ? dynamic_cast<VectorMathNode *>(node) :
                                               literal_math;
    }
    ASSERT_NE(literal_math, nullptr) << test_case.id;
    EXPECT_EQ(literal_math->get_math_type(), test_case.operation);

    materialx::Node first{"First",
                          test_case.type == materialx::Type::Vector2 ?
                              "ND_constant_vector2" :
                              "ND_constant_vector3"};
    materialx::Node second = first;
    second.name = "Second";
    if (test_case.type == materialx::Type::Vector2) {
      first.vector2_inputs["value"] = make_float2(8.0f, 12.0f);
      second.vector2_inputs["value"] = make_float2(2.0f, 3.0f);
    }
    else {
      first.vector3_inputs["value"] = make_float3(8.0f, 12.0f, 16.0f);
      second.vector3_inputs["value"] = make_float3(2.0f, 3.0f, 4.0f);
    }
    first.outputs["out"] = test_case.type;
    second.outputs["out"] = test_case.type;
    materialx::Node linked{"Linked", test_case.id};
    linked.links["in1"] = {"First", "out", test_case.type};
    const bool divide = string(test_case.id).find("divide") != string::npos;
    if (divide) {
      if (test_case.type == materialx::Type::Vector2) {
        linked.vector2_inputs["in2"] = make_float2(2.0f, 3.0f);
      }
      else {
        linked.vector3_inputs["in2"] = make_float3(2.0f, 3.0f, 4.0f);
      }
    }
    else {
      linked.links["in2"] = {"Second", "out", test_case.type};
    }
    linked.outputs["out"] = test_case.type;
    ShaderGraph linked_graph;
    ASSERT_TRUE(materialx::lower({{first, second, linked}}, &linked_graph)) << test_case.id;
    VectorMathNode *linked_math = nullptr;
    for (ShaderNode *node : linked_graph.nodes) {
      linked_math = node->name == "Linked" ? dynamic_cast<VectorMathNode *>(node) : linked_math;
    }
    ASSERT_NE(linked_math, nullptr);
    EXPECT_NE(linked_math->input("Vector1")->link, nullptr);
    if (divide) {
      EXPECT_EQ(linked_math->input("Vector2")->link, nullptr);
    }
    else {
      EXPECT_NE(linked_math->input("Vector2")->link, nullptr);
    }
  }

  for (const auto &[id, operation] :
       {std::pair{"ND_min_color3", NODE_MIX_DARK},
        std::pair{"ND_max_color3", NODE_MIX_LIGHT}})
  {
    materialx::Node first{"First", "ND_constant_color3"};
    first.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
    first.outputs["out"] = materialx::Type::Color3;
    materialx::Node second = first;
    second.name = "Second";
    second.color3_inputs["value"] = make_float3(0.7f, 0.3f, 0.5f);
    materialx::Node node{"ColorMath", id};
    node.links["in1"] = {"First", "out", materialx::Type::Color3};
    node.links["in2"] = {"Second", "out", materialx::Type::Color3};
    node.outputs["out"] = materialx::Type::Color3;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{first, second, node}}, &graph));
    MixNode *mix = nullptr;
    for (ShaderNode *lowered : graph.nodes) {
      mix = lowered->name == "ColorMath" ? dynamic_cast<MixNode *>(lowered) : mix;
    }
    ASSERT_NE(mix, nullptr);
    EXPECT_EQ(mix->get_mix_type(), operation);
    EXPECT_FLOAT_EQ(mix->get_fac(), 1.0f);
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

TEST(materialx_graph, lowers_color3_vector3_component_construction_chain)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node color_to_vector;
  color_to_vector.name = "ColorToVector";
  color_to_vector.nodedef = "ND_convert_color3_vector3";
  color_to_vector.links["in"] = {"Color", "out", materialx::Type::Color3};
  color_to_vector.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector_to_color;
  vector_to_color.name = "VectorToColor";
  vector_to_color.nodedef = "ND_convert_vector3_color3";
  vector_to_color.links["in"] = {"ColorToVector", "out", materialx::Type::Vector3};
  vector_to_color.outputs["out"] = materialx::Type::Color3;

  materialx::Node separate;
  separate.name = "Separate";
  separate.nodedef = "ND_separate3_color3";
  separate.links["in"] = {"VectorToColor", "out", materialx::Type::Color3};
  separate.outputs = {{"outx", materialx::Type::Float},
                      {"outy", materialx::Type::Float},
                      {"outz", materialx::Type::Float}};

  materialx::Node combine;
  combine.name = "Combine";
  combine.nodedef = "ND_combine3_color3";
  combine.links = {{"in1", {"Separate", "outx", materialx::Type::Float}},
                   {"in2", {"Separate", "outy", materialx::Type::Float}},
                   {"in3", {"Separate", "outz", materialx::Type::Float}}};
  combine.outputs["out"] = materialx::Type::Color3;

  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 0.75f;
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node scalar_to_vector;
  scalar_to_vector.name = "ScalarToVector";
  scalar_to_vector.nodedef = "ND_convert_float_vector3";
  scalar_to_vector.links["in"] = {"Scalar", "out", materialx::Type::Float};
  scalar_to_vector.outputs["out"] = materialx::Type::Vector3;

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"Combine", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{color, color_to_vector, vector_to_color, separate, combine, scalar, scalar_to_vector, surface}},
      &graph));

  CombineXYZNode *color_to_vector_node = nullptr;
  CombineColorNode *vector_to_color_node = nullptr;
  SeparateColorNode *separate_node = nullptr;
  CombineColorNode *combine_node = nullptr;
  CombineXYZNode *scalar_to_vector_node = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color_to_vector_node = node->name == "ColorToVector" ? dynamic_cast<CombineXYZNode *>(node) :
                                                        color_to_vector_node;
    vector_to_color_node = node->name == "VectorToColor" ? dynamic_cast<CombineColorNode *>(node) :
                                                        vector_to_color_node;
    separate_node = node->name == "Separate" ? dynamic_cast<SeparateColorNode *>(node) : separate_node;
    combine_node = node->name == "Combine" ? dynamic_cast<CombineColorNode *>(node) : combine_node;
    scalar_to_vector_node = node->name == "ScalarToVector" ? dynamic_cast<CombineXYZNode *>(node) :
                                                               scalar_to_vector_node;
  }
  ASSERT_NE(color_to_vector_node, nullptr);
  ASSERT_NE(vector_to_color_node, nullptr);
  ASSERT_NE(separate_node, nullptr);
  ASSERT_NE(combine_node, nullptr);
  ASSERT_NE(scalar_to_vector_node, nullptr);
  EXPECT_EQ(separate_node->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(combine_node->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(combine_node->input("Red")->link, separate_node->output("Red"));
  EXPECT_EQ(combine_node->input("Green")->link, separate_node->output("Green"));
  EXPECT_EQ(combine_node->input("Blue")->link, separate_node->output("Blue"));
  EXPECT_EQ(scalar_to_vector_node->input("X")->link, scalar_to_vector_node->input("Y")->link);
  EXPECT_EQ(scalar_to_vector_node->input("Y")->link, scalar_to_vector_node->input("Z")->link);
}

TEST(materialx_graph, lowers_exact_unary_color3_nodes)
{
  materialx::Node input;
  input.name = "Input"; input.nodedef = "ND_constant_color3";
  input.color3_inputs["value"] = make_float3(-1.25f, 2.75f, -0.5f);
  input.outputs["out"] = materialx::Type::Color3;
  const char *ids[] = {"ND_absval_color3", "ND_floor_color3", "ND_ceil_color3", "ND_fract_color3", "ND_round_color3", "ND_sign_color3"};
  const NodeMathType types[] = {NODE_MATH_ABSOLUTE, NODE_MATH_FLOOR, NODE_MATH_CEIL, NODE_MATH_FRACTION, NODE_MATH_ROUND, NODE_MATH_SIGN};
  materialx::Graph source; source.nodes.push_back(input);
  string previous = "Input";
  for (int i = 0; i < 6; i++) {
    materialx::Node node; node.name = ids[i]; node.nodedef = ids[i]; node.links["in"] = {previous, "out", materialx::Type::Color3}; node.outputs["out"] = materialx::Type::Color3; source.nodes.push_back(node); previous = ids[i];
  }
  ShaderGraph graph; ASSERT_TRUE(materialx::lower(source, &graph));
  for (const NodeMathType type : types) {
    int count = 0; for (ShaderNode *node : graph.nodes) if (const auto *math = dynamic_cast<MathNode *>(node)) count += math->get_math_type() == type;
    EXPECT_EQ(count, 3) << type;
  }
}

TEST(materialx_graph, lowers_color3_conditionals_with_exact_boundary_predicates)
{
  const char *ids[] = {"ND_ifgreater_color3", "ND_ifgreatereq_color3", "ND_ifequal_color3"};
  const NodeMathType predicates[] = {NODE_MATH_GREATER_THAN, NODE_MATH_MAXIMUM, NODE_MATH_COMPARE};
  for (int i = 0; i < 3; i++) {
    materialx::Node node; node.name = ids[i]; node.nodedef = ids[i];
    node.inputs = {{"value1", 1.0f}, {"value2", 1.0f}};
    node.color3_inputs = {{"in1", make_float3(0.8f, 0.4f, 0.2f)}, {"in2", make_float3(0.1f, 0.3f, 0.5f)}};
    node.outputs["out"] = materialx::Type::Color3;
    ShaderGraph graph; ASSERT_TRUE(materialx::lower({{node}}, &graph));
    bool found = false; for (ShaderNode *shader_node : graph.nodes) if (const auto *math = dynamic_cast<MathNode *>(shader_node)) found |= math->get_math_type() == predicates[i];
    EXPECT_TRUE(found) << ids[i];
  }
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

TEST(materialx_graph, lowers_noise3d_contract_forms_with_post_noise_transforms)
{
  materialx::Node position{"Position", "ND_constant_vector3"}; position.vector3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f); position.outputs["out"] = materialx::Type::Vector3;
  const struct { const char *id; materialx::Type type; bool vector_amplitude; } cases[] = {{"ND_noise3d_float", materialx::Type::Float, false}, {"ND_noise3d_color3", materialx::Type::Color3, true}, {"ND_noise3d_color3FA", materialx::Type::Color3, false}};
  for (const auto &test : cases) {
    materialx::Node noise{"Noise", test.id}; noise.inputs["pivot"] = 0.25f; if (test.vector_amplitude) noise.vector3_inputs["amplitude"] = make_float3(0.5f, 0.75f, 1.0f); else noise.inputs["amplitude"] = 0.5f; noise.links["position"] = {"Position", "out", materialx::Type::Vector3}; noise.outputs["out"] = test.type;
    ShaderGraph graph; ASSERT_TRUE(materialx::lower({{position, noise}}, &graph)) << test.id;
    NoiseTextureNode *texture = nullptr; for (ShaderNode *node : graph.nodes) texture = texture ? texture : dynamic_cast<NoiseTextureNode *>(node);
    ASSERT_NE(texture, nullptr) << test.id; EXPECT_EQ(texture->get_dimensions(), 3) << test.id;
  }
}

TEST(materialx_graph, lowers_homogeneous_fractal2d_contracts)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  const struct {
    const char *id;
    materialx::Type type;
    bool scalar_amplitude;
    int components;
  } cases[] = {{"ND_fractal2d_float", materialx::Type::Float, true, 1},
               {"ND_fractal2d_color3", materialx::Type::Color3, false, 3},
               {"ND_fractal2d_color3FA", materialx::Type::Color3, true, 3},
               {"ND_fractal2d_vector2", materialx::Type::Vector2, false, 2},
               {"ND_fractal2d_vector2FA", materialx::Type::Vector2, true, 2},
               {"ND_fractal2d_vector3", materialx::Type::Vector3, false, 3},
               {"ND_fractal2d_vector3FA", materialx::Type::Vector3, true, 3}};

  for (const auto &test : cases) {
    materialx::Node fractal{"Fractal", test.id};
    fractal.int_inputs["octaves"] = 5;
    fractal.inputs["lacunarity"] = 2.75f;
    fractal.inputs["diminish"] = 0.625f;
    if (test.scalar_amplitude) {
      fractal.inputs["amplitude"] = 0.5f;
    }
    else if (test.components == 2) {
      fractal.vector2_inputs["amplitude"] = make_float2(0.5f, 0.75f);
    }
    else {
      fractal.vector3_inputs["amplitude"] = make_float3(0.5f, 0.75f, 1.0f);
    }
    fractal.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
    fractal.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, fractal}}, &graph)) << test.id;
    NoiseTextureNode *texture = nullptr;
    ShaderNode *lowered = nullptr;
    MixNode *color_amplitude = nullptr;
    SeparateColorNode *vector_separate = nullptr;
    for (ShaderNode *node : graph.nodes) {
      texture = texture ? texture : dynamic_cast<NoiseTextureNode *>(node);
      lowered = node->name == "Fractal" ? node : lowered;
      color_amplitude = node->name == "Fractal.amplitude" ? dynamic_cast<MixNode *>(node) :
                                                             color_amplitude;
      vector_separate = node->name == "Fractal.separate" ? dynamic_cast<SeparateColorNode *>(node) :
                                                            vector_separate;
    }
    ASSERT_NE(texture, nullptr) << test.id;
    ASSERT_NE(lowered, nullptr) << test.id;
    EXPECT_EQ(texture->get_dimensions(), 2) << test.id;
    EXPECT_EQ(texture->get_type(), NODE_NOISE_FBM) << test.id;
    EXPECT_FLOAT_EQ(texture->get_detail(), 5.0f) << test.id;
    EXPECT_FLOAT_EQ(texture->get_lacunarity(), 2.75f) << test.id;
    EXPECT_FLOAT_EQ(texture->get_roughness(), 0.625f) << test.id;
    ASSERT_NE(texture->input("Vector")->link, nullptr) << test.id;
    EXPECT_NE(texture->input("Vector")->link->parent, nullptr) << test.id;

    if (test.type == materialx::Type::Float) {
      MathNode *amplitude = dynamic_cast<MathNode *>(lowered);
      ASSERT_NE(amplitude, nullptr) << test.id;
      EXPECT_EQ(amplitude->get_math_type(), NODE_MATH_MULTIPLY) << test.id;
      EXPECT_FLOAT_EQ(amplitude->get_value2(), 0.5f) << test.id;
      EXPECT_EQ(texture->output("Fac")->links[0], amplitude->input("Value1")) << test.id;
      continue;
    }
    if (test.type == materialx::Type::Color3) {
      color_amplitude = dynamic_cast<MixNode *>(lowered);
      ASSERT_NE(color_amplitude, nullptr) << test.id;
      EXPECT_EQ(texture->output("Color")->links[0], color_amplitude->input("Color1")) << test.id;
      EXPECT_EQ(color_amplitude->get_color2(), test.scalar_amplitude ? make_float3(0.5f) :
                                                                       make_float3(0.5f, 0.75f, 1.0f))
          << test.id;
      continue;
    }

    ASSERT_NE(vector_separate, nullptr) << test.id;
    EXPECT_EQ(texture->output("Color")->links[0], vector_separate->input("Color")) << test.id;
    for (const auto &[channel, source, expected_amplitude] :
         {std::tuple{"X", "Red", 0.5f},
          std::tuple{"Y", "Green", test.scalar_amplitude ? 0.5f : 0.75f},
          std::tuple{"Z", "Blue", test.scalar_amplitude ? 0.5f : 1.0f}})
    {
      if (test.components == 2 && channel[0] == 'Z') {
        EXPECT_EQ(lowered->input("Z")->link, nullptr) << test.id;
        continue;
      }
      MathNode *amplitude = nullptr;
      for (ShaderNode *node : graph.nodes) {
        amplitude = node->name == string("Fractal.") + channel + ".amplitude" ?
                        dynamic_cast<MathNode *>(node) :
                        amplitude;
      }
      ASSERT_NE(amplitude, nullptr) << test.id << "." << channel;
      EXPECT_EQ(amplitude->get_math_type(), NODE_MATH_MULTIPLY) << test.id;
      EXPECT_FLOAT_EQ(amplitude->get_value2(), expected_amplitude) << test.id;
      EXPECT_EQ(vector_separate->output(source)->links[0], amplitude->input("Value1")) << test.id;
      EXPECT_EQ(amplitude->output("Value")->links[0], lowered->input(channel)) << test.id;
    }
  }
}

TEST(materialx_graph, rejects_invalid_fractal2d_contracts_atomically)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  const struct {
    const char *id;
    bool vector2;
  } cases[] = {{"ND_fractal2d_float", false},
               {"ND_fractal2d_color3", false},
               {"ND_fractal2d_color3FA", false},
               {"ND_fractal2d_vector2", true},
               {"ND_fractal2d_vector2FA", true},
               {"ND_fractal2d_vector3", false},
               {"ND_fractal2d_vector3FA", false}};

  for (const auto &test : cases) {
    materialx::Node fractal{"Fractal", test.id};
    fractal.int_inputs["octaves"] = 0;
    fractal.inputs["lacunarity"] = std::numeric_limits<float>::infinity();
    fractal.inputs["diminish"] = 0.5f;
    const bool scalar_amplitude = string(test.id).find("FA") != string::npos ||
                                  string(test.id).find("float") != string::npos;
    if (scalar_amplitude) {
      fractal.inputs["amplitude"] = 1.0f;
    }
    else if (test.vector2) {
      fractal.vector2_inputs["amplitude"] = make_float2(1.0f, 1.0f);
    }
    else {
      fractal.vector3_inputs["amplitude"] = make_float3(1.0f, 1.0f, 1.0f);
    }
    fractal.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
    fractal.outputs["out"] = string(test.id).find("float") != string::npos ? materialx::Type::Float :
                              string(test.id).find("color3") != string::npos ? materialx::Type::Color3 :
                              test.vector2 ? materialx::Type::Vector2 : materialx::Type::Vector3;

    EXPECT_FALSE(materialx::validate({{texcoord, fractal}})) << test.id;
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower({{texcoord, fractal}}, &graph)) << test.id;
    EXPECT_EQ(graph.nodes.size(), original_node_count) << test.id;
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link) << test.id;
  }
}

TEST(materialx_graph, lowers_cellnoise_family_to_native_white_noise)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;
  materialx::Node position;
  position.name = "Position";
  position.nodedef = "ND_constant_vector3";
  position.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  position.outputs["out"] = materialx::Type::Vector3;

  const struct {
    const char *id;
    const char *input_name;
    const char *source_name;
    materialx::Type input_type;
    int dimensions;
  } cases[] = {{"ND_cellnoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, 2},
               {"ND_cellnoise3d_float", "position", "Position", materialx::Type::Vector3, 3}};

  for (const auto &test : cases) {
    materialx::Node cellnoise;
    cellnoise.name = "CellNoise";
    cellnoise.nodedef = test.id;
    cellnoise.links[test.input_name] = {test.source_name, "out", test.input_type};
    cellnoise.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, position, cellnoise}}, &graph)) << test.id;

    WhiteNoiseTextureNode *white_noise = nullptr;
    for (ShaderNode *node : graph.nodes) {
      white_noise = node->name == "CellNoise" ? dynamic_cast<WhiteNoiseTextureNode *>(node) :
                                                white_noise;
    }
    ASSERT_NE(white_noise, nullptr) << test.id;
    EXPECT_EQ(white_noise->get_dimensions(), test.dimensions) << test.id;
    ASSERT_NE(white_noise->input("Vector")->link, nullptr) << test.id;
    VectorMathNode *floor = nullptr;
    for (ShaderNode *node : graph.nodes) {
      floor = node->name == "CellNoise.floor" ? dynamic_cast<VectorMathNode *>(node) : floor;
    }
    ASSERT_NE(floor, nullptr) << test.id;
    EXPECT_EQ(floor->get_math_type(), NODE_VECTOR_MATH_FLOOR) << test.id;
    ASSERT_NE(floor->input("Vector1")->link, nullptr) << test.id;
  }
}

TEST(materialx_graph, rejects_invalid_cellnoise_before_mutating_destination)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;
  materialx::Node cellnoise;
  cellnoise.name = "CellNoise";
  cellnoise.nodedef = "ND_cellnoise3d_float";
  cellnoise.links["position"] = {"Texcoord", "out", materialx::Type::Vector2};
  cellnoise.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  graph.create_node<ValueNode>()->name = "Sentinel";
  const size_t original_node_count = graph.nodes.size();
  ASSERT_FALSE(materialx::lower({{texcoord, cellnoise}}, &graph));
  ASSERT_EQ(graph.nodes.size(), original_node_count);
  bool sentinel_seen = false;
  for (ShaderNode *node : graph.nodes) {
    sentinel_seen |= node->name == "Sentinel";
  }
  EXPECT_TRUE(sentinel_seen);
}

TEST(materialx_graph, lowers_vector3_conditionals_with_exact_boundary_predicates)
{
  for (const char *id : {"ND_ifgreater_vector3", "ND_ifgreatereq_vector3", "ND_ifequal_vector3"}) {
    materialx::Node conditional{"Conditional", id};
    conditional.inputs["value1"] = 1.0f; conditional.inputs["value2"] = 1.0f;
    conditional.vector3_inputs["in1"] = make_float3(0.8f, 0.4f, 0.2f);
    conditional.vector3_inputs["in2"] = make_float3(0.1f, 0.3f, 0.5f);
    conditional.outputs["out"] = materialx::Type::Vector3;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{conditional}}, &graph)) << id;
    bool mix_seen = false, predicate_seen = false;
    for (ShaderNode *node : graph.nodes) {
      mix_seen |= dynamic_cast<MixVectorNode *>(node) != nullptr;
      if (const auto *math = dynamic_cast<MathNode *>(node)) {
        predicate_seen |= math->get_math_type() == NODE_MATH_GREATER_THAN ||
                          math->get_math_type() == NODE_MATH_MAXIMUM ||
                          math->get_math_type() == NODE_MATH_COMPARE;
      }
    }
    EXPECT_TRUE(mix_seen) << id;
    EXPECT_TRUE(predicate_seen) << id;
  }
}


TEST(materialx_graph, lowers_homogeneous_fractal3d_contracts)
{
  materialx::Node position{"Position", "ND_constant_vector3"};
  position.vector3_inputs["value"] = make_float3(0.125f, 0.5f, 0.875f);
  position.outputs["out"] = materialx::Type::Vector3;

  const struct {
    const char *id;
    materialx::Type type;
    bool scalar_amplitude;
    int components;
  } cases[] = {{"ND_fractal3d_float", materialx::Type::Float, true, 1},
               {"ND_fractal3d_color3", materialx::Type::Color3, false, 3},
               {"ND_fractal3d_color3FA", materialx::Type::Color3, true, 3},
               {"ND_fractal3d_vector2", materialx::Type::Vector2, false, 2},
               {"ND_fractal3d_vector2FA", materialx::Type::Vector2, true, 2},
               {"ND_fractal3d_vector3", materialx::Type::Vector3, false, 3},
               {"ND_fractal3d_vector3FA", materialx::Type::Vector3, true, 3}};

  for (const auto &test : cases) {
    materialx::Node fractal{"Fractal", test.id};
    fractal.int_inputs["octaves"] = 5;
    fractal.inputs["lacunarity"] = 2.75f;
    fractal.inputs["diminish"] = 0.375f;
    if (test.scalar_amplitude) {
      fractal.inputs["amplitude"] = 0.5f;
    }
    else if (test.components == 2) {
      fractal.vector2_inputs["amplitude"] = make_float2(0.5f, 0.75f);
    }
    else {
      fractal.vector3_inputs["amplitude"] = make_float3(0.5f, 0.75f, 1.0f);
    }
    fractal.links["position"] = {"Position", "out", materialx::Type::Vector3};
    fractal.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{position, fractal}}, &graph)) << test.id;
    NoiseTextureNode *texture = nullptr;
    ShaderNode *lowered = nullptr;
    for (ShaderNode *node : graph.nodes) {
      texture = texture ? texture : dynamic_cast<NoiseTextureNode *>(node);
      lowered = node->name == "Fractal" ? node : lowered;
    }
    ASSERT_NE(texture, nullptr) << test.id;
    ASSERT_NE(lowered, nullptr) << test.id;
    EXPECT_EQ(texture->get_dimensions(), 3) << test.id;
    EXPECT_EQ(texture->get_type(), NODE_NOISE_FBM) << test.id;
    EXPECT_FLOAT_EQ(texture->get_detail(), 5.0f) << test.id;
    EXPECT_FLOAT_EQ(texture->get_lacunarity(), 2.75f) << test.id;
    EXPECT_FLOAT_EQ(texture->get_roughness(), 0.375f) << test.id;
    EXPECT_NE(lowered->output(test.type == materialx::Type::Float ? "Value" :
                               test.type == materialx::Type::Color3 ? "Color" : "Vector"),
              nullptr)
        << test.id;
  }
}

TEST(materialx_graph, lowers_split_defaults_and_linked_inputs_with_materialx_signature)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node center;
  center.name = "Center";
  center.nodedef = "ND_constant_float";
  center.inputs["value"] = 0.5f;
  center.outputs["out"] = materialx::Type::Float;

  materialx::Node left;
  left.name = "Left";
  left.nodedef = "ND_constant_color4";
  left.float4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  left.outputs["out"] = materialx::Type::Color4;

  materialx::Node split;
  split.name = "Split";
  split.nodedef = "ND_splitlr_color4";
  split.links["valuel"] = {"Left", "out", materialx::Type::Color4};
  split.links["center"] = {"Center", "out", materialx::Type::Float};
  split.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  split.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{uv, center, left, split}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *mix = dynamic_cast<MixNode *>(nodes["Split"]);
  auto *factor = dynamic_cast<MathNode *>(nodes["Split.factor"]);
  auto *alpha_delta = dynamic_cast<MathNode *>(nodes["Split.Alpha.delta"]);
  auto *alpha_sum = dynamic_cast<MathNode *>(nodes["Split.Alpha"]);
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(factor, nullptr);
  ASSERT_NE(alpha_delta, nullptr);
  ASSERT_NE(alpha_sum, nullptr);
  EXPECT_EQ(factor->input("Value2")->link, nodes["Center"]->output("Value"));
  EXPECT_EQ(mix->input("Color1")->link, nodes["Left"]->output("Color"));
  EXPECT_FLOAT_EQ(mix->get_color2().x, 0.0f);
  EXPECT_FLOAT_EQ(mix->get_color2().y, 0.0f);
  EXPECT_FLOAT_EQ(mix->get_color2().z, 0.0f);
  EXPECT_EQ(alpha_delta->input("Value2")->link, nodes["Left.Alpha"]->output("Value"));
  EXPECT_EQ(alpha_sum->input("Value1")->link, nodes["Left.Alpha"]->output("Value"));
  EXPECT_FLOAT_EQ(alpha_delta->get_value1(), 0.0f);
}

TEST(materialx_graph, rejects_split_invalid_shape_before_mutating_destination)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node split;
  split.name = "Split";
  split.nodedef = "ND_splitlr_float";
  split.inputs["valuel"] = 0.0f;
  split.inputs["valuer"] = 1.0f;
  split.inputs["center"] = std::numeric_limits<float>::infinity();
  split.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  split.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{uv, split}}, &graph));
  int principled_count = 0;
  for (ShaderNode *node : graph.nodes) {
    principled_count += node->type == PrincipledBsdfNode::get_node_type();
  }
  EXPECT_EQ(principled_count, 1);
}

TEST(materialx_graph, rejects_invalid_fractal3d_contracts_atomically)
{
  materialx::Node position{"Position", "ND_constant_vector3"};
  position.vector3_inputs["value"] = make_float3(0.125f, 0.5f, 0.875f);
  position.outputs["out"] = materialx::Type::Vector3;

  const struct {
    const char *id;
    bool vector2;
  } cases[] = {{"ND_fractal3d_float", false},
               {"ND_fractal3d_color3", false},
               {"ND_fractal3d_color3FA", false},
               {"ND_fractal3d_vector2", true},
               {"ND_fractal3d_vector2FA", true},
               {"ND_fractal3d_vector3", false},
               {"ND_fractal3d_vector3FA", false}};

  for (const auto &test : cases) {
    materialx::Node fractal{"Fractal", test.id};
    fractal.int_inputs["octaves"] = 0;
    fractal.inputs["lacunarity"] = 2.0f;
    fractal.inputs["diminish"] = 0.5f;
    if (string(test.id).find("FA") != string::npos || string(test.id).find("float") != string::npos) {
      fractal.inputs["amplitude"] = 0.5f;
    }
    else if (test.vector2) {
      fractal.vector2_inputs["amplitude"] = make_float2(0.5f, 0.75f);
    }
    else {
      fractal.vector3_inputs["amplitude"] = make_float3(0.5f, 0.75f, 1.0f);
    }
    fractal.links["position"] = {"Position", "out", materialx::Type::Vector3};
    fractal.outputs["out"] = test.vector2 ? materialx::Type::Vector2 :
        string(test.id).find("float") != string::npos ? materialx::Type::Float :
        string(test.id).find("color3") != string::npos ? materialx::Type::Color3 :
                                                          materialx::Type::Vector3;

    EXPECT_FALSE(materialx::validate({{position, fractal}})) << test.id;
    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower({{position, fractal}}, &graph)) << test.id;
    EXPECT_EQ(graph.nodes.size(), original_node_count) << test.id;
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link) << test.id;
  }
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

TEST(materialx_graph, lowers_omitted_constant_color4_to_installed_zero_default)
{
  materialx::Node constant;
  constant.name = "DefaultColor4";
  constant.nodedef = "ND_constant_color4";
  constant.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  CombineColorNode *color = nullptr;
  ValueNode *alpha = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color = node->name == "DefaultColor4" ? dynamic_cast<CombineColorNode *>(node) : color;
    alpha = node->name == "DefaultColor4.Alpha" ? dynamic_cast<ValueNode *>(node) : alpha;
  }
  ASSERT_NE(color, nullptr);
  ASSERT_NE(alpha, nullptr);
  EXPECT_FLOAT_EQ(color->get_r(), 0.0f);
  EXPECT_FLOAT_EQ(color->get_g(), 0.0f);
  EXPECT_FLOAT_EQ(color->get_b(), 0.0f);
  EXPECT_FLOAT_EQ(alpha->get_value(), 0.0f);
}

TEST(materialx_graph, rejects_omitted_constant_color4_bad_shape_and_value_links_atomically)
{
  const auto expect_rejected = [](materialx::Graph source) {
    EXPECT_FALSE(materialx::validate(source));

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower(source, &graph));
    EXPECT_EQ(graph.nodes.size(), original_node_count);
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
  };

  materialx::Node constant;
  constant.name = "DefaultColor4";
  constant.nodedef = "ND_constant_color4";
  constant.outputs["out"] = materialx::Type::Color4;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  materialx::Node extra_output = constant;
  extra_output.outputs["extra"] = materialx::Type::Color4;
  expect_rejected({{extra_output}});

  materialx::Node linked_value = constant;
  linked_value.links["value"] = {"Other", "out", materialx::Type::Color4};
  materialx::Node other;
  other.name = "Other";
  other.nodedef = "ND_constant_color4";
  other.float4_inputs["value"] = make_float4(1.0f);
  other.outputs["out"] = materialx::Type::Color4;
  expect_rejected({{other, linked_value}});
}

/* Task 4: four-component observation, Vector4 device ABI. Mirrors the
 * Color4 constant tests immediately above -- same "distinct native tag,
 * N-component payload plus a parallel scalar" shape, but a CombineXYZNode
 * (Vector) instead of a CombineColorNode (Color), and a ".W" ValueNode
 * instead of ".Alpha". */

TEST(materialx_graph, lowers_constant_vector4_preserving_w_component)
{
  materialx::Node constant;
  constant.name = "Vector4Constant";
  constant.nodedef = "ND_constant_vector4";
  constant.vector4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  constant.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  CombineXYZNode *vector = nullptr;
  ValueNode *w = nullptr;
  for (ShaderNode *node : graph.nodes) {
    vector = node->name == "Vector4Constant" ? dynamic_cast<CombineXYZNode *>(node) : vector;
    w = node->name == "Vector4Constant.W" ? dynamic_cast<ValueNode *>(node) : w;
  }
  ASSERT_NE(vector, nullptr);
  ASSERT_NE(w, nullptr);
  EXPECT_FLOAT_EQ(vector->get_x(), 0.1f);
  EXPECT_FLOAT_EQ(vector->get_y(), 0.2f);
  EXPECT_FLOAT_EQ(vector->get_z(), 0.3f);
  /* W preservation: the fourth component is a genuine, distinct native
   * payload, not dropped or folded into the three-component node. */
  EXPECT_FLOAT_EQ(w->get_value(), 0.4f);
}

TEST(materialx_graph, lowers_omitted_constant_vector4_to_installed_zero_default)
{
  materialx::Node constant;
  constant.name = "DefaultVector4";
  constant.nodedef = "ND_constant_vector4";
  constant.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  CombineXYZNode *vector = nullptr;
  ValueNode *w = nullptr;
  for (ShaderNode *node : graph.nodes) {
    vector = node->name == "DefaultVector4" ? dynamic_cast<CombineXYZNode *>(node) : vector;
    w = node->name == "DefaultVector4.W" ? dynamic_cast<ValueNode *>(node) : w;
  }
  ASSERT_NE(vector, nullptr);
  ASSERT_NE(w, nullptr);
  EXPECT_FLOAT_EQ(vector->get_x(), 0.0f);
  EXPECT_FLOAT_EQ(vector->get_y(), 0.0f);
  EXPECT_FLOAT_EQ(vector->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(w->get_value(), 0.0f);
}

TEST(materialx_graph, lowers_two_constant_vector4_nodes_with_independent_w_values)
{
  /* "Stale output" guard: two Vector4 constants in the same graph must not
   * cross-contaminate each other's W ValueNode. */
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector4";
  first.vector4_inputs["value"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  first.outputs["out"] = materialx::Type::Vector4;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector4";
  second.vector4_inputs["value"] = make_float4(5.0f, 6.0f, 7.0f, 8.0f);
  second.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second}}, &graph));

  ValueNode *first_w = nullptr;
  ValueNode *second_w = nullptr;
  for (ShaderNode *node : graph.nodes) {
    first_w = node->name == "First.W" ? dynamic_cast<ValueNode *>(node) : first_w;
    second_w = node->name == "Second.W" ? dynamic_cast<ValueNode *>(node) : second_w;
  }
  ASSERT_NE(first_w, nullptr);
  ASSERT_NE(second_w, nullptr);
  EXPECT_FLOAT_EQ(first_w->get_value(), 4.0f);
  EXPECT_FLOAT_EQ(second_w->get_value(), 8.0f);
}

TEST(materialx_graph, rejects_constant_vector4_bad_shape_value_and_tag_atomically)
{
  const auto expect_rejected = [](materialx::Graph source) {
    EXPECT_FALSE(materialx::validate(source));

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
    EXPECT_FALSE(materialx::lower(source, &graph));
    EXPECT_EQ(graph.nodes.size(), original_node_count);
    EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
  };

  materialx::Node constant;
  constant.name = "Vector4Constant";
  constant.nodedef = "ND_constant_vector4";
  constant.outputs["out"] = materialx::Type::Vector4;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  /* Wrong tag: nodedef is ND_constant_vector4 but the output type is
   * Color4, not Vector4. */
  materialx::Node wrong_tag = constant;
  wrong_tag.outputs["out"] = materialx::Type::Color4;
  expect_rejected({{wrong_tag}});

  /* Nonfinite context. */
  materialx::Node nonfinite = constant;
  nonfinite.vector4_inputs["value"] = make_float4(
      0.0f, 0.0f, 0.0f, std::numeric_limits<float>::infinity());
  expect_rejected({{nonfinite}});

  materialx::Node linked_value = constant;
  linked_value.links["value"] = {"Other", "out", materialx::Type::Vector4};
  materialx::Node other;
  other.name = "Other";
  other.nodedef = "ND_constant_vector4";
  other.vector4_inputs["value"] = make_float4(1.0f);
  other.outputs["out"] = materialx::Type::Vector4;
  expect_rejected({{other, linked_value}});
}

TEST(materialx_graph, lowers_color4_lr_tb_ramps_preserving_alpha)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Graph source;
  source.nodes.push_back(uv);
  for (const char *nodedef : {"ND_ramplr_color4", "ND_ramptb_color4"}) {
    materialx::Node ramp;
    ramp.name = nodedef;
    ramp.nodedef = nodedef;
    ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
    if (string(nodedef) == "ND_ramplr_color4") {
      ramp.float4_inputs["valuel"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
      ramp.float4_inputs["valuer"] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
    }
    else {
      ramp.float4_inputs["valuet"] = make_float4(0.1f, 0.2f, 0.3f, 0.45f);
      ramp.float4_inputs["valueb"] = make_float4(0.5f, 0.6f, 0.7f, 0.85f);
    }
    ramp.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(ramp));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  for (const char *name : {"ND_ramplr_color4", "ND_ramptb_color4"}) {
    ASSERT_NE(dynamic_cast<MixNode *>(nodes[name]), nullptr) << name;
    ASSERT_NE(dynamic_cast<SeparateXYZNode *>(nodes[string(name) + ".coordinate"]), nullptr)
        << name;
    ASSERT_NE(dynamic_cast<ClampNode *>(nodes[string(name) + ".factor"]), nullptr) << name;
    MathNode *alpha_delta = dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha.delta"]);
    MathNode *alpha_product = dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha.product"]);
    MathNode *alpha_sum = dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha"]);
    ASSERT_NE(alpha_delta, nullptr) << name;
    ASSERT_NE(alpha_product, nullptr) << name;
    ASSERT_NE(alpha_sum, nullptr) << name;
    EXPECT_EQ(alpha_delta->get_math_type(), NODE_MATH_SUBTRACT) << name;
    EXPECT_EQ(alpha_product->get_math_type(), NODE_MATH_MULTIPLY) << name;
    EXPECT_EQ(alpha_sum->get_math_type(), NODE_MATH_ADD) << name;
  }
}

TEST(materialx_graph, lowers_color4_ramps_with_installed_zero_color_defaults)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  for (const char *nodedef : {"ND_ramplr_color4", "ND_ramptb_color4"}) {
    materialx::Node ramp;
    ramp.name = nodedef;
    ramp.nodedef = nodedef;
    ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
    ramp.outputs["out"] = materialx::Type::Color4;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{uv, ramp}}, &graph)) << nodedef;
    MixNode *mix = nullptr;
    MathNode *alpha_delta = nullptr;
    MathNode *alpha_sum = nullptr;
    for (ShaderNode *node : graph.nodes) {
      mix = node->name == nodedef ? dynamic_cast<MixNode *>(node) : mix;
      alpha_delta = node->name == string(nodedef) + ".Alpha.delta" ?
                        dynamic_cast<MathNode *>(node) :
                        alpha_delta;
      alpha_sum = node->name == string(nodedef) + ".Alpha" ? dynamic_cast<MathNode *>(node) :
                                                             alpha_sum;
    }
    ASSERT_NE(mix, nullptr) << nodedef;
    ASSERT_NE(alpha_delta, nullptr) << nodedef;
    ASSERT_NE(alpha_sum, nullptr) << nodedef;
    EXPECT_EQ(mix->get_color1(), zero_float3()) << nodedef;
    EXPECT_EQ(mix->get_color2(), zero_float3()) << nodedef;
    EXPECT_FLOAT_EQ(alpha_delta->get_value1(), 0.0f) << nodedef;
    EXPECT_FLOAT_EQ(alpha_delta->get_value2(), 0.0f) << nodedef;
    EXPECT_FLOAT_EQ(alpha_sum->get_value1(), 0.0f) << nodedef;
  }
}

TEST(materialx_graph, rejects_invalid_linked_color4_ramp_values_atomically)
{
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(1.0f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node ramp;
  ramp.name = "Ramp";
  ramp.nodedef = "ND_ramplr_color4";
  ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  ramp.links["valuel"] = {"Color", "out", materialx::Type::Color4};
  ramp.outputs["out"] = materialx::Type::Color4;

  EXPECT_FALSE(materialx::validate({{uv, color, ramp}}));
  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{uv, color, ramp}}, &graph));
  int principled_count = 0;
  for (ShaderNode *shader_node : graph.nodes) {
    if (shader_node->type == PrincipledBsdfNode::get_node_type()) {
      principled_count++;
    }
  }
  EXPECT_EQ(principled_count, 1);
}

CCL_NAMESPACE_END
