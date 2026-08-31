/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <unordered_map>

#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/shader.h>

#include "materialx/authority_pipeline.h"
#include "materialx/graph.h"
#include "materialx/usdshade_reader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/sha256.h"

CCL_NAMESPACE_BEGIN

namespace {

class TemporaryImage {
 public:
  TemporaryImage()
      : path_(std::filesystem::temp_directory_path() / "cycles_materialx_reader_test.ppm")
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


TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector_rotation_utilities)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/TestMaterial/UV"));
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f, 0.75f));
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  pxr::UsdShadeShader rotate2d = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Rotate2D"));
  rotate2d.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_rotate2d_vector2")));
  ASSERT_TRUE(rotate2d.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  rotate2d.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).Set(90.0f);
  rotate2d.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  pxr::UsdShadeShader to_vector3 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ToVector3"));
  to_vector3.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector2_vector3")));
  ASSERT_TRUE(to_vector3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(rotate2d.ConnectableAPI(), pxr::TfToken("out")));
  to_vector3.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  pxr::UsdShadeShader angle = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Angle"));
  angle.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  angle.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(180.0f);
  angle.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  pxr::UsdShadeShader rotate3d = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Rotate3D"));
  rotate3d.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_rotate3d_vector3")));
  ASSERT_TRUE(rotate3d.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(to_vector3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(rotate3d.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(angle.ConnectableAPI(), pxr::TfToken("out")));
  rotate3d.CreateInput(pxr::TfToken("axis"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.0f, 1.0f, 0.0f));
  rotate3d.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(rotate3d.ConnectableAPI(), pxr::TfToken("out")));
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  const materialx::Node *read_rotate2d = nullptr;
  const materialx::Node *read_rotate3d = nullptr;
  for (const materialx::Node &node : graph.nodes) {
    read_rotate2d = node.nodedef == "ND_rotate2d_vector2" ? &node : read_rotate2d;
    read_rotate3d = node.nodedef == "ND_rotate3d_vector3" ? &node : read_rotate3d;
  }
  ASSERT_NE(read_rotate2d, nullptr);
  EXPECT_EQ(read_rotate2d->links.at("in").type, materialx::Type::Vector2);
  EXPECT_FLOAT_EQ(read_rotate2d->inputs.at("amount"), 90.0f);
  ASSERT_NE(read_rotate3d, nullptr);
  EXPECT_EQ(read_rotate3d->links.at("amount").type, materialx::Type::Float);
  EXPECT_EQ(read_rotate3d->vector3_inputs.at("axis"), make_float3(0.0f, 1.0f, 0.0f));

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  VectorRotateNode *native_rotate2d = nullptr;
  VectorRotateNode *native_rotate3d = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_rotate2d = node->name == "Rotate2D" ? dynamic_cast<VectorRotateNode *>(node) : native_rotate2d;
    native_rotate3d = node->name == "Rotate3D" ? dynamic_cast<VectorRotateNode *>(node) : native_rotate3d;
  }
  ASSERT_NE(native_rotate2d, nullptr);
  EXPECT_EQ(native_rotate2d->get_rotate_type(), NODE_VECTOR_ROTATE_TYPE_AXIS_Z);
  ASSERT_NE(native_rotate3d, nullptr);
  EXPECT_EQ(native_rotate3d->get_rotate_type(), NODE_VECTOR_ROTATE_TYPE_AXIS);
}

TEST(materialx_usdshade_reader, rejects_unsafe_vector_rotation_utilities_without_mutating_graph)
{
  const auto expect_rejected = [](const char *nodedef,
                                  const pxr::SdfValueTypeName &vector_type,
                                  const pxr::VtValue &input_value,
                                  const pxr::VtValue &axis_value,
                                  const float amount,
                                  const bool connect_amount) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial"));
    pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

    pxr::UsdShadeShader rotate = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/Rotate"));
    rotate.CreateIdAttr(pxr::VtValue(pxr::TfToken(nodedef)));
    rotate.CreateInput(pxr::TfToken("in"), vector_type).Set(input_value);
    if (connect_amount) {
      pxr::UsdShadeShader angle = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/TestMaterial/Angle"));
      angle.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
      angle.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(amount);
      angle.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
      ASSERT_TRUE(rotate.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float)
                      .ConnectToSource(angle.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      rotate.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).Set(amount);
    }
    if (string(nodedef) == "ND_rotate3d_vector3") {
      rotate.CreateInput(pxr::TfToken("axis"), pxr::SdfValueTypeNames->Float3).Set(axis_value);
    }
    rotate.CreateOutput(pxr::TfToken("out"), vector_type);

    if (vector_type == pxr::SdfValueTypeNames->Float2) {
      pxr::UsdShadeShader to_vector3 = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/TestMaterial/ToVector3"));
      to_vector3.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector2_vector3")));
      ASSERT_TRUE(to_vector3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                      .ConnectToSource(rotate.ConnectableAPI(), pxr::TfToken("out")));
      to_vector3.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
      pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
      normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
      ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                      .ConnectToSource(to_vector3.ConnectableAPI(), pxr::TfToken("out")));
      normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
      ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                      .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
      normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
      ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                      .ConnectToSource(rotate.ConnectableAPI(), pxr::TfToken("out")));
      normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
      ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                      .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
    }
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    EXPECT_NE(error.find("rotate"), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  };

  expect_rejected("ND_rotate2d_vector2",
                  pxr::SdfValueTypeNames->Float2,
                  pxr::VtValue(pxr::GfVec2f(1.0f, 0.0f)),
                  pxr::VtValue(pxr::GfVec3f(0.0f)),
                  std::numeric_limits<float>::infinity(),
                  false);
  expect_rejected("ND_rotate3d_vector3",
                  pxr::SdfValueTypeNames->Float3,
                  pxr::VtValue(pxr::GfVec3f(1.0f, 0.0f, 0.0f)),
                  pxr::VtValue(pxr::GfVec3f(0.0f, 0.0f, 0.0f)),
                  45.0f,
                  false);
}

TEST(materialx_usdshade_reader, reads_open_pbr_roughness_multiply_into_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessMultiply"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/First"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Second"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.7f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 2);
  EXPECT_EQ(graph.nodes[0].name, "RoughnessMultiply");
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_multiply_float");
  ASSERT_EQ(graph.nodes[0].inputs.size(), 2);
  EXPECT_FLOAT_EQ(graph.nodes[0].inputs.at("in1"), 0.7f);
  EXPECT_FLOAT_EQ(graph.nodes[0].inputs.at("in2"), 0.2f);
  EXPECT_EQ(graph.nodes[0].outputs.at("out"), materialx::Type::Float);

  EXPECT_EQ(graph.nodes[1].name, "OpenPBR");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_open_pbr_surface_surfaceshader");
  EXPECT_EQ(graph.nodes[1].outputs.at("out"), materialx::Type::SurfaceShader);
  const materialx::Link &roughness = graph.nodes[1].links.at("specular_roughness");
  EXPECT_EQ(roughness.source_node, "RoughnessMultiply");
  EXPECT_EQ(roughness.source_output, "out");
  EXPECT_EQ(roughness.type, materialx::Type::Float);
}

TEST(materialx_usdshade_reader, reads_and_lowers_luminance_color3_with_literal_coefficients)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Color"));
  pxr::UsdShadeShader luminance = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Luminance"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  luminance.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_luminance_color3")));
  luminance.CreateInput(pxr::TfToken("lumacoeffs"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2126f, 0.7152f, 0.0722f));
  luminance.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(luminance.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(luminance.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 3);
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_luminance_color3");
  EXPECT_EQ(graph.nodes[1].links.at("in").source_node, "Color");
  EXPECT_EQ(graph.nodes[1].outputs.at("out"), materialx::Type::Float);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  VectorMathNode *dot = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "Luminance") dot = dynamic_cast<VectorMathNode *>(node);
  }
  ASSERT_NE(dot, nullptr);
  EXPECT_EQ(dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
}

TEST(materialx_usdshade_reader, rejects_luminance_color3_with_dynamic_coefficients_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Color"));
  pxr::UsdShadeShader coefficients = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Coefficients"));
  pxr::UsdShadeShader luminance = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Luminance"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  for (pxr::UsdShadeShader shader : {color, coefficients}) {
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
    shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  }
  luminance.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_luminance_color3")));
  luminance.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(luminance.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(luminance.CreateInput(pxr::TfToken("lumacoeffs"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(coefficients.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(luminance.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("lumacoeffs"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_nested_standard_binary_float_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  const pxr::UsdShadeNodeGraph scalar_graph = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ScalarGraph"));
  pxr::UsdShadeShader add = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ScalarGraph/Add"));
  pxr::UsdShadeShader subtract = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ScalarGraph/Subtract"));
  pxr::UsdShadeShader divide = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ScalarGraph/Divide"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  add.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_float")));
  add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  add.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  subtract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_subtract_float")));
  ASSERT_TRUE(subtract.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  subtract.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.1f);
  subtract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  divide.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_divide_float")));
  ASSERT_TRUE(divide.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(subtract.ConnectableAPI(), pxr::TfToken("out")));
  divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  divide.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(scalar_graph.CreateOutput(pxr::TfToken("result"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(scalar_graph.ConnectableAPI(), pxr::TfToken("result")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 4);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_add_float");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_subtract_float");
  EXPECT_EQ(graph.nodes[1].links.at("in1").source_node, "Add");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_divide_float");
  EXPECT_EQ(graph.nodes[2].links.at("in1").source_node, "Subtract");
  EXPECT_EQ(graph.nodes[3].links.at("specular_roughness").source_node, "Divide");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  MathNode *native_add = nullptr;
  MathNode *native_subtract = nullptr;
  MathNode *native_divide = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "Add") {
      native_add = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "Subtract") {
      native_subtract = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "Divide") {
      native_divide = dynamic_cast<MathNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(native_add, nullptr);
  ASSERT_NE(native_subtract, nullptr);
  ASSERT_NE(native_divide, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(native_add->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(native_subtract->get_math_type(), NODE_MATH_SUBTRACT);
  EXPECT_EQ(native_divide->get_math_type(), NODE_MATH_DIVIDE);
  EXPECT_EQ(native_subtract->input("Value1")->link, native_add->output("Value"));
  EXPECT_EQ(native_divide->input("Value1")->link, native_subtract->output("Value"));
  EXPECT_EQ(principled->input("Roughness")->link, native_divide->output("Value"));
}

TEST(materialx_usdshade_reader, rejects_invalid_divide_float_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader divide = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Divide"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  divide.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_divide_float")));
  divide.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  divide.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("in2"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");

  divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(1.0f));
  error.clear();
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("float input 'in2'"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_dynamic_or_nonfinite_vector2_divide_denominators_without_mutating_graph)
{
  const auto expect_rejected = [](const pxr::GfVec2f &denominator, const bool dynamic) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/Vector2Divide"));
    const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
      pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/Vector2Divide").AppendChild(pxr::TfToken(name)));
      result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
      result.CreateOutput(pxr::TfToken("out"), type);
      return result;
    };
    pxr::UsdShadeShader surface = shader(
        "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader divide = shader("Divide", "ND_divide_vector2", pxr::SdfValueTypeNames->Float2);
    pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
    divide.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, 2.0f));
    if (dynamic) {
      pxr::UsdShadeShader constant = shader("Denominator", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
      constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(denominator);
      ASSERT_TRUE(divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2)
                      .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).Set(denominator);
    }
    /* Extracting and combining makes the vector2 result a valid float3 normalmap source. */
    pxr::UsdShadeShader extract_x = shader("ExtractX", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
    pxr::UsdShadeShader extract_y = shader("ExtractY", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
    pxr::UsdShadeShader combine = shader("Combine", "ND_combine3_vector3", pxr::SdfValueTypeNames->Float3);
    for (const auto item : {std::pair{&extract_x, 0}, std::pair{&extract_y, 1}}) {
      item.first->CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(item.second);
      ASSERT_TRUE(item.first->CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                      .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
    }
    ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract_x.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract_y.ConnectableAPI(), pxr::TfToken("out")));
    combine.CreateInput(pxr::TfToken("in3"), pxr::SdfValueTypeNames->Float).Set(1.0f);
    ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                    .ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                    .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    EXPECT_NE(error.find("in2"), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  };
  expect_rejected(pxr::GfVec2f(1.0f, 2.0f), true);
  expect_rejected(pxr::GfVec2f(1.0f, 0.0f), false);
  expect_rejected(pxr::GfVec2f(std::numeric_limits<float>::quiet_NaN(), 1.0f), false);
  expect_rejected(pxr::GfVec2f(std::numeric_limits<float>::infinity(), 1.0f), false);
}

TEST(materialx_usdshade_reader, rejects_dynamic_or_nonfinite_vector3_divide_denominators_without_mutating_graph)
{
  const auto expect_rejected = [](const pxr::GfVec3f &denominator, const bool dynamic) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/Vector3Divide"));
    const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
      pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/Vector3Divide").AppendChild(pxr::TfToken(name)));
      result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
      result.CreateOutput(pxr::TfToken("out"), type);
      return result;
    };
    pxr::UsdShadeShader surface = shader(
        "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader divide = shader("Divide", "ND_divide_vector3", pxr::SdfValueTypeNames->Float3);
    pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
    divide.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(1.0f, 2.0f, 3.0f));
    if (dynamic) {
      pxr::UsdShadeShader constant = shader("Denominator", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
      constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(denominator);
      ASSERT_TRUE(divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                      .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3).Set(denominator);
    }
    ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                    .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                    .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    EXPECT_NE(error.find("in2"), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  };
  expect_rejected(pxr::GfVec3f(1.0f, 2.0f, 3.0f), true);
  expect_rejected(pxr::GfVec3f(1.0f, 0.0f, 3.0f), false);
  expect_rejected(pxr::GfVec3f(std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.0f), false);
  expect_rejected(pxr::GfVec3f(std::numeric_limits<float>::infinity(), 1.0f, 1.0f), false);
}

TEST(materialx_usdshade_reader,
     rejects_nonfinite_vector_scalar_divisors_and_preserves_finite_linked_rhs)
{
  struct MaterialFixture {
    pxr::UsdStageRefPtr stage;
    pxr::UsdShadeMaterial material;
  };

  for (const bool vector2 : {true, false}) {
    const char *const nodedef = vector2 ? "ND_divide_vector2FA" : "ND_divide_vector3FA";
    const pxr::SdfValueTypeName vector_type = vector2 ? pxr::SdfValueTypeNames->Float2 :
                                                       pxr::SdfValueTypeNames->Float3;
    const auto make_material = [&](const char *path, const float divisor, const bool linked_rhs) {
      MaterialFixture fixture;
      fixture.stage = pxr::UsdStage::CreateInMemory();
      EXPECT_TRUE(fixture.stage);
      fixture.material = pxr::UsdShadeMaterial::Define(fixture.stage, pxr::SdfPath(path));
      const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
        pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
            fixture.stage, pxr::SdfPath(path).AppendChild(pxr::TfToken(name)));
        result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
        result.CreateOutput(pxr::TfToken("out"), type);
        return result;
      };
      pxr::UsdShadeShader surface = shader(
          "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
      pxr::UsdShadeShader input = shader(
          "Input", vector2 ? "ND_constant_vector2" : "ND_constant_vector3", vector_type);
      pxr::UsdShadeShader divide = shader("Divide", nodedef, vector_type);
      pxr::UsdShadeShader extract = shader(
          "Extract", vector2 ? "ND_extract_vector2" : "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
      if (vector2) {
        input.CreateInput(pxr::TfToken("value"), vector_type).Set(pxr::GfVec2f(8.0f, 12.0f));
      }
      else {
        input.CreateInput(pxr::TfToken("value"), vector_type).Set(pxr::GfVec3f(8.0f, 12.0f, 16.0f));
      }
      EXPECT_TRUE(divide.CreateInput(pxr::TfToken("in1"), vector_type)
                      .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
      if (linked_rhs) {
        pxr::UsdShadeShader rhs = shader("Rhs", "ND_constant_float", pxr::SdfValueTypeNames->Float);
        rhs.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(divisor);
        EXPECT_TRUE(divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                        .ConnectToSource(rhs.ConnectableAPI(), pxr::TfToken("out")));
      }
      else {
        divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(divisor);
      }
      extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
      EXPECT_TRUE(extract.CreateInput(pxr::TfToken("in"), vector_type)
                      .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
      EXPECT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                      .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
      const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
      EXPECT_TRUE(fixture.material.CreateSurfaceOutput(context).ConnectToSource(
          surface.ConnectableAPI(), pxr::TfToken("out")));
      return fixture;
    };

    for (const float divisor : {std::numeric_limits<float>::quiet_NaN(),
                                std::numeric_limits<float>::infinity(),
                                -std::numeric_limits<float>::infinity()})
    {
      const MaterialFixture fixture = make_material(
          vector2 ? "/Looks/Vector2NonfiniteScalarDivide" : "/Looks/Vector3NonfiniteScalarDivide",
          divisor,
          false);
      materialx::Graph graph;
      graph.nodes.push_back({"sentinel", "unsupported"});
      string error;
      EXPECT_FALSE(materialx::read_usdshade_graph(fixture.material, &graph, &error)) << nodedef;
      EXPECT_NE(error.find("in2"), string::npos) << error;
      ASSERT_EQ(graph.nodes.size(), 1) << nodedef;
      EXPECT_EQ(graph.nodes[0].name, "sentinel") << nodedef;
    }

    const MaterialFixture fixture = make_material(
        vector2 ? "/Looks/Vector2LinkedScalarDivide" : "/Looks/Vector3LinkedScalarDivide",
        2.0f,
        true);
    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(fixture.material, &graph, &error)) << error;
    const auto divide = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.nodedef == nodedef;
    });
    ASSERT_NE(divide, graph.nodes.end()) << nodedef;
    ASSERT_EQ(divide->links.count("in2"), 1) << nodedef;
    EXPECT_EQ(divide->links.at("in2").source_node, "Rhs") << nodedef;
    EXPECT_EQ(divide->links.at("in2").type, materialx::Type::Float) << nodedef;
  }
}

TEST(materialx_usdshade_reader, rejects_cyclic_divide_float_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader divide = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Divide"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  divide.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_divide_float")));
  divide.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(divide.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
  divide.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(divide.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("cyclic"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_shared_scalar_dag_once_and_lowers)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/SharedMultiply"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 2);
  EXPECT_EQ(graph.nodes[0].name, "SharedMultiply");
  EXPECT_EQ(graph.nodes[1].links.at("base_weight").source_node, "SharedMultiply");
  EXPECT_EQ(graph.nodes[1].links.at("specular_roughness").source_node, "SharedMultiply");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int shared_math_node_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    shared_math_node_count += node->name == "SharedMultiply" &&
                              dynamic_cast<MathNode *>(node) != nullptr;
  }
  EXPECT_EQ(shared_math_node_count, 1);
}

TEST(materialx_usdshade_reader, disambiguates_same_named_scalar_nodes_in_nested_graphs)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  const pxr::UsdShadeNodeGraph first_graph = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/FirstGraph"));
  const pxr::UsdShadeNodeGraph second_graph = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/SecondGraph"));
  pxr::UsdShadeShader first_math = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/FirstGraph/Math"));
  pxr::UsdShadeShader second_math = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/SecondGraph/Math"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  first_math.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_float")));
  first_math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  first_math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.3f);
  first_math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second_math.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_divide_float")));
  second_math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  second_math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  second_math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(first_graph.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first_math.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(second_graph.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second_math.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first_graph.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(second_graph.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 3);
  EXPECT_NE(graph.nodes[0].name, graph.nodes[1].name);

  ShaderGraph lowered;
  EXPECT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, disambiguates_same_named_scalar_and_color_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader scalar_math = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/FirstGraph/Math"));
  pxr::UsdShadeShader color_math = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/SecondGraph/Math"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  scalar_math.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_divide_float")));
  scalar_math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  scalar_math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  scalar_math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  color_math.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  color_math.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  color_math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(scalar_math.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_math.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 3);
  EXPECT_NE(graph.nodes[0].name, graph.nodes[1].name);

  ShaderGraph lowered;
  EXPECT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_linked_base_color_constant_into_typed_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader constant = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/BaseColor"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  constant.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  constant.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 2);
  const materialx::Node &color = graph.nodes[0];
  EXPECT_EQ(color.name, "BaseColor");
  EXPECT_EQ(color.nodedef, "ND_constant_color3");
  EXPECT_EQ(color.outputs.at("out"), materialx::Type::Color3);
  const float3 value = color.color3_inputs.at("value");
  EXPECT_FLOAT_EQ(value.x, 0.2f);
  EXPECT_FLOAT_EQ(value.y, 0.4f);
  EXPECT_FLOAT_EQ(value.z, 0.6f);

  const materialx::Node &open_pbr = graph.nodes[1];
  EXPECT_EQ(open_pbr.name, "OpenPBR");
  EXPECT_EQ(open_pbr.nodedef, "ND_open_pbr_surface_surfaceshader");
  EXPECT_EQ(open_pbr.outputs.at("out"), materialx::Type::SurfaceShader);
  const materialx::Link &base_color = open_pbr.links.at("base_color");
  EXPECT_EQ(base_color.source_node, "BaseColor");
  EXPECT_EQ(base_color.source_output, "out");
  EXPECT_EQ(base_color.type, materialx::Type::Color3);
}

TEST(materialx_usdshade_reader, reads_typed_uv_image_chain_into_shared_graph)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/BaseColorImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage,
                                                       pxr::SdfPath("/Looks/TestMaterial/UV"));
  pxr::UsdShadeShader offset = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Offset"));
  pxr::UsdShadeShader add = pxr::UsdShadeShader::Define(stage,
                                                         pxr::SdfPath("/Looks/TestMaterial/AddUV"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  offset.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  offset.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.5f));
  offset.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  add.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vector2")));
  add.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(offset.ConnectableAPI(), pxr::TfToken("out")));

  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 5);
  const materialx::Node &uv_node = graph.nodes[0];
  EXPECT_EQ(uv_node.name, "UV");
  EXPECT_EQ(uv_node.nodedef, "ND_geompropvalue_vector2");
  EXPECT_EQ(uv_node.string_inputs.at("geomprop"), "st");
  EXPECT_EQ(uv_node.outputs.at("out"), materialx::Type::Vector2);

  const materialx::Node &offset_node = graph.nodes[1];
  EXPECT_EQ(offset_node.nodedef, "ND_constant_vector2");
  EXPECT_FLOAT_EQ(offset_node.vector2_inputs.at("value").x, 0.25f);
  EXPECT_FLOAT_EQ(offset_node.vector2_inputs.at("value").y, 0.5f);
  const materialx::Node &add_node = graph.nodes[2];
  EXPECT_EQ(add_node.nodedef, "ND_add_vector2");
  EXPECT_EQ(add_node.links.at("in1").source_node, "UV");
  EXPECT_EQ(add_node.links.at("in2").source_node, "Offset");

  const materialx::Node &image_node = graph.nodes[3];
  EXPECT_EQ(image_node.name, "BaseColorImage");
  EXPECT_EQ(image_node.nodedef, "ND_image_color3");
  EXPECT_EQ(image_node.asset_inputs.at("file"), image_asset.path());
  EXPECT_EQ(image_node.outputs.at("out"), materialx::Type::Color3);
  const materialx::Link &texcoord = image_node.links.at("texcoord");
  EXPECT_EQ(texcoord.source_node, "AddUV");
  EXPECT_EQ(texcoord.source_output, "out");
  EXPECT_EQ(texcoord.type, materialx::Type::Vector2);

  const materialx::Link &base_color = graph.nodes[4].links.at("base_color");
  EXPECT_EQ(base_color.source_node, "BaseColorImage");
  EXPECT_EQ(base_color.source_output, "out");
  EXPECT_EQ(base_color.type, materialx::Type::Color3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_bulk_coordinate_image_ingress_family)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/UV"));
  pxr::UsdShadeShader image_float = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/Mask"));
  pxr::UsdShadeShader image_vector2 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/Vector2Image"));
  pxr::UsdShadeShader magnitude = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/Vector2Magnitude"));
  pxr::UsdShadeShader scalar_attribute = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/RoughnessAttribute"));
  pxr::UsdShadeShader color_attribute = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImageIngress/BaseColorAttribute"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  image_float.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_float")));
  image_float.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image_float.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(image_float.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  image_vector2.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_vector2")));
  image_vector2.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image_vector2.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image_vector2.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));

  magnitude.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_magnitude_vector2")));
  magnitude.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(image_vector2.ConnectableAPI(), pxr::TfToken("out")));

  scalar_attribute.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_float")));
  scalar_attribute.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String)
      .Set("roughness_attr");
  scalar_attribute.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  color_attribute.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_color3")));
  color_attribute.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String)
      .Set("base_color_attr");
  color_attribute.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);

  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_attribute.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(image_float.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_metalness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar_attribute.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  const materialx::Node &mask = find_node("Mask");
  EXPECT_EQ(mask.nodedef, "ND_image_float");
  EXPECT_EQ(mask.outputs.at("out"), materialx::Type::Float);
  const materialx::Node &vector2 = find_node("Vector2Image");
  EXPECT_EQ(vector2.nodedef, "ND_image_vector2");
  EXPECT_EQ(vector2.outputs.at("out"), materialx::Type::Vector2);
  const materialx::Node &mask_uv = find_node(mask.links.at("texcoord").source_node.c_str());
  EXPECT_EQ(mask_uv.nodedef, "ND_geompropvalue_vector2");
  EXPECT_EQ(mask_uv.outputs.at("out"), materialx::Type::Vector2);
  const materialx::Node &vector2_uv = find_node(vector2.links.at("texcoord").source_node.c_str());
  EXPECT_EQ(vector2_uv.nodedef, "ND_geompropvalue_vector2");
  EXPECT_EQ(vector2_uv.outputs.at("out"), materialx::Type::Vector2);
  const materialx::Node &roughness = find_node("RoughnessAttribute");
  EXPECT_EQ(roughness.nodedef, "ND_geompropvalue_float");
  EXPECT_EQ(roughness.string_inputs.at("geomprop"), "roughness_attr");
  EXPECT_EQ(roughness.outputs.at("out"), materialx::Type::Float);
  const materialx::Node &base_color = find_node("BaseColorAttribute");
  EXPECT_EQ(base_color.nodedef, "ND_geompropvalue_color3");
  EXPECT_EQ(base_color.string_inputs.at("geomprop"), "base_color_attr");
  EXPECT_EQ(base_color.outputs.at("out"), materialx::Type::Color3);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_texture_normalmap_to_open_pbr_normals)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage,
                                                       pxr::SdfPath("/Looks/TestMaterial/UV"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  /* Omitted scale is MaterialX's unit-strength default. */
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_vector3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("geometry_coat_normal"), pxr::SdfValueTypeNames->Float3)
          .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 4);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_geompropvalue_vector2");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_image_vector3");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_normalmap_float");
  EXPECT_EQ(graph.nodes[3].links.at("geometry_normal").source_node, graph.nodes[2].name);
  EXPECT_EQ(graph.nodes[3].links.at("geometry_coat_normal").source_node, graph.nodes[2].name);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  ImageTextureNode *native_image = nullptr;
  NormalMapNode *native_normalmap = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_image = native_image ? native_image : dynamic_cast<ImageTextureNode *>(node);
    native_normalmap = native_normalmap ? native_normalmap : dynamic_cast<NormalMapNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(native_image, nullptr);
  ASSERT_NE(native_normalmap, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(native_normalmap->input("Color")->link, native_image->output("Color"));
  EXPECT_EQ(principled->input("Normal")->link, native_normalmap->output("Normal"));
  EXPECT_EQ(principled->input("Coat Normal")->link, native_normalmap->output("Normal"));
}

TEST(materialx_usdshade_reader, rejects_unsupported_normalmap_semantics_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.5f, 1.0f));
  normalmap.CreateInput(pxr::TfToken("scale"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  normalmap.CreateInput(pxr::TfToken("normal"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.0f, 0.0f, 1.0f));
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("basis"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");

  normalmap.GetPrim().RemoveProperty(pxr::TfToken("inputs:normal"));
  normalmap.GetInput(pxr::TfToken("scale")).Set(0.5f);
  error.clear();
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("scale"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");

  normalmap.GetInput(pxr::TfToken("scale")).Set(1.0f);
  pxr::UsdShadeShader scale = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Scale"));
  scale.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  scale.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  scale.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(normalmap.GetInput(pxr::TfToken("scale"))
                  .ConnectToSource(scale.ConnectableAPI(), pxr::TfToken("out")));
  error.clear();
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("scale"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_open_pbr_opacity_and_emission_literals)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateInput(pxr::TfToken("geometry_opacity"), pxr::SdfValueTypeNames->Float).Set(0.35f);
  surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 1.0f));
  surface.CreateInput(pxr::TfToken("emission_luminance"), pxr::SdfValueTypeNames->Float).Set(3.0f);
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 1);
  const materialx::Node &open_pbr = graph.nodes[0];
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("geometry_opacity"), 0.35f);
  EXPECT_FLOAT_EQ(open_pbr.color3_inputs.at("emission_color").x, 0.25f);
  EXPECT_FLOAT_EQ(open_pbr.color3_inputs.at("emission_color").y, 0.5f);
  EXPECT_FLOAT_EQ(open_pbr.color3_inputs.at("emission_color").z, 1.0f);
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("emission_luminance"), 3.0f);
}

TEST(materialx_usdshade_reader, reads_direct_open_pbr_coat_and_fuzz_inputs)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader fuzz_weight = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/FuzzWeight"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateInput(pxr::TfToken("coat_weight"), pxr::SdfValueTypeNames->Float).Set(0.4f);
  surface.CreateInput(pxr::TfToken("coat_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.8f, 0.6f, 0.4f));
  surface.CreateInput(pxr::TfToken("coat_roughness"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  surface.CreateInput(pxr::TfToken("coat_ior"), pxr::SdfValueTypeNames->Float).Set(1.45f);
  surface.CreateInput(pxr::TfToken("fuzz_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  surface.CreateInput(pxr::TfToken("fuzz_roughness"), pxr::SdfValueTypeNames->Float).Set(0.35f);
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  fuzz_weight.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  fuzz_weight.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.6f);
  fuzz_weight.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("fuzz_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(fuzz_weight.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 2);
  const materialx::Node &open_pbr = graph.nodes[1];
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("coat_weight"), 0.4f);
  EXPECT_EQ(open_pbr.color3_inputs.at("coat_color"), make_float3(0.8f, 0.6f, 0.4f));
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("coat_roughness"), 0.2f);
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("coat_ior"), 1.45f);
  EXPECT_EQ(open_pbr.color3_inputs.at("fuzz_color"), make_float3(0.25f, 0.5f, 0.75f));
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("fuzz_roughness"), 0.35f);
  EXPECT_EQ(open_pbr.links.at("fuzz_weight").source_node, graph.nodes[0].name);
}

TEST(materialx_usdshade_reader, rejects_non_equivalent_open_pbr_coat_input_without_mutation)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateInput(pxr::TfToken("coat_weight"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  surface.CreateInput(pxr::TfToken("coat_darkening"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("no direct Cycles equivalent"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_open_pbr_base_color_literal)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.02f, 0.8f, 0.08f));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 1);
  const float3 base_color = graph.nodes[0].color3_inputs.at("base_color");
  EXPECT_FLOAT_EQ(base_color.x, 0.02f);
  EXPECT_FLOAT_EQ(base_color.y, 0.8f);
  EXPECT_FLOAT_EQ(base_color.z, 0.08f);
}

TEST(materialx_usdshade_reader, reads_linked_opacity_and_emission_into_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader opacity = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Opacity"));
  pxr::UsdShadeShader emission_color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/EmissionColor"));
  pxr::UsdShadeShader emission_strength = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/EmissionStrength"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/First"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Second"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  opacity.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  opacity.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.4f);
  opacity.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  emission_color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  emission_color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(1.0f, 0.5f, 0.25f));
  emission_color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  emission_strength.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  emission_strength.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(3.0f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(emission_strength.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(emission_strength.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_opacity"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(opacity.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(emission_color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("emission_luminance"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(emission_strength.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 4);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_constant_float");
  EXPECT_FLOAT_EQ(graph.nodes[0].inputs.at("value"), 0.4f);
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_constant_color3");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_multiply_float");
  const materialx::Node &open_pbr = graph.nodes[3];
  EXPECT_EQ(open_pbr.links.at("geometry_opacity").source_node, "Opacity");
  EXPECT_EQ(open_pbr.links.at("geometry_opacity").type, materialx::Type::Float);
  EXPECT_EQ(open_pbr.links.at("emission_color").source_node, "EmissionColor");
  EXPECT_EQ(open_pbr.links.at("emission_color").type, materialx::Type::Color3);
  EXPECT_EQ(open_pbr.links.at("emission_luminance").source_node, "EmissionStrength");
  EXPECT_EQ(open_pbr.links.at("emission_luminance").type, materialx::Type::Float);
}

TEST(materialx_usdshade_reader, reads_open_pbr_primary_scalar_literals)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  surface.CreateInput(pxr::TfToken("base_metalness"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  surface.CreateInput(pxr::TfToken("specular_ior"), pxr::SdfValueTypeNames->Float).Set(1.45f);
  surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
      .Set(0.35f);
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 1);
  const materialx::Node &open_pbr = graph.nodes[0];
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("base_weight"), 0.8f);
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("base_metalness"), 0.2f);
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("specular_ior"), 1.45f);
  EXPECT_FLOAT_EQ(open_pbr.inputs.at("specular_roughness"), 0.35f);
}

TEST(materialx_usdshade_reader, reads_linked_primary_scalars_into_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  const auto define_constant = [&](const char *name, const float value) {
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath(string("/Looks/TestMaterial/") + name));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
    shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(value);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return shader;
  };
  const auto define_multiply = [&](const char *name,
                                   const float first_value,
                                   const float second_value) {
    pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath(string("/Looks/TestMaterial/") + name));
    pxr::UsdShadeShader first = define_constant((string(name) + "First").c_str(), first_value);
    pxr::UsdShadeShader second = define_constant((string(name) + "Second").c_str(), second_value);
    multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
    multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    EXPECT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
    EXPECT_TRUE(multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
    return multiply;
  };

  const pxr::UsdShadeShader weight = define_constant("Weight", 0.8f);
  const pxr::UsdShadeShader metalness = define_multiply("Metalness", 0.4f, 0.5f);
  const pxr::UsdShadeShader ior = define_constant("IOR", 1.45f);
  const pxr::UsdShadeShader roughness = define_multiply("Roughness", 0.7f, 0.5f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(weight.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_metalness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(metalness.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_ior"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(ior.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(roughness.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 5);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_constant_float");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_multiply_float");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_constant_float");
  EXPECT_EQ(graph.nodes[3].nodedef, "ND_multiply_float");
  const materialx::Node &open_pbr = graph.nodes[4];
  EXPECT_EQ(open_pbr.links.at("base_weight").source_node, "Weight");
  EXPECT_EQ(open_pbr.links.at("base_metalness").source_node, "Metalness");
  EXPECT_EQ(open_pbr.links.at("specular_ior").source_node, "IOR");
  EXPECT_EQ(open_pbr.links.at("specular_roughness").source_node, "Roughness");
  EXPECT_EQ(open_pbr.links.at("base_weight").type, materialx::Type::Float);
  EXPECT_EQ(open_pbr.links.at("base_metalness").type, materialx::Type::Float);
  EXPECT_EQ(open_pbr.links.at("specular_ior").type, materialx::Type::Float);
  EXPECT_EQ(open_pbr.links.at("specular_roughness").type, materialx::Type::Float);
}

