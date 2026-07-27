/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <filesystem>
#include <fstream>
#include <limits>

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
      "ND_add_color3", "ND_subtract_color3", "ND_multiply_color3", "ND_divide_color3"};
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

CCL_NAMESPACE_END
