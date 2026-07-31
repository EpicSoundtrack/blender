/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <unordered_map>

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
    if (rejection != 1) {
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
                                 rejection == 1 ? "requires ND_image_color4" :
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

CCL_NAMESPACE_END