TEST(materialx_usdshade_reader, reads_standard_binary_color3_nodes_into_shared_graph)
{
  const char *nodedefs[] = {
      "ND_add_color3",
      "ND_subtract_color3",
      "ND_multiply_color3",
      "ND_divide_color3",
      "ND_min_color3",
      "ND_max_color3"};
  for (const char *nodedef : nodedefs) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial"));
    pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
    pxr::UsdShadeShader color_math = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/ColorMath"));
    pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/First"));
    pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TestMaterial/Second"));

    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    color_math.CreateIdAttr(pxr::VtValue(pxr::TfToken(nodedef)));
    color_math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    for (pxr::UsdShadeShader *constant : {&first, &second}) {
      constant->CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
      constant->CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
      constant->CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    }
    ASSERT_TRUE(color_math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(color_math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(color_math.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                    .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error))
        << nodedef << ": " << error;

    ASSERT_EQ(graph.nodes.size(), 4);
    EXPECT_EQ(graph.nodes[0].nodedef, "ND_constant_color3");
    EXPECT_EQ(graph.nodes[1].nodedef, "ND_constant_color3");
    EXPECT_EQ(graph.nodes[2].nodedef, nodedef);
    EXPECT_EQ(graph.nodes[2].links.at("in1").source_node, "First");
    EXPECT_EQ(graph.nodes[2].links.at("in2").source_node, "Second");
    EXPECT_EQ(graph.nodes[2].links.at("in1").type, materialx::Type::Color3);
    EXPECT_EQ(graph.nodes[2].links.at("in2").type, materialx::Type::Color3);
    EXPECT_EQ(graph.nodes[2].outputs.at("out"), materialx::Type::Color3);
    EXPECT_EQ(graph.nodes[3].links.at("base_color").source_node, "ColorMath");
  }
}

TEST(materialx_usdshade_reader, reads_image_extract_color3_into_shared_graph)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader extract = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessChannel"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/PackedImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage,
                                                       pxr::SdfPath("/Looks/TestMaterial/UV"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color3")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 4);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_geompropvalue_vector2");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_image_color3");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_extract_color3");
  EXPECT_EQ(graph.nodes[2].int_inputs.at("index"), 1);
  EXPECT_EQ(graph.nodes[2].links.at("in").source_node, "PackedImage");
  EXPECT_EQ(graph.nodes[2].links.at("in").type, materialx::Type::Color3);
  EXPECT_EQ(graph.nodes[2].outputs.at("out"), materialx::Type::Float);
  EXPECT_EQ(graph.nodes[3].links.at("specular_roughness").source_node, "RoughnessChannel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_color4_component_arithmetic)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4ComponentArithmetic"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader uv = shader("UV");
  pxr::UsdShadeShader image = shader("Image");
  pxr::UsdShadeShader right = shader("Right");
  pxr::UsdShadeShader convert = shader("RGB");
  pxr::UsdShadeShader alpha = shader("Alpha");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  right.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_safepower_color4")));
  right.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(0.5f, 0.75f, 1.25f, 1.5f));
  right.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(2.0f, 3.0f, 4.0f, 5.0f));
  right.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);

  struct Case {
    const char *nodedef;
    bool linked_second;
  };
  const std::array<Case, 8> cases = {{{"ND_add_color4", true},
                                      {"ND_subtract_color4", true},
                                      {"ND_multiply_color4", true},
                                      {"ND_divide_color4", true},
                                      {"ND_min_color4", true},
                                      {"ND_max_color4", true},
                                      {"ND_modulo_color4", true},
                                      {"ND_power_color4", false}}};
  pxr::UsdShadeShader previous = image;
  for (const Case &test : cases) {
    pxr::UsdShadeShader math = shader(test.nodedef);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test.nodedef)));
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    ASSERT_TRUE(math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
    if (test.linked_second) {
      ASSERT_TRUE(math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f)
                      .ConnectToSource(right.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(2.0f, 3.0f, 4.0f, 2.0f));
    }
    previous = math;
  }
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                  pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  for (const Case &test : cases) {
    const auto found = std::find_if(
        graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
          return node.nodedef == test.nodedef;
        });
    ASSERT_NE(found, graph.nodes.end()) << test.nodedef;
    EXPECT_EQ(found->outputs.at("out"), materialx::Type::Color4) << test.nodedef;
    EXPECT_EQ(found->links.at("in1").type, materialx::Type::Color4) << test.nodedef;
    if (test.linked_second) {
      EXPECT_EQ(found->links.at("in2").type, materialx::Type::Color4) << test.nodedef;
    }
    else {
      EXPECT_FLOAT_EQ(found->float4_inputs.at("in2").w, 2.0f) << test.nodedef;
    }
  }
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  MathNode *alpha_add = nullptr;
  MathNode *alpha_power = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "ND_add_color4.Alpha") {
      alpha_add = dynamic_cast<MathNode *>(node);
    }
    else if (node->name == "ND_power_color4.Alpha") {
      alpha_power = dynamic_cast<MathNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(alpha_add, nullptr);
  ASSERT_NE(alpha_power, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_NE(alpha_add->input("Value2")->link, nullptr);
  EXPECT_EQ(alpha_power->get_math_type(), NODE_MATH_POWER);
  EXPECT_FLOAT_EQ(alpha_power->get_value2(), 2.0f);
  EXPECT_EQ(principled->input("Roughness")->link, alpha_power->output("Value"));
}

TEST(materialx_usdshade_reader, rejects_invalid_color4_component_arithmetic_without_mutation)
{
  struct Rejection {
    const char *nodedef;
    pxr::GfVec4f first;
    pxr::GfVec4f second;
    const char *expected_error;
  };
  const Rejection rejections[] = {
      {"ND_add_color4",
       pxr::GfVec4f(1.0f, 2.0f, std::numeric_limits<float>::infinity(), 4.0f),
       pxr::GfVec4f(1.0f),
       "finite"},
      {"ND_power_color4",
       pxr::GfVec4f(1.0f),
       pxr::GfVec4f(1.0f, 2.0f, 3.0f, std::numeric_limits<float>::quiet_NaN()),
       "finite"},
      {"ND_divide_color4",
       pxr::GfVec4f(1.0f),
       pxr::GfVec4f(1.0f, 0.0f, 1.0f, 1.0f),
       "nonzero"},
      {"ND_modulo_color4",
       pxr::GfVec4f(1.0f),
       pxr::GfVec4f(1.0f, 1.0f, 0.0f, 1.0f),
       "nonzero"}};
  for (const Rejection &test : rejections) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/InvalidColor4ComponentArithmetic"));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(
          stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader math = shader("Math");
    pxr::UsdShadeShader convert = shader("RGB");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test.nodedef)));
    math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f).Set(test.first);
    math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f).Set(test.second);
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
    convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(math.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));
    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)) << test.nodedef;
    EXPECT_NE(error.find(test.expected_error), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1) << test.nodedef;
    EXPECT_EQ(graph.nodes[0].name, "sentinel") << test.nodedef;
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_color4_component_arithmetic_defaults)
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
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/Color4ComponentArithmeticDefaults"));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(
          stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader math = shader("Math");
    pxr::UsdShadeShader convert = shader("RGB");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test_case.nodedef)));
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
    convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(math.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error))
        << test_case.nodedef << ": " << error;
    const auto operation = std::find_if(
        graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
          return node.nodedef == test_case.nodedef;
        });
    ASSERT_NE(operation, graph.nodes.end()) << test_case.nodedef;
    EXPECT_TRUE(operation->float4_inputs.empty()) << test_case.nodedef;
    EXPECT_TRUE(operation->links.empty()) << test_case.nodedef;

    ShaderGraph lowered;
    ASSERT_TRUE(materialx::lower(graph, &lowered)) << test_case.nodedef;
    for (const char *channel : {"Red", "Green", "Blue", "Alpha"}) {
      MathNode *found = nullptr;
      const string expected_name = string("Math.") + channel;
      for (ShaderNode *shader_node : lowered.nodes) {
        if (shader_node->name == expected_name) {
          found = dynamic_cast<MathNode *>(shader_node);
          break;
        }
      }
      ASSERT_NE(found, nullptr) << test_case.nodedef << " " << expected_name;
      EXPECT_EQ(found->get_math_type(), test_case.math_type) << test_case.nodedef;
      EXPECT_FLOAT_EQ(found->get_value1(), 0.0f) << test_case.nodedef;
      EXPECT_FLOAT_EQ(found->get_value2(), test_case.second_default) << test_case.nodedef;
    }
  }
}

TEST(materialx_usdshade_reader, reads_exact_color4fa_specials_with_literal_and_linked_scalars)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4Specials"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };

  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader source = shader("Source");
  pxr::UsdShadeShader linked_amount = shader("LinkedAmount");
  pxr::UsdShadeShader invert = shader("Invert");
  pxr::UsdShadeShader safepower = shader("SafePower");
  pxr::UsdShadeShader clamp = shader("Clamp");
  pxr::UsdShadeShader convert = shader("RGB");

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  source.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_invert_color4FA")));
  source.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f));
  source.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  source.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  linked_amount.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  linked_amount.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  linked_amount.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  invert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_invert_color4FA")));
  ASSERT_TRUE(invert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(source.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(invert.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(linked_amount.ConnectableAPI(), pxr::TfToken("out")));
  invert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  safepower.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_safepower_color4FA")));
  ASSERT_TRUE(safepower.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(invert.ConnectableAPI(), pxr::TfToken("out")));
  safepower.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  safepower.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  clamp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_clamp_color4FA")));
  ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(safepower.ConnectableAPI(), pxr::TfToken("out")));
  clamp.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float).Set(-0.25f);
  clamp.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  clamp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(clamp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  const auto find_node = [&](const char *name) -> const materialx::Node * {
    const auto found = std::find_if(
        graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
          return node.name == name;
        });
    return found == graph.nodes.end() ? nullptr : &*found;
  };
  const materialx::Node *invert_node = find_node("Invert");
  const materialx::Node *safepower_node = find_node("SafePower");
  const materialx::Node *clamp_node = find_node("Clamp");
  ASSERT_NE(invert_node, nullptr);
  ASSERT_NE(safepower_node, nullptr);
  ASSERT_NE(clamp_node, nullptr);
  EXPECT_EQ(invert_node->nodedef, "ND_invert_color4FA");
  EXPECT_EQ(invert_node->links.at("amount").source_node, "LinkedAmount");
  EXPECT_EQ(invert_node->links.at("amount").type, materialx::Type::Float);
  EXPECT_EQ(safepower_node->nodedef, "ND_safepower_color4FA");
  EXPECT_FLOAT_EQ(safepower_node->inputs.at("in2"), 2.0f);
  EXPECT_EQ(clamp_node->nodedef, "ND_clamp_color4FA");
  EXPECT_FLOAT_EQ(clamp_node->inputs.at("low"), -0.25f);
  EXPECT_FLOAT_EQ(clamp_node->inputs.at("high"), 0.8f);
}

TEST(materialx_usdshade_reader, rejects_invalid_color4fa_specials_without_mutation)
{
  const struct Rejection {
    const char *nodedef;
    const char *first_input;
    const char *second_input;
    float second_value;
    const char *expected_error;
  } rejections[] = {{"ND_invert_color4FA",
                     "in",
                     "amount",
                     std::numeric_limits<float>::quiet_NaN(),
                     "finite"},
                    {"ND_safepower_color4FA",
                     "in1",
                     "in2",
                     std::numeric_limits<float>::infinity(),
                     "finite"},
                    {"ND_clamp_color4FA",
                     "in",
                     "high",
                     std::numeric_limits<float>::infinity(),
                     "finite"}};

  for (const Rejection &test : rejections) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/InvalidColor4Specials"));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader math = shader("Math");
    pxr::UsdShadeShader convert = shader("RGB");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test.nodedef)));
    math.CreateInput(pxr::TfToken(test.first_input), pxr::SdfValueTypeNames->Color4f)
        .Set(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f));
    if (test.nodedef == string("ND_clamp_color4FA")) {
      math.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float).Set(0.0f);
    }
    math.CreateInput(pxr::TfToken(test.second_input), pxr::SdfValueTypeNames->Float)
        .Set(test.second_value);
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
    convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(math.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)) << test.nodedef;
    EXPECT_NE(error.find(test.expected_error), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1) << test.nodedef;
    EXPECT_EQ(graph.nodes[0].name, "sentinel") << test.nodedef;
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_color4fa_scalar_arithmetic)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4ScalarArithmetic"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader uv = shader("UV");
  pxr::UsdShadeShader image = shader("Image");
  pxr::UsdShadeShader scalar = shader("Scalar");
  pxr::UsdShadeShader convert = shader("RGB");
  pxr::UsdShadeShader alpha = shader("Alpha");

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  scalar.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  scalar.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  struct Case {
    const char *nodedef;
    float scalar;
    bool linked_scalar;
  };
  const std::array<Case, 8> cases = {{{"ND_add_color4FA", 0.125f, false},
                                      {"ND_subtract_color4FA", 0.25f, true},
                                      {"ND_multiply_color4FA", 2.0f, false},
                                      {"ND_divide_color4FA", 2.0f, false},
                                      {"ND_min_color4FA", 0.9f, false},
                                      {"ND_max_color4FA", 0.1f, false},
                                      {"ND_modulo_color4FA", 0.7f, false},
                                      {"ND_power_color4FA", 1.5f, false}}};
  pxr::UsdShadeShader previous = image;
  for (const Case &test : cases) {
    pxr::UsdShadeShader math = shader(test.nodedef);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test.nodedef)));
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    ASSERT_TRUE(math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
    if (test.linked_scalar) {
      ASSERT_TRUE(math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                      .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(test.scalar);
    }
    previous = math;
  }
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  for (const Case &test : cases) {
    const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.nodedef == test.nodedef;
    });
    ASSERT_NE(found, graph.nodes.end()) << test.nodedef;
    EXPECT_EQ(found->outputs.at("out"), materialx::Type::Color4) << test.nodedef;
    EXPECT_EQ(found->links.at("in1").type, materialx::Type::Color4) << test.nodedef;
    if (test.linked_scalar) {
      EXPECT_EQ(found->links.at("in2").source_node, "Scalar") << test.nodedef;
      EXPECT_EQ(found->links.at("in2").type, materialx::Type::Float) << test.nodedef;
    }
    else {
      EXPECT_FLOAT_EQ(found->inputs.at("in2"), test.scalar) << test.nodedef;
    }
  }

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  CombineColorNode *rgb_power = nullptr;
  MathNode *alpha_power = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "ND_power_color4FA") {
      rgb_power = dynamic_cast<CombineColorNode *>(node);
    }
    else if (node->name == "ND_power_color4FA.Alpha") {
      alpha_power = dynamic_cast<MathNode *>(node);
    }
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(rgb_power, nullptr);
  ASSERT_NE(alpha_power, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(alpha_power->get_math_type(), NODE_MATH_POWER);
  EXPECT_EQ(principled->input("Base Color")->link, rgb_power->output("Color"));
  EXPECT_EQ(principled->input("Roughness")->link, alpha_power->output("Value"));
}

TEST(materialx_usdshade_reader, rejects_invalid_color4fa_scalar_arithmetic_without_mutation)
{
  struct Rejection {
    const char *nodedef;
    pxr::GfVec4f color;
    float scalar;
    const char *expected_error;
  };
  const Rejection rejections[] = {{"ND_add_color4FA",
                                   pxr::GfVec4f(1.0f, 2.0f, std::numeric_limits<float>::infinity(), 4.0f),
                                   0.5f,
                                   "finite"},
                                  {"ND_multiply_color4FA",
                                   pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                   std::numeric_limits<float>::quiet_NaN(),
                                   "finite"},
                                  {"ND_divide_color4FA",
                                   pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                   0.0f,
                                   "nonzero"},
                                  {"ND_modulo_color4FA",
                                   pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                   0.0f,
                                   "nonzero"}};
  for (const Rejection &test : rejections) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/InvalidColor4ScalarArithmetic"));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(
          stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader math = shader("Math");
    pxr::UsdShadeShader convert = shader("RGB");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(test.nodedef)));
    math.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f).Set(test.color);
    math.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(test.scalar);
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
    convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(math.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)) << test.nodedef;
    EXPECT_NE(error.find(test.expected_error), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1) << test.nodedef;
    EXPECT_EQ(graph.nodes[0].name, "sentinel") << test.nodedef;
  }
}


TEST(materialx_usdshade_reader,
     reads_exact_color4_math_batch_and_rejects_nonfinite_without_mutation)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4Math"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Color4Math").AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader uv = shader("UV");
  pxr::UsdShadeShader image = shader("Image");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));

  pxr::UsdShadeShader previous = image;
  for (const char *nodedef : {"ND_absval_color4",
                              "ND_ceil_color4",
                              "ND_floor_color4",
                              "ND_fract_color4",
                              "ND_round_color4",
                              "ND_sign_color4"})
  {
    pxr::UsdShadeShader math = shader(nodedef);
    math.CreateIdAttr(pxr::VtValue(pxr::TfToken(nodedef)));
    math.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    ASSERT_TRUE(math.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
    previous = math;
  }
  pxr::UsdShadeShader invert = shader("ND_invert_color4");
  invert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_invert_color4")));
  invert.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(1.0f, 0.5f, 0.25f, 0.75f));
  ASSERT_TRUE(invert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  invert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  pxr::UsdShadeShader safepower = shader("ND_safepower_color4");
  safepower.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_safepower_color4")));
  ASSERT_TRUE(safepower.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(invert.ConnectableAPI(), pxr::TfToken("out")));
  safepower.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(2.0f, 3.0f, 4.0f, 5.0f));
  safepower.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  pxr::UsdShadeShader convert = shader("RGB");
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(safepower.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  for (const char *nodedef : {"ND_absval_color4",
                              "ND_ceil_color4",
                              "ND_floor_color4",
                              "ND_fract_color4",
                              "ND_round_color4",
                              "ND_sign_color4",
                              "ND_invert_color4",
                              "ND_safepower_color4"})
  {
    const auto found = std::find_if(
        graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
          return node.nodedef == nodedef;
        });
    ASSERT_NE(found, graph.nodes.end()) << nodedef;
    EXPECT_EQ(found->outputs.at("out"), materialx::Type::Color4) << nodedef;
  }

  pxr::UsdShadeShader bad = shader("BadColor4");
  bad.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absval_color4")));
  bad.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(1.0f, 2.0f, std::numeric_limits<float>::infinity(), 4.0f));
  bad.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(convert.GetInput(pxr::TfToken("in"))
                  .ConnectToSource(bad.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph rejected;
  rejected.nodes.push_back({"sentinel", "unsupported"});
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &rejected, &error));
  EXPECT_NE(error.find("finite"), string::npos) << error;
  ASSERT_EQ(rejected.nodes.size(), 1);
  EXPECT_EQ(rejected.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_color4_math_with_exact_materialx_defaults)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4Defaults"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Color4Defaults").AppendChild(pxr::TfToken(name)));
  };

  pxr::UsdShadeShader surface = shader("OpenPBR");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader safepower = shader("DefaultSafePower");
  safepower.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_safepower_color4")));
  safepower.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);

  pxr::UsdShadeShader convert = shader("RGB");
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(safepower.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.nodedef == "ND_safepower_color4";
  });
  ASSERT_NE(found, graph.nodes.end());
  EXPECT_TRUE(found->float4_inputs.empty());
  EXPECT_TRUE(found->links.empty());
  EXPECT_EQ(found->outputs.at("out"), materialx::Type::Color4);

  ShaderGraph lowered;
  EXPECT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_color4_image_extract_and_convert_into_shared_graph)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4Image"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Color4Image").AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader image = shader("PackedImage");
  pxr::UsdShadeShader extract = shader("Alpha");
  pxr::UsdShadeShader convert = shader("BaseColor");
  pxr::UsdShadeShader uv = shader("UV");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  const auto find_node = [&](const char *nodedef) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.nodedef == nodedef;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  const materialx::Node &color4_image = find_node("ND_image_color4");
  EXPECT_EQ(color4_image.outputs.at("out"), materialx::Type::Color4);
  EXPECT_EQ(std::count_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
              return node.nodedef == "ND_image_color4";
            }),
            1);
  const materialx::Node &color4_extract = find_node("ND_extract_color4");
  EXPECT_EQ(color4_extract.int_inputs.at("index"), 3);
  EXPECT_EQ(color4_extract.links.at("in").type, materialx::Type::Color4);
  EXPECT_EQ(color4_extract.links.at("in").source_node, color4_image.name);
  EXPECT_EQ(color4_extract.outputs.at("out"), materialx::Type::Float);
  const materialx::Node &color4_convert = find_node("ND_convert_color4_color3");
  EXPECT_EQ(color4_convert.links.at("in").type, materialx::Type::Color4);
  EXPECT_EQ(color4_convert.links.at("in").source_node, color4_image.name);
  EXPECT_EQ(color4_convert.outputs.at("out"), materialx::Type::Color3);

  ShaderGraph lowered;
  EXPECT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_nested_color4_conversion_and_extract_with_one_image_node)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/NestedColor4Image"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader image = shader("PackedImage");
  pxr::UsdShadeShader extract = shader("Alpha");
  pxr::UsdShadeShader convert = shader("BaseColor");
  pxr::UsdShadeShader mix = shader("ColorMix");
  pxr::UsdShadeShader uv = shader("UV");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  mix.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_color3")));
  mix.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  mix.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  mix.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(mix.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(mix.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  EXPECT_EQ(std::count_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
              return node.nodedef == "ND_image_color4";
            }),
            1);
  const auto image_it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.nodedef == "ND_image_color4";
  });
  ASSERT_NE(image_it, graph.nodes.end());
  const auto convert_it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.nodedef == "ND_convert_color4_color3";
  });
  ASSERT_NE(convert_it, graph.nodes.end());
  const auto extract_it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.nodedef == "ND_extract_color4";
  });
  ASSERT_NE(extract_it, graph.nodes.end());
  EXPECT_EQ(convert_it->links.at("in").source_node, image_it->name);
  EXPECT_EQ(extract_it->links.at("in").source_node, image_it->name);
}

