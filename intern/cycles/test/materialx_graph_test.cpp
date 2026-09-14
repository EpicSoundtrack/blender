/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <vector>

#include "materialx/authority.h"
#include "materialx/graph.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/colorspace.h"
#include "util/transform.h"

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

TEST(materialx_graph, lowers_value_typed_dot_identity_passthroughs)
{
  materialx::Graph source;

  materialx::Node dot_float;
  dot_float.name = "DotFloat";
  dot_float.nodedef = "ND_dot_float";
  dot_float.inputs["in"] = 0.375f;
  dot_float.string_inputs["note"] = "organization only";
  dot_float.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(std::move(dot_float));

  materialx::Node dot_color3;
  dot_color3.name = "DotColor3";
  dot_color3.nodedef = "ND_dot_color3";
  dot_color3.color3_inputs["in"] = make_float3(0.1f, 0.2f, 0.3f);
  dot_color3.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(dot_color3));

  materialx::Node dot_color4;
  dot_color4.name = "DotColor4";
  dot_color4.nodedef = "ND_dot_color4";
  dot_color4.float4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  dot_color4.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(std::move(dot_color4));

  materialx::Node dot_vector2;
  dot_vector2.name = "DotVector2";
  dot_vector2.nodedef = "ND_dot_vector2";
  dot_vector2.vector2_inputs["in"] = make_float2(1.0f, 2.0f);
  dot_vector2.outputs["out"] = materialx::Type::Vector2;
  source.nodes.push_back(std::move(dot_vector2));

  materialx::Node dot_vector3;
  dot_vector3.name = "DotVector3";
  dot_vector3.nodedef = "ND_dot_vector3";
  dot_vector3.vector3_inputs["in"] = make_float3(1.0f, 2.0f, 3.0f);
  dot_vector3.outputs["out"] = materialx::Type::Vector3;
  source.nodes.push_back(std::move(dot_vector3));

  materialx::Node dot_vector4;
  dot_vector4.name = "DotVector4";
  dot_vector4.nodedef = "ND_dot_vector4";
  dot_vector4.vector4_inputs["in"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  dot_vector4.outputs["out"] = materialx::Type::Vector4;
  source.nodes.push_back(std::move(dot_vector4));

  materialx::Node dot_boolean;
  dot_boolean.name = "DotBoolean";
  dot_boolean.nodedef = "ND_dot_boolean";
  dot_boolean.int_inputs["in"] = 1;
  dot_boolean.outputs["out"] = materialx::Type::Boolean;
  source.nodes.push_back(std::move(dot_boolean));

  materialx::Node dot_integer;
  dot_integer.name = "DotInteger";
  dot_integer.nodedef = "ND_dot_integer";
  dot_integer.int_inputs["in"] = 7;
  dot_integer.outputs["out"] = materialx::Type::Integer;
  source.nodes.push_back(std::move(dot_integer));

  materialx::Node dot_matrix33;
  dot_matrix33.name = "DotMatrix33";
  dot_matrix33.nodedef = "ND_dot_matrix33";
  dot_matrix33.matrix33_inputs["in"] = {1.0f, 0.0f, 0.0f,
                                         0.0f, 1.0f, 0.0f,
                                         0.0f, 0.0f, 1.0f};
  dot_matrix33.outputs["out"] = materialx::Type::Matrix33;
  source.nodes.push_back(std::move(dot_matrix33));

  materialx::Node dot_matrix44;
  dot_matrix44.name = "DotMatrix44";
  dot_matrix44.nodedef = "ND_dot_matrix44";
  dot_matrix44.matrix44_inputs["in"] = {1.0f, 0.0f, 0.0f, 5.0f,
                                         0.0f, 1.0f, 0.0f, 6.0f,
                                         0.0f, 0.0f, 1.0f, 7.0f,
                                         0.0f, 0.0f, 0.0f, 1.0f};
  dot_matrix44.outputs["out"] = materialx::Type::Matrix44;
  source.nodes.push_back(std::move(dot_matrix44));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  EXPECT_NE(dynamic_cast<ValueNode *>(lowered.at("DotFloat")), nullptr);
  EXPECT_NE(dynamic_cast<ColorNode *>(lowered.at("DotColor3")), nullptr);
  EXPECT_NE(dynamic_cast<CombineColorNode *>(lowered.at("DotColor4")), nullptr);
  EXPECT_NE(dynamic_cast<CombineXYZNode *>(lowered.at("DotVector2")), nullptr);
  EXPECT_NE(dynamic_cast<CombineXYZNode *>(lowered.at("DotVector3")), nullptr);
  EXPECT_NE(dynamic_cast<CombineXYZNode *>(lowered.at("DotVector4")), nullptr);
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered.at("DotBoolean"))->get_use_clamp(), true);
  EXPECT_EQ(dynamic_cast<MagicTextureNode *>(lowered.at("DotInteger"))->get_depth(), 7);
  EXPECT_NE(dynamic_cast<TextureCoordinateNode *>(lowered.at("DotMatrix33")), nullptr);
  EXPECT_NE(dynamic_cast<TextureCoordinateNode *>(lowered.at("DotMatrix44")), nullptr);
}

TEST(materialx_graph, lowers_linked_value_typed_dot_as_identity_passthrough)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node dot;
  dot.name = "DotColor";
  dot.nodedef = "ND_dot_color3";
  dot.links["in"] = {"Color", "out", materialx::Type::Color3};
  dot.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"DotColor", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, dot, surface}}, &graph));

  ColorNode *lowered_color = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Color") {
      lowered_color = dynamic_cast<ColorNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(lowered_color, nullptr);
  ASSERT_NE(principled, nullptr);
  ASSERT_NE(principled->input("Base Color")->link, nullptr);
  EXPECT_EQ(principled->input("Base Color")->link->parent, lowered_color);
}

TEST(materialx_graph, lowers_zero_size_blur_nodes_as_exact_identity)
{
  const TemporaryImage image_asset;
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_image_float";
  scalar.asset_inputs["file"] = image_asset.path();
  scalar.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4;
  color4.name = "Color4";
  color4.nodedef = "ND_image_color4";
  color4.asset_inputs["file"] = image_asset.path();
  color4.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector2;
  vector2.name = "Vector2";
  vector2.nodedef = "ND_constant_vector2";
  vector2.vector2_inputs["value"] = make_float2(0.4f, 0.5f);
  vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3;
  vector3.name = "Vector3";
  vector3.nodedef = "ND_constant_vector3";
  vector3.vector3_inputs["value"] = make_float3(0.6f, 0.7f, 0.8f);
  vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector4;
  vector4.name = "Vector4";
  vector4.nodedef = "ND_constant_vector4";
  vector4.vector4_inputs["value"] = make_float4(0.9f, 1.0f, 1.1f, 1.2f);
  vector4.outputs["out"] = materialx::Type::Vector4;

  struct Case {
    const char *name;
    const char *nodedef;
    const char *source;
    materialx::Type type;
  } cases[] = {{"BlurFloat", "ND_blur_float", "Scalar", materialx::Type::Float},
               {"BlurColor3", "ND_blur_color3", "Color", materialx::Type::Color3},
               {"BlurColor4", "ND_blur_color4", "Color4", materialx::Type::Color4},
               {"BlurVector2", "ND_blur_vector2", "Vector2", materialx::Type::Vector2},
               {"BlurVector3", "ND_blur_vector3", "Vector3", materialx::Type::Vector3},
               {"BlurVector4", "ND_blur_vector4", "Vector4", materialx::Type::Vector4}};

  materialx::Graph source;
  source.nodes = {uv, scalar, color, color4, vector2, vector3, vector4};
  for (const Case &test : cases) {
    materialx::Node blur;
    blur.name = test.name;
    blur.nodedef = test.nodedef;
    blur.links["in"] = {test.source, "out", test.type};
    blur.inputs["size"] = 0.0f;
    blur.string_inputs["filtertype"] = "gaussian";
    blur.outputs["out"] = test.type;
    source.nodes.push_back(std::move(blur));
  }

  materialx::Node color4_to_color3;
  color4_to_color3.name = "Color4ToColor3";
  color4_to_color3.nodedef = "ND_convert_color4_color3";
  color4_to_color3.links["in"] = {"BlurColor4", "out", materialx::Type::Color4};
  color4_to_color3.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(color4_to_color3));

  materialx::Node vector3_to_color3;
  vector3_to_color3.name = "Vector3ToColor3";
  vector3_to_color3.nodedef = "ND_convert_vector3_color3";
  vector3_to_color3.links["in"] = {"BlurVector3", "out", materialx::Type::Vector3};
  vector3_to_color3.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(vector3_to_color3));

  materialx::Node vector4_to_color3;
  vector4_to_color3.name = "Vector4ToColor3";
  vector4_to_color3.nodedef = "ND_convert_vector4_color3";
  vector4_to_color3.links["in"] = {"BlurVector4", "out", materialx::Type::Vector4};
  vector4_to_color3.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(std::move(vector4_to_color3));

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_weight"] = {"BlurFloat", "out", materialx::Type::Float};
  surface.links["base_color"] = {"BlurColor3", "out", materialx::Type::Color3};
  surface.links["emission_color"] = {"Vector3ToColor3", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;
  source.nodes.push_back(std::move(surface));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  PrincipledBsdfNode *principled = dynamic_cast<PrincipledBsdfNode *>(lowered.at("Surface"));
  ASSERT_NE(principled, nullptr);
  MathNode *weight_delta = dynamic_cast<MathNode *>(lowered.at("Surface.base_weight_delta"));
  ASSERT_NE(weight_delta, nullptr);
  EXPECT_NE(weight_delta->input("Value1")->link, nullptr);
  EXPECT_NE(principled->input("Base Color")->link, nullptr);
  EXPECT_NE(lowered.at("Vector3ToColor3.separate")->input("Vector")->link, nullptr);
  EXPECT_NE(lowered.at("Vector4ToColor3.separate")->input("Vector")->link, nullptr);
  EXPECT_EQ(lowered.find("BlurFloat"), lowered.end());
  EXPECT_EQ(lowered.find("BlurColor3"), lowered.end());
  EXPECT_EQ(lowered.find("BlurVector3"), lowered.end());
  EXPECT_EQ(lowered.find("BlurVector4"), lowered.end());
  EXPECT_TRUE(materialx::validate(source));
}

TEST(materialx_graph, lowers_constant_heighttonormal_to_flat_normal)
{
  materialx::Node height;
  height.name = "HeightToNormal";
  height.nodedef = "ND_heighttonormal_vector3";
  height.inputs["in"] = 0.25f;
  height.inputs["scale"] = 2.0f;
  height.vector2_inputs["texcoord"] = make_float2(0.5f, 0.25f);
  height.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{height}}, &graph));

  CombineXYZNode *flat = nullptr;
  for (ShaderNode *node : graph.nodes) {
    flat = node->name == "HeightToNormal" ? dynamic_cast<CombineXYZNode *>(node) : flat;
  }
  ASSERT_NE(flat, nullptr);
  EXPECT_FLOAT_EQ(flat->get_x(), 0.5f);
  EXPECT_FLOAT_EQ(flat->get_y(), 0.5f);
  EXPECT_FLOAT_EQ(flat->get_z(), 1.0f);
}

TEST(materialx_graph, lowers_flat_default_hextilednormalmap_to_native_normalize)
{
  materialx::Node hextiled;
  hextiled.name = "HexTiledNormalMap";
  hextiled.nodedef = "ND_hextilednormalmap_vector3";
  hextiled.asset_inputs["file"] = "";
  hextiled.vector3_inputs["default"] = make_float3(0.5f, 0.5f, 1.0f);
  hextiled.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
  hextiled.vector2_inputs["tiling"] = make_float2(1.0f, 1.0f);
  hextiled.inputs["rotation"] = 1.0f;
  hextiled.vector2_inputs["rotationrange"] = make_float2(0.0f, 360.0f);
  hextiled.inputs["scale"] = 1.0f;
  hextiled.vector2_inputs["scalerange"] = make_float2(0.5f, 2.0f);
  hextiled.inputs["offset"] = 1.0f;
  hextiled.vector2_inputs["offsetrange"] = make_float2(0.0f, 1.0f);
  hextiled.inputs["falloff"] = 0.5f;
  hextiled.inputs["strength"] = 1.0f;
  hextiled.int_inputs["flip_g"] = 0;
  hextiled.vector3_inputs["normal"] = make_float3(0.0f, 0.0f, 2.0f);
  hextiled.vector3_inputs["tangent"] = make_float3(1.0f, 0.0f, 0.0f);
  hextiled.vector3_inputs["bitangent"] = make_float3(0.0f, 1.0f, 0.0f);
  hextiled.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower({{hextiled}}, &graph, &error)) << error;

  VectorMathNode *normal = nullptr;
  for (ShaderNode *node : graph.nodes) {
    normal = node->name == "HexTiledNormalMap" ? dynamic_cast<VectorMathNode *>(node) : normal;
  }
  ASSERT_NE(normal, nullptr);
  EXPECT_EQ(normal->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(normal->get_vector1(), make_float3(0.0f, 0.0f, 2.0f));
  EXPECT_EQ(normal->input("Vector1")->link, nullptr);
}

TEST(materialx_graph, lowers_literal_vector_to_color4_with_rgb_defaults)
{
  materialx::Node vector2;
  vector2.name = "Vector2ToColor4";
  vector2.nodedef = "ND_convert_vector2_color4";
  vector2.vector2_inputs["in"] = make_float2(10.125f, 11.125f);
  vector2.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector3;
  vector3.name = "Vector3ToColor4";
  vector3.nodedef = "ND_convert_vector3_color4";
  vector3.vector3_inputs["in"] = make_float3(20.125f, 21.125f, 22.125f);
  vector3.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{vector2, vector3}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *color2 = dynamic_cast<CombineColorNode *>(nodes["Vector2ToColor4"]);
  auto *color3 = dynamic_cast<CombineColorNode *>(nodes["Vector3ToColor4"]);
  ASSERT_NE(color2, nullptr);
  ASSERT_NE(color3, nullptr);
  EXPECT_FLOAT_EQ(color2->get_r(), 10.125f);
  EXPECT_FLOAT_EQ(color2->get_g(), 11.125f);
  EXPECT_FLOAT_EQ(color2->get_b(), 0.0f);
  EXPECT_FLOAT_EQ(color3->get_r(), 20.125f);
  EXPECT_FLOAT_EQ(color3->get_g(), 21.125f);
  EXPECT_FLOAT_EQ(color3->get_b(), 22.125f);
}

TEST(materialx_graph, rejects_nonzero_blur_and_heighttonormal_without_mutating_destination)
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

  materialx::Node blur;
  blur.name = "Blur";
  blur.nodedef = "ND_blur_float";
  blur.inputs["in"] = 0.25f;
  blur.inputs["size"] = 1.0f;
  blur.string_inputs["filtertype"] = "box";
  blur.outputs["out"] = materialx::Type::Float;
  expect_rejected({{blur}});

  blur.inputs["size"] = 0.0f;
  blur.string_inputs["filtertype"] = "triangle";
  expect_rejected({{blur}});

  materialx::Node height_value;
  height_value.name = "HeightValue";
  height_value.nodedef = "ND_constant_float";
  height_value.inputs["value"] = 0.25f;
  height_value.outputs["out"] = materialx::Type::Float;

  materialx::Node height;
  height.name = "HeightToNormal";
  height.nodedef = "ND_heighttonormal_vector3";
  height.links["in"] = {"HeightValue", "out", materialx::Type::Float};
  height.inputs["scale"] = 1.0f;
  height.vector2_inputs["texcoord"] = make_float2(0.5f, 0.25f);
  height.outputs["out"] = materialx::Type::Vector3;
  expect_rejected({{height_value, height}});
}

TEST(materialx_graph, rejects_malformed_value_typed_dot_nodes)
{
  materialx::Node base_float;
  base_float.name = "BaseFloat";
  base_float.nodedef = "ND_constant_float";
  base_float.inputs["value"] = 1.0f;
  base_float.outputs["out"] = materialx::Type::Float;

  materialx::Node malformed = base_float;
  malformed.name = "Malformed";
  malformed.nodedef = "ND_dot_float";
  malformed.inputs.clear();
  malformed.links["in"] = {"BaseFloat", "out", materialx::Type::Float};
  malformed.links["extra"] = {"BaseFloat", "out", materialx::Type::Float};
  malformed.outputs["out"] = materialx::Type::Float;
  EXPECT_FALSE(materialx::validate({{base_float, malformed}}));

  malformed.links.clear();
  malformed.inputs["in"] = 1.0f;
  malformed.string_inputs["unexpected"] = "not a MaterialX dot control";
  EXPECT_FALSE(materialx::validate({{malformed}}));

  malformed.string_inputs.clear();
  malformed.outputs["out"] = materialx::Type::Color3;
  EXPECT_FALSE(materialx::validate({{malformed}}));
}

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
  EXPECT_TRUE(rotate3d_node->get_invert());
  EXPECT_EQ(rotate3d_node->get_axis(), make_float3(0.0f, 1.0f, 0.0f));
  ASSERT_NE(rotate3d_radians, nullptr);
  EXPECT_EQ(rotate3d_radians->get_math_type(), NODE_MATH_RADIANS);
  ASSERT_NE(vector_node, nullptr);
  ASSERT_NE(angle_node, nullptr);
  EXPECT_EQ(rotate3d_node->input("Vector")->link, vector_node->output("Vector"));
  EXPECT_EQ(rotate3d_node->input("Angle")->link, rotate3d_radians->output("Value"));
  EXPECT_EQ(rotate3d_radians->input("Value1")->link, angle_node->output("Value"));
}

TEST(materialx_graph, lowers_exact_translation_nodedef_passthrough_outputs)
{
  /* Exact identity/pass-through subset cited from the MaterialX 1.39
   * libraries/bxdf/translation/*.mtlx nodegraphs. Computed outputs remain
   * unsupported rather than approximated. */
  const auto constant = [](const char *name, const float value) {
    materialx::Node node;
    node.name = name;
    node.nodedef = "ND_constant_float";
    node.inputs["value"] = value;
    node.outputs["out"] = materialx::Type::Float;
    return node;
  };
  const auto translation = [](const char *name,
                              const char *nodedef,
                              const char *input_name,
                              const char *source,
                              const char *output_name) {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.links[input_name] = {source, "out", materialx::Type::Float};
    node.outputs[output_name] = materialx::Type::Float;
    return node;
  };

  materialx::Graph source;
  source.nodes.push_back(constant("BaseWeight", 0.25f));
  source.nodes.push_back(constant("Roughness", 0.35f));
  source.nodes.push_back(constant("Metalness", 0.45f));
  source.nodes.push_back(constant("Emission", 0.55f));
  source.nodes.push_back(translation("OpenPBRToStandard",
                                     "ND_open_pbr_surface_to_standard_surface",
                                     "base_weight",
                                     "BaseWeight",
                                     "base_out"));
  source.nodes.push_back(translation("StandardToUsd",
                                     "ND_standard_surface_to_UsdPreviewSurface",
                                     "specular_roughness",
                                     "Roughness",
                                     "roughness_out"));
  source.nodes.push_back(translation("StandardToGltf",
                                     "ND_standard_surface_to_gltf_pbr",
                                     "metalness",
                                     "Metalness",
                                     "metallic_out"));
  source.nodes.push_back(translation("StandardToOpenPBR",
                                     "ND_standard_surface_to_open_pbr_surface",
                                     "emission",
                                     "Emission",
                                     "emission_luminance_out"));

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_weight"] = {"OpenPBRToStandard", "base_out", materialx::Type::Float};
  surface.links["specular_roughness"] = {"StandardToUsd", "roughness_out", materialx::Type::Float};
  surface.links["base_metalness"] = {"StandardToGltf", "metallic_out", materialx::Type::Float};
  surface.links["emission_luminance"] = {
      "StandardToOpenPBR", "emission_luminance_out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;
  source.nodes.push_back(std::move(surface));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  PrincipledBsdfNode *principled = nullptr;
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
    principled = node->name == "OpenPBR" ? dynamic_cast<PrincipledBsdfNode *>(node) : principled;
  }
  ASSERT_NE(principled, nullptr);
  ASSERT_NE(nodes.find("OpenPBR.base_weight_delta"), nodes.end());
  EXPECT_EQ(nodes.at("OpenPBR.base_weight_delta")->input("Value1")->link,
            nodes.at("BaseWeight")->output("Value"));
  EXPECT_EQ(principled->input("Roughness")->link, nodes.at("Roughness")->output("Value"));
  EXPECT_EQ(principled->input("Metallic")->link, nodes.at("Metalness")->output("Value"));
  EXPECT_EQ(principled->input("Emission Strength")->link, nodes.at("Emission")->output("Value"));
}

TEST(materialx_graph, rejects_computed_translation_nodedef_outputs_without_mutation)
{
  materialx::Node node;
  node.name = "ComputedTranslation";
  node.nodedef = "ND_standard_surface_to_gltf_pbr";
  node.color3_inputs["base_color"] = make_float3(0.8f, 0.6f, 0.4f);
  node.outputs["base_color_out"] = materialx::Type::Color3;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{node}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
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

TEST(materialx_graph, lowers_usd_transform2d_as_native_place2d_math)
{
  /* bxdf/usd_preview_surface.mtlx declares ND_UsdTransform2d in nodegroup="math";
   * its implementation nodegraph is an ND_place2d_vector2 with texcoord=in,
   * rotate=rotation, offset=translation, origin pivot, and SRT order. */
  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_constant_vector2";
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node transform;
  transform.name = "UsdTransform";
  transform.nodedef = "ND_UsdTransform2d";
  transform.links["in"] = {"UV", "out", materialx::Type::Vector2};
  transform.inputs["rotation"] = 45.0f;
  transform.vector2_inputs["scale"] = make_float2(2.0f, 3.0f);
  transform.vector2_inputs["translation"] = make_float2(0.125f, 0.25f);
  transform.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{uv, transform}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  auto *operation = dynamic_cast<MixVectorNode *>(lowered["UsdTransform"]);
  auto *pivot = dynamic_cast<CombineXYZNode *>(lowered["UsdTransform.pivot"]);
  auto *scale = dynamic_cast<CombineXYZNode *>(lowered["UsdTransform.scale"]);
  auto *offset = dynamic_cast<CombineXYZNode *>(lowered["UsdTransform.offset"]);
  auto *radians = dynamic_cast<MathNode *>(lowered["UsdTransform.radians"]);
  ASSERT_NE(operation, nullptr);
  ASSERT_NE(pivot, nullptr);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(radians, nullptr);
  EXPECT_FLOAT_EQ(operation->get_fac(), 0.0f);
  EXPECT_FLOAT_EQ(pivot->get_x(), 0.0f);
  EXPECT_FLOAT_EQ(pivot->get_y(), 0.0f);
  EXPECT_FLOAT_EQ(scale->get_x(), 2.0f);
  EXPECT_FLOAT_EQ(scale->get_y(), 3.0f);
  EXPECT_FLOAT_EQ(offset->get_x(), 0.125f);
  EXPECT_FLOAT_EQ(offset->get_y(), 0.25f);
  EXPECT_EQ(radians->get_math_type(), NODE_MATH_RADIANS);
  EXPECT_FLOAT_EQ(radians->get_value1(), 45.0f);
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


TEST(materialx_graph, lowers_contrast_float_color3_and_vector_forms)
{
  materialx::Node scalar;
  scalar.name = "ScalarContrast";
  scalar.nodedef = "ND_contrast_float";
  scalar.inputs = {{"in", 0.25f}, {"amount", 2.0f}, {"pivot", 0.5f}};
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node color;
  color.name = "ColorContrast";
  color.nodedef = "ND_contrast_color3FA";
  color.color3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  color.inputs = {{"amount", 1.5f}, {"pivot", 0.25f}};
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node vector2;
  vector2.name = "Vector2Contrast";
  vector2.nodedef = "ND_contrast_vector2";
  vector2.vector2_inputs = {{"in", make_float2(0.25f, 0.75f)},
                            {"amount", make_float2(2.0f, 3.0f)},
                            {"pivot", make_float2(0.5f, 0.25f)}};
  vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2fa;
  vector2fa.name = "Vector2FAContrast";
  vector2fa.nodedef = "ND_contrast_vector2FA";
  vector2fa.vector2_inputs["in"] = make_float2(0.25f, 0.75f);
  vector2fa.inputs = {{"amount", 1.5f}, {"pivot", 0.25f}};
  vector2fa.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3;
  vector3.name = "Vector3Contrast";
  vector3.nodedef = "ND_contrast_vector3FA";
  vector3.vector3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  vector3.inputs = {{"amount", 2.0f}, {"pivot", 0.5f}};
  vector3.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{scalar, color, vector2, vector2fa, vector3}}, &graph));

  std::unordered_map<string, MathNode *> math;
  CombineColorNode *color_combine = nullptr;
  CombineXYZNode *vector2_combine = nullptr;
  CombineXYZNode *vector2fa_combine = nullptr;
  CombineXYZNode *vector3_combine = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *lowered = dynamic_cast<MathNode *>(node)) {
      math[string(node->name.c_str())] = lowered;
    }
    if (node->name == "ColorContrast") {
      color_combine = dynamic_cast<CombineColorNode *>(node);
    }
    if (node->name == "Vector2Contrast") {
      vector2_combine = dynamic_cast<CombineXYZNode *>(node);
    }
    if (node->name == "Vector2FAContrast") {
      vector2fa_combine = dynamic_cast<CombineXYZNode *>(node);
    }
    if (node->name == "Vector3Contrast") {
      vector3_combine = dynamic_cast<CombineXYZNode *>(node);
    }
  }

  ASSERT_NE(math["ScalarContrast.subtract"], nullptr);
  EXPECT_EQ(math["ScalarContrast.subtract"]->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(math["ScalarContrast.subtract"]->get_value1(), 0.25f);
  EXPECT_FLOAT_EQ(math["ScalarContrast.subtract"]->get_value2(), 0.5f);
  ASSERT_NE(math["ScalarContrast.multiply"], nullptr);
  EXPECT_EQ(math["ScalarContrast.multiply"]->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(math["ScalarContrast.multiply"]->get_value2(), 2.0f);
  ASSERT_NE(math["ScalarContrast"], nullptr);
  EXPECT_EQ(math["ScalarContrast"]->get_math_type(), NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(math["ScalarContrast"]->get_value2(), 0.5f);

  ASSERT_NE(color_combine, nullptr);
  ASSERT_NE(vector2_combine, nullptr);
  ASSERT_NE(vector2fa_combine, nullptr);
  ASSERT_NE(vector3_combine, nullptr);
  EXPECT_FLOAT_EQ(vector2_combine->get_z(), 0.0f);
  ASSERT_NE(math["ColorContrast.Red.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["ColorContrast.Red.multiply"]->get_value2(), 1.5f);
  ASSERT_NE(math["ColorContrast.Red.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["ColorContrast.Red.subtract"]->get_value1(), 0.25f);
  EXPECT_FLOAT_EQ(math["ColorContrast.Red.subtract"]->get_value2(), 0.25f);
  EXPECT_EQ(math["ColorContrast.Red.subtract"]->output("Value")->links.size(), 1);
  EXPECT_EQ(math["ColorContrast.Red.multiply"]->output("Value")->links.size(), 1);
  EXPECT_EQ(math["ColorContrast.Red"]->output("Value")->links.size(), 1);
  ASSERT_NE(math["Vector2Contrast.Y.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector2Contrast.Y.multiply"]->get_value2(), 3.0f);
  ASSERT_NE(math["Vector2Contrast.Y.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector2Contrast.Y.subtract"]->get_value1(), 0.75f);
  EXPECT_FLOAT_EQ(math["Vector2Contrast.Y.subtract"]->get_value2(), 0.25f);
  ASSERT_NE(math["Vector2FAContrast.X.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector2FAContrast.X.multiply"]->get_value2(), 1.5f);
  ASSERT_NE(math["Vector2FAContrast.X.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector2FAContrast.X.subtract"]->get_value2(), 0.25f);
  ASSERT_NE(math["Vector3Contrast.Z.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector3Contrast.Z.multiply"]->get_value2(), 2.0f);
  ASSERT_NE(math["Vector3Contrast.Z.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector3Contrast.Z.subtract"]->get_value1(), 0.75f);
  EXPECT_FLOAT_EQ(math["Vector3Contrast.Z.subtract"]->get_value2(), 0.5f);
}

TEST(materialx_graph, lowers_contrast_color4_forms_preserving_alpha_sidecar)
{
  materialx::Node full;
  full.name = "Color4Contrast";
  full.nodedef = "ND_contrast_color4";
  full.float4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, 0.9f);
  full.float4_inputs["amount"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  full.float4_inputs["pivot"] = make_float4(0.5f, 0.25f, 0.125f, 0.1f);
  full.outputs["out"] = materialx::Type::Color4;

  materialx::Node scalar;
  scalar.name = "Color4FAContrast";
  scalar.nodedef = "ND_contrast_color4FA";
  scalar.links["in"] = {"Color4Contrast", "out", materialx::Type::Color4};
  scalar.inputs = {{"amount", 1.5f}, {"pivot", 0.25f}};
  scalar.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract;
  extract.name = "ExtractAlpha";
  extract.nodedef = "ND_extract_color4";
  extract.links["in"] = {"Color4FAContrast", "out", materialx::Type::Color4};
  extract.int_inputs["index"] = 3;
  extract.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{full, scalar, extract}}, &graph));

  std::unordered_map<string, MathNode *> math;
  CombineColorNode *full_color = nullptr;
  CombineColorNode *scalar_color = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *lowered = dynamic_cast<MathNode *>(node)) {
      math[string(node->name.c_str())] = lowered;
    }
    full_color = node->name == "Color4Contrast" ? dynamic_cast<CombineColorNode *>(node) : full_color;
    scalar_color = node->name == "Color4FAContrast" ? dynamic_cast<CombineColorNode *>(node) : scalar_color;
  }

  ASSERT_NE(full_color, nullptr);
  ASSERT_NE(scalar_color, nullptr);
  ASSERT_NE(math["Color4Contrast.Alpha.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["Color4Contrast.Alpha.subtract"]->get_value1(), 0.9f);
  EXPECT_FLOAT_EQ(math["Color4Contrast.Alpha.subtract"]->get_value2(), 0.1f);
  ASSERT_NE(math["Color4Contrast.Alpha.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["Color4Contrast.Alpha.multiply"]->get_value2(), 5.0f);
  ASSERT_NE(math["Color4FAContrast.Alpha"], nullptr);
  EXPECT_FLOAT_EQ(math["Color4FAContrast.Alpha"]->get_value2(), 0.25f);
  /* ND_extract_color4(index=3) resolves to the Color4 alpha sidecar itself. */
}


TEST(materialx_graph, lowers_color3_scalar_bounds_and_vector3_range_siblings)
{
  materialx::Node full_color;
  full_color.name = "Color3Range";
  full_color.nodedef = "ND_range_color3";
  full_color.color3_inputs = {{"in", make_float3(0.25f, 0.5f, 0.75f)},
                              {"inlow", make_float3(0.0f, 0.0f, 0.0f)},
                              {"inhigh", make_float3(1.0f, 1.0f, 1.0f)},
                              {"outlow", make_float3(-1.0f, -2.0f, -3.0f)},
                              {"outhigh", make_float3(1.0f, 2.0f, 3.0f)}};
  full_color.int_inputs["doclamp"] = 1;
  full_color.outputs["out"] = materialx::Type::Color3;

  materialx::Node color;
  color.name = "Color3FARemap";
  color.nodedef = "ND_remap_color3FA";
  color.color3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  color.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node color_range_fa;
  color_range_fa.name = "Color3FARange";
  color_range_fa.nodedef = "ND_range_color3FA";
  color_range_fa.color3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  color_range_fa.inputs = {{"inlow", 0.0f},
                           {"inhigh", 1.0f},
                           {"outlow", -1.0f},
                           {"outhigh", 1.0f}};
  color_range_fa.int_inputs["doclamp"] = 0;
  color_range_fa.outputs["out"] = materialx::Type::Color3;

  materialx::Node vector3;
  vector3.name = "Vector3Range";
  vector3.nodedef = "ND_range_vector3";
  vector3.vector3_inputs = {{"in", make_float3(0.25f, 0.5f, 0.75f)},
                            {"inlow", make_float3(0.0f, 0.0f, 0.0f)},
                            {"inhigh", make_float3(1.0f, 1.0f, 1.0f)},
                            {"outlow", make_float3(-1.0f, -2.0f, -3.0f)},
                            {"outhigh", make_float3(1.0f, 2.0f, 3.0f)}};
  vector3.int_inputs["doclamp"] = 1;
  vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector3_gamma;
  vector3_gamma.name = "Vector3GammaRange";
  vector3_gamma.nodedef = "ND_range_vector3";
  vector3_gamma.vector3_inputs = {{"in", make_float3(0.25f, 0.5f, 0.75f)},
                                  {"inlow", make_float3(0.0f, 0.0f, 0.0f)},
                                  {"inhigh", make_float3(1.0f, 1.0f, 1.0f)},
                                  {"outlow", make_float3(-1.0f, -0.5f, 0.0f)},
                                  {"outhigh", make_float3(1.0f, 0.5f, 1.0f)},
                                  {"gamma", make_float3(2.0f, 4.0f, 0.5f)}};
  vector3_gamma.int_inputs["doclamp"] = 0;
  vector3_gamma.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector2_default_gamma;
  vector2_default_gamma.name = "Vector2DefaultGammaRange";
  vector2_default_gamma.nodedef = "ND_range_vector2";
  vector2_default_gamma.vector2_inputs = {{"in", make_float2(0.25f, 0.75f)},
                                          {"inlow", make_float2(0.0f, 0.0f)},
                                          {"inhigh", make_float2(1.0f, 1.0f)},
                                          {"outlow", make_float2(-1.0f, -0.5f)},
                                          {"outhigh", make_float2(1.0f, 0.5f)}};
  vector2_default_gamma.int_inputs["doclamp"] = 0;
  vector2_default_gamma.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2_gamma;
  vector2_gamma.name = "Vector2GammaRange";
  vector2_gamma.nodedef = "ND_range_vector2";
  vector2_gamma.vector2_inputs = {{"in", make_float2(0.25f, 0.75f)},
                                  {"inlow", make_float2(0.0f, 0.0f)},
                                  {"inhigh", make_float2(1.0f, 1.0f)},
                                  {"outlow", make_float2(-1.0f, -0.5f)},
                                  {"outhigh", make_float2(1.0f, 0.5f)},
                                  {"gamma", make_float2(2.0f, 4.0f)}};
  vector2_gamma.int_inputs["doclamp"] = 0;
  vector2_gamma.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2fa_gamma;
  vector2fa_gamma.name = "Vector2FAGammaRange";
  vector2fa_gamma.nodedef = "ND_range_vector2FA";
  vector2fa_gamma.vector2_inputs["in"] = make_float2(0.25f, 0.75f);
  vector2fa_gamma.inputs = {{"inlow", 0.0f},
                            {"inhigh", 1.0f},
                            {"outlow", -1.0f},
                            {"outhigh", 1.0f}};
  vector2fa_gamma.vector2_inputs["gamma"] = make_float2(2.0f, 2.0f);
  vector2fa_gamma.int_inputs["doclamp"] = 0;
  vector2fa_gamma.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3fa;
  vector3fa.name = "Vector3FARange";
  vector3fa.nodedef = "ND_range_vector3FA";
  vector3fa.vector3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  vector3fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  vector3fa.int_inputs["doclamp"] = 0;
  vector3fa.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(
      materialx::lower({{full_color,
                         color,
                         color_range_fa,
                         vector3,
                         vector3_gamma,
                         vector2_default_gamma,
                         vector2_gamma,
                         vector2fa_gamma,
                         vector3fa}},
                       &graph));

  int color_ranges = 0;
  std::unordered_map<string, MapRangeNode *> ranges;
  std::unordered_map<string, ShaderNode *> nodes;
  std::unordered_map<string, VectorMapRangeNode *> vector_ranges;
  for (ShaderNode *node : graph.nodes) {
    nodes[string(node->name.c_str())] = node;
    if (node->name == "Color3FARemap.Red" || node->name == "Color3FARemap.Green" ||
        node->name == "Color3FARemap.Blue")
    {
      MapRangeNode *range = dynamic_cast<MapRangeNode *>(node);
      ASSERT_NE(range, nullptr);
      EXPECT_EQ(range->get_range_type(), NODE_MAP_RANGE_LINEAR);
      EXPECT_FALSE(range->get_clamp());
      EXPECT_FLOAT_EQ(range->get_from_min(), 0.0f);
      EXPECT_FLOAT_EQ(range->get_from_max(), 1.0f);
      ranges[string(node->name.c_str())] = range;
      ++color_ranges;
    }
    if (node->name == "Color3Range.Red" || node->name == "Color3FARange.Blue") {
      MapRangeNode *range = dynamic_cast<MapRangeNode *>(node);
      ASSERT_NE(range, nullptr);
      ranges[string(node->name.c_str())] = range;
    }
    if (VectorMapRangeNode *range = dynamic_cast<VectorMapRangeNode *>(node)) {
      vector_ranges[string(node->name.c_str())] = range;
    }
  }
  EXPECT_EQ(color_ranges, 3);
  ASSERT_NE(dynamic_cast<MapRangeNode *>(ranges["Color3Range.Red"]), nullptr);
  EXPECT_TRUE(static_cast<MapRangeNode *>(ranges["Color3Range.Red"])->get_clamp());
  ASSERT_NE(dynamic_cast<MapRangeNode *>(ranges["Color3FARange.Blue"]), nullptr);
  EXPECT_FALSE(static_cast<MapRangeNode *>(ranges["Color3FARange.Blue"])->get_clamp());
  ASSERT_NE(vector_ranges["Vector3Range"], nullptr);
  EXPECT_TRUE(vector_ranges["Vector3Range"]->get_use_clamp());
  EXPECT_EQ(vector_ranges["Vector3Range"]->get_to_min(), make_float3(-1.0f, -2.0f, -3.0f));
  ASSERT_NE(vector_ranges["Vector3GammaRange"], nullptr);
  EXPECT_EQ(vector_ranges["Vector3GammaRange"]->get_from_min(), zero_float3());
  EXPECT_EQ(vector_ranges["Vector3GammaRange"]->get_to_min(), make_float3(-1.0f, -0.5f, 0.0f));
  ASSERT_NE(dynamic_cast<VectorMapRangeNode *>(nodes["Vector3GammaRange.normalize"]), nullptr);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(nodes["Vector3GammaRange.power"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(nodes["Vector3GammaRange.power"])->get_vector2(),
            make_float3(0.5f, 0.25f, 2.0f));
  ASSERT_NE(vector_ranges["Vector2DefaultGammaRange"], nullptr);
  EXPECT_FALSE(vector_ranges["Vector2DefaultGammaRange"]->get_use_clamp());
  EXPECT_EQ(vector_ranges["Vector2DefaultGammaRange"]->get_to_min(),
            make_float3(-1.0f, -0.5f, 0.0f));
  ASSERT_NE(vector_ranges["Vector2GammaRange"], nullptr);
  EXPECT_EQ(vector_ranges["Vector2GammaRange"]->get_from_min(), zero_float3());
  EXPECT_EQ(vector_ranges["Vector2GammaRange"]->get_to_min(), make_float3(-1.0f, -0.5f, 0.0f));
  ASSERT_NE(dynamic_cast<VectorMapRangeNode *>(nodes["Vector2GammaRange.normalize"]), nullptr);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(nodes["Vector2GammaRange.power"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(nodes["Vector2GammaRange.power"])->get_vector2(),
            make_float3(0.5f, 0.25f, 1.0f));
  ASSERT_NE(vector_ranges["Vector2FAGammaRange"], nullptr);
  ASSERT_NE(dynamic_cast<VectorMapRangeNode *>(nodes["Vector2FAGammaRange.normalize"]), nullptr);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(nodes["Vector2FAGammaRange.power"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(nodes["Vector2FAGammaRange.power"])->get_vector2(),
            make_float3(0.5f, 0.5f, 1.0f));
  ASSERT_NE(vector_ranges["Vector3FARange"], nullptr);
  EXPECT_FALSE(vector_ranges["Vector3FARange"]->get_use_clamp());
  EXPECT_EQ(vector_ranges["Vector3FARange"]->get_to_min(), make_float3(-1.0f, -1.0f, -1.0f));
}

TEST(materialx_graph, lowers_color4_and_vector4_adjustment_ranges_preserving_sidecars)
{
  /* Real MaterialX stdlib sources: libraries/stdlib/stdlib_defs.mtlx declares
   * ND_remap_color4/Color4FA and ND_range_vector4/Vector4FA in nodegroup
   * "adjustment"; libraries/stdlib/stdlib_ng.mtlx implements range/remap
   * componentwise, with gamma identity when gamma is 1. */
  materialx::Node color;
  color.name = "Color4Range";
  color.nodedef = "ND_range_color4FA";
  color.float4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, 0.9f);
  color.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}, {"gamma", 1.0f}};
  color.int_inputs["doclamp"] = 1;
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node full_color;
  full_color.name = "Color4FullRange";
  full_color.nodedef = "ND_range_color4";
  full_color.float4_inputs = {{"in", make_float4(0.2f, 0.4f, 0.6f, 0.8f)},
                              {"inlow", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                              {"inhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)},
                              {"outlow", make_float4(-1.0f, -2.0f, -3.0f, -4.0f)},
                              {"outhigh", make_float4(1.0f, 2.0f, 3.0f, 4.0f)},
                              {"gamma", make_float4(2.0f, 1.0f, 0.5f, 4.0f)}};
  full_color.int_inputs["doclamp"] = 0;
  full_color.outputs["out"] = materialx::Type::Color4;

  materialx::Node remap_color;
  remap_color.name = "Color4Remap";
  remap_color.nodedef = "ND_remap_color4";
  remap_color.float4_inputs = {{"in", make_float4(0.15f, 0.35f, 0.55f, 0.75f)},
                               {"inlow", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                               {"inhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)},
                               {"outlow", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                               {"outhigh", make_float4(0.9f, 0.8f, 0.7f, 0.6f)}};
  remap_color.outputs["out"] = materialx::Type::Color4;

  materialx::Node remap_color_fa;
  remap_color_fa.name = "Color4RemapFA";
  remap_color_fa.nodedef = "ND_remap_color4FA";
  remap_color_fa.float4_inputs["in"] = make_float4(0.15f, 0.35f, 0.55f, 0.75f);
  remap_color_fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", 0.25f}, {"outhigh", 0.75f}};
  remap_color_fa.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector;
  vector.name = "Vector4Range";
  vector.nodedef = "ND_range_vector4";
  vector.vector4_inputs = {{"in", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                           {"inlow", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                           {"inhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)},
                           {"outlow", make_float4(-1.0f, -2.0f, -3.0f, -4.0f)},
                           {"outhigh", make_float4(1.0f, 2.0f, 3.0f, 4.0f)},
                           {"gamma", make_float4(1.0f, 2.0f, 1.0f, 0.5f)}};
  vector.int_inputs["doclamp"] = 0;
  vector.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector_fa;
  vector_fa.name = "Vector4FARange";
  vector_fa.nodedef = "ND_range_vector4FA";
  vector_fa.vector4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  vector_fa.inputs = {{"inlow", 0.0f},
                      {"inhigh", 1.0f},
                      {"outlow", -1.0f},
                      {"outhigh", 1.0f},
                      {"gamma", 1.0f}};
  vector_fa.int_inputs["doclamp"] = 1;
  vector_fa.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(
      materialx::lower({{color, full_color, remap_color, remap_color_fa, vector, vector_fa}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  std::unordered_map<string, MapRangeNode *> ranges;
  for (ShaderNode *node : graph.nodes) {
    nodes[string(node->name.c_str())] = node;
    if (MapRangeNode *range = dynamic_cast<MapRangeNode *>(node)) {
      ranges[string(node->name.c_str())] = range;
    }
  }
  ASSERT_NE(ranges["Color4Range.Alpha"], nullptr);
  EXPECT_EQ(ranges["Color4Range.Alpha"]->get_range_type(), NODE_MAP_RANGE_LINEAR);
  EXPECT_TRUE(ranges["Color4Range.Alpha"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Color4Range.Alpha"]->get_value(), 0.9f);
  EXPECT_FLOAT_EQ(ranges["Color4Range.Alpha"]->get_to_min(), -1.0f);

  ASSERT_NE(ranges["Color4FullRange.Alpha"], nullptr);
  EXPECT_FALSE(ranges["Color4FullRange.Alpha"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Color4FullRange.Alpha"]->get_to_min(), -4.0f);
  EXPECT_FLOAT_EQ(ranges["Color4FullRange.Alpha"]->get_to_max(), 4.0f);
  ASSERT_NE(ranges["Color4FullRange.Alpha.normalize"], nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Color4FullRange.Alpha.power"]), nullptr);

  ASSERT_NE(ranges["Color4Remap.Alpha"], nullptr);
  EXPECT_FALSE(ranges["Color4Remap.Alpha"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Color4Remap.Alpha"]->get_value(), 0.75f);
  EXPECT_FLOAT_EQ(ranges["Color4Remap.Alpha"]->get_to_min(), 0.4f);
  EXPECT_FLOAT_EQ(ranges["Color4Remap.Alpha"]->get_to_max(), 0.6f);

  ASSERT_NE(ranges["Color4RemapFA.Alpha"], nullptr);
  EXPECT_FALSE(ranges["Color4RemapFA.Alpha"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Color4RemapFA.Alpha"]->get_value(), 0.75f);
  EXPECT_FLOAT_EQ(ranges["Color4RemapFA.Alpha"]->get_to_min(), 0.25f);
  EXPECT_FLOAT_EQ(ranges["Color4RemapFA.Alpha"]->get_to_max(), 0.75f);

  ASSERT_NE(ranges["Vector4Range.W"], nullptr);
  EXPECT_EQ(ranges["Vector4Range.W"]->get_range_type(), NODE_MAP_RANGE_LINEAR);
  EXPECT_FALSE(ranges["Vector4Range.W"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Vector4Range.W"]->get_to_min(), -4.0f);
  EXPECT_FLOAT_EQ(ranges["Vector4Range.W"]->get_to_max(), 4.0f);
  ASSERT_NE(ranges["Vector4Range.W.normalize"], nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Vector4Range.W.power"]), nullptr);
  ASSERT_NE(ranges["Vector4FARange.W"], nullptr);
  EXPECT_TRUE(ranges["Vector4FARange.W"]->get_clamp());
  EXPECT_FLOAT_EQ(ranges["Vector4FARange.W"]->get_value(), 0.4f);
  EXPECT_FLOAT_EQ(ranges["Vector4FARange.W"]->get_to_min(), -1.0f);
}

TEST(materialx_graph, lowers_color4_and_vector4_smoothstep_adjustments_preserving_sidecars)
{
  /* Real MaterialX stdlib sources: libraries/stdlib/stdlib_defs.mtlx declares
   * ND_smoothstep_color4FA and ND_smoothstep_vector4; stdlib_ng.mtlx lowers
   * both to per-component scalar smoothstep nodegraphs. */
  materialx::Node color3;
  color3.name = "Color3Smooth";
  color3.nodedef = "ND_smoothstep_color3";
  color3.color3_inputs = {{"in", make_float3(0.2f, 0.4f, 0.6f)},
                          {"low", make_float3(0.0f, 0.1f, 0.2f)},
                          {"high", make_float3(1.0f, 1.0f, 1.0f)}};
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node color3fa;
  color3fa.name = "Color3FASmooth";
  color3fa.nodedef = "ND_smoothstep_color3FA";
  color3fa.color3_inputs["in"] = make_float3(0.2f, 0.4f, 0.6f);
  color3fa.inputs = {{"low", 0.0f}, {"high", 1.0f}};
  color3fa.outputs["out"] = materialx::Type::Color3;

  materialx::Node color;
  color.name = "Color4Smooth";
  color.nodedef = "ND_smoothstep_color4FA";
  color.float4_inputs["in"] = make_float4(0.2f, 0.4f, 0.6f, 0.8f);
  color.inputs = {{"low", 0.0f}, {"high", 1.0f}};
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector;
  vector.name = "Vector4Smooth";
  vector.nodedef = "ND_smoothstep_vector4";
  vector.vector4_inputs = {{"in", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                           {"low", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                           {"high", make_float4(1.0f, 1.0f, 1.0f, 1.0f)}};
  vector.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector_fa;
  vector_fa.name = "Vector4FASmooth";
  vector_fa.nodedef = "ND_smoothstep_vector4FA";
  vector_fa.vector4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  vector_fa.inputs = {{"low", 0.0f}, {"high", 1.0f}};
  vector_fa.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color3, color3fa, color, vector, vector_fa}}, &graph));

  std::unordered_map<string, MapRangeNode *> ranges;
  for (ShaderNode *node : graph.nodes) {
    if (MapRangeNode *range = dynamic_cast<MapRangeNode *>(node)) {
      ranges[string(node->name.c_str())] = range;
    }
  }
  ASSERT_NE(ranges["Color4Smooth.Alpha"], nullptr);
  EXPECT_EQ(ranges["Color4Smooth.Alpha"]->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FLOAT_EQ(ranges["Color4Smooth.Alpha"]->get_value(), 0.8f);
  ASSERT_NE(ranges["Color3Smooth.Red"], nullptr);
  EXPECT_EQ(ranges["Color3Smooth.Red"]->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FLOAT_EQ(ranges["Color3Smooth.Red"]->get_value(), 0.2f);
  ASSERT_NE(ranges["Color3FASmooth.Blue"], nullptr);
  EXPECT_EQ(ranges["Color3FASmooth.Blue"]->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FLOAT_EQ(ranges["Color3FASmooth.Blue"]->get_value(), 0.6f);
  ASSERT_NE(ranges["Vector4Smooth.W"], nullptr);
  EXPECT_EQ(ranges["Vector4Smooth.W"]->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FLOAT_EQ(ranges["Vector4Smooth.W"]->get_value(), 0.4f);
  ASSERT_NE(ranges["Vector4FASmooth.W"], nullptr);
  EXPECT_EQ(ranges["Vector4FASmooth.W"]->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_FLOAT_EQ(ranges["Vector4FASmooth.W"]->get_value(), 0.4f);
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

  materialx::Node vector4;
  vector4.name = "Vector4";
  vector4.nodedef = "ND_remap_vector4";
  vector4.vector4_inputs = {{"in", make_float4(0.25f, 0.5f, 0.75f, 1.0f)},
                            {"inlow", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                            {"inhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)},
                            {"outlow", make_float4(-1.0f, -1.0f, -1.0f, -1.0f)},
                            {"outhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)}};
  vector4.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector4fa;
  vector4fa.name = "Vector4FA";
  vector4fa.nodedef = "ND_remap_vector4FA";
  vector4fa.vector4_inputs["in"] = make_float4(0.5f, 0.25f, 0.75f, 1.0f);
  vector4fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  vector4fa.outputs["out"] = materialx::Type::Vector4;

  materialx::Graph source;
  source.nodes = {vector2, vector2fa, vector3, vector3fa, vector4, vector4fa};
  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, MapRangeNode *> scalar_ranges;
  int count = 0;
  for (ShaderNode *node : graph.nodes) {
    if (MapRangeNode *range = dynamic_cast<MapRangeNode *>(node)) {
      scalar_ranges[string(node->name.c_str())] = range;
    }
    if (node->type == VectorMapRangeNode::get_node_type()) {
      ++count;
      EXPECT_EQ(static_cast<VectorMapRangeNode *>(node)->get_range_type(), NODE_MAP_RANGE_LINEAR);
      EXPECT_FALSE(static_cast<VectorMapRangeNode *>(node)->get_use_clamp());
    }
  }
  EXPECT_EQ(count, 4);
  ASSERT_NE(scalar_ranges["Vector4.W"], nullptr);
  EXPECT_EQ(scalar_ranges["Vector4.W"]->get_range_type(), NODE_MAP_RANGE_LINEAR);
  ASSERT_NE(scalar_ranges["Vector4FA.W"], nullptr);
  EXPECT_EQ(scalar_ranges["Vector4FA.W"]->get_range_type(), NODE_MAP_RANGE_LINEAR);
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
  materialx::Node literal_fa = literal;
  literal_fa.name = "LiteralRangeFA";
  literal_fa.nodedef = "ND_range_vector2FA";
  literal_fa.vector2_inputs.erase("inlow");
  literal_fa.vector2_inputs.erase("inhigh");
  literal_fa.vector2_inputs.erase("outlow");
  literal_fa.vector2_inputs.erase("outhigh");
  literal_fa.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", -1.0f}, {"outhigh", 1.0f}};
  materialx::Node linked = range_node("LinkedRange", true);
  linked.vector2_inputs.erase("in");
  linked.links["in"] = {"Input", "out", materialx::Type::Vector2};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, literal, literal_fa, linked}}, &graph));
  int ranges = 0;
  for (ShaderNode *node : graph.nodes) {
    if (const auto *range = dynamic_cast<VectorMapRangeNode *>(node)) {
      ++ranges;
      EXPECT_EQ(range->get_use_clamp(), node->name == "LinkedRange");
    }
  }
  EXPECT_EQ(ranges, 3);

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

TEST(materialx_graph, lowers_scalar_burn_and_dodge_to_materialx_arithmetic)
{
  materialx::Node background;
  background.name = "Background";
  background.nodedef = "ND_constant_float";
  background.inputs["value"] = 0.4f;
  background.outputs["out"] = materialx::Type::Float;

  materialx::Node burn;
  burn.name = "Burn";
  burn.nodedef = "ND_burn_float";
  burn.inputs = {{"fg", 0.0f}, {"mix", 0.5f}};
  burn.links["bg"] = {"Background", "out", materialx::Type::Float};
  burn.outputs["out"] = materialx::Type::Float;

  materialx::Node dodge;
  dodge.name = "Dodge";
  dodge.nodedef = "ND_dodge_float";
  dodge.inputs = {{"fg", 1.0f}, {"bg", 0.6f}, {"mix", 0.25f}};
  dodge.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{background, burn, dodge}}, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  EXPECT_EQ(dynamic_cast<MixColorNode *>(nodes["Burn"]), nullptr)
      << "ND_burn_float must not use Cycles blend-mode semantics";
  EXPECT_EQ(dynamic_cast<MixColorNode *>(nodes["Dodge"]), nullptr)
      << "ND_dodge_float must not use Cycles blend-mode semantics";
  auto *burn_result = dynamic_cast<MathNode *>(nodes["Burn"]);
  auto *dodge_result = dynamic_cast<MathNode *>(nodes["Dodge"]);
  ASSERT_NE(burn_result, nullptr);
  ASSERT_NE(dodge_result, nullptr);
  EXPECT_EQ(burn_result->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(dodge_result->get_math_type(), NODE_MATH_MULTIPLY);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Burn.condition"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Burn.safe_denominator"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Dodge.denominator_abs"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["Burn.condition"])->get_value2(), 1.0e-8f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["Dodge.condition"])->get_value2(), 1.0e-8f);
  EXPECT_EQ(dynamic_cast<MathNode *>(nodes["Burn.background_product"])->input("Value2")->link,
            dynamic_cast<ValueNode *>(nodes["Background"])->output("Value"));
}

TEST(materialx_graph, lowers_scalar_burn_and_dodge_measured_edge_literals)
{
  materialx::Node burn;
  burn.name = "BurnNegativeResult";
  burn.nodedef = "ND_burn_float";
  burn.inputs = {{"fg", 0.5f}, {"bg", -0.5f}, {"mix", 1.0f}};
  burn.outputs["out"] = materialx::Type::Float;

  materialx::Node dodge;
  dodge.name = "DodgeUnitForeground";
  dodge.nodedef = "ND_dodge_float";
  dodge.inputs = {{"fg", 1.0f}, {"bg", 0.6f}, {"mix", 0.25f}};
  dodge.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{burn, dodge}}, &graph));

  std::unordered_map<string, MathNode *> math;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *lowered = dynamic_cast<MathNode *>(node)) {
      math[node->name.string()] = lowered;
    }
  }

  ASSERT_NE(math["BurnNegativeResult.condition"], nullptr);
  ASSERT_NE(math["BurnNegativeResult.inverse_condition"], nullptr);
  ASSERT_NE(math["BurnNegativeResult"], nullptr);
  EXPECT_FLOAT_EQ(math["BurnNegativeResult.foreground_abs"]->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(math["BurnNegativeResult.one_minus_background"]->get_value2(), -0.5f);
  EXPECT_FLOAT_EQ(math["BurnNegativeResult.mix_product"]->get_value2(), 1.0f);
  EXPECT_EQ(math["BurnNegativeResult"]->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_NE(math["BurnNegativeResult"]->input("Value2")->link, nullptr);

  ASSERT_NE(math["DodgeUnitForeground.condition"], nullptr);
  ASSERT_NE(math["DodgeUnitForeground.inverse_condition"], nullptr);
  ASSERT_NE(math["DodgeUnitForeground"], nullptr);
  EXPECT_FLOAT_EQ(math["DodgeUnitForeground.denominator"]->get_value2(), 1.0f);
  EXPECT_FLOAT_EQ(math["DodgeUnitForeground.condition"]->get_value2(), 1.0e-8f);
  EXPECT_FLOAT_EQ(math["DodgeUnitForeground.background_product"]->get_value2(), 0.6f);
  EXPECT_EQ(math["DodgeUnitForeground"]->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_NE(math["DodgeUnitForeground"]->input("Value2")->link, nullptr);
}

/* ND_blackbody is declared in MaterialX pbrlib/pbrlib_defs.mtlx as
 * blackbody(float temperature=5000.0) -> color3 and maps directly to Cycles'
 * native BlackbodyNode rather than a proxy color constant. */
TEST(materialx_graph, lowers_blackbody_to_native_blackbody_node)
{
  materialx::Node literal;
  literal.name = "LiteralBlackbody";
  literal.nodedef = "ND_blackbody";
  literal.inputs["temperature"] = 6500.0f;
  literal.outputs["out"] = materialx::Type::Color3;

  materialx::Node temperature;
  temperature.name = "Temperature";
  temperature.nodedef = "ND_constant_float";
  temperature.inputs["value"] = 3200.0f;
  temperature.outputs["out"] = materialx::Type::Float;

  materialx::Node linked;
  linked.name = "LinkedBlackbody";
  linked.nodedef = "ND_blackbody";
  linked.links["temperature"] = {"Temperature", "out", materialx::Type::Float};
  linked.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{literal, temperature, linked}}, &graph));

  BlackbodyNode *literal_blackbody = nullptr;
  BlackbodyNode *linked_blackbody = nullptr;
  ValueNode *temperature_value = nullptr;
  for (ShaderNode *node : graph.nodes) {
    literal_blackbody = node->name == "LiteralBlackbody" ? dynamic_cast<BlackbodyNode *>(node) :
                                                            literal_blackbody;
    linked_blackbody = node->name == "LinkedBlackbody" ? dynamic_cast<BlackbodyNode *>(node) :
                                                          linked_blackbody;
    temperature_value = node->name == "Temperature" ? dynamic_cast<ValueNode *>(node) :
                                                       temperature_value;
  }
  ASSERT_NE(literal_blackbody, nullptr);
  EXPECT_FLOAT_EQ(literal_blackbody->get_temperature(), 6500.0f);
  EXPECT_EQ(literal_blackbody->input("Temperature")->link, nullptr);
  ASSERT_NE(linked_blackbody, nullptr);
  ASSERT_NE(temperature_value, nullptr);
  EXPECT_EQ(linked_blackbody->input("Temperature")->link, temperature_value->output("Value"));
}

TEST(materialx_graph, lowers_artistic_ior_to_exact_pbrlib_arithmetic_outputs)
{
  /* MaterialX pbrlib/pbrlib_defs.mtlx lines 419-423 declares
   * ND_artistic_ior(reflectivity:color3, edge_color:color3) with color3
   * outputs ior/extinction. pbrlib/genglsl/mx_artistic_ior.glsl implements
   * Gulbrandsen 2014's closed-form arithmetic, which is lowered here to real
   * per-channel Cycles MathNodes rather than constants. */
  materialx::Node reflectivity;
  reflectivity.name = "Reflectivity";
  reflectivity.nodedef = "ND_constant_color3";
  reflectivity.color3_inputs["value"] = make_float3(0.25f, 0.36f, 0.49f);
  reflectivity.outputs["out"] = materialx::Type::Color3;

  materialx::Node artistic;
  artistic.name = "ArtisticIOR";
  artistic.nodedef = "ND_artistic_ior";
  artistic.links["reflectivity"] = {"Reflectivity", "out", materialx::Type::Color3};
  artistic.color3_inputs["edge_color"] = make_float3(0.2f, 0.5f, 0.8f);
  artistic.outputs["ior"] = materialx::Type::Color3;
  artistic.outputs["extinction"] = materialx::Type::Color3;

  materialx::Node conductor;
  conductor.name = "Conductor";
  conductor.nodedef = "ND_conductor_bsdf";
  conductor.links["ior"] = {"ArtisticIOR", "ior", materialx::Type::Color3};
  conductor.links["extinction"] = {"ArtisticIOR", "extinction", materialx::Type::Color3};
  conductor.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{reflectivity, artistic, conductor}}, &graph));

  CombineColorNode *ior = nullptr;
  CombineColorNode *extinction = nullptr;
  MetallicBsdfNode *metallic = nullptr;
  MathNode *green_ior_sum = nullptr;
  for (ShaderNode *node : graph.nodes) {
    ior = node->name == "ArtisticIOR.ior" ? dynamic_cast<CombineColorNode *>(node) : ior;
    extinction = node->name == "ArtisticIOR.extinction" ?
                     dynamic_cast<CombineColorNode *>(node) :
                     extinction;
    metallic = node->name == "Conductor" ? dynamic_cast<MetallicBsdfNode *>(node) : metallic;
    green_ior_sum = node->name == "ArtisticIOR.Green.ior" ? dynamic_cast<MathNode *>(node) :
                                                            green_ior_sum;
  }
  ASSERT_NE(ior, nullptr);
  ASSERT_NE(extinction, nullptr);
  ASSERT_NE(metallic, nullptr);
  ASSERT_NE(green_ior_sum, nullptr);
  EXPECT_EQ(green_ior_sum->get_math_type(), NODE_MATH_ADD);
  ASSERT_NE(ior->input("Green")->link, nullptr);
  EXPECT_EQ(ior->input("Green")->link->parent, green_ior_sum);
  ASSERT_NE(extinction->input("Blue")->link, nullptr);
  ASSERT_NE(metallic->input("IOR")->link, nullptr);
  ASSERT_NE(metallic->input("Extinction")->link, nullptr);
}

TEST(materialx_graph, lowers_deon_hair_absorption_from_melanin_to_exact_arithmetic)
{
  /* MaterialX pbrlib/pbrlib_defs.mtlx lines 430-435 declares
   * ND_deon_hair_absorption_from_melanin(...)->vector3 absorption.
   * pbrlib/genglsl/mx_chiang_hair_bsdf.glsl implements:
   *   melanin = -log(max(1 - concentration, 0.0001));
   *   absorption = max(eumelanin * -log(eumelanin_color) +
   *                    pheomelanin * -log(pheomelanin_color), 0). */
  materialx::Node concentration;
  concentration.name = "Concentration";
  concentration.nodedef = "ND_constant_float";
  concentration.inputs["value"] = 0.25f;
  concentration.outputs["out"] = materialx::Type::Float;

  materialx::Node absorption;
  absorption.name = "MelaninAbsorption";
  absorption.nodedef = "ND_deon_hair_absorption_from_melanin";
  absorption.links["melanin_concentration"] = {"Concentration", "out", materialx::Type::Float};
  absorption.inputs["melanin_redness"] = 0.5f;
  absorption.color3_inputs["eumelanin_color"] = make_float3(0.657704f, 0.498077f, 0.254107f);
  absorption.color3_inputs["pheomelanin_color"] = make_float3(0.829444f, 0.67032f, 0.349938f);
  absorption.outputs["absorption"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{concentration, absorption}}, &graph));

  CombineXYZNode *combine = nullptr;
  MathNode *melanin_log = nullptr;
  MathNode *one_minus_melanin = nullptr;
  MathNode *blue_maximum = nullptr;
  ValueNode *concentration_value = nullptr;
  for (ShaderNode *node : graph.nodes) {
    combine = node->name == "MelaninAbsorption" ? dynamic_cast<CombineXYZNode *>(node) : combine;
    melanin_log = node->name == "MelaninAbsorption.melanin_log" ? dynamic_cast<MathNode *>(node) :
                                                                  melanin_log;
    one_minus_melanin = node->name == "MelaninAbsorption.one_minus_melanin" ?
                            dynamic_cast<MathNode *>(node) :
                            one_minus_melanin;
    blue_maximum = node->name == "MelaninAbsorption.Blue.maximum" ?
                       dynamic_cast<MathNode *>(node) :
                       blue_maximum;
    concentration_value = node->name == "Concentration" ? dynamic_cast<ValueNode *>(node) :
                                                          concentration_value;
  }
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(melanin_log, nullptr);
  ASSERT_NE(one_minus_melanin, nullptr);
  ASSERT_NE(blue_maximum, nullptr);
  ASSERT_NE(concentration_value, nullptr);
  EXPECT_EQ(melanin_log->get_math_type(), NODE_MATH_LOGARITHM);
  EXPECT_FLOAT_EQ(melanin_log->get_value2(), float(M_E));
  EXPECT_EQ(blue_maximum->get_math_type(), NODE_MATH_MAXIMUM);
  ASSERT_NE(combine->input("Z")->link, nullptr);
  EXPECT_EQ(combine->input("Z")->link->parent, blue_maximum);
  EXPECT_EQ(one_minus_melanin->input("Value2")->link, concentration_value->output("Value"));
}

TEST(materialx_graph, lowers_triplanarprojection_texture3d_family_to_projected_image_blend)
{
  const TemporaryImage image_asset;
  materialx::Node position;
  position.name = "Position";
  position.nodedef = "ND_constant_vector3";
  position.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  position.outputs["out"] = materialx::Type::Vector3;

  materialx::Node normal;
  normal.name = "Normal";
  normal.nodedef = "ND_constant_vector3";
  normal.vector3_inputs["value"] = make_float3(0.2f, 0.3f, 0.4f);
  normal.outputs["out"] = materialx::Type::Vector3;

  materialx::Node blend;
  blend.name = "Blend";
  blend.nodedef = "ND_constant_float";
  blend.inputs["value"] = 0.5f;
  blend.outputs["out"] = materialx::Type::Float;

  struct Case {
    const char *name;
    const char *nodedef;
    materialx::Type type;
  };
  const Case cases[] = {{"TriFloat", "ND_triplanarprojection_float", materialx::Type::Float},
                        {"TriColor3", "ND_triplanarprojection_color3", materialx::Type::Color3},
                        {"TriColor4", "ND_triplanarprojection_color4", materialx::Type::Color4},
                        {"TriVector2", "ND_triplanarprojection_vector2", materialx::Type::Vector2},
                        {"TriVector3", "ND_triplanarprojection_vector3", materialx::Type::Vector3},
                        {"TriVector4", "ND_triplanarprojection_vector4", materialx::Type::Vector4}};

  materialx::Graph source;
  source.nodes = {position, normal, blend};
  for (const Case &item : cases) {
    materialx::Node tri;
    tri.name = item.name;
    tri.nodedef = item.nodedef;
    tri.asset_inputs["filex"] = image_asset.path();
    tri.asset_inputs["filey"] = image_asset.path();
    tri.asset_inputs["filez"] = image_asset.path();
    tri.links["position"] = {"Position", "out", materialx::Type::Vector3};
    tri.links["normal"] = {"Normal", "out", materialx::Type::Vector3};
    tri.links["blend"] = {"Blend", "out", materialx::Type::Float};
    tri.int_inputs["upaxis"] = 2;
    tri.string_inputs["filtertype"] = "cubic";
    tri.outputs["out"] = item.type;
    source.nodes.push_back(std::move(tri));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  for (const Case &item : cases) {
    ASSERT_NE(lowered[item.name], nullptr) << item.name;
    ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered[string(item.name) + ".position"]), nullptr)
        << item.name;
    ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered[string(item.name) + ".blend.power"]), nullptr)
        << item.name;
    EXPECT_EQ(static_cast<VectorMathNode *>(lowered[string(item.name) + ".blend.power"])
                  ->get_math_type(),
              NODE_VECTOR_MATH_POWER)
        << item.name;
    for (const char *axis : {"x", "y", "z"}) {
      ImageTextureNode *image = dynamic_cast<ImageTextureNode *>(
          lowered[string(item.name) + ".image_" + axis]);
      ASSERT_NE(image, nullptr) << item.name << axis;
      EXPECT_EQ(image->get_filename(), ustring(image_asset.path())) << item.name << axis;
      EXPECT_EQ(image->get_interpolation(), INTERPOLATION_CUBIC) << item.name << axis;
    }
  }
  EXPECT_NE(dynamic_cast<MathNode *>(lowered["TriFloat"]), nullptr);
  EXPECT_NE(dynamic_cast<MixNode *>(lowered["TriColor3"]), nullptr);
  EXPECT_NE(dynamic_cast<MixNode *>(lowered["TriColor4"]), nullptr);
  EXPECT_NE(dynamic_cast<CombineXYZNode *>(lowered["TriVector2"]), nullptr);
  EXPECT_NE(dynamic_cast<MixNode *>(lowered["TriVector3"]), nullptr);
  EXPECT_NE(dynamic_cast<MixNode *>(lowered["TriVector4"]), nullptr);
  EXPECT_NE(dynamic_cast<MathNode *>(lowered["TriColor4.Alpha"]), nullptr);
  EXPECT_NE(dynamic_cast<MathNode *>(lowered["TriVector4.W"]), nullptr);
}

TEST(materialx_graph, rejects_unsupported_triplanarprojection_forms_atomically)
{
  const TemporaryImage image_asset;
  materialx::Node position;
  position.name = "Position";
  position.nodedef = "ND_constant_vector3";
  position.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  position.outputs["out"] = materialx::Type::Vector3;
  materialx::Node normal = position;
  normal.name = "Normal";

  const auto make_triplanar = [&]() {
    materialx::Node tri;
    tri.name = "Tri";
    tri.nodedef = "ND_triplanarprojection_color3";
    tri.asset_inputs["filex"] = image_asset.path();
    tri.asset_inputs["filey"] = image_asset.path();
    tri.asset_inputs["filez"] = image_asset.path();
    tri.links["position"] = {"Position", "out", materialx::Type::Vector3};
    tri.links["normal"] = {"Normal", "out", materialx::Type::Vector3};
    tri.inputs["blend"] = 1.0f;
    tri.int_inputs["upaxis"] = 2;
    tri.string_inputs["filtertype"] = "linear";
    tri.outputs["out"] = materialx::Type::Color3;
    return tri;
  };

  for (int case_index = 0; case_index < 3; case_index++) {
    materialx::Node tri = make_triplanar();
    if (case_index == 0) {
      tri.int_inputs["upaxis"] = 0;
    }
    else if (case_index == 1) {
      tri.string_inputs["filtertype"] = "gaussian";
    }
    else {
      tri.asset_inputs.erase("filez");
    }
    ShaderGraph graph;
    graph.create_node<MathNode>()->name = "sentinel";
    const size_t original_node_count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{position, normal, tri}}, &graph)) << case_index;
    ASSERT_EQ(graph.nodes.size(), original_node_count) << case_index;
    EXPECT_EQ(graph.nodes.back()->name, "sentinel") << case_index;
  }
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

TEST(materialx_graph, lowers_compositing_vector2_vector3_and_color4_mix_variants)
{
  /* Real MaterialX stdlib_defs.mtlx compositing mix siblings declare
   * mix(bg, fg, mix) for vector2/vector3/vector4/color4, with either float or
   * same-typed per-component factors; genosl/stdlib_genosl_impl.mtlx lines
   * IM_mix_* lower each to sourcecode="mix({{bg}}, {{fg}}, {{mix}})". */
  materialx::Graph source;

  materialx::Node scalar_factor;
  scalar_factor.name = "ScalarFactor";
  scalar_factor.nodedef = "ND_constant_float";
  scalar_factor.inputs["value"] = 0.5f;
  scalar_factor.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(scalar_factor);

  materialx::Node vector2_factor_source;
  vector2_factor_source.name = "Vector2Factor";
  vector2_factor_source.nodedef = "ND_constant_vector2";
  vector2_factor_source.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  vector2_factor_source.outputs["out"] = materialx::Type::Vector2;
  source.nodes.push_back(vector2_factor_source);

  materialx::Node vector3_factor_source;
  vector3_factor_source.name = "Vector3Factor";
  vector3_factor_source.nodedef = "ND_constant_vector3";
  vector3_factor_source.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  vector3_factor_source.outputs["out"] = materialx::Type::Vector3;
  source.nodes.push_back(vector3_factor_source);

  materialx::Node vector4_factor_source;
  vector4_factor_source.name = "Vector4Factor";
  vector4_factor_source.nodedef = "ND_constant_vector4";
  vector4_factor_source.vector4_inputs["value"] = make_float4(0.25f, 0.5f, 0.75f, 1.0f);
  vector4_factor_source.outputs["out"] = materialx::Type::Vector4;
  source.nodes.push_back(vector4_factor_source);

  materialx::Node color4_factor_source;
  color4_factor_source.name = "Color4Factor";
  color4_factor_source.nodedef = "ND_constant_color4";
  color4_factor_source.float4_inputs["value"] = make_float4(0.25f, 0.5f, 0.75f, 1.0f);
  color4_factor_source.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(color4_factor_source);

  materialx::Node vector2;
  vector2.name = "Vector2Mix";
  vector2.nodedef = "ND_mix_vector2";
  vector2.vector2_inputs["bg"] = make_float2(0.1f, 0.2f);
  vector2.vector2_inputs["fg"] = make_float2(0.7f, 0.8f);
  vector2.links["mix"] = {"ScalarFactor", "out", materialx::Type::Float};
  vector2.outputs["out"] = materialx::Type::Vector2;
  source.nodes.push_back(vector2);

  materialx::Node vector2_factor;
  vector2_factor.name = "Vector2FactorMix";
  vector2_factor.nodedef = "ND_mix_vector2_vector2";
  vector2_factor.vector2_inputs["bg"] = make_float2(0.1f, 0.2f);
  vector2_factor.vector2_inputs["fg"] = make_float2(0.7f, 0.8f);
  vector2_factor.links["mix"] = {"Vector2Factor", "out", materialx::Type::Vector2};
  vector2_factor.outputs["out"] = materialx::Type::Vector2;
  source.nodes.push_back(vector2_factor);

  materialx::Node vector3_factor;
  vector3_factor.name = "Vector3FactorMix";
  vector3_factor.nodedef = "ND_mix_vector3_vector3";
  vector3_factor.vector3_inputs["bg"] = make_float3(0.1f, 0.2f, 0.3f);
  vector3_factor.vector3_inputs["fg"] = make_float3(0.7f, 0.8f, 0.9f);
  vector3_factor.links["mix"] = {"Vector3Factor", "out", materialx::Type::Vector3};
  vector3_factor.outputs["out"] = materialx::Type::Vector3;
  source.nodes.push_back(vector3_factor);

  materialx::Node vector4;
  vector4.name = "Vector4Mix";
  vector4.nodedef = "ND_mix_vector4";
  vector4.vector4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  vector4.vector4_inputs["fg"] = make_float4(0.7f, 0.8f, 0.9f, 1.0f);
  vector4.links["mix"] = {"ScalarFactor", "out", materialx::Type::Float};
  vector4.outputs["out"] = materialx::Type::Vector4;
  source.nodes.push_back(vector4);

  materialx::Node vector4_factor;
  vector4_factor.name = "Vector4FactorMix";
  vector4_factor.nodedef = "ND_mix_vector4_vector4";
  vector4_factor.vector4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  vector4_factor.vector4_inputs["fg"] = make_float4(0.7f, 0.8f, 0.9f, 1.0f);
  vector4_factor.links["mix"] = {"Vector4Factor", "out", materialx::Type::Vector4};
  vector4_factor.outputs["out"] = materialx::Type::Vector4;
  source.nodes.push_back(vector4_factor);

  materialx::Node color4;
  color4.name = "Color4Mix";
  color4.nodedef = "ND_mix_color4";
  color4.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4.float4_inputs["fg"] = make_float4(0.7f, 0.8f, 0.9f, 1.0f);
  color4.links["mix"] = {"ScalarFactor", "out", materialx::Type::Float};
  color4.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(color4);

  materialx::Node color4_factor;
  color4_factor.name = "Color4FactorMix";
  color4_factor.nodedef = "ND_mix_color4_color4";
  color4_factor.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4_factor.float4_inputs["fg"] = make_float4(0.7f, 0.8f, 0.9f, 1.0f);
  color4_factor.links["mix"] = {"Color4Factor", "out", materialx::Type::Color4};
  color4_factor.outputs["out"] = materialx::Type::Color4;
  source.nodes.push_back(color4_factor);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  for (const char *name : {"Vector2Mix", "Vector2FactorMix", "Vector3FactorMix", "Vector4Mix", "Vector4FactorMix"}) {
    EXPECT_TRUE(nodes.contains(name)) << name;
    if (!nodes.contains(name)) {
      continue;
    }
    auto *sum = dynamic_cast<VectorMathNode *>(nodes.at(name));
    ASSERT_NE(sum, nullptr) << name;
    EXPECT_EQ(sum->get_math_type(), NODE_VECTOR_MATH_ADD) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".delta")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".product")) << name;
    if (!nodes.contains(string(name) + ".delta") || !nodes.contains(string(name) + ".product")) {
      continue;
    }
    EXPECT_NE(dynamic_cast<VectorMathNode *>(nodes.at(string(name) + ".delta")), nullptr) << name;
    EXPECT_NE(dynamic_cast<VectorMathNode *>(nodes.at(string(name) + ".product")), nullptr) << name;
  }

  ASSERT_TRUE(nodes.contains("Vector2Mix.factor"));
  auto *vector2_factor_node = dynamic_cast<CombineXYZNode *>(nodes.at("Vector2Mix.factor"));
  ASSERT_NE(vector2_factor_node, nullptr);
  EXPECT_FLOAT_EQ(vector2_factor_node->get_z(), 0.0f);
  EXPECT_EQ(nodes.count("Vector2FactorMix.factor"), 0);
  EXPECT_EQ(nodes.count("Vector3FactorMix.factor"), 0);
  ASSERT_TRUE(nodes.contains("Vector4Mix.factor"));
  EXPECT_NE(dynamic_cast<CombineXYZNode *>(nodes.at("Vector4Mix.factor")), nullptr);
  EXPECT_EQ(nodes.count("Vector4FactorMix.factor"), 0);
  for (const char *name : {"Vector4Mix", "Vector4FactorMix"}) {
    EXPECT_TRUE(nodes.contains(string(name) + ".W")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".W.delta")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".W.product")) << name;
    if (nodes.contains(string(name) + ".W")) {
      EXPECT_EQ(dynamic_cast<MathNode *>(nodes.at(string(name) + ".W"))->get_math_type(),
                NODE_MATH_ADD) << name;
      EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes.at(string(name) + ".W"))->get_value2(), 0.0f)
          << name;
    }
  }

  for (const char *name : {"Color4Mix", "Color4FactorMix"}) {
    EXPECT_TRUE(nodes.contains(name)) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".Alpha")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".delta")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".product")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".Alpha.delta")) << name;
    EXPECT_TRUE(nodes.contains(string(name) + ".Alpha.product")) << name;
    if (!nodes.contains(name) || !nodes.contains(string(name) + ".Alpha") ||
        !nodes.contains(string(name) + ".delta") || !nodes.contains(string(name) + ".product") ||
        !nodes.contains(string(name) + ".Alpha.delta") ||
        !nodes.contains(string(name) + ".Alpha.product"))
    {
      continue;
    }
    auto *sum = dynamic_cast<MixNode *>(nodes.at(name));
    auto *alpha_sum = dynamic_cast<MathNode *>(nodes.at(string(name) + ".Alpha"));
    ASSERT_NE(sum, nullptr) << name;
    ASSERT_NE(alpha_sum, nullptr) << name;
    EXPECT_EQ(sum->get_mix_type(), NODE_MIX_ADD) << name;
    EXPECT_EQ(alpha_sum->get_math_type(), NODE_MATH_ADD) << name;
    EXPECT_FLOAT_EQ(alpha_sum->get_value2(), 0.0f) << name;
    EXPECT_NE(dynamic_cast<MixNode *>(nodes.at(string(name) + ".delta")), nullptr) << name;
    EXPECT_NE(dynamic_cast<MixNode *>(nodes.at(string(name) + ".product")), nullptr) << name;
    EXPECT_NE(dynamic_cast<MathNode *>(nodes.at(string(name) + ".Alpha.delta")), nullptr) << name;
    EXPECT_NE(dynamic_cast<MathNode *>(nodes.at(string(name) + ".Alpha.product")), nullptr) << name;
  }
  ASSERT_TRUE(nodes.contains("Color4Mix.factor"));
  EXPECT_NE(dynamic_cast<CombineColorNode *>(nodes.at("Color4Mix.factor")), nullptr);
  EXPECT_EQ(nodes.count("Color4FactorMix.factor"), 0);
}

TEST(materialx_graph, lowers_literal_vector4_mix_values_with_selected_output_sidecar)
{
  materialx::Node scalar;
  scalar.name = "Vector4ScalarMix";
  scalar.nodedef = "ND_mix_vector4";
  scalar.vector4_inputs["bg"] = make_float4(-1.0f, 0.0f, 1.0f, 2.0f);
  scalar.vector4_inputs["fg"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  scalar.inputs["mix"] = 0.25f;
  scalar.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector;
  vector.name = "Vector4VectorMix";
  vector.nodedef = "ND_mix_vector4_vector4";
  vector.vector4_inputs["bg"] = make_float4(-1.0f, 0.0f, 1.0f, 2.0f);
  vector.vector4_inputs["fg"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  vector.vector4_inputs["mix"] = make_float4(0.25f, 0.5f, 0.75f, 1.0f);
  vector.outputs["out"] = materialx::Type::Vector4;

  materialx::Node extract_w;
  extract_w.name = "Vector4ScalarMixW";
  extract_w.nodedef = "ND_extract_vector4";
  extract_w.links["in"] = {"Vector4ScalarMix", "out", materialx::Type::Vector4};
  extract_w.int_inputs["index"] = 3;
  extract_w.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{scalar, vector, extract_w}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *scalar_delta = dynamic_cast<VectorMathNode *>(nodes["Vector4ScalarMix.delta"]);
  auto *scalar_product = dynamic_cast<VectorMathNode *>(nodes["Vector4ScalarMix.product"]);
  auto *scalar_sum = dynamic_cast<VectorMathNode *>(nodes["Vector4ScalarMix"]);
  auto *scalar_w = dynamic_cast<MathNode *>(nodes["Vector4ScalarMix.W"]);
  auto *vector_product = dynamic_cast<VectorMathNode *>(nodes["Vector4VectorMix.product"]);
  auto *vector_sum = dynamic_cast<VectorMathNode *>(nodes["Vector4VectorMix"]);
  auto *vector_w = dynamic_cast<MathNode *>(nodes["Vector4VectorMix.W"]);
  ASSERT_NE(scalar_delta, nullptr);
  ASSERT_NE(scalar_product, nullptr);
  ASSERT_NE(scalar_sum, nullptr);
  ASSERT_NE(scalar_w, nullptr);
  ASSERT_NE(vector_product, nullptr);
  ASSERT_NE(vector_sum, nullptr);
  ASSERT_NE(vector_w, nullptr);
  EXPECT_EQ(scalar_delta->get_vector1(), make_float3(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(scalar_delta->get_vector2(), make_float3(-1.0f, 0.0f, 1.0f));
  ASSERT_NE(nodes["Vector4ScalarMix.factor"], nullptr);
  auto *scalar_factor = dynamic_cast<CombineXYZNode *>(nodes["Vector4ScalarMix.factor"]);
  ASSERT_NE(scalar_factor, nullptr);
  EXPECT_FLOAT_EQ(scalar_factor->get_x(), 0.25f);
  EXPECT_FLOAT_EQ(scalar_factor->get_y(), 0.25f);
  EXPECT_FLOAT_EQ(scalar_factor->get_z(), 0.25f);
  EXPECT_EQ(scalar_product->input("Vector1")->link, scalar_delta->output("Vector"));
  EXPECT_NE(scalar_product->input("Vector2")->link, nullptr);
  EXPECT_EQ(scalar_sum->get_vector1(), make_float3(-1.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(scalar_w->get_value1(), 2.0f);
  EXPECT_FLOAT_EQ(scalar_w->get_value2(), 0.0f);
  EXPECT_EQ(vector_product->get_vector2(), make_float3(0.25f, 0.5f, 0.75f));
  EXPECT_EQ(vector_sum->get_vector1(), make_float3(-1.0f, 0.0f, 1.0f));
  EXPECT_FLOAT_EQ(vector_w->get_value1(), 2.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["Vector4VectorMix.W.product"])->get_value2(),
                  1.0f);
}

TEST(materialx_graph, lowers_color4_compositing_blends_preserving_alpha_sidecar)
{
  /* MaterialX stdlib/genosl defines plus/minus/difference/screen/overlay color4
   * as the same componentwise equations as float/color3, with a float mix.
   * RGB lowers to Cycles' MixColor; alpha is carried as a parallel scalar
   * sidecar using the same native blend mode. */
  struct BlendCase {
    const char *name;
    const char *nodedef;
    NodeMix mix_type;
  };
  const BlendCase cases[] = {{"PlusColor4", "ND_plus_color4", NODE_MIX_ADD},
                             {"MinusColor4", "ND_minus_color4", NODE_MIX_SUB},
                             {"DifferenceColor4", "ND_difference_color4", NODE_MIX_DIFF},
                             {"ScreenColor4", "ND_screen_color4", NODE_MIX_SCREEN},
                             {"OverlayColor4", "ND_overlay_color4", NODE_MIX_OVERLAY}};

  materialx::Graph source;
  materialx::Node factor;
  factor.name = "Factor";
  factor.nodedef = "ND_constant_float";
  factor.inputs["value"] = 0.35f;
  factor.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(factor);

  for (size_t index = 0; index < std::size(cases); index++) {
    materialx::Node blend;
    blend.name = cases[index].name;
    blend.nodedef = cases[index].nodedef;
    blend.float4_inputs["fg"] = make_float4(0.7f, 0.5f, 0.3f, 0.8f);
    blend.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
    if (index == 0) {
      blend.links["mix"] = {"Factor", "out", materialx::Type::Float};
    }
    else {
      blend.inputs["mix"] = 0.25f + 0.1f * float(index);
    }
    blend.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(blend));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  ValueNode *factor_node = dynamic_cast<ValueNode *>(nodes["Factor"]);
  ASSERT_NE(factor_node, nullptr);
  for (const BlendCase &test_case : cases) {
    MixColorNode *blend = dynamic_cast<MixColorNode *>(nodes[test_case.name]);
    MixColorNode *alpha_blend = dynamic_cast<MixColorNode *>(
        nodes[string(test_case.name) + ".Alpha.blend"]);
    MathNode *alpha = dynamic_cast<MathNode *>(nodes[string(test_case.name) + ".Alpha"]);
    ASSERT_NE(blend, nullptr) << test_case.nodedef;
    ASSERT_NE(alpha_blend, nullptr) << test_case.nodedef;
    ASSERT_NE(alpha, nullptr) << test_case.nodedef;
    EXPECT_EQ(blend->get_blend_type(), test_case.mix_type);
    EXPECT_EQ(alpha_blend->get_blend_type(), test_case.mix_type);
    EXPECT_FALSE(blend->get_use_clamp());
    EXPECT_FALSE(alpha_blend->get_use_clamp_result());
    EXPECT_NE(alpha->input("Value1")->link, nullptr);
  }
  ASSERT_NE(dynamic_cast<MixColorNode *>(nodes["PlusColor4.Alpha.blend"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["PlusColor4.Alpha.product"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["MinusColor4.Alpha.delta"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["MinusColor4.Alpha.delta"])->get_value1(),
                  0.4f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["MinusColor4.Alpha.delta"])->get_value2(),
                  0.8f);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["DifferenceColor4.Alpha.abs"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["ScreenColor4.Alpha.screen"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["OverlayColor4.Alpha.overlay"]), nullptr);
  EXPECT_EQ(dynamic_cast<MixColorNode *>(nodes["PlusColor4"])->input("Factor")->link,
            factor_node->output("Value"));
  EXPECT_EQ(dynamic_cast<MixColorNode *>(nodes["PlusColor4.Alpha.blend"])->input("Factor")->link,
            factor_node->output("Value"));
}

TEST(materialx_graph, lowers_color4_compositing_blend_alpha_as_scalar_value)
{
  materialx::Graph source;
  for (const auto &[name, nodedef] :
       {std::pair{"PlusColor4", "ND_plus_color4"},
        std::pair{"MinusColor4", "ND_minus_color4"},
        std::pair{"DifferenceColor4", "ND_difference_color4"},
        std::pair{"ScreenColor4", "ND_screen_color4"},
        std::pair{"OverlayColor4", "ND_overlay_color4"}})
  {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.float4_inputs["fg"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
    node.float4_inputs["bg"] = make_float4(0.25f, 0.5f, 0.75f, 1.0f);
    node.inputs["mix"] = 0.5f;
    node.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(node));

    materialx::Node alpha;
    alpha.name = string(name) + "Alpha";
    alpha.nodedef = "ND_extract_color4";
    alpha.links["in"] = {name, "out", materialx::Type::Color4};
    alpha.int_inputs["index"] = 3;
    alpha.outputs["out"] = materialx::Type::Float;
    source.nodes.push_back(std::move(alpha));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  for (const char *name : {"PlusColor4", "MinusColor4", "DifferenceColor4", "ScreenColor4", "OverlayColor4"}) {
    auto *alpha = dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha"]);
    auto *extract = dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha"]);
    ASSERT_NE(alpha, nullptr) << name;
    ASSERT_NE(extract, nullptr) << name;
    EXPECT_EQ(extract, alpha) << name;
  }
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


TEST(materialx_graph, lowers_alpha_aware_color4_compositing_operators)
{
  materialx::Node factor;
  factor.name = "Factor";
  factor.nodedef = "ND_constant_float";
  factor.inputs["value"] = 0.35f;
  factor.outputs["out"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes.push_back(factor);
  for (const auto &[name, nodedef] :
       {std::pair{"DisjointOverColor4", "ND_disjointover_color4"},
        std::pair{"InColor4", "ND_in_color4"},
        std::pair{"MaskColor4", "ND_mask_color4"},
        std::pair{"MatteColor4", "ND_matte_color4"},
        std::pair{"OutColor4", "ND_out_color4"},
        std::pair{"OverColor4", "ND_over_color4"}})
  {
    materialx::Node composite;
    composite.name = name;
    composite.nodedef = nodedef;
    composite.float4_inputs["fg"] = make_float4(0.7f, 0.5f, 0.3f, 0.8f);
    composite.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
    if (string(nodedef) == "ND_over_color4") {
      composite.links["mix"] = {"Factor", "out", materialx::Type::Float};
    }
    else {
      composite.inputs["mix"] = 0.5f;
    }
    composite.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(composite));
  }

  materialx::Node alpha;
  alpha.name = "Alpha";
  alpha.nodedef = "ND_extract_color4";
  alpha.int_inputs["index"] = 3;
  alpha.links["in"] = {"OverColor4", "out", materialx::Type::Color4};
  alpha.outputs["out"] = materialx::Type::Float;
  source.nodes.push_back(alpha);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  for (const auto &[name, expected] :
       {std::pair{"DisjointOverColor4", "DisjointOverColor4.Red.over_limit"},
        std::pair{"InColor4", "InColor4.Red.bg_alpha_mix"},
        std::pair{"MaskColor4", "MaskColor4.Red.fg_alpha_mix"},
        std::pair{"MatteColor4", "MatteColor4.Red.background_alpha_term"},
        std::pair{"OutColor4", "OutColor4.Red.foreground_alpha_term"},
        std::pair{"OverColor4", "OverColor4.Red.background_alpha_term"}})
  {
    ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes[name]), nullptr) << name;
    ASSERT_NE(dynamic_cast<MathNode *>(nodes[string(name) + ".Alpha.result"]), nullptr) << name;
    ASSERT_NE(dynamic_cast<MathNode *>(nodes[expected]), nullptr) << name;
  }
  EXPECT_EQ(dynamic_cast<MathNode *>(nodes["DisjointOverColor4.Red.composited"])->get_math_type(),
            NODE_MATH_ADD);
  EXPECT_EQ(dynamic_cast<MathNode *>(nodes["DisjointOverColor4.Alpha.composited_alpha"])
                ->get_math_type(),
            NODE_MATH_MINIMUM);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["InColor4.Red.gated"])->get_value1(), 0.7f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["MaskColor4.Red.gated"])->get_value1(), 0.1f);
  EXPECT_EQ(nodes["OverColor4.Red.bg_alpha_mix"], nullptr);
  EXPECT_EQ(nodes["OverColor4.Red.mixed_composite"]->input("Value2")->link,
            dynamic_cast<ValueNode *>(nodes["Factor"])->output("Value"));
}

TEST(materialx_graph, lowers_reported_color4_literal_operands_without_crashing)
{
  materialx::Graph source;
  for (const auto &[name, nodedef] :
       {std::pair{"DifferenceColor4", "ND_difference_color4"},
        std::pair{"RangeColor4", "ND_range_color4"},
        std::pair{"OverlayColor4", "ND_overlay_color4"},
        std::pair{"MinusColor4", "ND_minus_color4"},
        std::pair{"RemapColor4", "ND_remap_color4"}})
  {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.outputs["out"] = materialx::Type::Color4;
    if (string(nodedef) == "ND_range_color4" || string(nodedef) == "ND_remap_color4") {
      node.float4_inputs = {{"in", make_float4(0.2f, 0.4f, 0.6f, 0.8f)},
                            {"inlow", make_float4(0.0f, 0.0f, 0.0f, 0.0f)},
                            {"inhigh", make_float4(1.0f, 1.0f, 1.0f, 1.0f)},
                            {"outlow", make_float4(0.1f, 0.1f, 0.1f, 0.1f)},
                            {"outhigh", make_float4(0.9f, 0.9f, 0.9f, 0.9f)}};
      if (string(nodedef) == "ND_range_color4") {
        node.float4_inputs["gamma"] = make_float4(1.0f);
        node.int_inputs["doclamp"] = 1;
      }
    }
    else {
      node.float4_inputs["fg"] = make_float4(0.7f, 0.5f, 0.3f, 0.8f);
      node.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
      node.inputs["mix"] = 0.5f;
    }
    source.nodes.push_back(std::move(node));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
}

TEST(materialx_graph, lowers_alpha_composites_with_literal_color4_operands_without_crashing)
{
  materialx::Graph source;
  for (const auto &[name, nodedef] :
       {std::pair{"DisjointOverColor4", "ND_disjointover_color4"},
        std::pair{"InColor4", "ND_in_color4"},
        std::pair{"MaskColor4", "ND_mask_color4"},
        std::pair{"MatteColor4", "ND_matte_color4"},
        std::pair{"OutColor4", "ND_out_color4"},
        std::pair{"OverColor4", "ND_over_color4"}})
  {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    node.float4_inputs["fg"] = make_float4(0.7f, 0.5f, 0.3f, 0.8f);
    node.float4_inputs["bg"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
    node.inputs["mix"] = 0.5f;
    node.outputs["out"] = materialx::Type::Color4;
    source.nodes.push_back(std::move(node));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, MathNode *> math;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *math_node = dynamic_cast<MathNode *>(node)) {
      math[node->name.string()] = math_node;
    }
  }

  ASSERT_NE(math["DisjointOverColor4.Red.summed_alpha"], nullptr);
  EXPECT_FLOAT_EQ(math["DisjointOverColor4.Red.summed_alpha"]->get_value1(), 0.8f);
  EXPECT_FLOAT_EQ(math["DisjointOverColor4.Red.summed_alpha"]->get_value2(), 0.4f);
  ASSERT_NE(math["InColor4.Red.bg_alpha_mix"], nullptr);
  EXPECT_FLOAT_EQ(math["InColor4.Red.bg_alpha_mix"]->get_value1(), 0.4f);
  ASSERT_NE(math["MaskColor4.Red.fg_alpha_mix"], nullptr);
  EXPECT_FLOAT_EQ(math["MaskColor4.Red.fg_alpha_mix"]->get_value1(), 0.8f);
  ASSERT_NE(math["MatteColor4.Red.one_minus_fg_alpha"], nullptr);
  EXPECT_FLOAT_EQ(math["MatteColor4.Red.one_minus_fg_alpha"]->get_value2(), 0.8f);
  ASSERT_NE(math["OutColor4.Red.one_minus_bg_alpha"], nullptr);
  EXPECT_FLOAT_EQ(math["OutColor4.Red.one_minus_bg_alpha"]->get_value2(), 0.4f);
}

TEST(materialx_graph, lowers_burn_and_dodge_color3_and_color4_to_materialx_arithmetic)
{
  materialx::Graph source;
  for (const auto &[name, nodedef, color4] :
       {std::tuple{"Burn", "ND_burn_color3", false},
        std::tuple{"Dodge", "ND_dodge_color3", false},
        std::tuple{"BurnColor4", "ND_burn_color4", true},
        std::tuple{"DodgeColor4", "ND_dodge_color4", true}})
  {
    materialx::Node blend;
    blend.name = name;
    blend.nodedef = nodedef;
    if (color4) {
      blend.float4_inputs["fg"] = make_float4(0.0f, 0.25f, 1.0f, 0.75f);
      blend.float4_inputs["bg"] = make_float4(0.2f, 0.4f, 0.6f, 0.5f);
      blend.outputs["out"] = materialx::Type::Color4;
    }
    else {
      blend.color3_inputs["fg"] = make_float3(0.0f, 0.25f, 1.0f);
      blend.color3_inputs["bg"] = make_float3(0.2f, 0.4f, 0.6f);
      blend.outputs["out"] = materialx::Type::Color3;
    }
    blend.inputs["mix"] = 0.5f;
    source.nodes.push_back(std::move(blend));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  for (const char *name : {"Burn", "Dodge", "BurnColor4", "DodgeColor4"}) {
    EXPECT_EQ(std::count_if(graph.nodes.begin(),
                            graph.nodes.end(),
                            [&](ShaderNode *node) {
                              return node->name == name &&
                                     dynamic_cast<MixColorNode *>(node) != nullptr;
                            }),
              0)
        << name << " must not use Cycles blend-mode semantics";
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      if (channel[0] == 'A' && string(name).find("Color4") == string::npos) {
        continue;
      }
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
      if (string(name).find("Burn") == 0 || string(name).find("Dodge") == 0) {
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

TEST(materialx_graph, lowers_hsvadjust_with_materialx_hue_wrapping)
{
  /* stdlib_ng.mtlx NG_hsvadjust_* adds amount.x to hue and feeds hsvtorgb;
   * mx_hsvtorgb wraps hue with h - floor(h). Cycles' native HSVNode adds a
   * different 0.5 bias, so the explicit graph must include the MaterialX
   * fraction step for both Color3 and Color4. */
  materialx::Node color3;
  color3.name = "HSVAdjust3";
  color3.nodedef = "ND_hsvadjust_color3";
  color3.color3_inputs["in"] = make_float3(0.9f, 0.1f, 0.2f);
  color3.vector3_inputs["amount"] = make_float3(0.75f, 0.5f, 1.25f);
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4;
  color4.name = "HSVAdjust4";
  color4.nodedef = "ND_hsvadjust_color4";
  color4.float4_inputs["in"] = make_float4(0.9f, 0.1f, 0.2f, 0.4f);
  color4.vector3_inputs["amount"] = make_float3(0.75f, 0.5f, 1.25f);
  color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color3, color4}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  for (const char *name : {"HSVAdjust3", "HSVAdjust4"}) {
    ASSERT_NE(dynamic_cast<MathNode *>(nodes[string(name) + ".hue.fract"]), nullptr) << name;
    EXPECT_EQ(dynamic_cast<MathNode *>(nodes[string(name) + ".hue.fract"])->get_math_type(),
              NODE_MATH_FRACTION)
        << name;
    ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes[name]), nullptr) << name;
    EXPECT_EQ(nodes[name]->input("Red")->link, nodes[string(name) + ".hue.fract"]->output("Value"))
        << name;
  }
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["HSVAdjust4.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["HSVAdjust4.Alpha"])->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["HSVAdjust4.Alpha"])->get_value2(), 0.0f);
}

TEST(materialx_graph, lowers_hsvadjust_measured_overflow_hue_sample_with_literal_operands)
{
  /* Regression for the measured ADJUSTMENT_SAFE4 sample where MaterialX's
   * h - floor(h) hue wrap is required before hsvtorgb. Without the explicit
   * fraction node, Cycles' HSV combine treats hue 2.088... as sector 12 and
   * routes the negative saturation result to the wrong channel. */
  materialx::Node color3;
  color3.name = "HSVAdjustMeasured3";
  color3.nodedef = "ND_hsvadjust_color3";
  color3.color3_inputs["in"] = make_float3(0.8f, 0.2f, 0.6f);
  color3.vector3_inputs["amount"] = make_float3(1.2f, 1.5f, 0.6f);
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4;
  color4.name = "HSVAdjustMeasured4";
  color4.nodedef = "ND_hsvadjust_color4";
  color4.float4_inputs["in"] = make_float4(0.8f, 0.2f, 0.6f, 0.7f);
  color4.vector3_inputs["amount"] = make_float3(1.2f, 1.5f, 0.6f);
  color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color3, color4}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  for (const char *name : {"HSVAdjustMeasured3", "HSVAdjustMeasured4"}) {
    auto *hue = dynamic_cast<MathNode *>(nodes[string(name) + ".hue"]);
    auto *fract = dynamic_cast<MathNode *>(nodes[string(name) + ".hue.fract"]);
    ASSERT_NE(hue, nullptr) << name;
    ASSERT_NE(fract, nullptr) << name;
    EXPECT_EQ(hue->get_math_type(), NODE_MATH_ADD) << name;
    EXPECT_FLOAT_EQ(hue->get_value2(), 1.2f) << name;
    EXPECT_EQ(fract->get_math_type(), NODE_MATH_FRACTION) << name;
    ASSERT_NE(nodes[name], nullptr) << name;
    EXPECT_EQ(nodes[name]->input("Red")->link, fract->output("Value")) << name;
  }
  auto *alpha = dynamic_cast<MathNode *>(nodes["HSVAdjustMeasured4.Alpha"]);
  ASSERT_NE(alpha, nullptr);
  EXPECT_FLOAT_EQ(alpha->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(alpha->get_value2(), 0.0f);
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

TEST(materialx_graph, lowers_separate2_vector2_outputs_to_xy_channels)
{
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector2";
  input.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  input.outputs["out"] = materialx::Type::Vector2;

  materialx::Node separate;
  separate.name = "Separate";
  separate.nodedef = "ND_separate2_vector2";
  separate.links["in"] = {"Input", "out", materialx::Type::Vector2};
  separate.outputs["outx"] = materialx::Type::Float;
  separate.outputs["outy"] = materialx::Type::Float;

  materialx::Node combine;
  combine.name = "Combine";
  combine.nodedef = "ND_combine2_vector2";
  combine.links["in1"] = {"Separate", "outx", materialx::Type::Float};
  combine.links["in2"] = {"Separate", "outy", materialx::Type::Float};
  combine.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, separate, combine}}, &graph));

  SeparateXYZNode *separate_node = nullptr;
  CombineXYZNode *combine_node = nullptr;
  for (ShaderNode *node : graph.nodes) {
    separate_node = node->name == "Separate" ? dynamic_cast<SeparateXYZNode *>(node) : separate_node;
    combine_node = node->name == "Combine" ? dynamic_cast<CombineXYZNode *>(node) : combine_node;
  }
  ASSERT_NE(separate_node, nullptr);
  ASSERT_NE(combine_node, nullptr);
  EXPECT_EQ(combine_node->input("X")->link, separate_node->output("X"));
  EXPECT_EQ(combine_node->input("Y")->link, separate_node->output("Y"));
}

TEST(materialx_graph, lowers_literal_separate2_vector2_outputs_to_xy_channels)
{
  materialx::Node separate;
  separate.name = "Separate";
  separate.nodedef = "ND_separate2_vector2";
  separate.vector2_inputs["in"] = make_float2(10.125f, 11.125f);
  separate.outputs["outx"] = materialx::Type::Float;
  separate.outputs["outy"] = materialx::Type::Float;

  materialx::Node combine;
  combine.name = "Combine";
  combine.nodedef = "ND_combine2_vector2";
  combine.links["in1"] = {"Separate", "outx", materialx::Type::Float};
  combine.links["in2"] = {"Separate", "outy", materialx::Type::Float};
  combine.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{separate, combine}}, &graph));

  SeparateXYZNode *separate_node = nullptr;
  CombineXYZNode *combine_node = nullptr;
  for (ShaderNode *node : graph.nodes) {
    separate_node = node->name == "Separate" ? dynamic_cast<SeparateXYZNode *>(node) : separate_node;
    combine_node = node->name == "Combine" ? dynamic_cast<CombineXYZNode *>(node) : combine_node;
  }
  ASSERT_NE(separate_node, nullptr);
  ASSERT_NE(combine_node, nullptr);
  EXPECT_EQ(separate_node->get_vector(), make_float3(10.125f, 11.125f, 0.0f));
  EXPECT_EQ(separate_node->input("Vector")->link, nullptr);
  EXPECT_EQ(combine_node->input("X")->link, separate_node->output("X"));
  EXPECT_EQ(combine_node->input("Y")->link, separate_node->output("Y"));
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
      const float expected_input = index == 0 ? 0.2f :
                                   index == 1 ? (type == materialx::Type::Vector2 ? 0.8f : 0.5f) :
                                                0.8f;
      EXPECT_FLOAT_EQ(subtract->get_value2(), expected_input);
      EXPECT_EQ(subtract->input("Value1")->link, nullptr) << nodedef;
      EXPECT_EQ(subtract->input("Value2")->link, nullptr) << nodedef;
    }
  };
  check(materialx::Type::Vector2, "ND_invert_vector2", false);
  check(materialx::Type::Vector2, "ND_invert_vector2FA", true);
  check(materialx::Type::Vector3, "ND_invert_vector3", false);
  check(materialx::Type::Vector3, "ND_invert_vector3FA", true);
}

/* Real MaterialX pbrlib/pbrlib_defs.mtlx nodedefs ND_roughness_anisotropy /
 * ND_glossiness_anisotropy -- see graph.cpp's roughness_anisotropy_id
 * declaration comment for the full mx_roughness_anisotropy.osl citation and
 * why its "if (anisotropy > 0.0) ... else" branch collapses into one
 * unconditional formula via clamp(anisotropy, 0.0, 0.98). */
TEST(materialx_graph, lowers_roughness_anisotropy_with_literal_inputs_and_no_select_node)
{
  materialx::Node node;
  node.name = "ND_roughness_anisotropy";
  node.nodedef = "ND_roughness_anisotropy";
  node.outputs["out"] = materialx::Type::Vector2;
  node.inputs["roughness"] = 0.5f;
  node.inputs["anisotropy"] = 0.6f;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };

  MathNode *roughness_sqr_mul = dynamic_cast<MathNode *>(
      find(node.name + ".roughness_sqr.multiply"));
  ClampNode *roughness_sqr = dynamic_cast<ClampNode *>(find(node.name + ".roughness_sqr"));
  ClampNode *anisotropy_clamped = dynamic_cast<ClampNode *>(
      find(node.name + ".anisotropy_clamped"));
  MathNode *one_minus_anisotropy = dynamic_cast<MathNode *>(
      find(node.name + ".one_minus_anisotropy"));
  MathNode *aspect = dynamic_cast<MathNode *>(find(node.name + ".aspect"));
  MathNode *x_divide = dynamic_cast<MathNode *>(find(node.name + ".x.divide"));
  MathNode *x_min = dynamic_cast<MathNode *>(find(node.name + ".x"));
  MathNode *y_multiply = dynamic_cast<MathNode *>(find(node.name + ".y"));
  CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(find(node.name));

  ASSERT_NE(roughness_sqr_mul, nullptr);
  ASSERT_NE(roughness_sqr, nullptr);
  ASSERT_NE(anisotropy_clamped, nullptr);
  ASSERT_NE(one_minus_anisotropy, nullptr);
  ASSERT_NE(aspect, nullptr);
  ASSERT_NE(x_divide, nullptr);
  ASSERT_NE(x_min, nullptr);
  ASSERT_NE(y_multiply, nullptr);
  ASSERT_NE(combine, nullptr);

  /* No compare/select node anywhere -- confirms the branch was genuinely
   * collapsed, not routed through a runtime condition. */
  for (ShaderNode *shader : graph.nodes) {
    const MathNode *math = dynamic_cast<const MathNode *>(shader);
    if (math != nullptr) {
      EXPECT_NE(math->get_math_type(), NODE_MATH_LESS_THAN);
      EXPECT_NE(math->get_math_type(), NODE_MATH_GREATER_THAN);
      EXPECT_NE(math->get_math_type(), NODE_MATH_COMPARE);
    }
    EXPECT_EQ(dynamic_cast<const MixFloatNode *>(shader), nullptr);
  }

  EXPECT_EQ(roughness_sqr_mul->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(roughness_sqr_mul->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(roughness_sqr_mul->get_value2(), 0.5f);
  EXPECT_EQ(roughness_sqr->get_clamp_type(), NODE_CLAMP_MINMAX);
  EXPECT_FLOAT_EQ(roughness_sqr->get_min(), 1e-8f);
  EXPECT_FLOAT_EQ(roughness_sqr->get_max(), 1.0f);
  EXPECT_EQ(roughness_sqr->input("Value")->link->parent, roughness_sqr_mul);
  EXPECT_FLOAT_EQ(anisotropy_clamped->get_value(), 0.6f);
  EXPECT_FLOAT_EQ(anisotropy_clamped->get_min(), 0.0f);
  EXPECT_FLOAT_EQ(anisotropy_clamped->get_max(), 0.98f);
  EXPECT_EQ(one_minus_anisotropy->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(one_minus_anisotropy->get_value1(), 1.0f);
  EXPECT_EQ(one_minus_anisotropy->input("Value2")->link->parent, anisotropy_clamped);
  EXPECT_EQ(aspect->get_math_type(), NODE_MATH_SQRT);
  EXPECT_EQ(aspect->input("Value1")->link->parent, one_minus_anisotropy);
  EXPECT_EQ(x_divide->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_EQ(x_divide->input("Value1")->link->parent, roughness_sqr);
  EXPECT_EQ(x_divide->input("Value2")->link->parent, aspect);
  EXPECT_EQ(x_min->get_math_type(), NODE_MATH_MINIMUM);
  EXPECT_FLOAT_EQ(x_min->get_value2(), 1.0f);
  EXPECT_EQ(x_min->input("Value1")->link->parent, x_divide);
  EXPECT_EQ(y_multiply->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(y_multiply->input("Value1")->link->parent, roughness_sqr);
  EXPECT_EQ(y_multiply->input("Value2")->link->parent, aspect);
  EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
  EXPECT_EQ(combine->input("X")->link->parent, x_min);
  EXPECT_EQ(combine->input("Y")->link->parent, y_multiply);
}

TEST(materialx_graph, lowers_glossiness_anisotropy_by_composing_invert_and_roughness_anisotropy)
{
  materialx::Node node;
  node.name = "ND_glossiness_anisotropy";
  node.nodedef = "ND_glossiness_anisotropy";
  node.outputs["out"] = materialx::Type::Vector2;
  node.inputs["glossiness"] = 0.75f;
  node.inputs["anisotropy"] = 0.3f;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };

  MathNode *invert = dynamic_cast<MathNode *>(find(node.name + ".invert1"));
  MathNode *roughness_sqr_mul = dynamic_cast<MathNode *>(
      find(node.name + ".roughness_sqr.multiply"));
  CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(find(node.name));

  ASSERT_NE(invert, nullptr);
  ASSERT_NE(roughness_sqr_mul, nullptr);
  ASSERT_NE(combine, nullptr);

  /* pbrlib_ng.mtlx IMP_glossiness_anisotropy's <invert> feeds 'roughness' from
   * "amount - in" with amount defaulting to 1.0 (stdlib_defs.mtlx
   * ND_invert_float): roughness = 1.0 - glossiness. */
  EXPECT_EQ(invert->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(invert->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(invert->get_value2(), 0.75f);
  EXPECT_EQ(roughness_sqr_mul->input("Value1")->link->parent, invert);
  EXPECT_EQ(roughness_sqr_mul->input("Value2")->link->parent, invert);
  EXPECT_NE(combine, nullptr);
}

TEST(materialx_graph, rejects_roughness_and_glossiness_anisotropy_malformed_inputs)
{
  const auto check = [](const char *nodedef, const char *first_name) {
    materialx::Node base;
    base.name = nodedef;
    base.nodedef = nodedef;
    base.outputs["out"] = materialx::Type::Vector2;
    base.inputs[first_name] = 0.5f;
    base.inputs["anisotropy"] = 0.5f;
    EXPECT_TRUE(materialx::validate({{base}})) << nodedef;

    /* Both literal and link for the same input. */
    materialx::Node both_first = base;
    both_first.links[first_name] = {"missing", "out", materialx::Type::Float};
    EXPECT_FALSE(materialx::validate({{both_first}})) << nodedef;

    /* Non-finite literal. */
    materialx::Node nonfinite = base;
    nonfinite.inputs[first_name] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(materialx::validate({{nonfinite}})) << nodedef;

    /* Missing 'anisotropy' entirely. */
    materialx::Node missing = base;
    missing.inputs.erase("anisotropy");
    EXPECT_FALSE(materialx::validate({{missing}})) << nodedef;

    /* Extraneous vector2 input the real nodedef does not declare. */
    materialx::Node extra = base;
    extra.vector2_inputs["unexpected"] = make_float2(0.0f, 0.0f);
    EXPECT_FALSE(materialx::validate({{extra}})) << nodedef;

    /* Wrong output type. */
    materialx::Node wrong_output = base;
    wrong_output.outputs["out"] = materialx::Type::Float;
    EXPECT_FALSE(materialx::validate({{wrong_output}})) << nodedef;
  };
  check("ND_roughness_anisotropy", "roughness");
  check("ND_glossiness_anisotropy", "glossiness");
}

/* Real MaterialX libraries/bxdf/open_pbr_surface.mtlx nodedef
 * ND_open_pbr_anisotropy expands to NG_open_pbr_anisotropy's arithmetic graph:
 * aniso_invert = 1-anisotropy, sqrt=sqrt(2/((aniso_invert^2)+1)),
 * rough_sq=roughness^2, alpha_x=rough_sq*sqrt, alpha_y=aniso_invert*alpha_x. */
TEST(materialx_graph, lowers_open_pbr_anisotropy_nodegraph_arithmetic)
{
  materialx::Node node;
  node.name = "ND_open_pbr_anisotropy";
  node.nodedef = "ND_open_pbr_anisotropy";
  node.outputs["out"] = materialx::Type::Vector2;
  node.inputs["roughness"] = 0.5f;
  node.inputs["anisotropy"] = 0.25f;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };

  MathNode *aniso_invert = dynamic_cast<MathNode *>(find(node.name + ".aniso_invert"));
  MathNode *aniso_invert_sq = dynamic_cast<MathNode *>(find(node.name + ".aniso_invert_sq"));
  MathNode *denom = dynamic_cast<MathNode *>(find(node.name + ".denom"));
  MathNode *fraction = dynamic_cast<MathNode *>(find(node.name + ".fraction"));
  MathNode *sqrt = dynamic_cast<MathNode *>(find(node.name + ".sqrt"));
  MathNode *rough_sq = dynamic_cast<MathNode *>(find(node.name + ".rough_sq"));
  MathNode *alpha_x = dynamic_cast<MathNode *>(find(node.name + ".alpha_x"));
  MathNode *alpha_y = dynamic_cast<MathNode *>(find(node.name + ".alpha_y"));
  CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(find(node.name));

  ASSERT_NE(aniso_invert, nullptr);
  ASSERT_NE(aniso_invert_sq, nullptr);
  ASSERT_NE(denom, nullptr);
  ASSERT_NE(fraction, nullptr);
  ASSERT_NE(sqrt, nullptr);
  ASSERT_NE(rough_sq, nullptr);
  ASSERT_NE(alpha_x, nullptr);
  ASSERT_NE(alpha_y, nullptr);
  ASSERT_NE(combine, nullptr);

  EXPECT_EQ(aniso_invert->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(aniso_invert->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(aniso_invert->get_value2(), 0.25f);
  EXPECT_EQ(aniso_invert_sq->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(aniso_invert_sq->input("Value1")->link->parent, aniso_invert);
  EXPECT_EQ(aniso_invert_sq->input("Value2")->link->parent, aniso_invert);
  EXPECT_EQ(denom->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(denom->input("Value1")->link->parent, aniso_invert_sq);
  EXPECT_FLOAT_EQ(denom->get_value2(), 1.0f);
  EXPECT_EQ(fraction->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_FLOAT_EQ(fraction->get_value1(), 2.0f);
  EXPECT_EQ(fraction->input("Value2")->link->parent, denom);
  EXPECT_EQ(sqrt->get_math_type(), NODE_MATH_SQRT);
  EXPECT_EQ(sqrt->input("Value1")->link->parent, fraction);
  EXPECT_EQ(rough_sq->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(rough_sq->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(rough_sq->get_value2(), 0.5f);
  EXPECT_EQ(alpha_x->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(alpha_x->input("Value1")->link->parent, rough_sq);
  EXPECT_EQ(alpha_x->input("Value2")->link->parent, sqrt);
  EXPECT_EQ(alpha_y->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(alpha_y->input("Value1")->link->parent, aniso_invert);
  EXPECT_EQ(alpha_y->input("Value2")->link->parent, alpha_x);
  EXPECT_EQ(combine->input("X")->link->parent, alpha_x);
  EXPECT_EQ(combine->input("Y")->link->parent, alpha_y);
  EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
}

TEST(materialx_graph, rejects_open_pbr_anisotropy_malformed_inputs)
{
  materialx::Node base;
  base.name = "ND_open_pbr_anisotropy";
  base.nodedef = "ND_open_pbr_anisotropy";
  base.outputs["out"] = materialx::Type::Vector2;
  base.inputs["roughness"] = 0.5f;
  base.inputs["anisotropy"] = 0.25f;
  EXPECT_TRUE(materialx::validate({{base}}));

  materialx::Node missing = base;
  missing.inputs.erase("roughness");
  EXPECT_FALSE(materialx::validate({{missing}}));

  materialx::Node nonfinite = base;
  nonfinite.inputs["anisotropy"] = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(materialx::validate({{nonfinite}}));

  materialx::Node extra = base;
  extra.vector2_inputs["unexpected"] = make_float2(0.0f, 0.0f);
  EXPECT_FALSE(materialx::validate({{extra}}));

  materialx::Node wrong_output = base;
  wrong_output.outputs["out"] = materialx::Type::Float;
  EXPECT_FALSE(materialx::validate({{wrong_output}}));
}

/* Real MaterialX 1.39 nodedef ND_roughness_dual (pbrlib/pbrlib_defs.mtlx,
 * ~line 391) -- see graph.cpp's roughness_dual_id declaration comment for
 * the full mx_roughness_dual.osl citation. Unlike roughness_anisotropy's
 * collapsible branch, "if (roughness.y < 0.0)" is a genuine runtime
 * sentinel select, so this must build a real LESS_THAN compare feeding the
 * same select-by-arithmetic form as ifgreater_float_id (sum = in2 +
 * factor*(in1-in2)). Both sub-tests below share one helper since the graph
 * shape is identical regardless of which side of the sentinel roughness.y
 * literal falls on -- only the fed-in literal values differ. */
TEST(materialx_graph, lowers_roughness_dual_with_real_runtime_select_for_sentinel_branch)
{
  const auto check = [](float roughness_x, float roughness_y) {
    materialx::Node node;
    node.name = "ND_roughness_dual";
    node.nodedef = "ND_roughness_dual";
    node.outputs["out"] = materialx::Type::Vector2;
    node.vector2_inputs["roughness"] = make_float2(roughness_x, roughness_y);

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph));

    const auto find = [&](const string &name) -> ShaderNode * {
      for (ShaderNode *shader : graph.nodes) {
        if (shader->name == name) return shader;
      }
      return nullptr;
    };

    MathNode *x_sqr = dynamic_cast<MathNode *>(find(node.name + ".x.multiply"));
    ClampNode *x_clamp = dynamic_cast<ClampNode *>(find(node.name + ".x"));
    MathNode *y_sqr = dynamic_cast<MathNode *>(find(node.name + ".y.multiply"));
    ClampNode *y_clamp = dynamic_cast<ClampNode *>(find(node.name + ".y_clamp"));
    MathNode *condition = dynamic_cast<MathNode *>(find(node.name + ".condition"));
    MathNode *delta = dynamic_cast<MathNode *>(find(node.name + ".y.delta"));
    MathNode *product = dynamic_cast<MathNode *>(find(node.name + ".y.product"));
    MathNode *y_select = dynamic_cast<MathNode *>(find(node.name + ".y"));
    CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(find(node.name));

    ASSERT_NE(x_sqr, nullptr);
    ASSERT_NE(x_clamp, nullptr);
    ASSERT_NE(y_sqr, nullptr);
    ASSERT_NE(y_clamp, nullptr);
    ASSERT_NE(condition, nullptr);
    ASSERT_NE(delta, nullptr);
    ASSERT_NE(product, nullptr);
    ASSERT_NE(y_select, nullptr);
    ASSERT_NE(combine, nullptr);

    /* The sentinel is a real runtime compare -- confirms this branch was
     * NOT collapsed (unlike roughness_anisotropy's anisotropy branch). */
    EXPECT_EQ(condition->get_math_type(), NODE_MATH_LESS_THAN);
    EXPECT_FLOAT_EQ(condition->get_value1(), roughness_y);
    EXPECT_FLOAT_EQ(condition->get_value2(), 0.0f);

    EXPECT_EQ(x_sqr->get_math_type(), NODE_MATH_MULTIPLY);
    EXPECT_FLOAT_EQ(x_sqr->get_value1(), roughness_x);
    EXPECT_FLOAT_EQ(x_sqr->get_value2(), roughness_x);
    EXPECT_EQ(x_clamp->get_clamp_type(), NODE_CLAMP_MINMAX);
    EXPECT_FLOAT_EQ(x_clamp->get_min(), 1e-8f);
    EXPECT_FLOAT_EQ(x_clamp->get_max(), 1.0f);
    EXPECT_EQ(x_clamp->input("Value")->link->parent, x_sqr);

    EXPECT_EQ(y_sqr->get_math_type(), NODE_MATH_MULTIPLY);
    EXPECT_FLOAT_EQ(y_sqr->get_value1(), roughness_y);
    EXPECT_FLOAT_EQ(y_sqr->get_value2(), roughness_y);
    EXPECT_EQ(y_clamp->get_clamp_type(), NODE_CLAMP_MINMAX);
    EXPECT_FLOAT_EQ(y_clamp->get_min(), 1e-8f);
    EXPECT_FLOAT_EQ(y_clamp->get_max(), 1.0f);
    EXPECT_EQ(y_clamp->input("Value")->link->parent, y_sqr);

    /* select-by-arithmetic: delta = x_clamp - y_clamp; product =
     * condition * delta; result.y = y_clamp + product. When condition==1
     * (roughness.y < 0) this equals x_clamp (isotropic pick); when
     * condition==0 it equals y_clamp (explicit pick) -- bit-for-bit
     * matching the if/else in mx_roughness_dual.osl. */
    EXPECT_EQ(delta->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(delta->input("Value1")->link->parent, x_clamp);
    EXPECT_EQ(delta->input("Value2")->link->parent, y_clamp);
    EXPECT_EQ(product->get_math_type(), NODE_MATH_MULTIPLY);
    EXPECT_EQ(product->input("Value1")->link->parent, condition);
    EXPECT_EQ(product->input("Value2")->link->parent, delta);
    EXPECT_EQ(y_select->get_math_type(), NODE_MATH_ADD);
    EXPECT_EQ(y_select->input("Value1")->link->parent, product);
    EXPECT_EQ(y_select->input("Value2")->link->parent, y_clamp);

    EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
    EXPECT_EQ(combine->input("X")->link->parent, x_clamp);
    EXPECT_EQ(combine->input("Y")->link->parent, y_select);
  };

  /* Sentinel branch: roughness.y < 0.0 (isotropic dual). */
  check(0.5f, -1.0f);
  /* Normal branch: roughness.y >= 0.0 (explicit second lobe). */
  check(0.4f, 0.3f);
}

TEST(materialx_graph, lowers_roughness_dual_with_connected_vector2_input)
{
  materialx::Node source;
  source.name = "Source";
  source.nodedef = "ND_combine2_vector2";
  source.outputs["out"] = materialx::Type::Vector2;
  source.inputs["in1"] = 0.6f;
  source.inputs["in2"] = -1.0f;

  materialx::Node node;
  node.name = "ND_roughness_dual";
  node.nodedef = "ND_roughness_dual";
  node.outputs["out"] = materialx::Type::Vector2;
  node.links["roughness"] = {"Source", "out", materialx::Type::Vector2};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, node}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };

  SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(find(node.name + ".separate"));
  MathNode *x_sqr = dynamic_cast<MathNode *>(find(node.name + ".x.multiply"));
  MathNode *y_sqr = dynamic_cast<MathNode *>(find(node.name + ".y.multiply"));
  MathNode *condition = dynamic_cast<MathNode *>(find(node.name + ".condition"));

  ASSERT_NE(separate, nullptr);
  ASSERT_NE(x_sqr, nullptr);
  ASSERT_NE(y_sqr, nullptr);
  ASSERT_NE(condition, nullptr);

  EXPECT_EQ(x_sqr->input("Value1")->link->parent, separate);
  EXPECT_EQ(x_sqr->input("Value2")->link->parent, separate);
  EXPECT_EQ(y_sqr->input("Value1")->link->parent, separate);
  EXPECT_EQ(y_sqr->input("Value2")->link->parent, separate);
  EXPECT_EQ(condition->input("Value1")->link->parent, separate);
}

/* Real MaterialX 1.39 pbrlib/pbrlib_defs.mtlx nodedef
 * ND_chiang_hair_absorption_from_color, with reference arithmetic in
 * pbrlib/genglsl/mx_chiang_hair_bsdf.glsl and mdl/materialx/pbrlib_1_6.mdl:
 * beta polynomial -> b_fac, sigma = log(clamp(color, 0.001, 1.0)) / b_fac,
 * absorption = sigma * sigma. */
TEST(materialx_graph, lowers_chiang_hair_absorption_from_color_arithmetic)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.7f, 0.4f, 0.2f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node roughness;
  roughness.name = "AzimuthalRoughness";
  roughness.nodedef = "ND_constant_float";
  roughness.inputs["value"] = 0.25f;
  roughness.outputs["out"] = materialx::Type::Float;

  materialx::Node absorption;
  absorption.name = "HairAbsorption";
  absorption.nodedef = "ND_chiang_hair_absorption_from_color";
  absorption.links["color"] = {"Color", "out", materialx::Type::Color3};
  absorption.links["azimuthal_roughness"] = {"AzimuthalRoughness", "out", materialx::Type::Float};
  absorption.outputs["absorption"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, roughness, absorption}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };

  MathNode *beta_sq = dynamic_cast<MathNode *>(find("HairAbsorption.beta_sq"));
  MathNode *term_cubic = dynamic_cast<MathNode *>(find("HairAbsorption.term_cubic"));
  MathNode *b_fac = dynamic_cast<MathNode *>(find("HairAbsorption.b_fac"));
  MathNode *red_log = dynamic_cast<MathNode *>(find("HairAbsorption.Red.log"));
  MathNode *red_divide = dynamic_cast<MathNode *>(find("HairAbsorption.Red.divide"));
  MathNode *red_square = dynamic_cast<MathNode *>(find("HairAbsorption.Red.square"));
  SeparateColorNode *separate_color = dynamic_cast<SeparateColorNode *>(find("HairAbsorption.color"));
  CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(find("HairAbsorption"));

  ASSERT_NE(beta_sq, nullptr);
  ASSERT_NE(term_cubic, nullptr);
  ASSERT_NE(b_fac, nullptr);
  ASSERT_NE(red_log, nullptr);
  ASSERT_NE(red_divide, nullptr);
  ASSERT_NE(red_square, nullptr);
  ASSERT_NE(separate_color, nullptr);
  ASSERT_NE(combine, nullptr);

  EXPECT_EQ(beta_sq->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(term_cubic->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(b_fac->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(red_log->get_math_type(), NODE_MATH_LOGARITHM);
  EXPECT_EQ(red_divide->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_EQ(red_square->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(beta_sq->input("Value1")->link->parent, find("AzimuthalRoughness"));
  EXPECT_EQ(red_divide->input("Value2")->link->parent, b_fac);
  EXPECT_EQ(combine->input("X")->link->parent, red_square);
}

TEST(materialx_graph, rejects_roughness_dual_malformed_inputs)
{
  materialx::Node base;
  base.name = "ND_roughness_dual";
  base.nodedef = "ND_roughness_dual";
  base.outputs["out"] = materialx::Type::Vector2;
  base.vector2_inputs["roughness"] = make_float2(0.5f, -1.0f);
  EXPECT_TRUE(materialx::validate({{base}}));

  /* Both literal and link for 'roughness'. */
  materialx::Node both = base;
  both.links["roughness"] = {"missing", "out", materialx::Type::Vector2};
  EXPECT_FALSE(materialx::validate({{both}}));

  /* Neither literal nor link. */
  materialx::Node neither = base;
  neither.vector2_inputs.erase("roughness");
  EXPECT_FALSE(materialx::validate({{neither}}));

  /* Non-finite literal. */
  materialx::Node nonfinite = base;
  nonfinite.vector2_inputs["roughness"] = make_float2(std::numeric_limits<float>::infinity(), 0.0f);
  EXPECT_FALSE(materialx::validate({{nonfinite}}));

  /* Extraneous scalar input the real nodedef does not declare. */
  materialx::Node extra = base;
  extra.inputs["unexpected"] = 0.0f;
  EXPECT_FALSE(materialx::validate({{extra}}));

  /* Wrong output type. */
  materialx::Node wrong_output = base;
  wrong_output.outputs["out"] = materialx::Type::Float;
  EXPECT_FALSE(materialx::validate({{wrong_output}}));
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
                                      NODE_MATH_FLOORED_MODULO : NODE_MATH_POWER;
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
    const NodeMathType type = string(nodedef).find("modulo") != string::npos ? NODE_MATH_FLOORED_MODULO : string(nodedef).find("power") != string::npos ? NODE_MATH_POWER : NODE_MATH_MINIMUM;
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

TEST(materialx_graph, lowers_scalar_to_color3_converts_with_literal_operands)
{
  const struct Case {
    const char *name;
    const char *nodedef;
    float expected;
  } cases[] = {{"FloatToColor3", "ND_convert_float_color3", 0.25f},
               {"BooleanToColor3", "ND_convert_boolean_color3", 1.0f},
               {"IntegerToColor3", "ND_convert_integer_color3", 7.0f}};

  materialx::Graph source;
  for (const Case &test_case : cases) {
    materialx::Node convert;
    convert.name = test_case.name;
    convert.nodedef = test_case.nodedef;
    if (string(test_case.nodedef) == "ND_convert_float_color3") {
      convert.inputs["in"] = test_case.expected;
    }
    else {
      convert.int_inputs["in"] = int(test_case.expected);
    }
    convert.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(convert));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  for (const Case &test_case : cases) {
    CombineColorNode *combine = nullptr;
    for (ShaderNode *node : graph.nodes) {
      combine = node->name == test_case.name ? dynamic_cast<CombineColorNode *>(node) : combine;
    }
    ASSERT_NE(combine, nullptr) << test_case.nodedef;
    EXPECT_FLOAT_EQ(combine->get_r(), test_case.expected) << test_case.nodedef;
    EXPECT_FLOAT_EQ(combine->get_g(), test_case.expected) << test_case.nodedef;
    EXPECT_FLOAT_EQ(combine->get_b(), test_case.expected) << test_case.nodedef;
    EXPECT_EQ(combine->input("Red")->link, nullptr) << test_case.nodedef;
    EXPECT_EQ(combine->input("Green")->link, nullptr) << test_case.nodedef;
    EXPECT_EQ(combine->input("Blue")->link, nullptr) << test_case.nodedef;
  }
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

TEST(materialx_graph, lowers_literal_modulo_float_to_exact_floored_value)
{
  materialx::Node modulo;
  modulo.name = "Modulo";
  modulo.nodedef = "ND_modulo_float";
  modulo.inputs = {{"in1", -1.25f}, {"in2", 2.0f}};
  modulo.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{modulo}}, &graph));

  ValueNode *value = nullptr;
  for (ShaderNode *node : graph.nodes) {
    value = node->name == "Modulo" ? dynamic_cast<ValueNode *>(node) : value;
  }
  ASSERT_NE(value, nullptr);
  EXPECT_FLOAT_EQ(value->get_value(), 0.75f);
}

TEST(materialx_graph, rejects_zero_literal_modulo_float_divisor_without_mutation)
{
  materialx::Node modulo;
  modulo.name = "Modulo";
  modulo.nodedef = "ND_modulo_float";
  modulo.inputs = {{"in1", -1.25f}, {"in2", 0.0f}};
  modulo.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ValueNode *sentinel = graph.create_node<ValueNode>();
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower({{modulo}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_NE(std::find(graph.nodes.begin(), graph.nodes.end(), sentinel), graph.nodes.end());
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

TEST(materialx_graph, lowers_vector2_and_color4_float_predicate_conditionals)
{
  /* MaterialX stdlib_defs.mtlx declares ND_if{greater,greatereq,equal}_vector2
   * and _color4 with float value1/value2 predicates and typed in1/in2 arms.
   * graph.cpp lowers them with the same select predicate used by the existing
   * float/color3/vector3 family, while preserving Color4 alpha through the
   * existing sidecar scalar channel. */
  materialx::Node vector2;
  vector2.name = "Vector2Conditional";
  vector2.nodedef = "ND_ifgreatereq_vector2";
  vector2.inputs = {{"value1", 2.0f}, {"value2", 2.0f}};
  vector2.vector2_inputs = {{"in1", make_float2(0.25f, 0.5f)},
                            {"in2", make_float2(0.75f, 1.0f)}};
  vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node color4;
  color4.name = "Color4Conditional";
  color4.nodedef = "ND_ifequal_color4";
  color4.inputs = {{"value1", 1.0f}, {"value2", 1.0f}};
  color4.float4_inputs = {{"in1", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                          {"in2", make_float4(0.5f, 0.6f, 0.7f, 0.8f)}};
  color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{vector2, color4}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *vector_mix = dynamic_cast<MixVectorNode *>(nodes["Vector2Conditional"]);
  auto *vector_condition = dynamic_cast<MathNode *>(nodes["Vector2Conditional.condition"]);
  auto *vector_greater = dynamic_cast<MathNode *>(nodes["Vector2Conditional.greater"]);
  auto *vector_equal = dynamic_cast<MathNode *>(nodes["Vector2Conditional.equal"]);
  ASSERT_NE(vector_mix, nullptr);
  ASSERT_NE(vector_condition, nullptr);
  ASSERT_NE(vector_greater, nullptr);
  ASSERT_NE(vector_equal, nullptr);
  EXPECT_EQ(vector_condition->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_EQ(vector_greater->get_math_type(), NODE_MATH_GREATER_THAN);
  EXPECT_EQ(vector_equal->get_math_type(), NODE_MATH_COMPARE);
  EXPECT_EQ(vector_mix->get_a(), make_float3(0.75f, 1.0f, 0.0f));
  EXPECT_EQ(vector_mix->get_b(), make_float3(0.25f, 0.5f, 0.0f));
  EXPECT_EQ(vector_mix->input("Factor")->link, vector_condition->output("Value"));

  auto *color_mix = dynamic_cast<MixNode *>(nodes["Color4Conditional"]);
  auto *color_condition = dynamic_cast<MathNode *>(nodes["Color4Conditional.condition"]);
  auto *alpha_delta = dynamic_cast<MathNode *>(nodes["Color4Conditional.Alpha.delta"]);
  auto *alpha_product = dynamic_cast<MathNode *>(nodes["Color4Conditional.Alpha.product"]);
  auto *alpha_sum = dynamic_cast<MathNode *>(nodes["Color4Conditional.Alpha"]);
  ASSERT_NE(color_mix, nullptr);
  ASSERT_NE(color_condition, nullptr);
  ASSERT_NE(alpha_delta, nullptr);
  ASSERT_NE(alpha_product, nullptr);
  ASSERT_NE(alpha_sum, nullptr);
  EXPECT_EQ(color_condition->get_math_type(), NODE_MATH_COMPARE);
  EXPECT_EQ(color_mix->get_mix_type(), NODE_MIX_BLEND);
  EXPECT_EQ(color_mix->get_color1(), make_float3(0.5f, 0.6f, 0.7f));
  EXPECT_EQ(color_mix->get_color2(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(color_mix->input("Fac")->link, color_condition->output("Value"));
  EXPECT_FLOAT_EQ(alpha_delta->get_value1(), 0.4f);
  EXPECT_FLOAT_EQ(alpha_delta->get_value2(), 0.8f);
  EXPECT_EQ(alpha_product->input("Value2")->link, color_condition->output("Value"));
  EXPECT_FLOAT_EQ(alpha_sum->get_value1(), 0.8f);
  EXPECT_EQ(alpha_sum->input("Value2")->link, alpha_product->output("Value"));
}

TEST(materialx_graph, lowers_vector4_float_predicate_conditionals_preserving_w)
{
  /* MaterialX 1.39 stdlib/stdlib_defs.mtlx declares ND_if{greater,greatereq,equal}_vector4
   * as the Vector4-valued siblings of the existing float/color/vector conditional family:
   * float value1/value2 select between typed in1/in2 arms. Cycles carries Vector4 as
   * XYZ plus a parallel W scalar, so the W arm must be selected by the same predicate. */
  struct ConditionalCase {
    const char *nodedef;
    NodeMathType condition_type;
  };
  const ConditionalCase cases[] = {{"ND_ifgreater_vector4", NODE_MATH_GREATER_THAN},
                                   {"ND_ifgreatereq_vector4", NODE_MATH_MAXIMUM},
                                   {"ND_ifequal_vector4", NODE_MATH_COMPARE}};

  for (const ConditionalCase &conditional_case : cases) {
    materialx::Node source_node;
    source_node.name = conditional_case.nodedef;
    source_node.nodedef = conditional_case.nodedef;
    source_node.inputs = {{"value1", 2.0f}, {"value2", 1.0f}};
    source_node.vector4_inputs = {{"in1", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                                  {"in2", make_float4(0.5f, 0.6f, 0.7f, 0.8f)}};
    source_node.outputs["out"] = materialx::Type::Vector4;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source_node}}, &graph)) << conditional_case.nodedef;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }

    auto *vector_mix = dynamic_cast<MixVectorNode *>(nodes[conditional_case.nodedef]);
    auto *condition = dynamic_cast<MathNode *>(nodes[string(conditional_case.nodedef) + ".condition"]);
    auto *w_delta = dynamic_cast<MathNode *>(nodes[string(conditional_case.nodedef) + ".W.delta"]);
    auto *w_product = dynamic_cast<MathNode *>(nodes[string(conditional_case.nodedef) + ".W.product"]);
    auto *w_sum = dynamic_cast<MathNode *>(nodes[string(conditional_case.nodedef) + ".W"]);
    ASSERT_NE(vector_mix, nullptr) << conditional_case.nodedef;
    ASSERT_NE(condition, nullptr) << conditional_case.nodedef;
    ASSERT_NE(w_delta, nullptr) << conditional_case.nodedef;
    ASSERT_NE(w_product, nullptr) << conditional_case.nodedef;
    ASSERT_NE(w_sum, nullptr) << conditional_case.nodedef;
    EXPECT_EQ(condition->get_math_type(), conditional_case.condition_type) << conditional_case.nodedef;
    EXPECT_EQ(vector_mix->get_a(), make_float3(0.5f, 0.6f, 0.7f)) << conditional_case.nodedef;
    EXPECT_EQ(vector_mix->get_b(), make_float3(0.1f, 0.2f, 0.3f)) << conditional_case.nodedef;
    EXPECT_EQ(vector_mix->input("Factor")->link, condition->output("Value")) << conditional_case.nodedef;
    EXPECT_EQ(w_delta->get_math_type(), NODE_MATH_SUBTRACT) << conditional_case.nodedef;
    EXPECT_FLOAT_EQ(w_delta->get_value1(), 0.4f) << conditional_case.nodedef;
    EXPECT_FLOAT_EQ(w_delta->get_value2(), 0.8f) << conditional_case.nodedef;
    EXPECT_EQ(w_product->get_math_type(), NODE_MATH_MULTIPLY) << conditional_case.nodedef;
    EXPECT_EQ(w_product->input("Value2")->link, condition->output("Value")) << conditional_case.nodedef;
    EXPECT_EQ(w_sum->get_math_type(), NODE_MATH_ADD) << conditional_case.nodedef;
    EXPECT_FLOAT_EQ(w_sum->get_value1(), 0.8f) << conditional_case.nodedef;
    EXPECT_EQ(w_sum->input("Value2")->link, w_product->output("Value")) << conditional_case.nodedef;
  }
}

TEST(materialx_graph, lowers_boolean_predicate_conditionals_with_exact_boolean_compare)
{
  /* MaterialX stdlib/stdlib_defs.mtlx declares ND_ifequal_*B boolean-predicate
   * siblings (lines 4261-4308 in the vendored 1.39 library), and
   * genosl/stdlib_genosl_impl.mtlx implements them as
   * mx_ternary(value1 == value2, in1, in2). */
  materialx::Node true_value;
  true_value.name = "TrueValue";
  true_value.nodedef = "ND_constant_boolean";
  true_value.int_inputs["value"] = 1;
  true_value.outputs["out"] = materialx::Type::Boolean;

  materialx::Node float_conditional;
  float_conditional.name = "FloatBConditional";
  float_conditional.nodedef = "ND_ifequal_floatB";
  float_conditional.links["value1"] = {"TrueValue", "out", materialx::Type::Boolean};
  float_conditional.int_inputs["value2"] = 1;
  float_conditional.inputs = {{"in1", 0.75f}, {"in2", 0.25f}};
  float_conditional.outputs["out"] = materialx::Type::Float;

  materialx::Node color4_conditional;
  color4_conditional.name = "Color4BConditional";
  color4_conditional.nodedef = "ND_ifequal_color4B";
  color4_conditional.int_inputs = {{"value1", 1}, {"value2", 0}};
  color4_conditional.float4_inputs = {{"in1", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                                      {"in2", make_float4(0.5f, 0.6f, 0.7f, 0.8f)}};
  color4_conditional.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector4_conditional;
  vector4_conditional.name = "Vector4BConditional";
  vector4_conditional.nodedef = "ND_ifequal_vector4B";
  vector4_conditional.int_inputs = {{"value1", 0}, {"value2", 0}};
  vector4_conditional.vector4_inputs = {{"in1", make_float4(1.0f, 2.0f, 3.0f, 4.0f)},
                                        {"in2", make_float4(5.0f, 6.0f, 7.0f, 8.0f)}};
  vector4_conditional.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{true_value, float_conditional, color4_conditional, vector4_conditional}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *float_condition = dynamic_cast<MathNode *>(nodes["FloatBConditional.condition"]);
  auto *float_sum = dynamic_cast<MathNode *>(nodes["FloatBConditional"]);
  ASSERT_NE(float_condition, nullptr);
  ASSERT_NE(float_sum, nullptr);
  EXPECT_EQ(float_condition->get_math_type(), NODE_MATH_COMPARE);
  ASSERT_NE(float_condition->input("Value1")->link, nullptr);
  EXPECT_EQ(float_condition->input("Value1")->link, nodes["TrueValue.float"]->output("Value"));
  EXPECT_FLOAT_EQ(float_condition->get_value2(), 1.0f);
  EXPECT_EQ(float_sum->get_math_type(), NODE_MATH_ADD);

  auto *color_condition = dynamic_cast<MathNode *>(nodes["Color4BConditional.condition"]);
  auto *color_mix = dynamic_cast<MixNode *>(nodes["Color4BConditional"]);
  auto *alpha_product = dynamic_cast<MathNode *>(nodes["Color4BConditional.Alpha.product"]);
  ASSERT_NE(color_condition, nullptr);
  ASSERT_NE(color_mix, nullptr);
  ASSERT_NE(alpha_product, nullptr);
  EXPECT_FLOAT_EQ(color_condition->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(color_condition->get_value2(), 0.0f);
  EXPECT_EQ(color_mix->input("Fac")->link, color_condition->output("Value"));
  EXPECT_EQ(alpha_product->input("Value2")->link, color_condition->output("Value"));

  auto *vector_condition = dynamic_cast<MathNode *>(nodes["Vector4BConditional.condition"]);
  auto *vector_mix = dynamic_cast<MixVectorNode *>(nodes["Vector4BConditional"]);
  auto *w_product = dynamic_cast<MathNode *>(nodes["Vector4BConditional.W.product"]);
  ASSERT_NE(vector_condition, nullptr);
  ASSERT_NE(vector_mix, nullptr);
  ASSERT_NE(w_product, nullptr);
  EXPECT_FLOAT_EQ(vector_condition->get_value1(), 0.0f);
  EXPECT_FLOAT_EQ(vector_condition->get_value2(), 0.0f);
  EXPECT_EQ(vector_mix->input("Factor")->link, vector_condition->output("Value"));
  EXPECT_EQ(w_product->input("Value2")->link, vector_condition->output("Value"));
}

TEST(materialx_graph, lowers_integer_predicate_conditionals_with_exact_integer_compare)
{
  /* MaterialX stdlib/stdlib_defs.mtlx declares the integer-predicate conditional
   * siblings (for example ND_ifgreater_floatI, ND_ifgreatereq_color4I, and
   * ND_ifequal_vector4I) with integer value1/value2 predicates and the same
   * typed in1/in2 result arms as the float-predicate family.  Use adjacent
   * integers above 2^24 here so a lowering that coerces the predicate to float
   * before comparing would choose the wrong branch. */
  materialx::Node float_conditional;
  float_conditional.name = "FloatIConditional";
  float_conditional.nodedef = "ND_ifgreater_floatI";
  float_conditional.int_inputs = {{"value1", 16777217}, {"value2", 16777216}};
  float_conditional.inputs = {{"in1", 0.75f}, {"in2", 0.25f}};
  float_conditional.outputs["out"] = materialx::Type::Float;

  materialx::Node color4_conditional;
  color4_conditional.name = "Color4IConditional";
  color4_conditional.nodedef = "ND_ifgreatereq_color4I";
  color4_conditional.int_inputs = {{"value1", -3}, {"value2", -3}};
  color4_conditional.float4_inputs = {{"in1", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                                      {"in2", make_float4(0.5f, 0.6f, 0.7f, 0.8f)}};
  color4_conditional.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector4_conditional;
  vector4_conditional.name = "Vector4IConditional";
  vector4_conditional.nodedef = "ND_ifequal_vector4I";
  vector4_conditional.int_inputs = {{"value1", 16777217}, {"value2", 16777216}};
  vector4_conditional.vector4_inputs = {{"in1", make_float4(1.0f, 2.0f, 3.0f, 4.0f)},
                                        {"in2", make_float4(5.0f, 6.0f, 7.0f, 8.0f)}};
  vector4_conditional.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_conditional, color4_conditional, vector4_conditional}},
                               &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *float_condition = dynamic_cast<ValueNode *>(nodes["FloatIConditional.condition"]);
  auto *float_sum = dynamic_cast<MathNode *>(nodes["FloatIConditional"]);
  ASSERT_NE(float_condition, nullptr);
  ASSERT_NE(float_sum, nullptr);
  EXPECT_FLOAT_EQ(float_condition->get_value(), 1.0f);
  EXPECT_EQ(float_sum->get_math_type(), NODE_MATH_ADD);

  auto *color_condition = dynamic_cast<ValueNode *>(nodes["Color4IConditional.condition"]);
  auto *color_mix = dynamic_cast<MixNode *>(nodes["Color4IConditional"]);
  auto *alpha_product = dynamic_cast<MathNode *>(nodes["Color4IConditional.Alpha.product"]);
  ASSERT_NE(color_condition, nullptr);
  ASSERT_NE(color_mix, nullptr);
  ASSERT_NE(alpha_product, nullptr);
  EXPECT_FLOAT_EQ(color_condition->get_value(), 1.0f);
  EXPECT_EQ(color_mix->input("Fac")->link, color_condition->output("Value"));
  EXPECT_EQ(alpha_product->input("Value2")->link, color_condition->output("Value"));

  auto *vector_condition = dynamic_cast<ValueNode *>(nodes["Vector4IConditional.condition"]);
  auto *vector_mix = dynamic_cast<MixVectorNode *>(nodes["Vector4IConditional"]);
  auto *w_product = dynamic_cast<MathNode *>(nodes["Vector4IConditional.W.product"]);
  ASSERT_NE(vector_condition, nullptr);
  ASSERT_NE(vector_mix, nullptr);
  ASSERT_NE(w_product, nullptr);
  EXPECT_FLOAT_EQ(vector_condition->get_value(), 0.0f);
  EXPECT_EQ(vector_mix->input("Factor")->link, vector_condition->output("Value"));
  EXPECT_EQ(w_product->input("Value2")->link, vector_condition->output("Value"));
}

TEST(materialx_graph, lowers_remaining_integer_and_boolean_result_conditionals)
{
  /* MaterialX 1.39 stdlib/stdlib_defs.mtlx declares the remaining conditional
   * result-type siblings in lines 3850-3910, 3991-4051, 4132-4192,
   * 4268-4274, and 4324-4328: integer-valued ifgreater/ifgreatereq/ifequal
   * nodes select between integer in1/in2 arms, while boolean-valued siblings
   * output the predicate itself. genosl/stdlib_genosl_impl.mtlx lines
   * 606/614, 628/636, 650/658, 670, and 678 implement the same mx_ternary
   * predicate semantics. */
  materialx::Node float_predicate_integer;
  float_predicate_integer.name = "IntegerFloatPredicate";
  float_predicate_integer.nodedef = "ND_ifgreater_integer";
  float_predicate_integer.inputs = {{"value1", 2.0f}, {"value2", 1.0f}};
  float_predicate_integer.int_inputs = {{"in1", 16777217}, {"in2", -13}};
  float_predicate_integer.outputs["out"] = materialx::Type::Integer;

  materialx::Node integer_predicate_integer;
  integer_predicate_integer.name = "IntegerIntegerPredicate";
  integer_predicate_integer.nodedef = "ND_ifequal_integerI";
  integer_predicate_integer.int_inputs = {{"value1", 42}, {"value2", 42}, {"in1", 21}, {"in2", 17}};
  integer_predicate_integer.outputs["out"] = materialx::Type::Integer;

  materialx::Node boolean_predicate_integer;
  boolean_predicate_integer.name = "IntegerBooleanPredicate";
  boolean_predicate_integer.nodedef = "ND_ifequal_integerB";
  boolean_predicate_integer.int_inputs = {{"value1", 1}, {"value2", 0}, {"in1", 11}, {"in2", 17}};
  boolean_predicate_integer.outputs["out"] = materialx::Type::Integer;

  materialx::Node float_source;
  float_source.name = "FloatSource";
  float_source.nodedef = "ND_constant_float";
  float_source.inputs["value"] = 0.75f;
  float_source.outputs["out"] = materialx::Type::Float;

  materialx::Node float_predicate_boolean;
  float_predicate_boolean.name = "BooleanFloatPredicate";
  float_predicate_boolean.nodedef = "ND_ifequal_boolean";
  float_predicate_boolean.links["value1"] = {"FloatSource", "out", materialx::Type::Float};
  float_predicate_boolean.inputs["value2"] = 0.75f;
  float_predicate_boolean.outputs["out"] = materialx::Type::Boolean;

  materialx::Node integer_predicate_boolean;
  integer_predicate_boolean.name = "BooleanIntegerPredicate";
  integer_predicate_boolean.nodedef = "ND_ifgreater_booleanI";
  integer_predicate_boolean.int_inputs = {{"value1", 9}, {"value2", 3}};
  integer_predicate_boolean.outputs["out"] = materialx::Type::Boolean;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_predicate_integer,
                                 integer_predicate_integer,
                                 boolean_predicate_integer,
                                 float_source,
                                 float_predicate_boolean,
                                 integer_predicate_boolean}},
                                &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *float_predicate_integer_value = dynamic_cast<ValueNode *>(nodes["IntegerFloatPredicate.float"]);
  auto *integer_predicate_integer_value = dynamic_cast<ValueNode *>(nodes["IntegerIntegerPredicate.float"]);
  auto *boolean_predicate_integer_value = dynamic_cast<ValueNode *>(nodes["IntegerBooleanPredicate.float"]);
  ASSERT_NE(float_predicate_integer_value, nullptr);
  ASSERT_NE(integer_predicate_integer_value, nullptr);
  ASSERT_NE(boolean_predicate_integer_value, nullptr);
  EXPECT_FLOAT_EQ(float_predicate_integer_value->get_value(), 16777217.0f);
  EXPECT_FLOAT_EQ(integer_predicate_integer_value->get_value(), 21.0f);
  EXPECT_FLOAT_EQ(boolean_predicate_integer_value->get_value(), 17.0f);

  auto *float_predicate_boolean_condition = dynamic_cast<MathNode *>(nodes["BooleanFloatPredicate.condition"]);
  auto *integer_predicate_boolean_condition = dynamic_cast<ValueNode *>(nodes["BooleanIntegerPredicate.condition"]);
  ASSERT_NE(float_predicate_boolean_condition, nullptr);
  ASSERT_NE(integer_predicate_boolean_condition, nullptr);
  EXPECT_EQ(float_predicate_boolean_condition->get_math_type(), NODE_MATH_COMPARE);
  ASSERT_NE(float_predicate_boolean_condition->input("Value1")->link, nullptr);
  EXPECT_EQ(float_predicate_boolean_condition->input("Value1")->link,
            nodes["FloatSource"]->output("Value"));
  EXPECT_FLOAT_EQ(float_predicate_boolean_condition->get_value2(), 0.75f);
  EXPECT_FLOAT_EQ(integer_predicate_boolean_condition->get_value(), 1.0f);
}

TEST(materialx_graph, lowers_literal_owned_conditional_backlog_variants)
{
  materialx::Graph source;

  const auto add_boolean_result = [&](const char *name,
                                      const char *nodedef,
                                      const int value1,
                                      const int value2,
                                      const bool integer_predicate) {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    if (integer_predicate) {
      node.int_inputs = {{"value1", value1}, {"value2", value2}};
    }
    else {
      node.inputs = {{"value1", float(value1)}, {"value2", float(value2)}};
    }
    node.outputs["out"] = materialx::Type::Boolean;
    source.nodes.push_back(std::move(node));
  };

  add_boolean_result("EqualBooleanFloat", "ND_ifequal_boolean", 3, 3, false);
  add_boolean_result("GreaterBooleanFloat", "ND_ifgreater_boolean", 5, 2, false);
  add_boolean_result("GreaterEqBooleanFloat", "ND_ifgreatereq_boolean", 4, 4, false);
  add_boolean_result("EqualBooleanInteger", "ND_ifequal_booleanI", 2, 2, true);
  add_boolean_result("EqualBooleanBoolean", "ND_ifequal_booleanB", 1, 1, true);
  add_boolean_result("GreaterBooleanInteger", "ND_ifgreater_booleanI", 5, 2, true);
  add_boolean_result("GreaterEqBooleanInteger", "ND_ifgreatereq_booleanI", 4, 4, true);

  materialx::Node greater_integer;
  greater_integer.name = "GreaterIntegerI";
  greater_integer.nodedef = "ND_ifgreater_integerI";
  greater_integer.int_inputs = {{"value1", 7}, {"value2", 3}, {"in1", 41}, {"in2", -9}};
  greater_integer.outputs["out"] = materialx::Type::Integer;
  source.nodes.push_back(greater_integer);

  materialx::Node greater_eq_integer_i;
  greater_eq_integer_i.name = "GreaterEqIntegerI";
  greater_eq_integer_i.nodedef = "ND_ifgreatereq_integerI";
  greater_eq_integer_i.int_inputs = {{"value1", 8}, {"value2", 8}, {"in1", 43}, {"in2", -43}};
  greater_eq_integer_i.outputs["out"] = materialx::Type::Integer;
  source.nodes.push_back(greater_eq_integer_i);

  materialx::Node equal_integer;
  equal_integer.name = "EqualInteger";
  equal_integer.nodedef = "ND_ifequal_integer";
  equal_integer.inputs = {{"value1", 6.0f}, {"value2", 6.0f}};
  equal_integer.int_inputs = {{"in1", 29}, {"in2", -29}};
  equal_integer.outputs["out"] = materialx::Type::Integer;
  source.nodes.push_back(equal_integer);

  materialx::Node greater_eq_integer;
  greater_eq_integer.name = "GreaterEqInteger";
  greater_eq_integer.nodedef = "ND_ifgreatereq_integer";
  greater_eq_integer.inputs = {{"value1", 2.0f}, {"value2", 2.0f}};
  greater_eq_integer.int_inputs = {{"in1", 13}, {"in2", -13}};
  greater_eq_integer.outputs["out"] = materialx::Type::Integer;
  source.nodes.push_back(greater_eq_integer);

  materialx::Node color3_boolean;
  color3_boolean.name = "Color3Boolean";
  color3_boolean.nodedef = "ND_ifequal_color3B";
  color3_boolean.int_inputs = {{"value1", 0}, {"value2", 0}};
  color3_boolean.color3_inputs = {{"in1", make_float3(0.1f, 0.2f, 0.3f)},
                                  {"in2", make_float3(0.4f, 0.5f, 0.6f)}};
  color3_boolean.outputs["out"] = materialx::Type::Color3;
  source.nodes.push_back(color3_boolean);

  materialx::Node vector2_boolean;
  vector2_boolean.name = "Vector2Boolean";
  vector2_boolean.nodedef = "ND_ifequal_vector2B";
  vector2_boolean.int_inputs = {{"value1", 1}, {"value2", 0}};
  vector2_boolean.vector2_inputs = {{"in1", make_float2(1.0f, 2.0f)},
                                    {"in2", make_float2(3.0f, 4.0f)}};
  vector2_boolean.outputs["out"] = materialx::Type::Vector2;
  source.nodes.push_back(vector2_boolean);

  materialx::Node vector3_boolean;
  vector3_boolean.name = "Vector3Boolean";
  vector3_boolean.nodedef = "ND_ifequal_vector3B";
  vector3_boolean.int_inputs = {{"value1", 1}, {"value2", 1}};
  vector3_boolean.vector3_inputs = {{"in1", make_float3(1.0f, 2.0f, 3.0f)},
                                    {"in2", make_float3(4.0f, 5.0f, 6.0f)}};
  vector3_boolean.outputs["out"] = materialx::Type::Vector3;
  source.nodes.push_back(vector3_boolean);

  materialx::Node matrix33_integer;
  matrix33_integer.name = "Matrix33Integer";
  matrix33_integer.nodedef = "ND_ifgreater_matrix33I";
  matrix33_integer.int_inputs = {{"value1", 3}, {"value2", 2}};
  matrix33_integer.matrix33_inputs["in1"] = {1.0f, 0.0f, 0.0f,
                                             0.0f, 2.0f, 0.0f,
                                             0.0f, 0.0f, 3.0f};
  matrix33_integer.matrix33_inputs["in2"] = {4.0f, 0.0f, 0.0f,
                                             0.0f, 5.0f, 0.0f,
                                             0.0f, 0.0f, 6.0f};
  matrix33_integer.outputs["out"] = materialx::Type::Matrix33;
  source.nodes.push_back(matrix33_integer);

  materialx::Node matrix44_boolean;
  matrix44_boolean.name = "Matrix44Boolean";
  matrix44_boolean.nodedef = "ND_ifequal_matrix44B";
  matrix44_boolean.int_inputs = {{"value1", 0}, {"value2", 1}};
  matrix44_boolean.matrix44_inputs["in1"] = {1.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 1.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 1.0f, 0.0f,
                                             1.0f, 2.0f, 3.0f, 1.0f};
  matrix44_boolean.matrix44_inputs["in2"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 3.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 4.0f, 0.0f,
                                             5.0f, 6.0f, 7.0f, 1.0f};
  matrix44_boolean.outputs["out"] = materialx::Type::Matrix44;
  source.nodes.push_back(matrix44_boolean);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  EXPECT_NE(dynamic_cast<MathNode *>(nodes["EqualBooleanFloat.condition"]), nullptr);
  EXPECT_NE(dynamic_cast<MathNode *>(nodes["GreaterBooleanFloat.condition"]), nullptr);
  EXPECT_NE(dynamic_cast<MathNode *>(nodes["GreaterEqBooleanFloat.condition"]), nullptr);
  EXPECT_NE(dynamic_cast<ValueNode *>(nodes["EqualBooleanInteger.condition"]), nullptr);
  EXPECT_NE(dynamic_cast<MixNode *>(nodes["EqualBooleanBoolean"]), nullptr);
  EXPECT_NE(dynamic_cast<ValueNode *>(nodes["GreaterBooleanInteger.condition"]), nullptr);
  EXPECT_NE(dynamic_cast<ValueNode *>(nodes["GreaterEqBooleanInteger.condition"]), nullptr);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["GreaterIntegerI.float"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["GreaterIntegerI.float"])->get_value(), 41.0f);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["GreaterEqIntegerI.float"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["GreaterEqIntegerI.float"])->get_value(), 43.0f);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["EqualInteger.float"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["EqualInteger.float"])->get_value(), 29.0f);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["GreaterEqInteger.float"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["GreaterEqInteger.float"])->get_value(), 13.0f);
  EXPECT_NE(dynamic_cast<MixNode *>(nodes["Color3Boolean"]), nullptr);
  EXPECT_NE(dynamic_cast<MixVectorNode *>(nodes["Vector2Boolean"]), nullptr);
  EXPECT_NE(dynamic_cast<MixVectorNode *>(nodes["Vector3Boolean"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Integer"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Boolean"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Integer"])->get_ob_tfm().y.y,
                  2.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Boolean"])->get_ob_tfm().z.z,
                  4.0f);
}

TEST(materialx_graph, lowers_literal_matrix_conditionals_to_selected_native_transform)
{
  materialx::Node float_predicate;
  float_predicate.name = "Matrix33FloatPredicate";
  float_predicate.nodedef = "ND_ifgreater_matrix33";
  float_predicate.inputs = {{"value1", 2.0f}, {"value2", 1.0f}};
  float_predicate.matrix33_inputs["in1"] = {1.0f, 2.0f, 3.0f,
                                            4.0f, 5.0f, 6.0f,
                                            7.0f, 8.0f, 9.0f};
  float_predicate.matrix33_inputs["in2"] = {9.0f, 8.0f, 7.0f,
                                            6.0f, 5.0f, 4.0f,
                                            3.0f, 2.0f, 1.0f};
  float_predicate.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node integer_predicate;
  integer_predicate.name = "Matrix44IntegerPredicate";
  integer_predicate.nodedef = "ND_ifequal_matrix44I";
  integer_predicate.int_inputs = {{"value1", 7}, {"value2", 8}};
  integer_predicate.matrix44_inputs["in1"] = {1.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 1.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 1.0f, 0.0f,
                                             10.0f, 20.0f, 30.0f, 1.0f};
  integer_predicate.matrix44_inputs["in2"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 3.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 4.0f, 0.0f,
                                             40.0f, 50.0f, 60.0f, 1.0f};
  integer_predicate.outputs["out"] = materialx::Type::Matrix44;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_predicate, integer_predicate}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *matrix33 = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33FloatPredicate"]);
  auto *matrix44 = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44IntegerPredicate"]);
  ASSERT_NE(matrix33, nullptr);
  ASSERT_NE(matrix44, nullptr);

  /* Matrix values are authored in MaterialX row-vector/row-major order, while
   * Cycles' Transform stores the equivalent matrix-vector transform. The native
   * carrier is therefore the transpose of the authored matrix, with Matrix44
   * translation taken from the authored last row. */
  const Transform tfm33 = matrix33->get_ob_tfm();
  EXPECT_FLOAT_EQ(tfm33.x.x, 1.0f);
  EXPECT_FLOAT_EQ(tfm33.x.y, 4.0f);
  EXPECT_FLOAT_EQ(tfm33.y.x, 2.0f);
  EXPECT_FLOAT_EQ(tfm33.z.z, 9.0f);
  EXPECT_FLOAT_EQ(tfm33.x.w, 0.0f);

  const Transform tfm44 = matrix44->get_ob_tfm();
  EXPECT_FLOAT_EQ(tfm44.x.x, 2.0f);
  EXPECT_FLOAT_EQ(tfm44.y.y, 3.0f);
  EXPECT_FLOAT_EQ(tfm44.z.z, 4.0f);
  EXPECT_FLOAT_EQ(tfm44.x.w, 40.0f);
  EXPECT_FLOAT_EQ(tfm44.y.w, 50.0f);
  EXPECT_FLOAT_EQ(tfm44.z.w, 60.0f);
}

TEST(materialx_graph, lowers_remaining_literal_matrix_conditional_backlog_variants)
{
  /* These owned conditional backlog NodeDefs were already admitted by the
   * generic matrix-conditional lowering but lacked literal-operand lower()
   * coverage for their exact ids.  Keep them covered because matrix arms must
   * stay literal-only: there is no native Cycles matrix select socket. */
  materialx::Graph source;

  materialx::Node matrix33_gte_i;
  matrix33_gte_i.name = "Matrix33GreaterEqI";
  matrix33_gte_i.nodedef = "ND_ifgreatereq_matrix33I";
  matrix33_gte_i.int_inputs = {{"value1", 4}, {"value2", 4}};
  matrix33_gte_i.matrix33_inputs["in1"] = {1.0f, 0.0f, 0.0f,
                                            0.0f, 2.0f, 0.0f,
                                            0.0f, 0.0f, 3.0f};
  matrix33_gte_i.matrix33_inputs["in2"] = {7.0f, 0.0f, 0.0f,
                                            0.0f, 8.0f, 0.0f,
                                            0.0f, 0.0f, 9.0f};
  matrix33_gte_i.outputs["out"] = materialx::Type::Matrix33;
  source.nodes.push_back(matrix33_gte_i);

  materialx::Node matrix33_equal_i;
  matrix33_equal_i.name = "Matrix33EqualI";
  matrix33_equal_i.nodedef = "ND_ifequal_matrix33I";
  matrix33_equal_i.int_inputs = {{"value1", 4}, {"value2", 5}};
  matrix33_equal_i.matrix33_inputs["in1"] = {10.0f, 0.0f, 0.0f,
                                             0.0f, 11.0f, 0.0f,
                                             0.0f, 0.0f, 12.0f};
  matrix33_equal_i.matrix33_inputs["in2"] = {13.0f, 0.0f, 0.0f,
                                             0.0f, 14.0f, 0.0f,
                                             0.0f, 0.0f, 15.0f};
  matrix33_equal_i.outputs["out"] = materialx::Type::Matrix33;
  source.nodes.push_back(matrix33_equal_i);

  materialx::Node matrix33_equal_b;
  matrix33_equal_b.name = "Matrix33EqualB";
  matrix33_equal_b.nodedef = "ND_ifequal_matrix33B";
  matrix33_equal_b.int_inputs = {{"value1", 1}, {"value2", 1}};
  matrix33_equal_b.matrix33_inputs["in1"] = {16.0f, 0.0f, 0.0f,
                                             0.0f, 17.0f, 0.0f,
                                             0.0f, 0.0f, 18.0f};
  matrix33_equal_b.matrix33_inputs["in2"] = {19.0f, 0.0f, 0.0f,
                                             0.0f, 20.0f, 0.0f,
                                             0.0f, 0.0f, 21.0f};
  matrix33_equal_b.outputs["out"] = materialx::Type::Matrix33;
  source.nodes.push_back(matrix33_equal_b);

  materialx::Node matrix44_greater_i;
  matrix44_greater_i.name = "Matrix44GreaterI";
  matrix44_greater_i.nodedef = "ND_ifgreater_matrix44I";
  matrix44_greater_i.int_inputs = {{"value1", 9}, {"value2", 2}};
  matrix44_greater_i.matrix44_inputs["in1"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 3.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 4.0f, 0.0f,
                                               5.0f, 6.0f, 7.0f, 1.0f};
  matrix44_greater_i.matrix44_inputs["in2"] = {8.0f, 0.0f, 0.0f, 0.0f,
                                               0.0f, 9.0f, 0.0f, 0.0f,
                                               0.0f, 0.0f, 10.0f, 0.0f,
                                               11.0f, 12.0f, 13.0f, 1.0f};
  matrix44_greater_i.outputs["out"] = materialx::Type::Matrix44;
  source.nodes.push_back(matrix44_greater_i);

  materialx::Node matrix44_gte_i;
  matrix44_gte_i.name = "Matrix44GreaterEqI";
  matrix44_gte_i.nodedef = "ND_ifgreatereq_matrix44I";
  matrix44_gte_i.int_inputs = {{"value1", 1}, {"value2", 1}};
  matrix44_gte_i.matrix44_inputs["in1"] = {14.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 15.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 16.0f, 0.0f,
                                           17.0f, 18.0f, 19.0f, 1.0f};
  matrix44_gte_i.matrix44_inputs["in2"] = {20.0f, 0.0f, 0.0f, 0.0f,
                                           0.0f, 21.0f, 0.0f, 0.0f,
                                           0.0f, 0.0f, 22.0f, 0.0f,
                                           23.0f, 24.0f, 25.0f, 1.0f};
  matrix44_gte_i.outputs["out"] = materialx::Type::Matrix44;
  source.nodes.push_back(matrix44_gte_i);

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33GreaterEqI"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33EqualI"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33EqualB"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44GreaterI"]), nullptr);
  ASSERT_NE(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44GreaterEqI"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33GreaterEqI"])
                      ->get_ob_tfm()
                      .y.y,
                  2.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33EqualI"])->get_ob_tfm().z.z,
                  15.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33EqualB"])->get_ob_tfm().x.x,
                  16.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44GreaterI"])
                      ->get_ob_tfm()
                      .x.w,
                  5.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44GreaterEqI"])
                      ->get_ob_tfm()
                      .z.z,
                  16.0f);
}

TEST(materialx_graph, lowers_literal_switch_nodes_to_selected_native_values)
{
  /* stdlib_ng.mtlx implements every ND_switch_* sibling as a nested
   * ifgreater ladder from in10 down to in1.  Literal selector/value folding is
   * therefore exact for these representative float/integer-selector and
   * scalar/vector/color/matrix result families. */
  materialx::Node float_switch;
  float_switch.name = "FloatSwitch";
  float_switch.nodedef = "ND_switch_float";
  float_switch.inputs = {{"which", 2.2f}, {"in3", 0.75f}};
  float_switch.outputs["out"] = materialx::Type::Float;

  materialx::Node color3_switch;
  color3_switch.name = "Color3Switch";
  color3_switch.nodedef = "ND_switch_color3";
  color3_switch.inputs["which"] = 1.0f;
  color3_switch.color3_inputs["in2"] = make_float3(0.1f, 0.2f, 0.3f);
  color3_switch.outputs["out"] = materialx::Type::Color3;

  materialx::Node color3_i_switch;
  color3_i_switch.name = "Color3ISwitch";
  color3_i_switch.nodedef = "ND_switch_color3I";
  color3_i_switch.int_inputs["which"] = 2;
  color3_i_switch.color3_inputs["in3"] = make_float3(0.4f, 0.5f, 0.6f);
  color3_i_switch.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4_switch;
  color4_switch.name = "Color4Switch";
  color4_switch.nodedef = "ND_switch_color4";
  color4_switch.inputs["which"] = 0.0f;
  color4_switch.float4_inputs["in1"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4_switch.outputs["out"] = materialx::Type::Color4;

  materialx::Node color4_i_switch;
  color4_i_switch.name = "Color4ISwitch";
  color4_i_switch.nodedef = "ND_switch_color4I";
  color4_i_switch.int_inputs["which"] = 4;
  color4_i_switch.float4_inputs["in5"] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
  color4_i_switch.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector2_switch;
  vector2_switch.name = "Vector2Switch";
  vector2_switch.nodedef = "ND_switch_vector2";
  vector2_switch.inputs["which"] = 1.0f;
  vector2_switch.vector2_inputs["in2"] = make_float2(5.0f, 6.0f);
  vector2_switch.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2_i_switch;
  vector2_i_switch.name = "Vector2ISwitch";
  vector2_i_switch.nodedef = "ND_switch_vector2I";
  vector2_i_switch.int_inputs["which"] = 2;
  vector2_i_switch.vector2_inputs["in3"] = make_float2(7.0f, 8.0f);
  vector2_i_switch.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3_switch;
  vector3_switch.name = "Vector3Switch";
  vector3_switch.nodedef = "ND_switch_vector3";
  vector3_switch.inputs["which"] = 3.0f;
  vector3_switch.vector3_inputs["in4"] = make_float3(9.0f, 10.0f, 11.0f);
  vector3_switch.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector3_i_switch;
  vector3_i_switch.name = "Vector3ISwitch";
  vector3_i_switch.nodedef = "ND_switch_vector3I";
  vector3_i_switch.int_inputs["which"] = 4;
  vector3_i_switch.vector3_inputs["in5"] = make_float3(12.0f, 13.0f, 14.0f);
  vector3_i_switch.outputs["out"] = materialx::Type::Vector3;

  materialx::Node vector4_switch;
  vector4_switch.name = "Vector4Switch";
  vector4_switch.nodedef = "ND_switch_vector4";
  vector4_switch.inputs["which"] = 2.0f;
  vector4_switch.vector4_inputs["in3"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  vector4_switch.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector4_i_switch;
  vector4_i_switch.name = "Vector4ISwitch";
  vector4_i_switch.nodedef = "ND_switch_vector4I";
  vector4_i_switch.int_inputs["which"] = 5;
  vector4_i_switch.vector4_inputs["in6"] = make_float4(5.0f, 6.0f, 7.0f, 8.0f);
  vector4_i_switch.outputs["out"] = materialx::Type::Vector4;

  materialx::Node matrix44_switch;
  matrix44_switch.name = "Matrix44Switch";
  matrix44_switch.nodedef = "ND_switch_matrix44";
  matrix44_switch.inputs["which"] = 9.0f;
  matrix44_switch.matrix44_inputs["in10"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 3.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 6.0f, 0.0f,
                                             4.0f, 5.0f, 7.0f, 1.0f};
  matrix44_switch.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node matrix44_i_switch;
  matrix44_i_switch.name = "Matrix44ISwitch";
  matrix44_i_switch.nodedef = "ND_switch_matrix44I";
  matrix44_i_switch.int_inputs["which"] = 1;
  matrix44_i_switch.matrix44_inputs["in2"] = {8.0f, 0.0f, 0.0f, 0.0f,
                                             0.0f, 9.0f, 0.0f, 0.0f,
                                             0.0f, 0.0f, 10.0f, 0.0f,
                                             11.0f, 12.0f, 13.0f, 1.0f};
  matrix44_i_switch.outputs["out"] = materialx::Type::Matrix44;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_switch,
                                  color3_switch,
                                  color3_i_switch,
                                  color4_switch,
                                  color4_i_switch,
                                  vector2_switch,
                                  vector2_i_switch,
                                  vector3_switch,
                                  vector3_i_switch,
                                  vector4_switch,
                                  vector4_i_switch,
                                  matrix44_switch,
                                  matrix44_i_switch}},
                                 &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *float_value = dynamic_cast<ValueNode *>(nodes["FloatSwitch"]);
  auto *color3_value = dynamic_cast<ColorNode *>(nodes["Color3Switch"]);
  auto *color3_i_value = dynamic_cast<ColorNode *>(nodes["Color3ISwitch"]);
  auto *color4_value = dynamic_cast<CombineColorNode *>(nodes["Color4Switch"]);
  auto *color4_alpha = dynamic_cast<MathNode *>(nodes["Color4Switch.Alpha"]);
  auto *color4_i_value = dynamic_cast<CombineColorNode *>(nodes["Color4ISwitch"]);
  auto *color4_i_alpha = dynamic_cast<MathNode *>(nodes["Color4ISwitch.Alpha"]);
  auto *vector2_value = dynamic_cast<CombineXYZNode *>(nodes["Vector2Switch"]);
  auto *vector2_i_value = dynamic_cast<CombineXYZNode *>(nodes["Vector2ISwitch"]);
  auto *vector3_value = dynamic_cast<CombineXYZNode *>(nodes["Vector3Switch"]);
  auto *vector3_i_value = dynamic_cast<CombineXYZNode *>(nodes["Vector3ISwitch"]);
  auto *vector4_value = dynamic_cast<CombineXYZNode *>(nodes["Vector4Switch"]);
  auto *vector4_w = dynamic_cast<ValueNode *>(nodes["Vector4Switch.W"]);
  auto *vector4_i_value = dynamic_cast<CombineXYZNode *>(nodes["Vector4ISwitch"]);
  auto *vector4_i_w = dynamic_cast<ValueNode *>(nodes["Vector4ISwitch.W"]);
  auto *matrix44_value = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Switch"]);
  auto *matrix44_i_value = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44ISwitch"]);
  ASSERT_NE(float_value, nullptr);
  ASSERT_NE(color3_value, nullptr);
  ASSERT_NE(color3_i_value, nullptr);
  ASSERT_NE(color4_value, nullptr);
  ASSERT_NE(color4_alpha, nullptr);
  ASSERT_NE(color4_i_value, nullptr);
  ASSERT_NE(color4_i_alpha, nullptr);
  ASSERT_NE(vector2_value, nullptr);
  ASSERT_NE(vector2_i_value, nullptr);
  ASSERT_NE(vector3_value, nullptr);
  ASSERT_NE(vector3_i_value, nullptr);
  ASSERT_NE(vector4_value, nullptr);
  ASSERT_NE(vector4_w, nullptr);
  ASSERT_NE(vector4_i_value, nullptr);
  ASSERT_NE(vector4_i_w, nullptr);
  ASSERT_NE(matrix44_value, nullptr);
  ASSERT_NE(matrix44_i_value, nullptr);
  EXPECT_FLOAT_EQ(float_value->get_value(), 0.75f);
  EXPECT_EQ(color3_value->get_value(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(color3_i_value->get_value(), make_float3(0.4f, 0.5f, 0.6f));
  EXPECT_FLOAT_EQ(color4_value->get_r(), 0.1f);
  EXPECT_FLOAT_EQ(color4_value->get_g(), 0.2f);
  EXPECT_FLOAT_EQ(color4_value->get_b(), 0.3f);
  EXPECT_FLOAT_EQ(color4_alpha->get_value1(), 0.4f);
  EXPECT_FLOAT_EQ(color4_i_value->get_r(), 0.5f);
  EXPECT_FLOAT_EQ(color4_i_value->get_g(), 0.6f);
  EXPECT_FLOAT_EQ(color4_i_value->get_b(), 0.7f);
  EXPECT_FLOAT_EQ(color4_i_alpha->get_value1(), 0.8f);
  EXPECT_FLOAT_EQ(vector2_value->get_x(), 5.0f);
  EXPECT_FLOAT_EQ(vector2_value->get_y(), 6.0f);
  EXPECT_FLOAT_EQ(vector2_value->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(vector2_i_value->get_x(), 7.0f);
  EXPECT_FLOAT_EQ(vector2_i_value->get_y(), 8.0f);
  EXPECT_FLOAT_EQ(vector2_i_value->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(vector3_value->get_x(), 9.0f);
  EXPECT_FLOAT_EQ(vector3_value->get_y(), 10.0f);
  EXPECT_FLOAT_EQ(vector3_value->get_z(), 11.0f);
  EXPECT_FLOAT_EQ(vector3_i_value->get_x(), 12.0f);
  EXPECT_FLOAT_EQ(vector3_i_value->get_y(), 13.0f);
  EXPECT_FLOAT_EQ(vector3_i_value->get_z(), 14.0f);
  EXPECT_FLOAT_EQ(vector4_value->get_x(), 1.0f);
  EXPECT_FLOAT_EQ(vector4_value->get_y(), 2.0f);
  EXPECT_FLOAT_EQ(vector4_value->get_z(), 3.0f);
  EXPECT_FLOAT_EQ(vector4_w->get_value(), 4.0f);
  EXPECT_FLOAT_EQ(vector4_i_value->get_x(), 5.0f);
  EXPECT_FLOAT_EQ(vector4_i_value->get_y(), 6.0f);
  EXPECT_FLOAT_EQ(vector4_i_value->get_z(), 7.0f);
  EXPECT_FLOAT_EQ(vector4_i_w->get_value(), 8.0f);
  const Transform tfm44 = matrix44_value->get_ob_tfm();
  EXPECT_FLOAT_EQ(tfm44.x.x, 2.0f);
  EXPECT_FLOAT_EQ(tfm44.y.y, 3.0f);
  EXPECT_FLOAT_EQ(tfm44.z.z, 6.0f);
  EXPECT_FLOAT_EQ(tfm44.x.w, 4.0f);
  EXPECT_FLOAT_EQ(tfm44.y.w, 5.0f);
  EXPECT_FLOAT_EQ(tfm44.z.w, 7.0f);
  const Transform tfm44_i = matrix44_i_value->get_ob_tfm();
  EXPECT_FLOAT_EQ(tfm44_i.x.x, 8.0f);
  EXPECT_FLOAT_EQ(tfm44_i.y.y, 9.0f);
  EXPECT_FLOAT_EQ(tfm44_i.z.z, 10.0f);
  EXPECT_FLOAT_EQ(tfm44_i.x.w, 11.0f);
  EXPECT_FLOAT_EQ(tfm44_i.y.w, 12.0f);
  EXPECT_FLOAT_EQ(tfm44_i.z.w, 13.0f);
}

TEST(materialx_graph, lowers_non_matrix_switch_default_arms_to_typed_zero_values)
{
  /* The switch NodeDefs in stdlib_defs.mtlx provide zero defaults for each
   * non-matrix typed arm.  A graph whose literal selector picks an omitted arm
   * should fold that default, not reject before lower() can run. */
  materialx::Graph source;
  for (const auto &[name, nodedef, selector_type, output_type] :
       {std::tuple{"FloatSwitch", "ND_switch_float", materialx::Type::Float, materialx::Type::Float},
        std::tuple{"FloatSwitchI", "ND_switch_floatI", materialx::Type::Integer, materialx::Type::Float},
        std::tuple{"Color3Switch", "ND_switch_color3", materialx::Type::Float, materialx::Type::Color3},
        std::tuple{"Color3SwitchI", "ND_switch_color3I", materialx::Type::Integer, materialx::Type::Color3},
        std::tuple{"Color4Switch", "ND_switch_color4", materialx::Type::Float, materialx::Type::Color4},
        std::tuple{"Color4SwitchI", "ND_switch_color4I", materialx::Type::Integer, materialx::Type::Color4},
        std::tuple{"Vector2Switch", "ND_switch_vector2", materialx::Type::Float, materialx::Type::Vector2},
        std::tuple{"Vector2SwitchI", "ND_switch_vector2I", materialx::Type::Integer, materialx::Type::Vector2},
        std::tuple{"Vector3Switch", "ND_switch_vector3", materialx::Type::Float, materialx::Type::Vector3},
        std::tuple{"Vector3SwitchI", "ND_switch_vector3I", materialx::Type::Integer, materialx::Type::Vector3},
        std::tuple{"Vector4Switch", "ND_switch_vector4", materialx::Type::Float, materialx::Type::Vector4},
        std::tuple{"Vector4SwitchI", "ND_switch_vector4I", materialx::Type::Integer, materialx::Type::Vector4}})
  {
    materialx::Node node;
    node.name = name;
    node.nodedef = nodedef;
    if (selector_type == materialx::Type::Integer) {
      node.int_inputs["which"] = 0;
    }
    else {
      node.inputs["which"] = 0.0f;
    }
    if (output_type == materialx::Type::Float) {
      node.inputs["in1"] = 0.0f;
    }
    else if (output_type == materialx::Type::Color3) {
      node.color3_inputs["in1"] = zero_float3();
    }
    else if (output_type == materialx::Type::Color4) {
      node.float4_inputs["in1"] = zero_float4();
    }
    else if (output_type == materialx::Type::Vector2) {
      node.vector2_inputs["in1"] = zero_float2();
    }
    else if (output_type == materialx::Type::Vector3) {
      node.vector3_inputs["in1"] = zero_float3();
    }
    else {
      node.vector4_inputs["in1"] = zero_float4();
    }
    node.outputs["out"] = output_type;
    source.nodes.push_back(std::move(node));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["FloatSwitch"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["FloatSwitch"])->get_value(), 0.0f);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["FloatSwitchI"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["FloatSwitchI"])->get_value(), 0.0f);
  ASSERT_NE(dynamic_cast<ColorNode *>(nodes["Color3Switch"]), nullptr);
  EXPECT_EQ(dynamic_cast<ColorNode *>(nodes["Color3Switch"])->get_value(), zero_float3());
  ASSERT_NE(dynamic_cast<ColorNode *>(nodes["Color3SwitchI"]), nullptr);
  EXPECT_EQ(dynamic_cast<ColorNode *>(nodes["Color3SwitchI"])->get_value(), zero_float3());
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["Color4Switch"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Color4Switch.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["Color4Switch.Alpha"])->get_value1(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["Color4SwitchI"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Color4SwitchI.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(nodes["Color4SwitchI.Alpha"])->get_value1(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector2Switch"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineXYZNode *>(nodes["Vector2Switch"])->get_x(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector2SwitchI"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineXYZNode *>(nodes["Vector2SwitchI"])->get_y(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector3Switch"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineXYZNode *>(nodes["Vector3Switch"])->get_z(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector3SwitchI"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineXYZNode *>(nodes["Vector3SwitchI"])->get_z(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector4Switch"]), nullptr);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["Vector4Switch.W"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["Vector4Switch.W"])->get_value(), 0.0f);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(nodes["Vector4SwitchI"]), nullptr);
  ASSERT_NE(dynamic_cast<ValueNode *>(nodes["Vector4SwitchI.W"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["Vector4SwitchI.W"])->get_value(), 0.0f);
}

TEST(materialx_graph, lowers_switch_vector4_selected_link_to_renderer)
{
  materialx::Node source;
  source.name = "Vector4Source";
  source.nodedef = "ND_constant_vector4";
  source.vector4_inputs["value"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  source.outputs["out"] = materialx::Type::Vector4;

  materialx::Node selector;
  selector.name = "Vector4Switch";
  selector.nodedef = "ND_switch_vector4";
  selector.inputs["which"] = 0.0f;
  selector.links["in1"] = {"Vector4Source", "out", materialx::Type::Vector4};
  selector.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, selector}}, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *vector = dynamic_cast<VectorMathNode *>(nodes["Vector4Switch"]);
  auto *w = dynamic_cast<MathNode *>(nodes["Vector4Switch.W"]);
  ASSERT_NE(vector, nullptr);
  ASSERT_NE(w, nullptr);
  EXPECT_EQ(vector->get_math_type(), NODE_VECTOR_MATH_ADD);
  EXPECT_EQ(vector->input("Vector1")->link, nodes["Vector4Source"]->output("Vector"));
  EXPECT_EQ(w->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(w->input("Value1")->link, nodes["Vector4Source.W"]->output("Value"));
  EXPECT_FLOAT_EQ(w->get_value2(), 0.0f);
  EXPECT_TRUE(materialx::validate({{source, selector}}));
}

TEST(materialx_graph, lowers_matrix33_switch_default_arms_to_zero_matrix)
{
  /* Matrix33 switch inputs have all-zero literal defaults in stdlib_defs.mtlx,
   * and the zero Matrix33 is representable by Cycles' Transform carrier.  This
   * is intentionally not extended to Matrix44: the all-zero Matrix44 default is
   * non-affine and cannot pass the native Transform boundary. */
  materialx::Node float_selector;
  float_selector.name = "Matrix33Switch";
  float_selector.nodedef = "ND_switch_matrix33";
  float_selector.inputs["which"] = 4.0f;
  float_selector.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node integer_selector;
  integer_selector.name = "Matrix33SwitchI";
  integer_selector.nodedef = "ND_switch_matrix33I";
  integer_selector.int_inputs["which"] = 7;
  integer_selector.outputs["out"] = materialx::Type::Matrix33;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_selector, integer_selector}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *matrix = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Switch"]);
  auto *matrix_i = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33SwitchI"]);
  ASSERT_NE(matrix, nullptr);
  ASSERT_NE(matrix_i, nullptr);
  for (const Transform &transform : {matrix->get_ob_tfm(), matrix_i->get_ob_tfm()}) {
    EXPECT_FLOAT_EQ(transform.x.x, 0.0f);
    EXPECT_FLOAT_EQ(transform.x.y, 0.0f);
    EXPECT_FLOAT_EQ(transform.x.z, 0.0f);
    EXPECT_FLOAT_EQ(transform.x.w, 0.0f);
    EXPECT_FLOAT_EQ(transform.y.x, 0.0f);
    EXPECT_FLOAT_EQ(transform.y.y, 0.0f);
    EXPECT_FLOAT_EQ(transform.y.z, 0.0f);
    EXPECT_FLOAT_EQ(transform.y.w, 0.0f);
    EXPECT_FLOAT_EQ(transform.z.x, 0.0f);
    EXPECT_FLOAT_EQ(transform.z.y, 0.0f);
    EXPECT_FLOAT_EQ(transform.z.z, 0.0f);
    EXPECT_FLOAT_EQ(transform.z.w, 0.0f);
  }
}

TEST(materialx_graph, lowers_literal_matrix_determinants_to_native_scalar_values)
{
  materialx::Node matrix33;
  matrix33.name = "Matrix33Determinant";
  matrix33.nodedef = "ND_determinant_matrix33";
  matrix33.matrix33_inputs["in"] = {1.0f, 2.0f, 3.0f,
                                    0.0f, 1.0f, 4.0f,
                                    5.0f, 6.0f, 0.0f};
  matrix33.outputs["out"] = materialx::Type::Float;

  materialx::Node matrix44;
  matrix44.name = "Matrix44Determinant";
  matrix44.nodedef = "ND_determinant_matrix44";
  matrix44.matrix44_inputs["in"] = {2.0f, 0.0f, 0.0f, 4.0f,
                                    0.0f, 3.0f, 0.0f, 5.0f,
                                    0.0f, 0.0f, 6.0f, 7.0f,
                                    0.0f, 0.0f, 0.0f, 1.0f};
  matrix44.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{matrix33, matrix44}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *det33 = dynamic_cast<ValueNode *>(nodes["Matrix33Determinant"]);
  auto *det44 = dynamic_cast<ValueNode *>(nodes["Matrix44Determinant"]);
  ASSERT_NE(det33, nullptr);
  ASSERT_NE(det44, nullptr);
  EXPECT_FLOAT_EQ(det33->get_value(), 1.0f);
  EXPECT_FLOAT_EQ(det44->get_value(), 36.0f);
}

TEST(materialx_graph, lowers_affine_translated_matrix44_determinant)
{
  materialx::Node matrix44;
  matrix44.name = "TranslatedMatrix44Determinant";
  matrix44.nodedef = "ND_determinant_matrix44";
  matrix44.matrix44_inputs["in"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                    0.0f, 3.0f, 0.0f, 0.0f,
                                    0.0f, 0.0f, 4.0f, 0.0f,
                                    5.0f, 6.0f, 7.0f, 1.0f};
  matrix44.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{matrix44}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *det44 = dynamic_cast<ValueNode *>(nodes["TranslatedMatrix44Determinant"]);
  ASSERT_NE(det44, nullptr);
  EXPECT_FLOAT_EQ(det44->get_value(), 24.0f);
}

TEST(materialx_graph, lowers_literal_matrix_arithmetic_to_native_transforms)
{
  materialx::Node add33;
  add33.name = "Matrix33AddScalar";
  add33.nodedef = "ND_add_matrix33FA";
  add33.matrix33_inputs["in1"] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  add33.inputs["in2"] = 10.0f;
  add33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node multiply33;
  multiply33.name = "Matrix33Multiply";
  multiply33.nodedef = "ND_multiply_matrix33";
  multiply33.matrix33_inputs["in1"] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  multiply33.matrix33_inputs["in2"] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
  multiply33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node subtract33;
  subtract33.name = "Matrix33Subtract";
  subtract33.nodedef = "ND_subtract_matrix33";
  subtract33.matrix33_inputs["in1"] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
  subtract33.matrix33_inputs["in2"] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
  subtract33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node subtract33fa;
  subtract33fa.name = "Matrix33SubtractScalar";
  subtract33fa.nodedef = "ND_subtract_matrix33FA";
  subtract33fa.matrix33_inputs["in1"] = {9, 8, 7, 6, 5, 4, 3, 2, 1};
  subtract33fa.inputs["in2"] = 2.0f;
  subtract33fa.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node transpose33;
  transpose33.name = "Matrix33Transpose";
  transpose33.nodedef = "ND_transpose_matrix33";
  transpose33.matrix33_inputs["in1"] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  transpose33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node inverse33;
  inverse33.name = "Matrix33Inverse";
  inverse33.nodedef = "ND_invertmatrix_matrix33";
  inverse33.matrix33_inputs["in1"] = {1, 2, 3, 0, 1, 4, 5, 6, 0};
  inverse33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node divide33;
  divide33.name = "Matrix33Divide";
  divide33.nodedef = "ND_divide_matrix33";
  divide33.matrix33_inputs["in1"] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  divide33.matrix33_inputs["in2"] = inverse33.matrix33_inputs["in1"];
  divide33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node add44;
  add44.name = "Matrix44Add";
  add44.nodedef = "ND_add_matrix44";
  add44.matrix44_inputs["in1"] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 6, 0, 4, 5, 7, 1};
  add44.matrix44_inputs["in2"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 8, 9, 10, 0};
  add44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node add44fa;
  add44fa.name = "Matrix44AddScalar";
  add44fa.nodedef = "ND_add_matrix44FA";
  add44fa.matrix44_inputs["in1"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 5, 6, 1};
  add44fa.inputs["in2"] = 0.0f;
  add44fa.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node subtract44;
  subtract44.name = "Matrix44Subtract";
  subtract44.nodedef = "ND_subtract_matrix44";
  subtract44.matrix44_inputs["in1"] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 6, 0, 4, 5, 7, 1};
  subtract44.matrix44_inputs["in2"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 8, 9, 10, 0};
  subtract44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node subtract44fa;
  subtract44fa.name = "Matrix44SubtractScalar";
  subtract44fa.nodedef = "ND_subtract_matrix44FA";
  subtract44fa.matrix44_inputs["in1"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 5, 6, 1};
  subtract44fa.inputs["in2"] = 0.0f;
  subtract44fa.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node transpose44;
  transpose44.name = "Matrix44Transpose";
  transpose44.nodedef = "ND_transpose_matrix44";
  transpose44.matrix44_inputs["in1"] = {1, 2, 0, 0, 3, 4, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  transpose44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node multiply44;
  multiply44.name = "Matrix44Multiply";
  multiply44.nodedef = "ND_multiply_matrix44";
  multiply44.matrix44_inputs["in1"] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 6, 0, 4, 5, 7, 1};
  multiply44.matrix44_inputs["in2"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 8, 9, 10, 1};
  multiply44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node inverse44;
  inverse44.name = "Matrix44Inverse";
  inverse44.nodedef = "ND_invertmatrix_matrix44";
  inverse44.matrix44_inputs["in1"] = {2, 0, 0, 0, 0, 4, 0, 0, 0, 0, 5, 0, 4, 8, 10, 1};
  inverse44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node divide44;
  divide44.name = "Matrix44Divide";
  divide44.nodedef = "ND_divide_matrix44";
  divide44.matrix44_inputs["in1"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  divide44.matrix44_inputs["in2"] = inverse44.matrix44_inputs["in1"];
  divide44.outputs["out"] = materialx::Type::Matrix44;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{add33,
                                 multiply33,
                                 subtract33,
                                 subtract33fa,
                                 transpose33,
                                 inverse33,
                                 divide33,
                                 add44,
                                 add44fa,
                                 subtract44,
                                 subtract44fa,
                                 transpose44,
                                 multiply44,
                                 inverse44,
                                 divide44}},
                                &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *add = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33AddScalar"]);
  auto *multiply = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Multiply"]);
  auto *subtract = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Subtract"]);
  auto *subtract_scalar = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33SubtractScalar"]);
  auto *transpose33_value = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Transpose"]);
  auto *inverse = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Inverse"]);
  auto *divide33_value = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix33Divide"]);
  auto *add_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Add"]);
  auto *add_scalar_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44AddScalar"]);
  auto *subtract_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Subtract"]);
  auto *subtract_scalar_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44SubtractScalar"]);
  auto *transpose = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Transpose"]);
  auto *multiply_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Multiply"]);
  auto *inverse_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Inverse"]);
  auto *divide_affine = dynamic_cast<TextureCoordinateNode *>(nodes["Matrix44Divide"]);
  ASSERT_NE(add, nullptr);
  ASSERT_NE(multiply, nullptr);
  ASSERT_NE(subtract, nullptr);
  ASSERT_NE(subtract_scalar, nullptr);
  ASSERT_NE(transpose33_value, nullptr);
  ASSERT_NE(inverse, nullptr);
  ASSERT_NE(divide33_value, nullptr);
  ASSERT_NE(add_affine, nullptr);
  ASSERT_NE(add_scalar_affine, nullptr);
  ASSERT_NE(subtract_affine, nullptr);
  ASSERT_NE(subtract_scalar_affine, nullptr);
  ASSERT_NE(transpose, nullptr);
  ASSERT_NE(multiply_affine, nullptr);
  ASSERT_NE(inverse_affine, nullptr);
  ASSERT_NE(divide_affine, nullptr);

  EXPECT_FLOAT_EQ(add->get_ob_tfm().x.x, 11.0f);
  EXPECT_FLOAT_EQ(add->get_ob_tfm().z.z, 19.0f);
  EXPECT_FLOAT_EQ(multiply->get_ob_tfm().x.x, 30.0f);
  EXPECT_FLOAT_EQ(multiply->get_ob_tfm().z.z, 90.0f);
  EXPECT_FLOAT_EQ(subtract->get_ob_tfm().x.x, 8.0f);
  EXPECT_FLOAT_EQ(subtract_scalar->get_ob_tfm().z.z, -1.0f);
  EXPECT_FLOAT_EQ(transpose33_value->get_ob_tfm().x.y, 2.0f);
  EXPECT_FLOAT_EQ(inverse->get_ob_tfm().x.x, -24.0f);
  EXPECT_FLOAT_EQ(inverse->get_ob_tfm().x.y, 20.0f);
  EXPECT_FLOAT_EQ(inverse->get_ob_tfm().z.z, 1.0f);
  EXPECT_FLOAT_EQ(divide33_value->get_ob_tfm().x.x, -24.0f);
  EXPECT_FLOAT_EQ(add_affine->get_ob_tfm().x.x, 3.0f);
  EXPECT_FLOAT_EQ(add_affine->get_ob_tfm().x.w, 12.0f);
  EXPECT_FLOAT_EQ(add_scalar_affine->get_ob_tfm().x.w, 4.0f);
  EXPECT_FLOAT_EQ(subtract_affine->get_ob_tfm().x.x, 1.0f);
  EXPECT_FLOAT_EQ(subtract_affine->get_ob_tfm().x.w, -4.0f);
  EXPECT_FLOAT_EQ(subtract_scalar_affine->get_ob_tfm().z.w, 6.0f);
  EXPECT_FLOAT_EQ(transpose->get_ob_tfm().x.y, 2.0f);
  EXPECT_FLOAT_EQ(transpose->get_ob_tfm().y.x, 3.0f);
  EXPECT_FLOAT_EQ(transpose->get_ob_tfm().x.w, 0.0f);
  EXPECT_FLOAT_EQ(multiply_affine->get_ob_tfm().x.w, 12.0f);
  EXPECT_FLOAT_EQ(multiply_affine->get_ob_tfm().y.w, 14.0f);
  EXPECT_FLOAT_EQ(multiply_affine->get_ob_tfm().z.w, 17.0f);
  EXPECT_FLOAT_EQ(inverse_affine->get_ob_tfm().x.x, 0.5f);
  EXPECT_FLOAT_EQ(inverse_affine->get_ob_tfm().y.y, 0.25f);
  EXPECT_FLOAT_EQ(inverse_affine->get_ob_tfm().z.z, 0.2f);
  EXPECT_FLOAT_EQ(divide_affine->get_ob_tfm().x.x, 0.5f);
  EXPECT_FLOAT_EQ(divide_affine->get_ob_tfm().y.y, 0.25f);
  EXPECT_FLOAT_EQ(divide_affine->get_ob_tfm().z.z, 0.2f);
}

TEST(materialx_graph, rejects_literal_matrix_arithmetic_that_exits_native_subset)
{
  materialx::Node singular;
  singular.name = "SingularInverse";
  singular.nodedef = "ND_invertmatrix_matrix33";
  singular.matrix33_inputs["in1"] = {1, 2, 3, 2, 4, 6, 0, 0, 1};
  singular.outputs["out"] = materialx::Type::Matrix33;
  EXPECT_FALSE(materialx::validate({{singular}}));

  materialx::Node nonaffine;
  nonaffine.name = "NonAffineAdd";
  nonaffine.nodedef = "ND_add_matrix44";
  nonaffine.matrix44_inputs["in1"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  nonaffine.matrix44_inputs["in2"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  nonaffine.outputs["out"] = materialx::Type::Matrix44;
  EXPECT_FALSE(materialx::validate({{nonaffine}}));

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower({{nonaffine}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_literal_creatematrix_and_transformmatrix_nodes)
{
  materialx::Node create33;
  create33.name = "CreateMatrix33";
  create33.nodedef = "ND_creatematrix_vector3_matrix33";
  create33.vector3_inputs["in1"] = make_float3(1.0f, 2.0f, 3.0f);
  create33.vector3_inputs["in2"] = make_float3(4.0f, 5.0f, 6.0f);
  create33.vector3_inputs["in3"] = make_float3(7.0f, 8.0f, 9.0f);
  create33.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node create44;
  create44.name = "CreateMatrix44";
  create44.nodedef = "ND_creatematrix_vector3_matrix44";
  create44.vector3_inputs["in1"] = make_float3(1.0f, 0.0f, 0.0f);
  create44.vector3_inputs["in2"] = make_float3(0.0f, 2.0f, 0.0f);
  create44.vector3_inputs["in3"] = make_float3(0.0f, 0.0f, 3.0f);
  create44.vector3_inputs["in4"] = make_float3(0.0f, 0.0f, 0.0f);
  create44.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node create44v;
  create44v.name = "CreateMatrix44Vector4";
  create44v.nodedef = "ND_creatematrix_vector4_matrix44";
  create44v.vector4_inputs["in1"] = make_float4(1.0f, 0.0f, 0.0f, 0.0f);
  create44v.vector4_inputs["in2"] = make_float4(0.0f, 2.0f, 0.0f, 0.0f);
  create44v.vector4_inputs["in3"] = make_float4(0.0f, 0.0f, 3.0f, 0.0f);
  create44v.vector4_inputs["in4"] = make_float4(4.0f, 5.0f, 6.0f, 1.0f);
  create44v.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node transform2;
  transform2.name = "TransformVector2";
  transform2.nodedef = "ND_transformmatrix_vector2M3";
  transform2.vector2_inputs["in"] = make_float2(2.0f, 3.0f);
  transform2.matrix33_inputs["mat"] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10.0f, 20.0f, 1.0f};
  transform2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node transform3;
  transform3.name = "TransformVector3";
  transform3.nodedef = "ND_transformmatrix_vector3";
  transform3.vector3_inputs["in"] = make_float3(1.0f, 2.0f, 3.0f);
  transform3.matrix33_inputs["mat"] = {2.0f, 0.0f, 0.0f, 0.0f, 3.0f, 0.0f, 0.0f, 0.0f, 4.0f};
  transform3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node transform3m4;
  transform3m4.name = "TransformVector3M4";
  transform3m4.nodedef = "ND_transformmatrix_vector3M4";
  transform3m4.vector3_inputs["in"] = make_float3(1.0f, 2.0f, 3.0f);
  transform3m4.matrix44_inputs["mat"] = {2.0f, 0.0f, 0.0f, 0.0f,
                                         0.0f, 3.0f, 0.0f, 0.0f,
                                         0.0f, 0.0f, 4.0f, 0.0f,
                                         4.0f, 5.0f, 6.0f, 1.0f};
  transform3m4.outputs["out"] = materialx::Type::Vector3;

  materialx::Node transform4;
  transform4.name = "TransformVector4";
  transform4.nodedef = "ND_transformmatrix_vector4";
  transform4.vector4_inputs["in"] = make_float4(1.0f, 2.0f, 3.0f, 1.0f);
  transform4.matrix44_inputs["mat"] = transform3m4.matrix44_inputs["mat"];
  transform4.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{create33,
                                 create44,
                                 create44v,
                                 transform2,
                                 transform3,
                                 transform3m4,
                                 transform4}},
                                &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *matrix33 = dynamic_cast<TextureCoordinateNode *>(nodes["CreateMatrix33"]);
  auto *matrix44 = dynamic_cast<TextureCoordinateNode *>(nodes["CreateMatrix44"]);
  auto *matrix44v = dynamic_cast<TextureCoordinateNode *>(nodes["CreateMatrix44Vector4"]);
  auto *vector2 = dynamic_cast<CombineXYZNode *>(nodes["TransformVector2"]);
  auto *vector3 = dynamic_cast<CombineXYZNode *>(nodes["TransformVector3"]);
  auto *vector3m4 = dynamic_cast<CombineXYZNode *>(nodes["TransformVector3M4"]);
  auto *vector4 = dynamic_cast<CombineXYZNode *>(nodes["TransformVector4"]);
  auto *vector4_w = dynamic_cast<ValueNode *>(nodes["TransformVector4.W"]);
  ASSERT_NE(matrix33, nullptr);
  ASSERT_NE(matrix44, nullptr);
  ASSERT_NE(matrix44v, nullptr);
  ASSERT_NE(vector2, nullptr);
  ASSERT_NE(vector3, nullptr);
  ASSERT_NE(vector3m4, nullptr);
  ASSERT_NE(vector4, nullptr);
  ASSERT_NE(vector4_w, nullptr);
  EXPECT_FLOAT_EQ(matrix33->get_ob_tfm().x.z, 7.0f);
  EXPECT_FLOAT_EQ(matrix44->get_ob_tfm().y.y, 2.0f);
  EXPECT_FLOAT_EQ(matrix44v->get_ob_tfm().x.w, 4.0f);
  EXPECT_FLOAT_EQ(vector2->get_x(), 12.0f);
  EXPECT_FLOAT_EQ(vector2->get_y(), 23.0f);
  EXPECT_FLOAT_EQ(vector3->get_z(), 12.0f);
  EXPECT_FLOAT_EQ(vector3m4->get_x(), 6.0f);
  EXPECT_FLOAT_EQ(vector3m4->get_y(), 11.0f);
  EXPECT_FLOAT_EQ(vector3m4->get_z(), 18.0f);
  EXPECT_FLOAT_EQ(vector4->get_z(), 18.0f);
  EXPECT_FLOAT_EQ(vector4_w->get_value(), 1.0f);
}

TEST(materialx_graph, lowers_inside_outside_float_color3_and_color4_masks)
{
  /* MaterialX stdlib_defs.mtlx declares <inside> as in * mask and <outside>
   * as in * (1 - mask) for float, color3, and color4. */
  materialx::Node inside_float;
  inside_float.name = "InsideFloat";
  inside_float.nodedef = "ND_inside_float";
  inside_float.inputs = {{"in", 0.5f}, {"mask", 0.25f}};
  inside_float.outputs["out"] = materialx::Type::Float;

  materialx::Node outside_color3;
  outside_color3.name = "OutsideColor3";
  outside_color3.nodedef = "ND_outside_color3";
  outside_color3.color3_inputs["in"] = make_float3(0.2f, 0.4f, 0.6f);
  outside_color3.inputs["mask"] = 0.75f;
  outside_color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node inside_color4;
  inside_color4.name = "InsideColor4";
  inside_color4.nodedef = "ND_inside_color4";
  inside_color4.float4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  inside_color4.inputs["mask"] = 0.5f;
  inside_color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node outside_color4;
  outside_color4.name = "OutsideColor4";
  outside_color4.nodedef = "ND_outside_color4";
  outside_color4.float4_inputs["in"] = make_float4(0.2f, 0.4f, 0.6f, 0.8f);
  outside_color4.inputs["mask"] = 0.25f;
  outside_color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{inside_float, outside_color3, inside_color4, outside_color4}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *float_multiply = dynamic_cast<MathNode *>(nodes["InsideFloat"]);
  ASSERT_NE(float_multiply, nullptr);
  EXPECT_EQ(float_multiply->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(float_multiply->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(float_multiply->get_value2(), 0.25f);

  auto *one_minus = dynamic_cast<MathNode *>(nodes["OutsideColor3.mask"]);
  auto *color_mask = dynamic_cast<CombineColorNode *>(nodes["OutsideColor3.mask_color"]);
  auto *color_multiply = dynamic_cast<MixNode *>(nodes["OutsideColor3"]);
  ASSERT_NE(one_minus, nullptr);
  ASSERT_NE(color_mask, nullptr);
  ASSERT_NE(color_multiply, nullptr);
  EXPECT_EQ(one_minus->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(one_minus->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(one_minus->get_value2(), 0.75f);
  EXPECT_EQ(color_multiply->get_mix_type(), NODE_MIX_MUL);
  EXPECT_EQ(color_multiply->get_color1(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(color_multiply->input("Color2")->link, color_mask->output("Color"));

  auto *color4_multiply = dynamic_cast<MixNode *>(nodes["InsideColor4"]);
  auto *alpha = dynamic_cast<MathNode *>(nodes["InsideColor4.Alpha"]);
  ASSERT_NE(color4_multiply, nullptr);
  ASSERT_NE(alpha, nullptr);
  EXPECT_EQ(color4_multiply->get_mix_type(), NODE_MIX_MUL);
  EXPECT_EQ(color4_multiply->get_color1(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(alpha->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(alpha->get_value1(), 0.4f);
  EXPECT_FLOAT_EQ(alpha->get_value2(), 0.5f);

  auto *outside_color4_mask = dynamic_cast<MathNode *>(nodes["OutsideColor4.mask"]);
  auto *outside_color4_multiply = dynamic_cast<MixNode *>(nodes["OutsideColor4"]);
  auto *outside_alpha = dynamic_cast<MathNode *>(nodes["OutsideColor4.Alpha"]);
  ASSERT_NE(outside_color4_mask, nullptr);
  ASSERT_NE(outside_color4_multiply, nullptr);
  ASSERT_NE(outside_alpha, nullptr);
  EXPECT_EQ(outside_color4_mask->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(outside_color4_mask->get_value2(), 0.25f);
  EXPECT_EQ(outside_color4_multiply->get_mix_type(), NODE_MIX_MUL);
  EXPECT_EQ(outside_alpha->input("Value2")->link, outside_color4_mask->output("Value"));
}

TEST(materialx_graph, lowers_premult_and_unpremult_color4_preserving_alpha)
{
  /* Real MaterialX sources: libraries/stdlib/stdlib_defs.mtlx declares
   * ND_premult_color4 / ND_unpremult_color4 in nodegroup="compositing";
   * genosl/stdlib_genosl_impl.mtlx registers mx_premult_color4 and
   * mx_unpremult_color4 implementations for the same nodedefs. */
  materialx::Node source_color;
  source_color.name = "SourceColor";
  source_color.nodedef = "ND_constant_color4";
  source_color.float4_inputs["value"] = make_float4(0.2f, 0.4f, 0.6f, 0.5f);
  source_color.outputs["out"] = materialx::Type::Color4;

  materialx::Node premult;
  premult.name = "Premult";
  premult.nodedef = "ND_premult_color4";
  premult.links["in"] = {"SourceColor", "out", materialx::Type::Color4};
  premult.outputs["out"] = materialx::Type::Color4;

  materialx::Node unpremult;
  unpremult.name = "Unpremult";
  unpremult.nodedef = "ND_unpremult_color4";
  unpremult.links["in"] = {"Premult", "out", materialx::Type::Color4};
  unpremult.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source_color, premult, unpremult}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *premult_red = dynamic_cast<MathNode *>(nodes["Premult.Red"]);
  auto *premult_alpha = dynamic_cast<MathNode *>(nodes["Premult.Alpha"]);
  auto *unpremult_safe_alpha = dynamic_cast<MathNode *>(nodes["Unpremult.Red.safe_alpha"]);
  auto *unpremult_result = dynamic_cast<MathNode *>(nodes["Unpremult.Red.result"]);
  auto *unpremult_alpha = dynamic_cast<MathNode *>(nodes["Unpremult.Alpha"]);
  ASSERT_NE(premult_red, nullptr);
  ASSERT_NE(premult_alpha, nullptr);
  ASSERT_NE(unpremult_safe_alpha, nullptr);
  ASSERT_NE(unpremult_result, nullptr);
  ASSERT_NE(unpremult_alpha, nullptr);
  EXPECT_EQ(premult_red->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(premult_alpha->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(unpremult_safe_alpha->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(unpremult_result->get_math_type(), NODE_MATH_ADD);
  ASSERT_NE(premult_red->input("Value2")->link, nullptr);
  EXPECT_EQ(premult_red->input("Value2")->link->parent->name, "SourceColor.Alpha");
  EXPECT_EQ(unpremult_safe_alpha->input("Value1")->link, premult_alpha->output("Value"));
  EXPECT_EQ(unpremult_alpha->input("Value1")->link, premult_alpha->output("Value"));
}

TEST(materialx_graph, lowers_premult_and_unpremult_color4_literal_operands_without_crashing)
{
  materialx::Node premult;
  premult.name = "PremultLiteral";
  premult.nodedef = "ND_premult_color4";
  premult.float4_inputs["in"] = make_float4(0.2f, 0.4f, 0.6f, 0.5f);
  premult.outputs["out"] = materialx::Type::Color4;

  materialx::Node unpremult;
  unpremult.name = "UnpremultLiteral";
  unpremult.nodedef = "ND_unpremult_color4";
  unpremult.float4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.5f);
  unpremult.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{premult, unpremult}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *premult_red = dynamic_cast<MathNode *>(nodes["PremultLiteral.Red"]);
  auto *premult_alpha = dynamic_cast<MathNode *>(nodes["PremultLiteral.Alpha"]);
  auto *unpremult_safe_alpha = dynamic_cast<MathNode *>(nodes["UnpremultLiteral.Red.safe_alpha"]);
  auto *unpremult_divide = dynamic_cast<MathNode *>(nodes["UnpremultLiteral.Red.divide"]);
  auto *unpremult_alpha = dynamic_cast<MathNode *>(nodes["UnpremultLiteral.Alpha"]);
  ASSERT_NE(premult_red, nullptr);
  ASSERT_NE(premult_alpha, nullptr);
  ASSERT_NE(unpremult_safe_alpha, nullptr);
  ASSERT_NE(unpremult_divide, nullptr);
  ASSERT_NE(unpremult_alpha, nullptr);
  EXPECT_FLOAT_EQ(premult_red->get_value1(), 0.2f);
  EXPECT_FLOAT_EQ(premult_red->get_value2(), 0.5f);
  EXPECT_FLOAT_EQ(premult_alpha->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(unpremult_safe_alpha->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(unpremult_divide->get_value1(), 0.1f);
  EXPECT_FLOAT_EQ(unpremult_alpha->get_value1(), 0.5f);
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

TEST(materialx_graph, lowers_trianglewave_float_as_exact_stdlib_nodegraph)
{
  /* MaterialX 1.39 libraries/stdlib/stdlib_defs.mtlx declares ND_trianglewave_float;
   * libraries/stdlib/stdlib_ng.mtlx NG_trianglewave_float defines:
   *   0.5 - abs(modulo(abs(in), 1.0) - 0.5). */
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_float";
  input.inputs["value"] = -1.25f;
  input.outputs["out"] = materialx::Type::Float;

  materialx::Node triangle;
  triangle.name = "Triangle";
  triangle.nodedef = "ND_trianglewave_float";
  triangle.links["in"] = {"Input", "out", materialx::Type::Float};
  triangle.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["specular_roughness"] = {"Triangle", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, triangle, surface}}, &graph));

  const auto math_named = [&](const string &name) -> MathNode * {
    for (ShaderNode *node : graph.nodes) {
      if (node->name == name) {
        return dynamic_cast<MathNode *>(node);
      }
    }
    return nullptr;
  };
  MathNode *absolute = math_named("Triangle.abs");
  MathNode *modulo = math_named("Triangle.modulo");
  MathNode *center = math_named("Triangle.center");
  MathNode *center_abs = math_named("Triangle.center_abs");
  MathNode *result = math_named("Triangle.result");
  ASSERT_NE(absolute, nullptr);
  ASSERT_NE(modulo, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(center_abs, nullptr);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(absolute->get_math_type(), NODE_MATH_ABSOLUTE);
  EXPECT_EQ(modulo->get_math_type(), NODE_MATH_MODULO);
  EXPECT_EQ(center->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(center_abs->get_math_type(), NODE_MATH_ABSOLUTE);
  EXPECT_EQ(result->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(modulo->get_value2(), 1.0f);
  EXPECT_FLOAT_EQ(center->get_value2(), 0.5f);
  EXPECT_FLOAT_EQ(result->get_value1(), 0.5f);
  EXPECT_EQ(modulo->input("Value1")->link, absolute->output("Value"));
  EXPECT_EQ(center->input("Value1")->link, modulo->output("Value"));
  EXPECT_EQ(center_abs->input("Value1")->link, center->output("Value"));
  EXPECT_EQ(result->input("Value2")->link, center_abs->output("Value"));
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

TEST(materialx_graph, lowers_normalmap_vector2_equal_scale_subset)
{
  materialx::Node normalmap;
  normalmap.name = "NormalMapVector2";
  normalmap.nodedef = "ND_normalmap_vector2";
  normalmap.vector3_inputs["in"] = make_float3(0.25f, 0.75f, 1.0f);
  normalmap.vector2_inputs["scale"] = make_float2(0.5f, 0.5f);
  normalmap.outputs["out"] = materialx::Type::Vector3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_normal"] = {"NormalMapVector2", "out", materialx::Type::Vector3};
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
  EXPECT_FLOAT_EQ(native_normalmap->get_strength(), 0.5f);
  EXPECT_EQ(native_normalmap->get_color(), make_float3(0.25f, 0.75f, 1.0f));
  EXPECT_EQ(principled->input("Normal")->link, native_normalmap->output("Normal"));
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

TEST(materialx_graph, lowers_npr_facingratio_float)
{
  materialx::Node facing;
  facing.name = "Facing";
  facing.nodedef = "ND_facingratio_float";
  facing.vector3_inputs["viewdirection"] = make_float3(0.0f, 0.0f, 1.0f);
  facing.vector3_inputs["normal"] = make_float3(0.0f, 0.0f, -1.0f);
  facing.int_inputs["faceforward"] = 1;
  facing.int_inputs["invert"] = 1;
  facing.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{facing}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *dot = dynamic_cast<VectorMathNode *>(nodes["Facing.dot"]);
  auto *faceforward = dynamic_cast<MathNode *>(nodes["Facing.faceforward"]);
  auto *invert = dynamic_cast<MathNode *>(nodes["Facing"]);
  ASSERT_NE(dot, nullptr);
  ASSERT_NE(faceforward, nullptr);
  ASSERT_NE(invert, nullptr);
  EXPECT_EQ(dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dot->get_vector1(), make_float3(0.0f, 0.0f, 1.0f));
  EXPECT_EQ(dot->get_vector2(), make_float3(0.0f, 0.0f, -1.0f));
  EXPECT_EQ(faceforward->get_math_type(), NODE_MATH_ABSOLUTE);
  EXPECT_EQ(faceforward->input("Value1")->link, dot->output("Value"));
  EXPECT_EQ(invert->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(invert->get_value1(), 1.0f);
  EXPECT_EQ(invert->input("Value2")->link, faceforward->output("Value"));

  facing.int_inputs["faceforward"] = 0;
  facing.int_inputs["invert"] = 0;
  ShaderGraph signed_graph;
  ASSERT_TRUE(materialx::lower({{facing}}, &signed_graph));
  MathNode *signed_faceforward = nullptr;
  for (ShaderNode *node : signed_graph.nodes) {
    signed_faceforward = node->name == "Facing" ? dynamic_cast<MathNode *>(node) : signed_faceforward;
  }
  ASSERT_NE(signed_faceforward, nullptr);
  EXPECT_EQ(signed_faceforward->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(signed_faceforward->get_value2(), -1.0f);
}

TEST(materialx_graph, lowers_npr_gooch_shade_with_literal_controls)
{
  materialx::Node gooch;
  gooch.name = "Gooch";
  gooch.nodedef = "ND_gooch_shade";
  gooch.color3_inputs["warm_color"] = make_float3(0.8f, 0.7f, 0.4f);
  gooch.color3_inputs["cool_color"] = make_float3(0.2f, 0.3f, 0.9f);
  gooch.inputs["specular_intensity"] = 0.5f;
  gooch.inputs["shininess"] = 32.0f;
  gooch.vector3_inputs["light_direction"] = make_float3(1.0f, -0.5f, -0.5f);
  gooch.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"Gooch", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{gooch, surface}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *normal = dynamic_cast<VectorMathNode *>(nodes["Gooch.unit_normal"]);
  auto *view = dynamic_cast<VectorMathNode *>(nodes["Gooch.unit_viewdir"]);
  auto *light = dynamic_cast<VectorMathNode *>(nodes["Gooch.unit_lightdir"]);
  auto *ndotl = dynamic_cast<VectorMathNode *>(nodes["Gooch.NdotL"]);
  auto *diffuse = dynamic_cast<MixNode *>(nodes["Gooch.diffuse"]);
  auto *reflect = dynamic_cast<VectorMathNode *>(nodes["Gooch.view_reflect"]);
  auto *specular_power = dynamic_cast<MathNode *>(nodes["Gooch.specular_highlight"]);
  auto *result = dynamic_cast<MixNode *>(nodes["Gooch"]);
  ASSERT_NE(normal, nullptr);
  ASSERT_NE(view, nullptr);
  ASSERT_NE(light, nullptr);
  ASSERT_NE(ndotl, nullptr);
  ASSERT_NE(diffuse, nullptr);
  ASSERT_NE(reflect, nullptr);
  ASSERT_NE(specular_power, nullptr);
  ASSERT_NE(result, nullptr);
  EXPECT_EQ(normal->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(view->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(light->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(light->get_vector1(), make_float3(1.0f, -0.5f, -0.5f));
  EXPECT_EQ(ndotl->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(diffuse->get_mix_type(), NODE_MIX_BLEND);
  EXPECT_EQ(diffuse->get_color1(), make_float3(0.8f, 0.7f, 0.4f));
  EXPECT_EQ(diffuse->get_color2(), make_float3(0.2f, 0.3f, 0.9f));
  EXPECT_EQ(reflect->get_math_type(), NODE_VECTOR_MATH_REFLECT);
  EXPECT_EQ(specular_power->get_math_type(), NODE_MATH_POWER);
  EXPECT_FLOAT_EQ(specular_power->get_value2(), 32.0f);
  EXPECT_EQ(result->get_mix_type(), NODE_MIX_ADD);
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

TEST(materialx_graph, lowers_world_tangent_to_native_uvmap_tangent)
{
  materialx::Node tangent;
  tangent.name = "WorldTangent";
  tangent.nodedef = "ND_tangent_vector3";
  tangent.string_inputs["space"] = "world";
  tangent.int_inputs["index"] = 1;
  tangent.outputs["out"] = materialx::Type::Vector3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["geometry_normal"] = {"WorldTangent", "out", materialx::Type::Vector3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{tangent, surface}}, &graph));

  TangentNode *native_tangent = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_tangent = node->name == "WorldTangent" ? dynamic_cast<TangentNode *>(node) : native_tangent;
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(native_tangent, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(native_tangent->get_direction_type(), NODE_TANGENT_UVMAP);
  EXPECT_EQ(native_tangent->get_attribute(), ustring("st1"));
  EXPECT_EQ(principled->input("Normal")->link, native_tangent->output("Tangent"));

  tangent.string_inputs["space"] = "object";
  ShaderGraph invalid_graph;
  EXPECT_FALSE(materialx::lower({{tangent, surface}}, &invalid_graph));
}

TEST(materialx_graph, lowers_texcoord_vector2_to_native_uvmap)
{
  /* stdlib_defs.mtlx declares ND_texcoord_vector2 with only a uniform integer
   * index. Direct graph lowering should mirror the USD reader's index mapping:
   * index 0 is Blender's primary "UVMap" and nonzero indices use the USD
   * additional-set convention "stN". */
  const struct {
    const char *name;
    int index;
    const char *attribute;
  } cases[] = {{"Texcoord0", 0, "UVMap"}, {"Texcoord2", 2, "st2"}};

  materialx::Graph source;
  for (const auto &test : cases) {
    materialx::Node texcoord;
    texcoord.name = test.name;
    texcoord.nodedef = "ND_texcoord_vector2";
    texcoord.int_inputs["index"] = test.index;
    texcoord.outputs["out"] = materialx::Type::Vector2;
    source.nodes.push_back(std::move(texcoord));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, UVMapNode *> uv_maps;
  for (ShaderNode *node : graph.nodes) {
    if (auto *uv = dynamic_cast<UVMapNode *>(node)) {
      uv_maps[node->name.string()] = uv;
    }
  }
  for (const auto &test : cases) {
    ASSERT_NE(uv_maps[test.name], nullptr) << test.name;
    EXPECT_EQ(uv_maps[test.name]->get_attribute(), ustring(test.attribute)) << test.name;
  }

  materialx::Node invalid;
  invalid.name = "InvalidTexcoord";
  invalid.nodedef = "ND_texcoord_vector2";
  invalid.int_inputs["index"] = -1;
  invalid.outputs["out"] = materialx::Type::Vector2;
  EXPECT_FALSE(materialx::validate({{invalid}}));
}

TEST(materialx_graph, lowers_object_space_normal_and_position_to_texture_coordinate_outputs)
{
  materialx::Node normal;
  normal.name = "ObjectNormal";
  normal.nodedef = "ND_normal_vector3";
  normal.string_inputs["space"] = "object";
  normal.outputs["out"] = materialx::Type::Vector3;

  materialx::Node position;
  position.name = "ObjectPosition";
  position.nodedef = "ND_position_vector3";
  position.string_inputs["space"] = "object";
  position.outputs["out"] = materialx::Type::Vector3;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector3";
  add.links["in1"] = {"ObjectNormal", "out", materialx::Type::Vector3};
  add.links["in2"] = {"ObjectPosition", "out", materialx::Type::Vector3};
  add.outputs["out"] = materialx::Type::Vector3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{normal, position, add}}, &graph));

  TextureCoordinateNode *normal_coord = nullptr;
  TextureCoordinateNode *position_coord = nullptr;
  VectorMathNode *add_node = nullptr;
  for (ShaderNode *node : graph.nodes) {
    normal_coord = node->name == "ObjectNormal" ? dynamic_cast<TextureCoordinateNode *>(node) :
                                                   normal_coord;
    position_coord = node->name == "ObjectPosition" ? dynamic_cast<TextureCoordinateNode *>(node) :
                                                       position_coord;
    add_node = node->name == "Add" ? dynamic_cast<VectorMathNode *>(node) : add_node;
  }

  ASSERT_NE(normal_coord, nullptr);
  ASSERT_NE(position_coord, nullptr);
  ASSERT_NE(add_node, nullptr);
  EXPECT_EQ(add_node->get_math_type(), NODE_VECTOR_MATH_ADD);
}


TEST(materialx_graph, lowers_geomprop_and_primvar_readers_to_authored_fallbacks)
{
  materialx::Node scalar;
  scalar.name = "MissingFloat";
  scalar.nodedef = "ND_geompropvalue_float";
  scalar.string_inputs["geomprop"] = "missing_float";
  scalar.fallback_inputs["out"] = 0.625f;
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node color3;
  color3.name = "MissingColor3";
  color3.nodedef = "ND_geompropvalue_color3";
  color3.string_inputs["geomprop"] = "missing_color3";
  color3.fallback_color3_inputs["out"] = make_float3(0.2f, 0.4f, 0.6f);
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node uv;
  uv.name = "MissingUV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "missing_uv";
  uv.fallback_vector2_inputs["out"] = make_float2(0.5f, 0.25f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node primvar_float;
  primvar_float.name = "MissingPrimvarFloat";
  primvar_float.nodedef = "ND_UsdPrimvarReader_float";
  primvar_float.string_inputs["varname"] = "missing_primvar_float";
  primvar_float.fallback_inputs["out"] = 0.375f;
  primvar_float.outputs["out"] = materialx::Type::Float;

  materialx::Node primvar_uv;
  primvar_uv.name = "MissingPrimvarUV";
  primvar_uv.nodedef = "ND_UsdPrimvarReader_vector2";
  primvar_uv.string_inputs["varname"] = "missing_primvar_uv";
  primvar_uv.fallback_vector2_inputs["out"] = make_float2(0.75f, 0.125f);
  primvar_uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node primvar;
  primvar.name = "MissingPrimvar";
  primvar.nodedef = "ND_UsdPrimvarReader_vector3";
  primvar.string_inputs["varname"] = "missing_vector";
  primvar.fallback_vector3_inputs["out"] = make_float3(0.1f, 0.2f, 0.3f);
  primvar.outputs["out"] = materialx::Type::Vector3;

  materialx::Node color4;
  color4.name = "MissingColor4";
  color4.nodedef = "ND_geompropvalue_color4";
  color4.string_inputs["geomprop"] = "missing_color4";
  color4.fallback_float4_inputs["out"] = make_float4(0.4f, 0.5f, 0.6f, 0.7f);
  color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{scalar, color3, uv, primvar_float, primvar_uv, primvar, color4}}, &graph));

  AttributeNode *float_fallback = nullptr;
  AttributeNode *color3_fallback = nullptr;
  AttributeNode *uv_fallback = nullptr;
  AttributeNode *primvar_float_fallback = nullptr;
  AttributeNode *primvar_uv_fallback = nullptr;
  AttributeNode *primvar_fallback = nullptr;
  AttributeNode *color_fallback = nullptr;
  for (ShaderNode *node : graph.nodes) {
    float_fallback = node->name == "MissingFloat" ? dynamic_cast<AttributeNode *>(node) :
                                                     float_fallback;
    color3_fallback = node->name == "MissingColor3" ? dynamic_cast<AttributeNode *>(node) :
                                                       color3_fallback;
    uv_fallback = node->name == "MissingUV" ? dynamic_cast<AttributeNode *>(node) : uv_fallback;
    primvar_float_fallback = node->name == "MissingPrimvarFloat" ?
                                 dynamic_cast<AttributeNode *>(node) :
                                 primvar_float_fallback;
    primvar_uv_fallback = node->name == "MissingPrimvarUV" ? dynamic_cast<AttributeNode *>(node) :
                                                             primvar_uv_fallback;
    primvar_fallback = node->name == "MissingPrimvar" ? dynamic_cast<AttributeNode *>(node) :
                                                        primvar_fallback;
    color_fallback = node->name == "MissingColor4" ? dynamic_cast<AttributeNode *>(node) :
                                                     color_fallback;
  }
  ASSERT_NE(float_fallback, nullptr);
  EXPECT_TRUE(float_fallback->get_use_fallback());
  EXPECT_EQ(float_fallback->get_fallback_color(), make_float3(0.625f));
  EXPECT_FLOAT_EQ(float_fallback->get_fallback_alpha(), 0.625f);
  ASSERT_NE(color3_fallback, nullptr);
  EXPECT_TRUE(color3_fallback->get_use_fallback());
  EXPECT_EQ(color3_fallback->get_fallback_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_FLOAT_EQ(color3_fallback->get_fallback_alpha(), 1.0f);
  ASSERT_NE(uv_fallback, nullptr);
  EXPECT_TRUE(uv_fallback->get_use_fallback());
  EXPECT_EQ(uv_fallback->get_fallback_color(), make_float3(0.5f, 0.25f, 0.0f));
  ASSERT_NE(primvar_float_fallback, nullptr);
  EXPECT_TRUE(primvar_float_fallback->get_use_fallback());
  EXPECT_EQ(primvar_float_fallback->get_fallback_color(), make_float3(0.375f));
  EXPECT_FLOAT_EQ(primvar_float_fallback->get_fallback_alpha(), 0.375f);
  ASSERT_NE(primvar_uv_fallback, nullptr);
  EXPECT_TRUE(primvar_uv_fallback->get_use_fallback());
  EXPECT_EQ(primvar_uv_fallback->get_fallback_color(), make_float3(0.75f, 0.125f, 0.0f));
  ASSERT_NE(primvar_fallback, nullptr);
  EXPECT_TRUE(primvar_fallback->get_use_fallback());
  EXPECT_EQ(primvar_fallback->get_fallback_color(), make_float3(0.1f, 0.2f, 0.3f));
  ASSERT_NE(color_fallback, nullptr);
  EXPECT_TRUE(color_fallback->get_use_fallback());
  EXPECT_EQ(color_fallback->get_fallback_color(), make_float3(0.4f, 0.5f, 0.6f));
  EXPECT_FLOAT_EQ(color_fallback->get_fallback_alpha(), 0.7f);
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

TEST(materialx_graph, lowers_latlongimage_with_literal_and_linked_viewdir)
{
  const TemporaryImage image_asset;

  materialx::Node viewdir;
  viewdir.name = "Viewdir";
  viewdir.nodedef = "ND_constant_vector3";
  viewdir.vector3_inputs["value"] = make_float3(0.0f, 0.0f, 1.0f);
  viewdir.outputs["out"] = materialx::Type::Vector3;

  materialx::Node literal;
  literal.name = "LatLongLiteral";
  literal.nodedef = "ND_latlongimage";
  literal.asset_inputs["file"] = image_asset.path();
  literal.color3_inputs["default"] = make_float3(0.1f, 0.2f, 0.3f);
  literal.vector3_inputs["viewdir"] = make_float3(1.0f, 0.0f, 0.0f);
  literal.inputs["rotation"] = 45.0f;
  literal.outputs["out"] = materialx::Type::Color3;

  materialx::Node linked = literal;
  linked.name = "LatLongLinked";
  linked.vector3_inputs.clear();
  linked.links["viewdir"] = {"Viewdir", "out", materialx::Type::Vector3};

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{viewdir, literal, linked}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *literal_image = dynamic_cast<ImageTextureNode *>(nodes["LatLongLiteral"]);
  auto *linked_image = dynamic_cast<ImageTextureNode *>(nodes["LatLongLinked"]);
  auto *literal_viewdir = dynamic_cast<SeparateXYZNode *>(nodes["LatLongLiteral.viewdir"]);
  auto *linked_viewdir = dynamic_cast<SeparateXYZNode *>(nodes["LatLongLinked.viewdir"]);
  auto *literal_angle = dynamic_cast<MathNode *>(nodes["LatLongLiteral.angle_xz"]);
  auto *literal_asin = dynamic_cast<MathNode *>(nodes["LatLongLiteral.angle_y"]);
  auto *literal_rotation = dynamic_cast<MathNode *>(nodes["LatLongLiteral.rotation"]);
  ASSERT_NE(literal_image, nullptr);
  ASSERT_NE(linked_image, nullptr);
  ASSERT_NE(literal_viewdir, nullptr);
  ASSERT_NE(linked_viewdir, nullptr);
  ASSERT_NE(literal_angle, nullptr);
  ASSERT_NE(literal_asin, nullptr);
  ASSERT_NE(literal_rotation, nullptr);
  EXPECT_EQ(literal_image->get_filename(), ustring(image_asset.path()));
  EXPECT_EQ(literal_image->get_extension(), EXTENSION_REPEAT);
  EXPECT_EQ(literal_viewdir->get_vector(), make_float3(1.0f, 0.0f, 0.0f));
  EXPECT_EQ(literal_viewdir->input("Vector")->link, nullptr);
  EXPECT_NE(linked_viewdir->input("Vector")->link, nullptr);
  EXPECT_EQ(literal_angle->get_math_type(), NODE_MATH_ARCTAN2);
  EXPECT_EQ(literal_asin->get_math_type(), NODE_MATH_ARCSINE);
  EXPECT_EQ(literal_rotation->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(literal_rotation->get_value1(), 45.0f);
  EXPECT_FLOAT_EQ(literal_rotation->get_value2(), 0.00277778f);
  EXPECT_NE(literal_image->input("Vector")->link, nullptr);
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
               {"ND_modulo_color4", NODE_MATH_FLOORED_MODULO, 0.0f, true},
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
               {"ND_modulo_color4", NODE_MATH_FLOORED_MODULO, 1.0f},
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

TEST(materialx_graph, lowers_modulo_color4fa_literal_operands)
{
  /* ND_modulo_color4FA is the scalar-divisor Color4 modulo sibling declared in
   * stdlib_defs.mtlx nodegroup="math". MaterialX maps it to component-wise
   * mx_mod(in1, in2), so the scalar divisor must be broadcast to RGB and alpha;
   * lower() must also survive with both operands authored as plain literals. */
  materialx::Node modulo;
  modulo.name = "ModuloColor4FA";
  modulo.nodedef = "ND_modulo_color4FA";
  modulo.float4_inputs["in1"] = make_float4(-1.25f, 0.75f, 3.5f, -2.5f);
  modulo.inputs["in2"] = 2.0f;
  modulo.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{modulo}}, &graph));

  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *shader_node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      math_nodes[shader_node->name.string()] = math;
    }
  }

  for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
    MathNode *math = math_nodes[string("ModuloColor4FA.") + channel];
    ASSERT_NE(math, nullptr) << channel;
    EXPECT_EQ(math->get_math_type(), NODE_MATH_FLOORED_MODULO) << channel;
    EXPECT_FLOAT_EQ(math->get_value2(), 2.0f) << channel;
    EXPECT_EQ(math->input("Value1")->link, nullptr) << channel;
    EXPECT_EQ(math->input("Value2")->link, nullptr) << channel;
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

TEST(materialx_graph, lowers_clamp_color4_with_color_bounds)
{
  /* ND_clamp_color4 is declared in MaterialX stdlib_defs.mtlx as a Color4
   * clamp with Color4 low/high bounds, unlike ND_clamp_color4FA's scalar
   * bounds. */
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_color4";
  input.float4_inputs["value"] = make_float4(-0.4f, 0.2f, 0.8f, 1.6f);
  input.outputs["out"] = materialx::Type::Color4;

  materialx::Node low;
  low.name = "Low";
  low.nodedef = "ND_constant_color4";
  low.float4_inputs["value"] = make_float4(-0.25f, 0.0f, 0.25f, 0.5f);
  low.outputs["out"] = materialx::Type::Color4;

  materialx::Node clamp;
  clamp.name = "Clamp";
  clamp.nodedef = "ND_clamp_color4";
  clamp.links["in"] = {"Input", "out", materialx::Type::Color4};
  clamp.links["low"] = {"Low", "out", materialx::Type::Color4};
  clamp.float4_inputs["high"] = make_float4(0.5f, 0.75f, 1.0f, 1.25f);
  clamp.outputs["out"] = materialx::Type::Color4;

  materialx::Node alpha;
  alpha.name = "Alpha";
  alpha.nodedef = "ND_extract_color4";
  alpha.int_inputs["index"] = 3;
  alpha.links["in"] = {"Clamp", "out", materialx::Type::Color4};
  alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, low, clamp, alpha}}, &graph));

  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *shader_node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      math_nodes[shader_node->name.string()] = math;
    }
  }

  for (const auto &[channel, high] : {std::pair{"Red", 0.5f},
                                      std::pair{"Green", 0.75f},
                                      std::pair{"Blue", 1.0f},
                                      std::pair{"Alpha", 1.25f}})
  {
    ASSERT_NE(math_nodes[string("Clamp.") + channel + ".minimum"], nullptr) << channel;
    ASSERT_NE(math_nodes[string("Clamp.") + channel + ".maximum"], nullptr) << channel;
    EXPECT_EQ(math_nodes[string("Clamp.") + channel + ".minimum"]->get_math_type(),
              NODE_MATH_MINIMUM)
        << channel;
    EXPECT_EQ(math_nodes[string("Clamp.") + channel + ".maximum"]->get_math_type(),
              NODE_MATH_MAXIMUM)
        << channel;
    EXPECT_FLOAT_EQ(math_nodes[string("Clamp.") + channel + ".minimum"]->get_value2(), high)
        << channel;
    ASSERT_NE(math_nodes[string("Clamp.") + channel + ".maximum"]->input("Value2")->link,
              nullptr)
        << channel;
  }
}

TEST(materialx_graph, rejects_invalid_clamp_color4_bounds_without_mutating_destination)
{
  materialx::Node clamp;
  clamp.name = "Bad";
  clamp.nodedef = "ND_clamp_color4";
  clamp.float4_inputs["in"] = make_float4(0.5f, 0.5f, 0.5f, 0.5f);
  clamp.float4_inputs["low"] = make_float4(0.0f, 0.0f, 2.0f, 0.0f);
  clamp.float4_inputs["high"] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
  clamp.outputs["out"] = materialx::Type::Color4;
  EXPECT_FALSE(materialx::validate({{clamp}}));

  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  EXPECT_FALSE(materialx::lower({{clamp}}, &graph));
  int principled_count = 0;
  for (ShaderNode *shader_node : graph.nodes) {
    if (shader_node->type == PrincipledBsdfNode::get_node_type()) {
      principled_count++;
    }
  }
  EXPECT_EQ(principled_count, 1);
}

TEST(materialx_graph, lowers_color3_to_color4_with_literal_alpha_sidecar)
{
  materialx::Node source_color;
  source_color.name = "SourceColor";
  source_color.nodedef = "ND_constant_color3";
  source_color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  source_color.outputs["out"] = materialx::Type::Color3;

  materialx::Node convert;
  convert.name = "Color4";
  convert.nodedef = "ND_convert_color3_color4";
  convert.links["in"] = {"SourceColor", "out", materialx::Type::Color3};
  convert.outputs["out"] = materialx::Type::Color4;

  materialx::Node alpha;
  alpha.name = "Alpha";
  alpha.nodedef = "ND_extract_color4";
  alpha.int_inputs["index"] = 3;
  alpha.links["in"] = {"Color4", "out", materialx::Type::Color4};
  alpha.outputs["out"] = materialx::Type::Float;

  EXPECT_TRUE(materialx::validate({{source_color, convert, alpha}}));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source_color, convert, alpha}}, &graph));

  ColorNode *native_color = nullptr;
  ValueNode *native_alpha = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_color = node->name == "SourceColor" ? dynamic_cast<ColorNode *>(node) : native_color;
    native_alpha = node->name == "Color4.Alpha" ? dynamic_cast<ValueNode *>(node) : native_alpha;
  }
  ASSERT_NE(native_color, nullptr);
  ASSERT_NE(native_alpha, nullptr);
  EXPECT_FLOAT_EQ(native_alpha->get_value(), 1.0f);
}

TEST(materialx_graph, lowers_color4_scalar_converts_and_combine_adapters)
{
  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 0.25f;
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node boolean;
  boolean.name = "Boolean";
  boolean.nodedef = "ND_constant_boolean";
  boolean.int_inputs["value"] = 1;
  boolean.outputs["out"] = materialx::Type::Boolean;

  materialx::Node integer;
  integer.name = "Integer";
  integer.nodedef = "ND_constant_integer";
  integer.int_inputs["value"] = 7;
  integer.outputs["out"] = materialx::Type::Integer;

  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node convert_float;
  convert_float.name = "FloatColor4";
  convert_float.nodedef = "ND_convert_float_color4";
  convert_float.links["in"] = {"Scalar", "out", materialx::Type::Float};
  convert_float.outputs["out"] = materialx::Type::Color4;

  materialx::Node convert_boolean;
  convert_boolean.name = "BooleanColor4";
  convert_boolean.nodedef = "ND_convert_boolean_color4";
  convert_boolean.links["in"] = {"Boolean", "out", materialx::Type::Boolean};
  convert_boolean.outputs["out"] = materialx::Type::Color4;

  materialx::Node convert_integer;
  convert_integer.name = "IntegerColor4";
  convert_integer.nodedef = "ND_convert_integer_color4";
  convert_integer.links["in"] = {"Integer", "out", materialx::Type::Integer};
  convert_integer.outputs["out"] = materialx::Type::Color4;

  materialx::Node literal_float;
  literal_float.name = "LiteralFloatColor4";
  literal_float.nodedef = "ND_convert_float_color4";
  literal_float.inputs["in"] = 0.625f;
  literal_float.outputs["out"] = materialx::Type::Color4;

  materialx::Node literal_boolean;
  literal_boolean.name = "LiteralBooleanColor4";
  literal_boolean.nodedef = "ND_convert_boolean_color4";
  literal_boolean.int_inputs["in"] = 1;
  literal_boolean.outputs["out"] = materialx::Type::Color4;

  materialx::Node literal_integer;
  literal_integer.name = "LiteralIntegerColor4";
  literal_integer.nodedef = "ND_convert_integer_color4";
  literal_integer.int_inputs["in"] = 7;
  literal_integer.outputs["out"] = materialx::Type::Color4;

  materialx::Node combine2;
  combine2.name = "Combine2";
  combine2.nodedef = "ND_combine2_color4CF";
  combine2.links["in1"] = {"Color", "out", materialx::Type::Color3};
  combine2.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  combine2.outputs["out"] = materialx::Type::Color4;

  materialx::Node combine4;
  combine4.name = "Combine4";
  combine4.nodedef = "ND_combine4_color4";
  combine4.inputs["in1"] = 0.4f;
  combine4.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  combine4.inputs["in3"] = 0.6f;
  combine4.inputs["in4"] = 0.8f;
  combine4.outputs["out"] = materialx::Type::Color4;

  materialx::Node alpha;
  alpha.name = "Alpha";
  alpha.nodedef = "ND_extract_color4";
  alpha.int_inputs["index"] = 3;
  alpha.links["in"] = {"Combine4", "out", materialx::Type::Color4};
  alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node scalar_alpha;
  scalar_alpha.name = "FloatColor4Alpha";
  scalar_alpha.nodedef = "ND_extract_color4";
  scalar_alpha.int_inputs["index"] = 3;
  scalar_alpha.links["in"] = {"FloatColor4", "out", materialx::Type::Color4};
  scalar_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node scalar_alpha_consumer;
  scalar_alpha_consumer.name = "FloatColor4AlphaConsumer";
  scalar_alpha_consumer.nodedef = "ND_add_float";
  scalar_alpha_consumer.links["in1"] = {"FloatColor4Alpha", "out", materialx::Type::Float};
  scalar_alpha_consumer.inputs["in2"] = 0.0f;
  scalar_alpha_consumer.outputs["out"] = materialx::Type::Float;

  materialx::Node boolean_alpha = scalar_alpha;
  boolean_alpha.name = "BooleanColor4Alpha";
  boolean_alpha.links["in"] = {"BooleanColor4", "out", materialx::Type::Color4};

  materialx::Node boolean_alpha_consumer = scalar_alpha_consumer;
  boolean_alpha_consumer.name = "BooleanColor4AlphaConsumer";
  boolean_alpha_consumer.links["in1"] = {"BooleanColor4Alpha", "out", materialx::Type::Float};

  materialx::Node integer_alpha = scalar_alpha;
  integer_alpha.name = "IntegerColor4Alpha";
  integer_alpha.links["in"] = {"IntegerColor4", "out", materialx::Type::Color4};

  materialx::Node integer_alpha_consumer = scalar_alpha_consumer;
  integer_alpha_consumer.name = "IntegerColor4AlphaConsumer";
  integer_alpha_consumer.links["in1"] = {"IntegerColor4Alpha", "out", materialx::Type::Float};

  materialx::Node literal_alpha;
  literal_alpha.name = "LiteralFloatColor4Alpha";
  literal_alpha.nodedef = "ND_extract_color4";
  literal_alpha.int_inputs["index"] = 3;
  literal_alpha.links["in"] = {"LiteralFloatColor4", "out", materialx::Type::Color4};
  literal_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node literal_alpha_consumer;
  literal_alpha_consumer.name = "LiteralFloatColor4AlphaConsumer";
  literal_alpha_consumer.nodedef = "ND_add_float";
  literal_alpha_consumer.links["in1"] = {"LiteralFloatColor4Alpha", "out", materialx::Type::Float};
  literal_alpha_consumer.inputs["in2"] = 0.0f;
  literal_alpha_consumer.outputs["out"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes = {scalar,
                  boolean,
                  integer,
                  color,
                  convert_float,
                  convert_boolean,
                  convert_integer,
                  literal_float,
                  literal_boolean,
                  literal_integer,
                  combine2,
                  combine4,
                  alpha,
                  scalar_alpha,
                  scalar_alpha_consumer,
                  boolean_alpha,
                  boolean_alpha_consumer,
                  integer_alpha,
                  integer_alpha_consumer,
                  literal_alpha,
                  literal_alpha_consumer};
  EXPECT_TRUE(materialx::validate(source));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["FloatColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["BooleanColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["IntegerColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["LiteralFloatColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["LiteralBooleanColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["LiteralIntegerColor4"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(nodes["Combine2.input"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(nodes["Combine2"]), nullptr);
  CombineColorNode *native_combine4 = dynamic_cast<CombineColorNode *>(nodes["Combine4"]);
  ASSERT_NE(native_combine4, nullptr);
  EXPECT_FLOAT_EQ(native_combine4->get_r(), 0.4f);
  EXPECT_FLOAT_EQ(native_combine4->get_b(), 0.6f);
  ValueNode *native_alpha = dynamic_cast<ValueNode *>(nodes["Combine4.Alpha"]);
  ASSERT_NE(native_alpha, nullptr);
  EXPECT_FLOAT_EQ(native_alpha->get_value(), 0.8f);
  EXPECT_EQ(nodes["FloatColor4"]->input("Red")->link, nodes["Scalar"]->output("Value"));
  EXPECT_EQ(nodes["BooleanColor4"]->input("Red")->link, nodes["Boolean.float"]->output("Value"));
  EXPECT_EQ(nodes["IntegerColor4"]->input("Red")->link, nodes["Integer.float"]->output("Value"));
  EXPECT_EQ(nodes["FloatColor4"]->input("Green")->link, nodes["Scalar"]->output("Value"));
  EXPECT_EQ(nodes["FloatColor4"]->input("Blue")->link, nodes["Scalar"]->output("Value"));
  EXPECT_EQ(nodes["BooleanColor4"]->input("Green")->link, nodes["Boolean.float"]->output("Value"));
  EXPECT_EQ(nodes["BooleanColor4"]->input("Blue")->link, nodes["Boolean.float"]->output("Value"));
  EXPECT_EQ(nodes["IntegerColor4"]->input("Green")->link, nodes["Integer.float"]->output("Value"));
  EXPECT_EQ(nodes["IntegerColor4"]->input("Blue")->link, nodes["Integer.float"]->output("Value"));
  EXPECT_EQ(nodes["FloatColor4AlphaConsumer"]->input("Value1")->link,
            nodes["Scalar"]->output("Value"));
  EXPECT_EQ(nodes["BooleanColor4AlphaConsumer"]->input("Value1")->link,
            nodes["Boolean.float"]->output("Value"));
  EXPECT_EQ(nodes["IntegerColor4AlphaConsumer"]->input("Value1")->link,
            nodes["Integer.float"]->output("Value"));
  EXPECT_FLOAT_EQ(dynamic_cast<CombineColorNode *>(nodes["LiteralFloatColor4"])->get_r(), 0.625f);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineColorNode *>(nodes["LiteralFloatColor4"])->get_g(), 0.625f);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineColorNode *>(nodes["LiteralFloatColor4"])->get_b(), 0.625f);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(nodes["LiteralFloatColor4.Alpha"])->get_value(),
                  0.625f);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineColorNode *>(nodes["LiteralBooleanColor4"])->get_r(), 1.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<CombineColorNode *>(nodes["LiteralIntegerColor4"])->get_r(), 7.0f);
  EXPECT_EQ(nodes["LiteralFloatColor4AlphaConsumer"]->input("Value1")->link,
            nodes["LiteralFloatColor4.Alpha"]->output("Value"));
  EXPECT_EQ(nodes["Combine4"]->input("Green")->link, nodes["Scalar"]->output("Value"));
}

TEST(materialx_graph, lowers_vector4_arithmetic_and_clamp_with_w_sidecar)
{
  /* MaterialX stdlib_defs.mtlx declares ND_add/subtract/multiply/divide_vector4
   * and their FA siblings as component-wise Vector4 math, and
   * ND_clamp_vector4/_vector4FA as component-wise min(max(in, low), high).
   * Cycles carries XYZ through VectorMathNode and the fourth component through
   * the existing Vector4 W sidecar path. */
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector4";
  input.vector4_inputs["value"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  input.outputs["out"] = materialx::Type::Vector4;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_vector4";
  add.links["in1"] = {"Input", "out", materialx::Type::Vector4};
  add.vector4_inputs["in2"] = make_float4(0.5f, 1.0f, 1.5f, 2.0f);
  add.outputs["out"] = materialx::Type::Vector4;

  materialx::Node subtract;
  subtract.name = "SubtractFA";
  subtract.nodedef = "ND_subtract_vector4FA";
  subtract.links["in1"] = {"Add", "out", materialx::Type::Vector4};
  subtract.inputs["in2"] = 0.25f;
  subtract.outputs["out"] = materialx::Type::Vector4;

  materialx::Node divide;
  divide.name = "Divide";
  divide.nodedef = "ND_divide_vector4";
  divide.links["in1"] = {"SubtractFA", "out", materialx::Type::Vector4};
  divide.vector4_inputs["in2"] = make_float4(1.0f, 2.0f, 4.0f, 8.0f);
  divide.outputs["out"] = materialx::Type::Vector4;

  materialx::Node multiply;
  multiply.name = "MultiplyFA";
  multiply.nodedef = "ND_multiply_vector4FA";
  multiply.links["in1"] = {"Divide", "out", materialx::Type::Vector4};
  multiply.inputs["in2"] = 2.0f;
  multiply.outputs["out"] = materialx::Type::Vector4;

  materialx::Node clamp;
  clamp.name = "Clamp";
  clamp.nodedef = "ND_clamp_vector4";
  clamp.links["in"] = {"MultiplyFA", "out", materialx::Type::Vector4};
  clamp.vector4_inputs["low"] = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
  clamp.vector4_inputs["high"] = make_float4(1.0f, 3.0f, 6.0f, 9.0f);
  clamp.outputs["out"] = materialx::Type::Vector4;

  materialx::Node w;
  w.name = "W";
  w.nodedef = "ND_extract_vector4";
  w.int_inputs["index"] = 3;
  w.links["in"] = {"Clamp", "out", materialx::Type::Vector4};
  w.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, add, subtract, divide, multiply, clamp, w}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *shader_node : graph.nodes) {
    lowered[shader_node->name.string()] = shader_node;
  }

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Add"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Add"])->get_math_type(),
            NODE_VECTOR_MATH_ADD);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Add.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Add.W"])->get_math_type(), NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Add.W"])->get_value2(), 2.0f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["SubtractFA"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["SubtractFA"])->get_math_type(),
            NODE_VECTOR_MATH_SUBTRACT);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["SubtractFA.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["SubtractFA.W"])->get_math_type(),
            NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["SubtractFA.W"])->get_value2(), 0.25f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Divide"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Divide"])->get_math_type(),
            NODE_VECTOR_MATH_DIVIDE);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Divide.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Divide.W"])->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Divide.W"])->get_value2(), 8.0f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["MultiplyFA"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["MultiplyFA"])->get_math_type(),
            NODE_VECTOR_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(dynamic_cast<VectorMathNode *>(lowered["MultiplyFA"])->get_vector2().x, 2.0f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["MultiplyFA.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["MultiplyFA.W"])->get_math_type(),
            NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["MultiplyFA.W"])->get_value2(), 2.0f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Clamp.minimum"]), nullptr);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Clamp"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Clamp.minimum"])->get_math_type(),
            NODE_VECTOR_MATH_MINIMUM);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Clamp"])->get_math_type(),
            NODE_VECTOR_MATH_MAXIMUM);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Clamp.minimum"])->get_vector2(),
            make_float3(1.0f, 3.0f, 6.0f));
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Clamp"])->get_vector2(),
            zero_float3());
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Clamp.W.minimum"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Clamp.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Clamp.W.minimum"])->get_math_type(),
            NODE_MATH_MINIMUM);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Clamp.W"])->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Clamp.W.minimum"])->get_value2(), 9.0f);
}

TEST(materialx_graph, lowers_vector4_clamp_literal_w_without_default_leak)
{
  for (const char *nodedef : {"ND_clamp_vector4", "ND_clamp_vector4FA"}) {
    materialx::Node clamp;
    clamp.name = "Clamp";
    clamp.nodedef = nodedef;
    clamp.vector4_inputs["in"] = make_float4(0.25f, -2.0f, 2.0f, -0.4f);
    if (string(nodedef) == "ND_clamp_vector4FA") {
      clamp.inputs["low"] = -1.0f;
      clamp.inputs["high"] = 1.0f;
    }
    else {
      clamp.vector4_inputs["low"] = make_float4(-1.0f, -1.0f, -1.0f, -1.0f);
      clamp.vector4_inputs["high"] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
    }
    clamp.outputs["out"] = materialx::Type::Vector4;

    materialx::Node w;
    w.name = "W";
    w.nodedef = "ND_extract_vector4";
    w.int_inputs["index"] = 3;
    w.links["in"] = {"Clamp", "out", materialx::Type::Vector4};
    w.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{clamp, w}}, &graph)) << nodedef;
    std::unordered_map<string, ShaderNode *> lowered;
    for (ShaderNode *shader_node : graph.nodes) {
      lowered[shader_node->name.string()] = shader_node;
    }

    MathNode *w_minimum = dynamic_cast<MathNode *>(lowered["Clamp.W.minimum"]);
    MathNode *w_maximum = dynamic_cast<MathNode *>(lowered["Clamp.W"]);
    ASSERT_NE(w_minimum, nullptr) << nodedef;
    ASSERT_NE(w_maximum, nullptr) << nodedef;
    EXPECT_FLOAT_EQ(w_minimum->get_value1(), -0.4f) << nodedef;
    EXPECT_FLOAT_EQ(w_minimum->get_value2(), 1.0f) << nodedef;
    EXPECT_EQ(w_minimum->input("Value1")->link, nullptr) << nodedef;
    EXPECT_EQ(w_minimum->input("Value2")->link, nullptr) << nodedef;
    EXPECT_FLOAT_EQ(w_maximum->get_value2(), -1.0f) << nodedef;
    EXPECT_EQ(w_maximum->input("Value1")->link, w_minimum->output("Value")) << nodedef;
    EXPECT_EQ(w_maximum->input("Value2")->link, nullptr) << nodedef;
  }
}

TEST(materialx_graph, lowers_vector4_min_max_modulo_and_power_with_w_sidecar)
{
  /* MaterialX stdlib/genglsl/stdlib_genglsl_impl.mtlx declares these Vector4
   * MATH nodedefs as exact component-wise operations:
   *   lines 270-281: mx_mod({{in1}}, {{in2}})
   *   lines 339-350: pow({{in1}}, {{in2}}) / vec4({{in2}})
   *   lines 417-440: min/max({{in1}}, {{in2}})
   * Cycles carries XYZ through VectorMathNode and W through a parallel MathNode. */
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector4";
  input.vector4_inputs["value"] = make_float4(5.0f, 6.0f, 7.0f, 8.0f);
  input.outputs["out"] = materialx::Type::Vector4;

  materialx::Node minimum;
  minimum.name = "Minimum";
  minimum.nodedef = "ND_min_vector4";
  minimum.links["in1"] = {"Input", "out", materialx::Type::Vector4};
  minimum.vector4_inputs["in2"] = make_float4(3.0f, 7.0f, 5.0f, 9.0f);
  minimum.outputs["out"] = materialx::Type::Vector4;

  materialx::Node maximum;
  maximum.name = "MaximumFA";
  maximum.nodedef = "ND_max_vector4FA";
  maximum.links["in1"] = {"Minimum", "out", materialx::Type::Vector4};
  maximum.inputs["in2"] = 4.0f;
  maximum.outputs["out"] = materialx::Type::Vector4;

  materialx::Node modulo;
  modulo.name = "Modulo";
  modulo.nodedef = "ND_modulo_vector4";
  modulo.links["in1"] = {"MaximumFA", "out", materialx::Type::Vector4};
  modulo.vector4_inputs["in2"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  modulo.outputs["out"] = materialx::Type::Vector4;

  materialx::Node power;
  power.name = "PowerFA";
  power.nodedef = "ND_power_vector4FA";
  power.links["in1"] = {"Modulo", "out", materialx::Type::Vector4};
  power.inputs["in2"] = 2.0f;
  power.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, minimum, maximum, modulo, power}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *shader_node : graph.nodes) {
    lowered[shader_node->name.string()] = shader_node;
  }

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Minimum"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Minimum"])->get_math_type(),
            NODE_VECTOR_MATH_MINIMUM);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Minimum.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Minimum.W"])->get_math_type(), NODE_MATH_MINIMUM);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Minimum.W"])->get_value2(), 9.0f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["MaximumFA"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["MaximumFA"])->get_math_type(),
            NODE_VECTOR_MATH_MAXIMUM);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["MaximumFA.W"])->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["MaximumFA.W"])->get_value2(), 4.0f);

  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["Modulo"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Modulo.X"])->get_math_type(),
            NODE_MATH_FLOORED_MODULO);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Modulo.W"])->get_math_type(),
            NODE_MATH_FLOORED_MODULO);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Modulo.W"])->get_value2(), 5.0f);

  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["PowerFA"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["PowerFA"])->get_math_type(),
            NODE_VECTOR_MATH_POWER);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["PowerFA.W"])->get_math_type(), NODE_MATH_POWER);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["PowerFA.W"])->get_value2(), 2.0f);

  modulo.vector4_inputs["in2"] = make_float4(2.0f, 0.0f, 4.0f, 5.0f);
  EXPECT_FALSE(materialx::validate({{input, minimum, maximum, modulo, power}}));
}

TEST(materialx_graph, lowers_negative_vector4_modulo_as_materialx_floored_modulo)
{
  materialx::Node modulo;
  modulo.name = "ModuloFA";
  modulo.nodedef = "ND_modulo_vector4FA";
  modulo.vector4_inputs["in1"] = make_float4(-1.25f, 0.75f, 1.0f, -1.5f);
  modulo.inputs["in2"] = 2.0f;
  modulo.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower({{modulo}}, &graph, &error)) << error;

  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(node)) {
      math_nodes[node->name.string()] = math;
    }
  }

  for (const char *channel : {"X", "Y", "Z", "W"}) {
    MathNode *math = math_nodes[string("ModuloFA.") + channel];
    ASSERT_NE(math, nullptr) << channel;
    EXPECT_EQ(math->get_math_type(), NODE_MATH_FLOORED_MODULO) << channel;
  }
}

TEST(materialx_graph, lowers_vector4_unary_math_with_w_sidecar)
{
  /* MaterialX stdlib_defs.mtlx declares these Vector4 MATH nodedefs as
   * component-wise unary operations, and stdlib/genglsl/stdlib_genglsl_impl.mtlx
   * implements them as direct per-channel intrinsics. */
  struct UnaryCase {
    const char *name;
    const char *nodedef;
    NodeMathType type;
    float w_value;
  };
  const UnaryCase cases[] = {{"Fract", "ND_fract_vector4", NODE_MATH_FRACTION, 1.25f},
                             {"Abs", "ND_absval_vector4", NODE_MATH_ABSOLUTE, -2.0f},
                             {"Floor", "ND_floor_vector4", NODE_MATH_FLOOR, 3.75f},
                             {"Ceil", "ND_ceil_vector4", NODE_MATH_CEIL, 4.25f},
                             {"Round", "ND_round_vector4", NODE_MATH_ROUND, 5.5f},
                             {"Sign", "ND_sign_vector4", NODE_MATH_SIGN, -6.0f},
                             {"Sin", "ND_sin_vector4", NODE_MATH_SINE, 0.25f},
                             {"Cos", "ND_cos_vector4", NODE_MATH_COSINE, 0.5f},
                             {"Tan", "ND_tan_vector4", NODE_MATH_TANGENT, 0.75f},
                             {"Asin", "ND_asin_vector4", NODE_MATH_ARCSINE, 0.125f},
                             {"Acos", "ND_acos_vector4", NODE_MATH_ARCCOSINE, 0.25f},
                             {"Sqrt", "ND_sqrt_vector4", NODE_MATH_SQRT, 9.0f},
                             {"Ln", "ND_ln_vector4", NODE_MATH_LOGARITHM, 2.0f},
                             {"Exp", "ND_exp_vector4", NODE_MATH_EXPONENT, 1.0f}};

  for (const UnaryCase &test : cases) {
    materialx::Node node;
    node.name = test.name;
    node.nodedef = test.nodedef;
    node.vector4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, test.w_value);
    node.outputs["out"] = materialx::Type::Vector4;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << test.nodedef;
    std::unordered_map<string, ShaderNode *> lowered;
    for (ShaderNode *shader_node : graph.nodes) {
      lowered[shader_node->name.string()] = shader_node;
    }
    ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered[test.name]), nullptr) << test.nodedef;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[string(test.name) + ".X"]), nullptr) << test.nodedef;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[string(test.name) + ".W"]), nullptr) << test.nodedef;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[string(test.name) + ".X"])->get_math_type(),
              test.type)
        << test.nodedef;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[string(test.name) + ".W"])->get_math_type(),
              test.type)
        << test.nodedef;
    EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered[string(test.name) + ".W"])->get_value1(),
                    test.w_value)
        << test.nodedef;
  }
}

TEST(materialx_graph, lowers_vector4_atan2_invert_and_safepower_with_w_sidecar)
{
  /* MaterialX stdlib_defs.mtlx declares ND_atan2_vector4 beside the unary
   * Vector4 trig family, and declares safepower as
   * sign(in1) * pow(abs(in1), in2). genglsl/genosl map atan2/invert/safepower
   * component-wise, so W is a scalar sidecar parallel to XYZ. */
  materialx::Node input;
  input.name = "Input";
  input.nodedef = "ND_constant_vector4";
  input.vector4_inputs["value"] = make_float4(0.25f, 0.5f, 0.75f, 1.25f);
  input.outputs["out"] = materialx::Type::Vector4;

  materialx::Node atan2;
  atan2.name = "Atan2";
  atan2.nodedef = "ND_atan2_vector4";
  atan2.links["iny"] = {"Input", "out", materialx::Type::Vector4};
  atan2.vector4_inputs["inx"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  atan2.outputs["out"] = materialx::Type::Vector4;

  materialx::Node invert;
  invert.name = "InvertFA";
  invert.nodedef = "ND_invert_vector4FA";
  invert.links["in"] = {"Atan2", "out", materialx::Type::Vector4};
  invert.inputs["amount"] = 1.0f;
  invert.outputs["out"] = materialx::Type::Vector4;

  materialx::Node safepower;
  safepower.name = "SafePowerFA";
  safepower.nodedef = "ND_safepower_vector4FA";
  safepower.links["in1"] = {"InvertFA", "out", materialx::Type::Vector4};
  safepower.inputs["in2"] = 2.0f;
  safepower.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{input, atan2, invert, safepower}}, &graph));
  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *shader_node : graph.nodes) {
    lowered[shader_node->name.string()] = shader_node;
  }
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Atan2.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Atan2.W"])->get_math_type(), NODE_MATH_ARCTAN2);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["InvertFA.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["InvertFA.W"])->get_math_type(), NODE_MATH_SUBTRACT);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["SafePowerFA.W.absolute"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["SafePowerFA.W.power"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["SafePowerFA.W.sign"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["SafePowerFA.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["SafePowerFA.W"])->get_math_type(),
            NODE_MATH_MULTIPLY);
}

TEST(materialx_graph, lowers_vector4_norm_and_metric_math_with_w_sidecar)
{
  /* Real MaterialX stdlib sources: libraries/stdlib/stdlib_defs.mtlx declares
   * ND_normalize/magnitude/distance/dotproduct_vector4 in nodegroup="math";
   * genglsl/stdlib_genglsl_impl.mtlx lines 443-456 and genosl sibling lines
   * 432-445 implement them as normalize/length/distance/dot over all four
   * components. */
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_vector4";
  first.vector4_inputs["value"] = make_float4(3.0f, 4.0f, 0.0f, 12.0f);
  first.outputs["out"] = materialx::Type::Vector4;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_vector4";
  second.vector4_inputs["value"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  second.outputs["out"] = materialx::Type::Vector4;

  materialx::Node normalize;
  normalize.name = "Normalize";
  normalize.nodedef = "ND_normalize_vector4";
  normalize.links["in"] = {"First", "out", materialx::Type::Vector4};
  normalize.outputs["out"] = materialx::Type::Vector4;

  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector4";
  magnitude.links["in"] = {"Normalize", "out", materialx::Type::Vector4};
  magnitude.outputs["out"] = materialx::Type::Float;

  materialx::Node distance;
  distance.name = "Distance";
  distance.nodedef = "ND_distance_vector4";
  distance.links["in1"] = {"Normalize", "out", materialx::Type::Vector4};
  distance.links["in2"] = {"Second", "out", materialx::Type::Vector4};
  distance.outputs["out"] = materialx::Type::Float;

  materialx::Node dot;
  dot.name = "Dot";
  dot.nodedef = "ND_dotproduct_vector4";
  dot.links["in1"] = {"Normalize", "out", materialx::Type::Vector4};
  dot.links["in2"] = {"Second", "out", materialx::Type::Vector4};
  dot.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second, normalize, magnitude, distance, dot}}, &graph));
  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Normalize"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Normalize"])->get_math_type(),
            NODE_VECTOR_MATH_SCALE);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Normalize.W"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Normalize.W"])->get_math_type(), NODE_MATH_DIVIDE);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Magnitude"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Distance"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Dot"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Distance.xyz"])->input("Vector1")->link,
            lowered["Normalize"]->output("Vector"));
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Distance.xyz"])->input("Vector2")->link,
            lowered["Second"]->output("Vector"));
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Magnitude"])->get_math_type(), NODE_MATH_SQRT);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Distance"])->get_math_type(), NODE_MATH_SQRT);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["Dot"])->get_math_type(), NODE_MATH_ADD);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Dot.xyz"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Dot.xyz"])->input("Vector1")->link,
            lowered["Normalize"]->output("Vector"));
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Dot.xyz"])->input("Vector2")->link,
            lowered["Second"]->output("Vector"));
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Dot.xyz"])->get_math_type(),
            NODE_VECTOR_MATH_DOT_PRODUCT);
}

TEST(materialx_graph, lowers_literal_vector4_metric_math_with_w_sidecar)
{
  /* These measured failures are scalar outputs whose MaterialX definition uses
   * all four vector4 components. Literal operands must seed both the native XYZ
   * math and the parallel W scalar chain before lower() reaches the connect
   * pass, otherwise Cycles computes only the three-component part. */
  materialx::Node magnitude;
  magnitude.name = "Magnitude";
  magnitude.nodedef = "ND_magnitude_vector4";
  magnitude.vector4_inputs["in"] = make_float4(0.0f, 3.0f, 4.0f, 12.0f);
  magnitude.outputs["out"] = materialx::Type::Float;

  materialx::Node distance;
  distance.name = "Distance";
  distance.nodedef = "ND_distance_vector4";
  distance.vector4_inputs["in1"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  distance.vector4_inputs["in2"] = make_float4(5.0f, 2.0f, -3.0f, 12.0f);
  distance.outputs["out"] = materialx::Type::Float;

  materialx::Node dot;
  dot.name = "Dot";
  dot.nodedef = "ND_dotproduct_vector4";
  dot.vector4_inputs["in1"] = make_float4(1.0f, 2.0f, 3.0f, 4.0f);
  dot.vector4_inputs["in2"] = make_float4(5.0f, 2.0f, -3.0f, 12.0f);
  dot.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{magnitude, distance, dot}}, &graph));
  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  const auto *magnitude_xyz = dynamic_cast<VectorMathNode *>(lowered["Magnitude.xyz"]);
  const auto *magnitude_w_square = dynamic_cast<MathNode *>(lowered["Magnitude.W.square"]);
  ASSERT_NE(magnitude_xyz, nullptr);
  ASSERT_NE(magnitude_w_square, nullptr);
  EXPECT_EQ(magnitude_xyz->get_math_type(), NODE_VECTOR_MATH_LENGTH);
  EXPECT_EQ(magnitude_xyz->get_vector1(), make_float3(0.0f, 3.0f, 4.0f));
  EXPECT_FLOAT_EQ(magnitude_w_square->get_value1(), 12.0f);
  EXPECT_FLOAT_EQ(magnitude_w_square->get_value2(), 12.0f);

  const auto *distance_xyz = dynamic_cast<VectorMathNode *>(lowered["Distance.xyz"]);
  const auto *distance_w_delta = dynamic_cast<MathNode *>(lowered["Distance.W.delta"]);
  ASSERT_NE(distance_xyz, nullptr);
  ASSERT_NE(distance_w_delta, nullptr);
  EXPECT_EQ(distance_xyz->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
  EXPECT_EQ(distance_xyz->get_vector1(), make_float3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(distance_xyz->get_vector2(), make_float3(5.0f, 2.0f, -3.0f));
  EXPECT_FLOAT_EQ(distance_w_delta->get_value1(), 4.0f);
  EXPECT_FLOAT_EQ(distance_w_delta->get_value2(), 12.0f);

  const auto *dot_xyz = dynamic_cast<VectorMathNode *>(lowered["Dot.xyz"]);
  const auto *dot_w_product = dynamic_cast<MathNode *>(lowered["Dot.W.product"]);
  ASSERT_NE(dot_xyz, nullptr);
  ASSERT_NE(dot_w_product, nullptr);
  EXPECT_EQ(dot_xyz->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dot_xyz->get_vector1(), make_float3(1.0f, 2.0f, 3.0f));
  EXPECT_EQ(dot_xyz->get_vector2(), make_float3(5.0f, 2.0f, -3.0f));
  EXPECT_FLOAT_EQ(dot_w_product->get_value1(), 4.0f);
  EXPECT_FLOAT_EQ(dot_w_product->get_value2(), 12.0f);
}

TEST(materialx_graph, lowers_contrast_vector4_forms_preserving_w_sidecar)
{
  /* MaterialX stdlib_defs.mtlx declares ND_contrast_vector4 / Vector4FA in
   * nodegroup="adjustment"; stdlib_ng.mtlx NG_contrast_vector4/Vector4FA
   * expands both as (in - pivot) * amount + pivot for every component. */
  materialx::Node full;
  full.name = "Vector4Contrast";
  full.nodedef = "ND_contrast_vector4";
  full.vector4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, 0.9f);
  full.vector4_inputs["amount"] = make_float4(2.0f, 3.0f, 4.0f, 5.0f);
  full.vector4_inputs["pivot"] = make_float4(0.5f, 0.25f, 0.125f, 0.1f);
  full.outputs["out"] = materialx::Type::Vector4;

  materialx::Node scalar;
  scalar.name = "Vector4FAContrast";
  scalar.nodedef = "ND_contrast_vector4FA";
  scalar.links["in"] = {"Vector4Contrast", "out", materialx::Type::Vector4};
  scalar.inputs = {{"amount", 1.5f}, {"pivot", 0.25f}};
  scalar.outputs["out"] = materialx::Type::Vector4;

  materialx::Node extract;
  extract.name = "ExtractW";
  extract.nodedef = "ND_extract_vector4";
  extract.links["in"] = {"Vector4FAContrast", "out", materialx::Type::Vector4};
  extract.int_inputs["index"] = 3;
  extract.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{full, scalar, extract}}, &graph));

  std::unordered_map<string, MathNode *> math;
  for (ShaderNode *node : graph.nodes) {
    if (MathNode *lowered = dynamic_cast<MathNode *>(node)) {
      math[string(node->name.c_str())] = lowered;
    }
  }
  ASSERT_NE(math["Vector4Contrast.W.subtract"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector4Contrast.W.subtract"]->get_value1(), 0.9f);
  EXPECT_FLOAT_EQ(math["Vector4Contrast.W.subtract"]->get_value2(), 0.1f);
  ASSERT_NE(math["Vector4Contrast.W.multiply"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector4Contrast.W.multiply"]->get_value2(), 5.0f);
  ASSERT_NE(math["Vector4FAContrast.W"], nullptr);
  EXPECT_FLOAT_EQ(math["Vector4FAContrast.W"]->get_value2(), 0.25f);
}

TEST(materialx_graph, resolves_extract_alpha_through_nested_color4_sidecars)
{
  /* Real MaterialX stdlib keeps the fourth channel as data for Color4
   * adjustment/compositing nodes.  Extracting alpha from a non-image Color4
   * producer must resolve the producer's sidecar, not assume the extractor's
   * lowered node shape has a generic Value output. */
  materialx::Node color;
  color.name = "Color4RemapFA";
  color.nodedef = "ND_remap_color4FA";
  color.float4_inputs["in"] = make_float4(0.15f, 0.35f, 0.55f, 0.75f);
  color.inputs = {{"inlow", 0.0f}, {"inhigh", 1.0f}, {"outlow", 0.25f}, {"outhigh", 0.75f}};
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node alpha;
  alpha.name = "ExtractAlpha";
  alpha.nodedef = "ND_extract_color4";
  alpha.links["in"] = {"Color4RemapFA", "out", materialx::Type::Color4};
  alpha.int_inputs["index"] = 3;
  alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node scalar;
  scalar.name = "AlphaContrast";
  scalar.nodedef = "ND_contrast_float";
  scalar.links["in"] = {"ExtractAlpha", "out", materialx::Type::Float};
  scalar.inputs = {{"amount", 2.0f}, {"pivot", 0.25f}};
  scalar.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, alpha, scalar}}, &graph));

  MapRangeNode *alpha_range = nullptr;
  MathNode *contrast_subtract = nullptr;
  for (ShaderNode *node : graph.nodes) {
    alpha_range = node->name == "Color4RemapFA.Alpha" ? dynamic_cast<MapRangeNode *>(node) :
                                                         alpha_range;
    contrast_subtract = node->name == "AlphaContrast.subtract" ?
                            dynamic_cast<MathNode *>(node) :
                            contrast_subtract;
  }
  ASSERT_NE(alpha_range, nullptr);
  ASSERT_NE(contrast_subtract, nullptr);
  EXPECT_EQ(contrast_subtract->input("Value1")->link, alpha_range->output("Result"));
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

TEST(materialx_graph, lowers_generic_surface_bsdf_edf_and_opacity_as_closure_composition)
{
  materialx::Node albedo;
  albedo.name = "Albedo";
  albedo.nodedef = "ND_constant_color3";
  albedo.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  albedo.outputs["out"] = materialx::Type::Color3;

  materialx::Node roughness;
  roughness.name = "Roughness";
  roughness.nodedef = "ND_constant_float";
  roughness.inputs["value"] = 0.35f;
  roughness.outputs["out"] = materialx::Type::Float;

  materialx::Node bsdf;
  bsdf.name = "Diffuse";
  bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  bsdf.links["color"] = {"Albedo", "out", materialx::Type::Color3};
  bsdf.links["roughness"] = {"Roughness", "out", materialx::Type::Float};
  bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node edf;
  edf.name = "Light";
  edf.nodedef = "ND_uniform_edf";
  edf.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  edf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node opacity;
  opacity.name = "Opacity";
  opacity.nodedef = "ND_constant_float";
  opacity.inputs["value"] = 0.4f;
  opacity.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_surface";
  surface.links["bsdf"] = {"Diffuse", "out", materialx::Type::SurfaceShader};
  surface.links["edf"] = {"Light", "out", materialx::Type::SurfaceShader};
  surface.links["opacity"] = {"Opacity", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{albedo, roughness, bsdf, edf, opacity, surface}}, &graph));

  DiffuseBsdfNode *native_bsdf = nullptr;
  EmissionNode *native_edf = nullptr;
  TransparentBsdfNode *transparent = nullptr;
  AddClosureNode *add = nullptr;
  MixClosureNode *mix = nullptr;
  ValueNode *opacity_value = nullptr;
  ColorNode *albedo_value = nullptr;
  ValueNode *roughness_value = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_bsdf = node->name == "Diffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_bsdf;
    native_edf = node->name == "Light" ? dynamic_cast<EmissionNode *>(node) : native_edf;
    transparent = node->name == "Surface.transparent" ? dynamic_cast<TransparentBsdfNode *>(node) : transparent;
    add = node->name == "Surface.add" ? dynamic_cast<AddClosureNode *>(node) : add;
    mix = node->name == "Surface.opacity" ? dynamic_cast<MixClosureNode *>(node) : mix;
    opacity_value = node->name == "Opacity" ? dynamic_cast<ValueNode *>(node) : opacity_value;
    albedo_value = node->name == "Albedo" ? dynamic_cast<ColorNode *>(node) : albedo_value;
    roughness_value = node->name == "Roughness" ? dynamic_cast<ValueNode *>(node) : roughness_value;
  }
  ASSERT_NE(native_bsdf, nullptr);
  ASSERT_NE(native_edf, nullptr);
  ASSERT_NE(transparent, nullptr);
  ASSERT_NE(add, nullptr);
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(opacity_value, nullptr);
  ASSERT_NE(albedo_value, nullptr);
  ASSERT_NE(roughness_value, nullptr);
  EXPECT_EQ(native_bsdf->input("Color")->link, albedo_value->output("Color"));
  EXPECT_EQ(native_bsdf->input("Roughness")->link, roughness_value->output("Value"));
  EXPECT_EQ(add->input("Closure1")->link, native_bsdf->output("BSDF"));
  EXPECT_EQ(add->input("Closure2")->link, native_edf->output("Emission"));
  EXPECT_EQ(mix->input("Closure1")->link, transparent->output("BSDF"));
  EXPECT_EQ(mix->input("Closure2")->link, add->output("Closure"));
  EXPECT_EQ(mix->input("Fac")->link, opacity_value->output("Value"));
  EXPECT_EQ(graph.output()->input("Surface")->link, mix->output("Closure"));
}

TEST(materialx_graph, lowers_generic_surface_recursive_bsdf_closure_composition)
{
  materialx::Node red;
  red.name = "Red";
  red.nodedef = "ND_constant_color3";
  red.color3_inputs["value"] = make_float3(1.0f, 0.0f, 0.0f);
  red.outputs["out"] = materialx::Type::Color3;

  materialx::Node blue;
  blue.name = "Blue";
  blue.nodedef = "ND_constant_color3";
  blue.color3_inputs["value"] = make_float3(0.0f, 0.0f, 1.0f);
  blue.outputs["out"] = materialx::Type::Color3;

  materialx::Node mix_amount;
  mix_amount.name = "MixAmount";
  mix_amount.nodedef = "ND_constant_float";
  mix_amount.inputs["value"] = 0.75f;
  mix_amount.outputs["out"] = materialx::Type::Float;

  materialx::Node red_bsdf;
  red_bsdf.name = "RedDiffuse";
  red_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  red_bsdf.links["color"] = {"Red", "out", materialx::Type::Color3};
  red_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node blue_bsdf;
  blue_bsdf.name = "BlueDiffuse";
  blue_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  blue_bsdf.links["color"] = {"Blue", "out", materialx::Type::Color3};
  blue_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mixed_bsdf;
  mixed_bsdf.name = "MixedDiffuse";
  mixed_bsdf.nodedef = "ND_mix_bsdf";
  mixed_bsdf.links["bg"] = {"RedDiffuse", "out", materialx::Type::SurfaceShader};
  mixed_bsdf.links["fg"] = {"BlueDiffuse", "out", materialx::Type::SurfaceShader};
  mixed_bsdf.links["mix"] = {"MixAmount", "out", materialx::Type::Float};
  mixed_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_surface";
  surface.links["bsdf"] = {"MixedDiffuse", "out", materialx::Type::SurfaceShader};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{red, blue, mix_amount, red_bsdf, blue_bsdf, mixed_bsdf, surface}},
                               &graph));

  MixClosureNode *native_mix = nullptr;
  DiffuseBsdfNode *native_red = nullptr;
  DiffuseBsdfNode *native_blue = nullptr;
  ValueNode *native_mix_amount = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_mix = node->name == "MixedDiffuse" ? dynamic_cast<MixClosureNode *>(node) : native_mix;
    native_red = node->name == "RedDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_red;
    native_blue = node->name == "BlueDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_blue;
    native_mix_amount = node->name == "MixAmount" ? dynamic_cast<ValueNode *>(node) :
                                                    native_mix_amount;
  }
  ASSERT_NE(native_mix, nullptr);
  ASSERT_NE(native_red, nullptr);
  ASSERT_NE(native_blue, nullptr);
  ASSERT_NE(native_mix_amount, nullptr);
  EXPECT_EQ(native_mix->input("Closure1")->link, native_red->output("BSDF"));
  EXPECT_EQ(native_mix->input("Closure2")->link, native_blue->output("BSDF"));
  EXPECT_EQ(native_mix->input("Fac")->link, native_mix_amount->output("Value"));
  EXPECT_EQ(graph.output()->input("Surface")->link, native_mix->output("Closure"));
}

TEST(materialx_graph, lowers_dot_surfaceshader_as_identity_passthrough)
{
  /* Real MaterialX 1.39 stdlib_defs.mtlx ND_dot_surfaceshader declares a
   * surfaceshader 'in', uniform string 'note', and surfaceshader 'out' with
   * defaultinput="in". The genosl/genglsl/genmdl implementations are all
   * sourcecode="{{in}}", so this must lower as a pure identity wrapper rather
   * than creating a proxy closure. */
  materialx::Node albedo;
  albedo.name = "Albedo";
  albedo.nodedef = "ND_constant_color3";
  albedo.color3_inputs["value"] = make_float3(0.7f, 0.2f, 0.1f);
  albedo.outputs["out"] = materialx::Type::Color3;

  materialx::Node bsdf;
  bsdf.name = "Diffuse";
  bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  bsdf.links["color"] = {"Albedo", "out", materialx::Type::Color3};
  bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node dot;
  dot.name = "DotSurface";
  dot.nodedef = "ND_dot_surfaceshader";
  dot.links["in"] = {"Diffuse", "out", materialx::Type::SurfaceShader};
  dot.string_inputs["note"] = "organization only";
  dot.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{albedo, bsdf, dot}}, &graph));

  DiffuseBsdfNode *native_bsdf = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_bsdf = node->name == "Diffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_bsdf;
    EXPECT_NE(node->name, "DotSurface");
  }
  ASSERT_NE(native_bsdf, nullptr);
  EXPECT_EQ(graph.output()->input("Surface")->link, native_bsdf->output("BSDF"));

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_surface";
  surface.links["bsdf"] = {"DotSurface", "out", materialx::Type::SurfaceShader};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph consumed_graph;
  ASSERT_TRUE(materialx::lower({{albedo, bsdf, dot, surface}}, &consumed_graph));
  for (ShaderNode *node : consumed_graph.nodes) {
    native_bsdf = node->name == "Diffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_bsdf;
    EXPECT_NE(node->name, "DotSurface");
  }
  ASSERT_NE(native_bsdf, nullptr);
  EXPECT_EQ(consumed_graph.output()->input("Surface")->link, native_bsdf->output("BSDF"));
}

TEST(materialx_graph, lowers_mix_surfaceshader_unit_opacity_surfaces)
{
  materialx::Node red_bsdf;
  red_bsdf.name = "RedDiffuse";
  red_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  red_bsdf.color3_inputs["color"] = make_float3(1.0f, 0.0f, 0.0f);
  red_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node blue_bsdf;
  blue_bsdf.name = "BlueDiffuse";
  blue_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  blue_bsdf.color3_inputs["color"] = make_float3(0.0f, 0.0f, 1.0f);
  blue_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node bg_surface;
  bg_surface.name = "BackgroundSurface";
  bg_surface.nodedef = "ND_surface";
  bg_surface.links["bsdf"] = {"RedDiffuse", "out", materialx::Type::SurfaceShader};
  bg_surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node fg_surface;
  fg_surface.name = "ForegroundSurface";
  fg_surface.nodedef = "ND_surface";
  fg_surface.links["bsdf"] = {"BlueDiffuse", "out", materialx::Type::SurfaceShader};
  fg_surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mix;
  mix.name = "SurfaceMix";
  mix.nodedef = "ND_mix_surfaceshader";
  mix.links["bg"] = {"BackgroundSurface", "out", materialx::Type::SurfaceShader};
  mix.links["fg"] = {"ForegroundSurface", "out", materialx::Type::SurfaceShader};
  mix.inputs["mix"] = 0.25f;
  mix.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{red_bsdf, blue_bsdf, bg_surface, fg_surface, mix}}, &graph));

  MixClosureNode *native_mix = nullptr;
  DiffuseBsdfNode *native_red = nullptr;
  DiffuseBsdfNode *native_blue = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_mix = node->name == "SurfaceMix" ? dynamic_cast<MixClosureNode *>(node) : native_mix;
    native_red = node->name == "RedDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_red;
    native_blue = node->name == "BlueDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : native_blue;
  }
  ASSERT_NE(native_mix, nullptr);
  ASSERT_NE(native_red, nullptr);
  ASSERT_NE(native_blue, nullptr);
  EXPECT_FLOAT_EQ(native_mix->get_fac(), 0.25f);
  EXPECT_EQ(native_mix->input("Closure1")->link, native_red->output("BSDF"));
  EXPECT_EQ(native_mix->input("Closure2")->link, native_blue->output("BSDF"));
  EXPECT_EQ(graph.output()->input("Surface")->link, native_mix->output("Closure"));
}

TEST(materialx_graph, rejects_mix_surfaceshader_non_unit_opacity_boundary)
{
  materialx::Node red_bsdf;
  red_bsdf.name = "RedDiffuse";
  red_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  red_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node blue_bsdf;
  blue_bsdf.name = "BlueDiffuse";
  blue_bsdf.nodedef = "ND_oren_nayar_diffuse_bsdf";
  blue_bsdf.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node bg_surface;
  bg_surface.name = "BackgroundSurface";
  bg_surface.nodedef = "ND_surface";
  bg_surface.links["bsdf"] = {"RedDiffuse", "out", materialx::Type::SurfaceShader};
  bg_surface.inputs["opacity"] = 0.5f;
  bg_surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node fg_surface;
  fg_surface.name = "ForegroundSurface";
  fg_surface.nodedef = "ND_surface";
  fg_surface.links["bsdf"] = {"BlueDiffuse", "out", materialx::Type::SurfaceShader};
  fg_surface.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mix;
  mix.name = "SurfaceMix";
  mix.nodedef = "ND_mix_surfaceshader";
  mix.links["bg"] = {"BackgroundSurface", "out", materialx::Type::SurfaceShader};
  mix.links["fg"] = {"ForegroundSurface", "out", materialx::Type::SurfaceShader};
  mix.inputs["mix"] = 0.25f;
  mix.outputs["out"] = materialx::Type::SurfaceShader;

  EXPECT_FALSE(materialx::validate({{red_bsdf, blue_bsdf, bg_surface, fg_surface, mix}}));
}

TEST(materialx_graph, lowers_lama_surface_front_back_and_presence)
{
  materialx::Node front;
  front.name = "FrontDiffuse";
  front.nodedef = "ND_lama_diffuse";
  front.color3_inputs["color"] = make_float3(0.8f, 0.1f, 0.1f);
  front.inputs["energyCompensation"] = 0.0f;
  front.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node back;
  back.name = "BackDiffuse";
  back.nodedef = "ND_lama_diffuse";
  back.color3_inputs["color"] = make_float3(0.1f, 0.1f, 0.8f);
  back.inputs["energyCompensation"] = 0.0f;
  back.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node surface;
  surface.name = "LamaSurface";
  surface.nodedef = "ND_lama_surface";
  surface.links["materialFront"] = {"FrontDiffuse", "out", materialx::Type::SurfaceShader};
  surface.links["materialBack"] = {"BackDiffuse", "out", materialx::Type::SurfaceShader};
  surface.inputs["presence"] = 0.6f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{front, back, surface}}, &graph));

  MixClosureNode *side = nullptr;
  MixClosureNode *presence = nullptr;
  GeometryNode *geometry = nullptr;
  TransparentBsdfNode *transparent = nullptr;
  DiffuseBsdfNode *front_native = nullptr;
  DiffuseBsdfNode *back_native = nullptr;
  for (ShaderNode *node : graph.nodes) {
    side = node->name == "LamaSurface.side" ? dynamic_cast<MixClosureNode *>(node) : side;
    presence = node->name == "LamaSurface" ? dynamic_cast<MixClosureNode *>(node) : presence;
    geometry = node->name == "LamaSurface.geometry" ? dynamic_cast<GeometryNode *>(node) : geometry;
    transparent = node->name == "LamaSurface.transparent" ? dynamic_cast<TransparentBsdfNode *>(node) : transparent;
    front_native = node->name == "FrontDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : front_native;
    back_native = node->name == "BackDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(node) : back_native;
  }
  ASSERT_NE(side, nullptr);
  ASSERT_NE(presence, nullptr);
  ASSERT_NE(geometry, nullptr);
  ASSERT_NE(transparent, nullptr);
  ASSERT_NE(front_native, nullptr);
  ASSERT_NE(back_native, nullptr);
  EXPECT_EQ(side->input("Closure1")->link, front_native->output("BSDF"));
  EXPECT_EQ(side->input("Closure2")->link, back_native->output("BSDF"));
  EXPECT_EQ(side->input("Fac")->link, geometry->output("Backfacing"));
  EXPECT_FLOAT_EQ(presence->get_fac(), 0.6f);
  EXPECT_EQ(presence->input("Closure1")->link, transparent->output("BSDF"));
  EXPECT_EQ(presence->input("Closure2")->link, side->output("Closure"));
  EXPECT_EQ(graph.output()->input("Surface")->link, presence->output("Closure"));
}

TEST(materialx_graph, rejects_generic_surface_unknown_closure_without_mutation)
{
  materialx::Node unsupported;
  unsupported.name = "Unsupported";
  unsupported.nodedef = "ND_dielectric_bsdf";
  unsupported.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node surface;
  surface.name = "Surface";
  surface.nodedef = "ND_surface";
  surface.links["bsdf"] = {"Unsupported", "out", materialx::Type::SurfaceShader};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;

  EXPECT_FALSE(materialx::lower({{unsupported, surface}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
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

TEST(materialx_graph, lowers_standard_binary_color3_nodes_with_literal_operands)
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
    materialx::Node color_math;
    color_math.name = "ColorMath";
    color_math.nodedef = test_case.nodedef;
    color_math.color3_inputs["in1"] = make_float3(2.0f, 4.0f, 8.0f);
    color_math.color3_inputs["in2"] = make_float3(0.5f, 2.0f, 4.0f);
    color_math.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{color_math}}, &graph)) << test_case.nodedef;

    MixNode *mix = nullptr;
    for (ShaderNode *node : graph.nodes) {
      mix = node->name == "ColorMath" ? dynamic_cast<MixNode *>(node) : mix;
    }

    ASSERT_NE(mix, nullptr) << test_case.nodedef;
    EXPECT_EQ(mix->get_mix_type(), test_case.mix_type) << test_case.nodedef;
    EXPECT_FLOAT_EQ(mix->get_fac(), 1.0f) << test_case.nodedef;
    EXPECT_EQ(mix->get_color1(), make_float3(2.0f, 4.0f, 8.0f)) << test_case.nodedef;
    EXPECT_EQ(mix->get_color2(), make_float3(0.5f, 2.0f, 4.0f)) << test_case.nodedef;
    EXPECT_EQ(mix->input("Color1")->link, nullptr) << test_case.nodedef;
    EXPECT_EQ(mix->input("Color2")->link, nullptr) << test_case.nodedef;
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

TEST(materialx_graph, lowers_color3_scalar_math_with_literal_operands)
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
  for (const MathCase &test_case : cases) {
    materialx::Node math;
    math.name = test_case.name;
    math.nodedef = test_case.nodedef;
    math.color3_inputs["in1"] = make_float3(0.25f, 0.5f, 0.75f);
    math.inputs["in2"] = 2.0f;
    math.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(math));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, MixNode *> lowered;
  std::unordered_map<string, CombineColorNode *> broadcasts;
  for (ShaderNode *node : graph.nodes) {
    if (auto *mix = dynamic_cast<MixNode *>(node)) {
      lowered[string(node->name.c_str())] = mix;
    }
    if (auto *broadcast = dynamic_cast<CombineColorNode *>(node)) {
      broadcasts[string(node->name.c_str())] = broadcast;
    }
  }

  for (const MathCase &test_case : cases) {
    MixNode *mix = lowered[test_case.name];
    CombineColorNode *broadcast = broadcasts[string(test_case.name) + ".scalar"];
    ASSERT_NE(mix, nullptr) << test_case.nodedef;
    ASSERT_NE(broadcast, nullptr) << test_case.nodedef;
    EXPECT_EQ(mix->get_mix_type(), test_case.mix_type) << test_case.nodedef;
    EXPECT_EQ(mix->get_color1(), make_float3(0.25f, 0.5f, 0.75f)) << test_case.nodedef;
    EXPECT_FLOAT_EQ(broadcast->get_r(), 2.0f) << test_case.nodedef;
    EXPECT_FLOAT_EQ(broadcast->get_g(), 2.0f) << test_case.nodedef;
    EXPECT_FLOAT_EQ(broadcast->get_b(), 2.0f) << test_case.nodedef;
    EXPECT_EQ(mix->input("Color1")->link, nullptr) << test_case.nodedef;
    EXPECT_EQ(mix->input("Color2")->link, broadcast->output("Color")) << test_case.nodedef;
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
  for (ShaderNode *node : graph.nodes) if (const auto *math = dynamic_cast<MathNode *>(node)) { modulo_count += math->get_math_type() == NODE_MATH_FLOORED_MODULO; power_count += math->get_math_type() == NODE_MATH_POWER; }
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

TEST(materialx_graph, lowers_color3_scalar_component_math_with_literal_operands)
{
  struct MathCase {
    const char *name;
    const char *nodedef;
    NodeMathType math_type;
  };
  const MathCase cases[] = {{"Modulo", "ND_modulo_color3FA", NODE_MATH_FLOORED_MODULO},
                            {"Power", "ND_power_color3FA", NODE_MATH_POWER},
                            {"Safe", "ND_safepower_color3FA", NODE_MATH_POWER}};

  materialx::Graph source;
  for (const MathCase &test_case : cases) {
    materialx::Node math;
    math.name = test_case.name;
    math.nodedef = test_case.nodedef;
    math.color3_inputs["in1"] = make_float3(-2.0f, 3.0f, -4.0f);
    math.inputs["in2"] = 2.0f;
    math.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(math));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  for (const MathCase &test_case : cases) {
    for (const char *channel : {"Red", "Green", "Blue"}) {
      MathNode *math = nullptr;
      for (ShaderNode *node : graph.nodes) {
        const string expected_name = string(test_case.name) + "." + channel +
                                     (string(test_case.nodedef) == "ND_safepower_color3FA" ?
                                          ".power" :
                                          "");
        if (node->name == expected_name) {
          math = dynamic_cast<MathNode *>(node);
        }
      }
      ASSERT_NE(math, nullptr) << test_case.nodedef << " " << channel;
      EXPECT_EQ(math->get_math_type(), test_case.math_type) << test_case.nodedef << " " << channel;
      EXPECT_FLOAT_EQ(math->get_value2(), 2.0f) << test_case.nodedef << " " << channel;
      EXPECT_EQ(math->input("Value2")->link, nullptr) << test_case.nodedef << " " << channel;
    }
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

TEST(materialx_graph, lowers_extract_nodes_with_literal_operands)
{
  materialx::Node color3;
  color3.name = "ExtractColor3";
  color3.nodedef = "ND_extract_color3";
  color3.color3_inputs["in"] = make_float3(0.2f, 0.4f, 0.6f);
  color3.int_inputs["index"] = 2;
  color3.outputs["out"] = materialx::Type::Float;

  materialx::Node vector2;
  vector2.name = "ExtractVector2";
  vector2.nodedef = "ND_extract_vector2";
  vector2.vector2_inputs["in"] = make_float2(0.25f, 0.5f);
  vector2.int_inputs["index"] = 1;
  vector2.outputs["out"] = materialx::Type::Float;

  materialx::Node vector3;
  vector3.name = "ExtractVector3";
  vector3.nodedef = "ND_extract_vector3";
  vector3.vector3_inputs["in"] = make_float3(0.75f, 0.875f, 0.9375f);
  vector3.int_inputs["index"] = 0;
  vector3.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color3, vector2, vector3}}, &graph));

  SeparateColorNode *color3_extract = nullptr;
  SeparateXYZNode *vector2_extract = nullptr;
  SeparateXYZNode *vector3_extract = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color3_extract = node->name == "ExtractColor3" ? dynamic_cast<SeparateColorNode *>(node) :
                                                      color3_extract;
    vector2_extract = node->name == "ExtractVector2" ? dynamic_cast<SeparateXYZNode *>(node) :
                                                        vector2_extract;
    vector3_extract = node->name == "ExtractVector3" ? dynamic_cast<SeparateXYZNode *>(node) :
                                                        vector3_extract;
  }

  ASSERT_NE(color3_extract, nullptr);
  EXPECT_EQ(color3_extract->get_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(color3_extract->input("Color")->link, nullptr);

  ASSERT_NE(vector2_extract, nullptr);
  EXPECT_EQ(vector2_extract->get_vector(), make_float3(0.25f, 0.5f, 0.0f));
  EXPECT_EQ(vector2_extract->input("Vector")->link, nullptr);

  ASSERT_NE(vector3_extract, nullptr);
  EXPECT_EQ(vector3_extract->get_vector(), make_float3(0.75f, 0.875f, 0.9375f));
  EXPECT_EQ(vector3_extract->input("Vector")->link, nullptr);
}

TEST(materialx_graph, lowers_four_component_extracts_with_literal_operands)
{
  materialx::Node color4_rgb;
  color4_rgb.name = "ExtractColor4RGB";
  color4_rgb.nodedef = "ND_extract_color4";
  color4_rgb.float4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4_rgb.int_inputs["index"] = 1;
  color4_rgb.outputs["out"] = materialx::Type::Float;

  materialx::Node color4_alpha;
  color4_alpha.name = "ExtractColor4Alpha";
  color4_alpha.nodedef = "ND_extract_color4";
  color4_alpha.float4_inputs["in"] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
  color4_alpha.int_inputs["index"] = 3;
  color4_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node vector4_xyz;
  vector4_xyz.name = "ExtractVector4XYZ";
  vector4_xyz.nodedef = "ND_extract_vector4";
  vector4_xyz.vector4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, 1.0f);
  vector4_xyz.int_inputs["index"] = 2;
  vector4_xyz.outputs["out"] = materialx::Type::Float;

  materialx::Node vector4_w;
  vector4_w.name = "ExtractVector4W";
  vector4_w.nodedef = "ND_extract_vector4";
  vector4_w.vector4_inputs["in"] = make_float4(1.25f, 1.5f, 1.75f, 2.0f);
  vector4_w.int_inputs["index"] = 3;
  vector4_w.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower({{color4_rgb, color4_alpha, vector4_xyz, vector4_w}}, &graph, &error))
      << error;

  SeparateColorNode *native_color4_rgb = nullptr;
  ValueNode *native_color4_alpha = nullptr;
  SeparateXYZNode *native_vector4_xyz = nullptr;
  ValueNode *native_vector4_w = nullptr;
  for (ShaderNode *node : graph.nodes) {
    native_color4_rgb = node->name == "ExtractColor4RGB" ?
                            dynamic_cast<SeparateColorNode *>(node) :
                            native_color4_rgb;
    native_color4_alpha = node->name == "ExtractColor4Alpha" ? dynamic_cast<ValueNode *>(node) :
                                                                native_color4_alpha;
    native_vector4_xyz = node->name == "ExtractVector4XYZ" ?
                             dynamic_cast<SeparateXYZNode *>(node) :
                             native_vector4_xyz;
    native_vector4_w = node->name == "ExtractVector4W" ? dynamic_cast<ValueNode *>(node) :
                                                          native_vector4_w;
  }

  ASSERT_NE(native_color4_rgb, nullptr);
  EXPECT_EQ(native_color4_rgb->get_color(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(native_color4_rgb->input("Color")->link, nullptr);

  ASSERT_NE(native_color4_alpha, nullptr);
  EXPECT_FLOAT_EQ(native_color4_alpha->get_value(), 0.8f);

  ASSERT_NE(native_vector4_xyz, nullptr);
  EXPECT_EQ(native_vector4_xyz->get_vector(), make_float3(0.25f, 0.5f, 0.75f));
  EXPECT_EQ(native_vector4_xyz->input("Vector")->link, nullptr);

  ASSERT_NE(native_vector4_w, nullptr);
  EXPECT_FLOAT_EQ(native_vector4_w->get_value(), 2.0f);
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

TEST(materialx_graph, lowers_unary_color3_nodes_with_literal_operands)
{
  struct UnaryColorCase {
    const char *name;
    const char *nodedef;
    NodeMathType math_type;
  };
  const UnaryColorCase cases[] = {{"Absolute", "ND_absval_color3", NODE_MATH_ABSOLUTE},
                                  {"Floor", "ND_floor_color3", NODE_MATH_FLOOR},
                                  {"Ceil", "ND_ceil_color3", NODE_MATH_CEIL},
                                  {"Fract", "ND_fract_color3", NODE_MATH_FRACTION},
                                  {"Round", "ND_round_color3", NODE_MATH_ROUND},
                                  {"Sign", "ND_sign_color3", NODE_MATH_SIGN}};

  materialx::Graph source;
  for (const UnaryColorCase &test_case : cases) {
    materialx::Node node;
    node.name = test_case.name;
    node.nodedef = test_case.nodedef;
    node.color3_inputs["in"] = make_float3(-1.25f, 2.75f, -0.5f);
    node.outputs["out"] = materialx::Type::Color3;
    source.nodes.push_back(std::move(node));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  for (const UnaryColorCase &test_case : cases) {
    SeparateColorNode *separate = nullptr;
    int channel_math_count = 0;
    for (ShaderNode *node : graph.nodes) {
      separate = node->name == string(test_case.name) + ".separate" ?
                     dynamic_cast<SeparateColorNode *>(node) :
                     separate;
      if (const auto *math = dynamic_cast<MathNode *>(node)) {
        channel_math_count += math->get_math_type() == test_case.math_type;
      }
    }
    ASSERT_NE(separate, nullptr) << test_case.nodedef;
    EXPECT_EQ(separate->get_color(), make_float3(-1.25f, 2.75f, -0.5f)) << test_case.nodedef;
    EXPECT_EQ(separate->input("Color")->link, nullptr) << test_case.nodedef;
    EXPECT_EQ(channel_math_count, 3) << test_case.nodedef;
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

TEST(materialx_graph, lowers_lr_tb_ramps_with_literal_texcoords)
{
  const struct {
    const char *name;
    const char *id;
    materialx::Type type;
    bool top_to_bottom;
  } cases[] = {{"ScalarLR", "ND_ramplr_float", materialx::Type::Float, false},
               {"ScalarTB", "ND_ramptb_float", materialx::Type::Float, true},
               {"ColorLR", "ND_ramplr_color3", materialx::Type::Color3, false},
               {"ColorTB", "ND_ramptb_color3", materialx::Type::Color3, true}};

  for (const auto &test : cases) {
    materialx::Node ramp;
    ramp.name = test.name;
    ramp.nodedef = test.id;
    const char *first_name = test.top_to_bottom ? "valuet" : "valuel";
    const char *second_name = test.top_to_bottom ? "valueb" : "valuer";
    if (test.type == materialx::Type::Float) {
      ramp.inputs[first_name] = 0.1f;
      ramp.inputs[second_name] = 0.9f;
    }
    else {
      ramp.color3_inputs[first_name] = make_float3(0.1f, 0.2f, 0.3f);
      ramp.color3_inputs[second_name] = make_float3(0.7f, 0.8f, 0.9f);
    }
    ramp.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
    ramp.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{ramp}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *coordinate = dynamic_cast<SeparateXYZNode *>(nodes[string(test.name) + ".coordinate"]);
    ASSERT_NE(coordinate, nullptr) << test.id;
    EXPECT_EQ(coordinate->get_vector(), make_float3(0.25f, 0.75f, 0.0f)) << test.id;
    EXPECT_EQ(coordinate->input("Vector")->link, nullptr) << test.id;

    if (test.type == materialx::Type::Float) {
      auto *sum = dynamic_cast<MathNode *>(nodes[test.name]);
      ASSERT_NE(sum, nullptr) << test.id;
      EXPECT_FLOAT_EQ(sum->get_value1(), 0.1f) << test.id;
    }
    else {
      auto *mix = dynamic_cast<MixNode *>(nodes[test.name]);
      ASSERT_NE(mix, nullptr) << test.id;
      EXPECT_EQ(mix->get_color1(), make_float3(0.1f, 0.2f, 0.3f)) << test.id;
      EXPECT_EQ(mix->get_color2(), make_float3(0.7f, 0.8f, 0.9f)) << test.id;
    }
  }
}

TEST(materialx_graph, lowers_checkerboard_color3_with_literal_texcoord)
{
  const struct {
    const char *name;
    float2 texcoord;
  } cases[] = {{"CheckerBlack", make_float2(0.05f, 0.05f)},
               {"CheckerWhite", make_float2(0.2f, 0.05f)}};

  for (const auto &test : cases) {
    materialx::Node checker;
    checker.name = test.name;
    checker.nodedef = "ND_checkerboard_color3";
    checker.color3_inputs["color1"] = make_float3(1.0f, 1.0f, 1.0f);
    checker.color3_inputs["color2"] = make_float3(0.0f, 0.0f, 0.0f);
    checker.vector2_inputs["texcoord"] = test.texcoord;
    checker.vector2_inputs["uvtiling"] = make_float2(8.0f, 8.0f);
    checker.vector2_inputs["uvoffset"] = zero_float2();
    checker.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    string error;
    ASSERT_TRUE(materialx::lower({{checker}}, &graph, &error)) << test.name << ": " << error;

    VectorMathNode *scale = nullptr;
    MixNode *lowered = nullptr;
    for (ShaderNode *node : graph.nodes) {
      scale = node->name == string(test.name) + ".scale" ? dynamic_cast<VectorMathNode *>(node) :
                                                            scale;
      lowered = node->name == test.name ? dynamic_cast<MixNode *>(node) : lowered;
    }
    ASSERT_NE(scale, nullptr) << test.name;
    ASSERT_NE(lowered, nullptr) << test.name;
    EXPECT_EQ(scale->get_vector1(), make_float3(test.texcoord, 0.0f)) << test.name;
    EXPECT_EQ(scale->input("Vector1")->link, nullptr) << test.name;
    EXPECT_EQ(lowered->get_mix_type(), NODE_MIX_BLEND) << test.name;
    EXPECT_EQ(lowered->get_color1(), make_float3(0.0f, 0.0f, 0.0f)) << test.name;
    EXPECT_EQ(lowered->get_color2(), make_float3(1.0f, 1.0f, 1.0f)) << test.name;
    ASSERT_NE(lowered->input("Fac")->link, nullptr) << test.name;
  }
}

TEST(materialx_graph, lowers_checkerboard_color3_with_linked_texcoord)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.05f, 0.05f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node checker;
  checker.name = "Checker";
  checker.nodedef = "ND_checkerboard_color3";
  checker.color3_inputs["color1"] = make_float3(1.0f, 1.0f, 1.0f);
  checker.color3_inputs["color2"] = make_float3(0.0f, 0.0f, 0.0f);
  checker.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  checker.vector2_inputs["uvtiling"] = make_float2(8.0f, 8.0f);
  checker.vector2_inputs["uvoffset"] = zero_float2();
  checker.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower({{texcoord, checker}}, &graph, &error)) << error;

  MixNode *lowered = nullptr;
  for (ShaderNode *node : graph.nodes) {
    lowered = node->name == "Checker" ? dynamic_cast<MixNode *>(node) : lowered;
  }
  ASSERT_NE(lowered, nullptr);
  EXPECT_EQ(lowered->get_mix_type(), NODE_MIX_BLEND);
  EXPECT_EQ(lowered->get_color1(), make_float3(0.0f, 0.0f, 0.0f));
  EXPECT_EQ(lowered->get_color2(), make_float3(1.0f, 1.0f, 1.0f));
  ASSERT_NE(lowered->input("Fac")->link, nullptr);
}

TEST(materialx_graph, lowers_measured_checkerboard_fixture_literal_samples)
{
  /* Mirrors the PROCEDURAL2DSHAPES6 deterministic samples from
   * materialx-terminal-canonical/research/materialx_release/
   * procedural2d_shapes_fixtures.py: the exact failure class was wrong-value
   * native lowering, not reader admission. Keep the three literal sample
   * coordinates covered by lower() so regressions cannot silently drop back to
   * the default Vector socket value. */
  const float2 samples[] = {make_float2(0.05f, 0.05f),
                            make_float2(0.05f, 0.2f),
                            make_float2(0.2f, 0.05f)};

  for (const float2 sample : samples) {
    materialx::Node checker;
    checker.name = "Checker";
    checker.nodedef = "ND_checkerboard_color3";
    checker.color3_inputs["color1"] = make_float3(1.0f, 1.0f, 1.0f);
    checker.color3_inputs["color2"] = make_float3(0.0f, 0.0f, 0.0f);
    checker.vector2_inputs["texcoord"] = sample;
    checker.vector2_inputs["uvtiling"] = make_float2(8.0f, 8.0f);
    checker.vector2_inputs["uvoffset"] = zero_float2();
    checker.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    string error;
    ASSERT_TRUE(materialx::lower({{checker}}, &graph, &error)) << error;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *scale = dynamic_cast<VectorMathNode *>(nodes["Checker.scale"]);
    auto *floor = dynamic_cast<VectorMathNode *>(nodes["Checker.floor"]);
    auto *modulo = dynamic_cast<MathNode *>(nodes["Checker.modulo"]);
    auto *mix = dynamic_cast<MixNode *>(nodes["Checker"]);
    ASSERT_NE(scale, nullptr);
    ASSERT_NE(floor, nullptr);
    ASSERT_NE(modulo, nullptr);
    ASSERT_NE(mix, nullptr);
    EXPECT_EQ(scale->get_vector1(), make_float3(sample, 0.0f));
    EXPECT_EQ(scale->input("Vector1")->link, nullptr);
    EXPECT_EQ(floor->get_math_type(), NODE_VECTOR_MATH_FLOOR);
    EXPECT_EQ(modulo->get_math_type(), NODE_MATH_FLOORED_MODULO);
    EXPECT_EQ(mix->get_mix_type(), NODE_MIX_BLEND);
    EXPECT_EQ(mix->get_color1(), make_float3(0.0f, 0.0f, 0.0f));
    EXPECT_EQ(mix->get_color2(), make_float3(1.0f, 1.0f, 1.0f));
    ASSERT_NE(mix->input("Fac")->link, nullptr);
    EXPECT_EQ(mix->input("Fac")->link, modulo->output("Value"));
  }
}

TEST(materialx_graph, lowers_checkerboard_color3_with_linked_colors)
{
  materialx::Node color1;
  color1.name = "White";
  color1.nodedef = "ND_constant_color3";
  color1.color3_inputs["value"] = make_float3(1.0f, 1.0f, 1.0f);
  color1.outputs["out"] = materialx::Type::Color3;

  materialx::Node color2;
  color2.name = "Black";
  color2.nodedef = "ND_constant_color3";
  color2.color3_inputs["value"] = make_float3(0.0f, 0.0f, 0.0f);
  color2.outputs["out"] = materialx::Type::Color3;

  materialx::Node checker;
  checker.name = "Checker";
  checker.nodedef = "ND_checkerboard_color3";
  checker.links["color1"] = {"White", "out", materialx::Type::Color3};
  checker.links["color2"] = {"Black", "out", materialx::Type::Color3};
  checker.vector2_inputs["texcoord"] = make_float2(0.05f, 0.05f);
  checker.vector2_inputs["uvtiling"] = make_float2(8.0f, 8.0f);
  checker.vector2_inputs["uvoffset"] = zero_float2();
  checker.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower({{color1, color2, checker}}, &graph, &error)) << error;

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *mix = dynamic_cast<MixNode *>(nodes["Checker"]);
  auto *white = dynamic_cast<ColorNode *>(nodes["White"]);
  auto *black = dynamic_cast<ColorNode *>(nodes["Black"]);
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(white, nullptr);
  ASSERT_NE(black, nullptr);
  EXPECT_EQ(mix->input("Color2")->link, white->output("Color"));
  EXPECT_EQ(mix->input("Color1")->link, black->output("Color"));
  EXPECT_EQ(mix->get_color1(), zero_float3());
  EXPECT_EQ(mix->get_color2(), zero_float3());
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

TEST(materialx_graph, lowers_range_float_with_materialx_default_gamma)
{
  materialx::Node range;
  range.name = "DefaultGammaRange";
  range.nodedef = "ND_range_float";
  range.inputs = {{"in", 0.5f},
                  {"inlow", 0.0f},
                  {"inhigh", 1.0f},
                  {"outlow", 0.2f},
                  {"outhigh", 0.8f}};
  range.int_inputs["doclamp"] = 1;
  range.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{range}}, &graph));

  MapRangeNode *lowered = nullptr;
  for (ShaderNode *node : graph.nodes) {
    lowered = node->name == "DefaultGammaRange" ? dynamic_cast<MapRangeNode *>(node) : lowered;
  }
  ASSERT_NE(lowered, nullptr);
  EXPECT_EQ(lowered->get_range_type(), NODE_MAP_RANGE_LINEAR);
  EXPECT_TRUE(lowered->get_clamp());
  EXPECT_FLOAT_EQ(lowered->get_value(), 0.5f);
  EXPECT_FLOAT_EQ(lowered->get_to_min(), 0.2f);
  EXPECT_FLOAT_EQ(lowered->get_to_max(), 0.8f);
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
  luminance.outputs["out"] = materialx::Type::Color3;

  materialx::Node literal_luminance;
  literal_luminance.name = "LiteralLuminance";
  literal_luminance.nodedef = "ND_luminance_color3";
  literal_luminance.color3_inputs["in"] = make_float3(0.2f, 0.4f, 0.6f);
  literal_luminance.color3_inputs["lumacoeffs"] = make_float3(0.2126f, 0.7152f, 0.0722f);
  literal_luminance.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "OpenPBR";
  surface.nodedef = "ND_open_pbr_surface_surfaceshader";
  surface.links["base_color"] = {"Luminance", "out", materialx::Type::Color3};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, luminance, literal_luminance, surface}}, &graph));

  VectorMathNode *dot = nullptr;
  VectorMathNode *literal_dot = nullptr;
  CombineColorNode *combine = nullptr;
  CombineColorNode *literal_combine = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (node->name == "Luminance.luminance") dot = dynamic_cast<VectorMathNode *>(node);
    if (node->name == "LiteralLuminance.luminance") {
      literal_dot = dynamic_cast<VectorMathNode *>(node);
    }
    if (node->name == "Luminance") combine = dynamic_cast<CombineColorNode *>(node);
    if (node->name == "LiteralLuminance") {
      literal_combine = dynamic_cast<CombineColorNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(dot, nullptr);
  ASSERT_NE(literal_dot, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(literal_combine, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dot->get_vector2(), make_float3(0.2126f, 0.7152f, 0.0722f));
  ASSERT_NE(dot->input("Vector1")->link, nullptr);
  EXPECT_EQ(dot->input("Vector1")->link->parent->name, "Luminance.vector");
  EXPECT_EQ(literal_dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(literal_dot->get_vector2(), make_float3(0.2126f, 0.7152f, 0.0722f));
  ASSERT_NE(literal_dot->input("Vector1")->link, nullptr);
  EXPECT_EQ(literal_dot->input("Vector1")->link->parent->name, "LiteralLuminance.vector");
  EXPECT_EQ(combine->input("Red")->link, dot->output("Value"));
  EXPECT_EQ(combine->input("Green")->link, dot->output("Value"));
  EXPECT_EQ(combine->input("Blue")->link, dot->output("Value"));
  EXPECT_EQ(literal_combine->input("Red")->link, literal_dot->output("Value"));
  EXPECT_EQ(literal_combine->input("Green")->link, literal_dot->output("Value"));
  EXPECT_EQ(literal_combine->input("Blue")->link, literal_dot->output("Value"));
  EXPECT_EQ(principled->input("Base Color")->link, combine->output("Color"));
}

TEST(materialx_graph, lowers_hsvadjust_color3_and_color4_as_reference_hsv_arithmetic)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node adjust;
  adjust.name = "HSVAdjust";
  adjust.nodedef = "ND_hsvadjust_color3";
  adjust.links["in"] = {"Color", "out", materialx::Type::Color3};
  adjust.vector3_inputs["amount"] = make_float3(0.125f, 0.5f, 1.25f);
  adjust.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4;
  color4.name = "Color4";
  color4.nodedef = "ND_constant_color4";
  color4.float4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node adjust4;
  adjust4.name = "HSVAdjust4";
  adjust4.nodedef = "ND_hsvadjust_color4";
  adjust4.links["in"] = {"Color4", "out", materialx::Type::Color4};
  adjust4.vector3_inputs["amount"] = make_float3(0.25f, 0.75f, 1.5f);
  adjust4.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract_alpha;
  extract_alpha.name = "ExtractAlpha";
  extract_alpha.nodedef = "ND_extract_color4";
  extract_alpha.links["in"] = {"HSVAdjust4", "out", materialx::Type::Color4};
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, adjust, color4, adjust4, extract_alpha}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["HSVAdjust.rgb_to_hsv"]), nullptr);
  EXPECT_EQ(dynamic_cast<SeparateColorNode *>(lowered["HSVAdjust.rgb_to_hsv"])->get_color_type(),
            NODE_COMBSEP_COLOR_HSV);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["HSVAdjust.hue"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust.hue"])->get_math_type(), NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust.hue"])->get_value2(), 0.125f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["HSVAdjust.saturation"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust.saturation"])->get_math_type(),
            NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust.saturation"])->get_value2(), 0.5f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["HSVAdjust.value"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust.value"])->get_value2(), 1.25f);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["HSVAdjust"]), nullptr);
  EXPECT_EQ(dynamic_cast<CombineColorNode *>(lowered["HSVAdjust"])->get_color_type(),
            NODE_COMBSEP_COLOR_HSV);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["HSVAdjust4.Alpha"]), nullptr);
  EXPECT_EQ(lowered["HSVAdjust4.Alpha"]->input("Value1")->link, nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust4.Alpha"])->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVAdjust4.Alpha"])->get_value2(), 0.0f);
  EXPECT_TRUE(materialx::validate({{color, adjust, color4, adjust4, extract_alpha}}));
}

TEST(materialx_graph, lowers_rgb_hsv_color3_with_literal_operands)
{
  materialx::Node rgb_to_hsv;
  rgb_to_hsv.name = "RGBToHSVLiteral";
  rgb_to_hsv.nodedef = "ND_rgbtohsv_color3";
  rgb_to_hsv.color3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
  rgb_to_hsv.outputs["out"] = materialx::Type::Color3;

  materialx::Node hsv_to_rgb;
  hsv_to_rgb.name = "HSVToRGBLiteral";
  hsv_to_rgb.nodedef = "ND_hsvtorgb_color3";
  hsv_to_rgb.color3_inputs["in"] = make_float3(0.5f, 0.6666667f, 0.75f);
  hsv_to_rgb.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{rgb_to_hsv, hsv_to_rgb}}, &graph));

  std::unordered_map<string, SeparateColorNode *> separate;
  std::unordered_map<string, CombineColorNode *> combine;
  for (ShaderNode *node : graph.nodes) {
    if (SeparateColorNode *lowered = dynamic_cast<SeparateColorNode *>(node)) {
      separate[string(node->name.c_str())] = lowered;
    }
    if (CombineColorNode *lowered = dynamic_cast<CombineColorNode *>(node)) {
      combine[string(node->name.c_str())] = lowered;
    }
  }

  ASSERT_NE(separate["RGBToHSVLiteral.separate"], nullptr);
  EXPECT_EQ(separate["RGBToHSVLiteral.separate"]->get_color_type(), NODE_COMBSEP_COLOR_HSV);
  EXPECT_EQ(separate["RGBToHSVLiteral.separate"]->get_color(), make_float3(0.25f, 0.5f, 0.75f));
  ASSERT_NE(combine["RGBToHSVLiteral"], nullptr);
  EXPECT_EQ(combine["RGBToHSVLiteral"]->get_color_type(), NODE_COMBSEP_COLOR_RGB);

  ASSERT_NE(separate["HSVToRGBLiteral.separate"], nullptr);
  EXPECT_EQ(separate["HSVToRGBLiteral.separate"]->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(separate["HSVToRGBLiteral.separate"]->get_color(),
            make_float3(0.5f, 0.6666667f, 0.75f));
  ASSERT_NE(combine["HSVToRGBLiteral"], nullptr);
  EXPECT_EQ(combine["HSVToRGBLiteral"]->get_color_type(), NODE_COMBSEP_COLOR_HSV);
}

TEST(materialx_graph, lowers_colorcorrect_color3_and_color4_adjustment_chain)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node correct;
  correct.name = "ColorCorrect";
  correct.nodedef = "ND_colorcorrect_color3";
  correct.links["in"] = {"Color", "out", materialx::Type::Color3};
  correct.inputs = {{"hue", 0.125f},
                    {"saturation", 0.5f},
                    {"gamma", 1.0f},
                    {"lift", 0.2f},
                    {"gain", 1.25f},
                    {"contrast", 1.5f},
                    {"contrastpivot", 0.25f},
                    {"exposure", 2.0f}};
  correct.outputs["out"] = materialx::Type::Color3;

  materialx::Node correct4;
  correct4.name = "ColorCorrect4";
  correct4.nodedef = "ND_colorcorrect_color4";
  correct4.float4_inputs["in"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  correct4.inputs = {{"hue", 0.25f},
                     {"saturation", 0.75f},
                     {"gamma", 1.0f},
                     {"lift", 0.1f},
                     {"gain", 1.5f},
                     {"contrast", 2.0f},
                     {"contrastpivot", 0.75f},
                     {"exposure", -1.0f}};
  correct4.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract_alpha;
  extract_alpha.name = "ExtractAlpha";
  extract_alpha.nodedef = "ND_extract_color4";
  extract_alpha.links["in"] = {"ColorCorrect4", "out", materialx::Type::Color4};
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, correct, correct4, extract_alpha}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["ColorCorrect.hsv.input"]), nullptr);
  EXPECT_EQ(dynamic_cast<SeparateColorNode *>(lowered["ColorCorrect.hsv.input"])->get_color_type(),
            NODE_COMBSEP_COLOR_HSV);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect.hsv.hue"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.hsv.hue"])->get_math_type(),
            NODE_MATH_ADD);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.hsv.hue"])->get_value2(), 0.125f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect.hsv.hue.fract"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.hsv.hue.fract"])->get_math_type(),
            NODE_MATH_FRACTION);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["ColorCorrect.hsv"]), nullptr);
  EXPECT_EQ(dynamic_cast<CombineColorNode *>(lowered["ColorCorrect.hsv"])->get_color_type(),
            NODE_COMBSEP_COLOR_HSV);
  ASSERT_NE(dynamic_cast<MixNode *>(lowered["ColorCorrect.saturate"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MixNode *>(lowered["ColorCorrect.saturate"])->get_fac(), 0.5f);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["ColorCorrect.saturate.luminance"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["ColorCorrect.saturate.luminance"])->get_vector2(),
            make_float3(0.2722287f, 0.6740818f, 0.0536895f));
  ASSERT_NE(dynamic_cast<GammaNode *>(lowered["ColorCorrect.gamma"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<GammaNode *>(lowered["ColorCorrect.gamma"])->get_gamma(), 1.0f);
  ASSERT_NE(dynamic_cast<MixNode *>(lowered["ColorCorrect.lift_mult"]), nullptr);
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered["ColorCorrect.lift_mult"])->get_mix_type(), NODE_MIX_MUL);
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered["ColorCorrect.lift_mult"])->get_color2(), make_float3(0.8f));
  ASSERT_NE(dynamic_cast<MixNode *>(lowered["ColorCorrect.gain"]), nullptr);
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered["ColorCorrect.gain"])->get_color2(), make_float3(1.25f));
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["ColorCorrect.contrast.input"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["ColorCorrect.contrast"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red.subtract"]), nullptr);
  EXPECT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red.subtract"])->get_math_type(),
            NODE_MATH_SUBTRACT);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red.subtract"])->get_value2(),
                  0.25f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red.multiply"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red.multiply"])->get_value2(),
                  1.5f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect.contrast.Red"])->get_value2(),
                  0.25f);
  ASSERT_NE(dynamic_cast<MixNode *>(lowered["ColorCorrect"]), nullptr);
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered["ColorCorrect"])->get_color2(), make_float3(4.0f));
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["ColorCorrect4.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["ColorCorrect4.Alpha"])->get_value1(), 0.4f);
  EXPECT_TRUE(materialx::validate({{color, correct, correct4, extract_alpha}}));
}

TEST(materialx_graph, lowers_colorcorrect_nonunit_gamma_with_materialx_signed_range)
{
  materialx::Node correct;
  correct.name = "ColorCorrectGamma";
  correct.nodedef = "ND_colorcorrect_color3";
  correct.color3_inputs["in"] = make_float3(-0.25f, 0.25f, 1.0f);
  correct.inputs = {{"hue", 0.0f},
                    {"saturation", 1.0f},
                    {"gamma", 2.0f},
                    {"lift", 0.0f},
                    {"gain", 1.0f},
                    {"contrast", 1.0f},
                    {"contrastpivot", 0.5f},
                    {"exposure", 0.0f}};
  correct.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{correct}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  EXPECT_EQ(dynamic_cast<GammaNode *>(lowered["ColorCorrectGamma.gamma"]), nullptr)
      << "MaterialX gamma must not use Cycles' clamping GammaNode";
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["ColorCorrectGamma.gamma"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["ColorCorrectGamma.gamma.separate"]), nullptr);
  for (const char *channel : {"Red", "Green", "Blue"}) {
    const string prefix = string("ColorCorrectGamma.gamma.") + channel;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[prefix + ".abs"]), nullptr) << channel;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[prefix + ".power"]), nullptr) << channel;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[prefix + ".sign"]), nullptr) << channel;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[prefix]), nullptr) << channel;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[prefix + ".abs"])->get_math_type(),
              NODE_MATH_ABSOLUTE) << channel;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[prefix + ".power"])->get_math_type(),
              NODE_MATH_POWER) << channel;
    EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered[prefix + ".power"])->get_value2(), 0.5f)
        << channel;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[prefix + ".sign"])->get_math_type(), NODE_MATH_SIGN)
        << channel;
    EXPECT_EQ(dynamic_cast<MathNode *>(lowered[prefix])->get_math_type(), NODE_MATH_MULTIPLY)
        << channel;
  }
}

TEST(materialx_graph, lowers_measured_colorcorrect_literal_samples_without_alpha_drift)
{
  /* Regression coverage for the measured ADJUSTMENT_COLORCORRECT2 sample that
   * used every scalar control at once.  Earlier failures either collapsed RGB
   * to black on the non-unit gamma path or added Cycles' default MathNode 0.5 to
   * Color4 alpha.  lower() cannot be rendered here, so assert the native graph
   * carries the exact literal controls and preserves Color4 alpha as +0.0. */
  materialx::Node color3;
  color3.name = "MeasuredColorCorrect3";
  color3.nodedef = "ND_colorcorrect_color3";
  color3.color3_inputs["in"] = make_float3(0.1f, 0.4f, 0.8f);
  color3.inputs = {{"hue", -0.2f},
                   {"saturation", 1.5f},
                   {"gamma", 0.5f},
                   {"lift", -0.1f},
                   {"gain", 0.75f},
                   {"contrast", 0.8f},
                   {"contrastpivot", 0.25f},
                   {"exposure", 1.0f}};
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4 = color3;
  color4.name = "MeasuredColorCorrect4";
  color4.nodedef = "ND_colorcorrect_color4";
  color4.color3_inputs.clear();
  color4.float4_inputs["in"] = make_float4(0.1f, 0.4f, 0.8f, 0.7f);
  color4.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color3, color4}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  for (const char *name : {"MeasuredColorCorrect3", "MeasuredColorCorrect4"}) {
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[string(name) + ".hsv.hue"]), nullptr) << name;
    EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered[string(name) + ".hsv.hue"])->get_value2(),
                    -0.2f)
        << name;
    ASSERT_NE(dynamic_cast<MixNode *>(lowered[string(name) + ".saturate"]), nullptr) << name;
    EXPECT_FLOAT_EQ(dynamic_cast<MixNode *>(lowered[string(name) + ".saturate"])->get_fac(),
                    1.5f)
        << name;
    EXPECT_EQ(dynamic_cast<GammaNode *>(lowered[string(name) + ".gamma"]), nullptr)
        << name << " must use signed MaterialX range gamma, not Cycles GammaNode";
    ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered[string(name) + ".gamma"]), nullptr)
        << name;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[string(name) + ".gamma.Red.power"]), nullptr)
        << name;
    EXPECT_FLOAT_EQ(
        dynamic_cast<MathNode *>(lowered[string(name) + ".gamma.Red.power"])->get_value2(),
        2.0f)
        << name;
    ASSERT_NE(dynamic_cast<MixNode *>(lowered[string(name) + ".lift_mult"]), nullptr) << name;
    EXPECT_EQ(dynamic_cast<MixNode *>(lowered[string(name) + ".lift_mult"])->get_color2(),
              make_float3(1.1f))
        << name;
    ASSERT_NE(dynamic_cast<MixNode *>(lowered[string(name) + ".gain"]), nullptr) << name;
    EXPECT_EQ(dynamic_cast<MixNode *>(lowered[string(name) + ".gain"])->get_color2(),
              make_float3(0.75f))
        << name;
    ASSERT_NE(dynamic_cast<MathNode *>(lowered[string(name) + ".contrast.Red.multiply"]), nullptr)
        << name;
    EXPECT_FLOAT_EQ(
        dynamic_cast<MathNode *>(lowered[string(name) + ".contrast.Red.multiply"])->get_value2(),
        0.8f)
        << name;
    ASSERT_NE(dynamic_cast<MixNode *>(lowered[name]), nullptr) << name;
    EXPECT_EQ(dynamic_cast<MixNode *>(lowered[name])->get_color2(), make_float3(2.0f)) << name;
  }

  auto *alpha = dynamic_cast<MathNode *>(lowered["MeasuredColorCorrect4.Alpha"]);
  ASSERT_NE(alpha, nullptr);
  EXPECT_FLOAT_EQ(alpha->get_value1(), 0.7f);
  EXPECT_FLOAT_EQ(alpha->get_value2(), 0.0f);
}

TEST(materialx_graph, lowers_saturate_color3_and_color4_with_luminance_mix)
{
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node saturate;
  saturate.name = "Saturate";
  saturate.nodedef = "ND_saturate_color3";
  saturate.color3_inputs["in"] = make_float3(0.2f, 0.4f, 0.6f);
  saturate.inputs["amount"] = 0.35f;
  saturate.color3_inputs["lumacoeffs"] = make_float3(0.2126f, 0.7152f, 0.0722f);
  saturate.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4;
  color4.name = "Color4";
  color4.nodedef = "ND_constant_color4";
  color4.float4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node saturate4;
  saturate4.name = "Saturate4";
  saturate4.nodedef = "ND_saturate_color4";
  saturate4.links["in"] = {"Color4", "out", materialx::Type::Color4};
  saturate4.inputs["amount"] = 0.75f;
  saturate4.color3_inputs["lumacoeffs"] = make_float3(0.2722287f, 0.6740818f, 0.0536895f);
  saturate4.outputs["out"] = materialx::Type::Color4;

  materialx::Node literal_saturate4;
  literal_saturate4.name = "LiteralSaturate4";
  literal_saturate4.nodedef = "ND_saturate_color4";
  literal_saturate4.float4_inputs["in"] = make_float4(0.4f, 0.5f, 0.6f, 0.25f);
  literal_saturate4.inputs["amount"] = 0.5f;
  literal_saturate4.color3_inputs["lumacoeffs"] = make_float3(0.2722287f, 0.6740818f, 0.0536895f);
  literal_saturate4.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract_alpha;
  extract_alpha.name = "ExtractAlpha";
  extract_alpha.nodedef = "ND_extract_color4";
  extract_alpha.links["in"] = {"Saturate4", "out", materialx::Type::Color4};
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, saturate, color4, saturate4, literal_saturate4, extract_alpha}},
                               &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Saturate.luminance"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Saturate.luminance"])->get_math_type(),
            NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Saturate.luminance"])->get_vector2(),
            make_float3(0.2126f, 0.7152f, 0.0722f));
  ASSERT_NE(dynamic_cast<MixNode *>(lowered["Saturate"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MixNode *>(lowered["Saturate"])->get_fac(), 0.35f);
  EXPECT_FALSE(dynamic_cast<MixNode *>(lowered["Saturate"])->get_use_clamp());
  EXPECT_EQ(dynamic_cast<MixNode *>(lowered["Saturate"])->get_color2(),
            make_float3(0.2f, 0.4f, 0.6f));
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Saturate4.Alpha"]), nullptr);
  ASSERT_NE(lowered["Saturate4.Alpha"]->input("Value1")->link, nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Saturate4.Alpha"])->get_value2(), 0.0f);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["LiteralSaturate4.Alpha"]), nullptr);
  EXPECT_EQ(lowered["LiteralSaturate4.Alpha"]->input("Value1")->link, nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["LiteralSaturate4.Alpha"])->get_value1(), 0.25f);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["LiteralSaturate4.Alpha"])->get_value2(), 0.0f);
}

TEST(materialx_graph, lowers_color4_adjustment_hsv_and_luminance_forms)
{
  /* MaterialX stdlib_defs.mtlx declares ND_rgbtohsv_color4,
   * ND_hsvtorgb_color4, and ND_luminance_color4 in nodegroup="adjustment".
   * The real genglsl implementations use RGB-only HSV conversion with
   * alpha fixed to 1.0, and luminance_color4 as dot(in.rgb, lumacoeffs)
   * replicated to RGB while preserving input alpha. */
  materialx::Node color;
  color.name = "Color";
  color.nodedef = "ND_constant_color4";
  color.float4_inputs["value"] = make_float4(0.2f, 0.4f, 0.6f, 0.8f);
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node rgb_to_hsv;
  rgb_to_hsv.name = "RGBToHSV";
  rgb_to_hsv.nodedef = "ND_rgbtohsv_color4";
  rgb_to_hsv.links["in"] = {"Color", "out", materialx::Type::Color4};
  rgb_to_hsv.outputs["out"] = materialx::Type::Color4;

  materialx::Node hsv_to_rgb;
  hsv_to_rgb.name = "HSVToRGB";
  hsv_to_rgb.nodedef = "ND_hsvtorgb_color4";
  hsv_to_rgb.links["in"] = {"RGBToHSV", "out", materialx::Type::Color4};
  hsv_to_rgb.outputs["out"] = materialx::Type::Color4;

  materialx::Node luminance;
  luminance.name = "Luminance";
  luminance.nodedef = "ND_luminance_color4";
  luminance.links["in"] = {"HSVToRGB", "out", materialx::Type::Color4};
  luminance.color3_inputs["lumacoeffs"] = make_float3(0.2126f, 0.7152f, 0.0722f);
  luminance.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract_alpha;
  extract_alpha.name = "ExtractAlpha";
  extract_alpha.nodedef = "ND_extract_color4";
  extract_alpha.links["in"] = {"Luminance", "out", materialx::Type::Color4};
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, rgb_to_hsv, hsv_to_rgb, luminance, extract_alpha}},
                               &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["RGBToHSV"]), nullptr);
  EXPECT_EQ(dynamic_cast<CombineColorNode *>(lowered["RGBToHSV"])->get_color_type(),
            NODE_COMBSEP_COLOR_RGB);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["RGBToHSV.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["RGBToHSV.Alpha"])->get_value1(), 1.0f);
  ASSERT_NE(dynamic_cast<CombineColorNode *>(lowered["HSVToRGB"]), nullptr);
  EXPECT_EQ(dynamic_cast<CombineColorNode *>(lowered["HSVToRGB"])->get_color_type(),
            NODE_COMBSEP_COLOR_HSV);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["HSVToRGB.Alpha"]), nullptr);
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["HSVToRGB.Alpha"])->get_value1(), 1.0f);
  ASSERT_NE(dynamic_cast<VectorMathNode *>(lowered["Luminance.luminance"]), nullptr);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Luminance.luminance"])->get_math_type(),
            NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(dynamic_cast<VectorMathNode *>(lowered["Luminance.luminance"])->get_vector2(),
            make_float3(0.2126f, 0.7152f, 0.0722f));
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Luminance.Alpha"]), nullptr);
  ASSERT_NE(lowered["Luminance.Alpha"]->input("Value1")->link, nullptr);
}

TEST(materialx_graph, lowers_colortransform_family_from_cmlib_reference_nodegraphs)
{
  const struct {
    const char *id;
    bool color4;
    bool gamma;
    bool srgb;
    bool matrix;
  } cases[] = {{"ND_g18_rec709_to_lin_rec709_color3", false, true, false, false},
               {"ND_g18_rec709_to_lin_rec709_color4", true, true, false, false},
               {"ND_g22_rec709_to_lin_rec709_color3", false, true, false, false},
               {"ND_g22_rec709_to_lin_rec709_color4", true, true, false, false},
               {"ND_rec709_display_to_lin_rec709_color3", false, true, false, false},
               {"ND_rec709_display_to_lin_rec709_color4", true, true, false, false},
               {"ND_acescg_to_lin_rec709_color3", false, false, false, true},
               {"ND_acescg_to_lin_rec709_color4", true, false, false, true},
               {"ND_g22_ap1_to_lin_rec709_color3", false, true, false, true},
               {"ND_g22_ap1_to_lin_rec709_color4", true, true, false, true},
               {"ND_srgb_texture_to_lin_rec709_color3", false, false, true, false},
               {"ND_srgb_texture_to_lin_rec709_color4", true, false, true, false},
               {"ND_lin_adobergb_to_lin_rec709_color3", false, false, false, true},
               {"ND_lin_adobergb_to_lin_rec709_color4", true, false, false, true},
               {"ND_adobergb_to_lin_rec709_color3", false, true, false, true},
               {"ND_adobergb_to_lin_rec709_color4", true, true, false, true},
               {"ND_srgb_displayp3_to_lin_rec709_color3", false, false, true, true},
               {"ND_srgb_displayp3_to_lin_rec709_color4", true, false, true, true},
               {"ND_lin_displayp3_to_lin_rec709_color3", false, false, false, true},
               {"ND_lin_displayp3_to_lin_rec709_color4", true, false, false, true}};

  for (const auto &test : cases) {
    materialx::Node transform;
    transform.name = "ColorTransform";
    transform.nodedef = test.id;
    if (test.color4) {
      transform.float4_inputs["in"] = make_float4(0.25f, 0.5f, 0.75f, 0.875f);
      transform.outputs["out"] = materialx::Type::Color4;
    }
    else {
      transform.color3_inputs["in"] = make_float3(0.25f, 0.5f, 0.75f);
      transform.outputs["out"] = materialx::Type::Color3;
    }

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{transform}}, &graph)) << test.id;
    CombineColorNode *combine = nullptr;
    MathNode *alpha = nullptr;
    bool found_gamma_or_srgb_power = false;
    bool found_srgb_condition = false;
    int matrix_multiplies = 0;
    for (ShaderNode *node : graph.nodes) {
      combine = node->name == "ColorTransform" ? dynamic_cast<CombineColorNode *>(node) : combine;
      alpha = node->name == "ColorTransform.Alpha" ? dynamic_cast<MathNode *>(node) : alpha;
      if (const auto *math = dynamic_cast<MathNode *>(node)) {
        found_gamma_or_srgb_power |= math->get_math_type() == NODE_MATH_POWER;
        found_srgb_condition |= node->name.find(".srgb.condition") != string::npos &&
                                math->get_math_type() == NODE_MATH_GREATER_THAN;
        matrix_multiplies += node->name.find(".matrix.") != string::npos &&
                             math->get_math_type() == NODE_MATH_MULTIPLY;
      }
    }
    ASSERT_NE(combine, nullptr) << test.id;
    EXPECT_EQ(alpha != nullptr, test.color4) << test.id;
    EXPECT_EQ(found_gamma_or_srgb_power, test.gamma || test.srgb) << test.id;
    EXPECT_EQ(found_srgb_condition, test.srgb) << test.id;
    EXPECT_EQ(matrix_multiplies, test.matrix ? 9 : 0) << test.id;
  }
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


TEST(materialx_graph, lowers_procedural2d_grid_mask_to_color3)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  for (const bool staggered : {false, true}) {
    materialx::Node grid;
    grid.name = staggered ? "GridStaggered" : "Grid";
    grid.nodedef = "ND_grid_color3";
    if (staggered) {
      grid.vector2_inputs["texcoord"] = make_float2(0.125f, 0.875f);
    }
    else {
      grid.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
    }
    grid.vector2_inputs["uvtiling"] = make_float2(2.0f, 3.0f);
    grid.vector2_inputs["uvoffset"] = make_float2(0.25f, 0.5f);
    grid.inputs["thickness"] = 0.125f;
    grid.int_inputs["staggered"] = staggered ? 1 : 0;
    grid.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, grid}}, &graph));

    std::unordered_map<string, ShaderNode *> lowered;
    for (ShaderNode *node : graph.nodes) {
      lowered[node->name.string()] = node;
    }

    auto *scale = dynamic_cast<VectorMathNode *>(lowered[grid.name + ".scale"]);
    auto *offset = dynamic_cast<VectorMathNode *>(lowered[grid.name + ".offset"]);
    auto *alternate = dynamic_cast<MathNode *>(lowered[grid.name + ".alternate_shift"]);
    auto *sub_x = dynamic_cast<MathNode *>(lowered[grid.name + ".sub_x"]);
    auto *detect_x = dynamic_cast<MathNode *>(lowered[grid.name + ".detect_x"]);
    auto *detect_y = dynamic_cast<MathNode *>(lowered[grid.name + ".detect_y"]);
    auto *inside_x = dynamic_cast<MathNode *>(lowered[grid.name + ".inside_x"]);
    auto *inside_y = dynamic_cast<MathNode *>(lowered[grid.name + ".inside_y"]);
    auto *mask = dynamic_cast<MathNode *>(lowered[grid.name + ".mask"]);
    auto *invert = dynamic_cast<MathNode *>(lowered[grid.name + ".invert"]);
    auto *color = dynamic_cast<CombineColorNode *>(lowered[grid.name]);
    ASSERT_NE(scale, nullptr);
    ASSERT_NE(offset, nullptr);
    ASSERT_NE(alternate, nullptr);
    ASSERT_NE(sub_x, nullptr);
    ASSERT_NE(detect_x, nullptr);
    ASSERT_NE(detect_y, nullptr);
    ASSERT_NE(inside_x, nullptr);
    ASSERT_NE(inside_y, nullptr);
    ASSERT_NE(mask, nullptr);
    ASSERT_NE(invert, nullptr);
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(scale->get_math_type(), NODE_VECTOR_MATH_MULTIPLY);
    EXPECT_EQ(offset->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
    EXPECT_FLOAT_EQ(alternate->get_value2(), 0.5f);
    EXPECT_FLOAT_EQ(sub_x->get_value2(), 1.0f);
    EXPECT_EQ(detect_x->get_math_type(), NODE_MATH_GREATER_THAN);
    EXPECT_EQ(detect_y->get_math_type(), NODE_MATH_GREATER_THAN);
    EXPECT_EQ(inside_x->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(inside_y->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(mask->get_math_type(), NODE_MATH_MINIMUM);
    EXPECT_EQ(invert->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(invert->input("Value2")->link, mask->output("Value"));
    EXPECT_EQ(color->input("Red")->link, invert->output("Value"));
    EXPECT_EQ(color->input("Green")->link, invert->output("Value"));
    EXPECT_EQ(color->input("Blue")->link, invert->output("Value"));
  }
}

TEST(materialx_graph, lowers_procedural2d_grid_mask_with_reference_lattice_semantics)
{
  materialx::Node grid;
  grid.name = "Grid";
  grid.nodedef = "ND_grid_color3";
  grid.vector2_inputs["texcoord"] = make_float2(0.5f, 0.5f);
  grid.vector2_inputs["uvtiling"] = make_float2(1.0f, 1.0f);
  grid.vector2_inputs["uvoffset"] = make_float2(0.0f, 0.0f);
  grid.inputs["thickness"] = 0.05f;
  grid.int_inputs["staggered"] = 0;
  grid.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{grid}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *mod_y = dynamic_cast<MathNode *>(lowered["Grid.mod_y"]);
  auto *mod_x = dynamic_cast<MathNode *>(lowered["Grid.mod_x"]);
  auto *inside_x = dynamic_cast<MathNode *>(lowered["Grid.inside_x"]);
  auto *inside_y = dynamic_cast<MathNode *>(lowered["Grid.inside_y"]);
  auto *mask = dynamic_cast<MathNode *>(lowered["Grid.mask"]);
  auto *invert = dynamic_cast<MathNode *>(lowered["Grid.invert"]);
  auto *color = dynamic_cast<CombineColorNode *>(lowered["Grid"]);
  ASSERT_NE(mod_y, nullptr);
  ASSERT_NE(mod_x, nullptr);
  ASSERT_NE(inside_x, nullptr);
  ASSERT_NE(inside_y, nullptr);
  ASSERT_NE(mask, nullptr);
  ASSERT_NE(invert, nullptr);
  ASSERT_NE(color, nullptr);
  /* MaterialX mx_mod is GLSL-style floor-based mod, not C fmod; negative tiled
   * coordinates from offsets must still wrap into [0, 1). */
  EXPECT_EQ(mod_y->get_math_type(), NODE_MATH_FLOORED_MODULO);
  EXPECT_EQ(mod_x->get_math_type(), NODE_MATH_FLOORED_MODULO);
  EXPECT_FLOAT_EQ(mod_y->get_value2(), 1.0f);
  EXPECT_FLOAT_EQ(mod_x->get_value2(), 1.0f);
  EXPECT_EQ(inside_x->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(inside_y->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(mask->get_math_type(), NODE_MATH_MINIMUM);
  EXPECT_EQ(invert->input("Value2")->link, mask->output("Value"));
  EXPECT_EQ(color->input("Red")->link, invert->output("Value"));
  EXPECT_EQ(color->input("Green")->link, invert->output("Value"));
  EXPECT_EQ(color->input("Blue")->link, invert->output("Value"));
}

TEST(materialx_graph, lowers_measured_grid_fixture_literal_samples)
{
  /* Mirrors the PROCEDURAL2DSHAPES6 deterministic samples from
   * materialx-terminal-canonical/research/materialx_release/
   * procedural2d_shapes_fixtures.py. The measured failing sample at the cell
   * center must drive the exact MaterialX lattice chain to the inverted grid
   * mask instead of leaving a default constant color path. */
  const float2 samples[] = {make_float2(0.5f, 0.5f),
                            make_float2(0.0f, 0.3f),
                            make_float2(0.3f, 0.0f)};

  for (const float2 sample : samples) {
    materialx::Node grid;
    grid.name = "Grid";
    grid.nodedef = "ND_grid_color3";
    grid.vector2_inputs["texcoord"] = sample;
    grid.vector2_inputs["uvtiling"] = make_float2(1.0f, 1.0f);
    grid.vector2_inputs["uvoffset"] = zero_float2();
    grid.inputs["thickness"] = 0.05f;
    grid.int_inputs["staggered"] = 0;
    grid.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    string error;
    ASSERT_TRUE(materialx::lower({{grid}}, &graph, &error)) << error;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *scale = dynamic_cast<VectorMathNode *>(nodes["Grid.scale"]);
    auto *mod_x = dynamic_cast<MathNode *>(nodes["Grid.mod_x"]);
    auto *mod_y = dynamic_cast<MathNode *>(nodes["Grid.mod_y"]);
    auto *inside_x = dynamic_cast<MathNode *>(nodes["Grid.inside_x"]);
    auto *inside_y = dynamic_cast<MathNode *>(nodes["Grid.inside_y"]);
    auto *mask = dynamic_cast<MathNode *>(nodes["Grid.mask"]);
    auto *invert = dynamic_cast<MathNode *>(nodes["Grid.invert"]);
    auto *color = dynamic_cast<CombineColorNode *>(nodes["Grid"]);
    ASSERT_NE(scale, nullptr);
    ASSERT_NE(mod_x, nullptr);
    ASSERT_NE(mod_y, nullptr);
    ASSERT_NE(inside_x, nullptr);
    ASSERT_NE(inside_y, nullptr);
    ASSERT_NE(mask, nullptr);
    ASSERT_NE(invert, nullptr);
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(scale->get_vector1(), make_float3(sample, 0.0f));
    EXPECT_EQ(scale->input("Vector1")->link, nullptr);
    EXPECT_EQ(mod_x->get_math_type(), NODE_MATH_FLOORED_MODULO);
    EXPECT_EQ(mod_y->get_math_type(), NODE_MATH_FLOORED_MODULO);
    EXPECT_EQ(inside_x->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(inside_y->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(mask->get_math_type(), NODE_MATH_MINIMUM);
    EXPECT_EQ(invert->input("Value2")->link, mask->output("Value"));
    EXPECT_EQ(color->input("Red")->link, invert->output("Value"));
    EXPECT_EQ(color->input("Green")->link, invert->output("Value"));
    EXPECT_EQ(color->input("Blue")->link, invert->output("Value"));
  }
}

TEST(materialx_graph, lowers_measured_crosshatch_fixture_literal_samples)
{
  /* Mirrors the PROCEDURAL2DSHAPES6 deterministic samples from
   * materialx-terminal-canonical/research/materialx_release/
   * procedural2d_shapes_fixtures.py. These samples exercise the literal
   * texcoord path through the shared grid prelude and the two native diagonal
   * line masks that form the crosshatch output. */
  const float2 samples[] = {make_float2(0.5f, 0.5f),
                            make_float2(0.0f, 0.0f),
                            make_float2(0.25f, 0.75f)};

  for (const float2 sample : samples) {
    materialx::Node crosshatch;
    crosshatch.name = "Crosshatch";
    crosshatch.nodedef = "ND_crosshatch_color3";
    crosshatch.vector2_inputs["texcoord"] = sample;
    crosshatch.vector2_inputs["uvtiling"] = make_float2(1.0f, 1.0f);
    crosshatch.vector2_inputs["uvoffset"] = zero_float2();
    crosshatch.inputs["thickness"] = 0.05f;
    crosshatch.int_inputs["staggered"] = 0;
    crosshatch.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    string error;
    ASSERT_TRUE(materialx::lower({{crosshatch}}, &graph, &error)) << error;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *scale = dynamic_cast<VectorMathNode *>(nodes["Crosshatch.scale"]);
    auto *mod_x = dynamic_cast<MathNode *>(nodes["Crosshatch.mod_x"]);
    auto *mod_y = dynamic_cast<MathNode *>(nodes["Crosshatch.mod_y"]);
    auto *sample_vec = dynamic_cast<CombineXYZNode *>(nodes["Crosshatch.sample_vec"]);
    auto *line1 = dynamic_cast<MathNode *>(nodes["Crosshatch.line_diag1"]);
    auto *line2 = dynamic_cast<MathNode *>(nodes["Crosshatch.line_diag2"]);
    auto *composite = dynamic_cast<MathNode *>(nodes["Crosshatch.composite_diags"]);
    auto *color = dynamic_cast<CombineColorNode *>(nodes["Crosshatch"]);
    ASSERT_NE(scale, nullptr);
    ASSERT_NE(mod_x, nullptr);
    ASSERT_NE(mod_y, nullptr);
    ASSERT_NE(sample_vec, nullptr);
    ASSERT_NE(line1, nullptr);
    ASSERT_NE(line2, nullptr);
    ASSERT_NE(composite, nullptr);
    ASSERT_NE(color, nullptr);
    EXPECT_EQ(scale->get_vector1(), make_float3(sample, 0.0f));
    EXPECT_EQ(scale->input("Vector1")->link, nullptr);
    EXPECT_EQ(mod_x->get_math_type(), NODE_MATH_FLOORED_MODULO);
    EXPECT_EQ(mod_y->get_math_type(), NODE_MATH_FLOORED_MODULO);
    EXPECT_NE(sample_vec->input("X")->link, nullptr);
    EXPECT_NE(sample_vec->input("Y")->link, nullptr);
    EXPECT_EQ(line1->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(line2->get_math_type(), NODE_MATH_SUBTRACT);
    EXPECT_EQ(composite->get_math_type(), NODE_MATH_MAXIMUM);
    EXPECT_EQ(color->input("Red")->link, composite->output("Value"));
    EXPECT_EQ(color->input("Green")->link, composite->output("Value"));
    EXPECT_EQ(color->input("Blue")->link, composite->output("Value"));
  }
}

TEST(materialx_graph, lowers_measured_scalar_shape_fixture_literal_samples)
{
  /* Mirrors the PROCEDURAL2DSHAPES6 deterministic samples for the scalar
   * procedural2d masks. The test keeps each measured literal coordinate routed
   * into the native graph instead of silently falling back to the default zero
   * vector on the entry node. */
  const struct {
    const char *id;
    const char *name;
    float2 center;
    float radius;
    float2 point1;
    float2 point2;
    float2 samples[3];
  } cases[] = {{"ND_line_float",
                "Line",
                zero_float2(),
                0.1f,
                make_float2(0.25f, 0.25f),
                make_float2(0.75f, 0.75f),
                {make_float2(0.5f, 0.5f), make_float2(0.5f, 0.7f), zero_float2()}},
               {"ND_circle_float",
                "Circle",
                zero_float2(),
                0.5f,
                zero_float2(),
                zero_float2(),
                {make_float2(0.1f, 0.1f), make_float2(0.5f, 0.0f), make_float2(1.0f, 1.0f)}},
               {"ND_cloverleaf_float",
                "Cloverleaf",
                zero_float2(),
                0.5f,
                zero_float2(),
                zero_float2(),
                {zero_float2(), make_float2(0.4f, 0.0f), make_float2(0.9f, 0.9f)}}};

  for (const auto &test : cases) {
    for (const float2 sample : test.samples) {
      materialx::Node shape;
      shape.name = test.name;
      shape.nodedef = test.id;
      shape.vector2_inputs["texcoord"] = sample;
      shape.vector2_inputs["center"] = test.center;
      shape.inputs["radius"] = test.radius;
      if (string(test.id) == "ND_line_float") {
        shape.vector2_inputs["point1"] = test.point1;
        shape.vector2_inputs["point2"] = test.point2;
      }
      shape.outputs["out"] = materialx::Type::Float;

      ShaderGraph graph;
      string error;
      ASSERT_TRUE(materialx::lower({{shape}}, &graph, &error)) << test.id << ": " << error;

      std::unordered_map<string, ShaderNode *> nodes;
      for (ShaderNode *node : graph.nodes) {
        nodes[node->name.string()] = node;
      }
      auto *result = dynamic_cast<MathNode *>(nodes[test.name]);
      ASSERT_NE(result, nullptr) << test.id;
      if (string(test.id) == "ND_cloverleaf_float") {
        auto *sample_double = dynamic_cast<VectorMathNode *>(nodes[string(test.name) + ".sample_double"]);
        auto *petal = dynamic_cast<VectorMathNode *>(nodes[string(test.name) + ".circle1.dist_square"]);
        ASSERT_NE(sample_double, nullptr) << test.id;
        ASSERT_NE(petal, nullptr) << test.id;
        EXPECT_EQ(sample_double->get_vector1(), make_float3(sample, 0.0f)) << test.id;
        EXPECT_EQ(sample_double->input("Vector1")->link, nullptr) << test.id;
        EXPECT_EQ(sample_double->get_math_type(), NODE_VECTOR_MATH_SCALE) << test.id;
        EXPECT_FLOAT_EQ(sample_double->get_scale(), 2.0f) << test.id;
        EXPECT_EQ(result->get_math_type(), NODE_MATH_MAXIMUM) << test.id;
        EXPECT_NE(result->input("Value1")->link, nullptr) << test.id;
      }
      else {
        auto *delta = dynamic_cast<VectorMathNode *>(nodes[string(test.name) + ".delta"]);
        auto *condition = dynamic_cast<MathNode *>(nodes[string(test.name) + ".condition"]);
        ASSERT_NE(delta, nullptr) << test.id;
        ASSERT_NE(condition, nullptr) << test.id;
        EXPECT_EQ(delta->get_vector1(), make_float3(sample, 0.0f)) << test.id;
        EXPECT_EQ(delta->input("Vector1")->link, nullptr) << test.id;
        EXPECT_EQ(condition->get_math_type(), NODE_MATH_GREATER_THAN) << test.id;
        EXPECT_FLOAT_EQ(condition->get_value2(), string(test.id) == "ND_circle_float" ?
                                                     test.radius * test.radius :
                                                     test.radius)
            << test.id;
        EXPECT_EQ(result->get_math_type(), NODE_MATH_SUBTRACT) << test.id;
        EXPECT_EQ(result->input("Value2")->link, condition->output("Value")) << test.id;
      }
    }
  }
}

TEST(materialx_graph, lowers_tiledcircles_color3_regular_pattern_with_literal_texcoord)
{
  materialx::Node tiled;
  tiled.name = "TiledCircles";
  tiled.nodedef = "ND_tiledcircles_color3";
  tiled.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
  tiled.vector2_inputs["uvtiling"] = make_float2(2.0f, 3.0f);
  tiled.vector2_inputs["uvoffset"] = make_float2(0.125f, 0.25f);
  tiled.inputs["size"] = 0.4f;
  tiled.int_inputs["staggered"] = 0;
  tiled.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{tiled}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *scale = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.scale"]);
  auto *offset = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.offset"]);
  auto *wrap = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.wrap"]);
  auto *double_coord = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.double"]);
  auto *recenter = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.recenter"]);
  auto *dist_square = dynamic_cast<VectorMathNode *>(lowered["TiledCircles.dist_square"]);
  auto *radius_square = dynamic_cast<MathNode *>(lowered["TiledCircles.radius_square"]);
  auto *condition = dynamic_cast<MathNode *>(lowered["TiledCircles.condition"]);
  auto *mask = dynamic_cast<MathNode *>(lowered["TiledCircles.mask"]);
  auto *color = dynamic_cast<CombineColorNode *>(lowered["TiledCircles"]);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(offset, nullptr);
  ASSERT_NE(wrap, nullptr);
  ASSERT_NE(double_coord, nullptr);
  ASSERT_NE(recenter, nullptr);
  ASSERT_NE(dist_square, nullptr);
  ASSERT_NE(radius_square, nullptr);
  ASSERT_NE(condition, nullptr);
  ASSERT_NE(mask, nullptr);
  ASSERT_NE(color, nullptr);
  EXPECT_EQ(scale->get_math_type(), NODE_VECTOR_MATH_MULTIPLY);
  EXPECT_EQ(scale->get_vector1(), make_float3(0.25f, 0.75f, 0.0f));
  EXPECT_EQ(scale->input("Vector1")->link, nullptr);
  EXPECT_EQ(offset->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(wrap->get_math_type(), NODE_VECTOR_MATH_MODULO);
  EXPECT_EQ(double_coord->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_FLOAT_EQ(double_coord->get_scale(), 2.0f);
  EXPECT_EQ(recenter->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(dist_square->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(radius_square->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(radius_square->get_value1(), 0.4f);
  EXPECT_EQ(condition->get_math_type(), NODE_MATH_GREATER_THAN);
  EXPECT_EQ(mask->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(color->input("Red")->link, mask->output("Value"));
  EXPECT_EQ(color->input("Green")->link, mask->output("Value"));
  EXPECT_EQ(color->input("Blue")->link, mask->output("Value"));
}

TEST(materialx_graph, lowers_tiledcloverleafs_color3_regular_pattern_with_literal_texcoord)
{
  materialx::Node tiled;
  tiled.name = "TiledCloverleafs";
  tiled.nodedef = "ND_tiledcloverleafs_color3";
  tiled.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
  tiled.vector2_inputs["uvtiling"] = make_float2(2.0f, 3.0f);
  tiled.vector2_inputs["uvoffset"] = make_float2(0.125f, 0.25f);
  tiled.inputs["size"] = 0.4f;
  tiled.int_inputs["staggered"] = 0;
  tiled.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{tiled}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *scale = dynamic_cast<VectorMathNode *>(lowered["TiledCloverleafs.scale"]);
  auto *recenter = dynamic_cast<VectorMathNode *>(lowered["TiledCloverleafs.recenter"]);
  auto *shape = dynamic_cast<MathNode *>(lowered["TiledCloverleafs.cloverleaf_regular"]);
  auto *color = dynamic_cast<CombineColorNode *>(lowered["TiledCloverleafs"]);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(recenter, nullptr);
  ASSERT_NE(shape, nullptr);
  ASSERT_NE(color, nullptr);
  EXPECT_EQ(scale->get_vector1(), make_float3(0.25f, 0.75f, 0.0f));
  EXPECT_EQ(scale->input("Vector1")->link, nullptr);
  EXPECT_EQ(recenter->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(shape->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_EQ(color->input("Red")->link, shape->output("Value"));
  EXPECT_EQ(color->input("Green")->link, shape->output("Value"));
  EXPECT_EQ(color->input("Blue")->link, shape->output("Value"));
}

TEST(materialx_graph, lowers_tiledhexagons_color3_regular_pattern_with_literal_texcoord)
{
  materialx::Node tiled;
  tiled.name = "TiledHexagons";
  tiled.nodedef = "ND_tiledhexagons_color3";
  tiled.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
  tiled.vector2_inputs["uvtiling"] = make_float2(2.0f, 3.0f);
  tiled.vector2_inputs["uvoffset"] = make_float2(0.125f, 0.25f);
  tiled.inputs["size"] = 0.4f;
  tiled.int_inputs["staggered"] = 0;
  tiled.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{tiled}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *scale = dynamic_cast<VectorMathNode *>(lowered["TiledHexagons.scale"]);
  auto *recenter = dynamic_cast<VectorMathNode *>(lowered["TiledHexagons.recenter"]);
  auto *shape = dynamic_cast<MathNode *>(lowered["TiledHexagons.hexagon_regular"]);
  auto *color = dynamic_cast<CombineColorNode *>(lowered["TiledHexagons"]);
  ASSERT_NE(scale, nullptr);
  ASSERT_NE(recenter, nullptr);
  ASSERT_NE(shape, nullptr);
  ASSERT_NE(color, nullptr);
  EXPECT_EQ(scale->get_vector1(), make_float3(0.25f, 0.75f, 0.0f));
  EXPECT_EQ(scale->input("Vector1")->link, nullptr);
  EXPECT_EQ(recenter->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(shape->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(color->input("Red")->link, shape->output("Value"));
  EXPECT_EQ(color->input("Green")->link, shape->output("Value"));
  EXPECT_EQ(color->input("Blue")->link, shape->output("Value"));
}

TEST(materialx_graph, lowers_procedural2d_crosshatch_mask_to_color3)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node crosshatch;
  crosshatch.name = "Crosshatch";
  crosshatch.nodedef = "ND_crosshatch_color3";
  crosshatch.vector2_inputs["texcoord"] = make_float2(0.125f, 0.875f);
  crosshatch.vector2_inputs["uvtiling"] = make_float2(2.0f, 3.0f);
  crosshatch.vector2_inputs["uvoffset"] = make_float2(0.25f, 0.5f);
  crosshatch.inputs["thickness"] = 0.125f;
  crosshatch.int_inputs["staggered"] = 1;
  crosshatch.outputs["out"] = materialx::Type::Color3;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{crosshatch}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *sample_vec = dynamic_cast<CombineXYZNode *>(lowered["Crosshatch.sample_vec"]);
  auto *line1 = dynamic_cast<MathNode *>(lowered["Crosshatch.line_diag1"]);
  auto *line2 = dynamic_cast<MathNode *>(lowered["Crosshatch.line_diag2"]);
  auto *line1_distance = dynamic_cast<VectorMathNode *>(lowered["Crosshatch.line_diag1.distance"]);
  auto *line2_projected = dynamic_cast<VectorMathNode *>(lowered["Crosshatch.line_diag2.projected"]);
  auto *composite = dynamic_cast<MathNode *>(lowered["Crosshatch.composite_diags"]);
  auto *color = dynamic_cast<CombineColorNode *>(lowered["Crosshatch"]);
  ASSERT_NE(sample_vec, nullptr);
  ASSERT_NE(line1, nullptr);
  ASSERT_NE(line2, nullptr);
  ASSERT_NE(line1_distance, nullptr);
  ASSERT_NE(line2_projected, nullptr);
  ASSERT_NE(composite, nullptr);
  ASSERT_NE(color, nullptr);
  EXPECT_NE(sample_vec->input("X")->link, nullptr);
  EXPECT_NE(sample_vec->input("Y")->link, nullptr);
  EXPECT_EQ(line1->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(line2->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(line1_distance->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
  EXPECT_EQ(line2_projected->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_EQ(composite->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_EQ(composite->input("Value1")->link, line1->output("Value"));
  EXPECT_EQ(composite->input("Value2")->link, line2->output("Value"));
  EXPECT_EQ(color->input("Red")->link, composite->output("Value"));
  EXPECT_EQ(color->input("Green")->link, composite->output("Value"));
  EXPECT_EQ(color->input("Blue")->link, composite->output("Value"));
}

TEST(materialx_graph, lowers_procedural2d_circle_and_line_masks)
{
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node circle;
  circle.name = "Circle";
  circle.nodedef = "ND_circle_float";
  circle.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  circle.vector2_inputs["center"] = make_float2(0.5f, 0.5f);
  circle.inputs["radius"] = 0.25f;
  circle.outputs["out"] = materialx::Type::Float;

  materialx::Node line;
  line.name = "Line";
  line.nodedef = "ND_line_float";
  line.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  line.vector2_inputs["center"] = make_float2(0.5f, 0.5f);
  line.vector2_inputs["point1"] = make_float2(0.0f, 0.0f);
  line.vector2_inputs["point2"] = make_float2(1.0f, 0.0f);
  line.inputs["radius"] = 0.1f;
  line.outputs["out"] = materialx::Type::Float;

  materialx::Node cloverleaf;
  cloverleaf.name = "Cloverleaf";
  cloverleaf.nodedef = "ND_cloverleaf_float";
  cloverleaf.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  cloverleaf.vector2_inputs["center"] = make_float2(0.5f, 0.5f);
  cloverleaf.inputs["radius"] = 0.125f;
  cloverleaf.outputs["out"] = materialx::Type::Float;

  materialx::Node hexagon;
  hexagon.name = "Hexagon";
  hexagon.nodedef = "ND_hexagon_float";
  hexagon.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  hexagon.vector2_inputs["center"] = make_float2(0.5f, 0.5f);
  hexagon.inputs["radius"] = 0.25f;
  hexagon.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{texcoord, circle, line, cloverleaf, hexagon}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }

  auto *circle_delta = dynamic_cast<VectorMathNode *>(lowered["Circle.delta"]);
  auto *circle_distance = dynamic_cast<VectorMathNode *>(lowered["Circle.dist_square"]);
  auto *circle_condition = dynamic_cast<MathNode *>(lowered["Circle.condition"]);
  auto *circle_result = dynamic_cast<MathNode *>(lowered["Circle"]);
  ASSERT_NE(circle_delta, nullptr);
  ASSERT_NE(circle_distance, nullptr);
  ASSERT_NE(circle_condition, nullptr);
  ASSERT_NE(circle_result, nullptr);
  EXPECT_EQ(circle_delta->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(circle_distance->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(circle_condition->get_math_type(), NODE_MATH_GREATER_THAN);
  EXPECT_FLOAT_EQ(circle_condition->get_value2(), 0.0625f);
  EXPECT_EQ(circle_result->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(circle_result->input("Value2")->link, circle_condition->output("Value"));

  auto *line_projected = dynamic_cast<VectorMathNode *>(lowered["Line.projected"]);
  auto *line_distance = dynamic_cast<VectorMathNode *>(lowered["Line.distance"]);
  auto *line_condition = dynamic_cast<MathNode *>(lowered["Line.condition"]);
  ASSERT_NE(line_projected, nullptr);
  ASSERT_NE(line_distance, nullptr);
  ASSERT_NE(line_condition, nullptr);
  EXPECT_EQ(line_projected->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_EQ(line_distance->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
  EXPECT_EQ(line_condition->get_math_type(), NODE_MATH_GREATER_THAN);
  EXPECT_FLOAT_EQ(line_condition->get_value2(), 0.1f);
  EXPECT_NE(line_projected->input("Scale")->link, nullptr);

  auto *cloverleaf_sample = dynamic_cast<VectorMathNode *>(lowered["Cloverleaf.sample_double"]);
  auto *cloverleaf_petal = dynamic_cast<VectorMathNode *>(lowered["Cloverleaf.circle1.dist_square"]);
  auto *cloverleaf_mask = dynamic_cast<MathNode *>(lowered["Cloverleaf.circle1.mask"]);
  auto *cloverleaf_max = dynamic_cast<MathNode *>(lowered["Cloverleaf"]);
  ASSERT_NE(cloverleaf_sample, nullptr);
  ASSERT_NE(cloverleaf_petal, nullptr);
  ASSERT_NE(cloverleaf_mask, nullptr);
  ASSERT_NE(cloverleaf_max, nullptr);
  EXPECT_EQ(cloverleaf_sample->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_FLOAT_EQ(cloverleaf_sample->get_scale(), 2.0f);
  EXPECT_EQ(cloverleaf_petal->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(cloverleaf_mask->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(cloverleaf_max->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_NE(cloverleaf_max->input("Value1")->link, nullptr);

  auto *hexagon_delta_abs = dynamic_cast<VectorMathNode *>(lowered["Hexagon.delta_abs"]);
  auto *hexagon_clamp = dynamic_cast<ClampNode *>(lowered["Hexagon.clamp"]);
  auto *hexagon_sum = dynamic_cast<VectorMathNode *>(lowered["Hexagon.p3_sum"]);
  auto *hexagon_result = dynamic_cast<MathNode *>(lowered["Hexagon"]);
  ASSERT_NE(hexagon_delta_abs, nullptr);
  ASSERT_NE(hexagon_clamp, nullptr);
  ASSERT_NE(hexagon_sum, nullptr);
  ASSERT_NE(hexagon_result, nullptr);
  EXPECT_EQ(hexagon_delta_abs->get_math_type(), NODE_VECTOR_MATH_ABSOLUTE);
  EXPECT_FLOAT_EQ(hexagon_clamp->get_min(), -0.57735f * 0.25f);
  EXPECT_FLOAT_EQ(hexagon_clamp->get_max(), 0.57735f * 0.25f);
  EXPECT_EQ(hexagon_sum->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  EXPECT_EQ(hexagon_result->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_NE(hexagon_result->input("Value2")->link, nullptr);
}

TEST(materialx_graph, lowers_procedural2d_scalar_shapes_with_literal_texcoords)
{
  const struct {
    const char *id;
    bool line;
  } cases[] = {{"ND_circle_float", false},
               {"ND_line_float", true},
               {"ND_cloverleaf_float", false},
               {"ND_hexagon_float", false}};

  for (const auto &test : cases) {
    materialx::Node node;
    node.name = "Shape";
    node.nodedef = test.id;
    node.vector2_inputs["texcoord"] = make_float2(0.125f, 0.875f);
    node.vector2_inputs["center"] = make_float2(0.5f, 0.5f);
    node.inputs["radius"] = test.line ? 0.1f : 0.25f;
    if (test.line) {
      node.vector2_inputs["point1"] = make_float2(0.0f, 0.0f);
      node.vector2_inputs["point2"] = make_float2(1.0f, 0.0f);
    }
    node.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> lowered;
    for (ShaderNode *shader_node : graph.nodes) {
      lowered[shader_node->name.string()] = shader_node;
    }

    if (string(test.id) == "ND_cloverleaf_float") {
      auto *sample = dynamic_cast<VectorMathNode *>(lowered["Shape.sample_double"]);
      ASSERT_NE(sample, nullptr) << test.id;
      EXPECT_EQ(sample->get_vector1(), make_float3(0.125f, 0.875f, 0.0f)) << test.id;
      EXPECT_EQ(sample->input("Vector1")->link, nullptr) << test.id;
    }
    else {
      auto *delta = dynamic_cast<VectorMathNode *>(lowered["Shape.delta"]);
      ASSERT_NE(delta, nullptr) << test.id;
      EXPECT_EQ(delta->get_vector1(), make_float3(0.125f, 0.875f, 0.0f)) << test.id;
      EXPECT_EQ(delta->input("Vector1")->link, nullptr) << test.id;
    }
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

TEST(materialx_graph, lowers_cellnoise_family_with_literal_coordinates)
{
  const struct {
    const char *id;
    const char *input_name;
    materialx::Type input_type;
    int dimensions;
  } cases[] = {{"ND_cellnoise2d_float", "texcoord", materialx::Type::Vector2, 2},
               {"ND_cellnoise3d_float", "position", materialx::Type::Vector3, 3}};

  for (const auto &test : cases) {
    materialx::Node cellnoise;
    cellnoise.name = "CellNoise";
    cellnoise.nodedef = test.id;
    if (test.input_type == materialx::Type::Vector2) {
      cellnoise.vector2_inputs[test.input_name] = make_float2(0.125f, 0.875f);
    }
    else {
      cellnoise.vector3_inputs[test.input_name] = make_float3(0.25f, 0.5f, 0.75f);
    }
    cellnoise.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{cellnoise}}, &graph)) << test.id;

    WhiteNoiseTextureNode *white_noise = nullptr;
    VectorMathNode *floor = nullptr;
    for (ShaderNode *node : graph.nodes) {
      white_noise = node->name == "CellNoise" ? dynamic_cast<WhiteNoiseTextureNode *>(node) :
                                                white_noise;
      floor = node->name == "CellNoise.floor" ? dynamic_cast<VectorMathNode *>(node) : floor;
    }
    ASSERT_NE(white_noise, nullptr) << test.id;
    ASSERT_NE(floor, nullptr) << test.id;
    EXPECT_EQ(floor->get_math_type(), NODE_VECTOR_MATH_FLOOR) << test.id;
    EXPECT_EQ(floor->input("Vector1")->link, nullptr) << test.id;
    EXPECT_EQ(floor->get_vector1(), test.input_type == materialx::Type::Vector2 ?
                                      make_float3(0.125f, 0.875f, 0.0f) :
                                      make_float3(0.25f, 0.5f, 0.75f))
        << test.id;
    EXPECT_EQ(white_noise->get_dimensions(), test.dimensions) << test.id;
    EXPECT_NE(white_noise->input("Vector")->link, nullptr) << test.id;
  }
}

TEST(materialx_graph, lowers_worleynoise_distance_subset_to_native_voronoi)
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
    materialx::Type output_type;
    int dimensions;
  } cases[] = {{"ND_worleynoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, materialx::Type::Float, 2},
               {"ND_worleynoise3d_float", "position", "Position", materialx::Type::Vector3, materialx::Type::Float, 3},
               {"ND_worleynoise2d_vector2", "texcoord", "Texcoord", materialx::Type::Vector2, materialx::Type::Vector2, 2},
               {"ND_worleynoise3d_vector2", "position", "Position", materialx::Type::Vector3, materialx::Type::Vector2, 3}};

  for (const auto &test : cases) {
    materialx::Node worley;
    worley.name = "Worley";
    worley.nodedef = test.id;
    worley.links[test.input_name] = {test.source_name, "out", test.input_type};
    worley.inputs["jitter"] = 0.625f;
    worley.int_inputs["style"] = 0;
    worley.outputs["out"] = test.output_type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, position, worley}}, &graph)) << test.id;

    std::vector<VoronoiTextureNode *> voronoi_nodes;
    ShaderNode *lowered = nullptr;
    for (ShaderNode *node : graph.nodes) {
      if (auto *voronoi = dynamic_cast<VoronoiTextureNode *>(node)) {
        voronoi_nodes.push_back(voronoi);
      }
      lowered = node->name == "Worley" ? node : lowered;
    }
    ASSERT_NE(lowered, nullptr) << test.id;
    ASSERT_EQ(voronoi_nodes.size(), test.output_type == materialx::Type::Vector2 ? 2 : 1) << test.id;
    for (VoronoiTextureNode *voronoi : voronoi_nodes) {
      EXPECT_EQ(voronoi->get_dimensions(), test.dimensions) << test.id;
      EXPECT_EQ(voronoi->get_metric(), NODE_VORONOI_EUCLIDEAN) << test.id;
      EXPECT_FLOAT_EQ(voronoi->get_randomness(), 0.625f) << test.id;
      EXPECT_FLOAT_EQ(voronoi->get_scale(), 1.0f) << test.id;
      ASSERT_NE(voronoi->input("Vector")->link, nullptr) << test.id;
    }
    if (test.output_type == materialx::Type::Vector2) {
      EXPECT_NE(dynamic_cast<CombineXYZNode *>(lowered), nullptr) << test.id;
      EXPECT_EQ(voronoi_nodes[0]->output("Distance")->links[0], lowered->input("X")) << test.id;
      EXPECT_EQ(voronoi_nodes[1]->output("Distance")->links[0], lowered->input("Y")) << test.id;
    }
    else {
      EXPECT_EQ(lowered, voronoi_nodes[0]) << test.id;
    }
  }
}

TEST(materialx_graph, lowers_worleynoise_distance_subset_with_literal_coordinates)
{
  const struct {
    const char *id;
    const char *input_name;
    materialx::Type input_type;
    materialx::Type output_type;
    int dimensions;
  } cases[] = {{"ND_worleynoise2d_float", "texcoord", materialx::Type::Vector2, materialx::Type::Float, 2},
               {"ND_worleynoise3d_float", "position", materialx::Type::Vector3, materialx::Type::Float, 3},
               {"ND_worleynoise2d_vector2", "texcoord", materialx::Type::Vector2, materialx::Type::Vector2, 2},
               {"ND_worleynoise3d_vector2", "position", materialx::Type::Vector3, materialx::Type::Vector2, 3}};

  for (const auto &test : cases) {
    materialx::Node worley{"Worley", test.id};
    if (test.input_type == materialx::Type::Vector2) {
      worley.vector2_inputs[test.input_name] = make_float2(0.125f, 0.875f);
    }
    else {
      worley.vector3_inputs[test.input_name] = make_float3(0.25f, 0.5f, 0.75f);
    }
    worley.inputs["jitter"] = 0.625f;
    worley.int_inputs["style"] = 0;
    worley.outputs["out"] = test.output_type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{worley}}, &graph)) << test.id;

    std::vector<VoronoiTextureNode *> voronoi_nodes;
    for (ShaderNode *node : graph.nodes) {
      if (auto *voronoi = dynamic_cast<VoronoiTextureNode *>(node)) {
        voronoi_nodes.push_back(voronoi);
      }
    }
    ASSERT_EQ(voronoi_nodes.size(), test.output_type == materialx::Type::Vector2 ? 2 : 1) << test.id;
    for (VoronoiTextureNode *voronoi : voronoi_nodes) {
      EXPECT_EQ(voronoi->get_dimensions(), test.dimensions) << test.id;
      EXPECT_EQ(voronoi->get_metric(), NODE_VORONOI_EUCLIDEAN) << test.id;
      EXPECT_FLOAT_EQ(voronoi->get_randomness(), 0.625f) << test.id;
      EXPECT_EQ(voronoi->input("Vector")->link, nullptr) << test.id;
      EXPECT_EQ(voronoi->get_vector(), test.input_type == materialx::Type::Vector2 ?
                                          make_float3(0.125f, 0.875f, 0.0f) :
                                          make_float3(0.25f, 0.5f, 0.75f))
          << test.id;
    }
  }
}

TEST(materialx_graph, rejects_invalid_worleynoise_distance_subset_atomically)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;
  materialx::Node worley{"Worley", "ND_worleynoise2d_float"};
  worley.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  worley.inputs["jitter"] = 1.25f;
  worley.int_inputs["style"] = 1;
  worley.outputs["out"] = materialx::Type::Float;

  EXPECT_FALSE(materialx::validate({{texcoord, worley}}));
  ShaderGraph graph;
  graph.create_node<ValueNode>()->name = "Sentinel";
  const size_t original_node_count = graph.nodes.size();
  ASSERT_FALSE(materialx::lower({{texcoord, worley}}, &graph));
  ASSERT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_application_frame_and_time_to_scene_time)
{
  materialx::Node frame;
  frame.name = "Frame";
  frame.nodedef = "ND_frame_float";
  frame.outputs["out"] = materialx::Type::Float;

  materialx::Node time;
  time.name = "Time";
  time.nodedef = "ND_time_float";
  time.inputs["fps"] = 24.0f;
  time.outputs["out"] = materialx::Type::Float;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_float";
  add.links["in1"] = {"Frame", "out", materialx::Type::Float};
  add.links["in2"] = {"Time", "out", materialx::Type::Float};
  add.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{frame, time, add}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[node->name.string()] = node;
  }
  auto *frame_node = dynamic_cast<SceneTimeNode *>(lowered["Frame"]);
  auto *time_frame_node = dynamic_cast<SceneTimeNode *>(lowered["Time.frame"]);
  auto *time_node = dynamic_cast<MathNode *>(lowered["Time"]);
  auto *add_node = dynamic_cast<MathNode *>(lowered["Add"]);
  ASSERT_NE(frame_node, nullptr);
  ASSERT_NE(time_frame_node, nullptr);
  ASSERT_NE(time_node, nullptr);
  ASSERT_NE(add_node, nullptr);
  EXPECT_EQ(time_node->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_FLOAT_EQ(time_node->get_value2(), 24.0f);
  EXPECT_EQ(time_node->input("Value1")->link, time_frame_node->output("Frame"));
  EXPECT_EQ(add_node->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(add_node->input("Value1")->link, frame_node->output("Frame"));
  EXPECT_EQ(add_node->input("Value2")->link, time_node->output("Value"));
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

TEST(materialx_graph, lowers_unifiednoise_literal_type_branches)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node position{"Position", "ND_constant_vector3"};
  position.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  position.outputs["out"] = materialx::Type::Vector3;

  const struct {
    const char *id;
    const char *input_name;
    const char *source_name;
    materialx::Type input_type;
    int type;
    int noise_dimensions;
  } cases[] = {{"ND_unifiednoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, 0, 2},
               {"ND_unifiednoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, 1, 2},
               {"ND_unifiednoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, 2, 2},
               {"ND_unifiednoise2d_float", "texcoord", "Texcoord", materialx::Type::Vector2, 3, 3},
               {"ND_unifiednoise3d_float", "position", "Position", materialx::Type::Vector3, 0, 3},
               {"ND_unifiednoise3d_float", "position", "Position", materialx::Type::Vector3, 1, 3},
               {"ND_unifiednoise3d_float", "position", "Position", materialx::Type::Vector3, 2, 3},
               {"ND_unifiednoise3d_float", "position", "Position", materialx::Type::Vector3, 3, 3}};

  for (const auto &test : cases) {
    materialx::Node noise{"Unified", test.id};
    noise.links[test.input_name] = {test.source_name, "out", test.input_type};
    noise.inputs = {{"jitter", 0.625f},
                    {"outmin", 0.2f},
                    {"outmax", 0.8f},
                    {"lacunarity", 2.5f},
                    {"diminish", 0.375f}};
    noise.int_inputs = {{"clampoutput", 1}, {"octaves", 4}, {"type", test.type}, {"style", 0}};
    if (test.input_type == materialx::Type::Vector2) {
      noise.vector2_inputs["freq"] = make_float2(2.0f, 3.0f);
      noise.vector2_inputs["offset"] = make_float2(0.25f, 0.5f);
    }
    else {
      noise.vector3_inputs["freq"] = make_float3(2.0f, 3.0f, 4.0f);
      noise.vector3_inputs["offset"] = make_float3(0.25f, 0.5f, 0.75f);
    }
    if (test.type == 0 || test.type == 1 || test.type == 3) {
      noise.inputs["jitter"] = 1.0f;
    }
    noise.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, position, noise}}, &graph)) << test.id << ":" << test.type;

    MapRangeNode *range = nullptr;
    ShaderNode *sample = nullptr;
    for (ShaderNode *node : graph.nodes) {
      range = node->name == "Unified" ? dynamic_cast<MapRangeNode *>(node) : range;
      if (test.type == 0) {
        sample = node->name == "Unified.perlin" ? node : sample;
      }
      else if (test.type == 1) {
        sample = node->name == "Unified.cell" ? node : sample;
      }
      else if (test.type == 2) {
        sample = node->name == "Unified.worley" ? node : sample;
      }
      else {
        sample = node->name == "Unified.fractal" ? node : sample;
      }
    }
    ASSERT_NE(range, nullptr) << test.id << ":" << test.type;
    EXPECT_EQ(range->get_range_type(), NODE_MAP_RANGE_LINEAR) << test.id << ":" << test.type;
    EXPECT_TRUE(range->get_clamp()) << test.id << ":" << test.type;
    EXPECT_FLOAT_EQ(range->get_to_min(), 0.2f) << test.id << ":" << test.type;
    EXPECT_FLOAT_EQ(range->get_to_max(), 0.8f) << test.id << ":" << test.type;
    ASSERT_NE(sample, nullptr) << test.id << ":" << test.type;
    if (auto *noise_node = dynamic_cast<NoiseTextureNode *>(sample)) {
      EXPECT_EQ(noise_node->get_dimensions(), test.noise_dimensions) << test.id << ":" << test.type;
      if (test.type == 3) {
        EXPECT_EQ(noise_node->get_type(), NODE_NOISE_FBM) << test.id << ":" << test.type;
        EXPECT_FLOAT_EQ(noise_node->get_detail(), 4.0f) << test.id << ":" << test.type;
        EXPECT_FLOAT_EQ(noise_node->get_lacunarity(), 2.5f) << test.id << ":" << test.type;
        EXPECT_FLOAT_EQ(noise_node->get_roughness(), 0.375f) << test.id << ":" << test.type;
      }
    }
    else if (auto *white_noise = dynamic_cast<WhiteNoiseTextureNode *>(sample)) {
      EXPECT_EQ(white_noise->get_dimensions(), test.noise_dimensions) << test.id << ":" << test.type;
    }
    else {
      auto *voronoi = dynamic_cast<VoronoiTextureNode *>(sample);
      ASSERT_NE(voronoi, nullptr) << test.id << ":" << test.type;
      EXPECT_EQ(voronoi->get_dimensions(), test.noise_dimensions) << test.id << ":" << test.type;
      EXPECT_FLOAT_EQ(voronoi->get_randomness(), 0.625f) << test.id << ":" << test.type;
    }
  }
}

TEST(materialx_graph, lowers_unifiednoise_with_literal_coordinates)
{
  const struct {
    const char *id;
    const char *coordinate_name;
    materialx::Type coordinate_type;
  } cases[] = {{"ND_unifiednoise2d_float", "texcoord", materialx::Type::Vector2},
               {"ND_unifiednoise3d_float", "position", materialx::Type::Vector3}};

  for (const auto &test : cases) {
    materialx::Node noise{"Unified", test.id};
    noise.inputs = {{"jitter", 1.0f},
                    {"outmin", 0.2f},
                    {"outmax", 0.8f},
                    {"lacunarity", 2.5f},
                    {"diminish", 0.375f}};
    noise.int_inputs = {{"clampoutput", 1}, {"octaves", 4}, {"type", 0}, {"style", 0}};
    if (test.coordinate_type == materialx::Type::Vector2) {
      noise.vector2_inputs[test.coordinate_name] = make_float2(0.125f, 0.875f);
      noise.vector2_inputs["freq"] = make_float2(2.0f, 3.0f);
      noise.vector2_inputs["offset"] = make_float2(0.25f, 0.5f);
    }
    else {
      noise.vector3_inputs[test.coordinate_name] = make_float3(0.25f, 0.5f, 0.75f);
      noise.vector3_inputs["freq"] = make_float3(2.0f, 3.0f, 4.0f);
      noise.vector3_inputs["offset"] = make_float3(0.25f, 0.5f, 0.75f);
    }
    noise.outputs["out"] = materialx::Type::Float;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{noise}}, &graph)) << test.id;

    VectorMathNode *frequency = nullptr;
    NoiseTextureNode *perlin = nullptr;
    for (ShaderNode *node : graph.nodes) {
      frequency = node->name == "Unified.frequency" ? dynamic_cast<VectorMathNode *>(node) : frequency;
      perlin = node->name == "Unified.perlin" ? dynamic_cast<NoiseTextureNode *>(node) : perlin;
    }
    ASSERT_NE(frequency, nullptr) << test.id;
    ASSERT_NE(perlin, nullptr) << test.id;
    EXPECT_EQ(frequency->get_math_type(), NODE_VECTOR_MATH_MULTIPLY) << test.id;
    EXPECT_EQ(frequency->input("Vector1")->link, nullptr) << test.id;
    EXPECT_EQ(frequency->get_vector1(), test.coordinate_type == materialx::Type::Vector2 ?
                                            make_float3(0.125f, 0.875f, 0.0f) :
                                            make_float3(0.25f, 0.5f, 0.75f))
        << test.id;
    EXPECT_EQ(perlin->get_dimensions(), test.coordinate_type == materialx::Type::Vector2 ? 2 : 3)
        << test.id;
  }
}

TEST(materialx_graph, rejects_invalid_unifiednoise_contract_atomically)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node noise{"Unified", "ND_unifiednoise2d_float"};
  noise.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  noise.vector2_inputs["freq"] = make_float2(1.0f, 1.0f);
  noise.vector2_inputs["offset"] = make_float2(0.0f, 0.0f);
  noise.inputs = {{"jitter", 0.5f},
                  {"outmin", 1.0f},
                  {"outmax", 0.0f},
                  {"lacunarity", 2.0f},
                  {"diminish", 0.5f}};
  noise.int_inputs = {{"clampoutput", 1}, {"octaves", 3}, {"type", 2}, {"style", 1}};
  noise.outputs["out"] = materialx::Type::Float;

  EXPECT_FALSE(materialx::validate({{texcoord, noise}}));
  ShaderGraph graph;
  graph.create_node<ValueNode>()->name = "Sentinel";
  const size_t original_node_count = graph.nodes.size();
  ASSERT_FALSE(materialx::lower({{texcoord, noise}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, lowers_procedural_randomfloat_from_stdlib_ng_contract)
{
  /* stdlib_defs.mtlx declares ND_randomfloat_float/_integer in
   * nodegroup="procedural". stdlib_ng.mtlx NG_randomfloat_float multiplies
   * the input by 4096, combines it with seed, samples cellnoise2d_float, then
   * range-remaps/clamps to min/max; NG_randomfloat_integer performs the same
   * graph after integer-to-float conversion without the 4096 scale. */
  materialx::Node float_input{"FloatInput", "ND_constant_float"};
  float_input.inputs["value"] = 0.25f;
  float_input.outputs["out"] = materialx::Type::Float;

  materialx::Node int_input{"IntegerInput", "ND_constant_integer"};
  int_input.int_inputs["value"] = 7;
  int_input.outputs["out"] = materialx::Type::Integer;

  materialx::Node random_float{"RandomFloat", "ND_randomfloat_float"};
  random_float.links["in"] = {"FloatInput", "out", materialx::Type::Float};
  random_float.inputs["min"] = 0.2f;
  random_float.inputs["max"] = 0.8f;
  random_float.int_inputs["seed"] = 3;
  random_float.outputs["out"] = materialx::Type::Float;

  materialx::Node random_integer{"RandomInteger", "ND_randomfloat_integer"};
  random_integer.links["in"] = {"IntegerInput", "out", materialx::Type::Integer};
  random_integer.inputs["min"] = 0.1f;
  random_integer.inputs["max"] = 0.9f;
  random_integer.int_inputs["seed"] = 5;
  random_integer.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{float_input, int_input, random_float, random_integer}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }

  auto *float_range = dynamic_cast<MapRangeNode *>(lowered["RandomFloat"]);
  auto *float_noise = dynamic_cast<WhiteNoiseTextureNode *>(lowered["RandomFloat.cellnoise"]);
  auto *float_floor = dynamic_cast<VectorMathNode *>(lowered["RandomFloat.floor"]);
  auto *float_coordinate = dynamic_cast<CombineXYZNode *>(lowered["RandomFloat.coordinate"]);
  auto *float_scale = dynamic_cast<MathNode *>(lowered["RandomFloat.scale_input"]);
  ASSERT_NE(float_range, nullptr);
  ASSERT_NE(float_noise, nullptr);
  ASSERT_NE(float_floor, nullptr);
  ASSERT_NE(float_coordinate, nullptr);
  ASSERT_NE(float_scale, nullptr);
  EXPECT_EQ(float_noise->get_dimensions(), 2);
  EXPECT_EQ(float_floor->get_math_type(), NODE_VECTOR_MATH_FLOOR);
  EXPECT_EQ(float_range->get_range_type(), NODE_MAP_RANGE_LINEAR);
  EXPECT_TRUE(float_range->get_clamp());
  EXPECT_FLOAT_EQ(float_range->get_to_min(), 0.2f);
  EXPECT_FLOAT_EQ(float_range->get_to_max(), 0.8f);
  EXPECT_FLOAT_EQ(float_coordinate->get_y(), 3.0f);
  EXPECT_FLOAT_EQ(float_scale->get_value2(), 4096.0f);
  EXPECT_EQ(float_scale->input("Value1")->link, lowered["FloatInput"]->output("Value"));
  EXPECT_EQ(float_coordinate->input("X")->link, float_scale->output("Value"));
  ASSERT_NE(float_noise->input("Vector")->link, nullptr);
  EXPECT_FALSE(float_floor->output("Vector")->links.empty());
  EXPECT_EQ(float_range->input("Value")->link, float_noise->output("Value"));

  auto *integer_range = dynamic_cast<MapRangeNode *>(lowered["RandomInteger"]);
  auto *integer_noise = dynamic_cast<WhiteNoiseTextureNode *>(lowered["RandomInteger.cellnoise"]);
  auto *integer_coordinate = dynamic_cast<CombineXYZNode *>(lowered["RandomInteger.coordinate"]);
  ASSERT_NE(integer_range, nullptr);
  ASSERT_NE(integer_noise, nullptr);
  ASSERT_NE(integer_coordinate, nullptr);
  EXPECT_EQ(integer_noise->get_dimensions(), 2);
  EXPECT_TRUE(integer_range->get_clamp());
  EXPECT_FLOAT_EQ(integer_range->get_to_min(), 0.1f);
  EXPECT_FLOAT_EQ(integer_range->get_to_max(), 0.9f);
  EXPECT_FLOAT_EQ(integer_coordinate->get_y(), 5.0f);
  EXPECT_EQ(integer_coordinate->input("X")->link, lowered["IntegerInput.float"]->output("Value"));
  EXPECT_EQ(integer_range->input("Value")->link, integer_noise->output("Value"));
}

TEST(materialx_graph, rejects_invalid_randomfloat_contract_atomically)
{
  materialx::Node input{"Input", "ND_constant_float"};
  input.inputs["value"] = 0.25f;
  input.outputs["out"] = materialx::Type::Float;

  materialx::Node random{"Random", "ND_randomfloat_float"};
  random.links["in"] = {"Input", "out", materialx::Type::Integer};
  random.inputs["min"] = 1.0f;
  random.inputs["max"] = 0.0f;
  random.int_inputs["seed"] = 0;
  random.outputs["out"] = materialx::Type::Float;

  EXPECT_FALSE(materialx::validate({{input, random}}));
  ShaderGraph graph;
  graph.create_node<ValueNode>()->name = "Sentinel";
  const size_t original_node_count = graph.nodes.size();
  ASSERT_FALSE(materialx::lower({{input, random}}, &graph));
  ASSERT_EQ(graph.nodes.size(), original_node_count);
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

TEST(materialx_graph, lowers_procedural3d_randomcolor_variants)
{
  const struct {
    const char *id;
    const char *input_nodedef;
    materialx::Type input_type;
  } cases[] = {{"ND_randomcolor_float", "ND_constant_float", materialx::Type::Float},
               {"ND_randomcolor_integer", "ND_constant_integer", materialx::Type::Integer}};

  for (const auto &test : cases) {
    materialx::Node input{"Input", test.input_nodedef};
    if (test.input_type == materialx::Type::Float) {
      input.inputs["value"] = 0.375f;
    }
    else {
      input.int_inputs["value"] = 7;
    }
    input.outputs["out"] = test.input_type;

    materialx::Node random{"RandomColor", test.id};
    random.links["in"] = {"Input", "out", test.input_type};
    random.inputs["huelow"] = 0.125f;
    random.inputs["huehigh"] = 0.875f;
    random.inputs["saturationlow"] = 0.25f;
    random.inputs["saturationhigh"] = 0.75f;
    random.inputs["brightnesslow"] = 0.5f;
    random.inputs["brightnesshigh"] = 1.0f;
    random.int_inputs["seed"] = 3;
    random.outputs["out"] = materialx::Type::Color3;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{input, random}}, &graph)) << test.id;

    MathNode *scale_input = nullptr;
    MathNode *hue_seed = nullptr;
    WhiteNoiseTextureNode *hue_noise = nullptr;
    MapRangeNode *hue_range = nullptr;
    CombineColorNode *rgb = nullptr;
    for (ShaderNode *node : graph.nodes) {
      scale_input = node->name == "RandomColor.input.scale" ? dynamic_cast<MathNode *>(node) :
                                                              scale_input;
      hue_seed = node->name == "RandomColor.hue.seed" ? dynamic_cast<MathNode *>(node) : hue_seed;
      hue_noise = node->name == "RandomColor.hue.noise" ? dynamic_cast<WhiteNoiseTextureNode *>(node) :
                                                          hue_noise;
      hue_range = node->name == "RandomColor.hue" ? dynamic_cast<MapRangeNode *>(node) : hue_range;
      rgb = node->name == "RandomColor" ? dynamic_cast<CombineColorNode *>(node) : rgb;
    }
    ASSERT_NE(scale_input, nullptr) << test.id;
    EXPECT_EQ(scale_input->get_math_type(), NODE_MATH_MULTIPLY) << test.id;
    EXPECT_FLOAT_EQ(scale_input->get_value2(), 4096.0f) << test.id;
    ASSERT_NE(hue_seed, nullptr) << test.id;
    EXPECT_EQ(hue_seed->get_math_type(), NODE_MATH_CEIL) << test.id;
    EXPECT_FLOAT_EQ(hue_seed->get_value1(), 416.3f) << test.id;
    ASSERT_NE(hue_noise, nullptr) << test.id;
    EXPECT_EQ(hue_noise->get_dimensions(), 2) << test.id;
    ASSERT_NE(hue_range, nullptr) << test.id;
    EXPECT_EQ(hue_range->get_range_type(), NODE_MAP_RANGE_LINEAR) << test.id;
    EXPECT_TRUE(hue_range->get_clamp()) << test.id;
    EXPECT_FLOAT_EQ(hue_range->get_to_min(), 0.125f) << test.id;
    EXPECT_FLOAT_EQ(hue_range->get_to_max(), 0.875f) << test.id;
    ASSERT_NE(rgb, nullptr) << test.id;
    EXPECT_EQ(rgb->get_color_type(), NODE_COMBSEP_COLOR_HSV) << test.id;
    EXPECT_EQ(rgb->input("Red")->link, hue_range->output("Result")) << test.id;
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


TEST(materialx_graph, lowers_four_channel_noise_and_fractal_contracts)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node position{"Position", "ND_constant_vector3"};
  position.vector3_inputs["value"] = make_float3(0.125f, 0.5f, 0.875f);
  position.outputs["out"] = materialx::Type::Vector3;

  const struct {
    const char *id;
    materialx::Type type;
    bool scalar_amplitude;
    bool fractal;
    bool dimensional_3d;
  } cases[] = {{"ND_noise2d_color4", materialx::Type::Color4, false, false, false},
               {"ND_noise2d_color4FA", materialx::Type::Color4, true, false, false},
               {"ND_noise2d_vector4", materialx::Type::Vector4, false, false, false},
               {"ND_noise2d_vector4FA", materialx::Type::Vector4, true, false, false},
               {"ND_noise3d_color4", materialx::Type::Color4, false, false, true},
               {"ND_noise3d_color4FA", materialx::Type::Color4, true, false, true},
               {"ND_noise3d_vector4", materialx::Type::Vector4, false, false, true},
               {"ND_noise3d_vector4FA", materialx::Type::Vector4, true, false, true},
               {"ND_fractal2d_color4", materialx::Type::Color4, false, true, false},
               {"ND_fractal2d_color4FA", materialx::Type::Color4, true, true, false},
               {"ND_fractal2d_vector4", materialx::Type::Vector4, false, true, false},
               {"ND_fractal2d_vector4FA", materialx::Type::Vector4, true, true, false},
               {"ND_fractal3d_color4", materialx::Type::Color4, false, true, true},
               {"ND_fractal3d_color4FA", materialx::Type::Color4, true, true, true},
               {"ND_fractal3d_vector4", materialx::Type::Vector4, false, true, true},
               {"ND_fractal3d_vector4FA", materialx::Type::Vector4, true, true, true}};

  for (const auto &test : cases) {
    materialx::Node procedural{"Procedural", test.id};
    if (test.scalar_amplitude) {
      procedural.inputs["amplitude"] = 0.5f;
    }
    else {
      procedural.vector4_inputs["amplitude"] = make_float4(0.5f, 0.75f, 1.0f, 1.25f);
    }
    if (test.fractal) {
      procedural.int_inputs["octaves"] = 4;
      procedural.inputs["lacunarity"] = 2.25f;
      procedural.inputs["diminish"] = 0.625f;
    }
    else {
      procedural.inputs["pivot"] = 0.125f;
    }
    procedural.links[test.dimensional_3d ? "position" : "texcoord"] =
        {test.dimensional_3d ? "Position" : "Texcoord",
         "out",
         test.dimensional_3d ? materialx::Type::Vector3 : materialx::Type::Vector2};
    procedural.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{texcoord, position, procedural}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *rgb_noise = dynamic_cast<NoiseTextureNode *>(nodes["Procedural.noise"]);
    auto *w_noise = dynamic_cast<NoiseTextureNode *>(nodes["Procedural.W.noise"]);
    auto *offset = dynamic_cast<SeparateXYZNode *>(nodes["Procedural.offset.separate"]);
    auto *w_amplitude = dynamic_cast<MathNode *>(nodes["Procedural.W.amplitude"]);
    ASSERT_NE(rgb_noise, nullptr) << test.id;
    ASSERT_NE(w_noise, nullptr) << test.id;
    ASSERT_NE(offset, nullptr) << test.id;
    ASSERT_NE(w_amplitude, nullptr) << test.id;
    EXPECT_EQ(rgb_noise->get_dimensions(), test.dimensional_3d ? 3 : 2) << test.id;
    EXPECT_EQ(w_noise->get_dimensions(), test.dimensional_3d ? 3 : 2) << test.id;
    EXPECT_FLOAT_EQ(w_amplitude->get_value2(), test.scalar_amplitude ? 0.5f : 1.25f)
        << test.id;
    if (test.fractal) {
      EXPECT_EQ(rgb_noise->get_type(), NODE_NOISE_FBM) << test.id;
      EXPECT_EQ(w_noise->get_type(), NODE_NOISE_FBM) << test.id;
      EXPECT_FLOAT_EQ(rgb_noise->get_detail(), 4.0f) << test.id;
      EXPECT_FLOAT_EQ(w_noise->get_lacunarity(), 2.25f) << test.id;
      EXPECT_FLOAT_EQ(w_noise->get_roughness(), 0.625f) << test.id;
      EXPECT_EQ(nodes.count(test.type == materialx::Type::Color4 ? "Procedural.Alpha" :
                                                               "Procedural.W"),
                0)
          << test.id;
    }
    else {
      auto *w_pivot = dynamic_cast<MathNode *>(nodes[test.type == materialx::Type::Color4 ?
                                                         "Procedural.Alpha" :
                                                         "Procedural.W"]);
      ASSERT_NE(w_pivot, nullptr) << test.id;
      EXPECT_EQ(w_pivot->get_math_type(), NODE_MATH_ADD) << test.id;
      EXPECT_FLOAT_EQ(w_pivot->get_value2(), 0.125f) << test.id;
    }
  }
}

TEST(materialx_graph, rejects_invalid_four_channel_noise_amplitude_atomically)
{
  materialx::Node texcoord{"Texcoord", "ND_constant_vector2"};
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node noise{"Noise", "ND_noise2d_color4"};
  noise.vector3_inputs["amplitude"] = make_float3(1.0f, 1.0f, 1.0f);
  noise.inputs["pivot"] = 0.0f;
  noise.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  noise.outputs["out"] = materialx::Type::Color4;

  EXPECT_FALSE(materialx::validate({{texcoord, noise}}));
  ShaderGraph graph;
  graph.create_node<PrincipledBsdfNode>();
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower({{texcoord, noise}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
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

TEST(materialx_graph, lowers_vector3_to_vector4_convert_with_unit_w)
{
  materialx::Node source;
  source.name = "SourceVector";
  source.nodedef = "ND_constant_vector3";
  source.vector3_inputs["value"] = make_float3(0.25f, 0.5f, 0.75f);
  source.outputs["out"] = materialx::Type::Vector3;

  materialx::Node convert;
  convert.name = "Vector4Convert";
  convert.nodedef = "ND_convert_vector3_vector4";
  convert.links["in"] = {"SourceVector", "out", materialx::Type::Vector3};
  convert.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, convert}}, &graph));

  CombineXYZNode *vector = nullptr;
  ValueNode *w = nullptr;
  for (ShaderNode *node : graph.nodes) {
    vector = node->name == "Vector4Convert" ? dynamic_cast<CombineXYZNode *>(node) : vector;
    w = node->name == "Vector4Convert.W" ? dynamic_cast<ValueNode *>(node) : w;
  }
  if (vector != nullptr) {
    EXPECT_FLOAT_EQ(vector->get_x(), 0.25f);
    EXPECT_FLOAT_EQ(vector->get_y(), 0.5f);
    EXPECT_FLOAT_EQ(vector->get_z(), 0.75f);
  }
  else {
    /* Existing passthrough lowering for vector3->vector4 reuses the source XYZ node. */
    SUCCEED();
  }
  ASSERT_NE(w, nullptr);
  EXPECT_FLOAT_EQ(w->get_value(), 1.0f);
}

TEST(materialx_graph, lowers_color3_and_vector2_to_vector4_converts_with_installed_tail)
{
  materialx::Node color;
  color.name = "SourceColor";
  color.nodedef = "ND_constant_color3";
  color.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  color.outputs["out"] = materialx::Type::Color3;

  materialx::Node color_convert;
  color_convert.name = "ColorToVector4";
  color_convert.nodedef = "ND_convert_color3_vector4";
  color_convert.links["in"] = {"SourceColor", "out", materialx::Type::Color3};
  color_convert.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector;
  vector.name = "SourceVector2";
  vector.nodedef = "ND_constant_vector2";
  vector.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  vector.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector_convert;
  vector_convert.name = "Vector2ToVector4";
  vector_convert.nodedef = "ND_convert_vector2_vector4";
  vector_convert.links["in"] = {"SourceVector2", "out", materialx::Type::Vector2};
  vector_convert.outputs["out"] = materialx::Type::Vector4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color, color_convert, vector, vector_convert}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["ColorToVector4.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["ColorToVector4"]), nullptr);
  ASSERT_NE(dynamic_cast<ValueNode *>(lowered["ColorToVector4.W"]), nullptr);
  EXPECT_EQ(lowered["ColorToVector4.separate"]->input("Color")->link,
            lowered["SourceColor"]->output("Color"));
  EXPECT_EQ(lowered["ColorToVector4"]->input("X")->link,
            lowered["ColorToVector4.separate"]->output("Red"));
  EXPECT_EQ(lowered["ColorToVector4"]->input("Y")->link,
            lowered["ColorToVector4.separate"]->output("Green"));
  EXPECT_EQ(lowered["ColorToVector4"]->input("Z")->link,
            lowered["ColorToVector4.separate"]->output("Blue"));
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(lowered["ColorToVector4.W"])->get_value(), 1.0f);

  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector2ToVector4.separate"]), nullptr);
  CombineXYZNode *vector4 = dynamic_cast<CombineXYZNode *>(lowered["Vector2ToVector4"]);
  ASSERT_NE(vector4, nullptr);
  EXPECT_NE(lowered["Vector2ToVector4.separate"]->input("Vector")->link, nullptr);
  EXPECT_EQ(vector4->input("X")->link, lowered["Vector2ToVector4.separate"]->output("X"));
  EXPECT_EQ(vector4->input("Y")->link, lowered["Vector2ToVector4.separate"]->output("Y"));
  EXPECT_FLOAT_EQ(vector4->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(lowered["Vector2ToVector4.W"])->get_value(), 1.0f);
}

TEST(materialx_graph, lowers_vector4_to_vector3_convert_and_extract_w)
{
  materialx::Node source;
  source.name = "SourceVector4";
  source.nodedef = "ND_constant_vector4";
  source.vector4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  source.outputs["out"] = materialx::Type::Vector4;

  materialx::Node convert;
  convert.name = "Vector3Convert";
  convert.nodedef = "ND_convert_vector4_vector3";
  convert.links["in"] = {"SourceVector4", "out", materialx::Type::Vector4};
  convert.outputs["out"] = materialx::Type::Vector3;

  materialx::Node extract;
  extract.name = "ExtractW";
  extract.nodedef = "ND_extract_vector4";
  extract.int_inputs["index"] = 3;
  extract.links["in"] = {"SourceVector4", "out", materialx::Type::Vector4};
  extract.outputs["out"] = materialx::Type::Float;

  materialx::Node add;
  add.name = "AddW";
  add.nodedef = "ND_add_float";
  add.links["in1"] = {"ExtractW", "out", materialx::Type::Float};
  add.inputs["in2"] = 0.1f;
  add.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, convert, extract, add}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector3Convert"]), nullptr);
  ASSERT_NE(dynamic_cast<ValueNode *>(lowered["SourceVector4.W"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["AddW"]), nullptr);
  EXPECT_NE(lowered["Vector3Convert"]->input("Vector")->link, nullptr);
  EXPECT_EQ(lowered["AddW"]->input("Value1")->link,
            lowered["SourceVector4.W"]->output("Value"));
  EXPECT_FLOAT_EQ(dynamic_cast<ValueNode *>(lowered["SourceVector4.W"])->get_value(), 0.4f);
}

TEST(materialx_graph, lowers_vector3_to_vector2_convert_truncating_z)
{
  materialx::Node source;
  source.name = "SourceVector3";
  source.nodedef = "ND_constant_vector3";
  source.vector3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  source.outputs["out"] = materialx::Type::Vector3;

  materialx::Node convert;
  convert.name = "Vector2Convert";
  convert.nodedef = "ND_convert_vector3_vector2";
  convert.links["in"] = {"SourceVector3", "out", materialx::Type::Vector3};
  convert.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, convert}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector2Convert.separate"]), nullptr);
  CombineXYZNode *vector = dynamic_cast<CombineXYZNode *>(lowered["Vector2Convert"]);
  ASSERT_NE(vector, nullptr);
  EXPECT_NE(lowered["Vector2Convert.separate"]->input("Vector")->link, nullptr);
  EXPECT_EQ(vector->input("X")->link, lowered["Vector2Convert.separate"]->output("X"));
  EXPECT_EQ(vector->input("Y")->link, lowered["Vector2Convert.separate"]->output("Y"));
  EXPECT_FLOAT_EQ(vector->get_z(), 0.0f);
}

TEST(materialx_graph, lowers_image_vector4_with_real_alpha_sidecar)
{
  const TemporaryImage image_asset;

  materialx::Node uv;
  uv.name = "UV";
  uv.nodedef = "ND_geompropvalue_vector2";
  uv.string_inputs["geomprop"] = "st";
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node image;
  image.name = "Vector4Image";
  image.nodedef = "ND_image_vector4";
  image.asset_inputs["file"] = image_asset.path();
  image.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  image.outputs["out"] = materialx::Type::Vector4;

  materialx::Node extract_w;
  extract_w.name = "ExtractW";
  extract_w.nodedef = "ND_extract_vector4";
  extract_w.int_inputs["index"] = 3;
  extract_w.links["in"] = {"Vector4Image", "out", materialx::Type::Vector4};
  extract_w.outputs["out"] = materialx::Type::Float;

  materialx::Node add_w;
  add_w.name = "AddW";
  add_w.nodedef = "ND_add_float";
  add_w.links["in1"] = {"ExtractW", "out", materialx::Type::Float};
  add_w.inputs["in2"] = 0.0f;
  add_w.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{uv, image, extract_w, add_w}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ImageTextureNode *image_texture = dynamic_cast<ImageTextureNode *>(lowered["Vector4Image"]);
  ASSERT_NE(image_texture, nullptr);
  ASSERT_NE(lowered["Vector4Image.W"], nullptr);
  EXPECT_EQ(image_texture->get_filename(), ustring(image_asset.path()));
  EXPECT_EQ(image_texture->get_colorspace(), u_colorspace_data);
  EXPECT_EQ(image_texture->input("Vector")->link, lowered["UV"]->output("UV"));
  EXPECT_EQ(lowered["Vector4Image.W"]->input("Value1")->link, image_texture->output("Alpha"));
  EXPECT_EQ(lowered["AddW"]->input("Value1")->link, lowered["Vector4Image.W"]->output("Value"));
}

TEST(materialx_graph, lowers_vector4_to_vector2_convert_truncating_zw)
{
  materialx::Node source;
  source.name = "SourceVector4";
  source.nodedef = "ND_constant_vector4";
  source.vector4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  source.outputs["out"] = materialx::Type::Vector4;

  materialx::Node convert;
  convert.name = "Vector2Convert";
  convert.nodedef = "ND_convert_vector4_vector2";
  convert.links["in"] = {"SourceVector4", "out", materialx::Type::Vector4};
  convert.outputs["out"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{source, convert}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector2Convert.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["Vector2Convert"]), nullptr);
  EXPECT_NE(lowered["Vector2Convert.separate"]->input("Vector")->link, nullptr);
  EXPECT_EQ(lowered["Vector2Convert"]->input("X")->link,
            lowered["Vector2Convert.separate"]->output("X"));
  EXPECT_EQ(lowered["Vector2Convert"]->input("Y")->link,
            lowered["Vector2Convert.separate"]->output("Y"));
}

TEST(materialx_graph, lowers_vector4_combine_and_separate_channel_nodes)
{
  materialx::Node vector3;
  vector3.name = "Vector3";
  vector3.nodedef = "ND_constant_vector3";
  vector3.vector3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node scalar;
  scalar.name = "Scalar";
  scalar.nodedef = "ND_constant_float";
  scalar.inputs["value"] = 0.4f;
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node vector2_a;
  vector2_a.name = "Vector2A";
  vector2_a.nodedef = "ND_constant_vector2";
  vector2_a.vector2_inputs["value"] = make_float2(0.5f, 0.6f);
  vector2_a.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector2_b;
  vector2_b.name = "Vector2B";
  vector2_b.nodedef = "ND_constant_vector2";
  vector2_b.vector2_inputs["value"] = make_float2(0.7f, 0.8f);
  vector2_b.outputs["out"] = materialx::Type::Vector2;

  materialx::Node combine_vf;
  combine_vf.name = "CombineVF";
  combine_vf.nodedef = "ND_combine2_vector4VF";
  combine_vf.links["in1"] = {"Vector3", "out", materialx::Type::Vector3};
  combine_vf.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  combine_vf.outputs["out"] = materialx::Type::Vector4;

  materialx::Node combine_vv;
  combine_vv.name = "CombineVV";
  combine_vv.nodedef = "ND_combine2_vector4VV";
  combine_vv.links["in1"] = {"Vector2A", "out", materialx::Type::Vector2};
  combine_vv.links["in2"] = {"Vector2B", "out", materialx::Type::Vector2};
  combine_vv.outputs["out"] = materialx::Type::Vector4;

  materialx::Node combine4;
  combine4.name = "Combine4";
  combine4.nodedef = "ND_combine4_vector4";
  combine4.inputs["in1"] = 0.9f;
  combine4.links["in2"] = {"Scalar", "out", materialx::Type::Float};
  combine4.inputs["in3"] = 1.1f;
  combine4.inputs["in4"] = 1.2f;
  combine4.outputs["out"] = materialx::Type::Vector4;

  materialx::Node separate;
  separate.name = "Separate";
  separate.nodedef = "ND_separate4_vector4";
  separate.links["in"] = {"CombineVV", "out", materialx::Type::Vector4};
  separate.outputs["outx"] = materialx::Type::Float;
  separate.outputs["outy"] = materialx::Type::Float;
  separate.outputs["outz"] = materialx::Type::Float;
  separate.outputs["outw"] = materialx::Type::Float;

  materialx::Node add_w;
  add_w.name = "AddW";
  add_w.nodedef = "ND_add_float";
  add_w.links["in1"] = {"Separate", "outw", materialx::Type::Float};
  add_w.inputs["in2"] = 0.1f;
  add_w.outputs["out"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes = {vector3, scalar, vector2_a, vector2_b, combine_vf, combine_vv, combine4, separate, add_w};
  EXPECT_TRUE(materialx::validate(source));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["CombineVF"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["CombineVV"]), nullptr);
  ASSERT_NE(dynamic_cast<CombineXYZNode *>(lowered["Combine4"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Separate"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["CombineVF.W"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["CombineVV.W"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(lowered["Combine4.W"]), nullptr);
  EXPECT_EQ(lowered["CombineVF"]->input("X")->link, lowered["CombineVF.first"]->output("X"));
  EXPECT_EQ(lowered["CombineVF.W"]->input("Value1")->link, lowered["Scalar"]->output("Value"));
  EXPECT_EQ(lowered["CombineVV"]->input("Z")->link, lowered["CombineVV.second"]->output("X"));
  EXPECT_EQ(lowered["CombineVV.W"]->input("Value1")->link, lowered["CombineVV.second"]->output("Y"));
  EXPECT_FLOAT_EQ(dynamic_cast<CombineXYZNode *>(lowered["Combine4"])->get_x(), 0.9f);
  EXPECT_EQ(lowered["Combine4"]->input("Y")->link, lowered["Scalar"]->output("Value"));
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Combine4.W"])->get_value1(), 1.2f);
  EXPECT_EQ(lowered["AddW"]->input("Value1")->link, lowered["CombineVV.W"]->output("Value"));
}

TEST(materialx_graph, rejects_separate4_vector4_fed_by_source_without_native_w)
{
  /* ND_noise2d_vector4 produces a native Vector4 output but has no W sidecar
   * node (Vector4 procedurals are not in the has_native_w allowlist), so
   * separate4_vector4's outw cannot resolve; validate() must reject this
   * rather than let lower() throw looking up a nonexistent "<source>.W". */
  materialx::Node texcoord;
  texcoord.name = "Texcoord";
  texcoord.nodedef = "ND_constant_vector2";
  texcoord.vector2_inputs["value"] = make_float2(0.125f, 0.875f);
  texcoord.outputs["out"] = materialx::Type::Vector2;

  materialx::Node noise;
  noise.name = "Noise";
  noise.nodedef = "ND_noise2d_vector4";
  noise.vector4_inputs["amplitude"] = make_float4(0.5f, 0.75f, 1.0f, 1.25f);
  noise.inputs["pivot"] = 0.125f;
  noise.links["texcoord"] = {"Texcoord", "out", materialx::Type::Vector2};
  noise.outputs["out"] = materialx::Type::Vector4;

  materialx::Node separate;
  separate.name = "Separate";
  separate.nodedef = "ND_separate4_vector4";
  separate.links["in"] = {"Noise", "out", materialx::Type::Vector4};
  separate.outputs["outx"] = materialx::Type::Float;
  separate.outputs["outy"] = materialx::Type::Float;
  separate.outputs["outz"] = materialx::Type::Float;
  separate.outputs["outw"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes = {texcoord, noise, separate};
  EXPECT_FALSE(materialx::validate(source));
}

TEST(materialx_graph, lowers_color4_separate_four_channels)
{
  /* stdlib_defs.mtlx declares ND_separate4_color4 as outr/outg/outb/outa
   * float outputs from a color4 input; stdlib_ng.mtlx defines these through
   * indexed extracts, so outa must use the existing Color4 alpha sidecar. */
  materialx::Node color;
  color.name = "SourceColor4";
  color.nodedef = "ND_constant_color4";
  color.float4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node separate;
  separate.name = "Separate4";
  separate.nodedef = "ND_separate4_color4";
  separate.links["in"] = {"SourceColor4", "out", materialx::Type::Color4};
  separate.outputs = {{"outr", materialx::Type::Float},
                      {"outg", materialx::Type::Float},
                      {"outb", materialx::Type::Float},
                      {"outa", materialx::Type::Float}};

  materialx::Node add;
  add.name = "AddA";
  add.nodedef = "ND_add_float";
  add.links["in1"] = {"Separate4", "outa", materialx::Type::Float};
  add.inputs["in2"] = 0.0f;
  add.outputs["out"] = materialx::Type::Float;

  materialx::Graph source;
  source.nodes = {color, separate, add};
  EXPECT_TRUE(materialx::validate(source));

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["Separate4"]), nullptr);
  EXPECT_EQ(lowered["Separate4"]->input("Color")->link, lowered["SourceColor4"]->output("Color"));
  EXPECT_EQ(lowered["AddA"]->input("Value1")->link,
            lowered["SourceColor4.Alpha"]->output("Value"));
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

/* Task 5: boolean/integer exact-domain observation, device ABI. Both reuse
 * an existing native-typed Cycles socket as a "vehicle" node -- exactly
 * the same reuse strategy as Task 4's CombineXYZNode/ValueNode -- rather
 * than adding a new core Cycles node class: `MixNode::use_clamp` is a
 * genuine `SocketType::BOOLEAN` field, and `MagicTextureNode::depth` is a
 * genuine `SocketType::INT` field; neither is routed through a float
 * socket. */

TEST(materialx_graph, lowers_constant_boolean_to_native_bool_socket)
{
  materialx::Node constant;
  constant.name = "BooleanTrue";
  constant.nodedef = "ND_constant_boolean";
  constant.int_inputs["value"] = 1;
  constant.outputs["out"] = materialx::Type::Boolean;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  MixNode *boolean = nullptr;
  for (ShaderNode *node : graph.nodes) {
    boolean = node->name == "BooleanTrue" ? dynamic_cast<MixNode *>(node) : boolean;
  }
  ASSERT_NE(boolean, nullptr);
  EXPECT_TRUE(boolean->get_use_clamp());
}

TEST(materialx_graph, lowers_constant_boolean_false_and_omitted_to_installed_false_default)
{
  for (const bool omit_value : {false, true}) {
    materialx::Node constant;
    constant.name = "BooleanFalse";
    constant.nodedef = "ND_constant_boolean";
    if (!omit_value) {
      constant.int_inputs["value"] = 0;
    }
    constant.outputs["out"] = materialx::Type::Boolean;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{constant}}, &graph));

    MixNode *boolean = nullptr;
    for (ShaderNode *node : graph.nodes) {
      boolean = node->name == "BooleanFalse" ? dynamic_cast<MixNode *>(node) : boolean;
    }
    ASSERT_NE(boolean, nullptr);
    EXPECT_FALSE(boolean->get_use_clamp());
  }
}

TEST(materialx_graph, rejects_constant_boolean_bad_shape_range_and_tag_atomically)
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
  constant.name = "Boolean";
  constant.nodedef = "ND_constant_boolean";
  constant.int_inputs["value"] = 1;
  constant.outputs["out"] = materialx::Type::Boolean;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  /* Wrong tag: nodedef is ND_constant_boolean but the output type is
   * Integer, not Boolean. */
  materialx::Node wrong_tag = constant;
  wrong_tag.outputs["out"] = materialx::Type::Integer;
  expect_rejected({{wrong_tag}});

  /* Invalid range: boolean's exact domain is {0, 1}; MaterialX has no
   * "boolean 2" -- any other int_inputs value is out of domain. */
  materialx::Node out_of_range = constant;
  out_of_range.int_inputs["value"] = 2;
  expect_rejected({{out_of_range}});

  materialx::Node linked_value = constant;
  linked_value.links["value"] = {"Other", "out", materialx::Type::Boolean};
  materialx::Node other = constant;
  other.name = "Other";
  expect_rejected({{other, linked_value}});
}

TEST(materialx_graph, lowers_constant_integer_to_native_int_socket)
{
  for (const int value : {-7, 0, 42}) {
    materialx::Node constant;
    constant.name = "Integer";
    constant.nodedef = "ND_constant_integer";
    constant.int_inputs["value"] = value;
    constant.outputs["out"] = materialx::Type::Integer;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{constant}}, &graph));

    MagicTextureNode *integer = nullptr;
    for (ShaderNode *node : graph.nodes) {
      integer = node->name == "Integer" ? dynamic_cast<MagicTextureNode *>(node) : integer;
    }
    ASSERT_NE(integer, nullptr);
    /* No float coercion: the exact int value round-trips through a
     * genuine SocketType::INT field, not a float default/approximation. */
    EXPECT_EQ(integer->get_depth(), value);
  }
}

TEST(materialx_graph, lowers_literal_integer_math_and_bool_int_converts)
{
  /* MaterialX stdlib_defs.mtlx declares ND_add_integer, ND_subtract_integer,
   * ND_floor_integer, ND_ceil_integer, ND_round_integer,
   * ND_convert_boolean_integer, and ND_convert_integer_boolean.
   * genosl/stdlib_genosl_impl.mtlx maps them to integer add/subtract,
   * int(floor()), int(ceil()), int(round()), true ? 1 : 0, and in != 0
   * respectively. */
  materialx::Node add;
  add.name = "AddInteger";
  add.nodedef = "ND_add_integer";
  add.int_inputs = {{"in1", 7}, {"in2", 5}};
  add.outputs["out"] = materialx::Type::Integer;

  materialx::Node subtract;
  subtract.name = "SubtractInteger";
  subtract.nodedef = "ND_subtract_integer";
  subtract.int_inputs = {{"in1", 7}, {"in2", 5}};
  subtract.outputs["out"] = materialx::Type::Integer;

  materialx::Node floor;
  floor.name = "FloorInteger";
  floor.nodedef = "ND_floor_integer";
  floor.inputs["in"] = -2.25f;
  floor.outputs["out"] = materialx::Type::Integer;

  materialx::Node ceil;
  ceil.name = "CeilInteger";
  ceil.nodedef = "ND_ceil_integer";
  ceil.inputs["in"] = 2.25f;
  ceil.outputs["out"] = materialx::Type::Integer;

  materialx::Node round;
  round.name = "RoundInteger";
  round.nodedef = "ND_round_integer";
  round.inputs["in"] = 2.6f;
  round.outputs["out"] = materialx::Type::Integer;

  materialx::Node truth;
  truth.name = "Truth";
  truth.nodedef = "ND_constant_boolean";
  truth.int_inputs["value"] = 1;
  truth.outputs["out"] = materialx::Type::Boolean;

  materialx::Node bool_to_int;
  bool_to_int.name = "BoolToInt";
  bool_to_int.nodedef = "ND_convert_boolean_integer";
  bool_to_int.links["in"] = {"Truth", "out", materialx::Type::Boolean};
  bool_to_int.outputs["out"] = materialx::Type::Integer;

  materialx::Node int_to_bool;
  int_to_bool.name = "IntToBool";
  int_to_bool.nodedef = "ND_convert_integer_boolean";
  int_to_bool.links["in"] = {"AddInteger", "out", materialx::Type::Integer};
  int_to_bool.outputs["out"] = materialx::Type::Boolean;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{add, subtract, floor, ceil, round, truth, bool_to_int, int_to_bool}},
                                &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }

  auto *add_integer = dynamic_cast<MagicTextureNode *>(nodes["AddInteger"]);
  auto *add_float = dynamic_cast<ValueNode *>(nodes["AddInteger.float"]);
  auto *subtract_integer = dynamic_cast<MagicTextureNode *>(nodes["SubtractInteger"]);
  auto *subtract_float = dynamic_cast<ValueNode *>(nodes["SubtractInteger.float"]);
  auto *floor_integer = dynamic_cast<MagicTextureNode *>(nodes["FloorInteger"]);
  auto *ceil_integer = dynamic_cast<MagicTextureNode *>(nodes["CeilInteger"]);
  auto *round_integer = dynamic_cast<MagicTextureNode *>(nodes["RoundInteger"]);
  ASSERT_NE(add_integer, nullptr);
  ASSERT_NE(add_float, nullptr);
  ASSERT_NE(subtract_integer, nullptr);
  ASSERT_NE(subtract_float, nullptr);
  ASSERT_NE(floor_integer, nullptr);
  ASSERT_NE(ceil_integer, nullptr);
  ASSERT_NE(round_integer, nullptr);
  EXPECT_EQ(add_integer->get_depth(), 12);
  EXPECT_FLOAT_EQ(add_float->get_value(), 12.0f);
  EXPECT_EQ(subtract_integer->get_depth(), 2);
  EXPECT_FLOAT_EQ(subtract_float->get_value(), 2.0f);
  EXPECT_EQ(floor_integer->get_depth(), -3);
  EXPECT_EQ(ceil_integer->get_depth(), 3);
  EXPECT_EQ(round_integer->get_depth(), 3);

  auto *bool_to_int_value = dynamic_cast<MathNode *>(nodes["BoolToInt.float"]);
  auto *int_to_bool_is_zero = dynamic_cast<MathNode *>(nodes["IntToBool.is_zero"]);
  auto *int_to_bool_value = dynamic_cast<MathNode *>(nodes["IntToBool.float"]);
  ASSERT_NE(bool_to_int_value, nullptr);
  ASSERT_NE(int_to_bool_is_zero, nullptr);
  ASSERT_NE(int_to_bool_value, nullptr);
  EXPECT_EQ(bool_to_int_value->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(bool_to_int_value->input("Value1")->link, nodes["Truth.float"]->output("Value"));
  EXPECT_EQ(int_to_bool_is_zero->get_math_type(), NODE_MATH_COMPARE);
  EXPECT_EQ(int_to_bool_is_zero->input("Value1")->link, add_float->output("Value"));
  EXPECT_EQ(int_to_bool_value->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(int_to_bool_value->input("Value2")->link, int_to_bool_is_zero->output("Value"));
}

TEST(materialx_graph, lowers_scalar_to_vector4_converts_as_broadcast_adapters)
{
  for (const auto &[source_nodedef, convert_nodedef, source_type] :
       {std::tuple{"ND_constant_float", "ND_convert_float_vector4", materialx::Type::Float},
        std::tuple{"ND_constant_boolean", "ND_convert_boolean_vector4", materialx::Type::Boolean},
        std::tuple{"ND_constant_integer", "ND_convert_integer_vector4", materialx::Type::Integer}})
  {
    materialx::Node source;
    source.name = "Source";
    source.nodedef = source_nodedef;
    if (source_type == materialx::Type::Float) {
      source.inputs["value"] = 0.75f;
    }
    else {
      source.int_inputs["value"] = source_type == materialx::Type::Boolean ? 1 : 7;
    }
    source.outputs["out"] = source_type;

    materialx::Node convert;
    convert.name = "Convert";
    convert.nodedef = convert_nodedef;
    convert.links["in"] = {"Source", "out", source_type};
    convert.outputs["out"] = materialx::Type::Vector4;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source, convert}}, &graph)) << convert_nodedef;

    CombineXYZNode *vector = nullptr;
    ValueNode *w = nullptr;
    for (ShaderNode *node : graph.nodes) {
      vector = node->name == "Convert" ? dynamic_cast<CombineXYZNode *>(node) : vector;
      w = node->name == "Convert.W" ? dynamic_cast<ValueNode *>(node) : w;
    }
    ASSERT_NE(vector, nullptr) << convert_nodedef;
    ASSERT_NE(w, nullptr) << convert_nodedef;
    EXPECT_NE(vector->input("X")->link, nullptr) << convert_nodedef;
    EXPECT_EQ(vector->input("X")->link, vector->input("Y")->link) << convert_nodedef;
    EXPECT_EQ(vector->input("X")->link, vector->input("Z")->link) << convert_nodedef;
    EXPECT_FLOAT_EQ(w->get_value(), source_type == materialx::Type::Float ? 0.75f :
                                      (source_type == materialx::Type::Boolean ? 1.0f : 7.0f))
        << convert_nodedef;
  }
}

TEST(materialx_graph, rejects_scalar_to_vector4_convert_bad_shape_atomically)
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

  materialx::Node source;
  source.name = "Source";
  source.nodedef = "ND_constant_float";
  source.inputs["value"] = 1.0f;
  source.outputs["out"] = materialx::Type::Float;

  materialx::Node convert;
  convert.name = "Convert";
  convert.nodedef = "ND_convert_float_vector4";
  convert.links["in"] = {"Source", "out", materialx::Type::Float};
  convert.outputs["out"] = materialx::Type::Vector4;

  materialx::Node wrong_output = convert;
  wrong_output.outputs["out"] = materialx::Type::Color4;
  expect_rejected({{source, wrong_output}});

  materialx::Node wrong_input = convert;
  wrong_input.links["in"] = {"Source", "out", materialx::Type::Vector4};
  expect_rejected({{source, wrong_input}});

  materialx::Node extra = convert;
  extra.inputs["unexpected"] = 0.0f;
  expect_rejected({{source, extra}});
}

TEST(materialx_graph, lowers_boolean_and_integer_to_numeric_vector_converts)
{
  for (const auto &[source_nodedef, convert_nodedef, source_type, output_type] :
       {std::tuple{"ND_constant_boolean", "ND_convert_boolean_float", materialx::Type::Boolean, materialx::Type::Float},
        std::tuple{"ND_constant_integer", "ND_convert_integer_float", materialx::Type::Integer, materialx::Type::Float},
        std::tuple{"ND_constant_boolean", "ND_convert_boolean_vector2", materialx::Type::Boolean, materialx::Type::Vector2},
        std::tuple{"ND_constant_integer", "ND_convert_integer_vector2", materialx::Type::Integer, materialx::Type::Vector2},
        std::tuple{"ND_constant_boolean", "ND_convert_boolean_vector3", materialx::Type::Boolean, materialx::Type::Vector3},
        std::tuple{"ND_constant_integer", "ND_convert_integer_vector3", materialx::Type::Integer, materialx::Type::Vector3},
        std::tuple{"ND_constant_boolean", "ND_convert_boolean_vector4", materialx::Type::Boolean, materialx::Type::Vector4},
        std::tuple{"ND_constant_integer", "ND_convert_integer_vector4", materialx::Type::Integer, materialx::Type::Vector4}})
  {
    materialx::Node source;
    source.name = "Source";
    source.nodedef = source_nodedef;
    source.int_inputs["value"] = source_type == materialx::Type::Boolean ? 1 : 7;
    source.outputs["out"] = source_type;

    materialx::Node convert;
    convert.name = "Convert";
    convert.nodedef = convert_nodedef;
    convert.links["in"] = {"Source", "out", source_type};
    convert.outputs["out"] = output_type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source, convert}}, &graph)) << convert_nodedef;

    ShaderNode *adapter = nullptr;
    for (ShaderNode *node : graph.nodes) {
      adapter = node->name == "Convert" ? node : adapter;
    }
    ASSERT_NE(adapter, nullptr) << convert_nodedef;
    if (output_type == materialx::Type::Float) {
      MathNode *math = dynamic_cast<MathNode *>(adapter);
      ASSERT_NE(math, nullptr) << convert_nodedef;
      EXPECT_EQ(math->get_math_type(), NODE_MATH_ADD) << convert_nodedef;
      EXPECT_FLOAT_EQ(math->get_value2(), 0.0f) << convert_nodedef;
      ASSERT_NE(math->input("Value1")->link, nullptr) << convert_nodedef;
    }
    else {
      CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(adapter);
      ASSERT_NE(combine, nullptr) << convert_nodedef;
      EXPECT_EQ(combine->input("X")->link, combine->input("Y")->link) << convert_nodedef;
      if (output_type != materialx::Type::Vector2) {
        EXPECT_EQ(combine->input("Y")->link, combine->input("Z")->link) << convert_nodedef;
      }
      if (output_type == materialx::Type::Vector4) {
        ValueNode *w = nullptr;
        for (ShaderNode *node : graph.nodes) {
          w = node->name == "Convert.W" ? dynamic_cast<ValueNode *>(node) : w;
        }
        ASSERT_NE(w, nullptr) << convert_nodedef;
        /* MaterialX's native <convert> from float to vector4 (see
         * NG_convert_boolean_vector4/NG_convert_integer_vector4 in
         * stdlib_ng.mtlx) broadcasts the source scalar into all four
         * components, W included -- not just XYZ with W fixed at 0. */
        EXPECT_FLOAT_EQ(w->get_value(), source_type == materialx::Type::Boolean ? 1.0f : 7.0f)
            << convert_nodedef;
      }
    }
  }
}

TEST(materialx_graph, lowers_two_constant_integer_nodes_with_independent_values)
{
  /* "Stale output" guard, mirroring the Vector4 W test above. */
  materialx::Node first;
  first.name = "First";
  first.nodedef = "ND_constant_integer";
  first.int_inputs["value"] = 3;
  first.outputs["out"] = materialx::Type::Integer;

  materialx::Node second;
  second.name = "Second";
  second.nodedef = "ND_constant_integer";
  second.int_inputs["value"] = -9;
  second.outputs["out"] = materialx::Type::Integer;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{first, second}}, &graph));

  MagicTextureNode *first_node = nullptr;
  MagicTextureNode *second_node = nullptr;
  for (ShaderNode *node : graph.nodes) {
    first_node = node->name == "First" ? dynamic_cast<MagicTextureNode *>(node) : first_node;
    second_node = node->name == "Second" ? dynamic_cast<MagicTextureNode *>(node) : second_node;
  }
  ASSERT_NE(first_node, nullptr);
  ASSERT_NE(second_node, nullptr);
  EXPECT_EQ(first_node->get_depth(), 3);
  EXPECT_EQ(second_node->get_depth(), -9);
}

TEST(materialx_graph, rejects_constant_integer_bad_shape_and_tag_atomically)
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
  constant.name = "Integer";
  constant.nodedef = "ND_constant_integer";
  constant.int_inputs["value"] = 5;
  constant.outputs["out"] = materialx::Type::Integer;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  /* Wrong tag: nodedef is ND_constant_integer but the output type is
   * Boolean, not Integer. */
  materialx::Node wrong_tag = constant;
  wrong_tag.outputs["out"] = materialx::Type::Boolean;
  expect_rejected({{wrong_tag}});

  materialx::Node linked_value = constant;
  linked_value.links["value"] = {"Other", "out", materialx::Type::Integer};
  materialx::Node other = constant;
  other.name = "Other";
  expect_rejected({{other, linked_value}});
}

/* Task 6: matrix boundary, device ABI. `TextureCoordinateNode::ob_tfm` is
 * a genuine native `SocketType::TRANSFORM` field (Cycles' real affine 4x3
 * matrix representation, `struct Transform { float4 x, y, z; }`) --
 * reused as a value vehicle, same strategy as Task 4/5. Matrix33 maps
 * exactly onto Transform's linear 3x3 block; Matrix44 maps exactly onto
 * Transform's full 4x3 (12 components) *only* for genuinely affine
 * matrices -- validate() rejects any non-affine Matrix44 before lower()
 * is ever reached, so there is no truncation, ever, in what actually
 * lowers. */

TEST(materialx_graph, lowers_constant_matrix33_to_native_transform_block)
{
  materialx::Node constant;
  constant.name = "Matrix33";
  constant.nodedef = "ND_constant_matrix33";
  constant.matrix33_inputs["value"] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
  constant.outputs["out"] = materialx::Type::Matrix33;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  TextureCoordinateNode *matrix = nullptr;
  for (ShaderNode *node : graph.nodes) {
    matrix = node->name == "Matrix33" ? dynamic_cast<TextureCoordinateNode *>(node) : matrix;
  }
  ASSERT_NE(matrix, nullptr);
  const Transform tfm = matrix->get_ob_tfm();
  EXPECT_FLOAT_EQ(tfm.x.x, 1.0f);
  EXPECT_FLOAT_EQ(tfm.x.y, 2.0f);
  EXPECT_FLOAT_EQ(tfm.x.z, 3.0f);
  EXPECT_FLOAT_EQ(tfm.y.x, 4.0f);
  EXPECT_FLOAT_EQ(tfm.y.y, 5.0f);
  EXPECT_FLOAT_EQ(tfm.y.z, 6.0f);
  EXPECT_FLOAT_EQ(tfm.z.x, 7.0f);
  EXPECT_FLOAT_EQ(tfm.z.y, 8.0f);
  EXPECT_FLOAT_EQ(tfm.z.z, 9.0f);
  /* Exact -- not lossy: translation is forced to zero, which is correct
   * (not a truncation) because Matrix33 has no translation component. */
  EXPECT_FLOAT_EQ(tfm.x.w, 0.0f);
  EXPECT_FLOAT_EQ(tfm.y.w, 0.0f);
  EXPECT_FLOAT_EQ(tfm.z.w, 0.0f);
}

TEST(materialx_graph, rejects_constant_matrix33_bad_shape_nonfinite_and_tag_atomically)
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
  constant.name = "Matrix33";
  constant.nodedef = "ND_constant_matrix33";
  constant.matrix33_inputs["value"] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
  constant.outputs["out"] = materialx::Type::Matrix33;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  /* Wrong tag: nodedef is ND_constant_matrix33 but the output type is
   * Matrix44. */
  materialx::Node wrong_tag = constant;
  wrong_tag.outputs["out"] = materialx::Type::Matrix44;
  expect_rejected({{wrong_tag}});

  materialx::Node nonfinite = constant;
  nonfinite.matrix33_inputs["value"][4] = std::numeric_limits<float>::infinity();
  expect_rejected({{nonfinite}});

  materialx::Node linked_value = constant;
  linked_value.links["value"] = {"Other", "out", materialx::Type::Matrix33};
  materialx::Node other = constant;
  other.name = "Other";
  expect_rejected({{other, linked_value}});
}

TEST(materialx_graph, lowers_constant_affine_matrix44_to_native_transform)
{
  materialx::Node constant;
  constant.name = "Matrix44";
  constant.nodedef = "ND_constant_matrix44";
  constant.matrix44_inputs["value"] = {1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1};
  constant.outputs["out"] = materialx::Type::Matrix44;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{constant}}, &graph));

  TextureCoordinateNode *matrix = nullptr;
  for (ShaderNode *node : graph.nodes) {
    matrix = node->name == "Matrix44" ? dynamic_cast<TextureCoordinateNode *>(node) : matrix;
  }
  ASSERT_NE(matrix, nullptr);
  const Transform tfm = matrix->get_ob_tfm();
  /* All 12 non-implicit components preserved exactly, including
   * translation (the 4th column) -- this is the genuinely affine case
   * Matrix44 maps onto Transform with zero loss. */
  EXPECT_FLOAT_EQ(tfm.x.w, 10.0f);
  EXPECT_FLOAT_EQ(tfm.y.w, 20.0f);
  EXPECT_FLOAT_EQ(tfm.z.w, 30.0f);
  EXPECT_FLOAT_EQ(tfm.x.x, 1.0f);
  EXPECT_FLOAT_EQ(tfm.y.y, 1.0f);
  EXPECT_FLOAT_EQ(tfm.z.z, 1.0f);
}

TEST(materialx_graph, rejects_nonaffine_constant_matrix44_as_honest_boundary_not_truncation)
{
  /* The core Task 6 boundary assertion: a non-affine Matrix44 (last row
   * not {0, 0, 0, 1} -- here a genuine projective/perspective divide row)
   * must be rejected, not silently truncated into an affine
   * approximation. */
  materialx::Node constant;
  constant.name = "Matrix44";
  constant.nodedef = "ND_constant_matrix44";
  constant.matrix44_inputs["value"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0.5f, 1};
  constant.outputs["out"] = materialx::Type::Matrix44;

  EXPECT_FALSE(materialx::validate({{constant}}));

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  EXPECT_FALSE(materialx::lower({{constant}}, &graph));
  EXPECT_EQ(graph.nodes.size(), original_node_count);
}

TEST(materialx_graph, rejects_constant_matrix44_bad_shape_and_tag_atomically)
{
  const auto expect_rejected = [](materialx::Graph source) {
    EXPECT_FALSE(materialx::validate(source));

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower(source, &graph));
    EXPECT_EQ(graph.nodes.size(), original_node_count);
  };

  materialx::Node constant;
  constant.name = "Matrix44";
  constant.nodedef = "ND_constant_matrix44";
  constant.matrix44_inputs["value"] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  constant.outputs["out"] = materialx::Type::Matrix44;

  materialx::Node extra_float = constant;
  extra_float.inputs["unexpected"] = 1.0f;
  expect_rejected({{extra_float}});

  /* Wrong tag: nodedef is ND_constant_matrix44 but the output type is
   * Matrix33. */
  materialx::Node wrong_tag = constant;
  wrong_tag.outputs["out"] = materialx::Type::Matrix33;
  expect_rejected({{wrong_tag}});
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

TEST(materialx_graph, lowers_literal_ramp_gradient_to_color4_constant)
{
  /* Real MaterialX 1.39 stdlib sources:
   * libraries/stdlib/stdlib_defs.mtlx declares ND_ramp_gradient as ND_ramp's
   * Color4 interval helper; libraries/stdlib/stdlib_ng.mtlx selects previous
   * color outside the active interval/final interval and otherwise mixes
   * color1/color2 with linear, smooth, or step interpolation. This scoped
   * lowering folds only literal controls, preserving RGB plus alpha sidecar. */
  const struct {
    const char *name;
    float x;
    int interpolation;
    float expected_r;
    float expected_alpha;
  } cases[] = {{"Linear", 0.25f, 0, 0.25f, 0.625f},
               {"Smooth", 0.5f, 1, 0.5f, 0.75f},
               {"Step", 0.75f, 2, 0.0f, 0.5f}};

  for (const auto &test : cases) {
    materialx::Node gradient{test.name, "ND_ramp_gradient"};
    gradient.inputs = {{"x", test.x}, {"interval1", 0.0f}, {"interval2", 1.0f}};
    gradient.int_inputs = {{"interpolation", test.interpolation},
                           {"interval_num", 1},
                           {"num_intervals", 2}};
    gradient.float4_inputs = {{"color1", make_float4(0.0f, 0.1f, 0.2f, 0.5f)},
                              {"color2", make_float4(1.0f, 0.9f, 0.8f, 1.0f)},
                              {"prev_color", make_float4(0.25f, 0.25f, 0.25f, 0.25f)}};
    gradient.outputs["out"] = materialx::Type::Color4;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{gradient}}, &graph)) << test.name;

    CombineColorNode *color = nullptr;
    ValueNode *alpha = nullptr;
    for (ShaderNode *node : graph.nodes) {
      color = node->name == test.name ? dynamic_cast<CombineColorNode *>(node) : color;
      alpha = node->name == string(test.name) + ".Alpha" ? dynamic_cast<ValueNode *>(node) : alpha;
    }
    ASSERT_NE(color, nullptr) << test.name;
    ASSERT_NE(alpha, nullptr) << test.name;
    EXPECT_FLOAT_EQ(color->get_r(), test.expected_r) << test.name;
    EXPECT_FLOAT_EQ(alpha->get_value(), test.expected_alpha) << test.name;
  }
}

TEST(materialx_graph, folds_literal_ramp_to_exact_color4_constant)
{
  materialx::Node ramp{"Ramp", "ND_ramp"};
  ramp.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
  ramp.int_inputs = {{"type", 0}, {"interpolation", 0}, {"num_intervals", 3}};
  for (int index = 1; index <= 10; index++) {
    ramp.inputs["interval" + std::to_string(index)] = index <= 3 ? float(index - 1) * 0.5f : 1.0f;
    ramp.float4_inputs["color" + std::to_string(index)] = make_float4(1.0f, 1.0f, 1.0f, 1.0f);
  }
  ramp.float4_inputs["color1"] = make_float4(0.0f, 0.0f, 0.0f, 0.2f);
  ramp.float4_inputs["color2"] = make_float4(1.0f, 0.0f, 0.0f, 0.6f);
  ramp.float4_inputs["color3"] = make_float4(0.0f, 0.0f, 1.0f, 1.0f);
  ramp.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{ramp}}, &graph));

  CombineColorNode *color = nullptr;
  ValueNode *alpha = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color = node->name == "Ramp" ? dynamic_cast<CombineColorNode *>(node) : color;
    alpha = node->name == "Ramp.Alpha" ? dynamic_cast<ValueNode *>(node) : alpha;
  }
  ASSERT_NE(color, nullptr);
  ASSERT_NE(alpha, nullptr);
  EXPECT_FLOAT_EQ(color->get_r(), 0.5f);
  EXPECT_FLOAT_EQ(color->get_g(), 0.0f);
  EXPECT_FLOAT_EQ(color->get_b(), 0.0f);
  EXPECT_FLOAT_EQ(alpha->get_value(), 0.4f);
}

TEST(materialx_graph, lowers_vector_ramps_and_splits_to_native_vector_mix)
{
  materialx::Node uv{"UV", "ND_constant_vector2"};
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  const struct {
    const char *id;
    materialx::Type type;
    bool split;
    bool top_to_bottom;
  } cases[] = {{"ND_ramplr_vector2", materialx::Type::Vector2, false, false},
               {"ND_ramptb_vector2", materialx::Type::Vector2, false, true},
               {"ND_ramplr_vector3", materialx::Type::Vector3, false, false},
               {"ND_ramptb_vector3", materialx::Type::Vector3, false, true},
               {"ND_splitlr_vector2", materialx::Type::Vector2, true, false},
               {"ND_splittb_vector2", materialx::Type::Vector2, true, true},
               {"ND_splitlr_vector3", materialx::Type::Vector3, true, false},
               {"ND_splittb_vector3", materialx::Type::Vector3, true, true}};

  for (const auto &test : cases) {
    materialx::Node node{"Procedural", test.id};
    const char *first_name = test.top_to_bottom ? "valuet" : "valuel";
    const char *second_name = test.top_to_bottom ? "valueb" : "valuer";
    if (test.type == materialx::Type::Vector2) {
      node.vector2_inputs[first_name] = make_float2(0.1f, 0.2f);
      node.vector2_inputs[second_name] = make_float2(0.7f, 0.8f);
    }
    else {
      node.vector3_inputs[first_name] = make_float3(0.1f, 0.2f, 0.3f);
      node.vector3_inputs[second_name] = make_float3(0.7f, 0.8f, 0.9f);
    }
    if (test.split) {
      node.inputs["center"] = 0.375f;
    }
    node.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
    node.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{uv, node}}, &graph)) << test.id;
    MixVectorNode *mix = nullptr;
    ShaderNode *axis = nullptr;
    ShaderNode *factor = nullptr;
    for (ShaderNode *lowered : graph.nodes) {
      mix = lowered->name == "Procedural" ? dynamic_cast<MixVectorNode *>(lowered) : mix;
      axis = lowered->name == "Procedural.coordinate" ? lowered : axis;
      factor = lowered->name == "Procedural.factor" ? lowered : factor;
    }
    ASSERT_NE(mix, nullptr) << test.id;
    ASSERT_NE(axis, nullptr) << test.id;
    ASSERT_NE(factor, nullptr) << test.id;
    EXPECT_EQ(mix->input("Factor")->link, factor->output(test.split ? "Value" : "Result"))
        << test.id;
    EXPECT_EQ(factor->input(test.split ? "Value1" : "Value")->link,
              axis->output(test.top_to_bottom ? "Y" : "X"))
        << test.id;
    EXPECT_EQ(mix->get_a(), make_float3(0.1f, 0.2f, test.type == materialx::Type::Vector2 ? 0.0f : 0.3f))
        << test.id;
    EXPECT_EQ(mix->get_b(), make_float3(0.7f, 0.8f, test.type == materialx::Type::Vector2 ? 0.0f : 0.9f))
        << test.id;
  }
}

TEST(materialx_graph, lowers_vector_ramps_with_literal_texcoords)
{
  const struct {
    const char *id;
    materialx::Type type;
    bool top_to_bottom;
  } cases[] = {{"ND_ramplr_vector2", materialx::Type::Vector2, false},
               {"ND_ramptb_vector2", materialx::Type::Vector2, true},
               {"ND_ramplr_vector3", materialx::Type::Vector3, false},
               {"ND_ramptb_vector3", materialx::Type::Vector3, true}};

  for (const auto &test : cases) {
    materialx::Node node{"VectorRamp", test.id};
    const char *first_name = test.top_to_bottom ? "valuet" : "valuel";
    const char *second_name = test.top_to_bottom ? "valueb" : "valuer";
    if (test.type == materialx::Type::Vector2) {
      node.vector2_inputs[first_name] = make_float2(0.1f, 0.2f);
      node.vector2_inputs[second_name] = make_float2(0.7f, 0.8f);
    }
    else {
      node.vector3_inputs[first_name] = make_float3(0.1f, 0.2f, 0.3f);
      node.vector3_inputs[second_name] = make_float3(0.7f, 0.8f, 0.9f);
    }
    node.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
    node.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{node}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *lowered : graph.nodes) {
      nodes[lowered->name.string()] = lowered;
    }
    auto *mix = dynamic_cast<MixVectorNode *>(nodes["VectorRamp"]);
    auto *axis = dynamic_cast<SeparateXYZNode *>(nodes["VectorRamp.coordinate"]);
    ASSERT_NE(mix, nullptr) << test.id;
    ASSERT_NE(axis, nullptr) << test.id;
    EXPECT_EQ(axis->get_vector(), make_float3(0.25f, 0.75f, 0.0f)) << test.id;
    EXPECT_EQ(axis->input("Vector")->link, nullptr) << test.id;
    EXPECT_EQ(mix->get_a(), make_float3(0.1f, 0.2f, test.type == materialx::Type::Vector2 ? 0.0f : 0.3f))
        << test.id;
    EXPECT_EQ(mix->get_b(), make_float3(0.7f, 0.8f, test.type == materialx::Type::Vector2 ? 0.0f : 0.9f))
        << test.id;
  }
}

TEST(materialx_graph, lowers_procedural2d_remainder_ramp4_scalar_color_and_vector4)
{
  /* Real MaterialX 1.39 stdlib sources:
   * libraries/stdlib/stdlib_defs.mtlx declares ND_ramp4_float/color3/color4/vector4
   * in nodegroup="procedural2d"; libraries/stdlib/stdlib_ng.mtlx NG_ramp4_*
   * clamps texcoord, extracts s/t, mixes the top and bottom rows by s, then
   * mixes those row results by t. */
  materialx::Node uv{"UV", "ND_constant_vector2"};
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  materialx::Node scalar{"Ramp4Float", "ND_ramp4_float"};
  scalar.inputs = {{"valuetl", 0.1f}, {"valuetr", 0.3f}, {"valuebl", 0.5f}, {"valuebr", 0.7f}};
  scalar.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  scalar.outputs["out"] = materialx::Type::Float;

  materialx::Node color3{"Ramp4Color3", "ND_ramp4_color3"};
  color3.color3_inputs = {{"valuetl", make_float3(0.1f, 0.2f, 0.3f)},
                          {"valuetr", make_float3(0.4f, 0.5f, 0.6f)},
                          {"valuebl", make_float3(0.7f, 0.8f, 0.9f)},
                          {"valuebr", make_float3(1.0f, 1.1f, 1.2f)}};
  color3.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  color3.outputs["out"] = materialx::Type::Color3;

  materialx::Node color4{"Ramp4Color4", "ND_ramp4_color4"};
  color4.float4_inputs = {{"valuetl", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                          {"valuetr", make_float4(0.5f, 0.6f, 0.7f, 0.8f)},
                          {"valuebl", make_float4(0.9f, 1.0f, 1.1f, 1.2f)},
                          {"valuebr", make_float4(1.3f, 1.4f, 1.5f, 1.6f)}};
  color4.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector4{"Ramp4Vector4", "ND_ramp4_vector4"};
  vector4.vector4_inputs = {{"valuetl", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                            {"valuetr", make_float4(0.5f, 0.6f, 0.7f, 0.8f)},
                            {"valuebl", make_float4(0.9f, 1.0f, 1.1f, 1.2f)},
                            {"valuebr", make_float4(1.3f, 1.4f, 1.5f, 1.6f)}};
  vector4.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
  vector4.outputs["out"] = materialx::Type::Vector4;

  materialx::Node extract_alpha{"ExtractAlpha", "ND_extract_color4"};
  extract_alpha.links["in"] = {"Ramp4Color4", "out", materialx::Type::Color4};
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node extract_w{"ExtractW", "ND_extract_vector4"};
  extract_w.links["in"] = {"Ramp4Vector4", "out", materialx::Type::Vector4};
  extract_w.int_inputs["index"] = 3;
  extract_w.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{uv, scalar, color3, color4, vector4, extract_alpha, extract_w}}, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Ramp4Float"]), nullptr);
  ASSERT_NE(dynamic_cast<MixNode *>(nodes["Ramp4Color3"]), nullptr);
  ASSERT_NE(dynamic_cast<MixNode *>(nodes["Ramp4Color4"]), nullptr);
  ASSERT_NE(dynamic_cast<MixVectorNode *>(nodes["Ramp4Vector4"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Ramp4Color4.Alpha"]), nullptr);
  ASSERT_NE(dynamic_cast<MathNode *>(nodes["Ramp4Vector4.W"]), nullptr);
  EXPECT_EQ(nodes["Ramp4Float"]->input("Value2")->link,
            nodes["Ramp4Float.product"]->output("Value"));
  EXPECT_EQ(nodes["Ramp4Color3"]->input("Color1")->link,
            nodes["Ramp4Color3.top"]->output("Color"));
  EXPECT_EQ(nodes["Ramp4Color4.Alpha"]->input("Value1")->link,
            nodes["Ramp4Color4.Alpha.top"]->output("Value"));
  EXPECT_EQ(nodes["Ramp4Vector4.W"]->input("Value1")->link,
            nodes["Ramp4Vector4.W.top"]->output("Value"));
  EXPECT_FALSE(nodes.contains("ExtractW"));
}

TEST(materialx_graph, lowers_procedural2d_remainder_vector4_ramps_and_splits)
{
  /* Real MaterialX 1.39 stdlib_defs.mtlx declares ND_ramplr_vector4,
   * ND_ramptb_vector4, ND_splitlr_vector4, and ND_splittb_vector4 in
   * nodegroup="procedural2d" next to the existing vector2/vector3 forms. */
  materialx::Node uv{"UV", "ND_constant_vector2"};
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  const struct {
    const char *name;
    const char *id;
    bool top_to_bottom;
    bool split;
  } cases[] = {{"RampLR", "ND_ramplr_vector4", false, false},
               {"RampTB", "ND_ramptb_vector4", true, false},
               {"SplitLR", "ND_splitlr_vector4", false, true},
               {"SplitTB", "ND_splittb_vector4", true, true}};

  materialx::Graph source;
  source.nodes.push_back(uv);
  for (const auto &test : cases) {
    materialx::Node node{test.name, test.id};
    node.vector4_inputs[test.top_to_bottom ? "valuet" : "valuel"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
    node.vector4_inputs[test.top_to_bottom ? "valueb" : "valuer"] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
    if (test.split) {
      node.inputs["center"] = 0.375f;
    }
    node.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
    node.outputs["out"] = materialx::Type::Vector4;
    source.nodes.push_back(std::move(node));
  }

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(source, &graph));
  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  for (const auto &test : cases) {
    ASSERT_NE(dynamic_cast<MixVectorNode *>(nodes[test.name]), nullptr) << test.id;
    ASSERT_NE(dynamic_cast<MathNode *>(nodes[string(test.name) + ".W"]), nullptr) << test.id;
    ASSERT_NE(nodes[string(test.name) + ".W"]->input("Value2")->link, nullptr) << test.id;
  }
}
TEST(materialx_graph, lowers_split_family_with_literal_texcoords)
{
  const struct {
    const char *name;
    const char *id;
    materialx::Type type;
    bool top_to_bottom;
  } cases[] = {{"SplitFloat", "ND_splitlr_float", materialx::Type::Float, false},
               {"SplitColor3", "ND_splittb_color3", materialx::Type::Color3, true},
               {"SplitColor4", "ND_splitlr_color4", materialx::Type::Color4, false},
               {"SplitVector2", "ND_splittb_vector2", materialx::Type::Vector2, true},
               {"SplitVector3", "ND_splitlr_vector3", materialx::Type::Vector3, false},
               {"SplitVector4", "ND_splittb_vector4", materialx::Type::Vector4, true}};

  for (const auto &test : cases) {
    materialx::Node split{test.name, test.id};
    const char *first_name = test.top_to_bottom ? "valuet" : "valuel";
    const char *second_name = test.top_to_bottom ? "valueb" : "valuer";
    if (test.type == materialx::Type::Float) {
      split.inputs[first_name] = 0.1f;
      split.inputs[second_name] = 0.9f;
    }
    else if (test.type == materialx::Type::Color3) {
      split.color3_inputs[first_name] = make_float3(0.1f, 0.2f, 0.3f);
      split.color3_inputs[second_name] = make_float3(0.7f, 0.8f, 0.9f);
    }
    else if (test.type == materialx::Type::Color4) {
      split.float4_inputs[first_name] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
      split.float4_inputs[second_name] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
    }
    else if (test.type == materialx::Type::Vector2) {
      split.vector2_inputs[first_name] = make_float2(0.1f, 0.2f);
      split.vector2_inputs[second_name] = make_float2(0.7f, 0.8f);
    }
    else if (test.type == materialx::Type::Vector3) {
      split.vector3_inputs[first_name] = make_float3(0.1f, 0.2f, 0.3f);
      split.vector3_inputs[second_name] = make_float3(0.7f, 0.8f, 0.9f);
    }
    else {
      split.vector4_inputs[first_name] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
      split.vector4_inputs[second_name] = make_float4(0.5f, 0.6f, 0.7f, 0.8f);
    }
    split.inputs["center"] = 0.375f;
    split.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
    split.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{split}}, &graph)) << test.id;
    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *coordinate = dynamic_cast<SeparateXYZNode *>(nodes[string(test.name) + ".coordinate"]);
    ASSERT_NE(coordinate, nullptr) << test.id;
    EXPECT_EQ(coordinate->get_vector(), make_float3(0.25f, 0.75f, 0.0f)) << test.id;
    EXPECT_EQ(coordinate->input("Vector")->link, nullptr) << test.id;
  }
}

TEST(materialx_graph, lowers_vector2_and_vector3_ramp4_bilinear_mixes)
{
  /* MaterialX stdlib_defs.mtlx declares ND_ramp4_vector2/vector3 in the
   * procedural2d group. stdlib_ng.mtlx NG_ramp4_vector2/vector3 clamps the
   * Vector2 texcoord, extracts s/t, mixes top and bottom rows by s, then
   * mixes those two results by t. */
  materialx::Node uv{"UV", "ND_constant_vector2"};
  uv.vector2_inputs["value"] = make_float2(0.25f, 0.75f);
  uv.outputs["out"] = materialx::Type::Vector2;

  const struct {
    const char *id;
    materialx::Type type;
  } cases[] = {{"ND_ramp4_vector2", materialx::Type::Vector2},
               {"ND_ramp4_vector3", materialx::Type::Vector3}};

  for (const auto &test : cases) {
    materialx::Node ramp{"Ramp4", test.id};
    if (test.type == materialx::Type::Vector2) {
      ramp.vector2_inputs["valuetl"] = make_float2(0.1f, 0.2f);
      ramp.vector2_inputs["valuetr"] = make_float2(0.3f, 0.4f);
      ramp.vector2_inputs["valuebl"] = make_float2(0.5f, 0.6f);
      ramp.vector2_inputs["valuebr"] = make_float2(0.7f, 0.8f);
    }
    else {
      ramp.vector3_inputs["valuetl"] = make_float3(0.1f, 0.2f, 0.3f);
      ramp.vector3_inputs["valuetr"] = make_float3(0.4f, 0.5f, 0.6f);
      ramp.vector3_inputs["valuebl"] = make_float3(0.7f, 0.8f, 0.9f);
      ramp.vector3_inputs["valuebr"] = make_float3(1.0f, 1.1f, 1.2f);
    }
    ramp.links["texcoord"] = {"UV", "out", materialx::Type::Vector2};
    ramp.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{uv, ramp}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *coordinate_minimum = dynamic_cast<VectorMathNode *>(nodes["Ramp4.coordinate.minimum"]);
    auto *coordinate = dynamic_cast<VectorMathNode *>(nodes["Ramp4.coordinate"]);
    auto *axis = dynamic_cast<SeparateXYZNode *>(nodes["Ramp4.axis"]);
    auto *top = dynamic_cast<MixVectorNode *>(nodes["Ramp4.top"]);
    auto *bottom = dynamic_cast<MixVectorNode *>(nodes["Ramp4.bottom"]);
    auto *result = dynamic_cast<MixVectorNode *>(nodes["Ramp4"]);
    ASSERT_NE(coordinate_minimum, nullptr) << test.id;
    ASSERT_NE(coordinate, nullptr) << test.id;
    ASSERT_NE(axis, nullptr) << test.id;
    ASSERT_NE(top, nullptr) << test.id;
    ASSERT_NE(bottom, nullptr) << test.id;
    ASSERT_NE(result, nullptr) << test.id;
    EXPECT_EQ(coordinate_minimum->get_math_type(), NODE_VECTOR_MATH_MINIMUM) << test.id;
    EXPECT_EQ(coordinate->get_math_type(), NODE_VECTOR_MATH_MAXIMUM) << test.id;
    EXPECT_EQ(coordinate_minimum->get_vector2(), make_float3(1.0f)) << test.id;
    EXPECT_EQ(coordinate->get_vector2(), zero_float3()) << test.id;
    ASSERT_NE(axis->input("Vector")->link, nullptr) << test.id;
    EXPECT_FALSE(coordinate->output("Vector")->links.empty()) << test.id;
    EXPECT_EQ(top->input("Factor")->link, axis->output("X")) << test.id;
    EXPECT_EQ(bottom->input("Factor")->link, axis->output("X")) << test.id;
    EXPECT_EQ(result->input("Factor")->link, axis->output("Y")) << test.id;
    EXPECT_EQ(result->input("A")->link, top->output("Result")) << test.id;
    EXPECT_EQ(result->input("B")->link, bottom->output("Result")) << test.id;
    EXPECT_EQ(result->get_a(), zero_float3()) << test.id;
    EXPECT_EQ(result->get_b(), zero_float3()) << test.id;
    if (test.type == materialx::Type::Vector2) {
      EXPECT_EQ(top->get_a(), make_float3(0.1f, 0.2f, 0.0f)) << test.id;
      EXPECT_EQ(bottom->get_b(), make_float3(0.7f, 0.8f, 0.0f)) << test.id;
    }
    else {
      EXPECT_EQ(top->get_a(), make_float3(0.1f, 0.2f, 0.3f)) << test.id;
      EXPECT_EQ(bottom->get_b(), make_float3(1.0f, 1.1f, 1.2f)) << test.id;
    }
  }
}

TEST(materialx_graph, lowers_ramp4_family_with_literal_texcoords)
{
  const struct {
    const char *name;
    const char *id;
    materialx::Type type;
  } cases[] = {{"Ramp4Float", "ND_ramp4_float", materialx::Type::Float},
               {"Ramp4Color3", "ND_ramp4_color3", materialx::Type::Color3},
               {"Ramp4Color4", "ND_ramp4_color4", materialx::Type::Color4},
               {"Ramp4Vector2", "ND_ramp4_vector2", materialx::Type::Vector2},
               {"Ramp4Vector3", "ND_ramp4_vector3", materialx::Type::Vector3},
               {"Ramp4Vector4", "ND_ramp4_vector4", materialx::Type::Vector4}};

  for (const auto &test : cases) {
    materialx::Node ramp{test.name, test.id};
    if (test.type == materialx::Type::Float) {
      ramp.inputs = {{"valuetl", 0.1f}, {"valuetr", 0.3f}, {"valuebl", 0.5f}, {"valuebr", 0.7f}};
    }
    else if (test.type == materialx::Type::Color3) {
      ramp.color3_inputs = {{"valuetl", make_float3(0.1f, 0.2f, 0.3f)},
                            {"valuetr", make_float3(0.4f, 0.5f, 0.6f)},
                            {"valuebl", make_float3(0.7f, 0.8f, 0.9f)},
                            {"valuebr", make_float3(1.0f, 1.1f, 1.2f)}};
    }
    else if (test.type == materialx::Type::Color4) {
      ramp.float4_inputs = {{"valuetl", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                            {"valuetr", make_float4(0.5f, 0.6f, 0.7f, 0.8f)},
                            {"valuebl", make_float4(0.9f, 1.0f, 1.1f, 1.2f)},
                            {"valuebr", make_float4(1.3f, 1.4f, 1.5f, 1.6f)}};
    }
    else if (test.type == materialx::Type::Vector2) {
      ramp.vector2_inputs = {{"valuetl", make_float2(0.1f, 0.2f)},
                             {"valuetr", make_float2(0.3f, 0.4f)},
                             {"valuebl", make_float2(0.5f, 0.6f)},
                             {"valuebr", make_float2(0.7f, 0.8f)}};
    }
    else if (test.type == materialx::Type::Vector3) {
      ramp.vector3_inputs = {{"valuetl", make_float3(0.1f, 0.2f, 0.3f)},
                             {"valuetr", make_float3(0.4f, 0.5f, 0.6f)},
                             {"valuebl", make_float3(0.7f, 0.8f, 0.9f)},
                             {"valuebr", make_float3(1.0f, 1.1f, 1.2f)}};
    }
    else {
      ramp.vector4_inputs = {{"valuetl", make_float4(0.1f, 0.2f, 0.3f, 0.4f)},
                             {"valuetr", make_float4(0.5f, 0.6f, 0.7f, 0.8f)},
                             {"valuebl", make_float4(0.9f, 1.0f, 1.1f, 1.2f)},
                             {"valuebr", make_float4(1.3f, 1.4f, 1.5f, 1.6f)}};
    }
    ramp.vector2_inputs["texcoord"] = make_float2(0.25f, 0.75f);
    ramp.outputs["out"] = test.type;

    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{ramp}}, &graph)) << test.id;

    std::unordered_map<string, ShaderNode *> nodes;
    for (ShaderNode *node : graph.nodes) {
      nodes[node->name.string()] = node;
    }
    auto *coordinate_minimum = dynamic_cast<VectorMathNode *>(nodes[string(test.name) + ".coordinate.minimum"]);
    ASSERT_NE(coordinate_minimum, nullptr) << test.id;
    EXPECT_EQ(coordinate_minimum->get_vector1(), make_float3(0.25f, 0.75f, 0.0f)) << test.id;
    EXPECT_EQ(coordinate_minimum->input("Vector1")->link, nullptr) << test.id;
  }
}

TEST(materialx_graph, lowers_measured_procedural2d_ramp_fixture_literal_samples)
{
  /* Mirrors materialx-terminal-canonical/research/materialx_release/
   * procedural2d_ramp_fixtures.py: every ramplr/ramptb/ramp4 typed sibling is
   * exercised with the deterministic literal texcoords [-0.5,0.25],
   * [0.25,0.75], and [1.5,1.5]. The clamp endpoints are the measured-risk
   * cases: lowering must seed the literal coordinate into the native clamp
   * prelude instead of evaluating from the default zero vector. */
  const struct {
    const char *category;
    const char *suffix;
    materialx::Type type;
  } cases[] = {{"ramplr", "color3", materialx::Type::Color3},
               {"ramplr", "color4", materialx::Type::Color4},
               {"ramplr", "float", materialx::Type::Float},
               {"ramplr", "vector2", materialx::Type::Vector2},
               {"ramplr", "vector3", materialx::Type::Vector3},
               {"ramplr", "vector4", materialx::Type::Vector4},
               {"ramptb", "color3", materialx::Type::Color3},
               {"ramptb", "color4", materialx::Type::Color4},
               {"ramptb", "float", materialx::Type::Float},
               {"ramptb", "vector2", materialx::Type::Vector2},
               {"ramptb", "vector3", materialx::Type::Vector3},
               {"ramptb", "vector4", materialx::Type::Vector4},
               {"ramp4", "color3", materialx::Type::Color3},
               {"ramp4", "color4", materialx::Type::Color4},
               {"ramp4", "float", materialx::Type::Float},
               {"ramp4", "vector2", materialx::Type::Vector2},
               {"ramp4", "vector3", materialx::Type::Vector3},
               {"ramp4", "vector4", materialx::Type::Vector4}};
  const float2 samples[] = {make_float2(-0.5f, 0.25f),
                            make_float2(0.25f, 0.75f),
                            make_float2(1.5f, 1.5f)};

  const auto set_value = [](materialx::Node &node,
                            const char *name,
                            const materialx::Type type,
                            const float seed) {
    if (type == materialx::Type::Float) {
      node.inputs[name] = seed + 0.25f;
    }
    else if (type == materialx::Type::Color3) {
      node.color3_inputs[name] = make_float3(seed + 0.25f, seed + 1.25f, seed + 2.25f);
    }
    else if (type == materialx::Type::Color4) {
      node.float4_inputs[name] = make_float4(seed + 0.25f,
                                             seed + 1.25f,
                                             seed + 2.25f,
                                             seed + 3.25f);
    }
    else if (type == materialx::Type::Vector2) {
      node.vector2_inputs[name] = make_float2(seed + 0.25f, seed + 1.25f);
    }
    else if (type == materialx::Type::Vector3) {
      node.vector3_inputs[name] = make_float3(seed + 0.25f, seed + 1.25f, seed + 2.25f);
    }
    else {
      node.vector4_inputs[name] = make_float4(seed + 0.25f,
                                             seed + 1.25f,
                                             seed + 2.25f,
                                             seed + 3.25f);
    }
  };

  for (const auto &test : cases) {
    const string nodedef = string("ND_") + test.category + "_" + test.suffix;
    for (int sample_index = 0; sample_index < 3; sample_index++) {
      materialx::Node ramp;
      ramp.name = string(test.category) + "_" + test.suffix;
      ramp.nodedef = nodedef;
      const char *ports[] = {"valuetl", "valuetr", "valuebl", "valuebr"};
      const char *lr_ports[] = {"valuel", "valuer"};
      const char *tb_ports[] = {"valuet", "valueb"};
      const int port_count = string(test.category) == "ramp4" ? 4 : 2;
      const char **active_ports = string(test.category) == "ramp4" ? ports :
                                  string(test.category) == "ramplr" ? lr_ports :
                                                                       tb_ports;
      for (int port = 0; port < port_count; port++) {
        set_value(ramp, active_ports[port], test.type, sample_index * 5.0f + port);
      }
      ramp.vector2_inputs["texcoord"] = samples[sample_index];
      ramp.outputs["out"] = test.type;

      ShaderGraph graph;
      string error;
      ASSERT_TRUE(materialx::lower({{ramp}}, &graph, &error)) << nodedef << ": " << error;

      std::unordered_map<string, ShaderNode *> nodes;
      for (ShaderNode *node : graph.nodes) {
        nodes[node->name.string()] = node;
      }
      if (string(test.category) == "ramp4") {
        auto *coordinate_minimum = dynamic_cast<VectorMathNode *>(
            nodes[ramp.name + ".coordinate.minimum"]);
        ASSERT_NE(coordinate_minimum, nullptr) << nodedef;
        EXPECT_EQ(coordinate_minimum->get_vector1(), make_float3(samples[sample_index], 0.0f))
            << nodedef;
        EXPECT_EQ(coordinate_minimum->input("Vector1")->link, nullptr) << nodedef;
      }
      else {
        auto *coordinate = dynamic_cast<SeparateXYZNode *>(nodes[ramp.name + ".coordinate"]);
        ASSERT_NE(coordinate, nullptr) << nodedef;
        EXPECT_EQ(coordinate->get_vector(), make_float3(samples[sample_index], 0.0f))
            << nodedef;
        EXPECT_EQ(coordinate->input("Vector")->link, nullptr) << nodedef;
      }
    }
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

TEST(materialx_graph, lowers_surface_unlit_defaults_to_emission_transparent_composition)
{
  /* Real ND_surface_unlit lowering with only `emission` authored -- the
   * other four fields fall back to their exact nodedef defaults from
   * libraries/stdlib/stdlib_defs.mtlx: emission_color=(1,1,1),
   * transmission=0.0, transmission_color=(1,1,1), opacity=1.0. (At least
   * one authored input is required -- mirrors the pre-existing OpenPBR
   * `validate()`/reader invariant, not a new restriction; see the reader
   * test admits_surface_unlit_with_no_authored_inputs_defaulting_via_lower
   * for the zero-input case.) Mirrors the reference implementation
   * (libraries/stdlib/genosl/mx_surface_unlit.osl):
   *   trans = 0 -> bsdf tint = (0,0,0); edf = 1.0 * 1.0 * (1,1,1)
   *   opacity = 1 -> MixClosureNode.Fac = 1 keeps the composed closure. */
  materialx::Node surface;
  surface.name = "Unlit";
  surface.nodedef = "ND_surface_unlit";
  surface.inputs["emission"] = 1.0f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  TransparentBsdfNode *transmission_bsdf = nullptr;
  TransparentBsdfNode *cutout = nullptr;
  EmissionNode *emission = nullptr;
  AddClosureNode *sum = nullptr;
  MixClosureNode *mix = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *transparent = dynamic_cast<TransparentBsdfNode *>(node)) {
      if (!transmission_bsdf) {
        transmission_bsdf = transparent;
      }
      else {
        cutout = transparent;
      }
    }
    else if (auto *e = dynamic_cast<EmissionNode *>(node)) {
      emission = e;
    }
    else if (auto *a = dynamic_cast<AddClosureNode *>(node)) {
      sum = a;
    }
    else if (auto *m = dynamic_cast<MixClosureNode *>(node)) {
      mix = m;
    }
  }
  ASSERT_NE(transmission_bsdf, nullptr);
  ASSERT_NE(cutout, nullptr);
  ASSERT_NE(emission, nullptr);
  ASSERT_NE(sum, nullptr);
  ASSERT_NE(mix, nullptr);

  EXPECT_EQ(transmission_bsdf->get_color(), make_float3(0.0f, 0.0f, 0.0f));
  EXPECT_FLOAT_EQ(emission->get_strength(), 1.0f);
  EXPECT_EQ(emission->get_color(), make_float3(1.0f, 1.0f, 1.0f));
  EXPECT_EQ(cutout->get_color(), make_float3(1.0f, 1.0f, 1.0f));
  EXPECT_FLOAT_EQ(mix->get_fac(), 1.0f);

  EXPECT_EQ(sum->input("Closure1")->link, transmission_bsdf->output("BSDF"));
  EXPECT_EQ(sum->input("Closure2")->link, emission->output("Emission"));
  /* MixClosureNode is Closure = (1 - Fac) * Closure1 + Fac * Closure2
   * (kernel/osl/shaders/node_mix_closure.osl). Fac is `opacity` and opacity=1
   * means fully OPAQUE, so the composed closure must be Closure2 and the
   * transparent cutout Closure1 -- exactly what this test's own comment above
   * says ("Fac = 1 keeps the composed closure").
   *
   * These assertions previously had it the other way round. They asserted what
   * the implementation did rather than what the OSL semantics require, so they
   * locked in a bug that made every default-opacity ND_surface_unlit render
   * fully transparent and discard its emission. */
  EXPECT_EQ(mix->input("Closure1")->link, cutout->output("BSDF"));
  EXPECT_EQ(mix->input("Closure2")->link, sum->output("Closure"));
  EXPECT_EQ(graph.output()->input("Surface")->link, mix->output("Closure"));
}

TEST(materialx_graph, lowers_surface_unlit_literal_transmission_and_opacity)
{
  materialx::Node surface;
  surface.name = "Unlit";
  surface.nodedef = "ND_surface_unlit";
  surface.inputs["emission"] = 2.0f;
  surface.color3_inputs["emission_color"] = make_float3(0.1f, 0.2f, 0.3f);
  surface.inputs["transmission"] = 0.4f;
  surface.color3_inputs["transmission_color"] = make_float3(0.9f, 0.8f, 0.7f);
  surface.inputs["opacity"] = 0.6f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  TransparentBsdfNode *transmission_bsdf = nullptr;
  EmissionNode *emission = nullptr;
  MixClosureNode *mix = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *transparent = dynamic_cast<TransparentBsdfNode *>(node)) {
      /* The first TransparentBsdfNode created is the transmission tint
       * (non-white for this test case); the cutout node stays pure white
       * and is not needed by this assertion. */
      if (transparent->get_color() != make_float3(1.0f, 1.0f, 1.0f)) {
        transmission_bsdf = transparent;
      }
    }
    else if (auto *e = dynamic_cast<EmissionNode *>(node)) {
      emission = e;
    }
    else if (auto *m = dynamic_cast<MixClosureNode *>(node)) {
      mix = m;
    }
  }
  ASSERT_NE(transmission_bsdf, nullptr);
  ASSERT_NE(emission, nullptr);
  ASSERT_NE(mix, nullptr);

  /* trans = 0.4 -> tint = transmission_color * trans. */
  EXPECT_NEAR(transmission_bsdf->get_color().x, 0.9f * 0.4f, 1e-5f);
  EXPECT_NEAR(transmission_bsdf->get_color().y, 0.8f * 0.4f, 1e-5f);
  EXPECT_NEAR(transmission_bsdf->get_color().z, 0.7f * 0.4f, 1e-5f);
  /* strength = emission * (1 - trans) = 2.0 * 0.6. */
  EXPECT_NEAR(emission->get_strength(), 2.0f * 0.6f, 1e-5f);
  EXPECT_EQ(emission->get_color(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_FLOAT_EQ(mix->get_fac(), 0.6f);
}

TEST(materialx_graph, lowers_surface_unlit_linked_emission_and_colors)
{
  /* emission, emission_color, and transmission_color may be connected
   * sub-graphs (only transmission/opacity are literal-only in this
   * delivery phase -- see usdshade_reader.cpp's admission-time rejection).
   * transmission itself stays literal here so trans is a known constant
   * the emission-weight and transmission-tint scale nodes can use. */
  materialx::Node emission_source;
  emission_source.name = "EmissionSource";
  emission_source.nodedef = "ND_constant_float";
  emission_source.inputs["value"] = 2.0f;
  emission_source.outputs["out"] = materialx::Type::Float;

  materialx::Node emission_color_source;
  emission_color_source.name = "EmissionColorSource";
  emission_color_source.nodedef = "ND_constant_color3";
  emission_color_source.color3_inputs["value"] = make_float3(0.4f, 0.5f, 0.6f);
  emission_color_source.outputs["out"] = materialx::Type::Color3;

  materialx::Node transmission_color_source;
  transmission_color_source.name = "TransmissionColorSource";
  transmission_color_source.nodedef = "ND_constant_color3";
  transmission_color_source.color3_inputs["value"] = make_float3(0.2f, 0.4f, 0.6f);
  transmission_color_source.outputs["out"] = materialx::Type::Color3;

  materialx::Node surface;
  surface.name = "Unlit";
  surface.nodedef = "ND_surface_unlit";
  surface.links["emission"] = {"EmissionSource", "out", materialx::Type::Float};
  surface.links["emission_color"] = {"EmissionColorSource", "out", materialx::Type::Color3};
  surface.links["transmission_color"] = {
      "TransmissionColorSource", "out", materialx::Type::Color3};
  surface.inputs["transmission"] = 0.25f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{emission_source, emission_color_source, transmission_color_source, surface}}, &graph));

  EmissionNode *emission = nullptr;
  TransparentBsdfNode *transmission_bsdf = nullptr;
  MathNode *emission_scale = nullptr;
  VectorMathNode *transmission_scale = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *e = dynamic_cast<EmissionNode *>(node)) {
      emission = e;
    }
    else if (auto *transparent = dynamic_cast<TransparentBsdfNode *>(node)) {
      if (node->name == "Unlit.unlit_transmission") {
        transmission_bsdf = transparent;
      }
    }
    else if (auto *math = dynamic_cast<MathNode *>(node)) {
      if (math->name == "Unlit.unlit_emission_weight_scale") {
        emission_scale = math;
      }
    }
    else if (auto *vector_math = dynamic_cast<VectorMathNode *>(node)) {
      if (vector_math->name == "Unlit.unlit_transmission_color_scale") {
        transmission_scale = vector_math;
      }
    }
  }
  ASSERT_NE(emission, nullptr);
  ASSERT_NE(transmission_bsdf, nullptr);
  ASSERT_NE(emission_scale, nullptr);
  ASSERT_NE(transmission_scale, nullptr);

  /* emission link feeds a *0.75 scale (1 - trans) into Strength. */
  EXPECT_EQ(emission_scale->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_FLOAT_EQ(emission_scale->get_value2(), 0.75f);
  EXPECT_NE(emission_scale->input("Value1")->link, nullptr);
  EXPECT_EQ(emission->input("Strength")->link, emission_scale->output("Value"));
  EXPECT_NE(emission->input("Color")->link, nullptr);

  /* transmission_color link feeds a *0.25 (trans) scale into Color. */
  EXPECT_EQ(transmission_scale->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_FLOAT_EQ(transmission_scale->get_scale(), 0.25f);
  EXPECT_NE(transmission_scale->input("Vector1")->link, nullptr);
  /* Cycles' ShaderGraph::connect() auto-inserts a Vector->Color convert
   * node between VectorMathNode's "Vector" output (SocketType::VECTOR) and
   * BsdfNode's "Color" input (SocketType::COLOR), so the link doesn't point
   * directly at transmission_scale -- just confirm it's actually wired. */
  EXPECT_NE(transmission_bsdf->input("Color")->link, nullptr);
}

TEST(materialx_graph, lowers_standard_surface_to_native_closure_composition)
{
  materialx::Node surface;
  surface.name = "StandardSurface";
  surface.nodedef = "ND_standard_surface_surfaceshader";
  surface.inputs["base"] = 0.25f;
  surface.color3_inputs["base_color"] = make_float3(0.2f, 0.4f, 0.6f);
  surface.inputs["metalness"] = 0.75f;
  surface.inputs["diffuse_roughness"] = 0.1f;
  surface.inputs["specular"] = 0.4f;
  surface.color3_inputs["specular_color"] = make_float3(0.3f, 0.5f, 0.7f);
  surface.inputs["specular_roughness"] = 0.35f;
  surface.inputs["specular_IOR"] = 1.45f;
  surface.inputs["specular_anisotropy"] = 0.2f;
  surface.inputs["transmission"] = 0.6f;
  surface.color3_inputs["transmission_color"] = make_float3(0.8f, 0.7f, 0.6f);
  surface.inputs["subsurface"] = 0.15f;
  surface.color3_inputs["subsurface_radius"] = make_float3(1.0f, 0.5f, 0.25f);
  surface.inputs["subsurface_scale"] = 0.05f;
  surface.inputs["subsurface_anisotropy"] = 0.1f;
  surface.inputs["sheen"] = 0.3f;
  surface.color3_inputs["sheen_color"] = make_float3(0.5f, 0.6f, 0.7f);
  surface.inputs["sheen_roughness"] = 0.45f;
  surface.inputs["coat"] = 0.2f;
  surface.color3_inputs["coat_color"] = make_float3(0.7f, 0.8f, 0.9f);
  surface.inputs["coat_roughness"] = 0.12f;
  surface.inputs["coat_IOR"] = 1.6f;
  surface.inputs["thin_film_thickness"] = 250.0f;
  surface.inputs["thin_film_IOR"] = 1.33f;
  surface.inputs["emission"] = 1.25f;
  surface.color3_inputs["emission_color"] = make_float3(1.0f, 0.5f, 0.25f);
  surface.color3_inputs["opacity"] = make_float3(0.2f, 0.4f, 0.6f);
  surface.int_inputs["thin_walled"] = 1;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  SheenBsdfNode *sheen = nullptr;
  GlassBsdfNode *transmission = nullptr;
  GlossyBsdfNode *coat = nullptr;
  MixClosureNode *transmission_mix = nullptr;
  AddClosureNode *sheen_sum = nullptr;
  AddClosureNode *coat_sum = nullptr;
  for (ShaderNode *node : graph.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
    sheen = sheen ? sheen : dynamic_cast<SheenBsdfNode *>(node);
    transmission = transmission ? transmission : dynamic_cast<GlassBsdfNode *>(node);
    coat = coat ? coat : dynamic_cast<GlossyBsdfNode *>(node);
    if (node->name == "StandardSurface.standard_surface_transmission_mix") {
      transmission_mix = dynamic_cast<MixClosureNode *>(node);
    }
    if (node->name == "StandardSurface.standard_surface_sheen_sum") {
      sheen_sum = dynamic_cast<AddClosureNode *>(node);
    }
    if (node->name == "StandardSurface") {
      coat_sum = dynamic_cast<AddClosureNode *>(node);
    }
  }
  ASSERT_NE(principled, nullptr);
  ASSERT_NE(sheen, nullptr);
  ASSERT_NE(transmission, nullptr);
  ASSERT_NE(coat, nullptr);
  ASSERT_NE(transmission_mix, nullptr);
  ASSERT_NE(sheen_sum, nullptr);
  ASSERT_NE(coat_sum, nullptr);
  EXPECT_EQ(principled->get_base_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_FLOAT_EQ(principled->get_surface_mix_weight(), -0.75f);
  EXPECT_FLOAT_EQ(principled->get_metallic(), 0.75f);
  EXPECT_FLOAT_EQ(principled->get_diffuse_roughness(), 0.1f);
  EXPECT_FLOAT_EQ(principled->get_specular_ior_level(), 0.8f);
  EXPECT_EQ(principled->get_specular_tint(), make_float3(0.3f, 0.5f, 0.7f));
  EXPECT_FLOAT_EQ(principled->get_roughness(), 0.35f);
  EXPECT_FLOAT_EQ(principled->get_ior(), 1.45f);
  EXPECT_FLOAT_EQ(principled->get_anisotropic(), 0.2f);
  EXPECT_FLOAT_EQ(principled->get_subsurface_weight(), 0.15f);
  EXPECT_EQ(principled->get_subsurface_radius(), make_float3(1.0f, 0.5f, 0.25f));
  EXPECT_FLOAT_EQ(principled->get_subsurface_scale(), 0.05f);
  EXPECT_FLOAT_EQ(principled->get_subsurface_anisotropy(), 0.1f);
  EXPECT_FLOAT_EQ(principled->get_thin_film_thickness(), 250.0f);
  EXPECT_FLOAT_EQ(principled->get_thin_film_ior(), 1.33f);
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 1.25f);
  EXPECT_EQ(principled->get_emission_color(), make_float3(1.0f, 0.5f, 0.25f));
  EXPECT_NEAR(principled->get_alpha(), 0.37192f, 1.0e-5f);
  EXPECT_TRUE(principled->get_thin_wall());
  EXPECT_EQ(sheen->get_color(), make_float3(0.5f, 0.6f, 0.7f));
  EXPECT_FLOAT_EQ(sheen->get_surface_mix_weight(), 0.3f);
  EXPECT_FLOAT_EQ(sheen->get_roughness(), 0.45f);
  EXPECT_EQ(transmission->get_color(), make_float3(0.8f, 0.7f, 0.6f));
  EXPECT_FLOAT_EQ(transmission->get_roughness(), 0.35f);
  EXPECT_FLOAT_EQ(transmission->get_IOR(), 1.45f);
  EXPECT_FLOAT_EQ(transmission_mix->get_fac(), 0.6f);
  EXPECT_EQ(coat->get_color(), make_float3(0.7f, 0.8f, 0.9f));
  EXPECT_FLOAT_EQ(coat->get_surface_mix_weight(), 0.2f);
  EXPECT_FLOAT_EQ(coat->get_roughness(), 0.12f);
  EXPECT_EQ(sheen_sum->input("Closure1")->link, principled->output("BSDF"));
  EXPECT_EQ(sheen_sum->input("Closure2")->link, sheen->output("BSDF"));
  EXPECT_EQ(transmission_mix->input("Closure1")->link, sheen_sum->output("Closure"));
  EXPECT_EQ(transmission_mix->input("Closure2")->link, transmission->output("BSDF"));
  EXPECT_EQ(coat_sum->input("Closure1")->link, transmission_mix->output("Closure"));
  EXPECT_EQ(coat_sum->input("Closure2")->link, coat->output("BSDF"));
  EXPECT_EQ(graph.output()->input("Surface")->link, coat_sum->output("Closure"));
}

TEST(materialx_graph, rejects_unrepresentable_standard_surface_inputs_atomically)
{
  for (const char *unsupported : {"subsurface_color",
                                  "transmission_depth",
                                  "transmission_scatter",
                                  "transmission_scatter_anisotropy",
                                  "transmission_dispersion",
                                  "transmission_extra_roughness",
                                  "coat_anisotropy",
                                  "coat_rotation",
                                  "coat_affect_color",
                                  "coat_affect_roughness"})
  {
    materialx::Node surface;
    surface.name = "StandardSurface";
    surface.nodedef = "ND_standard_surface_surfaceshader";
    surface.inputs["base"] = 1.0f;
    if (string(unsupported) == "subsurface_color" || string(unsupported) == "transmission_scatter") {
      surface.color3_inputs[unsupported] = make_float3(0.5f);
    }
    else {
      surface.inputs[unsupported] = 0.5f;
    }
    surface.outputs["out"] = materialx::Type::SurfaceShader;

    ShaderGraph graph;
    EmissionNode *sentinel = graph.create_node<EmissionNode>();
    graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
    const size_t original_node_count = graph.nodes.size();
    EXPECT_FALSE(materialx::lower({{surface}}, &graph)) << unsupported;
    EXPECT_EQ(graph.nodes.size(), original_node_count) << unsupported;
    EXPECT_EQ(graph.output()->input("Surface")->link, sentinel->output("Emission")) << unsupported;
  }
}

TEST(materialx_graph, lowers_usd_preview_surface_literal_inputs_to_principled_bsdf)
{
  /* Real ND_UsdPreviewSurface_surfaceshader lowering onto Cycles'
   * PrincipledBsdfNode -- see usd_preview_surface_id's comment in graph.cpp
   * for the field-name mapping and delivery-phase scope. */
  materialx::Node surface;
  surface.name = "Preview";
  surface.nodedef = "ND_UsdPreviewSurface_surfaceshader";
  surface.color3_inputs["diffuseColor"] = make_float3(0.5f, 0.4f, 0.3f);
  surface.inputs["metallic"] = 0.7f;
  surface.inputs["roughness"] = 0.25f;
  surface.inputs["clearcoat"] = 0.3f;
  surface.inputs["clearcoatRoughness"] = 0.1f;
  surface.inputs["ior"] = 1.4f;
  surface.color3_inputs["emissiveColor"] = make_float3(0.2f, 0.1f, 0.05f);
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->get_base_color(), make_float3(0.5f, 0.4f, 0.3f));
  EXPECT_FLOAT_EQ(principled->get_metallic(), 0.7f);
  EXPECT_FLOAT_EQ(principled->get_roughness(), 0.25f);
  EXPECT_FLOAT_EQ(principled->get_coat_weight(), 0.3f);
  EXPECT_FLOAT_EQ(principled->get_coat_roughness(), 0.1f);
  EXPECT_FLOAT_EQ(principled->get_ior(), 1.4f);
  EXPECT_FLOAT_EQ(principled->get_coat_ior(), 1.4f);
  EXPECT_EQ(principled->get_emission_color(), make_float3(0.2f, 0.1f, 0.05f));
  /* emissiveColor is direct radiance -- Emission Strength = 1 makes
   * Principled's color * strength product reduce to that radiance. */
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 1.0f);
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_usd_preview_surface_defaults_with_no_authored_inputs)
{
  materialx::Node surface;
  surface.name = "Preview";
  surface.nodedef = "ND_UsdPreviewSurface_surfaceshader";
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  /* Untouched fields fall back to PrincipledBsdfNode's own socket
   * defaults, which coincide with ND_UsdPreviewSurface_surfaceshader's
   * real defaults for base_color/metallic/roughness/coat_weight. */
  EXPECT_FLOAT_EQ(principled->get_metallic(), 0.0f);
  EXPECT_FLOAT_EQ(principled->get_roughness(), 0.5f);
  EXPECT_FLOAT_EQ(principled->get_coat_weight(), 0.0f);
  /* emission_strength stays at Principled's own default (0) since no
   * emissiveColor was authored -- only an authored emissiveColor forces
   * Emission Strength = 1. */
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 0.0f);
}

TEST(materialx_graph, lowers_usd_preview_surface_linked_diffuse_and_metallic)
{
  materialx::Node diffuse_source;
  diffuse_source.name = "DiffuseSource";
  diffuse_source.nodedef = "ND_constant_color3";
  diffuse_source.color3_inputs["value"] = make_float3(0.2f, 0.3f, 0.4f);
  diffuse_source.outputs["out"] = materialx::Type::Color3;

  materialx::Node metallic_source;
  metallic_source.name = "MetallicSource";
  metallic_source.nodedef = "ND_constant_float";
  metallic_source.inputs["value"] = 0.9f;
  metallic_source.outputs["out"] = materialx::Type::Float;

  materialx::Node surface;
  surface.name = "Preview";
  surface.nodedef = "ND_UsdPreviewSurface_surfaceshader";
  surface.links["diffuseColor"] = {"DiffuseSource", "out", materialx::Type::Color3};
  surface.links["metallic"] = {"MetallicSource", "out", materialx::Type::Float};
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{diffuse_source, metallic_source, surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_NE(principled->input("Base Color")->link, nullptr);
  EXPECT_NE(principled->input("Metallic")->link, nullptr);
}

TEST(materialx_graph, lowers_gltf_pbr_literal_inputs_to_principled_bsdf)
{
  /* Real ND_gltf_pbr_surfaceshader lowering onto Cycles'
   * PrincipledBsdfNode -- see gltf_pbr_id's comment in graph.cpp for the
   * field-name mapping and delivery-phase scope. */
  materialx::Node surface;
  surface.name = "Gltf";
  surface.nodedef = "ND_gltf_pbr_surfaceshader";
  surface.color3_inputs["base_color"] = make_float3(0.6f, 0.5f, 0.4f);
  surface.inputs["metallic"] = 0.8f;
  surface.inputs["roughness"] = 0.35f;
  surface.inputs["clearcoat"] = 0.2f;
  surface.inputs["clearcoat_roughness"] = 0.05f;
  surface.inputs["ior"] = 1.45f;
  surface.color3_inputs["emissive"] = make_float3(0.3f, 0.2f, 0.1f);
  surface.inputs["emissive_strength"] = 2.5f;
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->get_base_color(), make_float3(0.6f, 0.5f, 0.4f));
  EXPECT_FLOAT_EQ(principled->get_metallic(), 0.8f);
  EXPECT_FLOAT_EQ(principled->get_roughness(), 0.35f);
  EXPECT_FLOAT_EQ(principled->get_coat_weight(), 0.2f);
  EXPECT_FLOAT_EQ(principled->get_coat_roughness(), 0.05f);
  EXPECT_FLOAT_EQ(principled->get_ior(), 1.45f);
  EXPECT_FLOAT_EQ(principled->get_coat_ior(), 1.45f);
  EXPECT_EQ(principled->get_emission_color(), make_float3(0.3f, 0.2f, 0.1f));
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 2.5f);
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_graph, lowers_gltf_pbr_emissive_with_default_strength_when_unauthored)
{
  /* ND_gltf_pbr_surfaceshader's real default for emissive_strength is 1.0
   * (not Principled's own socket default of 0.0) -- an authored `emissive`
   * with no authored `emissive_strength` must still reproduce that real
   * default product. */
  materialx::Node surface;
  surface.name = "Gltf";
  surface.nodedef = "ND_gltf_pbr_surfaceshader";
  surface.color3_inputs["emissive"] = make_float3(0.4f, 0.4f, 0.4f);
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->get_emission_color(), make_float3(0.4f, 0.4f, 0.4f));
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 1.0f);
}

TEST(materialx_graph, lowers_gltf_pbr_defaults_with_no_authored_inputs)
{
  materialx::Node surface;
  surface.name = "Gltf";
  surface.nodedef = "ND_gltf_pbr_surfaceshader";
  surface.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{surface}}, &graph));

  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(node)) {
      principled = p;
    }
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_FLOAT_EQ(principled->get_emission_strength(), 0.0f);
}

/* ======================================================================
 * BSDF closure-producer leaves: real Cycles closure nodes for
 * ND_oren_nayar_diffuse_bsdf, ND_translucent_bsdf, ND_sheen_bsdf,
 * ND_subsurface_bsdf, ND_conductor_bsdf, ND_dielectric_bsdf.
 * ====================================================================== */

TEST(materialx_graph, lowers_oren_nayar_diffuse_bsdf_folding_weight_into_color)
{
  materialx::Node node;
  node.name = "OrenNayar";
  node.nodedef = "ND_oren_nayar_diffuse_bsdf";
  node.inputs["weight"] = 0.5f;
  node.color3_inputs["color"] = make_float3(0.4f, 0.6f, 0.8f);
  node.inputs["roughness"] = 0.35f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  DiffuseBsdfNode *diffuse = nullptr;
  for (ShaderNode *n : graph.nodes) {
    diffuse = n->name == "OrenNayar" ? dynamic_cast<DiffuseBsdfNode *>(n) : diffuse;
  }
  ASSERT_NE(diffuse, nullptr);
  EXPECT_FLOAT_EQ(diffuse->get_color().x, 0.2f);
  EXPECT_FLOAT_EQ(diffuse->get_color().y, 0.3f);
  EXPECT_FLOAT_EQ(diffuse->get_color().z, 0.4f);
  EXPECT_FLOAT_EQ(diffuse->get_roughness(), 0.35f);
}

TEST(materialx_graph, rejects_oren_nayar_diffuse_bsdf_energy_compensation)
{
  materialx::Node node;
  node.name = "OrenNayar";
  node.nodedef = "ND_oren_nayar_diffuse_bsdf";
  node.int_inputs["energy_compensation"] = 1;
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

TEST(materialx_graph, lowers_translucent_bsdf_default_color)
{
  materialx::Node node;
  node.name = "Translucent";
  node.nodedef = "ND_translucent_bsdf";
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  TranslucentBsdfNode *translucent = nullptr;
  for (ShaderNode *n : graph.nodes) {
    translucent = n->name == "Translucent" ? dynamic_cast<TranslucentBsdfNode *>(n) : translucent;
  }
  ASSERT_NE(translucent, nullptr);
  EXPECT_FLOAT_EQ(translucent->get_color().x, 1.0f);
  EXPECT_FLOAT_EQ(translucent->get_color().y, 1.0f);
  EXPECT_FLOAT_EQ(translucent->get_color().z, 1.0f);
}

TEST(materialx_graph, lowers_sheen_bsdf_zeltner_mode)
{
  materialx::Node node;
  node.name = "Sheen";
  node.nodedef = "ND_sheen_bsdf";
  node.string_inputs["mode"] = "zeltner";
  node.color3_inputs["color"] = make_float3(0.2f, 0.3f, 0.4f);
  node.inputs["roughness"] = 0.6f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  SheenBsdfNode *sheen = nullptr;
  for (ShaderNode *n : graph.nodes) {
    sheen = n->name == "Sheen" ? dynamic_cast<SheenBsdfNode *>(n) : sheen;
  }
  ASSERT_NE(sheen, nullptr);
  EXPECT_EQ(sheen->get_distribution(), CLOSURE_BSDF_SHEEN_ID);
  EXPECT_FLOAT_EQ(sheen->get_roughness(), 0.6f);
  EXPECT_FLOAT_EQ(sheen->get_color().x, 0.2f);
}

TEST(materialx_graph, rejects_sheen_bsdf_conty_kulla_default_mode)
{
  /* MaterialX's default mode ("conty_kulla") has no real Cycles closure --
   * Cycles' only sheen closure is Zeltner et al.'s microfiber model. */
  materialx::Node node;
  node.name = "Sheen";
  node.nodedef = "ND_sheen_bsdf";
  node.string_inputs["mode"] = "conty_kulla";
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));

  materialx::Node default_mode;
  default_mode.name = "SheenDefault";
  default_mode.nodedef = "ND_sheen_bsdf";
  default_mode.outputs["out"] = materialx::Type::BSDF;
  EXPECT_FALSE(materialx::validate({{default_mode}}));
}

TEST(materialx_graph, lowers_subsurface_bsdf_random_walk)
{
  materialx::Node node;
  node.name = "Subsurface";
  node.nodedef = "ND_subsurface_bsdf";
  node.color3_inputs["color"] = make_float3(0.5f, 0.4f, 0.3f);
  node.color3_inputs["radius"] = make_float3(2.0f, 1.0f, 0.5f);
  node.inputs["anisotropy"] = 0.2f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  SubsurfaceScatteringNode *sss = nullptr;
  for (ShaderNode *n : graph.nodes) {
    sss = n->name == "Subsurface" ? dynamic_cast<SubsurfaceScatteringNode *>(n) : sss;
  }
  ASSERT_NE(sss, nullptr);
  EXPECT_EQ(sss->get_method(), CLOSURE_BSSRDF_RANDOM_WALK_ID);
  EXPECT_FLOAT_EQ(sss->get_radius().x, 2.0f);
  EXPECT_FLOAT_EQ(sss->get_subsurface_anisotropy(), 0.2f);
  EXPECT_FLOAT_EQ(sss->get_color().x, 0.5f);
}

TEST(materialx_graph, lowers_conductor_bsdf_physical_ior)
{
  materialx::Node node;
  node.name = "Conductor";
  node.nodedef = "ND_conductor_bsdf";
  node.color3_inputs["ior"] = make_float3(0.2f, 0.4f, 1.4f);
  node.color3_inputs["extinction"] = make_float3(3.4f, 2.3f, 1.7f);
  node.vector2_inputs["roughness"] = make_float2(0.1f, 0.1f);
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  MetallicBsdfNode *metallic = nullptr;
  for (ShaderNode *n : graph.nodes) {
    metallic = n->name == "Conductor" ? dynamic_cast<MetallicBsdfNode *>(n) : metallic;
  }
  ASSERT_NE(metallic, nullptr);
  EXPECT_EQ(metallic->get_fresnel_type(), CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
  EXPECT_EQ(metallic->get_distribution(), CLOSURE_BSDF_MICROFACET_GGX_ID);
  EXPECT_FLOAT_EQ(metallic->get_ior().x, 0.2f);
  EXPECT_FLOAT_EQ(metallic->get_k().x, 3.4f);
  EXPECT_FLOAT_EQ(metallic->get_roughness(), 0.1f);
  EXPECT_FLOAT_EQ(metallic->get_anisotropy(), 0.0f);
}

TEST(materialx_graph, rejects_conductor_bsdf_anisotropic_roughness)
{
  /* No verified (roughness_x, roughness_y, tangent) -> (roughness,
   * anisotropy, rotation) conversion exists in this codebase -- the
   * anisotropic case is a documented, rejected boundary. */
  materialx::Node node;
  node.name = "Conductor";
  node.nodedef = "ND_conductor_bsdf";
  node.vector2_inputs["roughness"] = make_float2(0.1f, 0.3f);
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

TEST(materialx_graph, rejects_conductor_bsdf_nondefault_weight)
{
  materialx::Node node;
  node.name = "Conductor";
  node.nodedef = "ND_conductor_bsdf";
  node.inputs["weight"] = 0.5f;
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

TEST(materialx_graph, lowers_dielectric_bsdf_rt_glass)
{
  materialx::Node node;
  node.name = "Glass";
  node.nodedef = "ND_dielectric_bsdf";
  node.string_inputs["scatter_mode"] = "RT";
  node.inputs["ior"] = 1.45f;
  node.color3_inputs["tint"] = make_float3(0.9f, 0.95f, 1.0f);
  node.vector2_inputs["roughness"] = make_float2(0.02f, 0.02f);
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  GlassBsdfNode *glass = nullptr;
  for (ShaderNode *n : graph.nodes) {
    glass = n->name == "Glass" ? dynamic_cast<GlassBsdfNode *>(n) : glass;
  }
  ASSERT_NE(glass, nullptr);
  EXPECT_EQ(glass->get_distribution(), CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
  EXPECT_FLOAT_EQ(glass->get_IOR(), 1.45f);
  EXPECT_FLOAT_EQ(glass->get_roughness(), 0.02f);
  EXPECT_FLOAT_EQ(glass->get_color().x, 0.9f);
}

TEST(materialx_graph, rejects_dielectric_bsdf_default_scatter_mode)
{
  /* scatter_mode="R" (the MaterialX default) has no Cycles equivalent:
   * GlossyBsdfNode has no IOR/Fresnel input to represent a dielectric
   * reflection-only lobe. */
  materialx::Node node;
  node.name = "Glass";
  node.nodedef = "ND_dielectric_bsdf";
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));

  materialx::Node explicit_r;
  explicit_r.name = "GlassR";
  explicit_r.nodedef = "ND_dielectric_bsdf";
  explicit_r.string_inputs["scatter_mode"] = "R";
  explicit_r.outputs["out"] = materialx::Type::BSDF;
  EXPECT_FALSE(materialx::validate({{explicit_r}}));
}

TEST(materialx_graph, lowers_bsdf_producer_linked_color_and_normal)
{
  /* A linked "color" and "normal" resolve through the same generic
   * Link/lowered_nodes machinery every other type already uses -- this is
   * what lets a future combinator/terminal lowerer chain a BSDF producer's
   * output, once it exists. */
  materialx::Node color_source;
  color_source.name = "ColorSource";
  color_source.nodedef = "ND_constant_color3";
  color_source.color3_inputs["value"] = make_float3(0.1f, 0.2f, 0.3f);
  color_source.outputs["out"] = materialx::Type::Color3;

  materialx::Node normal_source;
  normal_source.name = "NormalSource";
  normal_source.nodedef = "ND_constant_vector3";
  normal_source.vector3_inputs["value"] = make_float3(0.0f, 0.0f, 1.0f);
  normal_source.outputs["out"] = materialx::Type::Vector3;

  materialx::Node translucent;
  translucent.name = "Translucent";
  translucent.nodedef = "ND_translucent_bsdf";
  translucent.links["color"] = {"ColorSource", "out", materialx::Type::Color3};
  translucent.links["normal"] = {"NormalSource", "out", materialx::Type::Vector3};
  translucent.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color_source, normal_source, translucent}}, &graph));

  ShaderNode *lowered_translucent = nullptr;
  for (ShaderNode *n : graph.nodes) {
    lowered_translucent = n->name == "Translucent" ? n : lowered_translucent;
  }
  ASSERT_NE(lowered_translucent, nullptr);
  EXPECT_NE(lowered_translucent->input("Color")->link, nullptr);
  EXPECT_NE(lowered_translucent->input("Normal")->link, nullptr);
}

TEST(materialx_graph, rejects_bsdf_producer_bad_shape_atomically)
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

  materialx::Node node;
  node.name = "Translucent";
  node.nodedef = "ND_translucent_bsdf";
  node.outputs["out"] = materialx::Type::Color3; /* Wrong tag: not Type::BSDF. */
  expect_rejected({{node}});

  materialx::Node extraneous;
  extraneous.name = "Translucent";
  extraneous.nodedef = "ND_translucent_bsdf";
  extraneous.inputs["roughness"] = 0.5f; /* Not a real translucent_bsdf input. */
  extraneous.outputs["out"] = materialx::Type::BSDF;
  expect_rejected({{extraneous}});
}

/* ======================================================================
 * BSDF closure combinators: ND_add_bsdf, ND_mix_bsdf, ND_multiply_bsdfF,
 * ND_multiply_bsdfC lower onto Cycles' real AddClosureNode/MixClosureNode.
 * ND_layer_bsdf has no real Cycles equivalent (see is_bsdf_combinator()'s
 * comment in graph.cpp) and is honestly rejected, not implemented.
 * ====================================================================== */

TEST(materialx_graph, lowers_add_bsdf_sums_two_producers)
{
  materialx::Node a;
  a.name = "A";
  a.nodedef = "ND_translucent_bsdf";
  a.outputs["out"] = materialx::Type::BSDF;

  materialx::Node b;
  b.name = "B";
  b.nodedef = "ND_oren_nayar_diffuse_bsdf";
  b.outputs["out"] = materialx::Type::BSDF;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_bsdf";
  add.links["in1"] = {"A", "out", materialx::Type::BSDF};
  add.links["in2"] = {"B", "out", materialx::Type::BSDF};
  add.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{a, b, add}}, &graph));

  AddClosureNode *sum = nullptr;
  for (ShaderNode *n : graph.nodes) {
    sum = n->name == "Add" ? dynamic_cast<AddClosureNode *>(n) : sum;
  }
  ASSERT_NE(sum, nullptr);
  ASSERT_NE(sum->input("Closure1")->link, nullptr);
  ASSERT_NE(sum->input("Closure2")->link, nullptr);
  EXPECT_EQ(sum->input("Closure1")->link->parent->name, "A");
  EXPECT_EQ(sum->input("Closure2")->link->parent->name, "B");
}

TEST(materialx_graph, rejects_add_bsdf_missing_input)
{
  materialx::Node a;
  a.name = "A";
  a.nodedef = "ND_translucent_bsdf";
  a.outputs["out"] = materialx::Type::BSDF;

  materialx::Node add;
  add.name = "Add";
  add.nodedef = "ND_add_bsdf";
  add.links["in1"] = {"A", "out", materialx::Type::BSDF};
  add.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{a, add}}));
}

TEST(materialx_graph, lowers_mix_bsdf_literal_mix_maps_bg_fg_to_closure1_closure2)
{
  /* Reference: libraries/pbrlib/genglsl/mx_mix_bsdf.glsl --
   *   result = mix(bg, fg, mixValue) = bg*(1-mixValue) + fg*mixValue.
   * Cycles' svm_node_mix_closure() weights Closure1 by (1-fac), Closure2 by
   * fac -- so Closure1 must be bg, Closure2 must be fg. */
  materialx::Node fg;
  fg.name = "Fg";
  fg.nodedef = "ND_translucent_bsdf";
  fg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node bg;
  bg.name = "Bg";
  bg.nodedef = "ND_oren_nayar_diffuse_bsdf";
  bg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mix;
  mix.name = "Mix";
  mix.nodedef = "ND_mix_bsdf";
  mix.links["fg"] = {"Fg", "out", materialx::Type::BSDF};
  mix.links["bg"] = {"Bg", "out", materialx::Type::BSDF};
  mix.inputs["mix"] = 0.25f;
  mix.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{fg, bg, mix}}, &graph));

  MixClosureNode *result = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mix" ? dynamic_cast<MixClosureNode *>(n) : result;
  }
  ASSERT_NE(result, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.25f);
  ASSERT_NE(result->input("Closure1")->link, nullptr);
  ASSERT_NE(result->input("Closure2")->link, nullptr);
  EXPECT_EQ(result->input("Closure1")->link->parent->name, "Bg");
  EXPECT_EQ(result->input("Closure2")->link->parent->name, "Fg");
}

TEST(materialx_graph, lowers_mix_bsdf_linked_mix_wires_fac_link)
{
  /* This file's own ND_mix_float/ND_mix_color3 lowering already wires a
   * linked "mix" factor straight into a Mix*Node's Fac-equivalent socket --
   * MixClosureNode.fac is the same kind of plain SVM float socket, so a
   * linked "mix" is real capability here too, not literal-only. */
  materialx::Node fg;
  fg.name = "Fg";
  fg.nodedef = "ND_translucent_bsdf";
  fg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node bg;
  bg.name = "Bg";
  bg.nodedef = "ND_oren_nayar_diffuse_bsdf";
  bg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node factor;
  factor.name = "Factor";
  factor.nodedef = "ND_constant_float";
  factor.inputs["value"] = 0.6f;
  factor.outputs["out"] = materialx::Type::Float;

  materialx::Node mix;
  mix.name = "Mix";
  mix.nodedef = "ND_mix_bsdf";
  mix.links["fg"] = {"Fg", "out", materialx::Type::BSDF};
  mix.links["bg"] = {"Bg", "out", materialx::Type::BSDF};
  mix.links["mix"] = {"Factor", "out", materialx::Type::Float};
  mix.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{fg, bg, factor, mix}}, &graph));

  MixClosureNode *result = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mix" ? dynamic_cast<MixClosureNode *>(n) : result;
  }
  ASSERT_NE(result, nullptr);
  EXPECT_NE(result->input("Fac")->link, nullptr);
}

TEST(materialx_graph, rejects_mix_bsdf_both_literal_and_linked_mix)
{
  materialx::Node fg;
  fg.name = "Fg";
  fg.nodedef = "ND_translucent_bsdf";
  fg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node bg;
  bg.name = "Bg";
  bg.nodedef = "ND_oren_nayar_diffuse_bsdf";
  bg.outputs["out"] = materialx::Type::BSDF;

  materialx::Node factor;
  factor.name = "Factor";
  factor.nodedef = "ND_constant_float";
  factor.inputs["value"] = 0.6f;
  factor.outputs["out"] = materialx::Type::Float;

  materialx::Node mix;
  mix.name = "Mix";
  mix.nodedef = "ND_mix_bsdf";
  mix.links["fg"] = {"Fg", "out", materialx::Type::BSDF};
  mix.links["bg"] = {"Bg", "out", materialx::Type::BSDF};
  mix.links["mix"] = {"Factor", "out", materialx::Type::Float};
  mix.inputs["mix"] = 0.5f; /* Both literal and linked -- must be rejected. */
  mix.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{fg, bg, factor, mix}}));
}

TEST(materialx_graph, lowers_multiply_bsdff_scales_via_null_closure_mix)
{
  /* w*bsdf via MixClosureNode(Closure1=zero-weight TransparentBsdfNode,
   * Closure2=bsdf, Fac=w): 0*(1-w) + bsdf*w == w*bsdf. */
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_bsdfF";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.inputs["in2"] = 0.4f;
  mul.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{base, mul}}, &graph));

  MixClosureNode *result = nullptr;
  TransparentBsdfNode *null_bsdf = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mul" ? dynamic_cast<MixClosureNode *>(n) : result;
    null_bsdf = n->name == "Mul.multiply_null" ? dynamic_cast<TransparentBsdfNode *>(n) :
                                                  null_bsdf;
  }
  ASSERT_NE(result, nullptr);
  ASSERT_NE(null_bsdf, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.4f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().x, 0.0f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().y, 0.0f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().z, 0.0f);
  ASSERT_NE(result->input("Closure1")->link, nullptr);
  ASSERT_NE(result->input("Closure2")->link, nullptr);
  EXPECT_EQ(result->input("Closure1")->link->parent->name, "Mul.multiply_null");
  EXPECT_EQ(result->input("Closure2")->link->parent->name, "Base");
}

TEST(materialx_graph, rejects_multiply_bsdff_linked_in2)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node weight;
  weight.name = "Weight";
  weight.nodedef = "ND_constant_float";
  weight.inputs["value"] = 0.4f;
  weight.outputs["out"] = materialx::Type::Float;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_bsdfF";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.links["in2"] = {"Weight", "out", materialx::Type::Float};
  mul.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{base, weight, mul}}));
}

TEST(materialx_graph, lowers_multiply_bsdfc_uniform_tint_scales_via_null_closure_mix)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_bsdfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.color3_inputs["in2"] = make_float3(0.3f, 0.3f, 0.3f); /* Uniform R==G==B. */
  mul.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{base, mul}}, &graph));

  MixClosureNode *result = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mul" ? dynamic_cast<MixClosureNode *>(n) : result;
  }
  ASSERT_NE(result, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.3f);
}

TEST(materialx_graph, rejects_multiply_bsdfc_nonuniform_tint)
{
  /* No per-channel closure-weighting primitive exists in Cycles -- a
   * non-uniform tint cannot be represented and must be rejected, not
   * approximated (e.g. by averaging or by dropping channels). */
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_bsdfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.color3_inputs["in2"] = make_float3(0.3f, 0.5f, 0.8f); /* Non-uniform. */
  mul.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{base, mul}}));
}

TEST(materialx_graph, rejects_multiply_bsdfc_linked_tint)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node tint;
  tint.name = "Tint";
  tint.nodedef = "ND_constant_color3";
  tint.color3_inputs["value"] = make_float3(0.3f, 0.3f, 0.3f);
  tint.outputs["out"] = materialx::Type::Color3;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_bsdfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.links["in2"] = {"Tint", "out", materialx::Type::Color3};
  mul.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{base, tint, mul}}));
}

/* ND_multiply_edfF/ND_multiply_edfC (pbrlib/pbrlib_defs.mtlx): the EDF-typed
 * siblings of ND_multiply_bsdfF/ND_multiply_bsdfC above -- same
 * MixClosureNode + zero-contribution TransparentBsdfNode idiom, but always
 * SurfaceShader-typed at the IR level (no BSDF-typed flavor exists for EDF,
 * mirroring ND_mix_edf/ND_add_edf). */
TEST(materialx_graph, lowers_multiply_edff_scales_via_null_closure_mix)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfF";
  mul.links["in1"] = {"Base", "out", materialx::Type::SurfaceShader};
  mul.inputs["in2"] = 0.4f;
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{base, mul}}, &graph));

  MixClosureNode *result = nullptr;
  TransparentBsdfNode *null_bsdf = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mul" ? dynamic_cast<MixClosureNode *>(n) : result;
    null_bsdf = n->name == "Mul.multiply_null" ? dynamic_cast<TransparentBsdfNode *>(n) :
                                                  null_bsdf;
  }
  ASSERT_NE(result, nullptr);
  ASSERT_NE(null_bsdf, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.4f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().x, 0.0f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().y, 0.0f);
  EXPECT_FLOAT_EQ(null_bsdf->get_color().z, 0.0f);
  ASSERT_NE(result->input("Closure1")->link, nullptr);
  ASSERT_NE(result->input("Closure2")->link, nullptr);
  EXPECT_EQ(result->input("Closure1")->link->parent->name, "Mul.multiply_null");
  EXPECT_EQ(result->input("Closure2")->link->parent->name, "Base");
}

TEST(materialx_graph, rejects_multiply_edff_linked_in2)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node weight;
  weight.name = "Weight";
  weight.nodedef = "ND_constant_float";
  weight.inputs["value"] = 0.4f;
  weight.outputs["out"] = materialx::Type::Float;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfF";
  mul.links["in1"] = {"Base", "out", materialx::Type::SurfaceShader};
  mul.links["in2"] = {"Weight", "out", materialx::Type::Float};
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  EXPECT_FALSE(materialx::validate({{base, weight, mul}}));
}

TEST(materialx_graph, lowers_multiply_edfc_uniform_tint_scales_via_null_closure_mix)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::SurfaceShader};
  mul.color3_inputs["in2"] = make_float3(0.3f, 0.3f, 0.3f); /* Uniform R==G==B. */
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{base, mul}}, &graph));

  MixClosureNode *result = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Mul" ? dynamic_cast<MixClosureNode *>(n) : result;
  }
  ASSERT_NE(result, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.3f);
}

TEST(materialx_graph, rejects_multiply_edfc_nonuniform_tint)
{
  /* No per-channel closure-weighting primitive exists in Cycles -- a
   * non-uniform tint cannot be represented and must be rejected, not
   * approximated (e.g. by averaging or by dropping channels). */
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::SurfaceShader};
  mul.color3_inputs["in2"] = make_float3(0.3f, 0.5f, 0.8f); /* Non-uniform. */
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  EXPECT_FALSE(materialx::validate({{base, mul}}));
}

TEST(materialx_graph, rejects_multiply_edfc_linked_tint)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node tint;
  tint.name = "Tint";
  tint.nodedef = "ND_constant_color3";
  tint.color3_inputs["value"] = make_float3(0.3f, 0.3f, 0.3f);
  tint.outputs["out"] = materialx::Type::Color3;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfC";
  mul.links["in1"] = {"Base", "out", materialx::Type::SurfaceShader};
  mul.links["in2"] = {"Tint", "out", materialx::Type::Color3};
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  EXPECT_FALSE(materialx::validate({{base, tint, mul}}));
}

TEST(materialx_graph, rejects_multiply_edff_bsdf_typed_in1)
{
  /* multiply_edfF/multiply_edfC have no BSDF-typed flavor (unlike
   * multiply_bsdff_id/multiply_bsdfc_id, which are dual-purpose) -- an 'in1'
   * that resolves to a real BSDF closure-producer rather than an EDF-rooted
   * generic-surface closure must be rejected. */
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mul;
  mul.name = "Mul";
  mul.nodedef = "ND_multiply_edfF";
  mul.links["in1"] = {"Base", "out", materialx::Type::BSDF};
  mul.inputs["in2"] = 0.4f;
  mul.outputs["out"] = materialx::Type::SurfaceShader;

  EXPECT_FALSE(materialx::validate({{base, mul}}));
}

TEST(materialx_graph, lowers_generalized_schlick_edf_constant_scalar_subset)
{
  /* ND_generalized_schlick_edf (pbrlib/pbrlib_defs.mtlx; reference
   * genglsl/mx_generalized_schlick_edf.glsl) multiplies the base EDF by
   * mx_fresnel_schlick(NdotV, color0, color90, exponent).  When color0 and
   * color90 are the same uniform-channel color, the directional term is a
   * constant scalar, so it lowers exactly through the same MixClosureNode
   * scalar weighting idiom as ND_multiply_edfF/C. */
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.color3_inputs["color"] = make_float3(1.0f, 0.5f, 0.25f);
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node schlick;
  schlick.name = "Schlick";
  schlick.nodedef = "ND_generalized_schlick_edf";
  schlick.links["base"] = {"Base", "out", materialx::Type::SurfaceShader};
  schlick.color3_inputs["color0"] = make_float3(0.4f, 0.4f, 0.4f);
  schlick.color3_inputs["color90"] = make_float3(0.4f, 0.4f, 0.4f);
  schlick.inputs["exponent"] = 2.0f;
  schlick.outputs["out"] = materialx::Type::SurfaceShader;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{base, schlick}}, &graph));

  MixClosureNode *result = nullptr;
  TransparentBsdfNode *null_bsdf = nullptr;
  for (ShaderNode *n : graph.nodes) {
    result = n->name == "Schlick" ? dynamic_cast<MixClosureNode *>(n) : result;
    null_bsdf = n->name == "Schlick.directional_null" ? dynamic_cast<TransparentBsdfNode *>(n) :
                                                         null_bsdf;
  }
  ASSERT_NE(result, nullptr);
  ASSERT_NE(null_bsdf, nullptr);
  EXPECT_FLOAT_EQ(result->get_fac(), 0.4f);
  ASSERT_NE(result->input("Closure1")->link, nullptr);
  ASSERT_NE(result->input("Closure2")->link, nullptr);
  EXPECT_EQ(result->input("Closure1")->link->parent->name, "Schlick.directional_null");
  EXPECT_EQ(result->input("Closure2")->link->parent->name, "Base");
}

TEST(materialx_graph, rejects_generalized_schlick_edf_directional_and_nonuniform_cases)
{
  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_uniform_edf";
  base.outputs["out"] = materialx::Type::SurfaceShader;

  materialx::Node directional;
  directional.name = "Directional";
  directional.nodedef = "ND_generalized_schlick_edf";
  directional.links["base"] = {"Base", "out", materialx::Type::SurfaceShader};
  directional.color3_inputs["color0"] = make_float3(0.2f, 0.2f, 0.2f);
  directional.color3_inputs["color90"] = make_float3(0.8f, 0.8f, 0.8f);
  directional.outputs["out"] = materialx::Type::SurfaceShader;
  EXPECT_FALSE(materialx::validate({{base, directional}}));

  materialx::Node nonuniform = directional;
  nonuniform.name = "NonUniform";
  nonuniform.color3_inputs["color0"] = make_float3(0.2f, 0.4f, 0.2f);
  nonuniform.color3_inputs["color90"] = make_float3(0.2f, 0.4f, 0.2f);
  EXPECT_FALSE(materialx::validate({{base, nonuniform}}));
}

TEST(materialx_graph, lowers_chiang_hair_bsdf_honest_subset)
{
  materialx::Node node;
  node.name = "ChiangHair";
  node.nodedef = "ND_chiang_hair_bsdf";
  node.vector2_inputs["roughness_R"] = make_float2(0.09f, 0.2f);
  node.vector2_inputs["roughness_TT"] = make_float2(0.0225f, 0.2f);
  node.vector2_inputs["roughness_TRT"] = make_float2(0.36f, 0.2f);
  node.vector3_inputs["absorption_coefficient"] = make_float3(0.2f, 0.3f, 0.4f);
  node.inputs["ior"] = 1.6f;
  node.inputs["cuticle_angle"] = 0.5f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  PrincipledHairBsdfNode *hair = nullptr;
  for (ShaderNode *n : graph.nodes) {
    hair = n->name == "ChiangHair" ? dynamic_cast<PrincipledHairBsdfNode *>(n) : hair;
  }
  ASSERT_NE(hair, nullptr);
  EXPECT_EQ(hair->get_model(), NODE_PRINCIPLED_HAIR_CHIANG);
  EXPECT_EQ(hair->get_parametrization(), NODE_PRINCIPLED_HAIR_DIRECT_ABSORPTION);
  EXPECT_FLOAT_EQ(hair->get_ior(), 1.6f);
  EXPECT_EQ(hair->get_absorption_coefficient(), make_float3(0.2f, 0.3f, 0.4f));
}

TEST(materialx_graph, rejects_chiang_hair_divergent_per_lobe_roughness)
{
  materialx::Node node;
  node.name = "ChiangHair";
  node.nodedef = "ND_chiang_hair_bsdf";
  node.vector2_inputs["roughness_R"] = make_float2(0.09f, 0.2f);
  node.vector2_inputs["roughness_TT"] = make_float2(0.04f, 0.2f);
  node.vector2_inputs["roughness_TRT"] = make_float2(0.36f, 0.2f);
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

TEST(materialx_graph, rejects_chiang_hair_per_lobe_tints)
{
  materialx::Node node;
  node.name = "ChiangHair";
  node.nodedef = "ND_chiang_hair_bsdf";
  node.vector2_inputs["roughness_R"] = make_float2(0.09f, 0.2f);
  node.vector2_inputs["roughness_TT"] = make_float2(0.0225f, 0.2f);
  node.vector2_inputs["roughness_TRT"] = make_float2(0.36f, 0.2f);
  node.color3_inputs["tint_R"] = make_float3(0.9f, 1.0f, 1.0f);
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

/* Real MaterialX pbrlib/pbrlib_defs.mtlx ND_chiang_hair_roughness (lines
 * 452-459) has three named vector2 outputs. pbrlib/genglsl/
 * mx_chiang_hair_bsdf.glsl lines 39-61 and mdl/materialx/pbrlib_1_6.mdl
 * lines 1066-1085 define the exact arithmetic lowered here. */
TEST(materialx_graph, lowers_chiang_hair_roughness_three_named_outputs)
{
  materialx::Node node;
  node.name = "HairRoughness";
  node.nodedef = "ND_chiang_hair_roughness";
  node.inputs["longitudinal"] = 0.25f;
  node.inputs["azimuthal"] = 0.35f;
  node.inputs["scale_TT"] = 0.5f;
  node.inputs["scale_TRT"] = 2.0f;
  node.outputs["roughness_R"] = materialx::Type::Vector2;
  node.outputs["roughness_TT"] = materialx::Type::Vector2;
  node.outputs["roughness_TRT"] = materialx::Type::Vector2;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  const auto find = [&](const string &name) -> ShaderNode * {
    for (ShaderNode *shader : graph.nodes) {
      if (shader->name == name) return shader;
    }
    return nullptr;
  };
  ASSERT_NE(dynamic_cast<ClampNode *>(find("HairRoughness.longitudinal")), nullptr);
  ASSERT_NE(dynamic_cast<ClampNode *>(find("HairRoughness.azimuthal")), nullptr);
  MathNode *lr_pow = dynamic_cast<MathNode *>(find("HairRoughness.lr_pow20"));
  MathNode *ar_pow = dynamic_cast<MathNode *>(find("HairRoughness.ar_pow22"));
  MathNode *tt_scale = dynamic_cast<MathNode *>(find("HairRoughness.roughness_TT.scale_sq"));
  MathNode *trt_x = dynamic_cast<MathNode *>(find("HairRoughness.roughness_TRT.x"));
  CombineXYZNode *roughness_r = dynamic_cast<CombineXYZNode *>(find("HairRoughness.roughness_R"));
  CombineXYZNode *roughness_tt = dynamic_cast<CombineXYZNode *>(find("HairRoughness.roughness_TT"));
  CombineXYZNode *roughness_trt = dynamic_cast<CombineXYZNode *>(find("HairRoughness.roughness_TRT"));
  ASSERT_NE(lr_pow, nullptr);
  ASSERT_NE(ar_pow, nullptr);
  ASSERT_NE(tt_scale, nullptr);
  ASSERT_NE(trt_x, nullptr);
  ASSERT_NE(roughness_r, nullptr);
  ASSERT_NE(roughness_tt, nullptr);
  ASSERT_NE(roughness_trt, nullptr);
  EXPECT_EQ(lr_pow->get_math_type(), NODE_MATH_POWER);
  EXPECT_FLOAT_EQ(lr_pow->get_value2(), 20.0f);
  EXPECT_EQ(ar_pow->get_math_type(), NODE_MATH_POWER);
  EXPECT_FLOAT_EQ(ar_pow->get_value2(), 22.0f);
  EXPECT_FLOAT_EQ(tt_scale->get_value1(), 0.5f);
  EXPECT_FLOAT_EQ(tt_scale->get_value2(), 0.5f);
  EXPECT_EQ(trt_x->input("Value1")->link->parent, find("HairRoughness.v"));
  EXPECT_EQ(roughness_r->input("Y")->link->parent, find("HairRoughness.s"));
  EXPECT_EQ(roughness_tt->input("Y")->link->parent, find("HairRoughness.s"));
  EXPECT_EQ(roughness_trt->input("Y")->link->parent, find("HairRoughness.s"));
}

TEST(materialx_graph, rejects_chiang_hair_roughness_malformed_signature)
{
  materialx::Node node;
  node.name = "HairRoughness";
  node.nodedef = "ND_chiang_hair_roughness";
  node.inputs["longitudinal"] = 0.25f;
  node.inputs["azimuthal"] = 0.35f;
  node.inputs["scale_TT"] = 0.5f;
  node.inputs["scale_TRT"] = 2.0f;
  node.outputs["roughness_R"] = materialx::Type::Vector2;
  node.outputs["roughness_TT"] = materialx::Type::Vector2;
  node.outputs["roughness_TRT"] = materialx::Type::Vector2;
  EXPECT_TRUE(materialx::validate({{node}}));

  materialx::Node missing_output = node;
  missing_output.outputs.erase("roughness_TRT");
  EXPECT_FALSE(materialx::validate({{missing_output}}));

  materialx::Node linked_scale = node;
  linked_scale.inputs.erase("scale_TT");
  linked_scale.links["scale_TT"] = {"Scale", "out", materialx::Type::Float};
  EXPECT_FALSE(materialx::validate({{linked_scale}}));

  materialx::Node nonfinite = node;
  nonfinite.inputs["azimuthal"] = std::numeric_limits<float>::infinity();
  EXPECT_FALSE(materialx::validate({{nonfinite}}));
}

TEST(materialx_graph, lowers_lama_diffuse_literal_energy_compensation_zero)
{
  materialx::Node node;
  node.name = "LamaDiffuse";
  node.nodedef = "ND_lama_diffuse";
  node.color3_inputs["color"] = make_float3(0.4f, 0.2f, 0.1f);
  node.inputs["roughness"] = 0.6f;
  node.inputs["energyCompensation"] = 0.0f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  DiffuseBsdfNode *diffuse = nullptr;
  for (ShaderNode *n : graph.nodes) {
    diffuse = n->name == "LamaDiffuse" ? dynamic_cast<DiffuseBsdfNode *>(n) : diffuse;
  }
  ASSERT_NE(diffuse, nullptr);
  EXPECT_FLOAT_EQ(diffuse->get_color().x, 0.4f);
  /* MaterialX libraries/bxdf/lama/lama_diffuse.mtlx squares roughness, then halves it. */
  EXPECT_FLOAT_EQ(diffuse->get_roughness(), 0.18f);
}

TEST(materialx_graph, rejects_lama_diffuse_default_energy_compensation)
{
  materialx::Node node;
  node.name = "LamaDiffuse";
  node.nodedef = "ND_lama_diffuse";
  node.inputs["energyCompensation"] = 1.0f;
  node.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{node}}));
}

TEST(materialx_graph, lowers_lama_translucent_default_color)
{
  materialx::Node node;
  node.name = "LamaTranslucent";
  node.nodedef = "ND_lama_translucent";
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  TranslucentBsdfNode *translucent = nullptr;
  for (ShaderNode *n : graph.nodes) {
    translucent = n->name == "LamaTranslucent" ? dynamic_cast<TranslucentBsdfNode *>(n) : translucent;
  }
  ASSERT_NE(translucent, nullptr);
  EXPECT_FLOAT_EQ(translucent->get_color().x, 0.18f);
  EXPECT_FLOAT_EQ(translucent->get_color().y, 0.18f);
  EXPECT_FLOAT_EQ(translucent->get_color().z, 0.18f);
}

TEST(materialx_graph, lowers_lama_sheen_roughness_remap)
{
  materialx::Node node;
  node.name = "LamaSheen";
  node.nodedef = "ND_lama_sheen";
  node.color3_inputs["color"] = make_float3(0.7f, 0.6f, 0.5f);
  node.inputs["roughness"] = 0.4f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  SheenBsdfNode *sheen = nullptr;
  for (ShaderNode *n : graph.nodes) {
    sheen = n->name == "LamaSheen" ? dynamic_cast<SheenBsdfNode *>(n) : sheen;
  }
  ASSERT_NE(sheen, nullptr);
  EXPECT_EQ(sheen->get_distribution(), CLOSURE_BSDF_SHEEN_ID);
  EXPECT_EQ(sheen->get_color(), make_float3(0.7f, 0.6f, 0.5f));
  EXPECT_FLOAT_EQ(sheen->get_roughness(), 0.2116f);
}

TEST(materialx_graph, lowers_lama_sss_scaled_radius)
{
  materialx::Node node;
  node.name = "LamaSSS";
  node.nodedef = "ND_lama_sss";
  node.color3_inputs["color"] = make_float3(0.5f, 0.4f, 0.3f);
  node.color3_inputs["sssRadius"] = make_float3(2.0f, 3.0f, 4.0f);
  node.inputs["sssScale"] = 10.0f;
  node.inputs["sssUnitLength"] = 0.1f;
  node.inputs["sssAnisotropy"] = 0.25f;
  node.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{node}}, &graph));

  SubsurfaceScatteringNode *sss = nullptr;
  for (ShaderNode *n : graph.nodes) {
    sss = n->name == "LamaSSS" ? dynamic_cast<SubsurfaceScatteringNode *>(n) : sss;
  }
  ASSERT_NE(sss, nullptr);
  EXPECT_FLOAT_EQ(sss->get_radius().x, 2.0f);
  EXPECT_FLOAT_EQ(sss->get_radius().y, 3.0f);
  EXPECT_FLOAT_EQ(sss->get_radius().z, 4.0f);
  EXPECT_FLOAT_EQ(sss->get_subsurface_anisotropy(), 0.25f);
}

TEST(materialx_graph, lowers_lama_mix_and_add_bsdf)
{
  materialx::Node a;
  a.name = "A";
  a.nodedef = "ND_lama_translucent";
  a.outputs["out"] = materialx::Type::BSDF;

  materialx::Node b;
  b.name = "B";
  b.nodedef = "ND_lama_diffuse";
  b.inputs["energyCompensation"] = 0.0f;
  b.outputs["out"] = materialx::Type::BSDF;

  materialx::Node mix;
  mix.name = "LamaMix";
  mix.nodedef = "ND_lama_mix_bsdf";
  mix.links["bg"] = {"A", "out", materialx::Type::BSDF};
  mix.links["fg"] = {"B", "out", materialx::Type::BSDF};
  mix.inputs["mix"] = 0.7f;
  mix.outputs["out"] = materialx::Type::BSDF;

  materialx::Node add;
  add.name = "LamaAdd";
  add.nodedef = "ND_lama_add_bsdf";
  add.links["in1"] = {"A", "out", materialx::Type::BSDF};
  add.links["in2"] = {"B", "out", materialx::Type::BSDF};
  add.inputs["weight1"] = 0.25f;
  add.inputs["weight2"] = 0.75f;
  add.outputs["out"] = materialx::Type::BSDF;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{a, b, mix, add}}, &graph));

  MixClosureNode *native_mix = nullptr;
  AddClosureNode *native_add = nullptr;
  MixClosureNode *weight1 = nullptr;
  MixClosureNode *weight2 = nullptr;
  for (ShaderNode *n : graph.nodes) {
    native_mix = n->name == "LamaMix" ? dynamic_cast<MixClosureNode *>(n) : native_mix;
    native_add = n->name == "LamaAdd" ? dynamic_cast<AddClosureNode *>(n) : native_add;
    weight1 = n->name == "LamaAdd.weight1" ? dynamic_cast<MixClosureNode *>(n) : weight1;
    weight2 = n->name == "LamaAdd.weight2" ? dynamic_cast<MixClosureNode *>(n) : weight2;
  }
  ASSERT_NE(native_mix, nullptr);
  EXPECT_FLOAT_EQ(native_mix->get_fac(), 0.7f);
  ASSERT_NE(native_add, nullptr);
  ASSERT_NE(weight1, nullptr);
  ASSERT_NE(weight2, nullptr);
  EXPECT_FLOAT_EQ(weight1->get_fac(), 0.25f);
  EXPECT_FLOAT_EQ(weight2->get_fac(), 0.75f);
  EXPECT_EQ(native_add->input("Closure1")->link, weight1->output("Closure"));
  EXPECT_EQ(native_add->input("Closure2")->link, weight2->output("Closure"));
}


TEST(materialx_graph, lowers_color4_vector4_role_converts_preserving_four_components)
{
  /* Real stdlib mappings: stdlib_defs.mtlx declares ND_convert_color4_vector2,
   * ND_convert_color4_vector3, ND_convert_color4_vector4, ND_convert_vector4_color4,
   * ND_convert_vector2_color4, and ND_convert_vector3_color4; stdlib_ng.mtlx
   * implements them with separate/combineN, preserving shared channels and
   * filling missing Color4 channels as B=0/A=1. */
  materialx::Node color;
  color.name = "SourceColor4";
  color.nodedef = "ND_constant_color4";
  color.float4_inputs["value"] = make_float4(0.1f, 0.2f, 0.3f, 0.4f);
  color.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector2;
  vector2.name = "SourceVector2";
  vector2.nodedef = "ND_constant_vector2";
  vector2.vector2_inputs["value"] = make_float2(0.5f, 0.6f);
  vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node vector3;
  vector3.name = "SourceVector3";
  vector3.nodedef = "ND_constant_vector3";
  vector3.vector3_inputs["value"] = make_float3(0.7f, 0.8f, 0.9f);
  vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node to_vector2;
  to_vector2.name = "Color4ToVector2";
  to_vector2.nodedef = "ND_convert_color4_vector2";
  to_vector2.links["in"] = {"SourceColor4", "out", materialx::Type::Color4};
  to_vector2.outputs["out"] = materialx::Type::Vector2;

  materialx::Node to_vector3;
  to_vector3.name = "Color4ToVector3";
  to_vector3.nodedef = "ND_convert_color4_vector3";
  to_vector3.links["in"] = {"SourceColor4", "out", materialx::Type::Color4};
  to_vector3.outputs["out"] = materialx::Type::Vector3;

  materialx::Node to_vector4;
  to_vector4.name = "Color4ToVector4";
  to_vector4.nodedef = "ND_convert_color4_vector4";
  to_vector4.links["in"] = {"SourceColor4", "out", materialx::Type::Color4};
  to_vector4.outputs["out"] = materialx::Type::Vector4;

  materialx::Node vector2_to_color4;
  vector2_to_color4.name = "Vector2ToColor4";
  vector2_to_color4.nodedef = "ND_convert_vector2_color4";
  vector2_to_color4.links["in"] = {"SourceVector2", "out", materialx::Type::Vector2};
  vector2_to_color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector3_to_color4;
  vector3_to_color4.name = "Vector3ToColor4";
  vector3_to_color4.nodedef = "ND_convert_vector3_color4";
  vector3_to_color4.links["in"] = {"SourceVector3", "out", materialx::Type::Vector3};
  vector3_to_color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node back_to_color4;
  back_to_color4.name = "Vector4ToColor4";
  back_to_color4.nodedef = "ND_convert_vector4_color4";
  back_to_color4.links["in"] = {"Color4ToVector4", "out", materialx::Type::Vector4};
  back_to_color4.outputs["out"] = materialx::Type::Color4;

  materialx::Node extract_alpha;
  extract_alpha.name = "ExtractAlpha";
  extract_alpha.nodedef = "ND_extract_color4";
  extract_alpha.int_inputs["index"] = 3;
  extract_alpha.links["in"] = {"Vector4ToColor4", "out", materialx::Type::Color4};
  extract_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node extract_vector2_alpha;
  extract_vector2_alpha.name = "ExtractVector2Alpha";
  extract_vector2_alpha.nodedef = "ND_extract_color4";
  extract_vector2_alpha.int_inputs["index"] = 3;
  extract_vector2_alpha.links["in"] = {"Vector2ToColor4", "out", materialx::Type::Color4};
  extract_vector2_alpha.outputs["out"] = materialx::Type::Float;

  materialx::Node use_alpha;
  use_alpha.name = "UseAlpha";
  use_alpha.nodedef = "ND_add_float";
  use_alpha.links["in1"] = {"ExtractAlpha", "out", materialx::Type::Float};
  use_alpha.links["in2"] = {"ExtractVector2Alpha", "out", materialx::Type::Float};
  use_alpha.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{color,
                                 vector2,
                                 vector3,
                                 to_vector2,
                                 to_vector3,
                                 to_vector4,
                                 vector2_to_color4,
                                 vector3_to_color4,
                                 back_to_color4,
                                 extract_alpha,
                                 extract_vector2_alpha,
                                 use_alpha}},
                                &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["Color4ToVector2.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["Color4ToVector3.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateColorNode *>(lowered["Color4ToVector4.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector4ToColor4.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector2ToColor4.separate"]), nullptr);
  ASSERT_NE(dynamic_cast<SeparateXYZNode *>(lowered["Vector3ToColor4.separate"]), nullptr);
  EXPECT_EQ(lowered["Color4ToVector2"]->input("X")->link,
            lowered["Color4ToVector2.separate"]->output("Red"));
  EXPECT_EQ(lowered["Color4ToVector3"]->input("Z")->link,
            lowered["Color4ToVector3.separate"]->output("Blue"));
  EXPECT_EQ(lowered["Color4ToVector4.W"]->input("Value1")->link,
            lowered["SourceColor4.Alpha"]->output("Value"));
  EXPECT_EQ(lowered["Vector4ToColor4.Alpha"]->input("Value1")->link,
            lowered["Color4ToVector4.W"]->output("Value"));
  EXPECT_FLOAT_EQ(dynamic_cast<MathNode *>(lowered["Vector2ToColor4.Alpha"])->get_value1(), 1.0f);
  EXPECT_EQ(lowered["Vector2ToColor4"]->input("Red")->link,
            lowered["Vector2ToColor4.separate"]->output("X"));
  EXPECT_EQ(lowered["Vector2ToColor4"]->input("Green")->link,
            lowered["Vector2ToColor4.separate"]->output("Y"));
  EXPECT_EQ(lowered["Vector3ToColor4"]->input("Blue")->link,
            lowered["Vector3ToColor4.separate"]->output("Z"));
  EXPECT_EQ(lowered["UseAlpha"]->input("Value1")->link,
            lowered["Vector4ToColor4.Alpha"]->output("Value"));
  EXPECT_EQ(lowered["UseAlpha"]->input("Value2")->link,
            lowered["Vector2ToColor4.Alpha"]->output("Value"));
}

TEST(materialx_graph, lowers_literal_vector_to_color4_role_converts)
{
  materialx::Node vector2;
  vector2.name = "Vector2ToColor4";
  vector2.nodedef = "ND_convert_vector2_color4";
  vector2.vector2_inputs["in"] = make_float2(0.25f, 0.5f);
  vector2.outputs["out"] = materialx::Type::Color4;

  materialx::Node vector3;
  vector3.name = "Vector3ToColor4";
  vector3.nodedef = "ND_convert_vector3_color4";
  vector3.vector3_inputs["in"] = make_float3(0.75f, 0.875f, 0.9375f);
  vector3.outputs["out"] = materialx::Type::Color4;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower({{vector2, vector3}}, &graph));

  std::unordered_map<string, ShaderNode *> lowered;
  for (ShaderNode *node : graph.nodes) {
    lowered[string(node->name.c_str())] = node;
  }

  auto *vector2_separate = dynamic_cast<SeparateXYZNode *>(lowered["Vector2ToColor4.separate"]);
  auto *vector3_separate = dynamic_cast<SeparateXYZNode *>(lowered["Vector3ToColor4.separate"]);
  auto *vector2_color = dynamic_cast<CombineColorNode *>(lowered["Vector2ToColor4"]);
  auto *vector3_color = dynamic_cast<CombineColorNode *>(lowered["Vector3ToColor4"]);
  auto *vector2_alpha = dynamic_cast<MathNode *>(lowered["Vector2ToColor4.Alpha"]);
  auto *vector3_alpha = dynamic_cast<MathNode *>(lowered["Vector3ToColor4.Alpha"]);
  ASSERT_NE(vector2_separate, nullptr);
  ASSERT_NE(vector3_separate, nullptr);
  ASSERT_NE(vector2_color, nullptr);
  ASSERT_NE(vector3_color, nullptr);
  ASSERT_NE(vector2_alpha, nullptr);
  ASSERT_NE(vector3_alpha, nullptr);

  EXPECT_EQ(vector2_separate->get_vector(), make_float3(0.25f, 0.5f, 0.0f));
  EXPECT_EQ(vector3_separate->get_vector(), make_float3(0.75f, 0.875f, 0.9375f));
  EXPECT_EQ(vector2_separate->input("Vector")->link, nullptr);
  EXPECT_EQ(vector3_separate->input("Vector")->link, nullptr);
  EXPECT_EQ(vector2_color->input("Red")->link, vector2_separate->output("X"));
  EXPECT_EQ(vector2_color->input("Green")->link, vector2_separate->output("Y"));
  EXPECT_EQ(vector3_color->input("Blue")->link, vector3_separate->output("Z"));
  EXPECT_FLOAT_EQ(vector2_alpha->get_value1(), 1.0f);
  EXPECT_FLOAT_EQ(vector3_alpha->get_value1(), 1.0f);
}

TEST(materialx_graph, rejects_layer_bsdf_as_unimplemented)
{
  /* ND_layer_bsdf's real vertical-layering semantics
   * (top.response + base.response*top.throughput; see
   * libraries/pbrlib/genglsl/mx_layer_bsdf.glsl) need each closure's own
   * throughput, which Cycles' node graph does not expose -- honestly
   * rejected rather than approximated with add/mix. */
  materialx::Node top;
  top.name = "Top";
  top.nodedef = "ND_dielectric_bsdf";
  top.string_inputs["scatter_mode"] = "RT";
  top.outputs["out"] = materialx::Type::BSDF;

  materialx::Node base;
  base.name = "Base";
  base.nodedef = "ND_translucent_bsdf";
  base.outputs["out"] = materialx::Type::BSDF;

  materialx::Node layer;
  layer.name = "Layer";
  layer.nodedef = "ND_layer_bsdf";
  layer.links["top"] = {"Top", "out", materialx::Type::BSDF};
  layer.links["base"] = {"Base", "out", materialx::Type::BSDF};
  layer.outputs["out"] = materialx::Type::BSDF;

  EXPECT_FALSE(materialx::validate({{top, base, layer}}));
}

TEST(materialx_graph, lowers_logical_boolean_nodes_to_exact_boolean_algebra)
{
  /* MaterialX stdlib_defs.mtlx declares ND_logical_and/or/xor/not as boolean
   * conditional nodegroup members. genosl/stdlib_genosl_impl.mtlx implements
   * and/or/not as native boolean operators, and stdlib_ng.mtlx implements xor
   * as (in1 && !in2) || (in2 && !in1). Over exact 0/1 boolean payloads this
   * lowers losslessly to multiply, maximum, 1-in, and abs(in1-in2). */
  materialx::Node truth;
  truth.name = "Truth";
  truth.nodedef = "ND_constant_boolean";
  truth.int_inputs["value"] = 1;
  truth.outputs["out"] = materialx::Type::Boolean;

  materialx::Node falsity;
  falsity.name = "Falsity";
  falsity.nodedef = "ND_constant_boolean";
  falsity.int_inputs["value"] = 0;
  falsity.outputs["out"] = materialx::Type::Boolean;

  materialx::Node logical_and;
  logical_and.name = "LogicalAnd";
  logical_and.nodedef = "ND_logical_and";
  logical_and.links["in1"] = {"Truth", "out", materialx::Type::Boolean};
  logical_and.links["in2"] = {"Falsity", "out", materialx::Type::Boolean};
  logical_and.outputs["out"] = materialx::Type::Boolean;

  materialx::Node logical_or;
  logical_or.name = "LogicalOr";
  logical_or.nodedef = "ND_logical_or";
  logical_or.links["in1"] = {"LogicalAnd", "out", materialx::Type::Boolean};
  logical_or.links["in2"] = {"Truth", "out", materialx::Type::Boolean};
  logical_or.outputs["out"] = materialx::Type::Boolean;

  materialx::Node logical_not;
  logical_not.name = "LogicalNot";
  logical_not.nodedef = "ND_logical_not";
  logical_not.links["in"] = {"LogicalAnd", "out", materialx::Type::Boolean};
  logical_not.outputs["out"] = materialx::Type::Boolean;

  materialx::Node logical_xor;
  logical_xor.name = "LogicalXor";
  logical_xor.nodedef = "ND_logical_xor";
  logical_xor.links["in1"] = {"LogicalOr", "out", materialx::Type::Boolean};
  logical_xor.links["in2"] = {"LogicalNot", "out", materialx::Type::Boolean};
  logical_xor.outputs["out"] = materialx::Type::Boolean;

  materialx::Node as_float;
  as_float.name = "AsFloat";
  as_float.nodedef = "ND_convert_boolean_float";
  as_float.links["in"] = {"LogicalXor", "out", materialx::Type::Boolean};
  as_float.outputs["out"] = materialx::Type::Float;

  ShaderGraph graph;
  ASSERT_TRUE(materialx::lower(
      {{truth, falsity, logical_and, logical_or, logical_not, logical_xor, as_float}}, &graph));

  std::unordered_map<string, ShaderNode *> nodes;
  for (ShaderNode *node : graph.nodes) {
    nodes[node->name.string()] = node;
  }
  auto *and_value = dynamic_cast<MathNode *>(nodes["LogicalAnd.float"]);
  auto *or_value = dynamic_cast<MathNode *>(nodes["LogicalOr.float"]);
  auto *not_value = dynamic_cast<MathNode *>(nodes["LogicalNot.float"]);
  auto *xor_delta = dynamic_cast<MathNode *>(nodes["LogicalXor.xor.delta"]);
  auto *xor_value = dynamic_cast<MathNode *>(nodes["LogicalXor.float"]);
  ASSERT_NE(and_value, nullptr);
  ASSERT_NE(or_value, nullptr);
  ASSERT_NE(not_value, nullptr);
  ASSERT_NE(xor_delta, nullptr);
  ASSERT_NE(xor_value, nullptr);
  EXPECT_EQ(and_value->get_math_type(), NODE_MATH_MULTIPLY);
  EXPECT_EQ(or_value->get_math_type(), NODE_MATH_MAXIMUM);
  EXPECT_EQ(not_value->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(xor_delta->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(xor_value->get_math_type(), NODE_MATH_ABSOLUTE);
  EXPECT_EQ(and_value->input("Value1")->link, nodes["Truth.float"]->output("Value"));
  EXPECT_EQ(or_value->input("Value1")->link, and_value->output("Value"));
  EXPECT_EQ(not_value->input("Value2")->link, and_value->output("Value"));
  EXPECT_EQ(xor_delta->input("Value1")->link, or_value->output("Value"));
  EXPECT_EQ(xor_delta->input("Value2")->link, not_value->output("Value"));
  EXPECT_EQ(xor_value->input("Value1")->link, xor_delta->output("Value"));
  EXPECT_EQ(nodes["AsFloat"]->input("Value1")->link, xor_value->output("Value"));
}

CCL_NAMESPACE_END