TEST(materialx_usdshade_reader, rejects_invalid_color4_reader_inputs_without_mutating_graph)
{
  const TemporaryImage image_asset;
  for (const int rejection : {0, 1, 2}) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/InvalidColor4Reader" + std::to_string(rejection)));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(
          stage,
          material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader source = shader("Source");
    pxr::UsdShadeShader extract = shader("Extract");
    pxr::UsdShadeShader uv = shader("UV");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    source.CreateIdAttr(pxr::VtValue(pxr::TfToken(
        rejection == 1 ? "ND_constant_color4" : "ND_image_color4")));
    source.CreateOutput(pxr::TfToken("out"),
                        rejection == 0 ? pxr::SdfValueTypeNames->Color3f :
                                         pxr::SdfValueTypeNames->Color4f);
    if (rejection == 1) {
      source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f));
    }
    else {
      source.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
          .Set(pxr::SdfAssetPath(image_asset.path()));
      uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
      uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
      uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
      ASSERT_TRUE(source.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                      .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
    }
    if (rejection == 2) {
      source.CreateInput(pxr::TfToken("filtertype"), pxr::SdfValueTypeNames->String).Set("closest");
    }
    extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
    extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
    extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    ASSERT_TRUE(extract.CreateInput(
                            pxr::TfToken("in"),
                            rejection == 0 ? pxr::SdfValueTypeNames->Color3f :
                                             pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(source.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                    pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    const char *expected_error = rejection == 0 ? "Color4 input must use Color4f" :
                                 rejection == 1 ? "requires a literal finite color4" :
                                                  "no supported Cycles control: filtertype";
    EXPECT_NE(error.find(expected_error), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  }
}

TEST(materialx_usdshade_reader, rejects_invalid_color4_extract_indices_without_mutating_graph)
{
  const TemporaryImage image_asset;
  for (const int invalid_index : {-1, 4}) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/InvalidColor4Extract"));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(
          stage, pxr::SdfPath("/Looks/InvalidColor4Extract").AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader image = shader("Image");
    pxr::UsdShadeShader extract = shader("Extract");
    pxr::UsdShadeShader uv = shader("UV");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
    image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
        .Set(pxr::SdfAssetPath(image_asset.path()));
    image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
    uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
    uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
    ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
    extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
    extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(invalid_index);
    extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    EXPECT_NE(error.find("ND_extract_color4 'index'"), string::npos);
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  }
}

TEST(materialx_usdshade_reader, rejects_linked_color4_extract_index_without_mutating_graph)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/LinkedColor4Extract"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/LinkedColor4Extract").AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader image = shader("Image");
  pxr::UsdShadeShader extract = shader("Extract");
  pxr::UsdShadeShader index = shader("Index");
  pxr::UsdShadeShader uv = shader("UV");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color4")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  index.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_integer")));
  index.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Int);
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int)
                  .ConnectToSource(index.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("ND_extract_color4 'index'"), string::npos);
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_geomprop_place2d_image_extract_chain)
{
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/CoordinateImage"));
  const auto shader = [&](const char *name, const char *id) {
    return pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/CoordinateImage").AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader");
  pxr::UsdShadeShader geomprop = shader("WorldNormal", "ND_geompropvalue_vector3");
  pxr::UsdShadeShader convert = shader("ToVector2", "ND_convert_vector3_vector2");
  pxr::UsdShadeShader place = shader("Place", "ND_place2d_vector2");
  pxr::UsdShadeShader image = shader("Image", "ND_image_color3");
  pxr::UsdShadeShader extract = shader("Roughness", "ND_extract_color3");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  geomprop.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector3")));
  geomprop.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("Nworld");
  geomprop.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector3_vector2")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(geomprop.ConnectableAPI(), pxr::TfToken("out")));
  place.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_place2d_vector2")));
  place.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  place.CreateInput(pxr::TfToken("pivot"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.5f));
  place.CreateInput(pxr::TfToken("scale"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(2.0f));
  place.CreateInput(pxr::TfToken("rotate"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  place.CreateInput(pxr::TfToken("offset"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f));
  place.CreateInput(pxr::TfToken("operationorder"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  ASSERT_TRUE(place.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(place.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color3")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 6);
  EXPECT_EQ(source.nodes[0].nodedef, "ND_geompropvalue_vector3");
  EXPECT_EQ(source.nodes[1].nodedef, "ND_convert_vector3_vector2");
  EXPECT_EQ(source.nodes[2].nodedef, "ND_place2d_vector2");
  EXPECT_EQ(source.nodes[3].nodedef, "ND_image_color3");
  EXPECT_EQ(source.nodes[4].nodedef, "ND_extract_color3");
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool has_open_pbr = false;
  for (ShaderNode *node : lowered.nodes) {
    has_open_pbr |= node->name == "OpenPBR";
  }
  EXPECT_TRUE(has_open_pbr);
}

TEST(materialx_usdshade_reader, rejects_invalid_extract_color3_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader extract = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/InvalidChannel"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/PackedColor"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color3")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_missing_image_asset_without_mutating_graph)
{
  const string missing_path =
      (std::filesystem::temp_directory_path() / "cycles_materialx_reader_missing.ppm").string();
  std::error_code file_error;
  std::filesystem::remove(missing_path, file_error);

  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/BaseColorImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage,
                                                       pxr::SdfPath("/Looks/TestMaterial/UV"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(missing_path));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_nested_nodegraph_interfaces_into_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  const pxr::UsdShadeNodeGraph outer = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer"));
  const pxr::UsdShadeNodeGraph inner = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/Inner"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/OpenPBR"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/Inner/BaseColor"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/Inner/RoughnessMultiply"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/Inner/RoughnessFirst"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Outer/Inner/RoughnessSecond"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  ASSERT_TRUE(
      inner.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
          .ConnectToSource(pxr::UsdShadeConnectionSourceInfo(
              color.ConnectableAPI(), pxr::TfToken("out"), pxr::UsdShadeAttributeType::Output)));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
          .ConnectToSource(pxr::UsdShadeConnectionSourceInfo(inner.ConnectableAPI(),
                                                             pxr::TfToken("base_color"),
                                                             pxr::UsdShadeAttributeType::Input)));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(inner.CreateOutput(pxr::TfToken("roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(outer.CreateOutput(pxr::TfToken("roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(inner.ConnectableAPI(), pxr::TfToken("roughness")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(outer.ConnectableAPI(), pxr::TfToken("roughness")));

  ASSERT_TRUE(inner.CreateInput(pxr::TfToken("surface"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      inner.CreateOutput(pxr::TfToken("surface"), pxr::SdfValueTypeNames->Token)
          .ConnectToSource(pxr::UsdShadeConnectionSourceInfo(inner.ConnectableAPI(),
                                                             pxr::TfToken("surface"),
                                                             pxr::UsdShadeAttributeType::Input)));
  ASSERT_TRUE(outer.CreateOutput(pxr::TfToken("surface"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(inner.ConnectableAPI(), pxr::TfToken("surface")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(outer.ConnectableAPI(), pxr::TfToken("surface")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 3);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_constant_color3");
  EXPECT_EQ(graph.nodes[1].nodedef, "ND_multiply_float");
  EXPECT_EQ(graph.nodes[2].nodedef, "ND_open_pbr_surface_surfaceshader");
  EXPECT_EQ(graph.nodes[2].links.at("base_color").source_node, "BaseColor");
  EXPECT_EQ(graph.nodes[2].links.at("specular_roughness").source_node, "RoughnessMultiply");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(principled, nullptr);
  EXPECT_NE(principled->input("Base Color")->link, nullptr);
  EXPECT_NE(principled->input("Roughness")->link, nullptr);
}

TEST(materialx_usdshade_reader, rejects_cyclic_nodegraph_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  const pxr::UsdShadeNodeGraph first = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/First"));
  const pxr::UsdShadeNodeGraph second = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Second"));
  ASSERT_TRUE(first.CreateOutput(pxr::TfToken("surface"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("surface")));
  ASSERT_TRUE(second.CreateOutput(pxr::TfToken("surface"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("surface")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("surface")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("cyclic"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_authority_pipeline, lowers_canonical_usdshade_through_shared_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader constant = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/BaseColor"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessMultiply"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessFirst"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessSecond"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  constant.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  constant.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.9f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  string usda;
  ASSERT_TRUE(stage->GetRootLayer()->ExportToString(&usda));
  const materialx::Authority authority = {
      "9c37e82e-63a1-470d-a704-e0daf9cfd814",
      materialx::usda_sha256_digest(usda),
      ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "/Looks/TestMaterial",
      usda,
  };

  ShaderGraph graph;
  string error;
  ASSERT_TRUE(materialx::lower_usdshade_authority(authority, &graph, &error)) << error;

  ColorNode *color = nullptr;
  MathNode *math = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : graph.nodes) {
    color = color ? color : dynamic_cast<ColorNode *>(node);
    math = math ? math : dynamic_cast<MathNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(color, nullptr);
  ASSERT_NE(math, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(principled->input("Base Color")->link, color->output("Color"));
  EXPECT_EQ(principled->input("Roughness")->link, math->output("Value"));
  EXPECT_EQ(graph.output()->input("Surface")->link, principled->output("BSDF"));
}

TEST(materialx_authority_pipeline, rejects_tampered_usda_digest_without_mutating_graph)
{
  const string original_usda =
      "#usda 1.0\n"
      "\n"
      "def Scope \"Looks\"\n"
      "{\n"
      "    def Material \"TestMaterial\"\n"
      "    {\n"
      "        token outputs:mtlx:surface.connect = "
      "</Looks/TestMaterial/OpenPBR.outputs:out>\n"
      "\n"
      "        def Shader \"OpenPBR\"\n"
      "        {\n"
      "            uniform token info:id = \"ND_open_pbr_surface_surfaceshader\"\n"
      "            color3f inputs:base_color = (0.02, 0.8, 0.08)\n"
      "            token outputs:out\n"
      "        }\n"
      "    }\n"
      "}\n";
  const materialx::Authority authority = {
      "9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "sha256:c36fe4ed7ff2385201bb534535f924f3951f1f086825bebce851e59b66acd022",
      ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "/Looks/TestMaterial",
      original_usda + "# tampered after digest\n",
  };

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
  string error;
  EXPECT_FALSE(materialx::lower_usdshade_authority(authority, &graph, &error));
  EXPECT_NE(error.find("contract"), string::npos) << error;
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_authority_pipeline, rejects_malformed_digest_without_mutating_graph)
{
  const string usda =
      "#usda 1.0\n"
      "\n"
      "def Scope \"Looks\" {}\n";
  const materialx::Authority authority = {
      "9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "sha256:not-a-canonical-digest",
      ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814",
      "/Looks/TestMaterial",
      usda,
  };

  ShaderGraph graph;
  EmissionNode *sentinel = graph.create_node<EmissionNode>();
  graph.connect(sentinel->output("Emission"), graph.output()->input("Surface"));
  const size_t original_node_count = graph.nodes.size();
  ShaderOutput *const original_surface_link = graph.output()->input("Surface")->link;
  string error;
  EXPECT_FALSE(materialx::lower_usdshade_authority(authority, &graph, &error));
  EXPECT_NE(error.find("contract"), string::npos) << error;
  EXPECT_EQ(graph.nodes.size(), original_node_count);
  EXPECT_EQ(graph.output()->input("Surface")->link, original_surface_link);
}

TEST(materialx_authority_pipeline, rejects_invalid_authority_without_mutating_graph)
{
  const materialx::Authority authority = {
      "not-a-uuid",
      "not-a-digest",
      ".materialx_usdshade_not-a-uuid",
      "Looks/TestMaterial",
      "#usda 1.0\n",
  };

  ShaderGraph graph;
  string error;
  EXPECT_FALSE(materialx::lower_usdshade_authority(authority, &graph, &error));
  EXPECT_FALSE(error.empty());

  for (ShaderNode *node : graph.nodes) {
    EXPECT_EQ(dynamic_cast<ColorNode *>(node), nullptr);
    EXPECT_EQ(dynamic_cast<PrincipledBsdfNode *>(node), nullptr);
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector_operations_to_normal_and_roughness)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/First"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Second"));
  pxr::UsdShadeShader add = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Add"));
  pxr::UsdShadeShader normalize = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Normalize"));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
  pxr::UsdShadeShader magnitude = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Magnitude"));
  pxr::UsdShadeShader extract = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Extract"));
  pxr::UsdShadeShader dot = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/TestMaterial/Dot"));
  pxr::UsdShadeShader roughness = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Roughness"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.0f, 1.0f));
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.0f, 0.5f, 0.0f));
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  add.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vector3")));
  add.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  normalize.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalize_vector3")));
  normalize.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(normalize.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  magnitude.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_magnitude_vector3")));
  magnitude.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector3")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  dot.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_dotproduct_vector3")));
  dot.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  roughness.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_float")));
  roughness.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(roughness.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(roughness.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(dot.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(roughness.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));

  VectorMathNode *add_node = nullptr;
  VectorMathNode *dot_node = nullptr;
  SeparateXYZNode *extract_node = nullptr;
  NormalMapNode *normal_node = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (auto *math = dynamic_cast<VectorMathNode *>(node)) {
      add_node = math->get_math_type() == NODE_VECTOR_MATH_ADD ? math : add_node;
      dot_node = math->get_math_type() == NODE_VECTOR_MATH_DOT_PRODUCT ? math : dot_node;
    }
    normal_node = normal_node ? normal_node : dynamic_cast<NormalMapNode *>(node);
    extract_node = extract_node ? extract_node : dynamic_cast<SeparateXYZNode *>(node);
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(add_node, nullptr);
  ASSERT_NE(dot_node, nullptr);
  ASSERT_NE(extract_node, nullptr);
  ASSERT_NE(normal_node, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_NE(principled->input("Roughness")->link, nullptr);
}

TEST(materialx_usdshade_reader, reads_linked_combine3_vector3)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/First"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Second"));
  pxr::UsdShadeShader third = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Third"));
  pxr::UsdShadeShader combine = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Combine"));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  for (auto [shader, value] :
       {std::pair{first, 0.25f}, std::pair{second, 0.5f}, std::pair{third, 0.75f}})
  {
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
    shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(value);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  }
  combine.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_combine3_vector3")));
  combine.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in3"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(third.ConnectableAPI(), pxr::TfToken("out")));
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 6);
  const materialx::Node &combine_node = graph.nodes[3];
  EXPECT_EQ(combine_node.name, "Combine");
  EXPECT_EQ(combine_node.nodedef, "ND_combine3_vector3");
  for (const auto &[input, source] :
       {std::pair{"in1", "First"}, std::pair{"in2", "Second"}, std::pair{"in3", "Third"}})
  {
    const materialx::Link &link = combine_node.links.at(input);
    EXPECT_EQ(link.source_node, source);
    EXPECT_EQ(link.source_output, "out");
    EXPECT_EQ(link.type, materialx::Type::Float);
  }
  EXPECT_EQ(graph.nodes[4].links.at("in").source_node, "Combine");
  EXPECT_EQ(graph.nodes[5].links.at("geometry_normal").source_node, "NormalMap");

}

TEST(materialx_usdshade_reader, rejects_wrong_type_combine3_vector3_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader combine = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Combine"));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalMap"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  combine.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_combine3_vector3")));
  combine.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  combine.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  combine.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.5f));
  combine.CreateInput(pxr::TfToken("in3"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("requires float inputs"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_invert_and_clamp_utility_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Utilities"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Utilities").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader scalar_invert =
      shader("ScalarInvert", "ND_invert_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader clamp = shader("Clamp", "ND_clamp_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader amount =
      shader("Amount", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader input =
      shader("Input", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader low = shader("Low", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader high = shader("High", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader color_invert =
      shader("ColorInvert", "ND_invert_color3", pxr::SdfValueTypeNames->Color3f);
  pxr::UsdShadeShader color_amount =
      shader("ColorAmount", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  pxr::UsdShadeShader color_input =
      shader("ColorInput", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);

  amount.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  low.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.1f);
  high.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  color_amount.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(1.0f));
  color_input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  ASSERT_TRUE(scalar_invert.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(amount.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(scalar_invert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar_invert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(low.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(high.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(color_invert.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_amount.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(color_invert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(clamp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_invert.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_scalar_invert = false, found_clamp = false, found_color_invert = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_scalar_invert |= math->get_math_type() == NODE_MATH_SUBTRACT;
    }
    if (const auto *candidate = dynamic_cast<ClampNode *>(node)) {
      found_clamp |= candidate->get_clamp_type() == NODE_CLAMP_MINMAX;
    }
    if (const auto *mix = dynamic_cast<MixNode *>(node)) {
      found_color_invert |= mix->get_mix_type() == NODE_MIX_SUB;
    }
  }
  EXPECT_TRUE(found_scalar_invert);
  EXPECT_TRUE(found_clamp);
  EXPECT_TRUE(found_color_invert);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_unary_and_float_color_utilities)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ExactUtilities"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ExactUtilities").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader absolute = shader("Absolute", "ND_absval_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader floor = shader("Floor", "ND_floor_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader ceiling = shader("Ceiling", "ND_ceil_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader color = shader(
      "Color", "ND_convert_float_color3", pxr::SdfValueTypeNames->Color3f);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(-1.25f);
  ASSERT_TRUE(absolute.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(floor.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(absolute.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(ceiling.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(floor.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(color.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(ceiling.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(ceiling.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_absolute = false, found_floor = false, found_ceil = false, found_color = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_absolute |= math->get_math_type() == NODE_MATH_ABSOLUTE;
      found_floor |= math->get_math_type() == NODE_MATH_FLOOR;
      found_ceil |= math->get_math_type() == NODE_MATH_CEIL;
    }
    found_color |= dynamic_cast<CombineColorNode *>(node) != nullptr;
  }
  EXPECT_TRUE(found_absolute);
  EXPECT_TRUE(found_floor);
  EXPECT_TRUE(found_ceil);
  EXPECT_TRUE(found_color);
}

TEST(materialx_usdshade_reader, reads_and_lowers_nested_exact_trigonometric_and_exponential_float_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ExactTrig"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ExactTrig").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ExactTrig/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_float");
  pxr::UsdShadeShader sine = shader("Sine", "ND_sin_float");
  pxr::UsdShadeShader cosine = shader("Cosine", "ND_cos_float");
  pxr::UsdShadeShader tangent = shader("Tangent", "ND_tan_float");
  pxr::UsdShadeShader exponent = shader("Exponent", "ND_exp_float");
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  ASSERT_TRUE(sine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(cosine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(sine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(tangent.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(cosine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(exponent.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(tangent.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(exponent.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_sine = false, found_cosine = false, found_tangent = false, found_exponent = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_sine |= math->get_math_type() == NODE_MATH_SINE;
      found_cosine |= math->get_math_type() == NODE_MATH_COSINE;
      found_tangent |= math->get_math_type() == NODE_MATH_TANGENT;
      found_exponent |= math->get_math_type() == NODE_MATH_EXPONENT;
    }
  }
  EXPECT_TRUE(found_sine);
  EXPECT_TRUE(found_cosine);
  EXPECT_TRUE(found_tangent);
  EXPECT_TRUE(found_exponent);
}

TEST(materialx_usdshade_reader, reads_nested_fraction_sign_minimum_and_maximum_float_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ExactScalarMath"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ExactScalarMath").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ExactScalarMath/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_float");
  pxr::UsdShadeShader limit = shader("Limit", "ND_constant_float");
  pxr::UsdShadeShader fraction = shader("Fraction", "ND_fract_float");
  pxr::UsdShadeShader sign = shader("Sign", "ND_sign_float");
  pxr::UsdShadeShader minimum = shader("Minimum", "ND_min_float");
  pxr::UsdShadeShader maximum = shader("Maximum", "ND_max_float");
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(-1.25f);
  limit.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  ASSERT_TRUE(fraction.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sign.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(fraction.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(fraction.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(limit.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(minimum.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(sign.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(maximum.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_fraction = false, found_sign = false, found_minimum = false, found_maximum = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_fraction |= math->get_math_type() == NODE_MATH_FRACTION;
      found_sign |= math->get_math_type() == NODE_MATH_SIGN;
      found_minimum |= math->get_math_type() == NODE_MATH_MINIMUM;
      found_maximum |= math->get_math_type() == NODE_MATH_MAXIMUM;
    }
  }
  EXPECT_TRUE(found_fraction);
  EXPECT_TRUE(found_sign);
  EXPECT_TRUE(found_minimum);
  EXPECT_TRUE(found_maximum);
}

TEST(materialx_usdshade_reader, reads_and_lowers_power_and_modulo_float_nodes)
{
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/PowerModulo"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/PowerModulo").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/PowerModulo/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader base = shader("Base", "ND_constant_float");
  base.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.5f);
  pxr::UsdShadeShader exponent = shader("Exponent", "ND_constant_float");
  exponent.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  pxr::UsdShadeShader divisor = shader("Divisor", "ND_constant_float");
  divisor.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  pxr::UsdShadeShader power = shader("Power", "ND_power_float");
  pxr::UsdShadeShader modulo = shader("Modulo", "ND_modulo_float");

  power.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
      .ConnectToSource(base.ConnectableAPI(), pxr::TfToken("out"));
  power.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
      .ConnectToSource(exponent.ConnectableAPI(), pxr::TfToken("out"));
  modulo.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
      .ConnectToSource(power.ConnectableAPI(), pxr::TfToken("out"));
  modulo.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
      .ConnectToSource(divisor.ConnectableAPI(), pxr::TfToken("out"));
  surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
      .ConnectToSource(modulo.ConnectableAPI(), pxr::TfToken("out"));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_power = false, found_modulo = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_power |= math->get_math_type() == NODE_MATH_POWER;
      found_modulo |= math->get_math_type() == NODE_MATH_MODULO;
    }
  }
  EXPECT_TRUE(found_power);
  EXPECT_TRUE(found_modulo);
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_float_conditionals)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/FloatConditionals"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/FloatConditionals").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FloatConditionals/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader one = shader("One", "ND_constant_float");
  one.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  pxr::UsdShadeShader greater = shader("Greater", "ND_ifgreater_float");
  pxr::UsdShadeShader greater_equal = shader("GreaterEqual", "ND_ifgreatereq_float");
  pxr::UsdShadeShader equal = shader("Equal", "ND_ifequal_float");

  ASSERT_TRUE(greater.CreateInput(pxr::TfToken("value1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(one.ConnectableAPI(), pxr::TfToken("out")));
  greater.CreateInput(pxr::TfToken("value2"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  greater.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  greater.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  ASSERT_TRUE(greater_equal.CreateInput(pxr::TfToken("value1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(greater.ConnectableAPI(), pxr::TfToken("out")));
  greater_equal.CreateInput(pxr::TfToken("value2"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  greater_equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  greater_equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  ASSERT_TRUE(equal.CreateInput(pxr::TfToken("value1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(greater_equal.ConnectableAPI(), pxr::TfToken("out")));
  equal.CreateInput(pxr::TfToken("value2"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(equal.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  int greater_count = 0, compare_count = 0, maximum_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      greater_count += math->get_math_type() == NODE_MATH_GREATER_THAN;
      compare_count += math->get_math_type() == NODE_MATH_COMPARE;
      maximum_count += math->get_math_type() == NODE_MATH_MAXIMUM;
    }
  }
  EXPECT_EQ(greater_count, 2);
  EXPECT_EQ(compare_count, 2);
  EXPECT_EQ(maximum_count, 1);
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_color3_conditionals)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ColorConditionals"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ColorConditionals").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ColorConditionals/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  const auto color = [&](const char *name, const pxr::GfVec3f &value) {
    pxr::UsdShadeShader result = shader(name, "ND_constant_color3");
    result.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(value);
    return result;
  };
  pxr::UsdShadeShader red = color("Red", pxr::GfVec3f(0.8f, 0.1f, 0.1f));
  pxr::UsdShadeShader green = color("Green", pxr::GfVec3f(0.1f, 0.8f, 0.1f));
  pxr::UsdShadeShader blue = color("Blue", pxr::GfVec3f(0.1f, 0.1f, 0.8f));
  pxr::UsdShadeShader white = color("White", pxr::GfVec3f(0.8f));
  pxr::UsdShadeShader greater = shader("Greater", "ND_ifgreater_color3");
  pxr::UsdShadeShader greater_equal = shader("GreaterEqual", "ND_ifgreatereq_color3");
  pxr::UsdShadeShader equal = shader("Equal", "ND_ifequal_color3");
  for (pxr::UsdShadeShader conditional : {greater, greater_equal, equal}) {
    conditional.CreateInput(pxr::TfToken("value1"), pxr::SdfValueTypeNames->Float).Set(1.0f);
    conditional.CreateInput(pxr::TfToken("value2"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  }
  ASSERT_TRUE(greater.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(red.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(greater.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(green.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(greater_equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(greater.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(greater_equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(blue.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(greater_equal.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(white.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(equal.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  int greater_count = 0, compare_count = 0, maximum_count = 0, mix_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      greater_count += math->get_math_type() == NODE_MATH_GREATER_THAN;
      compare_count += math->get_math_type() == NODE_MATH_COMPARE;
      maximum_count += math->get_math_type() == NODE_MATH_MAXIMUM;
    }
    mix_count += dynamic_cast<MixNode *>(node) != nullptr;
  }
  EXPECT_EQ(greater_count, 2);
  EXPECT_EQ(compare_count, 2);
  EXPECT_EQ(maximum_count, 1);
  EXPECT_EQ(mix_count, 3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_vector3_conditionals)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/VectorConditionals"));
  const auto shader = [&](const char *name, const char *id) { pxr::UsdShadeShader node = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/VectorConditionals").AppendChild(pxr::TfToken(name))); node.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); node.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3); return node; };
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector3"); first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(1.0f, 0.0f, 0.0f));
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector3"); second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.0f, 1.0f, 0.0f));
  pxr::UsdShadeShader greater = shader("Greater", "ND_ifgreater_vector3"), greater_equal = shader("GreaterEqual", "ND_ifgreatereq_vector3"), equal = shader("Equal", "ND_ifequal_vector3");
  for (pxr::UsdShadeShader node : {greater, greater_equal, equal}) { node.CreateInput(pxr::TfToken("value1"), pxr::SdfValueTypeNames->Float).Set(1.0f); node.CreateInput(pxr::TfToken("value2"), pxr::SdfValueTypeNames->Float).Set(1.0f); }
  ASSERT_TRUE(greater.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(greater.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3).ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(greater_equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).ConnectToSource(greater.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(greater_equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3).ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(equal.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).ConnectToSource(greater_equal.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(equal.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3).ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader normalmap = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/VectorConditionals/Normalmap")); normalmap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float"))); normalmap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3); ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(equal.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/VectorConditionals/OpenPBR")); surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3).ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered));
  int mixes = 0, greater_count = 0, compare_count = 0, maximum_count = 0; for (ShaderNode *node : lowered.nodes) { mixes += dynamic_cast<MixVectorNode *>(node) != nullptr; if (const auto *math=dynamic_cast<MathNode *>(node)) { greater_count += math->get_math_type() == NODE_MATH_GREATER_THAN; compare_count += math->get_math_type() == NODE_MATH_COMPARE; maximum_count += math->get_math_type() == NODE_MATH_MAXIMUM; } }
  EXPECT_EQ(mixes, 3); EXPECT_EQ(greater_count, 2); EXPECT_EQ(compare_count, 2); EXPECT_EQ(maximum_count, 1);
}

TEST(materialx_usdshade_reader, reads_and_lowers_noise3d_contract_forms)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage); const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/Noise3D"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){ auto node=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Noise3D").AppendChild(pxr::TfToken(name))); node.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); node.CreateOutput(pxr::TfToken("out"),type); return node; };
  auto position=shader("Position","ND_constant_vector3",pxr::SdfValueTypeNames->Float3); position.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.1f,0.2f,0.3f));
  auto scalar=shader("Scalar","ND_noise3d_float",pxr::SdfValueTypeNames->Float); auto color=shader("Color","ND_noise3d_color3",pxr::SdfValueTypeNames->Color3f); auto fa=shader("FA","ND_noise3d_color3FA",pxr::SdfValueTypeNames->Color3f);
  for (pxr::UsdShadeShader node : {scalar,color,fa}) { node.CreateInput(pxr::TfToken("pivot"),pxr::SdfValueTypeNames->Float).Set(0.25f); ASSERT_TRUE(node.CreateInput(pxr::TfToken("position"),pxr::SdfValueTypeNames->Float3).ConnectToSource(position.ConnectableAPI(),pxr::TfToken("out"))); }
  scalar.CreateInput(pxr::TfToken("amplitude"),pxr::SdfValueTypeNames->Float).Set(0.5f); color.CreateInput(pxr::TfToken("amplitude"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.5f,0.75f,1.0f)); fa.CreateInput(pxr::TfToken("amplitude"),pxr::SdfValueTypeNames->Float).Set(0.5f);
  ASSERT_TRUE(fa.CreateInput(pxr::TfToken("position"),pxr::SdfValueTypeNames->Float3).ConnectToSource(position.ConnectableAPI(),pxr::TfToken("out")));
  auto add=shader("Add","ND_add_color3",pxr::SdfValueTypeNames->Color3f); ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(color.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(fa.ConnectableAPI(),pxr::TfToken("out")));
  auto surface=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Noise3D/OpenPBR")); surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Token); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(fa.ConnectableAPI(),pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(add.ConnectableAPI(),pxr::TfToken("out")));
  const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int noise_count=0; for(ShaderNode *node:lowered.nodes) if(const auto *noise=dynamic_cast<NoiseTextureNode *>(node)){ EXPECT_EQ(noise->get_dimensions(),3); noise_count++; } EXPECT_EQ(noise_count,3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_cellnoise_family)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/CellNoise"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader node = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/CellNoise").AppendChild(pxr::TfToken(name)));
    node.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    node.CreateOutput(pxr::TfToken("out"), type);
    return node;
  };

  pxr::UsdShadeShader texcoord = shader(
      "Texcoord", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  texcoord.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.125f, 0.875f));
  pxr::UsdShadeShader position = shader(
      "Position", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  position.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  pxr::UsdShadeShader cellnoise2d = shader(
      "CellNoise2D", "ND_cellnoise2d_float", pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(cellnoise2d.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(texcoord.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader cellnoise3d = shader(
      "CellNoise3D", "ND_cellnoise3d_float", pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(cellnoise3d.CreateInput(pxr::TfToken("position"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(position.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader add = shader("Add", "ND_add_float", pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(cellnoise2d.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(cellnoise3d.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;

  const struct {
    const char *name;
    const char *id;
    const char *input_name;
    materialx::Type input_type;
  } expected[] = {{"CellNoise2D", "ND_cellnoise2d_float", "texcoord", materialx::Type::Vector2},
                 {"CellNoise3D", "ND_cellnoise3d_float", "position", materialx::Type::Vector3}};
  for (const auto &test : expected) {
    const auto node = std::find_if(source.nodes.begin(), source.nodes.end(), [&](const materialx::Node &n) {
      return n.name == test.name;
    });
    ASSERT_NE(node, source.nodes.end()) << test.id;
    EXPECT_EQ(node->nodedef, test.id);
    EXPECT_EQ(node->outputs.at("out"), materialx::Type::Float);
    EXPECT_EQ(node->links.at(test.input_name).type, test.input_type);
  }

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  int white_noise_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *white_noise = dynamic_cast<WhiteNoiseTextureNode *>(node)) {
      ++white_noise_count;
      if (node->name == "CellNoise2D") {
        EXPECT_EQ(white_noise->get_dimensions(), 2);
      }
      else if (node->name == "CellNoise3D") {
        EXPECT_EQ(white_noise->get_dimensions(), 3);
      }
    }
  }
  EXPECT_EQ(white_noise_count, 2);
}

TEST(materialx_usdshade_reader, rejects_cellnoise_signature_mismatch_before_emitting_node)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/InvalidCellNoise"));
  pxr::UsdShadeShader position = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidCellNoise/Position"));
  position.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  position.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  position.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader cellnoise = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidCellNoise/CellNoise"));
  cellnoise.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_cellnoise2d_float")));
  ASSERT_TRUE(cellnoise.CreateInput(pxr::TfToken("position"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(position.ConnectableAPI(), pxr::TfToken("out")));
  cellnoise.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidCellNoise/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(cellnoise.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("ND_cellnoise2d_float"), string::npos);
  EXPECT_TRUE(source.nodes.empty());
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_round_float_node)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/RoundFloat"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/RoundFloat/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/RoundFloat/Input"));
  input.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.25f);
  input.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader round = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/RoundFloat/Round"));
  round.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_round_float")));
  round.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(round.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(round.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_round = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_round |= math->get_math_type() == NODE_MATH_ROUND;
    }
  }
  EXPECT_TRUE(found_round);
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_sqrt_float_node)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/SqrtFloat"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SqrtFloat/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SqrtFloat/Input"));
  input.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.25f);
  input.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sqrt = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SqrtFloat/Sqrt"));
  sqrt.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_sqrt_float")));
  sqrt.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(sqrt.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(sqrt.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_sqrt = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      found_sqrt |= math->get_math_type() == NODE_MATH_SQRT;
    }
  }
  EXPECT_TRUE(found_sqrt);
}

TEST(materialx_usdshade_reader, reads_and_lowers_float_to_vector3_broadcast)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/FloatToVector3"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/FloatToVector3").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader scalar = shader("Scalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader convert = shader(
      "Broadcast", "ND_convert_float_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader magnitude = shader(
      "Magnitude", "ND_magnitude_vector3", pxr::SdfValueTypeNames->Float);
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto broadcast = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.name == "Broadcast";
  });
  ASSERT_NE(broadcast, graph.nodes.end());
  EXPECT_EQ(broadcast->nodedef, "ND_convert_float_vector3");
  EXPECT_EQ(broadcast->outputs.at("out"), materialx::Type::Vector3);
  EXPECT_EQ(broadcast->links.at("in").type, materialx::Type::Float);
  EXPECT_EQ(broadcast->links.at("in").source_node, "Scalar");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  CombineXYZNode *native_broadcast = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_broadcast = node->name == "Broadcast" ? dynamic_cast<CombineXYZNode *>(node) :
                                                   native_broadcast;
  }
  ASSERT_NE(native_broadcast, nullptr);
  EXPECT_EQ(native_broadcast->input("X")->link, native_broadcast->input("Y")->link);
  EXPECT_EQ(native_broadcast->input("Y")->link, native_broadcast->input("Z")->link);
}

TEST(materialx_usdshade_reader, reads_and_lowers_color3_vector3_component_construction_chain)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/ColorVector"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ColorVector").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), type); return result;
  };
  pxr::UsdShadeShader surface = shader("Surface", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader color = shader("Color", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  pxr::UsdShadeShader ctv = shader("ColorToVector", "ND_convert_color3_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader vtc = shader("VectorToColor", "ND_convert_vector3_color3", pxr::SdfValueTypeNames->Color3f);
  pxr::UsdShadeShader separate = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ColorVector/Separate"));
  separate.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_separate3_color3")));
  separate.CreateOutput(pxr::TfToken("outx"), pxr::SdfValueTypeNames->Float);
  separate.CreateOutput(pxr::TfToken("outy"), pxr::SdfValueTypeNames->Float);
  separate.CreateOutput(pxr::TfToken("outz"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader combine = shader("Combine", "ND_combine3_color3", pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(ctv.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(vtc.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(ctv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(separate.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(vtc.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[input, output] : {std::pair{"in1", "outx"}, std::pair{"in2", "outy"}, std::pair{"in3", "outz"}})
    ASSERT_TRUE(combine.CreateInput(pxr::TfToken(input), pxr::SdfValueTypeNames->Float).ConnectToSource(separate.ConnectableAPI(), pxr::TfToken(output)));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_color_separate = false, found_color_combine = false;
  for (ShaderNode *node : lowered.nodes) { found_color_separate |= dynamic_cast<SeparateColorNode *>(node) != nullptr; found_color_combine |= dynamic_cast<CombineColorNode *>(node) != nullptr; }
  EXPECT_TRUE(found_color_separate); EXPECT_TRUE(found_color_combine);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_unary_color3_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/UnaryColor"));
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/UnaryColor").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f); return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/UnaryColor/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_color3");
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(-1.25f, 2.75f, -0.5f));
  const char *ids[] = {"ND_absval_color3", "ND_floor_color3", "ND_ceil_color3", "ND_fract_color3", "ND_round_color3", "ND_sign_color3"};
  pxr::UsdShadeShader previous = input;
  for (int i = 0; i < 6; i++) {
    pxr::UsdShadeShader node = shader(ids[i], ids[i]);
    ASSERT_TRUE(node.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
    previous = node;
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered));
  for (const NodeMathType type : {NODE_MATH_ABSOLUTE, NODE_MATH_FLOOR, NODE_MATH_CEIL, NODE_MATH_FRACTION, NODE_MATH_ROUND, NODE_MATH_SIGN}) {
    int count = 0; for (ShaderNode *node : lowered.nodes) if (const auto *math = dynamic_cast<MathNode *>(node)) count += math->get_math_type() == type;
    EXPECT_EQ(count, 3) << type;
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_top_to_bottom_color_ramp)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/UV"));
  pxr::UsdShadeShader shift = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/UVShift"));
  pxr::UsdShadeShader shifted_uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/ShiftedUV"));
  pxr::UsdShadeShader ramp = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Ramp"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("UVMap");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  shift.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  shift.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.5f));
  shift.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  shifted_uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vector2")));
  ASSERT_TRUE(shifted_uv.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(shifted_uv.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(shift.ConnectableAPI(), pxr::TfToken("out")));
  shifted_uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramptb_color3")));
  ramp.CreateInput(pxr::TfToken("valuet"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  ramp.CreateInput(pxr::TfToken("valueb"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.7f, 0.8f, 0.9f));
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(shifted_uv.ConnectableAPI(), pxr::TfToken("out")));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 5);
  EXPECT_EQ(source.nodes[2].nodedef, "ND_add_vector2");
  EXPECT_EQ(source.nodes[3].nodedef, "ND_ramptb_color3");
  EXPECT_EQ(source.nodes[3].links.at("texcoord").source_node, "ShiftedUV");
  EXPECT_EQ(source.nodes[3].color3_inputs.at("valuet"), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(source.nodes[3].color3_inputs.at("valueb"), make_float3(0.7f, 0.8f, 0.9f));

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MixNode *mix = nullptr;
  SeparateXYZNode *coordinate = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "Ramp") mix = dynamic_cast<MixNode *>(node);
    if (node->name == "Ramp.coordinate") coordinate = dynamic_cast<SeparateXYZNode *>(node);
  }
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(coordinate, nullptr);
  EXPECT_EQ(mix->get_mix_type(), NODE_MIX_BLEND);
  EXPECT_NE(mix->input("Fac")->link, nullptr);
  EXPECT_EQ(mix->get_color1(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(mix->get_color2(), make_float3(0.7f, 0.8f, 0.9f));
  ASSERT_NE(mix->input("Fac")->link, nullptr);
  EXPECT_EQ(mix->input("Fac")->link->parent->input("Value")->link, coordinate->output("Y"));
}

TEST(materialx_usdshade_reader, reads_and_lowers_top_to_bottom_scalar_ramp)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage); const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/TopBottomRamp"));
  auto surface=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/TopBottomRamp/OpenPBR")); auto uv=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/TopBottomRamp/UV")); auto ramp=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/TopBottomRamp/Ramp"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Token); uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2"))); uv.CreateInput(pxr::TfToken("geomprop"),pxr::SdfValueTypeNames->String).Set("st"); uv.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Float2);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramptb_float"))); ramp.CreateInput(pxr::TfToken("valuet"),pxr::SdfValueTypeNames->Float).Set(0.2f); ramp.CreateInput(pxr::TfToken("valueb"),pxr::SdfValueTypeNames->Float).Set(0.8f); ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"),pxr::SdfValueTypeNames->Float2).ConnectToSource(uv.ConnectableAPI(),pxr::TfToken("out"))); ramp.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Float); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(ramp.ConnectableAPI(),pxr::TfToken("out")));
  const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out"))); materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ASSERT_EQ(source.nodes[1].nodedef,"ND_ramptb_float"); EXPECT_EQ(source.nodes[1].links.at("texcoord").source_node,"UV"); EXPECT_FLOAT_EQ(source.nodes[1].inputs.at("valuet"),0.2f); EXPECT_FLOAT_EQ(source.nodes[1].inputs.at("valueb"),0.8f);
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); SeparateXYZNode *coordinate=nullptr; ClampNode *clamp=nullptr; for(ShaderNode *node:lowered.nodes){ if(node->name=="Ramp.coordinate") coordinate=dynamic_cast<SeparateXYZNode *>(node); if(node->name=="Ramp.factor") clamp=dynamic_cast<ClampNode *>(node); } ASSERT_NE(coordinate,nullptr); ASSERT_NE(clamp,nullptr); EXPECT_EQ(coordinate->output("Y")->links.size(),1); EXPECT_EQ(clamp->get_clamp_type(),NODE_CLAMP_MINMAX);
}

TEST(materialx_usdshade_reader, reads_scalar_left_to_right_ramp_with_nested_vector2_link)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/ScalarRamp"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ScalarRamp/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ScalarRamp/UV"));
  pxr::UsdShadeShader offset = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ScalarRamp/Offset"));
  pxr::UsdShadeShader shifted = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ScalarRamp/Shifted"));
  pxr::UsdShadeShader ramp = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/ScalarRamp/Ramp"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("UVMap");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  offset.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  offset.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f, 0.5f));
  offset.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  shifted.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vector2")));
  ASSERT_TRUE(shifted.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(shifted.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(offset.ConnectableAPI(), pxr::TfToken("out")));
  shifted.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_float")));
  ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  ramp.CreateInput(pxr::TfToken("valuer"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2).ConnectToSource(shifted.ConnectableAPI(), pxr::TfToken("out")));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 5);
  EXPECT_EQ(source.nodes[3].nodedef, "ND_ramplr_float");
  EXPECT_EQ(source.nodes[3].links.at("texcoord").source_node, "Shifted");
  EXPECT_FLOAT_EQ(source.nodes[3].inputs.at("valuel"), 0.2f);
  EXPECT_FLOAT_EQ(source.nodes[3].inputs.at("valuer"), 0.8f);
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MathNode *sum = nullptr;
  MathNode *product = nullptr;
  SeparateXYZNode *coordinate = nullptr;
  ClampNode *clamp = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    sum = node->name == "Ramp" ? dynamic_cast<MathNode *>(node) : sum;
    product = node->name == "Ramp.product" ? dynamic_cast<MathNode *>(node) : product;
    coordinate = node->name == "Ramp.coordinate" ? dynamic_cast<SeparateXYZNode *>(node) : coordinate;
    clamp = node->name == "Ramp.factor" ? dynamic_cast<ClampNode *>(node) : clamp;
  }
  ASSERT_NE(sum, nullptr);
  ASSERT_NE(product, nullptr);
  ASSERT_NE(coordinate, nullptr);
  ASSERT_NE(clamp, nullptr);
  EXPECT_EQ(sum->get_math_type(), NODE_MATH_ADD);
  EXPECT_EQ(sum->input("Value2")->link, product->output("Value"));
  EXPECT_EQ(product->input("Value2")->link, clamp->output("Result"));
  EXPECT_EQ(clamp->input("Value")->link, coordinate->output("X"));
}

TEST(materialx_usdshade_reader, reads_smoothstep_float_with_nested_typed_link)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Smoothstep"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Smoothstep/OpenPBR"));
  pxr::UsdShadeShader value = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Smoothstep/Value"));
  pxr::UsdShadeShader smoothstep = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Smoothstep/Smoothstep"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  value.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  value.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  value.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  value.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  smoothstep.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_smoothstep_float")));
  smoothstep.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  smoothstep.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  ASSERT_TRUE(smoothstep.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(value.ConnectableAPI(), pxr::TfToken("out")));
  smoothstep.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(smoothstep.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 3);
  EXPECT_EQ(source.nodes[1].nodedef, "ND_smoothstep_float");
  EXPECT_EQ(source.nodes[1].links.at("in").source_node, "Value");
  EXPECT_FLOAT_EQ(source.nodes[1].inputs.at("low"), 0.25f);
  EXPECT_FLOAT_EQ(source.nodes[1].inputs.at("high"), 0.75f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MapRangeNode *range = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    range = node->name == "Smoothstep" ? dynamic_cast<MapRangeNode *>(node) : range;
  }
  ASSERT_NE(range, nullptr);
  EXPECT_EQ(range->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
  EXPECT_EQ(range->input("Value")->link->parent->name, "Value");
}

TEST(materialx_usdshade_reader, reads_and_lowers_literal_float_remap_and_range)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Range"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Range/OpenPBR"));
  pxr::UsdShadeShader value = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Range/Value"));
  pxr::UsdShadeShader remap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Range/Remap"));
  pxr::UsdShadeShader range = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Range/Range"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  value.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  value.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  value.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  value.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  remap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_remap_float")));
  ASSERT_TRUE(remap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(value.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", -1.0f},
                                    {"inhigh", 1.0f},
                                    {"outlow", 0.25f},
                                    {"outhigh", 0.75f}})
  {
    remap.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float).Set(value);
  }
  remap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  range.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_range_float")));
  ASSERT_TRUE(range.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(remap.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", 0.0f},
                                    {"inhigh", 1.0f},
                                    {"gamma", 1.0f},
                                    {"outlow", 0.2f},
                                    {"outhigh", 0.8f}})
  {
    range.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float).Set(value);
  }
  range.CreateInput(pxr::TfToken("doclamp"), pxr::SdfValueTypeNames->Bool).Set(true);
  range.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                  pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(range.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 4);
  EXPECT_EQ(source.nodes[1].nodedef, "ND_remap_float");
  EXPECT_EQ(source.nodes[2].nodedef, "ND_range_float");
  EXPECT_EQ(source.nodes[2].int_inputs.at("doclamp"), 1);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MapRangeNode *remap_node = nullptr;
  MapRangeNode *range_node = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    remap_node = node->name == "Remap" ? dynamic_cast<MapRangeNode *>(node) : remap_node;
    range_node = node->name == "Range" ? dynamic_cast<MapRangeNode *>(node) : range_node;
  }
  ASSERT_NE(remap_node, nullptr);
  ASSERT_NE(range_node, nullptr);
  EXPECT_EQ(remap_node->get_range_type(), NODE_MAP_RANGE_LINEAR);
  EXPECT_FALSE(remap_node->get_clamp());
  ASSERT_NE(remap_node->input("Value")->link, nullptr);
  EXPECT_EQ(remap_node->input("Value")->link->parent->name, "Value");
  EXPECT_TRUE(range_node->get_clamp());
  ASSERT_NE(range_node->input("Value")->link, nullptr);
  EXPECT_EQ(range_node->input("Value")->link->parent->name, "Remap");
}

TEST(materialx_usdshade_reader, rejects_degenerate_or_dynamic_float_range_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/InvalidRange"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidRange/OpenPBR"));
  pxr::UsdShadeShader range = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidRange/Range"));
  pxr::UsdShadeShader value = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/InvalidRange/Value"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  range.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_range_float")));
  range.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  range.CreateInput(pxr::TfToken("inlow"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  range.CreateInput(pxr::TfToken("inhigh"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  range.CreateInput(pxr::TfToken("gamma"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  range.CreateInput(pxr::TfToken("outlow"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  range.CreateInput(pxr::TfToken("outhigh"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  range.CreateInput(pxr::TfToken("doclamp"), pxr::SdfValueTypeNames->Bool).Set(false);
  range.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                  pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(range.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("inlow != inhigh"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");

  range.GetInput(pxr::TfToken("inhigh")).Set(1.0f);
  value.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  value.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  value.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(range.GetInput(pxr::TfToken("gamma")).ConnectToSource(
      value.ConnectableAPI(), pxr::TfToken("out")));
  error.clear();
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("gamma 1.0"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_color3_mix_remap_range_adjustment_chain)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ColorAdjustment"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ColorAdjustment/OpenPBR"));
  pxr::UsdShadeShader mix = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ColorAdjustment/Mix"));
  pxr::UsdShadeShader remap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ColorAdjustment/Remap"));
  pxr::UsdShadeShader range = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ColorAdjustment/Range"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  mix.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_color3")));
  mix.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  mix.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.7f, 0.6f, 0.5f));
  mix.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  mix.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);

  remap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_remap_color3")));
  ASSERT_TRUE(remap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(mix.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", pxr::GfVec3f(0.0f)},
                                    {"inhigh", pxr::GfVec3f(1.0f)},
                                    {"outlow", pxr::GfVec3f(0.2f)},
                                    {"outhigh", pxr::GfVec3f(0.8f)}})
  {
    remap.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Color3f).Set(value);
  }
  remap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);

  range.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_range_color3")));
  ASSERT_TRUE(range.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(remap.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", pxr::GfVec3f(0.0f)},
                                    {"inhigh", pxr::GfVec3f(1.0f)},
                                    {"gamma", pxr::GfVec3f(1.0f)},
                                    {"outlow", pxr::GfVec3f(0.1f)},
                                    {"outhigh", pxr::GfVec3f(0.9f)}})
  {
    range.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Color3f).Set(value);
  }
  range.CreateInput(pxr::TfToken("doclamp"), pxr::SdfValueTypeNames->Bool).Set(true);
  range.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(range.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  const materialx::Node &remap_node = find_node("Remap");
  EXPECT_EQ(remap_node.nodedef, "ND_remap_color3");
  EXPECT_EQ(remap_node.outputs.at("out"), materialx::Type::Color3);
  EXPECT_EQ(remap_node.links.at("in").source_node, "Mix");
  const materialx::Node &range_node = find_node("Range");
  EXPECT_EQ(range_node.nodedef, "ND_range_color3");
  EXPECT_EQ(range_node.outputs.at("out"), materialx::Type::Color3);
  EXPECT_EQ(range_node.links.at("in").source_node, "Remap");
  EXPECT_EQ(range_node.int_inputs.at("doclamp"), 1);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector2_magnitude_and_dotproduct)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2Metrics"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2Metrics").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader magnitude = shader("Magnitude", "ND_magnitude_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader dotproduct = shader("DotProduct", "ND_dotproduct_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum = shader("Sum", "ND_add_float", pxr::SdfValueTypeNames->Float);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(3.0f, 4.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, 2.0f));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dotproduct.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dotproduct.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(dotproduct.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_magnitude = false, found_dotproduct = false;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<VectorMathNode *>(node)) {
      found_magnitude |= math->get_math_type() == NODE_VECTOR_MATH_LENGTH;
      found_dotproduct |= math->get_math_type() == NODE_VECTOR_MATH_DOT_PRODUCT;
    }
  }
  EXPECT_TRUE(found_magnitude);
  EXPECT_TRUE(found_dotproduct);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector2_distance)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2Distance"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2Distance").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader distance = shader("Distance", "ND_distance_vector2", pxr::SdfValueTypeNames->Float);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(3.0f, 4.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.0f, 0.0f));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(distance.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Distance" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector3_distance)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector3Distance"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector3Distance").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader distance = shader("Distance", "ND_distance_vector3", pxr::SdfValueTypeNames->Float);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(3.0f, 4.0f, 0.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(0.0f, 0.0f, 0.0f));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(distance.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Distance" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_DISTANCE);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector3_crossproduct)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector3Crossproduct"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector3Crossproduct").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader crossproduct = shader(
      "Cross", "ND_crossproduct_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(1.0f, 0.0f, 0.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(0.0f, 1.0f, 0.0f));
  ASSERT_TRUE(crossproduct.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(crossproduct.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(crossproduct.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Cross" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_CROSS_PRODUCT);
}

TEST(materialx_usdshade_reader, reads_chained_vector3_normalize_magnitude_and_dotproduct)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ChainedVector3Math"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ChainedVector3Math").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalize = shader("Normalize", "ND_normalize_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader magnitude = shader("Magnitude", "ND_magnitude_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader dot = shader("Dot", "ND_dotproduct_vector3", pxr::SdfValueTypeNames->Float);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(3.0f, 4.0f, 0.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(0.0f, 1.0f, 0.0f));
  ASSERT_TRUE(normalize.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_metalness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(dot.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(source.nodes.begin(), source.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, source.nodes.end());
    return *it;
  };
  EXPECT_NE(find_node("Normalize").links.at("in").source_node.find("First"), string::npos);
  EXPECT_NE(find_node("Magnitude").links.at("in").source_node.find("Normalize"), string::npos);
  EXPECT_NE(find_node("Dot").links.at("in1").source_node.find("Normalize"), string::npos);
  EXPECT_NE(find_node("Dot").links.at("in2").source_node.find("Second"), string::npos);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *native_normalize = nullptr;
  VectorMathNode *native_magnitude = nullptr;
  VectorMathNode *native_dot = nullptr;
  PrincipledBsdfNode *principled = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_normalize = node->name == "Normalize" ? dynamic_cast<VectorMathNode *>(node) : native_normalize;
    native_magnitude = node->name == "Magnitude" ? dynamic_cast<VectorMathNode *>(node) : native_magnitude;
    native_dot = node->name == "Dot" ? dynamic_cast<VectorMathNode *>(node) : native_dot;
    principled = principled ? principled : dynamic_cast<PrincipledBsdfNode *>(node);
  }
  ASSERT_NE(native_normalize, nullptr);
  ASSERT_NE(native_magnitude, nullptr);
  ASSERT_NE(native_dot, nullptr);
  ASSERT_NE(principled, nullptr);
  EXPECT_EQ(native_normalize->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
  EXPECT_EQ(native_magnitude->get_math_type(), NODE_VECTOR_MATH_LENGTH);
  EXPECT_EQ(native_dot->get_math_type(), NODE_VECTOR_MATH_DOT_PRODUCT);
  ASSERT_NE(native_magnitude->input("Vector1")->link, nullptr);
  ASSERT_NE(native_dot->input("Vector1")->link, nullptr);
  EXPECT_NE(native_magnitude->input("Vector1")->link->parent->name.find("Normalize"), string::npos);
  EXPECT_NE(native_dot->input("Vector1")->link->parent->name.find("Normalize"), string::npos);
  EXPECT_EQ(principled->input("Roughness")->link, native_magnitude->output("Value"));
  EXPECT_EQ(principled->input("Metallic")->link, native_dot->output("Value"));
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_unary_vector3_utilities)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/UnaryVector3Utilities"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/UnaryVector3Utilities").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader absval = shader("Abs", "ND_absval_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader floor = shader("Floor", "ND_floor_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader ceil = shader("Ceil", "ND_ceil_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader fract = shader("Fract", "ND_fract_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(-1.25f, 2.75f, -3.5f));
  ASSERT_TRUE(absval.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(floor.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(absval.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(ceil.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(floor.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(fract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(ceil.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(fract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_absval = false;
  bool found_floor = false;
  bool found_ceil = false;
  bool found_fract = false;
  for (ShaderNode *node : lowered.nodes) {
    const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
    if (!math) continue;
    found_absval |= math->get_math_type() == NODE_VECTOR_MATH_ABSOLUTE;
    found_floor |= math->get_math_type() == NODE_VECTOR_MATH_FLOOR;
    found_ceil |= math->get_math_type() == NODE_VECTOR_MATH_CEIL;
    found_fract |= math->get_math_type() == NODE_VECTOR_MATH_FRACTION;
  }
  EXPECT_TRUE(found_absval);
  EXPECT_TRUE(found_floor);
  EXPECT_TRUE(found_ceil);
  EXPECT_TRUE(found_fract);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_trigonometric_vector3_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TrigonometricVector3"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/TrigonometricVector3").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader sine = shader("Sine", "ND_sin_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader cosine = shader("Cosine", "ND_cos_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader tangent = shader("Tangent", "ND_tan_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  ASSERT_TRUE(sine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(cosine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(sine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(tangent.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(cosine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(tangent.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_sine = false;
  bool found_cosine = false;
  bool found_tangent = false;
  for (ShaderNode *node : lowered.nodes) {
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

TEST(materialx_usdshade_reader, reads_and_lowers_exact_minimum_and_maximum_vector3_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/MinimumMaximumVector3"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/MinimumMaximumVector3").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader second = shader("Second", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader minimum = shader("Minimum", "ND_min_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader maximum = shader("Maximum", "ND_max_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(-1.0f, 2.0f, 5.0f));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(3.0f, 1.0f, 4.0f));
  ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(minimum.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(maximum.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_minimum = false;
  bool found_maximum = false;
  for (ShaderNode *node : lowered.nodes) {
    const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
    if (!math) continue;
    found_minimum |= math->get_math_type() == NODE_VECTOR_MATH_MINIMUM;
    found_maximum |= math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM;
  }
  EXPECT_TRUE(found_minimum);
  EXPECT_TRUE(found_maximum);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_sign_vector3_node)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/SignVector3"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/SignVector3").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader sign = shader("Sign", "ND_sign_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(-1.0f, 0.0f, 1.0f));
  ASSERT_TRUE(sign.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(sign.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Sign" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SIGN);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector3_float_multiply)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector3FloatMultiply"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector3FloatMultiply").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader multiply = shader("Multiply", "ND_multiply_vector3FA", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(1.0f, 2.0f, 3.0f));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.5f);
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Multiply" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_EQ(math->get_scale(), 2.5f);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector3_float_add_and_subtract_into_normalmap)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector3FloatAddSubtract"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector3FloatAddSubtract").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader scalar = shader("Scalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader add = shader("Add", "ND_add_vector3FA", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader subtract = shader("Subtract", "ND_subtract_vector3FA", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader normalmap = shader("NormalMap", "ND_normalmap_float", pxr::SdfValueTypeNames->Float3);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(
      pxr::GfVec3f(1.0f, 2.0f, 3.0f));
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.5f);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(subtract.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  subtract.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  ASSERT_TRUE(normalmap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(subtract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normalmap.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  const materialx::Node *parsed_normalmap = nullptr;
  for (const materialx::Node &node : source.nodes) {
    parsed_normalmap = node.name == "NormalMap" ? &node : parsed_normalmap;
  }
  ASSERT_NE(parsed_normalmap, nullptr);
  EXPECT_EQ(parsed_normalmap->links.at("in").source_node, "Subtract");
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *add_math = nullptr;
  VectorMathNode *subtract_math = nullptr;
  CombineXYZNode *broadcast = nullptr;
  NormalMapNode *native_normalmap = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    add_math = node->name == "Add" ? dynamic_cast<VectorMathNode *>(node) : add_math;
    subtract_math = node->name == "Subtract" ? dynamic_cast<VectorMathNode *>(node) : subtract_math;
    broadcast = node->name == "Add.broadcast" ? dynamic_cast<CombineXYZNode *>(node) : broadcast;
    native_normalmap = node->name == "NormalMap" ? dynamic_cast<NormalMapNode *>(node) : native_normalmap;
  }
  ASSERT_NE(add_math, nullptr);
  ASSERT_NE(subtract_math, nullptr);
  ASSERT_NE(broadcast, nullptr);
  ASSERT_NE(native_normalmap, nullptr);
  EXPECT_EQ(add_math->get_math_type(), NODE_VECTOR_MATH_ADD);
  EXPECT_EQ(subtract_math->get_math_type(), NODE_VECTOR_MATH_SUBTRACT);
  EXPECT_EQ(subtract_math->get_vector2(), make_float3(0.5f, 0.5f, 0.5f));
  ASSERT_NE(native_normalmap->input("Color")->link, nullptr);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector2_float_multiply)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector2FloatMultiply"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Vector2FloatMultiply").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), type); return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader multiply = shader("Multiply", "ND_multiply_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader extract = shader("Extract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, 2.0f));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.5f);
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) math = node->name == "Multiply" ? dynamic_cast<VectorMathNode *>(node) : math;
  ASSERT_NE(math, nullptr); EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_SCALE); EXPECT_EQ(math->get_scale(), 2.5f);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector2_float_add_and_subtract)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2FloatAddSubtract"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2FloatAddSubtract").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader scalar = shader("Scalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader add = shader("Add", "ND_add_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader subtract = shader("Subtract", "ND_subtract_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader extract = shader("Extract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, 2.0f));
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(2.5f);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(subtract.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  subtract.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(subtract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *add_math = nullptr;
  VectorMathNode *subtract_math = nullptr;
  CombineXYZNode *broadcast = nullptr;
  CombineXYZNode *subtract_repack = nullptr;
  for (ShaderNode *node : lowered.nodes) {
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

TEST(materialx_usdshade_reader, reads_and_lowers_exact_minimum_and_maximum_vector2_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/MinMaxVector2"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) { pxr::UsdShadeShader r = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/MinMaxVector2").AppendChild(pxr::TfToken(name))); r.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); r.CreateOutput(pxr::TfToken("out"), type); return r; };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token), a = shader("A", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2), b = shader("B", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2), minimum = shader("Min", "ND_min_vector2", pxr::SdfValueTypeNames->Float2), maximum = shader("Max", "ND_max_vector2", pxr::SdfValueTypeNames->Float2), extract = shader("Extract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  a.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(-1, 2)); b.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(3, 1));
  ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(a.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(minimum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(b.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(minimum.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(maximum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(b.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(maximum.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out"))); const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered)); bool found_min = false, found_max = false; for (ShaderNode *node : lowered.nodes) { const VectorMathNode *math = dynamic_cast<VectorMathNode *>(node); if (math) { found_min |= math->get_math_type() == NODE_VECTOR_MATH_MINIMUM; found_max |= math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM; } } EXPECT_TRUE(found_min); EXPECT_TRUE(found_max);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_unary_vector2_utilities)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/UnaryVector2"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) { pxr::UsdShadeShader r = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/UnaryVector2").AppendChild(pxr::TfToken(name))); r.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); r.CreateOutput(pxr::TfToken("out"), type); return r; };
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token), input=shader("Input","ND_constant_vector2",pxr::SdfValueTypeNames->Float2), absval=shader("Abs","ND_absval_vector2",pxr::SdfValueTypeNames->Float2), floor=shader("Floor","ND_floor_vector2",pxr::SdfValueTypeNames->Float2), ceil=shader("Ceil","ND_ceil_vector2",pxr::SdfValueTypeNames->Float2), fract=shader("Fract","ND_fract_vector2",pxr::SdfValueTypeNames->Float2), extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(-1.25f,2.75f));
  ASSERT_TRUE(absval.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(floor.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(absval.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(ceil.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(floor.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(fract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(ceil.ConnectableAPI(),pxr::TfToken("out")));
  extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(fract.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(),pxr::TfToken("out"))); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); bool a=false,f=false,c=false,r=false; for(ShaderNode *node:lowered.nodes){const VectorMathNode *m=dynamic_cast<VectorMathNode *>(node);if(m){a|=m->get_math_type()==NODE_VECTOR_MATH_ABSOLUTE;f|=m->get_math_type()==NODE_VECTOR_MATH_FLOOR;c|=m->get_math_type()==NODE_VECTOR_MATH_CEIL;r|=m->get_math_type()==NODE_VECTOR_MATH_FRACTION;}} EXPECT_TRUE(a);EXPECT_TRUE(f);EXPECT_TRUE(c);EXPECT_TRUE(r);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_trigonometric_and_sign_vector2_nodes)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage); const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/TrigSignVector2"));
  const auto shader=[&](const char*n,const char*i,const pxr::SdfValueTypeName&t){pxr::UsdShadeShader r=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/TrigSignVector2").AppendChild(pxr::TfToken(n)));r.CreateIdAttr(pxr::VtValue(pxr::TfToken(i)));r.CreateOutput(pxr::TfToken("out"),t);return r;};
  auto surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token);auto input=shader("Input","ND_constant_vector2",pxr::SdfValueTypeNames->Float2);auto sine=shader("Sine","ND_sin_vector2",pxr::SdfValueTypeNames->Float2);auto cosine=shader("Cosine","ND_cos_vector2",pxr::SdfValueTypeNames->Float2);auto tangent=shader("Tangent","ND_tan_vector2",pxr::SdfValueTypeNames->Float2);auto sign=shader("Sign","ND_sign_vector2",pxr::SdfValueTypeNames->Float2);auto extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f,-0.5f)); ASSERT_TRUE(sine.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(),pxr::TfToken("out")));ASSERT_TRUE(cosine.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(sine.ConnectableAPI(),pxr::TfToken("out")));ASSERT_TRUE(tangent.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(cosine.ConnectableAPI(),pxr::TfToken("out")));ASSERT_TRUE(sign.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(tangent.ConnectableAPI(),pxr::TfToken("out")));extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0);ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(sign.ConnectableAPI(),pxr::TfToken("out")));ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(),pxr::TfToken("out")));const pxr::TfToken context("mtlx",pxr::TfToken::Immortal);ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source;string error;ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error;ShaderGraph lowered;ASSERT_TRUE(materialx::lower(source,&lowered));bool s=false,c=false,t=false,g=false;for(ShaderNode*n:lowered.nodes){const VectorMathNode*m=dynamic_cast<VectorMathNode*>(n);if(m){s|=m->get_math_type()==NODE_VECTOR_MATH_SINE;c|=m->get_math_type()==NODE_VECTOR_MATH_COSINE;t|=m->get_math_type()==NODE_VECTOR_MATH_TANGENT;g|=m->get_math_type()==NODE_VECTOR_MATH_SIGN;}}EXPECT_TRUE(s);EXPECT_TRUE(c);EXPECT_TRUE(t);EXPECT_TRUE(g);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_domain_math_vector2_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2DomainMath"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2DomainMath").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader acos = shader("Acos", "ND_acos_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader asin = shader("Asin", "ND_asin_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader exponent = shader("Exp", "ND_exp_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader logarithm = shader("Ln", "ND_ln_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader square_root = shader("Sqrt", "ND_sqrt_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader round = shader("Round", "ND_round_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader acos_extract = shader("AcosExtract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sqrt_extract = shader("SqrtExtract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum = shader("Sum", "ND_add_float", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f, 0.5f));
  ASSERT_TRUE(acos.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(asin.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(exponent.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(asin.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(logarithm.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(exponent.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(square_root.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(logarithm.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(round.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(square_root.ConnectableAPI(), pxr::TfToken("out")));
  acos_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  sqrt_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  ASSERT_TRUE(acos_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(acos.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sqrt_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(round.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(acos_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(sqrt_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_acos = false, found_asin = false, found_exp = false, found_ln = false,
       found_ln_base_e = false, found_sqrt = false;
  for (ShaderNode *node : lowered.nodes) {
    const MathNode *math = dynamic_cast<MathNode *>(node);
    if (!math) continue;
    found_acos |= math->get_math_type() == NODE_MATH_ARCCOSINE;
    found_asin |= math->get_math_type() == NODE_MATH_ARCSINE;
    found_exp |= math->get_math_type() == NODE_MATH_EXPONENT;
    found_ln |= math->get_math_type() == NODE_MATH_LOGARITHM;
    found_ln_base_e |= math->get_math_type() == NODE_MATH_LOGARITHM &&
                       std::abs(math->get_value2() - float(M_E)) < 1e-6f;
    found_sqrt |= math->get_math_type() == NODE_MATH_SQRT;
  }
  EXPECT_TRUE(found_acos);
  EXPECT_TRUE(found_asin);
  EXPECT_TRUE(found_exp);
  EXPECT_TRUE(found_ln);
  EXPECT_TRUE(found_ln_base_e);
  EXPECT_TRUE(found_sqrt);
}

TEST(materialx_usdshade_reader, reads_and_lowers_modulo_and_power_vector2_component_forms)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector2ComponentMath"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Vector2ComponentMath").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), type); return result;
  };
  const auto connect = [](pxr::UsdShadeShader &node, const char *name, pxr::UsdShadeShader &source, const pxr::SdfValueTypeName &type) {
    return node.CreateInput(pxr::TfToken(name), type).ConnectToSource(source.ConnectableAPI(), pxr::TfToken("out"));
  };
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token), input=shader("Input","ND_constant_vector2",pxr::SdfValueTypeNames->Float2), modulo=shader("Modulo","ND_modulo_vector2",pxr::SdfValueTypeNames->Float2), modulo_fa=shader("ModuloFA","ND_modulo_vector2FA",pxr::SdfValueTypeNames->Float2), power=shader("Power","ND_power_vector2",pxr::SdfValueTypeNames->Float2), power_fa=shader("PowerFA","ND_power_vector2FA",pxr::SdfValueTypeNames->Float2), extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(5,7));
  ASSERT_TRUE(connect(modulo,"in1",input,pxr::SdfValueTypeNames->Float2)); modulo.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(2,3));
  ASSERT_TRUE(connect(modulo_fa,"in1",modulo,pxr::SdfValueTypeNames->Float2)); modulo_fa.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f);
  ASSERT_TRUE(connect(power,"in1",modulo_fa,pxr::SdfValueTypeNames->Float2)); power.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(2,3));
  ASSERT_TRUE(connect(power_fa,"in1",power,pxr::SdfValueTypeNames->Float2)); power_fa.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f);
  extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(connect(extract,"in",power_fa,pxr::SdfValueTypeNames->Float2)); ASSERT_TRUE(connect(surface,"specular_roughness",extract,pxr::SdfValueTypeNames->Float)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int modulo_count=0,power_count=0; for(ShaderNode *node:lowered.nodes) if(const MathNode *math=dynamic_cast<MathNode *>(node)){modulo_count+=math->get_math_type()==NODE_MATH_MODULO;power_count+=math->get_math_type()==NODE_MATH_POWER;} EXPECT_EQ(modulo_count,4); EXPECT_EQ(power_count,4);
}

TEST(materialx_usdshade_reader, reads_and_lowers_modulo_and_power_vector3_component_forms)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector3ComponentMath"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector3ComponentMath").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  const auto connect=[](pxr::UsdShadeShader &node,const char *name,pxr::UsdShadeShader &source,const pxr::SdfValueTypeName &type){return node.CreateInput(pxr::TfToken(name),type).ConnectToSource(source.ConnectableAPI(),pxr::TfToken("out"));};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector3",pxr::SdfValueTypeNames->Float3),modulo=shader("Modulo","ND_modulo_vector3",pxr::SdfValueTypeNames->Float3),modulo_fa=shader("ModuloFA","ND_modulo_vector3FA",pxr::SdfValueTypeNames->Float3),power=shader("Power","ND_power_vector3",pxr::SdfValueTypeNames->Float3),power_fa=shader("PowerFA","ND_power_vector3FA",pxr::SdfValueTypeNames->Float3),extract=shader("Extract","ND_extract_vector3",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(5,7,11)); ASSERT_TRUE(connect(modulo,"in1",input,pxr::SdfValueTypeNames->Float3)); modulo.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(2,3,4)); ASSERT_TRUE(connect(modulo_fa,"in1",modulo,pxr::SdfValueTypeNames->Float3)); modulo_fa.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); ASSERT_TRUE(connect(power,"in1",modulo_fa,pxr::SdfValueTypeNames->Float3)); power.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(2,3,4)); ASSERT_TRUE(connect(power_fa,"in1",power,pxr::SdfValueTypeNames->Float3)); power_fa.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(connect(extract,"in",power_fa,pxr::SdfValueTypeNames->Float3)); ASSERT_TRUE(connect(surface,"specular_roughness",extract,pxr::SdfValueTypeNames->Float)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int modulo_count=0,power_count=0; for(ShaderNode *node:lowered.nodes)if(const MathNode *math=dynamic_cast<MathNode *>(node)){modulo_count+=math->get_math_type()==NODE_MATH_MODULO;power_count+=math->get_math_type()==NODE_MATH_POWER;} EXPECT_EQ(modulo_count,6);EXPECT_EQ(power_count,6);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector3_clamp_and_scalar_bound_broadcast)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector3Clamp"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector3Clamp").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector3",pxr::SdfValueTypeNames->Float3),clamp=shader("Clamp","ND_clamp_vector3",pxr::SdfValueTypeNames->Float3),clamp_fa=shader("ClampFA","ND_clamp_vector3FA",pxr::SdfValueTypeNames->Float3),extract=shader("Extract","ND_extract_vector3",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(-1,0.5f,4)); ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(input.ConnectableAPI(),pxr::TfToken("out"))); clamp.CreateInput(pxr::TfToken("low"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0,0.25f,1)); clamp.CreateInput(pxr::TfToken("high"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(1,0.75f,3)); ASSERT_TRUE(clamp_fa.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(clamp.ConnectableAPI(),pxr::TfToken("out"))); clamp_fa.CreateInput(pxr::TfToken("low"),pxr::SdfValueTypeNames->Float).Set(0.0f); clamp_fa.CreateInput(pxr::TfToken("high"),pxr::SdfValueTypeNames->Float).Set(2.0f); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(clamp_fa.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(),pxr::TfToken("out"))); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source;string error;ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error;ShaderGraph lowered;ASSERT_TRUE(materialx::lower(source,&lowered));int minimum=0,maximum=0;for(ShaderNode *node:lowered.nodes)if(const VectorMathNode *math=dynamic_cast<VectorMathNode *>(node)){minimum+=math->get_math_type()==NODE_VECTOR_MATH_MINIMUM;maximum+=math->get_math_type()==NODE_VECTOR_MATH_MAXIMUM;}EXPECT_EQ(minimum,2);EXPECT_EQ(maximum,2);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector2_scalar_broadcast_min_max_divide_and_clamp)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector2FABatch"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector2FABatch").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  const auto connect=[](pxr::UsdShadeShader &node,const char *name,pxr::UsdShadeShader &source,const pxr::SdfValueTypeName &type){return node.CreateInput(pxr::TfToken(name),type).ConnectToSource(source.ConnectableAPI(),pxr::TfToken("out"));};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector2",pxr::SdfValueTypeNames->Float2),clamp=shader("Clamp","ND_clamp_vector2FA",pxr::SdfValueTypeNames->Float2),minimum=shader("Minimum","ND_min_vector2FA",pxr::SdfValueTypeNames->Float2),maximum=shader("Maximum","ND_max_vector2FA",pxr::SdfValueTypeNames->Float2),divide=shader("Divide","ND_divide_vector2FA",pxr::SdfValueTypeNames->Float2),extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(-1,4)); ASSERT_TRUE(connect(clamp,"in",input,pxr::SdfValueTypeNames->Float2)); clamp.CreateInput(pxr::TfToken("low"),pxr::SdfValueTypeNames->Float).Set(0.0f); clamp.CreateInput(pxr::TfToken("high"),pxr::SdfValueTypeNames->Float).Set(3.0f); ASSERT_TRUE(connect(minimum,"in1",clamp,pxr::SdfValueTypeNames->Float2)); minimum.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); ASSERT_TRUE(connect(maximum,"in1",minimum,pxr::SdfValueTypeNames->Float2)); maximum.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(1.0f); ASSERT_TRUE(connect(divide,"in1",maximum,pxr::SdfValueTypeNames->Float2)); divide.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(connect(extract,"in",divide,pxr::SdfValueTypeNames->Float2)); ASSERT_TRUE(connect(surface,"specular_roughness",extract,pxr::SdfValueTypeNames->Float)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int min_count=0,max_count=0,divide_count=0; for(ShaderNode *node:lowered.nodes)if(const MathNode *math=dynamic_cast<MathNode *>(node)){min_count+=math->get_math_type()==NODE_MATH_MINIMUM;max_count+=math->get_math_type()==NODE_MATH_MAXIMUM;divide_count+=math->get_math_type()==NODE_MATH_DIVIDE;} EXPECT_EQ(min_count,2); EXPECT_EQ(max_count,2); EXPECT_EQ(divide_count,2);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector3_scalar_broadcast_min_max_and_divide)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Vector3FABatch"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector3FABatch").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  const auto connect=[](pxr::UsdShadeShader &node,const char *name,pxr::UsdShadeShader &source,const pxr::SdfValueTypeName &type){return node.CreateInput(pxr::TfToken(name),type).ConnectToSource(source.ConnectableAPI(),pxr::TfToken("out"));};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector3",pxr::SdfValueTypeNames->Float3),minimum=shader("Minimum","ND_min_vector3FA",pxr::SdfValueTypeNames->Float3),maximum=shader("Maximum","ND_max_vector3FA",pxr::SdfValueTypeNames->Float3),divide=shader("Divide","ND_divide_vector3FA",pxr::SdfValueTypeNames->Float3),extract=shader("Extract","ND_extract_vector3",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(4,6,8)); ASSERT_TRUE(connect(minimum,"in1",input,pxr::SdfValueTypeNames->Float3)); minimum.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(5.0f); ASSERT_TRUE(connect(maximum,"in1",minimum,pxr::SdfValueTypeNames->Float3)); maximum.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); ASSERT_TRUE(connect(divide,"in1",maximum,pxr::SdfValueTypeNames->Float3)); divide.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Float).Set(2.0f); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(connect(extract,"in",divide,pxr::SdfValueTypeNames->Float3)); ASSERT_TRUE(connect(surface,"specular_roughness",extract,pxr::SdfValueTypeNames->Float)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int min_count=0,max_count=0,divide_count=0; for(ShaderNode *node:lowered.nodes)if(const MathNode *math=dynamic_cast<MathNode *>(node)){min_count+=math->get_math_type()==NODE_MATH_MINIMUM;max_count+=math->get_math_type()==NODE_MATH_MAXIMUM;divide_count+=math->get_math_type()==NODE_MATH_DIVIDE;} EXPECT_EQ(min_count,3); EXPECT_EQ(max_count,3); EXPECT_EQ(divide_count,3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector2_color3_conversion_adapters)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/ConversionAdapters"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/ConversionAdapters").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  const auto connect=[](pxr::UsdShadeShader &node,const char *name,pxr::UsdShadeShader &source,const pxr::SdfValueTypeName &type){return node.CreateInput(pxr::TfToken(name),type).ConnectToSource(source.ConnectableAPI(),pxr::TfToken("out"));};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),scalar=shader("Scalar","ND_constant_float",pxr::SdfValueTypeNames->Float),float_to_v2=shader("FloatToV2","ND_convert_float_vector2",pxr::SdfValueTypeNames->Float2),v2_to_v3=shader("V2ToV3","ND_convert_vector2_vector3",pxr::SdfValueTypeNames->Float3),v3_to_color=shader("V3ToColor","ND_convert_vector3_color3",pxr::SdfValueTypeNames->Color3f),color_to_v2=shader("ColorToV2","ND_convert_color3_vector2",pxr::SdfValueTypeNames->Float2),extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float);
  scalar.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float).Set(0.25f); ASSERT_TRUE(connect(float_to_v2,"in",scalar,pxr::SdfValueTypeNames->Float)); ASSERT_TRUE(connect(v2_to_v3,"in",float_to_v2,pxr::SdfValueTypeNames->Float2)); ASSERT_TRUE(connect(v3_to_color,"in",v2_to_v3,pxr::SdfValueTypeNames->Float3)); ASSERT_TRUE(connect(color_to_v2,"in",v3_to_color,pxr::SdfValueTypeNames->Color3f)); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(connect(extract,"in",color_to_v2,pxr::SdfValueTypeNames->Float2)); ASSERT_TRUE(connect(surface,"specular_roughness",extract,pxr::SdfValueTypeNames->Float)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); bool found_float_to_v2=false,found_v2_to_v3=false,found_v3_to_color=false,found_color_to_v2=false; for(ShaderNode *node:lowered.nodes){found_float_to_v2|=node->name=="FloatToV2";found_v2_to_v3|=node->name=="V2ToV3";found_v3_to_color|=node->name=="V3ToColor";found_color_to_v2|=node->name=="ColorToV2";} EXPECT_TRUE(found_float_to_v2);EXPECT_TRUE(found_v2_to_v3);EXPECT_TRUE(found_v3_to_color);EXPECT_TRUE(found_color_to_v2);
}

TEST(materialx_usdshade_reader, reads_and_lowers_color3_clamp_and_scalar_component_math)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/Color3Batch"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader result=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Color3Batch").AppendChild(pxr::TfToken(name)));result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));result.CreateOutput(pxr::TfToken("out"),type);return result;};
  const auto connect=[](pxr::UsdShadeShader &node,const char *name,pxr::UsdShadeShader &source,const pxr::SdfValueTypeName &type){return node.CreateInput(pxr::TfToken(name),type).ConnectToSource(source.ConnectableAPI(),pxr::TfToken("out"));};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),color=shader("Color","ND_constant_color3",pxr::SdfValueTypeNames->Color3f),scalar=shader("Scalar","ND_constant_float",pxr::SdfValueTypeNames->Float),clamp=shader("Clamp","ND_clamp_color3",pxr::SdfValueTypeNames->Color3f),clamp_fa=shader("ClampFA","ND_clamp_color3FA",pxr::SdfValueTypeNames->Color3f),modulo=shader("Modulo","ND_modulo_color3FA",pxr::SdfValueTypeNames->Color3f),power=shader("Power","ND_power_color3FA",pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(-1,0.5f,4)); scalar.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float).Set(2.0f); ASSERT_TRUE(connect(clamp,"in",color,pxr::SdfValueTypeNames->Color3f)); clamp.CreateInput(pxr::TfToken("low"),pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(0,0.25f,1)); clamp.CreateInput(pxr::TfToken("high"),pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(1,0.75f,3)); ASSERT_TRUE(connect(clamp_fa,"in",clamp,pxr::SdfValueTypeNames->Color3f)); clamp_fa.CreateInput(pxr::TfToken("low"),pxr::SdfValueTypeNames->Float).Set(0.0f); clamp_fa.CreateInput(pxr::TfToken("high"),pxr::SdfValueTypeNames->Float).Set(2.0f); ASSERT_TRUE(connect(modulo,"in1",clamp_fa,pxr::SdfValueTypeNames->Color3f)); ASSERT_TRUE(connect(modulo,"in2",scalar,pxr::SdfValueTypeNames->Float)); ASSERT_TRUE(connect(power,"in1",modulo,pxr::SdfValueTypeNames->Color3f)); ASSERT_TRUE(connect(power,"in2",scalar,pxr::SdfValueTypeNames->Float)); ASSERT_TRUE(connect(surface,"base_color",power,pxr::SdfValueTypeNames->Color3f)); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int minimum=0,maximum=0,modulo_count=0,power_count=0;for(ShaderNode *node:lowered.nodes)if(const MathNode *math=dynamic_cast<MathNode *>(node)){minimum+=math->get_math_type()==NODE_MATH_MINIMUM;maximum+=math->get_math_type()==NODE_MATH_MAXIMUM;modulo_count+=math->get_math_type()==NODE_MATH_MODULO;power_count+=math->get_math_type()==NODE_MATH_POWER;} EXPECT_EQ(minimum,6);EXPECT_EQ(maximum,6);EXPECT_EQ(modulo_count,3);EXPECT_EQ(power_count,3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_exact_domain_math_vector3_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector3DomainMath"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector3DomainMath").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader acos = shader("Acos", "ND_acos_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader asin = shader("Asin", "ND_asin_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader exponent = shader("Exp", "ND_exp_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader logarithm = shader("Ln", "ND_ln_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader square_root = shader("Sqrt", "ND_sqrt_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader round = shader("Round", "ND_round_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader acos_extract = shader("AcosExtract", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sqrt_extract = shader("SqrtExtract", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum = shader("Sum", "ND_add_float", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  ASSERT_TRUE(acos.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(asin.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(exponent.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(asin.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(logarithm.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(exponent.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(square_root.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(logarithm.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(round.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(square_root.ConnectableAPI(), pxr::TfToken("out")));
  acos_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  sqrt_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(acos_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(acos.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sqrt_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(round.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(acos_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(sqrt_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  bool found_acos = false, found_asin = false, found_exp = false, found_ln = false,
       found_ln_base_e = false, found_sqrt = false;
  for (ShaderNode *node : lowered.nodes) {
    const MathNode *math = dynamic_cast<MathNode *>(node);
    if (!math) continue;
    found_acos |= math->get_math_type() == NODE_MATH_ARCCOSINE;
    found_asin |= math->get_math_type() == NODE_MATH_ARCSINE;
    found_exp |= math->get_math_type() == NODE_MATH_EXPONENT;
    found_ln |= math->get_math_type() == NODE_MATH_LOGARITHM;
    found_ln_base_e |= math->get_math_type() == NODE_MATH_LOGARITHM &&
                       std::abs(math->get_value2() - float(M_E)) < 1e-6f;
    found_sqrt |= math->get_math_type() == NODE_MATH_SQRT;
  }
  EXPECT_TRUE(found_acos);
  EXPECT_TRUE(found_asin);
  EXPECT_TRUE(found_exp);
  EXPECT_TRUE(found_ln);
  EXPECT_TRUE(found_ln_base_e);
  EXPECT_TRUE(found_sqrt);
}

TEST(materialx_usdshade_reader, reads_and_lowers_atan2_vector2_and_vector3_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Atan2Vectors"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Atan2Vectors").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader vector2_y = shader("Vector2Y", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader vector2_x = shader("Vector2X", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader atan2_vector2 = shader("Atan2Vector2", "ND_atan2_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader vector3_y = shader("Vector3Y", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader vector3_x = shader("Vector3X", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader atan2_vector3 = shader("Atan2Vector3", "ND_atan2_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader extract2 = shader("Extract2", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3 = shader("Extract3", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum = shader("Sum", "ND_add_float", pxr::SdfValueTypeNames->Float);
  vector2_y.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(4.0f, 5.0f));
  vector2_x.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(3.0f, 2.0f));
  vector3_y.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(4.0f, 5.0f, 6.0f));
  vector3_x.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(3.0f, 2.0f, 1.0f));
  ASSERT_TRUE(atan2_vector2.CreateInput(pxr::TfToken("iny"), pxr::SdfValueTypeNames->Float2).ConnectToSource(vector2_y.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(atan2_vector2.CreateInput(pxr::TfToken("inx"), pxr::SdfValueTypeNames->Float2).ConnectToSource(vector2_x.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(atan2_vector3.CreateInput(pxr::TfToken("iny"), pxr::SdfValueTypeNames->Float3).ConnectToSource(vector3_y.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(atan2_vector3.CreateInput(pxr::TfToken("inx"), pxr::SdfValueTypeNames->Float3).ConnectToSource(vector3_x.ConnectableAPI(), pxr::TfToken("out")));
  extract2.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  extract3.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(extract2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(atan2_vector2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(extract3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(atan2_vector3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  int atan2_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    const MathNode *math = dynamic_cast<MathNode *>(node);
    atan2_count += math != nullptr && math->get_math_type() == NODE_MATH_ARCTAN2;
  }
  EXPECT_EQ(atan2_count, 5);
}

TEST(materialx_usdshade_reader, reads_and_lowers_invert_vector2_and_vector3_with_literal_and_linked_amounts)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/InvertVectors"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/InvertVectors").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader constant2 = shader("Constant2", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader constant3 = shader("Constant3", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader constant_amount = shader("Amount", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader invert2 = shader("Invert2", "ND_invert_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader invert2fa = shader("Invert2FA", "ND_invert_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader invert3 = shader("Invert3", "ND_invert_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader invert3fa = shader("Invert3FA", "ND_invert_vector3FA", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader extract2 = shader("Extract2", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract2fa = shader("Extract2FA", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3 = shader("Extract3", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3fa = shader("Extract3FA", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum1 = shader("Sum1", "ND_add_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum2 = shader("Sum2", "ND_add_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum3 = shader("Sum3", "ND_add_float", pxr::SdfValueTypeNames->Float);
  constant2.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.2f, 0.8f));
  constant3.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.2f, 0.5f, 0.8f));
  constant_amount.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  invert2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.2f, 0.8f));
  invert2.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.0f, 0.5f));
  ASSERT_TRUE(invert2fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(constant2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(invert2fa.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).ConnectToSource(constant_amount.ConnectableAPI(), pxr::TfToken("out")));
  invert3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.2f, 0.5f, 0.8f));
  invert3.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.0f, 0.5f, 1.0f));
  ASSERT_TRUE(invert3fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(constant3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(invert3fa.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).ConnectToSource(constant_amount.ConnectableAPI(), pxr::TfToken("out")));
  extract2.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  extract2fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  extract3.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  extract3fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(extract2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(invert2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(extract2fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(invert2fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(extract3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(invert3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(extract3fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(invert3fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum1.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum1.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract2fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum2.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum1.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum2.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum3.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum3.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(extract3fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum3.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  int subtracts = 0;
  for (ShaderNode *node : lowered.nodes) {
    const MathNode *math = dynamic_cast<MathNode *>(node);
    subtracts += math != nullptr && math->get_math_type() == NODE_MATH_SUBTRACT;
  }
  EXPECT_GE(subtracts, 10);
}

TEST(materialx_usdshade_reader, reads_and_lowers_smoothstep_vector2_and_vector3_forms)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/SmoothVectors"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SmoothVectors").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), type); return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader v2 = shader("V2", "ND_smoothstep_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader v2fa = shader("V2FA", "ND_smoothstep_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader v3 = shader("V3", "ND_smoothstep_vector3", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader v3fa = shader("V3FA", "ND_smoothstep_vector3FA", pxr::SdfValueTypeNames->Float3);
  pxr::UsdShadeShader e2 = shader("E2", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader e2fa = shader("E2FA", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader e3 = shader("E3", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader e3fa = shader("E3FA", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum1 = shader("Sum1", "ND_add_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum2 = shader("Sum2", "ND_add_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum3 = shader("Sum3", "ND_add_float", pxr::SdfValueTypeNames->Float);
  v2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(.25f, .75f));
  v2.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.0f, .25f));
  v2.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, 1.25f));
  v2fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(.25f, .75f));
  v2fa.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float).Set(0.0f); v2fa.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  v3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(.25f, .5f, .75f));
  v3.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.0f, .25f, .5f));
  v3.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(1.0f, 1.25f, 1.5f));
  v3fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(.25f, .5f, .75f));
  v3fa.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float).Set(0.0f); v3fa.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  e2.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0); e2fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1); e3.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1); e3fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(e2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(e2fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(e3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(e3fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum1.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(e2.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(sum1.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(e2fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum2.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum1.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(sum2.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(e3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum3.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum2.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(sum3.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(e3fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum3.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source, &lowered));
  int divides = 0, results = 0; for (ShaderNode *node : lowered.nodes) { const MathNode *math = dynamic_cast<MathNode *>(node); divides += math && math->get_math_type() == NODE_MATH_DIVIDE; results += math && math->get_math_type() == NODE_MATH_MULTIPLY; }
  EXPECT_GE(divides, 10); EXPECT_GE(results, 20);
}

TEST(materialx_usdshade_reader, keeps_vector2_cosine_z_zero_before_magnitude)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2CosineMagnitude"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2CosineMagnitude").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader cosine = shader("Cosine", "ND_cos_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader magnitude = shader("Magnitude", "ND_magnitude_vector2", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.0f, 0.0f));
  ASSERT_TRUE(cosine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(cosine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  CombineXYZNode *mask = nullptr;
  VectorMathNode *magnitude_math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    mask = node->name == "Cosine.vector2" ? dynamic_cast<CombineXYZNode *>(node) : mask;
    magnitude_math = node->name == "Magnitude" ? dynamic_cast<VectorMathNode *>(node) : magnitude_math;
  }
  ASSERT_NE(mask, nullptr);
  ASSERT_NE(magnitude_math, nullptr);
  ASSERT_NE(magnitude_math->input("Vector1")->link, nullptr);
  EXPECT_EQ(magnitude_math->input("Vector1")->link->parent, mask);
}

TEST(materialx_usdshade_reader, keeps_supported_vector2_math_chains_dimension_safe)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2DimensionSafeChain"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2DimensionSafeChain").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader scalar = shader("Scalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader combine = shader("Combine", "ND_combine2_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader cosine = shader("Cosine", "ND_cos_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader scale = shader("Scale", "ND_multiply_vector2FA", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader offset = shader("Offset", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader add = shader("Add", "ND_add_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader normalize = shader("Normalize", "ND_normalize_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader magnitude = shader("Magnitude", "ND_magnitude_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader dot = shader("Dot", "ND_dotproduct_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader distance = shader("Distance", "ND_distance_vector2", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader sum = shader("Sum", "ND_add_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader total = shader("Total", "ND_add_float", pxr::SdfValueTypeNames->Float);
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  ASSERT_TRUE(combine.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  combine.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  ASSERT_TRUE(cosine.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(scale.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(cosine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(scale.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  offset.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(1.0f, -1.0f));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(scale.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(offset.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(normalize.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(distance.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(dot.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(total.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(total.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(distance.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(total.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  std::unordered_map<string, CombineXYZNode *> masks;
  std::unordered_map<string, VectorMathNode *> consumers;
  for (ShaderNode *node : lowered.nodes) {
    if (auto *mask = dynamic_cast<CombineXYZNode *>(node)) masks.emplace(node->name, mask);
    if (auto *math = dynamic_cast<VectorMathNode *>(node)) consumers.emplace(node->name, math);
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

TEST(materialx_usdshade_reader, reads_and_lowers_exact_vector2_normalize)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Vector2Normalize"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Vector2Normalize").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader input = shader("Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader normalize = shader("Normalize", "ND_normalize_vector2", pxr::SdfValueTypeNames->Float2);
  pxr::UsdShadeShader extract = shader("Extract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(3.0f, 4.0f));
  ASSERT_TRUE(normalize.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(normalize.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VectorMathNode *math = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    math = node->name == "Normalize" ? dynamic_cast<VectorMathNode *>(node) : math;
  }
  ASSERT_NE(math, nullptr);
  EXPECT_EQ(math->get_math_type(), NODE_VECTOR_MATH_NORMALIZE);
}

TEST(materialx_usdshade_reader, reads_and_lowers_scalar_displacement_terminal)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);

  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/DisplacementMultiply"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/DisplacementFirst"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/DisplacementSecond"));
  pxr::UsdShadeShader displacement = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Displacement"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.4f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  displacement.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_displacementshader")));
  ASSERT_TRUE(displacement.CreateInput(pxr::TfToken("displacement"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")));
  displacement.CreateInput(pxr::TfToken("scale"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  displacement.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateDisplacementOutput(mtlx_render_context)
                  .ConnectToSource(displacement.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_displacement);
  EXPECT_TRUE(source.displacement.is_linked);
  EXPECT_EQ(source.displacement.link.source_node, "DisplacementMultiply");
  EXPECT_FLOAT_EQ(source.displacement_scale.value, 2.0f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  DisplacementNode *native_displacement = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (node->name == "Displacement") native_displacement = dynamic_cast<DisplacementNode *>(node);
  }
  ASSERT_NE(native_displacement, nullptr);
  EXPECT_FLOAT_EQ(native_displacement->get_midlevel(), 0.0f);
  EXPECT_FLOAT_EQ(native_displacement->get_scale(), 2.0f);
  ASSERT_NE(native_displacement->input("Height")->link, nullptr);
  EXPECT_EQ(native_displacement->input("Height")->link->parent->name, "DisplacementMultiply");
  ASSERT_NE(lowered.output()->input("Displacement")->link, nullptr);
  EXPECT_EQ(lowered.output()->input("Displacement")->link->parent, native_displacement);
}

TEST(materialx_usdshade_reader, reads_literal_and_linked_unclamped_mix_nodes)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Mix"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Mix/OpenPBR"));
  pxr::UsdShadeShader scalar = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Mix/ScalarMix"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Mix/ColorMix"));
  pxr::UsdShadeShader vector = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Mix/VectorMix"));
  pxr::UsdShadeShader normal = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Mix/Normal"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  scalar.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_float")));
  scalar.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  scalar.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Float).Set(6.0f);
  scalar.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).Set(-0.5f);
  scalar.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_color3")));
  color.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(0.1f));
  color.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(0.8f));
  ASSERT_TRUE(color.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  vector.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_vector3")));
  vector.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.0f, 0.0f, 1.0f));
  vector.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(1.0f, 0.0f, 0.0f));
  ASSERT_TRUE(vector.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  vector.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  normal.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_normalmap_float")));
  ASSERT_TRUE(normal.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(vector.ConnectableAPI(), pxr::TfToken("out")));
  normal.CreateInput(pxr::TfToken("scale"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  normal.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_normal"), pxr::SdfValueTypeNames->Float3).ConnectToSource(normal.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph graph; string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  bool scalar_seen = false, color_seen = false, vector_seen = false;
  for (const materialx::Node &node : graph.nodes) { scalar_seen |= node.nodedef == "ND_mix_float"; color_seen |= node.nodedef == "ND_mix_color3"; vector_seen |= node.nodedef == "ND_mix_vector3"; }
  EXPECT_TRUE(scalar_seen); EXPECT_TRUE(color_seen); EXPECT_TRUE(vector_seen);
  ShaderGraph lowered; EXPECT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_chained_scalar_compositing_blends)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ScalarCompositingBlends"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ScalarCompositingBlends/OpenPBR"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
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
  std::vector<pxr::UsdShadeShader> blends;
  blends.reserve(std::size(cases));
  for (size_t index = 0; index < std::size(cases); index++) {
    pxr::UsdShadeShader blend = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ScalarCompositingBlends").AppendChild(pxr::TfToken(cases[index].name)));
    blend.CreateIdAttr(pxr::VtValue(pxr::TfToken(cases[index].nodedef)));
    blend.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Float).Set(0.2f);
    blend.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float).Set(0.75f);
    blend.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    if (index == 0) {
      blend.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Float).Set(0.8f);
    }
    else {
      ASSERT_TRUE(blend.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Float)
                      .ConnectToSource(blends.back().ConnectableAPI(), pxr::TfToken("out")));
    }
    blends.push_back(blend);
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(blends.back().ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  for (const BlendCase &test_case : cases) {
    EXPECT_NE(std::find_if(source.nodes.begin(), source.nodes.end(), [&](const materialx::Node &node) {
                return node.nodedef == test_case.nodedef;
              }),
              source.nodes.end()) << test_case.nodedef;
  }
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  std::unordered_set<NodeMix> actual_types;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *blend = dynamic_cast<MixColorNode *>(node)) {
      actual_types.insert(blend->get_blend_type());
      EXPECT_FALSE(blend->get_use_clamp());
      EXPECT_FALSE(blend->get_use_clamp_result());
    }
  }
  for (const BlendCase &test_case : cases) {
    EXPECT_TRUE(actual_types.contains(test_case.mix_type)) << test_case.nodedef;
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_color3_compositing_blends)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::SdfPath root("/Looks/Color3CompositingBlends");
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader background = shader(
      "Background", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  pxr::UsdShadeShader foreground = shader(
      "Foreground", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  pxr::UsdShadeShader factor = shader("Factor", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  background.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  foreground.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.8f, 0.3f, 0.1f));
  factor.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.35f);

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
  std::vector<pxr::UsdShadeShader> blends;
  for (size_t index = 0; index < std::size(cases); index++) {
    pxr::UsdShadeShader blend = shader(
        cases[index].name, cases[index].nodedef, pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(blend.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource((index == 0 ? background : blends.back()).ConnectableAPI(),
                                     pxr::TfToken("out")));
    ASSERT_TRUE(blend.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(foreground.ConnectableAPI(), pxr::TfToken("out")));
    if (index % 2 == 0) {
      ASSERT_TRUE(blend.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float)
                      .ConnectToSource(factor.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      blend.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float)
          .Set(0.25f + 0.05f * float(index));
    }
    blends.push_back(blend);
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(blends.back().ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  std::unordered_map<string, MixColorNode *> lowered_blends;
  ValueNode *lowered_factor = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (auto *blend = dynamic_cast<MixColorNode *>(node)) {
      lowered_blends.emplace(node->name, blend);
    }
    lowered_factor = node->name == "Factor" ? dynamic_cast<ValueNode *>(node) : lowered_factor;
  }
  ASSERT_NE(lowered_factor, nullptr);
  for (size_t index = 0; index < std::size(cases); index++) {
    if (string(cases[index].nodedef) == "ND_burn_color3" ||
        string(cases[index].nodedef) == "ND_dodge_color3")
    {
      EXPECT_EQ(lowered_blends.count(cases[index].name), 0);
      continue;
    }
    MixColorNode *blend = lowered_blends[cases[index].name];
    ASSERT_NE(blend, nullptr) << cases[index].nodedef;
    EXPECT_EQ(blend->get_blend_type(), cases[index].mix_type);
    if (index % 2 == 0) {
      EXPECT_EQ(blend->input("Factor")->link, lowered_factor->output("Value"));
    }
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_mix_color3_color3_literal_factor)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::SdfPath root("/Looks/MixColor3Color3");
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    return result;
  };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, root.AppendChild(pxr::TfToken("OpenPBR")));
  surface.CreateIdAttr(
      pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader background = shader("Background", "ND_constant_color3");
  pxr::UsdShadeShader foreground = shader("Foreground", "ND_constant_color3");
  pxr::UsdShadeShader mix = shader("Mix", "ND_mix_color3_color3");
  background.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  foreground.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.8f, 0.6f, 0.4f));
  ASSERT_TRUE(mix.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(background.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(mix.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(foreground.ConnectableAPI(), pxr::TfToken("out")));
  mix.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.2f, 0.5f, 0.8f));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(mix.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  const auto mix_node = std::find_if(
      source.nodes.begin(), source.nodes.end(), [](const materialx::Node &node) {
        return node.nodedef == "ND_mix_color3_color3";
      });
  ASSERT_NE(mix_node, source.nodes.end());
  EXPECT_EQ(mix_node->color3_inputs.at("mix"), make_float3(0.2f, 0.5f, 0.8f));
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MixNode *product = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    product = node->name == "Mix.product" ? dynamic_cast<MixNode *>(node) : product;
  }
  ASSERT_NE(product, nullptr);
  EXPECT_EQ(product->get_color2(), make_float3(0.2f, 0.5f, 0.8f));
  EXPECT_EQ(product->input("Color2")->link, nullptr);
}

TEST(materialx_usdshade_reader, rejects_invalid_color_compositing_factors_without_mutation)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (const auto &[nodedef, wrong_type, nonfinite] :
       {std::tuple{"ND_plus_color3", true, false},
        std::tuple{"ND_plus_color3", false, true},
        std::tuple{"ND_mix_color3_color3", true, false},
        std::tuple{"ND_mix_color3_color3", false, true}})
  {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::SdfPath root("/Looks/InvalidColorFactor");
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
    const auto shader = [&](const char *name,
                            const char *id,
                            const pxr::SdfValueTypeName &output_type) {
      pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
          stage, root.AppendChild(pxr::TfToken(name)));
      result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
      result.CreateOutput(pxr::TfToken("out"), output_type);
      return result;
    };
    pxr::UsdShadeShader surface = shader(
        "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader blend = shader(
        "Blend", nodedef, pxr::SdfValueTypeNames->Color3f);
    blend.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
    blend.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(0.4f, 0.5f, 0.6f));
    const bool expects_color = string(nodedef) == "ND_mix_color3_color3";
    const bool provide_color = wrong_type ? !expects_color : expects_color;
    if (provide_color) {
      blend.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(nonfinite ? nan : 0.2f, 0.5f, 0.8f));
    }
    else {
      blend.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float)
          .Set(nonfinite ? nan : 0.5f);
    }
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(blend.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    materialx::Node sentinel;
    sentinel.name = "sentinel";
    sentinel.nodedef = "sentinel";
    graph.nodes.push_back(sentinel);
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_chained_color3_scalar_math)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color3ScalarMath"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Color3ScalarMath").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader color = shader("Color", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.2f, 0.4f, 0.6f));
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
  std::vector<pxr::UsdShadeShader> math;
  math.reserve(std::size(cases));
  for (size_t index = 0; index < std::size(cases); index++) {
    pxr::UsdShadeShader scalar = shader((string(cases[index].name) + "Scalar").c_str(),
                                        "ND_constant_float",
                                        pxr::SdfValueTypeNames->Float);
    scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(
        0.25f + float(index) * 0.1f);
    pxr::UsdShadeShader current = shader(cases[index].name, cases[index].nodedef, pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(current.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource((index == 0 ? color : math.back()).ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(current.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
    math.push_back(current);
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(math.back().ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  for (const MathCase &test_case : cases) {
    const auto it = std::find_if(source.nodes.begin(), source.nodes.end(), [&](const materialx::Node &node) {
      return node.nodedef == test_case.nodedef;
    });
    ASSERT_NE(it, source.nodes.end()) << test_case.nodedef;
    EXPECT_EQ(it->links.at("in1").type, materialx::Type::Color3);
    EXPECT_EQ(it->links.at("in2").type, materialx::Type::Float);
  }
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  std::unordered_set<NodeMix> actual_types;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *mix = dynamic_cast<MixNode *>(node)) actual_types.insert(mix->get_mix_type());
  }
  for (const MathCase &test_case : cases) EXPECT_TRUE(actual_types.contains(test_case.mix_type));
}

TEST(materialx_usdshade_reader, reads_and_lowers_chained_color3_modulo_and_power)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Color3ComponentMath"));
  const auto shader = [&](const char *name, const char *id) { pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Color3ComponentMath").AppendChild(pxr::TfToken(name))); result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); result.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f); return result; };
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Color3ComponentMath/OpenPBR")); surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader first = shader("First", "ND_constant_color3"); first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(5.5f, 6.5f, 7.5f));
  pxr::UsdShadeShader second = shader("Second", "ND_constant_color3"); second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(2.0f, 3.0f, 4.0f));
  pxr::UsdShadeShader modulo = shader("Modulo", "ND_modulo_color3"); pxr::UsdShadeShader power = shader("Power", "ND_power_color3");
  ASSERT_TRUE(modulo.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(modulo.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(power.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(modulo.ConnectableAPI(), pxr::TfToken("out"))); ASSERT_TRUE(power.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f).ConnectToSource(power.ConnectableAPI(), pxr::TfToken("out"))); const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph graph; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(graph, &lowered));
  int modulo_count = 0, power_count = 0; for (ShaderNode *node : lowered.nodes) if (const auto *math = dynamic_cast<MathNode *>(node)) { modulo_count += math->get_math_type() == NODE_MATH_MODULO; power_count += math->get_math_type() == NODE_MATH_POWER; } EXPECT_EQ(modulo_count, 3); EXPECT_EQ(power_count, 3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_color3_safepower_with_negative_channel)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage); const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/SafePower"));
  const auto shader=[&](const char *name,const char *id){ pxr::UsdShadeShader s=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/SafePower").AppendChild(pxr::TfToken(name))); s.CreateIdAttr(pxr::VtValue(pxr::TfToken(id))); s.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Color3f); return s; };
  pxr::UsdShadeShader surface=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/SafePower/OpenPBR")); surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"),pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader value=shader("Value","ND_constant_color3"); value.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(-2.0f,3.0f,4.0f)); pxr::UsdShadeShader exponent=shader("Exponent","ND_constant_color3"); exponent.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Color3f).Set(pxr::GfVec3f(2.0f,2.0f,2.0f)); pxr::UsdShadeShader safe=shader("Safe","ND_safepower_color3");
  ASSERT_TRUE(safe.CreateInput(pxr::TfToken("in1"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(value.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(safe.CreateInput(pxr::TfToken("in2"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(exponent.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"),pxr::SdfValueTypeNames->Color3f).ConnectToSource(safe.ConnectableAPI(),pxr::TfToken("out"))); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph graph; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&graph,&error)) << error; const auto it=std::find_if(graph.nodes.begin(),graph.nodes.end(),[](const materialx::Node &n){return n.nodedef=="ND_safepower_color3";}); ASSERT_NE(it,graph.nodes.end()); EXPECT_EQ(it->links.at("in1").type,materialx::Type::Color3); EXPECT_EQ(it->links.at("in2").type,materialx::Type::Color3);
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(graph,&lowered)); int abs=0,sign=0,power=0,multiply=0; for(ShaderNode *n:lowered.nodes) if(const auto *m=dynamic_cast<MathNode *>(n)){abs+=m->get_math_type()==NODE_MATH_ABSOLUTE;sign+=m->get_math_type()==NODE_MATH_SIGN;power+=m->get_math_type()==NODE_MATH_POWER;multiply+=m->get_math_type()==NODE_MATH_MULTIPLY;} EXPECT_EQ(abs,3); EXPECT_EQ(sign,3); EXPECT_EQ(power,3); EXPECT_EQ(multiply,3);
}

TEST(materialx_usdshade_reader, reads_and_lowers_scalar_domain_and_safepower_batch)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::SdfPath root("/Looks/ScalarDomainSafePower");
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
  const auto shader = [&](const char *name, const char *id) {
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(stage, root.AppendChild(pxr::TfToken(name)));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    return shader;
  };

  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, root.AppendChild(pxr::TfToken("OpenPBR")));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader acos = shader("Acos", "ND_acos_float");
  acos.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  pxr::UsdShadeShader asin = shader("Asin", "ND_asin_float");
  ASSERT_TRUE(asin.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float).ConnectToSource(acos.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader atan2 = shader("Atan2", "ND_atan2_float");
  ASSERT_TRUE(atan2.CreateInput(pxr::TfToken("iny"), pxr::SdfValueTypeNames->Float).ConnectToSource(asin.ConnectableAPI(), pxr::TfToken("out")));
  atan2.CreateInput(pxr::TfToken("inx"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  pxr::UsdShadeShader ln = shader("Ln", "ND_ln_float");
  ASSERT_TRUE(ln.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float).ConnectToSource(atan2.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader safepower = shader("SafePower", "ND_safepower_float");
  ASSERT_TRUE(safepower.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(ln.ConnectableAPI(), pxr::TfToken("out")));
  safepower.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(safepower.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  for (const char *nodedef : {"ND_acos_float", "ND_asin_float", "ND_atan2_float", "ND_ln_float", "ND_safepower_float"}) {
    EXPECT_NE(std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) { return node.nodedef == nodedef; }), graph.nodes.end()) << nodedef;
  }
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int acos_count = 0, asin_count = 0, atan2_count = 0, logarithm_count = 0, power_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      acos_count += math->get_math_type() == NODE_MATH_ARCCOSINE;
      asin_count += math->get_math_type() == NODE_MATH_ARCSINE;
      atan2_count += math->get_math_type() == NODE_MATH_ARCTAN2;
      logarithm_count += math->get_math_type() == NODE_MATH_LOGARITHM;
      power_count += math->get_math_type() == NODE_MATH_POWER;
    }
  }
  EXPECT_EQ(acos_count, 1);
  EXPECT_EQ(asin_count, 1);
  EXPECT_EQ(atan2_count, 1);
  EXPECT_EQ(logarithm_count, 1);
  EXPECT_GE(power_count, 1);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector_safepower_batch)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::SdfPath root("/Looks/VectorSafePower");
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(stage, root.AppendChild(pxr::TfToken(name)));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    shader.CreateOutput(pxr::TfToken("out"), type);
    return shader;
  };

  pxr::UsdShadeShader surface = shader("OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader v2_input = shader("Vector2Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  v2_input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(-2.0f, 3.0f));
  pxr::UsdShadeShader v2_exp = shader("Vector2Exponent", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  v2_exp.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(2.0f, 3.0f));
  pxr::UsdShadeShader v2 = shader("SafeVector2", "ND_safepower_vector2", pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(v2.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2_input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(v2.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2_exp.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader v2fa = shader("SafeVector2FA", "ND_safepower_vector2FA", pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(v2fa.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2.ConnectableAPI(), pxr::TfToken("out")));
  v2fa.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  pxr::UsdShadeShader v2_extract = shader("ExtractVector2", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
  v2_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(v2_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2).ConnectToSource(v2fa.ConnectableAPI(), pxr::TfToken("out")));

  pxr::UsdShadeShader v3_input = shader("Vector3Input", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  v3_input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(-2.0f, 3.0f, -4.0f));
  pxr::UsdShadeShader v3_exp = shader("Vector3Exponent", "ND_constant_vector3", pxr::SdfValueTypeNames->Float3);
  v3_exp.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(2.0f, 3.0f, 0.5f));
  pxr::UsdShadeShader v3 = shader("SafeVector3", "ND_safepower_vector3", pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(v3.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3_input.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(v3.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3_exp.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader v3fa = shader("SafeVector3FA", "ND_safepower_vector3FA", pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(v3fa.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3.ConnectableAPI(), pxr::TfToken("out")));
  v3fa.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  pxr::UsdShadeShader v3_extract = shader("ExtractVector3", "ND_extract_vector3", pxr::SdfValueTypeNames->Float);
  v3_extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(v3_extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(v3fa.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader add = shader("Add", "ND_add_float", pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float).ConnectToSource(v2_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(add.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).ConnectToSource(v3_extract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  for (const char *nodedef : {"ND_safepower_vector2", "ND_safepower_vector2FA", "ND_safepower_vector3", "ND_safepower_vector3FA"}) {
    EXPECT_NE(std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) { return node.nodedef == nodedef; }), graph.nodes.end()) << nodedef;
  }
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int power_count = 0, sign_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *math = dynamic_cast<MathNode *>(node)) {
      power_count += math->get_math_type() == NODE_MATH_POWER;
      sign_count += math->get_math_type() == NODE_MATH_SIGN;
    }
  }
  EXPECT_EQ(power_count, 10);
  EXPECT_EQ(sign_count, 10);
}

TEST(materialx_usdshade_reader, reads_color3fa_invert_and_safepower_literal_scalars)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::SdfPath root("/Looks/Color3FA");
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
  const auto shader = [&](const char *name,
                          const char *id,
                          const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader color = shader(
      "Color", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(-2.0f, 3.0f, -4.0f));
  pxr::UsdShadeShader invert = shader(
      "Invert", "ND_invert_color3FA", pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(invert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  invert.CreateInput(pxr::TfToken("amount"), pxr::SdfValueTypeNames->Float).Set(0.625f);
  pxr::UsdShadeShader safe = shader(
      "Safe", "ND_safepower_color3FA", pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(safe.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(invert.ConnectableAPI(), pxr::TfToken("out")));
  safe.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.25f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(safe.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto invert_node = std::find_if(
      graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
        return node.nodedef == "ND_invert_color3FA";
      });
  const auto safe_node = std::find_if(
      graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
        return node.nodedef == "ND_safepower_color3FA";
      });
  ASSERT_NE(invert_node, graph.nodes.end());
  ASSERT_NE(safe_node, graph.nodes.end());
  EXPECT_FLOAT_EQ(invert_node->inputs.at("amount"), 0.625f);
  EXPECT_FLOAT_EQ(safe_node->inputs.at("in2"), 2.25f);
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, rejects_nonfinite_color3fa_scalar_operands_without_mutation)
{
  for (const auto &[nodedef, scalar_input, connected, invalid] :
       {std::tuple{"ND_invert_color3FA",
                   "amount",
                   false,
                   std::numeric_limits<float>::infinity()},
        std::tuple{"ND_invert_color3FA",
                   "amount",
                   true,
                   std::numeric_limits<float>::quiet_NaN()},
        std::tuple{"ND_safepower_color3FA",
                   "in2",
                   false,
                   std::numeric_limits<float>::quiet_NaN()},
        std::tuple{"ND_safepower_color3FA",
                   "in2",
                   true,
                   std::numeric_limits<float>::infinity()}})
  {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::SdfPath root("/Looks/InvalidColor3FAScalar");
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
    const auto shader = [&](const char *name,
                            const char *id,
                            const pxr::SdfValueTypeName &type) {
      pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
          stage, root.AppendChild(pxr::TfToken(name)));
      result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
      result.CreateOutput(pxr::TfToken("out"), type);
      return result;
    };
    pxr::UsdShadeShader surface = shader(
        "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader node = shader(
        "Invalid", nodedef, pxr::SdfValueTypeNames->Color3f);
    pxr::UsdShadeShader color = shader(
        "Color", "ND_constant_color3", pxr::SdfValueTypeNames->Color3f);
    color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(-2.0f, 3.0f, -4.0f));
    const char *color_input = string(nodedef) == "ND_invert_color3FA" ? "in" : "in1";
    ASSERT_TRUE(node.CreateInput(pxr::TfToken(color_input), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
    pxr::UsdShadeInput scalar =
        node.CreateInput(pxr::TfToken(scalar_input), pxr::SdfValueTypeNames->Float);
    if (connected) {
      pxr::UsdShadeShader constant = shader(
          "InvalidScalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
      constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(invalid);
      ASSERT_TRUE(
          scalar.ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
    }
    else {
      scalar.Set(invalid);
    }
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"),
                                   pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(node.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error))
        << nodedef << (connected ? " connected" : " literal");
    EXPECT_NE(error.find("finite"), string::npos)
        << nodedef << (connected ? " connected" : " literal") << ": " << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes.front().name, "sentinel");
  }
}

TEST(materialx_usdshade_reader, rejects_nonfinite_vector_safepower_literals_without_mutation)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  for (const auto &[nodedef, vector3, scalar_exponent, invalid_first] :
       {std::tuple{"ND_safepower_vector2", false, false, true},
        std::tuple{"ND_safepower_vector3", true, false, false},
        std::tuple{"ND_safepower_vector2FA", false, true, false},
        std::tuple{"ND_safepower_vector3FA", true, true, false}})
  {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::SdfPath root("/Looks/InvalidVectorSafePower");
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
    const pxr::SdfValueTypeName vector_type =
        vector3 ? pxr::SdfValueTypeNames->Float3 : pxr::SdfValueTypeNames->Float2;
    pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken("OpenPBR")));
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader power = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken("SafePower")));
    power.CreateIdAttr(pxr::VtValue(pxr::TfToken(nodedef)));
    power.CreateOutput(pxr::TfToken("out"), vector_type);
    if (vector3) {
      power.CreateInput(pxr::TfToken("in1"), vector_type)
          .Set(pxr::GfVec3f(invalid_first ? nan : -2.0f, 3.0f, 4.0f));
      if (!scalar_exponent) {
        power.CreateInput(pxr::TfToken("in2"), vector_type)
            .Set(pxr::GfVec3f(2.0f, 3.0f, invalid_first ? 0.5f : infinity));
      }
    }
    else {
      power.CreateInput(pxr::TfToken("in1"), vector_type)
          .Set(pxr::GfVec2f(invalid_first ? nan : -2.0f, 3.0f));
      if (!scalar_exponent) {
        power.CreateInput(pxr::TfToken("in2"), vector_type).Set(pxr::GfVec2f(2.0f, 3.0f));
      }
    }
    if (scalar_exponent) {
      power.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
          .Set(vector3 ? infinity : nan);
    }
    pxr::UsdShadeShader extract = pxr::UsdShadeShader::Define(
        stage, root.AppendChild(pxr::TfToken("Extract")));
    extract.CreateIdAttr(pxr::VtValue(pxr::TfToken(
        vector3 ? "ND_extract_vector3" : "ND_extract_vector2")));
    extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
    ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), vector_type)
                    .ConnectToSource(power.ConnectableAPI(), pxr::TfToken("out")));
    extract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                   pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)) << nodedef;
    ASSERT_EQ(graph.nodes.size(), 1) << nodedef;
    EXPECT_EQ(graph.nodes.front().name, "sentinel") << nodedef;
  }
}

TEST(materialx_usdshade_reader, rejects_wrong_mix_factor_type_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/BadMix"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/BadMix/OpenPBR"));
  pxr::UsdShadeShader mix = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/BadMix/Mix"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  mix.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_float"))); mix.CreateInput(pxr::TfToken("bg"), pxr::SdfValueTypeNames->Float).Set(0.0f); mix.CreateInput(pxr::TfToken("fg"), pxr::SdfValueTypeNames->Float).Set(1.0f); mix.CreateInput(pxr::TfToken("mix"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.5f)); mix.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(mix.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph graph; graph.nodes.push_back({"sentinel", "unsupported"}); string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)); EXPECT_EQ(graph.nodes.size(), 1); EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_noise2d_into_scalar_ramp)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp/UV"));
  pxr::UsdShadeShader noise = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp/Noise"));
  pxr::UsdShadeShader noise_coordinate = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp/NoiseCoordinate"));
  pxr::UsdShadeShader ramp = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NoiseRamp/Ramp"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  noise.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_noise2d_float")));
  noise.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.75f);
  noise.CreateInput(pxr::TfToken("pivot"), pxr::SdfValueTypeNames->Float).Set(0.1f);
  ASSERT_TRUE(noise.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  noise.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  noise_coordinate.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_combine2_vector2")));
  ASSERT_TRUE(noise_coordinate.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(noise.ConnectableAPI(), pxr::TfToken("out")));
  noise_coordinate.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  noise_coordinate.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_float")));
  ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Float).Set(0.2f);
  ramp.CreateInput(pxr::TfToken("valuer"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(noise_coordinate.ConnectableAPI(), pxr::TfToken("out")));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  const materialx::Node &noise_node = find_node("Noise");
  EXPECT_EQ(noise_node.nodedef, "ND_noise2d_float");
  EXPECT_EQ(noise_node.outputs.at("out"), materialx::Type::Float);
  EXPECT_FLOAT_EQ(noise_node.inputs.at("amplitude"), 0.75f);
  EXPECT_FLOAT_EQ(noise_node.inputs.at("pivot"), 0.1f);
  EXPECT_EQ(noise_node.links.at("texcoord").type, materialx::Type::Vector2);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector2_scalar_remap)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage); const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/Vector2RemapFA")); const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader s=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector2RemapFA").AppendChild(pxr::TfToken(name)));s.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));s.CreateOutput(pxr::TfToken("out"),type);return s;}; pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector2",pxr::SdfValueTypeNames->Float2),remap=shader("Remap","ND_remap_vector2FA",pxr::SdfValueTypeNames->Float2),extract=shader("Extract","ND_extract_vector2",pxr::SdfValueTypeNames->Float); input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(.25f,.75f)); ASSERT_TRUE(remap.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(input.ConnectableAPI(),pxr::TfToken("out"))); for(const auto &[name,value]:{std::pair{"inlow",0.0f},{"inhigh",1.0f},{"outlow",-1.0f},{"outhigh",1.0f}})remap.CreateInput(pxr::TfToken(name),pxr::SdfValueTypeNames->Float).Set(value); extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float2).ConnectToSource(remap.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(),pxr::TfToken("out"))); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out"))); materialx::Graph source;string error;ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error;ShaderGraph lowered;ASSERT_TRUE(materialx::lower(source,&lowered));int ranges=0;for(ShaderNode *node:lowered.nodes)if(const VectorMapRangeNode *range=dynamic_cast<VectorMapRangeNode *>(node)){++ranges;EXPECT_FALSE(range->get_use_clamp());}EXPECT_EQ(ranges,1);
}

TEST(materialx_usdshade_reader, reads_and_lowers_vector3_remap_and_scalar_remap)
{
  const pxr::UsdStageRefPtr stage=pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material=pxr::UsdShadeMaterial::Define(stage,pxr::SdfPath("/Looks/Vector3Remap"));
  const auto shader=[&](const char *name,const char *id,const pxr::SdfValueTypeName &type){pxr::UsdShadeShader s=pxr::UsdShadeShader::Define(stage,pxr::SdfPath("/Looks/Vector3Remap").AppendChild(pxr::TfToken(name)));s.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));s.CreateOutput(pxr::TfToken("out"),type);return s;};
  pxr::UsdShadeShader surface=shader("OpenPBR","ND_open_pbr_surface_surfaceshader",pxr::SdfValueTypeNames->Token),input=shader("Input","ND_constant_vector3",pxr::SdfValueTypeNames->Float3),remap=shader("Remap","ND_remap_vector3",pxr::SdfValueTypeNames->Float3),remap_fa=shader("RemapFA","ND_remap_vector3FA",pxr::SdfValueTypeNames->Float3),extract=shader("Extract","ND_extract_vector3",pxr::SdfValueTypeNames->Float);
  input.CreateInput(pxr::TfToken("value"),pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(.25f,.5f,.75f));
  ASSERT_TRUE(remap.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(input.ConnectableAPI(),pxr::TfToken("out")));
  for(const auto &[name,value]:{std::pair{"inlow",pxr::GfVec3f(0)}, {"inhigh",pxr::GfVec3f(1)}, {"outlow",pxr::GfVec3f(-1)}, {"outhigh",pxr::GfVec3f(1)}}) remap.CreateInput(pxr::TfToken(name),pxr::SdfValueTypeNames->Float3).Set(value);
  ASSERT_TRUE(remap_fa.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(remap.ConnectableAPI(),pxr::TfToken("out")));
  for(const auto &[name,value]:{std::pair{"inlow",0.0f}, {"inhigh",1.0f}, {"outlow",-1.0f}, {"outhigh",1.0f}}) remap_fa.CreateInput(pxr::TfToken(name),pxr::SdfValueTypeNames->Float).Set(value);
  extract.CreateInput(pxr::TfToken("index"),pxr::SdfValueTypeNames->Int).Set(0); ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"),pxr::SdfValueTypeNames->Float3).ConnectToSource(remap_fa.ConnectableAPI(),pxr::TfToken("out"))); ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),pxr::SdfValueTypeNames->Float).ConnectToSource(extract.ConnectableAPI(),pxr::TfToken("out"))); const pxr::TfToken context("mtlx",pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),pxr::TfToken("out")));
  materialx::Graph source; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material,&source,&error))<<error; ShaderGraph lowered; ASSERT_TRUE(materialx::lower(source,&lowered)); int ranges=0; for(ShaderNode *node:lowered.nodes) if(const VectorMapRangeNode *range=dynamic_cast<VectorMapRangeNode *>(node)){++ranges; EXPECT_EQ(range->get_range_type(),NODE_MAP_RANGE_LINEAR); EXPECT_FALSE(range->get_use_clamp());} EXPECT_EQ(ranges,2);
}

TEST(materialx_usdshade_reader, reads_and_lowers_linear_vector2_range_and_clamp)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/UV"));
  pxr::UsdShadeShader remap = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/Remap"));
  pxr::UsdShadeShader range = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/Range"));
  pxr::UsdShadeShader clamp = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/Clamp"));
  pxr::UsdShadeShader magnitude = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VectorAdjustment/Magnitude"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  remap.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_remap_vector2")));
  ASSERT_TRUE(remap.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", pxr::GfVec2f(0.0f, 0.0f)},
                                    {"inhigh", pxr::GfVec2f(1.0f, 1.0f)},
                                    {"outlow", pxr::GfVec2f(-1.0f, -0.5f)},
                                    {"outhigh", pxr::GfVec2f(1.0f, 0.5f)}})
  {
    remap.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float2).Set(value);
  }
  remap.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  range.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_range_vector2")));
  ASSERT_TRUE(range.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(remap.ConnectableAPI(), pxr::TfToken("out")));
  for (const auto &[name, value] : {std::pair{"inlow", pxr::GfVec2f(-1.0f, -0.5f)},
                                    {"inhigh", pxr::GfVec2f(1.0f, 0.5f)},
                                    {"gamma", pxr::GfVec2f(1.0f, 1.0f)},
                                    {"outlow", pxr::GfVec2f(0.0f, 0.0f)},
                                    {"outhigh", pxr::GfVec2f(1.0f, 1.0f)}})
  {
    range.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float2).Set(value);
  }
  range.CreateInput(pxr::TfToken("doclamp"), pxr::SdfValueTypeNames->Bool).Set(true);
  range.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  clamp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_clamp_vector2")));
  ASSERT_TRUE(clamp.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(range.ConnectableAPI(), pxr::TfToken("out")));
  clamp.CreateInput(pxr::TfToken("low"), pxr::SdfValueTypeNames->Float2).Set(
      pxr::GfVec2f(0.1f, 0.2f));
  clamp.CreateInput(pxr::TfToken("high"), pxr::SdfValueTypeNames->Float2).Set(
      pxr::GfVec2f(0.9f, 0.8f));
  clamp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  magnitude.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_magnitude_vector2")));
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(clamp.ConnectableAPI(), pxr::TfToken("out")));
  magnitude.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int ranges = 0;
  int clamps = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const VectorMapRangeNode *range_node = dynamic_cast<VectorMapRangeNode *>(node)) {
      ++ranges;
      if (node->name == "Range") {
        EXPECT_TRUE(range_node->get_use_clamp());
      }
    }
    if (const VectorMathNode *clamp_node = dynamic_cast<VectorMathNode *>(node)) {
      if (node->name == "Clamp.minimum") {
        EXPECT_EQ(clamp_node->get_math_type(), NODE_VECTOR_MATH_MINIMUM);
        ++clamps;
      }
      else if (node->name == "Clamp") {
        EXPECT_EQ(clamp_node->get_math_type(), NODE_VECTOR_MATH_MAXIMUM);
        ++clamps;
      }
    }
  }
  EXPECT_EQ(ranges, 2);
  EXPECT_EQ(clamps, 2);
}

TEST(materialx_usdshade_reader, rejects_inexact_vector2_range_inputs_without_mutation)
{
  for (const int rejection : {0, 1, 2, 3, 4}) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::SdfPath root("/Looks/InvalidVector2Range");
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, root);
    const auto shader = [&](const char *name,
                            const char *id,
                            const pxr::SdfValueTypeName &type) {
      pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
          stage, root.AppendChild(pxr::TfToken(name)));
      result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
      result.CreateOutput(pxr::TfToken("out"), type);
      return result;
    };
    pxr::UsdShadeShader surface = shader(
        "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
    pxr::UsdShadeShader input = shader(
        "Input", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
    input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
        .Set(pxr::GfVec2f(0.25f, 0.75f));
    pxr::UsdShadeShader range = shader(
        "Range", "ND_range_vector2", pxr::SdfValueTypeNames->Float2);
    ASSERT_TRUE(range.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
    const float nan = std::numeric_limits<float>::quiet_NaN();
    range.CreateInput(pxr::TfToken("inlow"), pxr::SdfValueTypeNames->Float2)
        .Set(rejection == 2 ? pxr::GfVec2f(nan, 0.0f) : pxr::GfVec2f(0.0f, 0.0f));
    range.CreateInput(pxr::TfToken("inhigh"), pxr::SdfValueTypeNames->Float2)
        .Set(rejection == 3 ? pxr::GfVec2f(0.0f, 1.0f) : pxr::GfVec2f(1.0f, 1.0f));
    range.CreateInput(pxr::TfToken("gamma"), pxr::SdfValueTypeNames->Float2)
        .Set(rejection == 0 ? pxr::GfVec2f(2.0f, 1.0f) : pxr::GfVec2f(1.0f, 1.0f));
    range.CreateInput(pxr::TfToken("outlow"), pxr::SdfValueTypeNames->Float2)
        .Set(rejection == 4 ? pxr::GfVec2f(2.0f, 0.0f) : pxr::GfVec2f(0.0f, 0.0f));
    range.CreateInput(pxr::TfToken("outhigh"), pxr::SdfValueTypeNames->Float2)
        .Set(pxr::GfVec2f(1.0f, 1.0f));
    if (rejection != 1) {
      range.CreateInput(pxr::TfToken("doclamp"), pxr::SdfValueTypeNames->Bool).Set(true);
    }
    pxr::UsdShadeShader extract = shader(
        "Extract", "ND_extract_vector2", pxr::SdfValueTypeNames->Float);
    extract.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
    ASSERT_TRUE(extract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(range.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"),
                                   pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(extract.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error)) << rejection;
    ASSERT_EQ(graph.nodes.size(), 1) << rejection;
    EXPECT_EQ(graph.nodes.front().name, "sentinel") << rejection;
  }
}

TEST(materialx_usdshade_reader, reads_and_lowers_rgb_hsv_color3_round_trip)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/HsvRoundTrip"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/HsvRoundTrip/OpenPBR"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/HsvRoundTrip/Color"));
  pxr::UsdShadeShader rgb_to_hsv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/HsvRoundTrip/RgbToHsv"));
  pxr::UsdShadeShader hsv_to_rgb = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/HsvRoundTrip/HsvToRgb"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
  color.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.2f, 0.6f, 0.8f));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  rgb_to_hsv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_rgbtohsv_color3")));
  ASSERT_TRUE(rgb_to_hsv.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  rgb_to_hsv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  hsv_to_rgb.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_hsvtorgb_color3")));
  ASSERT_TRUE(hsv_to_rgb.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(rgb_to_hsv.ConnectableAPI(), pxr::TfToken("out")));
  hsv_to_rgb.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(hsv_to_rgb.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  EXPECT_EQ(find_node("RgbToHsv").nodedef, "ND_rgbtohsv_color3");
  EXPECT_EQ(find_node("HsvToRgb").nodedef, "ND_hsvtorgb_color3");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_separate3_vector3_component)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/OpenPBR"));
  pxr::UsdShadeShader x = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/X"));
  pxr::UsdShadeShader y = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/Y"));
  pxr::UsdShadeShader z = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/Z"));
  pxr::UsdShadeShader combine = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/Combine"));
  pxr::UsdShadeShader separate = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/Separate"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  for (auto [shader, value] : {std::pair{x, 0.2f}, std::pair{y, 0.5f}, std::pair{z, 0.8f}}) {
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
    shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(value);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  }
  combine.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_combine3_vector3")));
  for (const auto &[name, shader] : {std::pair{"in1", x}, std::pair{"in2", y}, std::pair{"in3", z}}) {
    ASSERT_TRUE(combine.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(shader.ConnectableAPI(), pxr::TfToken("out")));
  }
  combine.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  separate.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_separate3_vector3")));
  ASSERT_TRUE(separate.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  for (const char *name : {"outx", "outy", "outz"}) {
    separate.CreateOutput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float);
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(separate.ConnectableAPI(), pxr::TfToken("outy")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.name == "Separate";
  });
  ASSERT_NE(it, graph.nodes.end());
  EXPECT_EQ(it->nodedef, "ND_separate3_vector3");
  EXPECT_EQ(it->outputs.at("outy"), materialx::Type::Float);
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_reflect_vector3)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Reflect"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Reflect/OpenPBR"));
  pxr::UsdShadeShader incident = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Reflect/Incident"));
  pxr::UsdShadeShader normal = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Reflect/Normal"));
  pxr::UsdShadeShader reflect = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Reflect/Reflect"));
  pxr::UsdShadeShader magnitude = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Reflect/Magnitude"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  for (auto [shader, value] : {std::pair{incident, pxr::GfVec3f(0.0f, -1.0f, 0.0f)}, std::pair{normal, pxr::GfVec3f(0.0f, 1.0f, 0.0f)}}) {
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3"))); shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(value); shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  }
  reflect.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_reflect_vector3"))); reflect.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(reflect.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(incident.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(reflect.CreateInput(pxr::TfToken("normal"), pxr::SdfValueTypeNames->Float3).ConnectToSource(normal.ConnectableAPI(), pxr::TfToken("out")));
  magnitude.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_magnitude_vector3"))); magnitude.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(reflect.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  materialx::Graph graph; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) { return node.name == "Reflect"; });
  ASSERT_NE(it, graph.nodes.end()); EXPECT_EQ(it->nodedef, "ND_reflect_vector3"); EXPECT_EQ(it->outputs.at("out"), materialx::Type::Vector3);
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_and_lowers_refract_vector3_with_connected_ior)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory(); ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Looks/Refract"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/OpenPBR"));
  pxr::UsdShadeShader incident = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/Incident"));
  pxr::UsdShadeShader normal = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/Normal"));
  pxr::UsdShadeShader ior = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/Ior"));
  pxr::UsdShadeShader refract = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/Refract"));
  pxr::UsdShadeShader magnitude = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/Refract/Magnitude"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader"))); surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  incident.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3"))); incident.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.8660254f, 0.0f, -0.5f)); incident.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  normal.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3"))); normal.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3).Set(pxr::GfVec3f(0.0f, 0.0f, 1.0f)); normal.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ior.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float"))); ior.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.5f); ior.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  refract.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_refract_vector3"))); refract.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(refract.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(incident.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(refract.CreateInput(pxr::TfToken("normal"), pxr::SdfValueTypeNames->Float3).ConnectToSource(normal.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(refract.CreateInput(pxr::TfToken("ior"), pxr::SdfValueTypeNames->Float).ConnectToSource(ior.ConnectableAPI(), pxr::TfToken("out")));
  magnitude.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_magnitude_vector3"))); magnitude.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(magnitude.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3).ConnectToSource(refract.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float).ConnectToSource(magnitude.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal); ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph; string error; ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) { return node.name == "Refract"; });
  ASSERT_NE(it, graph.nodes.end()); EXPECT_EQ(it->nodedef, "ND_refract_vector3"); EXPECT_EQ(it->links.at("scale").source_node, "Ior");
  ShaderGraph lowered; ASSERT_TRUE(materialx::lower(graph, &lowered));
  VectorMathNode *native_refract = nullptr;
  for (ShaderNode *node : lowered.nodes) native_refract = node->name == "Refract" ? dynamic_cast<VectorMathNode *>(node) : native_refract;
  ASSERT_NE(native_refract, nullptr); EXPECT_EQ(native_refract->get_math_type(), NODE_VECTOR_MATH_REFRACT);
  ASSERT_NE(native_refract->input("Scale")->link, nullptr); EXPECT_EQ(native_refract->input("Scale")->link->parent->name, "Ior");
}

TEST(materialx_usdshade_reader, reads_and_lowers_native_materialx_space_transform_family)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader input = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/InputVector"));
  pxr::UsdShadeShader point = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/PointTransform"));
  pxr::UsdShadeShader vector = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/VectorTransform"));
  pxr::UsdShadeShader normal = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/NormalTransform"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/VectorToColor"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  input.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  input.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  input.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  point.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_transformpoint_vector3")));
  ASSERT_TRUE(point.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(input.ConnectableAPI(), pxr::TfToken("out")));
  point.CreateInput(pxr::TfToken("fromspace"), pxr::SdfValueTypeNames->String).Set("object");
  point.CreateInput(pxr::TfToken("tospace"), pxr::SdfValueTypeNames->String).Set("world");
  point.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  vector.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_transformvector_vector3")));
  ASSERT_TRUE(vector.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(point.ConnectableAPI(), pxr::TfToken("out")));
  vector.CreateInput(pxr::TfToken("fromspace"), pxr::SdfValueTypeNames->String).Set("world");
  vector.CreateInput(pxr::TfToken("tospace"), pxr::SdfValueTypeNames->String).Set("camera");
  vector.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  normal.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_transformnormal_vector3")));
  ASSERT_TRUE(normal.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(vector.ConnectableAPI(), pxr::TfToken("out")));
  normal.CreateInput(pxr::TfToken("fromspace"), pxr::SdfValueTypeNames->String).Set("camera");
  normal.CreateInput(pxr::TfToken("tospace"), pxr::SdfValueTypeNames->String).Set("object");
  normal.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector3_color3")));
  ASSERT_TRUE(color.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(normal.ConnectableAPI(), pxr::TfToken("out")));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  bool saw_point = false, saw_vector = false, saw_normal = false;
  for (const materialx::Node &node : graph.nodes) {
    saw_point |= node.nodedef == "ND_transformpoint_vector3" &&
                 node.string_inputs.at("fromspace") == "object" &&
                 node.string_inputs.at("tospace") == "world";
    saw_vector |= node.nodedef == "ND_transformvector_vector3" &&
                  node.string_inputs.at("fromspace") == "world" &&
                  node.string_inputs.at("tospace") == "camera";
    saw_normal |= node.nodedef == "ND_transformnormal_vector3" &&
                  node.string_inputs.at("fromspace") == "camera" &&
                  node.string_inputs.at("tospace") == "object";
  }
  EXPECT_TRUE(saw_point);
  EXPECT_TRUE(saw_vector);
  EXPECT_TRUE(saw_normal);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int transform_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (node->type == VectorTransformNode::get_node_type()) {
      transform_count++;
    }
  }
  EXPECT_EQ(transform_count, 3);
}

TEST(materialx_usdshade_reader, rejects_malformed_native_materialx_space_transform_signature)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader transform = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/BadTransform"));
  pxr::UsdShadeShader color = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/VectorToColor"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  transform.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_transformpoint_vector3")));
  transform.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.25f, 0.5f, 0.75f));
  transform.CreateInput(pxr::TfToken("fromspace"), pxr::SdfValueTypeNames->String).Set("model");
  transform.CreateInput(pxr::TfToken("tospace"), pxr::SdfValueTypeNames->String).Set("world");
  transform.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  color.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector3_color3")));
  ASSERT_TRUE(color.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(transform.ConnectableAPI(), pxr::TfToken("out")));
  color.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(mtlx_render_context)
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("literal supported transform space"), string::npos) << error;
  EXPECT_TRUE(graph.nodes.empty());
}


TEST(materialx_usdshade_reader, reads_and_lowers_homogeneous_fractal2d_variants)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/UV"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  const auto shader = [&](const char *name,
                          const char *id,
                          const pxr::SdfValueTypeName &output_type) -> pxr::UsdShadeShader {
    pxr::UsdShadeShader node = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Fractal2D").AppendChild(pxr::TfToken(name)));
    node.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    node.CreateOutput(pxr::TfToken("out"), output_type);
    EXPECT_TRUE(node.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
    node.CreateInput(pxr::TfToken("octaves"), pxr::SdfValueTypeNames->Int).Set(4);
    node.CreateInput(pxr::TfToken("lacunarity"), pxr::SdfValueTypeNames->Float).Set(2.5f);
    node.CreateInput(pxr::TfToken("diminish"), pxr::SdfValueTypeNames->Float).Set(0.625f);
    return node;
  };

  pxr::UsdShadeShader scalar = shader(
      "Scalar", "ND_fractal2d_float", pxr::SdfValueTypeNames->Float);
  scalar.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  pxr::UsdShadeShader color = shader(
      "Color", "ND_fractal2d_color3", pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.75f, 1.0f));
  pxr::UsdShadeShader color_fa = shader(
      "ColorFA", "ND_fractal2d_color3FA", pxr::SdfValueTypeNames->Color3f);
  color_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.6f);
  pxr::UsdShadeShader vector2 = shader(
      "Vector2", "ND_fractal2d_vector2", pxr::SdfValueTypeNames->Float2);
  vector2.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.2f, 0.4f));
  pxr::UsdShadeShader vector2_fa = shader(
      "Vector2FA", "ND_fractal2d_vector2FA", pxr::SdfValueTypeNames->Float2);
  vector2_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.7f);
  pxr::UsdShadeShader vector3 = shader(
      "Vector3", "ND_fractal2d_vector3", pxr::SdfValueTypeNames->Float3);
  vector3.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.3f, 0.5f, 0.7f));
  pxr::UsdShadeShader vector3_fa = shader(
      "Vector3FA", "ND_fractal2d_vector3FA", pxr::SdfValueTypeNames->Float3);
  vector3_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.8f);

  pxr::UsdShadeShader extract2 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/Extract2"));
  extract2.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector2")));
  extract2.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  ASSERT_TRUE(extract2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(vector2.ConnectableAPI(), pxr::TfToken("out")));
  extract2.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract2_fa = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/Extract2FA"));
  extract2_fa.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector2")));
  extract2_fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(extract2_fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(vector2_fa.ConnectableAPI(), pxr::TfToken("out")));
  extract2_fa.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/Extract3"));
  extract3.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector3")));
  extract3.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(extract3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(vector3.ConnectableAPI(), pxr::TfToken("out")));
  extract3.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3_fa = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal2D/Extract3FA"));
  extract3_fa.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector3")));
  extract3_fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  ASSERT_TRUE(extract3_fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(vector3_fa.ConnectableAPI(), pxr::TfToken("out")));
  extract3_fa.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_opacity"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract2_fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("coat_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("coat_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract3_fa.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  for (const auto &[name, id, type] : {std::tuple{"Scalar", "ND_fractal2d_float", materialx::Type::Float},
                                      std::tuple{"Color", "ND_fractal2d_color3", materialx::Type::Color3},
                                      std::tuple{"ColorFA", "ND_fractal2d_color3FA", materialx::Type::Color3},
                                      std::tuple{"Vector2", "ND_fractal2d_vector2", materialx::Type::Vector2},
                                      std::tuple{"Vector2FA", "ND_fractal2d_vector2FA", materialx::Type::Vector2},
                                      std::tuple{"Vector3", "ND_fractal2d_vector3", materialx::Type::Vector3},
                                      std::tuple{"Vector3FA", "ND_fractal2d_vector3FA", materialx::Type::Vector3}})
  {
    const materialx::Node &node = find_node(name);
    EXPECT_EQ(node.nodedef, id);
    EXPECT_EQ(node.outputs.at("out"), type);
    EXPECT_EQ(node.links.at("texcoord").type, materialx::Type::Vector2);
    EXPECT_EQ(node.int_inputs.at("octaves"), 4);
    EXPECT_FLOAT_EQ(node.inputs.at("lacunarity"), 2.5f);
    EXPECT_FLOAT_EQ(node.inputs.at("diminish"), 0.625f);
  }

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int fractal_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *noise = dynamic_cast<NoiseTextureNode *>(node)) {
      EXPECT_EQ(noise->get_dimensions(), 2);
      EXPECT_EQ(noise->get_type(), NODE_NOISE_FBM);
      EXPECT_FLOAT_EQ(noise->get_detail(), 4.0f);
      EXPECT_FLOAT_EQ(noise->get_lacunarity(), 2.5f);
      EXPECT_FLOAT_EQ(noise->get_roughness(), 0.625f);
      ++fractal_count;
    }
  }
  EXPECT_EQ(fractal_count, 7);
}

TEST(materialx_usdshade_reader, reads_split_defaults_and_linked_inputs)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/SplitDefaults"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };

  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader uv = shader("UV", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f, 0.75f));
  pxr::UsdShadeShader center = shader("Center", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  center.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  pxr::UsdShadeShader left = shader("Left", "ND_constant_color4", pxr::SdfValueTypeNames->Color4f);
  left.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color4f).Set(
      pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f));
  pxr::UsdShadeShader split = shader("Split", "ND_splitlr_color4", pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(split.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(left.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(split.CreateInput(pxr::TfToken("center"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(center.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(split.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader alpha = shader("Alpha", "ND_extract_color4", pxr::SdfValueTypeNames->Float);
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(split.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto found = std::find_if(graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
    return node.name == "Split";
  });
  ASSERT_NE(found, graph.nodes.end());
  EXPECT_EQ(found->nodedef, "ND_splitlr_color4");
  EXPECT_EQ(found->links.at("valuel").source_node, "Left");
  EXPECT_EQ(found->links.at("center").source_node, "Center");
  ASSERT_TRUE(found->float4_inputs.contains("valuer"));
  EXPECT_FLOAT_EQ(found->float4_inputs.at("valuer").w, 0.0f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  std::unordered_map<string, MathNode *> math_nodes;
  for (ShaderNode *node : lowered.nodes) {
    if (auto *math = dynamic_cast<MathNode *>(node)) {
      math_nodes[node->name.string()] = math;
    }
  }
  ASSERT_NE(math_nodes["Split.factor"], nullptr);
  ASSERT_NE(math_nodes["Split.Alpha"], nullptr);
  EXPECT_NE(math_nodes["Split.factor"]->input("Value2")->link, nullptr);
  EXPECT_NE(math_nodes["Split.Alpha"]->input("Value1")->link, nullptr);
}

TEST(materialx_usdshade_reader, rejects_bad_split_shape_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/BadSplit"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadSplit/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadSplit/UV"));
  pxr::UsdShadeShader split = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadSplit/Split"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2).Set(pxr::GfVec2f(0.25f, 0.75f));
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  split.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_splitlr_float")));
  split.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  split.CreateInput(pxr::TfToken("valuer"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  split.CreateInput(pxr::TfToken("center"), pxr::SdfValueTypeNames->Float).Set(
      std::numeric_limits<float>::infinity());
  ASSERT_TRUE(split.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  split.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(split.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_EQ(graph.nodes.size(), 1);
}

TEST(materialx_usdshade_reader, rejects_invalid_fractal2d_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/BadFractal"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal/UV"));
  pxr::UsdShadeShader fractal = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal/Fractal"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  fractal.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_fractal2d_color3")));
  fractal.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.75f, 1.0f));
  fractal.CreateInput(pxr::TfToken("octaves"), pxr::SdfValueTypeNames->Int).Set(0);
  fractal.CreateInput(pxr::TfToken("lacunarity"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  fractal.CreateInput(pxr::TfToken("diminish"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  ASSERT_TRUE(fractal.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  fractal.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(fractal.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}


TEST(materialx_usdshade_reader, reads_and_lowers_homogeneous_fractal3d_variants)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/OpenPBR"));
  pxr::UsdShadeShader position = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/Position"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  position.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  position.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.125f, 0.5f, 0.875f));
  position.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);

  const auto shader = [&](const char *name,
                          const char *id,
                          const pxr::SdfValueTypeName &output_type) -> pxr::UsdShadeShader {
    pxr::UsdShadeShader node = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/Fractal3D").AppendChild(pxr::TfToken(name)));
    node.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    node.CreateOutput(pxr::TfToken("out"), output_type);
    EXPECT_TRUE(node.CreateInput(pxr::TfToken("position"), pxr::SdfValueTypeNames->Float3)
                    .ConnectToSource(position.ConnectableAPI(), pxr::TfToken("out")));
    node.CreateInput(pxr::TfToken("octaves"), pxr::SdfValueTypeNames->Int).Set(5);
    node.CreateInput(pxr::TfToken("lacunarity"), pxr::SdfValueTypeNames->Float).Set(2.75f);
    node.CreateInput(pxr::TfToken("diminish"), pxr::SdfValueTypeNames->Float).Set(0.375f);
    return node;
  };

  pxr::UsdShadeShader scalar = shader(
      "Scalar", "ND_fractal3d_float", pxr::SdfValueTypeNames->Float);
  scalar.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  pxr::UsdShadeShader color = shader(
      "Color", "ND_fractal3d_color3", pxr::SdfValueTypeNames->Color3f);
  color.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.75f, 1.0f));
  pxr::UsdShadeShader color_fa = shader(
      "ColorFA", "ND_fractal3d_color3FA", pxr::SdfValueTypeNames->Color3f);
  color_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.6f);
  pxr::UsdShadeShader vector2 = shader(
      "Vector2", "ND_fractal3d_vector2", pxr::SdfValueTypeNames->Float2);
  vector2.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.2f, 0.4f));
  pxr::UsdShadeShader vector2_fa = shader(
      "Vector2FA", "ND_fractal3d_vector2FA", pxr::SdfValueTypeNames->Float2);
  vector2_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.7f);
  pxr::UsdShadeShader vector3 = shader(
      "Vector3", "ND_fractal3d_vector3", pxr::SdfValueTypeNames->Float3);
  vector3.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.3f, 0.5f, 0.7f));
  pxr::UsdShadeShader vector3_fa = shader(
      "Vector3FA", "ND_fractal3d_vector3FA", pxr::SdfValueTypeNames->Float3);
  vector3_fa.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(0.8f);

  pxr::UsdShadeShader extract2 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/Extract2"));
  extract2.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector2")));
  extract2.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  ASSERT_TRUE(extract2.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(vector2.ConnectableAPI(), pxr::TfToken("out")));
  extract2.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract2_fa = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/Extract2FA"));
  extract2_fa.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector2")));
  extract2_fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(0);
  ASSERT_TRUE(extract2_fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(vector2_fa.ConnectableAPI(), pxr::TfToken("out")));
  extract2_fa.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3 = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/Extract3"));
  extract3.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector3")));
  extract3.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(2);
  ASSERT_TRUE(extract3.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(vector3.ConnectableAPI(), pxr::TfToken("out")));
  extract3.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader extract3_fa = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Fractal3D/Extract3FA"));
  extract3_fa.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_vector3")));
  extract3_fa.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(1);
  ASSERT_TRUE(extract3_fa.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(vector3_fa.ConnectableAPI(), pxr::TfToken("out")));
  extract3_fa.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(color_fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract2.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_opacity"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract2_fa.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("coat_weight"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract3.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("coat_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(extract3_fa.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  for (const auto &[name, id, type] : {std::tuple{"Scalar", "ND_fractal3d_float", materialx::Type::Float},
                                      std::tuple{"Color", "ND_fractal3d_color3", materialx::Type::Color3},
                                      std::tuple{"ColorFA", "ND_fractal3d_color3FA", materialx::Type::Color3},
                                      std::tuple{"Vector2", "ND_fractal3d_vector2", materialx::Type::Vector2},
                                      std::tuple{"Vector2FA", "ND_fractal3d_vector2FA", materialx::Type::Vector2},
                                      std::tuple{"Vector3", "ND_fractal3d_vector3", materialx::Type::Vector3},
                                      std::tuple{"Vector3FA", "ND_fractal3d_vector3FA", materialx::Type::Vector3}})
  {
    const materialx::Node &node = find_node(name);
    EXPECT_EQ(node.nodedef, id);
    EXPECT_EQ(node.outputs.at("out"), type);
    EXPECT_EQ(node.links.at("position").type, materialx::Type::Vector3);
    EXPECT_EQ(node.int_inputs.at("octaves"), 5);
    EXPECT_FLOAT_EQ(node.inputs.at("lacunarity"), 2.75f);
    EXPECT_FLOAT_EQ(node.inputs.at("diminish"), 0.375f);
  }

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  int fractal_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (const auto *noise = dynamic_cast<NoiseTextureNode *>(node)) {
      EXPECT_EQ(noise->get_dimensions(), 3);
      EXPECT_EQ(noise->get_type(), NODE_NOISE_FBM);
      EXPECT_FLOAT_EQ(noise->get_detail(), 5.0f);
      EXPECT_FLOAT_EQ(noise->get_lacunarity(), 2.75f);
      EXPECT_FLOAT_EQ(noise->get_roughness(), 0.375f);
      ++fractal_count;
    }
  }
  EXPECT_EQ(fractal_count, 7);
}

TEST(materialx_usdshade_reader, rejects_invalid_fractal3d_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/BadFractal3D"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal3D/OpenPBR"));
  pxr::UsdShadeShader position = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal3D/Position"));
  pxr::UsdShadeShader fractal = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadFractal3D/Fractal"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  position.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector3")));
  position.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.125f, 0.5f, 0.875f));
  position.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  fractal.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_fractal3d_color3")));
  fractal.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.5f, 0.75f, 1.0f));
  fractal.CreateInput(pxr::TfToken("octaves"), pxr::SdfValueTypeNames->Int).Set(0);
  fractal.CreateInput(pxr::TfToken("lacunarity"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  fractal.CreateInput(pxr::TfToken("diminish"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  ASSERT_TRUE(fractal.CreateInput(pxr::TfToken("position"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(position.ConnectableAPI(), pxr::TfToken("out")));
  fractal.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(fractal.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, reads_and_lowers_direct_color_procedural_variants)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/DirectColorProcedurals"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DirectColorProcedurals/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DirectColorProcedurals/UV"));
  pxr::UsdShadeShader noise = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DirectColorProcedurals/Noise"));
  pxr::UsdShadeShader checker = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DirectColorProcedurals/Checker"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);

  noise.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_noise2d_color3FA")));
  noise.CreateInput(pxr::TfToken("amplitude"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  noise.CreateInput(pxr::TfToken("pivot"), pxr::SdfValueTypeNames->Float).Set(0.0f);
  ASSERT_TRUE(noise.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  noise.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);

  checker.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_checkerboard_color3")));
  checker.CreateInput(pxr::TfToken("color1"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  checker.CreateInput(pxr::TfToken("color2"), pxr::SdfValueTypeNames->Color3f).Set(
      pxr::GfVec3f(0.8f, 0.7f, 0.6f));
  checker.CreateInput(pxr::TfToken("uvtiling"), pxr::SdfValueTypeNames->Float2).Set(
      pxr::GfVec2f(4.0f, 4.0f));
  checker.CreateInput(pxr::TfToken("uvoffset"), pxr::SdfValueTypeNames->Float2).Set(
      pxr::GfVec2f(0.0f, 0.0f));
  ASSERT_TRUE(checker.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  checker.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);

  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(checker.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(noise.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  const auto find_node = [&](const char *name) -> const materialx::Node & {
    const auto it = std::find_if(graph.nodes.begin(), graph.nodes.end(), [&](const materialx::Node &node) {
      return node.name == name;
    });
    EXPECT_NE(it, graph.nodes.end());
    return *it;
  };
  const materialx::Node &noise_node = find_node("Noise");
  EXPECT_EQ(noise_node.nodedef, "ND_noise2d_color3FA");
  EXPECT_FLOAT_EQ(noise_node.inputs.at("amplitude"), 1.0f);
  EXPECT_FLOAT_EQ(noise_node.inputs.at("pivot"), 0.0f);
  const materialx::Node &checker_node = find_node("Checker");
  EXPECT_EQ(checker_node.nodedef, "ND_checkerboard_color3");
  EXPECT_EQ(checker_node.color3_inputs.at("color1"), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(checker_node.links.at("texcoord").type, materialx::Type::Vector2);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
}

TEST(materialx_usdshade_reader, reads_omitted_constant_color4_as_installed_zero_default)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/DefaultColor4"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, material.GetPath().AppendChild(pxr::TfToken("OpenPBR")));
  pxr::UsdShadeShader color4 = pxr::UsdShadeShader::Define(
      stage, material.GetPath().AppendChild(pxr::TfToken("DefaultColor4")));
  pxr::UsdShadeShader convert = pxr::UsdShadeShader::Define(
      stage, material.GetPath().AppendChild(pxr::TfToken("RGB")));
  pxr::UsdShadeShader alpha = pxr::UsdShadeShader::Define(
      stage, material.GetPath().AppendChild(pxr::TfToken("Alpha")));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  color4.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color4")));
  color4.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(color4.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(color4.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  ASSERT_EQ(graph.nodes.size(), 4);
  EXPECT_EQ(graph.nodes[0].nodedef, "ND_constant_color4");
  ASSERT_EQ(graph.nodes[0].float4_inputs.size(), 1);
  EXPECT_EQ(graph.nodes[0].float4_inputs.at("value"), make_float4(0.0f, 0.0f, 0.0f, 0.0f));

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  ValueNode *alpha_value = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    alpha_value = node->name == "DefaultColor4.Alpha" ? dynamic_cast<ValueNode *>(node) :
                                                        alpha_value;
  }
  ASSERT_NE(alpha_value, nullptr);
  EXPECT_FLOAT_EQ(alpha_value->get_value(), 0.0f);
}

TEST(materialx_usdshade_reader, reads_and_lowers_color4_lr_tb_ramps)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4Ramps"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader uv = shader("UV", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.75f));
  pxr::UsdShadeShader previous;
  pxr::UsdShadeShader left_right;
  for (const char *id : {"ND_ramplr_color4", "ND_ramptb_color4"}) {
    pxr::UsdShadeShader ramp = shader(id, id, pxr::SdfValueTypeNames->Color4f);
    ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
    if (string(id) == "ND_ramplr_color4") {
      ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f));
      ramp.CreateInput(pxr::TfToken("valuer"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(0.5f, 0.6f, 0.7f, 0.8f));
    }
    else {
      ramp.CreateInput(pxr::TfToken("valuet"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.45f));
      ramp.CreateInput(pxr::TfToken("valueb"), pxr::SdfValueTypeNames->Color4f)
          .Set(pxr::GfVec4f(0.5f, 0.6f, 0.7f, 0.85f));
    }
    if (string(id) == "ND_ramplr_color4") {
      left_right = ramp;
    }
    previous = ramp;
  }
  pxr::UsdShadeShader alpha = shader("Alpha", "ND_extract_color4", pxr::SdfValueTypeNames->Float);
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(left_right.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader convert = shader(
      "RGB", "ND_convert_color4_color3", pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(previous.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  EXPECT_NE(
      std::find_if(graph.nodes.begin(),
                   graph.nodes.end(),
                   [](const materialx::Node &node) { return node.nodedef == "ND_ramplr_color4"; }),
      graph.nodes.end());
  EXPECT_NE(
      std::find_if(graph.nodes.begin(),
                   graph.nodes.end(),
                   [](const materialx::Node &node) { return node.nodedef == "ND_ramptb_color4"; }),
      graph.nodes.end());

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(graph, &lowered));
  bool found_alpha = false;
  for (ShaderNode *node : lowered.nodes) {
    found_alpha |= node->name == "ND_ramptb_color4.Alpha" && dynamic_cast<MathNode *>(node);
  }
  EXPECT_TRUE(found_alpha);
}

TEST(materialx_usdshade_reader, reads_color4_ramps_with_installed_zero_color_defaults)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/DefaultColor4Ramp"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, material.GetPath().AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader surface = shader(
      "OpenPBR", "ND_open_pbr_surface_surfaceshader", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader uv = shader("UV", "ND_constant_vector2", pxr::SdfValueTypeNames->Float2);
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.75f));
  pxr::UsdShadeShader ramp = shader("Ramp", "ND_ramplr_color4", pxr::SdfValueTypeNames->Color4f);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  pxr::UsdShadeShader convert = shader(
      "RGB", "ND_convert_color4_color3", pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  auto ramp_node = std::find_if(
      graph.nodes.begin(), graph.nodes.end(), [](const materialx::Node &node) {
        return node.nodedef == "ND_ramplr_color4";
      });
  ASSERT_NE(ramp_node, graph.nodes.end());
  EXPECT_EQ(ramp_node->float4_inputs.at("valuel"), make_float4(0.0f));
  EXPECT_EQ(ramp_node->float4_inputs.at("valuer"), make_float4(0.0f));
}

TEST(materialx_usdshade_reader, rejects_color4_ramp_bad_shape_and_linked_values_without_mutation)
{
  const auto expect_rejected = [](const int rejection) {
    const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage);
    const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
        stage, pxr::SdfPath("/Looks/BadColor4Ramp" + std::to_string(rejection)));
    const auto shader = [&](const char *name) {
      return pxr::UsdShadeShader::Define(stage,
                                         material.GetPath().AppendChild(pxr::TfToken(name)));
    };
    pxr::UsdShadeShader surface = shader("OpenPBR");
    pxr::UsdShadeShader uv = shader("UV");
    pxr::UsdShadeShader ramp = shader("Ramp");
    pxr::UsdShadeShader convert = shader("RGB");
    pxr::UsdShadeShader linked = shader("LinkedValue");
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
    uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
        .Set(pxr::GfVec2f(0.25f, 0.75f));
    uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
    ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_color4")));
    ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                    .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
    if (rejection == 0) {
      ramp.CreateInput(pxr::TfToken("unexpected"), pxr::SdfValueTypeNames->Float).Set(1.0f);
    }
    else if (rejection == 1) {
      ramp.CreateOutput(pxr::TfToken("extra"), pxr::SdfValueTypeNames->Color4f);
    }
    else {
      linked.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
      linked.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
      linked.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
      ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Color4f)
                      .ConnectToSource(linked.ConnectableAPI(), pxr::TfToken("out")));
    }
    ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
    convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
    convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                    .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                    .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
    const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
    ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                      pxr::TfToken("out")));

    materialx::Graph graph;
    graph.nodes.push_back({"sentinel", "unsupported"});
    string error;
    EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
    EXPECT_NE(error.find("ND_ramplr_color4"), string::npos) << error;
    ASSERT_EQ(graph.nodes.size(), 1);
    EXPECT_EQ(graph.nodes[0].name, "sentinel");
  };

  expect_rejected(0);
  expect_rejected(1);
  expect_rejected(2);
}

TEST(materialx_usdshade_reader, reads_color4_ramps_with_defaults_uv0_and_connectable_endpoints)
{
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4RampDefaults"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };

  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader ramp = shader("Ramp");
  pxr::UsdShadeShader convert = shader("RGB");
  pxr::UsdShadeShader alpha = shader("Alpha");
  pxr::UsdShadeShader uv = shader("UV0");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_color4")));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("UV0");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  convert.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_color4_color3")));
  convert.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(convert.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(convert.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  const auto ramp_node = std::find_if(
      source.nodes.begin(), source.nodes.end(), [](const materialx::Node &node) {
        return node.name == "Ramp";
      });
  ASSERT_NE(ramp_node, source.nodes.end());
  EXPECT_EQ(ramp_node->float4_inputs.at("valuel"), zero_float4());
  EXPECT_EQ(ramp_node->float4_inputs.at("valuer"), zero_float4());
  ASSERT_EQ(ramp_node->links.at("texcoord").source_node, "UV0");

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  UVMapNode *uv_map = nullptr;
  MixNode *mix = nullptr;
  MathNode *alpha_delta = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    uv_map = node->name == "UV0" ? dynamic_cast<UVMapNode *>(node) : uv_map;
    mix = node->name == "Ramp" ? dynamic_cast<MixNode *>(node) : mix;
    alpha_delta = node->name == "Ramp.Alpha.delta" ? dynamic_cast<MathNode *>(node) : alpha_delta;
  }
  ASSERT_NE(uv_map, nullptr);
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(alpha_delta, nullptr);
  EXPECT_EQ(uv_map->get_attribute(), ustring("UV0"));
  EXPECT_EQ(mix->get_color1(), zero_float3());
  EXPECT_EQ(mix->get_color2(), zero_float3());
  EXPECT_FLOAT_EQ(alpha_delta->get_value1(), 0.0f);
  EXPECT_FLOAT_EQ(alpha_delta->get_value2(), 0.0f);
}

TEST(materialx_usdshade_reader, accepts_color4_ramp_inputs_from_generic_producers)
{
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Color4RampGeneric"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };

  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader left = shader("Left");
  pxr::UsdShadeShader right = shader("Right");
  pxr::UsdShadeShader ramp = shader("Ramp");
  pxr::UsdShadeShader alpha = shader("Alpha");
  pxr::UsdShadeShader uv = shader("UV");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  left.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_color4")));
  left.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f).Set(pxr::GfVec4f(0.1f));
  left.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f).Set(pxr::GfVec4f(0.2f));
  left.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  right.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_color4")));
  right.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(0.3f, 0.4f, 0.5f, 0.6f));
  right.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color4f).Set(pxr::GfVec4f(0.5f));
  right.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_color4")));
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(left.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("valuer"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(right.ConnectableAPI(), pxr::TfToken("out")));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.75f));
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  const auto ramp_node = std::find_if(
      source.nodes.begin(), source.nodes.end(), [](const materialx::Node &node) {
        return node.name == "Ramp";
      });
  ASSERT_NE(ramp_node, source.nodes.end());
  EXPECT_EQ(ramp_node->links.at("valuel").source_node, "Left");
  EXPECT_EQ(ramp_node->links.at("valuer").source_node, "Right");
  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  MathNode *left_alpha = nullptr;
  MathNode *right_alpha = nullptr;
  MathNode *alpha_delta = nullptr;
  MathNode *alpha_sum = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    left_alpha = node->name == "Left.Alpha" ? dynamic_cast<MathNode *>(node) : left_alpha;
    right_alpha = node->name == "Right.Alpha" ? dynamic_cast<MathNode *>(node) : right_alpha;
    alpha_delta = node->name == "Ramp.Alpha.delta" ? dynamic_cast<MathNode *>(node) : alpha_delta;
    alpha_sum = node->name == "Ramp.Alpha" ? dynamic_cast<MathNode *>(node) : alpha_sum;
  }
  ASSERT_NE(left_alpha, nullptr);
  ASSERT_NE(right_alpha, nullptr);
  ASSERT_NE(alpha_delta, nullptr);
  ASSERT_NE(alpha_sum, nullptr);
  EXPECT_EQ(alpha_delta->input("Value1")->link, right_alpha->output("Value"));
  EXPECT_EQ(alpha_delta->input("Value2")->link, left_alpha->output("Value"));
  EXPECT_EQ(alpha_sum->input("Value1")->link, left_alpha->output("Value"));
}

TEST(materialx_usdshade_reader, rejects_invalid_color4_ramp_inputs_without_mutation)
{
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/BadColor4Ramp"));
  const auto shader = [&](const char *name) {
    return pxr::UsdShadeShader::Define(stage, material.GetPath().AppendChild(pxr::TfToken(name)));
  };
  pxr::UsdShadeShader surface = shader("OpenPBR");
  pxr::UsdShadeShader ramp = shader("Ramp");
  pxr::UsdShadeShader alpha = shader("Alpha");
  pxr::UsdShadeShader uv = shader("UV");
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ramp.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_ramplr_color4")));
  ramp.CreateInput(pxr::TfToken("valuel"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0.0f));
  ramp.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector2")));
  uv.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float2)
      .Set(pxr::GfVec2f(0.25f, 0.75f));
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(ramp.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  alpha.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_extract_color4")));
  alpha.CreateInput(pxr::TfToken("index"), pxr::SdfValueTypeNames->Int).Set(3);
  alpha.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(alpha.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Color4f)
                  .ConnectToSource(ramp.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(alpha.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                                    pxr::TfToken("out")));

  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("valuel"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
}

/* ------------------------------------------------------------------------
 * Task 2: generic admission and typed output selection (Phase 1).
 *
 * `resolve_manifest_outputs` replaces the fixed per-terminal-input,
 * per-NodeDef admission gate with manifest-bound output-port descriptors
 * for float/color3/vector2/vector3, dispatched through one typed resolver.
 * ------------------------------------------------------------------------ */

namespace {

/** A canonical OpenPBR material with a roughness math chain and one
 *  standalone node that is never wired to any material terminal. */
struct ManifestFixture {
  pxr::UsdStageRefPtr stage;
  pxr::UsdShadeMaterial material;
  string multiply_path;
  string standalone_path;
  /** Task 4: four-component observation canaries, reachable from the same
   *  authenticated surface terminal as the rest of the fixture. */
  string color4_path;
  string vector4_path;
  /** Task 5: boolean/integer exact-domain observation canaries. */
  string boolean_path;
  string integer_path;
  /** Task 6: matrix boundary canaries. */
  string matrix33_path;
  string matrix44_path;
};

ManifestFixture build_manifest_fixture(const char *context_name = "mtlx")
{
  ManifestFixture fixture;
  fixture.stage = pxr::UsdStage::CreateInMemory();
  fixture.material = pxr::UsdShadeMaterial::Define(fixture.stage,
                                                    pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/OpenPBR"));
  pxr::UsdShadeShader multiply = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessMultiply"));
  pxr::UsdShadeShader first = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessFirst"));
  pxr::UsdShadeShader second = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/RoughnessSecond"));
  pxr::UsdShadeShader standalone = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/Standalone"));
  pxr::UsdShadeShader color4 = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/Color4Canary"));
  pxr::UsdShadeShader vector4 = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/Vector4Canary"));
  pxr::UsdShadeShader boolean = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/BooleanCanary"));
  pxr::UsdShadeShader integer = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/IntegerCanary"));
  pxr::UsdShadeShader matrix33 = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/Matrix33Canary"));
  pxr::UsdShadeShader matrix44 = pxr::UsdShadeShader::Define(
      fixture.stage, pxr::SdfPath("/Looks/TestMaterial/Matrix44Canary"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  multiply.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_float")));
  multiply.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  first.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  first.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.8f);
  first.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  second.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  second.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.9f);
  second.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  standalone.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  standalone.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.42f);
  standalone.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  color4.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color4")));
  color4.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color4f)
      .Set(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.7f));
  color4.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color4f);
  vector4.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector4")));
  vector4.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float4)
      .Set(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.5f));
  vector4.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float4);
  boolean.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_boolean")));
  boolean.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Bool).Set(true);
  boolean.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Bool);
  integer.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_integer")));
  integer.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Int).Set(7);
  integer.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Int);
  matrix33.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_matrix33")));
  matrix33.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Matrix3d)
      .Set(pxr::GfMatrix3d(1, 2, 3, 4, 5, 6, 7, 8, 9));
  matrix33.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Matrix3d);
  matrix44.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_matrix44")));
  /* Column-vector/affine convention (matching Cycles' native Transform:
   * translation in the 4th column of each row, last row exactly
   * {0, 0, 0, 1}) -- row-major GfMatrix4d(m00, m01, m02, m03, ...). */
  matrix44.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Matrix4d)
      .Set(pxr::GfMatrix4d(1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1));
  matrix44.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Matrix4d);

  if (multiply.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")) &&
      multiply.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")) &&
      surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
          .ConnectToSource(multiply.ConnectableAPI(), pxr::TfToken("out")) &&
      /* Task 4: wire the Color4/Vector4 canaries as reachable (but
       * otherwise inert) surface inputs -- the manifest resolver's
       * reachability walk is structural, not type-aware, so any connected
       * input works, matching the existing `specular_roughness` pattern. */
      surface.CreateInput(pxr::TfToken("unused_color4"), pxr::SdfValueTypeNames->Color4f)
          .ConnectToSource(color4.ConnectableAPI(), pxr::TfToken("out")) &&
      surface.CreateInput(pxr::TfToken("unused_vector4"), pxr::SdfValueTypeNames->Float4)
          .ConnectToSource(vector4.ConnectableAPI(), pxr::TfToken("out")) &&
      /* Task 5: same reachable-but-inert wiring for the boolean/integer
       * canaries. */
      surface.CreateInput(pxr::TfToken("unused_boolean"), pxr::SdfValueTypeNames->Bool)
          .ConnectToSource(boolean.ConnectableAPI(), pxr::TfToken("out")) &&
      surface.CreateInput(pxr::TfToken("unused_integer"), pxr::SdfValueTypeNames->Int)
          .ConnectToSource(integer.ConnectableAPI(), pxr::TfToken("out")) &&
      /* Task 6: same reachable-but-inert wiring for the matrix canaries. */
      surface.CreateInput(pxr::TfToken("unused_matrix33"), pxr::SdfValueTypeNames->Matrix3d)
          .ConnectToSource(matrix33.ConnectableAPI(), pxr::TfToken("out")) &&
      surface.CreateInput(pxr::TfToken("unused_matrix44"), pxr::SdfValueTypeNames->Matrix4d)
          .ConnectToSource(matrix44.ConnectableAPI(), pxr::TfToken("out")))
  {
    if (context_name && *context_name) {
      fixture.material.CreateSurfaceOutput(pxr::TfToken(context_name))
          .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out"));
    }
    else {
      fixture.material.CreateSurfaceOutput().ConnectToSource(surface.ConnectableAPI(),
                                                              pxr::TfToken("out"));
    }
  }

  fixture.multiply_path = "/Looks/TestMaterial/RoughnessMultiply";
  fixture.standalone_path = "/Looks/TestMaterial/Standalone";
  fixture.color4_path = "/Looks/TestMaterial/Color4Canary";
  fixture.vector4_path = "/Looks/TestMaterial/Vector4Canary";
  fixture.boolean_path = "/Looks/TestMaterial/BooleanCanary";
  fixture.integer_path = "/Looks/TestMaterial/IntegerCanary";
  fixture.matrix33_path = "/Looks/TestMaterial/Matrix33Canary";
  fixture.matrix44_path = "/Looks/TestMaterial/Matrix44Canary";
  return fixture;
}

}  // namespace

TEST(materialx_usdshade_reader, resolves_manifest_bound_output_for_unlisted_reachable_node)
{
  const ManifestFixture fixture = build_manifest_fixture();

  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;

  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Float);

  bool found_multiply = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_multiply_float") {
      found_multiply = true;
      EXPECT_FLOAT_EQ(node.inputs.at("in1"), 0.8f);
      EXPECT_FLOAT_EQ(node.inputs.at("in2"), 0.9f);
    }
  }
  EXPECT_TRUE(found_multiply);
}

TEST(materialx_usdshade_reader, resolves_ordered_multi_output_separate3_into_one_receipt)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/OpenPBR"));
  pxr::UsdShadeShader x = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/X"));
  pxr::UsdShadeShader y = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/Y"));
  pxr::UsdShadeShader z = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/SeparateVector3/Z"));
  pxr::UsdShadeShader combine = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/Combine"));
  pxr::UsdShadeShader separate = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/SeparateVector3/Separate"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  for (auto [shader, value] : {std::pair{x, 0.2f}, std::pair{y, 0.5f}, std::pair{z, 0.8f}}) {
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
    shader.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(value);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  }
  combine.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_combine3_vector3")));
  for (const auto &[name, shader] : {std::pair{"in1", x}, std::pair{"in2", y}, std::pair{"in3", z}}) {
    ASSERT_TRUE(combine.CreateInput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float)
                    .ConnectToSource(shader.ConnectableAPI(), pxr::TfToken("out")));
  }
  combine.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float3);
  separate.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_separate3_vector3")));
  ASSERT_TRUE(separate.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(combine.ConnectableAPI(), pxr::TfToken("out")));
  for (const char *name : {"outx", "outy", "outz"}) {
    separate.CreateOutput(pxr::TfToken(name), pxr::SdfValueTypeNames->Float);
  }
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("specular_roughness"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(separate.ConnectableAPI(), pxr::TfToken("outy")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const string separate_path = "/Looks/SeparateVector3/Separate";
  const vector<materialx::SelectedOutput> selected = {
      {separate_path, "ND_separate3_vector3", "outx", materialx::Type::Float},
      {separate_path, "ND_separate3_vector3", "outy", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 2);
  EXPECT_EQ(results[0].type, materialx::Type::Float);
  EXPECT_EQ(results[1].type, materialx::Type::Float);
}

TEST(materialx_usdshade_reader,
    clears_complete_manifest_receipt_when_any_selected_output_is_invalid)
{
  const ManifestFixture fixture = build_manifest_fixture();

  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
      {fixture.multiply_path, "ND_multiply_float", "does_not_exist", materialx::Type::Float},
  };
  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  vector<materialx::Link> results;
  results.push_back({"sentinel", "out", materialx::Type::Float});
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].source_node, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_manifest_output_with_wrong_node_path_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/TestMaterial/DoesNotExist", "ND_multiply_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("path"), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_output_with_wrong_nodedef_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_add_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("NodeDef"), string::npos) << error;
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_output_with_wrong_output_name_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "not_an_output", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("not_an_output"), string::npos) << error;
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_output_with_wrong_type_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Color3},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("type"), string::npos) << error;
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_unreachable_manifest_output_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.standalone_path, "ND_constant_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("reachable"), string::npos) << error;
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_missing_manifest_render_context_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(materialx::resolve_manifest_outputs(
      fixture.material, "preview", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_ambiguous_manifest_render_context_without_mutating_graph)
{
  /* The material authors a named 'mtlx' surface terminal; requesting the
   * universal context without saying so explicitly must fail closed rather
   * than silently falling back. */
  const ManifestFixture fixture = build_manifest_fixture("mtlx");
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(fixture.material, "", selected, &graph, &results, &error));
  EXPECT_NE(error.find("ambiguous"), string::npos) << error;
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, resolves_manifest_output_with_explicit_universal_render_context)
{
  const ManifestFixture fixture = build_manifest_fixture(nullptr);
  const vector<materialx::SelectedOutput> selected = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(
      materialx::resolve_manifest_outputs(fixture.material, "", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
}

/* Task 4: four-component observation -- Color4/Vector4 exact canaries,
 * alpha/W preservation, wrong tag, stale output, nonfinite context, and
 * missing sink. */

TEST(materialx_usdshade_reader, resolves_manifest_bound_color4_output_preserving_alpha)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.color4_path, "ND_constant_color4", "out", materialx::Type::Color4},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Color4);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_color4") {
      found = true;
      const float4 value = node.float4_inputs.at("value");
      EXPECT_FLOAT_EQ(value.x, 0.1f);
      EXPECT_FLOAT_EQ(value.y, 0.2f);
      EXPECT_FLOAT_EQ(value.z, 0.3f);
      /* Alpha preservation: the fourth component is not dropped, truncated
       * to RGB, or defaulted. */
      EXPECT_FLOAT_EQ(value.w, 0.7f);
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, resolves_manifest_bound_vector4_output_preserving_w)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.vector4_path, "ND_constant_vector4", "out", materialx::Type::Vector4},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Vector4);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_vector4") {
      found = true;
      const float4 value = node.vector4_inputs.at("value");
      EXPECT_FLOAT_EQ(value.x, 1.0f);
      EXPECT_FLOAT_EQ(value.y, 2.0f);
      EXPECT_FLOAT_EQ(value.z, 3.0f);
      /* W preservation: the fourth component is not dropped, truncated to
       * Vector3, or defaulted. */
      EXPECT_FLOAT_EQ(value.w, 4.5f);
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, rejects_manifest_color4_output_declared_with_wrong_tag_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  /* Wrong tag: the real node at color4_path is Color4f-typed, but the
   * manifest declares it as Vector4 (Float4). Type authentication must
   * fail closed, not coerce. */
  const vector<materialx::SelectedOutput> selected = {
      {fixture.color4_path, "ND_constant_color4", "out", materialx::Type::Vector4},
  };
  materialx::Graph graph;
  graph.has_displacement = true;
  graph.displacement.value = 42.0f;
  vector<materialx::Link> results;
  results.push_back({"sentinel", "out", materialx::Type::Float});
  string error;
  EXPECT_FALSE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  /* Sentinel-value check: caller-visible graph/results are untouched on
   * failure, not partially written. */
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_FLOAT_EQ(graph.displacement.value, 42.0f);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].source_node, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_manifest_vector4_output_for_unsupported_operation_as_missing_sink)
{
  /* "Missing sink": a real, reachable, correctly-typed Vector4 node whose
   * NodeDef has no native Vector4 lowerer implemented in this pass (only
   * ND_constant_vector4 is). Must fail closed with a named boundary, not
   * silently coerce or crash. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedVector4"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedVector4/OpenPBR"));
  pxr::UsdShadeShader add = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedVector4/Add"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  add.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vector4")));
  add.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float4);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("unused_vector4"), pxr::SdfValueTypeNames->Float4)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/UnsupportedVector4/Add", "ND_add_vector4", "out", materialx::Type::Vector4},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("ND_add_vector4"), string::npos);
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_vector4_output_with_nonfinite_value_without_mutating_graph)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/NonfiniteVector4"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NonfiniteVector4/OpenPBR"));
  pxr::UsdShadeShader constant = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NonfiniteVector4/Constant"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  constant.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_vector4")));
  constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float4)
      .Set(pxr::GfVec4f(0.0f, 0.0f, 0.0f, std::numeric_limits<float>::infinity()));
  constant.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float4);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("unused_vector4"), pxr::SdfValueTypeNames->Float4)
                  .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/NonfiniteVector4/Constant", "ND_constant_vector4", "out", materialx::Type::Vector4},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

/* Task 5: boolean/integer exact-domain observation -- RED exact-domain
 * canaries and invalid range/tag cases at the manifest admission layer. */

TEST(materialx_usdshade_reader, resolves_manifest_bound_boolean_output_exactly)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.boolean_path, "ND_constant_boolean", "out", materialx::Type::Boolean},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Boolean);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_boolean") {
      found = true;
      /* No float coercion: the value is authenticated and stored as an
       * exact int_inputs domain value {0, 1}, not a float approximation. */
      EXPECT_EQ(node.int_inputs.at("value"), 1);
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, resolves_manifest_bound_integer_output_exactly)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.integer_path, "ND_constant_integer", "out", materialx::Type::Integer},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Integer);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_integer") {
      found = true;
      EXPECT_EQ(node.int_inputs.at("value"), 7);
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, rejects_manifest_boolean_output_declared_with_wrong_tag_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  /* Wrong tag: the real node at boolean_path is Bool-typed, but the
   * manifest declares it as Integer. */
  const vector<materialx::SelectedOutput> selected = {
      {fixture.boolean_path, "ND_constant_boolean", "out", materialx::Type::Integer},
  };
  materialx::Graph graph;
  graph.has_displacement = true;
  graph.displacement.value = 42.0f;
  vector<materialx::Link> results;
  results.push_back({"sentinel", "out", materialx::Type::Float});
  string error;
  EXPECT_FALSE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_FLOAT_EQ(graph.displacement.value, 42.0f);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].source_node, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_manifest_integer_output_for_unsupported_operation_as_missing_sink)
{
  /* Missing sink: a real, reachable, correctly-Int-typed ND_add_integer
   * node has no native Integer lowerer implemented in this pass (only
   * ND_constant_integer is). */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedInteger"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedInteger/OpenPBR"));
  pxr::UsdShadeShader add = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedInteger/Add"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  add.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_integer")));
  add.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Int);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("unused_integer"), pxr::SdfValueTypeNames->Int)
                  .ConnectToSource(add.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/UnsupportedInteger/Add", "ND_add_integer", "out", materialx::Type::Integer},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("ND_add_integer"), string::npos);
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_boolean_output_out_of_domain_without_mutating_graph)
{
  /* Invalid range: USD's Bool type cannot itself carry an out-of-domain
   * value, so this exercises the domain check at the IR/validate() layer
   * instead -- a manifest selection whose underlying node authenticates
   * fine at the USD layer but was hand-constructed (bypassing the reader)
   * with an out-of-domain int_inputs value must fail lower()/validate(),
   * not silently accept it. This is exercised directly against validate()
   * in materialx_graph_test.cpp's
   * rejects_constant_boolean_bad_shape_range_and_tag_atomically; this
   * manifest-layer test instead confirms the reader itself never
   * constructs an out-of-domain node from a genuine USD Bool literal. */
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.boolean_path, "ND_constant_boolean", "out", materialx::Type::Boolean},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_boolean") {
      const int value = node.int_inputs.at("value");
      EXPECT_TRUE(value == 0 || value == 1);
    }
  }
}

/* Task 6: matrix boundary -- RED 9/16-component preservation tests and
 * the non-affine-rejection boundary, at the manifest admission layer. */

TEST(materialx_usdshade_reader, resolves_manifest_bound_matrix33_output_preserving_all_nine_components)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.matrix33_path, "ND_constant_matrix33", "out", materialx::Type::Matrix33},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Matrix33);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_matrix33") {
      found = true;
      const std::array<float, 9> value = node.matrix33_inputs.at("value");
      for (int index = 0; index < 9; index++) {
        EXPECT_FLOAT_EQ(value[size_t(index)], float(index + 1));
      }
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, resolves_manifest_bound_matrix44_output_preserving_all_components)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.matrix44_path, "ND_constant_matrix44", "out", materialx::Type::Matrix44},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Matrix44);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_constant_matrix44") {
      found = true;
      const std::array<float, 16> value = node.matrix44_inputs.at("value");
      const std::array<float, 16> expected = {
          1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1};
      for (int index = 0; index < 16; index++) {
        EXPECT_FLOAT_EQ(value[size_t(index)], expected[size_t(index)]);
      }
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, rejects_manifest_matrix33_output_declared_with_wrong_tag_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();
  const vector<materialx::SelectedOutput> selected = {
      {fixture.matrix33_path, "ND_constant_matrix33", "out", materialx::Type::Matrix44},
  };
  materialx::Graph graph;
  graph.has_displacement = true;
  graph.displacement.value = 42.0f;
  vector<materialx::Link> results;
  results.push_back({"sentinel", "out", materialx::Type::Float});
  string error;
  EXPECT_FALSE(materialx::resolve_manifest_outputs(
      fixture.material, "mtlx", selected, &graph, &results, &error));
  EXPECT_FALSE(error.empty());
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_FLOAT_EQ(graph.displacement.value, 42.0f);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].source_node, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_manifest_nonaffine_matrix44_literal_without_mutating_graph)
{
  /* The manifest-admission-layer version of the honest-boundary
   * assertion: a genuine, reachable, correctly-typed non-affine
   * Matrix4d literal (last row not {0, 0, 0, 1}) must fail closed at
   * the USD-literal-read step, before it ever becomes an IR node. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/NonaffineMatrix44"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NonaffineMatrix44/OpenPBR"));
  pxr::UsdShadeShader constant = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/NonaffineMatrix44/Constant"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  constant.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_matrix44")));
  constant.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Matrix4d)
      .Set(pxr::GfMatrix4d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0.5, 1));
  constant.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Matrix4d);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("unused_matrix44"), pxr::SdfValueTypeNames->Matrix4d)
                  .ConnectToSource(constant.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/NonaffineMatrix44/Constant", "ND_constant_matrix44", "out", materialx::Type::Matrix44},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("affine"), string::npos);
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_usdshade_reader, rejects_manifest_matrix33_output_for_unsupported_operation_as_missing_sink)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedMatrix33"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedMatrix33/OpenPBR"));
  pxr::UsdShadeShader transpose = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnsupportedMatrix33/Transpose"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  transpose.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_transpose_matrix33")));
  transpose.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Matrix3d);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("unused_matrix33"), pxr::SdfValueTypeNames->Matrix3d)
                  .ConnectToSource(transpose.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/UnsupportedMatrix33/Transpose", "ND_transpose_matrix33", "out", materialx::Type::Matrix33},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error));
  EXPECT_NE(error.find("ND_transpose_matrix33"), string::npos);
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_authority_pipeline, resolves_manifest_authority_outputs_into_shared_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();

  string usda;
  ASSERT_TRUE(fixture.stage->GetRootLayer()->ExportToString(&usda));
  materialx::Authority authority;
  authority.document_uuid = "9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.digest = materialx::usda_sha256_digest(usda);
  authority.usda_text_name = ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.material_path = "/Looks/TestMaterial";
  authority.usda = usda;
  authority.render_context = "mtlx";
  authority.selected_outputs = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Float);
}

TEST(materialx_authority_pipeline, rejects_tampered_digest_for_manifest_outputs_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();

  string usda;
  ASSERT_TRUE(fixture.stage->GetRootLayer()->ExportToString(&usda));
  materialx::Authority authority;
  authority.document_uuid = "9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.digest = materialx::usda_sha256_digest(usda);
  authority.usda_text_name = ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.material_path = "/Looks/TestMaterial";
  authority.usda = usda + "# tampered after digest\n";
  authority.render_context = "mtlx";
  authority.selected_outputs = {
      {fixture.multiply_path, "ND_multiply_float", "out", materialx::Type::Float},
  };

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error));
  EXPECT_NE(error.find("contract"), string::npos) << error;
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_authority_pipeline,
    rejects_manifest_authority_nodedef_mismatch_without_mutating_graph)
{
  const ManifestFixture fixture = build_manifest_fixture();

  string usda;
  ASSERT_TRUE(fixture.stage->GetRootLayer()->ExportToString(&usda));
  materialx::Authority authority;
  authority.document_uuid = "9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.digest = materialx::usda_sha256_digest(usda);
  authority.usda_text_name = ".materialx_usdshade_9c37e82e-63a1-470d-a704-e0daf9cfd814";
  authority.material_path = "/Looks/TestMaterial";
  authority.usda = usda;
  authority.render_context = "mtlx";
  authority.selected_outputs = {
      {fixture.multiply_path, "ND_add_float", "out", materialx::Type::Float},
  };

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error));
  EXPECT_NE(error.find("NodeDef"), string::npos) << error;
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

/* ------------------------------------------------------------------------ */
/* Task 7: fixture-bound cases (texture bytes, UV/primvar geometry).        */
/* ------------------------------------------------------------------------ */

TEST(materialx_usdshade_reader, resolves_manifest_bound_image_output_reachable_through_real_texture_asset)
{
  /* Texture bytes: proves the Phase 1 generic admission layer
   * (resolve_manifest_outputs) already reaches a fixture-bound
   * ND_image_color3 node -- Task 2's admission generalization was never
   * exercised against an image/texture node through the manifest path
   * before this task. No production code change was needed for this
   * case; this is real, new coverage proving existing capability. */
  const TemporaryImage image_asset;
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/OpenPBR"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/BaseColorImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/UV"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  ASSERT_TRUE(image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
                  .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/FixtureBound/BaseColorImage", "ND_image_color3", "out", materialx::Type::Color3},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Color3);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_image_color3") {
      found = true;
      ASSERT_TRUE(node.asset_inputs.contains("file"));
      EXPECT_EQ(node.asset_inputs.at("file"), image_asset.path());
    }
  }
  EXPECT_TRUE(found);
}

TEST(materialx_usdshade_reader, resolves_manifest_bound_geompropvalue_output_for_uv_primvar)
{
  /* UV/primvar geometry: same proof for ND_geompropvalue_vector2 (the
   * canonical "st"/UV primvar reader), through the manifest path. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/OpenPBR"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/UV"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("geometry_opacity"), pxr::SdfValueTypeNames->Float2)
                  .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out")));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateSurfaceOutput(context).ConnectToSource(
      surface.ConnectableAPI(), pxr::TfToken("out")));

  const vector<materialx::SelectedOutput> selected = {
      {"/Looks/FixtureBound/UV", "ND_geompropvalue_vector2", "out", materialx::Type::Vector2},
  };
  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(
      materialx::resolve_manifest_outputs(material, "mtlx", selected, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].type, materialx::Type::Vector2);

  bool found = false;
  for (const materialx::Node &node : graph.nodes) {
    if (node.nodedef == "ND_geompropvalue_vector2") {
      found = true;
      ASSERT_TRUE(node.string_inputs.contains("geomprop"));
      EXPECT_EQ(node.string_inputs.at("geomprop"), "st");
    }
  }
  EXPECT_TRUE(found);
}

namespace {

/** Task 7: a minimal Authority + image-node fixture for fixture-byte
 *  authentication tests, mirroring ManifestFixture's shape. */
struct FixtureBoundAuthorityFixture {
  materialx::Authority authority;
  string image_path;
};

FixtureBoundAuthorityFixture build_fixture_bound_authority_fixture(const TemporaryImage &image_asset)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/OpenPBR"));
  pxr::UsdShadeShader image = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/BaseColorImage"));
  pxr::UsdShadeShader uv = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/FixtureBound/UV"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  uv.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_geompropvalue_vector2")));
  uv.CreateInput(pxr::TfToken("geomprop"), pxr::SdfValueTypeNames->String).Set("st");
  uv.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float2);
  image.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_image_color3")));
  image.CreateInput(pxr::TfToken("file"), pxr::SdfValueTypeNames->Asset)
      .Set(pxr::SdfAssetPath(image_asset.path()));
  image.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
  image.CreateInput(pxr::TfToken("texcoord"), pxr::SdfValueTypeNames->Float2)
      .ConnectToSource(uv.ConnectableAPI(), pxr::TfToken("out"));
  surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
      .ConnectToSource(image.ConnectableAPI(), pxr::TfToken("out"));
  const pxr::TfToken context("mtlx", pxr::TfToken::Immortal);
  material.CreateSurfaceOutput(context).ConnectToSource(surface.ConnectableAPI(),
                                                        pxr::TfToken("out"));

  string usda;
  stage->GetRootLayer()->ExportToString(&usda);

  FixtureBoundAuthorityFixture fixture;
  fixture.authority.document_uuid = "3f9c9a34-7d5e-4a1e-9d2b-4c6b6e6c9b31";
  fixture.authority.digest = materialx::usda_sha256_digest(usda);
  fixture.authority.usda_text_name = ".materialx_usdshade_3f9c9a34-7d5e-4a1e-9d2b-4c6b6e6c9b31";
  fixture.authority.material_path = "/Looks/FixtureBound";
  fixture.authority.usda = usda;
  fixture.authority.render_context = "mtlx";
  fixture.authority.selected_outputs = {
      {"/Looks/FixtureBound/BaseColorImage", "ND_image_color3", "out", materialx::Type::Color3},
  };
  fixture.image_path = image_asset.path();
  return fixture;
}

string sha256_of_file(const string &path)
{
  std::ifstream file(path, std::ios::binary);
  const std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  return "sha256:" + util_sha256_string(bytes);
}

}  // namespace

TEST(materialx_authority_pipeline, authenticates_resolved_fixture_bytes_against_manifest_digest)
{
  const TemporaryImage image_asset;
  const FixtureBoundAuthorityFixture fixture = build_fixture_bound_authority_fixture(image_asset);
  materialx::Authority authority = fixture.authority;
  authority.fixture_digests[fixture.image_path] = sha256_of_file(fixture.image_path);

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
}

TEST(materialx_authority_pipeline, rejects_tampered_fixture_digest_without_mutating_graph)
{
  const TemporaryImage image_asset;
  const FixtureBoundAuthorityFixture fixture = build_fixture_bound_authority_fixture(image_asset);
  materialx::Authority authority = fixture.authority;
  /* A well-formed but wrong digest -- the fixture's real bytes hash to
   * something else. */
  authority.fixture_digests[fixture.image_path] =
      "sha256:0000000000000000000000000000000000000000000000000000000000000";

  materialx::Graph graph;
  graph.has_displacement = true;
  graph.displacement.value = 42.0f;
  vector<materialx::Link> results;
  results.push_back({"sentinel", "out", materialx::Type::Float});
  string error;
  EXPECT_FALSE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error));
  EXPECT_NE(error.find("digest mismatch"), string::npos) << error;
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_FLOAT_EQ(graph.displacement.value, 42.0f);
  ASSERT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].source_node, "sentinel");
}

TEST(materialx_authority_pipeline, rejects_fixture_with_no_manifest_declared_digest_without_mutating_graph)
{
  /* No fixture is implicitly trusted: a resolved graph containing an
   * image/texture node, but an authority that declares zero fixture
   * digests for it specifically (a non-empty map missing this exact
   * path), fails closed rather than silently skipping authentication. */
  const TemporaryImage image_asset;
  const FixtureBoundAuthorityFixture fixture = build_fixture_bound_authority_fixture(image_asset);
  materialx::Authority authority = fixture.authority;
  authority.fixture_digests["/some/other/unrelated/path.png"] =
      "sha256:0000000000000000000000000000000000000000000000000000000000000";

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  EXPECT_FALSE(materialx::resolve_usdshade_authority_outputs(authority, &graph, &results, &error));
  EXPECT_NE(error.find(fixture.image_path), string::npos) << error;
  EXPECT_TRUE(graph.nodes.empty());
  EXPECT_TRUE(results.empty());
}

TEST(materialx_authority_pipeline, allows_fixture_bound_resolution_without_declared_digests_unchanged)
{
  /* Backward compatibility: an authority with an empty fixture_digests
   * map (the default -- every Task 2-6 test's Authority) behaves exactly
   * as before this task, even when the resolved graph contains a
   * fixture-bound node. */
  const TemporaryImage image_asset;
  const FixtureBoundAuthorityFixture fixture = build_fixture_bound_authority_fixture(image_asset);

  materialx::Graph graph;
  vector<materialx::Link> results;
  string error;
  ASSERT_TRUE(
      materialx::resolve_usdshade_authority_outputs(fixture.authority, &graph, &results, &error))
      << error;
  ASSERT_EQ(results.size(), 1);
}

/* ------------------------------------------------------------------------ */
/* Task 3: metadata-driven terminal routing.                                */
/* ------------------------------------------------------------------------ */

TEST(materialx_usdshade_reader, reads_and_lowers_volume_only_material_without_surface)
{
  /* This is the headline Task 3 fix: previously read_usdshade_graph()
   * returned false immediately when there was no connected surface output,
   * so a volume-only material (e.g. a smoke/fog MaterialX graph with no
   * surface at all) was rejected even though its volume terminal was
   * completely valid. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/VolumeOnly"));
  pxr::UsdShadeShader volume_combinator = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeOnly/Volume"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeOnly/Anisotropic"));

  /* ND_absorption_vdf/ND_anisotropic_vdf's real MaterialX 1.39 nodedef
   * (pbrlib/pbrlib_defs.mtlx) declares 'absorption'/'scattering' as
   * vector3, which UsdMtlx surfaces as SdfValueTypeNames->Float3 -- not
   * Color3f, per this reader's own established vector3 convention (see
   * e.g. the fractal3d 'amplitude' input a few hundred lines above). */
  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_anisotropic_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  vdf.CreateInput(pxr::TfToken("scattering"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.4f, 0.5f, 0.6f));
  vdf.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float).Set(0.25f);
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  volume_combinator.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_volume")));
  ASSERT_TRUE(volume_combinator.CreateInput(pxr::TfToken("vdf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));
  volume_combinator.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  const pxr::TfToken mtlx_render_context("mtlx", pxr::TfToken::Immortal);
  ASSERT_TRUE(material.CreateVolumeOutput(mtlx_render_context)
                  .ConnectToSource(volume_combinator.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_FALSE(source.has_displacement);
  EXPECT_FALSE(source.has_light);
  EXPECT_FALSE(source.volume_absorption.is_linked);
  EXPECT_FLOAT_EQ(source.volume_absorption.value.x, 0.1f);
  EXPECT_FLOAT_EQ(source.volume_scattering.value.y, 0.5f);
  EXPECT_FLOAT_EQ(source.volume_anisotropy.value, 0.25f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_volume = dynamic_cast<VolumeCoefficientsNode *>(node);
    if (native_volume) break;
  }
  ASSERT_NE(native_volume, nullptr);
  EXPECT_FLOAT_EQ(native_volume->get_anisotropy(), 0.25f);
  ASSERT_NE(lowered.output()->input("Volume")->link, nullptr);
  EXPECT_EQ(lowered.output()->input("Volume")->link->parent, native_volume);
  EXPECT_EQ(lowered.output()->input("Surface")->link, nullptr);
}

TEST(materialx_usdshade_reader, reads_and_lowers_direct_absorption_vdf_at_volume_terminal)
{
  /* No ND_volume wrapper: a bare VDF connected directly to the material's
   * volume output is also admissible. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/DirectVdf"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DirectVdf/Absorption"));
  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.9f, 0.8f, 0.7f));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_FLOAT_EQ(source.volume_absorption.value.x, 0.9f);
  EXPECT_FLOAT_EQ(source.volume_scattering.value.x, 0.0f);
  EXPECT_FLOAT_EQ(source.volume_anisotropy.value, 0.0f);
}

TEST(materialx_usdshade_reader,
    preserves_co_authored_surface_volume_and_displacement_terminals_atomically)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/AllThree"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/AllThree/OpenPBR"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/AllThree/Absorption"));
  pxr::UsdShadeShader displacement = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/AllThree/Displacement"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float).Set(1.0f);

  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.2f));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  displacement.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_displacementshader")));
  displacement.CreateInput(pxr::TfToken("displacement"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  displacement.CreateInput(pxr::TfToken("scale"), pxr::SdfValueTypeNames->Float).Set(1.5f);
  displacement.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateDisplacementOutput()
                  .ConnectToSource(displacement.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  EXPECT_TRUE(source.has_volume);
  EXPECT_TRUE(source.has_displacement);
  bool found_surface_node = false;
  for (const materialx::Node &node : source.nodes) {
    if (node.nodedef == "ND_open_pbr_surface_surfaceshader") found_surface_node = true;
  }
  EXPECT_TRUE(found_surface_node);
}

TEST(materialx_usdshade_reader,
    rejects_whole_material_when_volume_is_invalid_despite_valid_surface_without_mutating_graph)
{
  /* Atomicity: a valid, otherwise-lowerable surface terminal must not be
   * committed if a co-authored volume terminal fails to authenticate. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/BadVolume"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadVolume/OpenPBR"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/BadVolume/Bogus"));

  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_open_pbr_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float).Set(1.0f);

  /* Not a recognized VDF NodeDef. */
  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_mix_vdf")));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  source.has_displacement = true;
  source.displacement.value = 42.0f;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_FALSE(error.empty());
  /* The caller's pre-existing graph is untouched -- no partial commit. */
  EXPECT_TRUE(source.has_displacement);
  EXPECT_FLOAT_EQ(source.displacement.value, 42.0f);
  EXPECT_TRUE(source.nodes.empty());
}

TEST(materialx_usdshade_reader, routes_lightshader_through_light_path_not_material_surface_output)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Light"));
  pxr::UsdShadeShader light = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Light/PointLight"));
  light.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_point_light")));
  light.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateOutput(pxr::TfToken("mtlx:light"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(light.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  EXPECT_TRUE(source.has_light);
  EXPECT_EQ(source.light_nodedef, "ND_point_light");
  EXPECT_EQ(source.light_node_name, "PointLight");
  /* Never folded into the surface terminal. */
  EXPECT_TRUE(source.nodes.empty());

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  EXPECT_EQ(lowered.output()->input("Surface")->link, nullptr);
  EXPECT_EQ(lowered.output()->input("Volume")->link, nullptr);
}

TEST(materialx_usdshade_reader, rejects_surface_model_with_no_registered_semantic_lowerer)
{
  /* Standard Surface authenticates as a real, reachable surfaceshader
   * terminal but has no Semantic Lowerer registered in this delivery
   * phase (Phase 6 per the design spec), so it must fail closed with a
   * named boundary record rather than being silently coerced into
   * OpenPBR. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/StdSurface"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/StdSurface/Standard"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_standard_surface_surfaceshader")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.5f));

  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("ND_standard_surface_surfaceshader"), string::npos) << error;
  EXPECT_NE(error.find("Standard Surface"), string::npos) << error;
  EXPECT_TRUE(source.nodes.empty());
}

TEST(materialx_usdshade_reader, admits_surface_nodedef_that_declares_inherit_from_open_pbr)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/Inherited"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/Inherited/Custom"));
  /* A custom, versioned NodeDef whose own info:id differs from OpenPBR's,
   * but which explicitly declares (Task 3 NodeDefProvider, single-hop)
   * that it inherits from it. */
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_studio_open_pbr_surface_v2_surfaceshader")));
  surface.GetPrim()
      .CreateAttribute(pxr::TfToken("info:mtlx:inherit"), pxr::SdfValueTypeNames->String)
      .Set(string("ND_open_pbr_surface_surfaceshader"));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  surface.CreateInput(pxr::TfToken("base_weight"), pxr::SdfValueTypeNames->Float).Set(1.0f);

  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_EQ(source.nodes.size(), 1);
  /* Repaired to the canonical NodeDef identity for the existing lowerer. */
  EXPECT_EQ(source.nodes[0].nodedef, "ND_open_pbr_surface_surfaceshader");
}

TEST(materialx_usdshade_reader, admits_surface_unlit_and_reads_literal_inputs)
{
  /* Real ND_surface_unlit admission and field reading -- the
   * five inputs and their literal values come straight from the bundled
   * libraries/stdlib/stdlib_defs.mtlx nodedef; surface_unlit is not an
   * OpenPBR synonym and gets its own Node.nodedef. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Unlit"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface_unlit")));
  surface.CreateInput(pxr::TfToken("emission"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  surface.CreateInput(pxr::TfToken("emission_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  surface.CreateInput(pxr::TfToken("transmission"), pxr::SdfValueTypeNames->Float).Set(0.4f);
  surface.CreateInput(pxr::TfToken("transmission_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.9f, 0.8f, 0.7f));
  surface.CreateInput(pxr::TfToken("opacity"), pxr::SdfValueTypeNames->Float).Set(0.6f);
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;

  ASSERT_EQ(graph.nodes.size(), 1);
  const materialx::Node &unlit = graph.nodes[0];
  EXPECT_EQ(unlit.nodedef, "ND_surface_unlit");
  EXPECT_FLOAT_EQ(unlit.inputs.at("emission"), 2.0f);
  EXPECT_EQ(unlit.color3_inputs.at("emission_color"), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_FLOAT_EQ(unlit.inputs.at("transmission"), 0.4f);
  EXPECT_EQ(unlit.color3_inputs.at("transmission_color"), make_float3(0.9f, 0.8f, 0.7f));
  EXPECT_FLOAT_EQ(unlit.inputs.at("opacity"), 0.6f);
}

TEST(materialx_usdshade_reader, admits_surface_unlit_with_no_authored_inputs_defaulting_via_lower)
{
  /* Authoring zero inputs is valid MaterialX (every ND_surface_unlit input
   * has a real nodedef default) but the existing has_supported_input gate
   * (shared with OpenPBR) still requires at least one authored input on the
   * USD shader prim itself -- mirrors the pre-existing OpenPBR behavior,
   * not a new restriction introduced here. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Unlit"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface_unlit")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("surface_unlit has no supported inputs"), string::npos) << error;
}

TEST(materialx_usdshade_reader, rejects_surface_unlit_with_connected_transmission_input)
{
  /* trans is folded as a compile-time constant into both the transmission
   * tint and the (1 - trans) emission scale in graph.cpp's lowerer -- a
   * connected transmission source is an honest, explicit scope boundary
   * for this delivery phase, not silently mishandled. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Unlit"));
  pxr::UsdShadeShader trans_source = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Trans"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface_unlit")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  trans_source.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  trans_source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  trans_source.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("transmission"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(trans_source.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("connected transmission or opacity input is not yet supported"),
            string::npos)
      << error;
  EXPECT_TRUE(graph.nodes.empty());
}

TEST(materialx_usdshade_reader, rejects_surface_unlit_with_connected_opacity_input)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Unlit"));
  pxr::UsdShadeShader opacity_source = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Opacity"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface_unlit")));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  opacity_source.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  opacity_source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  opacity_source.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(surface.CreateInput(pxr::TfToken("opacity"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(opacity_source.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("connected transmission or opacity input is not yet supported"),
            string::npos)
      << error;
  EXPECT_TRUE(graph.nodes.empty());
}

TEST(materialx_usdshade_reader, rejects_unsupported_surface_unlit_input)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/TestMaterial/Unlit"));
  surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface_unlit")));
  surface.CreateInput(pxr::TfToken("emission"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  /* Not a real ND_surface_unlit input -- OpenPBR's field name, authored on
   * a surface_unlit shader by mistake. */
  surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.5f));
  surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(material.CreateSurfaceOutput()
                  .ConnectToSource(surface.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph graph;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &graph, &error));
  EXPECT_NE(error.find("surface_unlit input has no direct Cycles equivalent: base_color"),
            string::npos)
      << error;
}

TEST(materialx_usdshade_reader, rejects_unconnected_volume_output_but_admits_valid_displacement)
{
  /* An authored-but-not-connected volume output is optional (mirrors the
   * existing displacement behavior) and must not block a co-authored,
   * valid displacement terminal. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/UnconnectedVolume"));
  material.CreateVolumeOutput();  // Authored, never connected.

  pxr::UsdShadeShader displacement = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/UnconnectedVolume/Displacement"));
  displacement.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_displacementshader")));
  displacement.CreateInput(pxr::TfToken("displacement"), pxr::SdfValueTypeNames->Float).Set(0.5f);
  displacement.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(material.CreateDisplacementOutput()
                  .ConnectToSource(displacement.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  EXPECT_FALSE(source.has_volume);
  EXPECT_TRUE(source.has_displacement);
}

TEST(materialx_usdshade_reader, reads_and_lowers_nd_volume_uniform_edf_emission_input)
{
  /* ND_uniform_edf is a real MaterialX 1.39 EDF NodeDef
   * (pbrlib/pbrlib_defs.mtlx) with a direct Cycles equivalent: its 'color'
   * input maps onto VolumeCoefficientsNode's "Emission Coefficients"
   * socket, which already existed natively but was previously always
   * hardcoded to zero regardless of what the reader discovered. This is
   * exactly the shape of the real exact91 ND_volume corpus document
   * (semantic-documents/documents/ND_volume.mtlx), which connects both
   * 'vdf' (ND_absorption_vdf) and 'edf' (ND_uniform_edf). */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission"));
  pxr::UsdShadeShader volume_combinator = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Volume"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Absorption"));
  pxr::UsdShadeShader edf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Emission"));

  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.05f, 0.25f, 0.95f));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  edf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_uniform_edf")));
  edf.CreateInput(pxr::TfToken("color"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.05f, 0.25f, 0.95f));
  edf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  volume_combinator.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_volume")));
  ASSERT_TRUE(volume_combinator.CreateInput(pxr::TfToken("vdf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(volume_combinator.CreateInput(pxr::TfToken("edf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(edf.ConnectableAPI(), pxr::TfToken("out")));
  volume_combinator.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(volume_combinator.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_FALSE(source.volume_emission.is_linked);
  EXPECT_FLOAT_EQ(source.volume_emission.value.x, 0.05f);
  EXPECT_FLOAT_EQ(source.volume_emission.value.y, 0.25f);
  EXPECT_FLOAT_EQ(source.volume_emission.value.z, 0.95f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_volume = dynamic_cast<VolumeCoefficientsNode *>(node);
    if (native_volume) break;
  }
  ASSERT_NE(native_volume, nullptr);
  EXPECT_FLOAT_EQ(native_volume->get_emission_coeffs().y, 0.25f);
}

TEST(materialx_usdshade_reader, rejects_nd_volume_unsupported_edf_type_as_boundary)
{
  /* Only ND_uniform_edf has a native Cycles mapping; any other EDF
   * NodeDef (e.g. a directional/point-style EDF, or a bogus one) is an
   * honest, documented boundary -- fail closed rather than guess. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission"));
  pxr::UsdShadeShader volume_combinator = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Volume"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Absorption"));
  pxr::UsdShadeShader edf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/VolumeEmission/Emission"));

  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.1f));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  edf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_conical_edf")));
  edf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  volume_combinator.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_volume")));
  ASSERT_TRUE(volume_combinator.CreateInput(pxr::TfToken("vdf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(volume_combinator.CreateInput(pxr::TfToken("edf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(edf.ConnectableAPI(), pxr::TfToken("out")));
  volume_combinator.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(volume_combinator.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("edf"), string::npos) << error;
  EXPECT_TRUE(source.nodes.empty());
}

TEST(materialx_usdshade_reader, rejects_cyclic_volume_nodegraph_without_mutating_graph)
{
  /* EndpointResolver depth/cycle protection (shared with the existing
   * resolve_connected_shader recursion, already proven by
   * rejects_cyclic_nodegraph_without_mutating_graph for the surface
   * terminal) also covers the new volume terminal path. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/CyclicVolume"));
  const pxr::UsdShadeNodeGraph first = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/CyclicVolume/First"));
  const pxr::UsdShadeNodeGraph second = pxr::UsdShadeNodeGraph::Define(
      stage, pxr::SdfPath("/Looks/CyclicVolume/Second"));
  ASSERT_TRUE(first.CreateOutput(pxr::TfToken("vdf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("vdf")));
  ASSERT_TRUE(second.CreateOutput(pxr::TfToken("vdf"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("vdf")));
  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("vdf")));

  materialx::Graph source;
  source.nodes.push_back({"sentinel", "unsupported"});
  string error;
  /* The volume terminal probes ND_volume, then ND_anisotropic_vdf, then
   * ND_absorption_vdf in turn; whichever probe first walks the cyclic
   * NodeGraph pair fails closed, and the final reported error is the
   * generic "must connect a supported VDF" boundary rather than the
   * specific "cyclic" message from the swallowed probe attempts. Either
   * way, resolution fails and the caller's graph is untouched -- that is
   * what this test asserts. */
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(source.nodes.size(), 1);
  EXPECT_EQ(source.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_absorption_vdf_absorption_authored_as_color3f)
{
  /* The real MaterialX 1.39 nodedef (pbrlib/pbrlib_defs.mtlx) declares
   * ND_absorption_vdf's 'absorption' input as vector3 (-> USD Float3), not
   * color3 (-> USD Color3f). A document that authors it as Color3f is a
   * genuine type mismatch against the nodedef and must fail closed with a
   * clear reason rather than being silently accepted. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/WrongType"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/WrongType/Absorption"));
  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.5f));
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("absorption"), string::npos) << error;
  EXPECT_NE(error.find("vector3"), string::npos) << error;
  EXPECT_TRUE(source.nodes.empty());
}

TEST(materialx_usdshade_reader, reads_and_lowers_linked_vector3_absorption_on_anisotropic_vdf)
{
  /* 'absorption'/'scattering' can also be authored as a connected vector3
   * sub-graph (mirroring the existing literal-Float3 coverage above), not
   * just a literal. Reuse ND_convert_float_vector3, already proven for the
   * generic vector3 link-reading path elsewhere in this file. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/LinkedVolume"));
  const auto shader = [&](const char *name, const char *id, const pxr::SdfValueTypeName &type) {
    pxr::UsdShadeShader result = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/LinkedVolume").AppendChild(pxr::TfToken(name)));
    result.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    result.CreateOutput(pxr::TfToken("out"), type);
    return result;
  };
  pxr::UsdShadeShader vdf = shader(
      "Anisotropic", "ND_anisotropic_vdf", pxr::SdfValueTypeNames->Token);
  pxr::UsdShadeShader scalar = shader(
      "Scalar", "ND_constant_float", pxr::SdfValueTypeNames->Float);
  pxr::UsdShadeShader broadcast = shader(
      "Broadcast", "ND_convert_float_vector3", pxr::SdfValueTypeNames->Float3);
  scalar.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(0.6f);
  ASSERT_TRUE(broadcast.CreateInput(pxr::TfToken("in"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(scalar.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
                  .ConnectToSource(broadcast.ConnectableAPI(), pxr::TfToken("out")));
  vdf.CreateInput(pxr::TfToken("scattering"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.3f, 0.3f, 0.3f));
  vdf.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float).Set(0.1f);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_TRUE(source.volume_absorption.is_linked);
  EXPECT_EQ(source.volume_absorption.link.type, materialx::Type::Vector3);
  EXPECT_FALSE(source.volume_scattering.is_linked);
  EXPECT_FLOAT_EQ(source.volume_scattering.value.x, 0.3f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    native_volume = dynamic_cast<VolumeCoefficientsNode *>(node);
    if (native_volume) break;
  }
  ASSERT_NE(native_volume, nullptr);
  ASSERT_NE(native_volume->input("Absorption Coefficients")->link, nullptr);
}

/* Closure combinator lowering: ND_multiply_vdfF/ND_multiply_vdfC scale a
 * VDF's absorption/scattering coefficients by a literal weight, ND_add_vdf
 * sums two VDFs' coefficients (see read_vdf_coefficients() in
 * usdshade_reader.cpp for the full real-native-mapping rationale and its
 * honest anisotropy-conflict boundary). */

TEST(materialx_usdshade_reader, reads_and_lowers_multiply_vdff_scaling_absorption_vdf)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ScaledAbsorption"));
  pxr::UsdShadeShader absorption = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ScaledAbsorption/Absorption"));
  absorption.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  absorption.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.2f, 0.4f, 0.6f));
  absorption.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader scaled = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ScaledAbsorption/Scaled"));
  scaled.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_vdfF")));
  ASSERT_TRUE(scaled.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(absorption.ConnectableAPI(), pxr::TfToken("out")));
  scaled.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float).Set(2.0f);
  scaled.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(scaled.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_TRUE(source.volume_absorption.is_linked);
  EXPECT_EQ(source.volume_absorption.link.type, materialx::Type::Vector3);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (VolumeCoefficientsNode *volume = dynamic_cast<VolumeCoefficientsNode *>(node)) {
      native_volume = volume;
      break;
    }
  }
  ASSERT_NE(native_volume, nullptr);
  ASSERT_NE(native_volume->input("Absorption Coefficients")->link, nullptr);
  /* The lowering emits one scale node per coefficient channel (absorption
   * and scattering), both with NODE_VECTOR_MATH_SCALE -- identify the one
   * that actually feeds "Absorption Coefficients" via its link rather than
   * scanning lowered.nodes for "a" scale node, since more than one exists. */
  VectorMathNode *scale_node = dynamic_cast<VectorMathNode *>(
      native_volume->input("Absorption Coefficients")->link->parent);
  ASSERT_NE(scale_node, nullptr);
  EXPECT_EQ(scale_node->get_math_type(), NODE_VECTOR_MATH_SCALE);
  EXPECT_FLOAT_EQ(scale_node->get_scale(), 2.0f);
}

TEST(materialx_usdshade_reader, reads_and_lowers_multiply_vdfc_scaling_anisotropic_vdf)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ScaledAnisotropic"));
  pxr::UsdShadeShader vdf = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ScaledAnisotropic/Anisotropic"));
  vdf.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_anisotropic_vdf")));
  vdf.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.1f, 0.1f, 0.1f));
  vdf.CreateInput(pxr::TfToken("scattering"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.2f, 0.2f, 0.2f));
  vdf.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float).Set(0.4f);
  vdf.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader scaled = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ScaledAnisotropic/Scaled"));
  scaled.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_vdfC")));
  ASSERT_TRUE(scaled.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(vdf.ConnectableAPI(), pxr::TfToken("out")));
  scaled.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Color3f)
      .Set(pxr::GfVec3f(0.5f, 1.0f, 2.0f));
  scaled.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(scaled.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_TRUE(source.volume_absorption.is_linked);
  EXPECT_TRUE(source.volume_scattering.is_linked);
  /* Anisotropy passes through the scaling weight untouched: it shapes the
   * phase function, which weighting the coefficients' magnitude does not
   * change. */
  EXPECT_FALSE(source.volume_anisotropy.is_linked);
  EXPECT_FLOAT_EQ(source.volume_anisotropy.value, 0.4f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  int multiply_count = 0;
  for (ShaderNode *node : lowered.nodes) {
    if (VolumeCoefficientsNode *volume = dynamic_cast<VolumeCoefficientsNode *>(node)) {
      native_volume = volume;
    }
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node)) {
      if (math->get_math_type() == NODE_VECTOR_MATH_MULTIPLY) {
        multiply_count++;
      }
    }
  }
  ASSERT_NE(native_volume, nullptr);
  EXPECT_EQ(multiply_count, 2);
  EXPECT_FLOAT_EQ(native_volume->get_anisotropy(), 0.4f);
}

TEST(materialx_usdshade_reader, reads_and_lowers_add_vdf_of_two_absorption_only_vdfs)
{
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/AddedAbsorption"));
  const auto absorption_shader = [&](const char *name, const pxr::GfVec3f &value) {
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/AddedAbsorption").AppendChild(pxr::TfToken(name)));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
    shader.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3).Set(value);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    return shader;
  };
  pxr::UsdShadeShader first = absorption_shader("First", pxr::GfVec3f(0.1f, 0.2f, 0.3f));
  pxr::UsdShadeShader second = absorption_shader("Second", pxr::GfVec3f(0.05f, 0.1f, 0.15f));

  pxr::UsdShadeShader sum = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/AddedAbsorption/Sum"));
  sum.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vdf")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  sum.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_TRUE(source.volume_absorption.is_linked);
  EXPECT_FALSE(source.volume_anisotropy.is_linked);
  EXPECT_FLOAT_EQ(source.volume_anisotropy.value, 0.0f);

  ShaderGraph lowered;
  ASSERT_TRUE(materialx::lower(source, &lowered));
  VolumeCoefficientsNode *native_volume = nullptr;
  for (ShaderNode *node : lowered.nodes) {
    if (VolumeCoefficientsNode *volume = dynamic_cast<VolumeCoefficientsNode *>(node)) {
      native_volume = volume;
      break;
    }
  }
  ASSERT_NE(native_volume, nullptr);
  ASSERT_NE(native_volume->input("Absorption Coefficients")->link, nullptr);
  /* The lowering emits one combinator node per coefficient channel
   * (absorption and scattering), both with NODE_VECTOR_MATH_ADD -- identify
   * the one that actually feeds "Absorption Coefficients" via its link
   * rather than scanning lowered.nodes for "an" add node, since more than
   * one exists. */
  VectorMathNode *add_node = dynamic_cast<VectorMathNode *>(
      native_volume->input("Absorption Coefficients")->link->parent);
  ASSERT_NE(add_node, nullptr);
  EXPECT_EQ(add_node->get_math_type(), NODE_VECTOR_MATH_ADD);
}

TEST(materialx_usdshade_reader, reads_and_lowers_add_vdf_absorption_plus_anisotropic)
{
  /* Only one operand contributes scattering, so its anisotropy is used
   * unambiguously -- no conflict to fail closed on. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/MixedAdd"));
  pxr::UsdShadeShader absorption = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/MixedAdd/Absorption"));
  absorption.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  absorption.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.1f, 0.1f, 0.1f));
  absorption.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader anisotropic = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/MixedAdd/Anisotropic"));
  anisotropic.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_anisotropic_vdf")));
  anisotropic.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.05f, 0.05f, 0.05f));
  anisotropic.CreateInput(pxr::TfToken("scattering"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.3f, 0.3f, 0.3f));
  anisotropic.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float).Set(0.7f);
  anisotropic.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader sum = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Looks/MixedAdd/Sum"));
  sum.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vdf")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(absorption.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(anisotropic.ConnectableAPI(), pxr::TfToken("out")));
  sum.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
  ASSERT_TRUE(source.has_volume);
  EXPECT_FALSE(source.volume_anisotropy.is_linked);
  EXPECT_FLOAT_EQ(source.volume_anisotropy.value, 0.7f);
}

TEST(materialx_usdshade_reader, rejects_add_vdf_of_two_differently_anisotropic_vdfs)
{
  /* Genuine architectural gap: Cycles' VolumeCoefficientsNode has one
   * scalar anisotropy for its whole coefficient bundle, so it cannot
   * represent the superposition of two independently-anisotropic phase
   * functions. This must fail closed, not silently average or pick one. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/ConflictingAdd"));
  const auto anisotropic_shader = [&](const char *name, const float anisotropy) {
    pxr::UsdShadeShader shader = pxr::UsdShadeShader::Define(
        stage, pxr::SdfPath("/Looks/ConflictingAdd").AppendChild(pxr::TfToken(name)));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_anisotropic_vdf")));
    shader.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
        .Set(pxr::GfVec3f(0.1f));
    shader.CreateInput(pxr::TfToken("scattering"), pxr::SdfValueTypeNames->Float3)
        .Set(pxr::GfVec3f(0.2f));
    shader.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float).Set(anisotropy);
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    return shader;
  };
  pxr::UsdShadeShader first = anisotropic_shader("First", 0.2f);
  pxr::UsdShadeShader second = anisotropic_shader("Second", 0.8f);

  pxr::UsdShadeShader sum = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/ConflictingAdd/Sum"));
  sum.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_add_vdf")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(first.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(sum.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(second.ConnectableAPI(), pxr::TfToken("out")));
  sum.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(sum.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  source.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("architectural gap"), string::npos) << error;
  ASSERT_EQ(source.nodes.size(), 1);
  EXPECT_EQ(source.nodes[0].name, "sentinel");
}

TEST(materialx_usdshade_reader, rejects_multiply_vdff_with_dynamic_weight_as_boundary)
{
  /* A dynamic/graph-driven scaling weight is an explicit, unsupported
   * boundary this pass -- not silently narrowed to its default. */
  const pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  ASSERT_TRUE(stage);
  const pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(
      stage, pxr::SdfPath("/Looks/DynamicWeight"));
  pxr::UsdShadeShader absorption = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DynamicWeight/Absorption"));
  absorption.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_absorption_vdf")));
  absorption.CreateInput(pxr::TfToken("absorption"), pxr::SdfValueTypeNames->Float3)
      .Set(pxr::GfVec3f(0.1f));
  absorption.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  pxr::UsdShadeShader weight_source = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DynamicWeight/WeightConst"));
  weight_source.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_float")));
  weight_source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Float).Set(1.5f);
  weight_source.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);

  pxr::UsdShadeShader scaled = pxr::UsdShadeShader::Define(
      stage, pxr::SdfPath("/Looks/DynamicWeight/Scaled"));
  scaled.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_multiply_vdfF")));
  ASSERT_TRUE(scaled.CreateInput(pxr::TfToken("in1"), pxr::SdfValueTypeNames->Token)
                  .ConnectToSource(absorption.ConnectableAPI(), pxr::TfToken("out")));
  ASSERT_TRUE(scaled.CreateInput(pxr::TfToken("in2"), pxr::SdfValueTypeNames->Float)
                  .ConnectToSource(weight_source.ConnectableAPI(), pxr::TfToken("out")));
  scaled.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);

  ASSERT_TRUE(material.CreateVolumeOutput()
                  .ConnectToSource(scaled.ConnectableAPI(), pxr::TfToken("out")));

  materialx::Graph source;
  source.nodes.push_back({"sentinel", "unsupported"});
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(material, &source, &error));
  EXPECT_NE(error.find("literal value"), string::npos) << error;
  ASSERT_EQ(source.nodes.size(), 1);
  EXPECT_EQ(source.nodes[0].name, "sentinel");
}

CCL_NAMESPACE_END
