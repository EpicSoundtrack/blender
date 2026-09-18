/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/shader.h>

#include "materialx/graph.h"
#include "materialx/usdshade_reader.h"
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/sha256.h"

CCL_NAMESPACE_BEGIN
#include "materialx_standard_surface_defaults_fixtures.h"
namespace {
using Default = ss_completion_data::Default;
const char *ss_id(const bool legacy)
{
  return legacy ? "ND_standard_surface_surfaceshader_100" : "ND_standard_surface_surfaceshader";
}
const pxr::SdfValueTypeName &ss_type(const Default &d)
{
  if (string(d.kind) == "color3")
    return pxr::SdfValueTypeNames->Color3f;
  if (string(d.kind) == "boolean")
    return pxr::SdfValueTypeNames->Bool;
  return pxr::SdfValueTypeNames->Float;
}
pxr::VtValue ss_value(const Default &d, const bool legacy, const bool nondefault = false)
{
  if (string(d.kind) == "color3") {
    const float3 c = nondefault ? make_float3(0.2f, 0.4f, 0.6f) :
                                  (legacy ? d.color100 : d.color101);
    return pxr::VtValue(pxr::GfVec3f(c.x, c.y, c.z));
  }
  if (string(d.kind) == "boolean")
    return pxr::VtValue(nondefault);
  return pxr::VtValue(nondefault ? 0.375f : (legacy ? d.scalar100 : d.scalar101));
}
struct DefaultsFixture {
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Material"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/S"));
  DefaultsFixture(const bool legacy)
  {
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken(ss_id(legacy))));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    material.CreateSurfaceOutput(pxr::TfToken("mtlx"))
        .ConnectToSource(surface.GetOutput(pxr::TfToken("out")));
    // Keep a real authored recognized control for baseline reader admission.
    surface.CreateInput(pxr::TfToken("specular"), pxr::SdfValueTypeNames->Float).Set(1.0f);
  }
  pxr::UsdShadeOutput constant(const Default &d, const bool legacy, const bool nondefault = false)
  {
    auto s = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Constant"));
    const char *id = string(d.kind) == "color3"  ? "ND_constant_color3" :
                     string(d.kind) == "boolean" ? "ND_constant_boolean" :
                                                   "ND_constant_float";
    s.CreateIdAttr(pxr::VtValue(pxr::TfToken(id)));
    s.CreateInput(pxr::TfToken("value"), ss_type(d))
        .GetAttr()
        .Set(ss_value(d, legacy, nondefault));
    return s.CreateOutput(pxr::TfToken("out"), ss_type(d));
  }
};
string layer_bytes(const pxr::SdfLayerHandle &layer)
{
  string text;
  EXPECT_TRUE(layer->ExportToString(&text));
  return text;
}
void reject_unchanged(DefaultsFixture &f)
{
  const string root = layer_bytes(f.stage->GetRootLayer()),
               session = layer_bytes(f.stage->GetSessionLayer());
  materialx::Graph graph;
  graph.nodes.push_back({"sentinel", "unsupported"});
  graph.has_displacement = true;
  graph.displacement.value = 0.125f;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(f.material, &graph, &error));
  EXPECT_FALSE(error.empty());
  ASSERT_EQ(graph.nodes.size(), 1u);
  EXPECT_EQ(graph.nodes[0].name, "sentinel");
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_FLOAT_EQ(graph.displacement.value, 0.125f);
  EXPECT_EQ(layer_bytes(f.stage->GetRootLayer()), root);
  EXPECT_EQ(layer_bytes(f.stage->GetSessionLayer()), session);
}
void expect_default(const materialx::Node &node, const Default &d, const bool legacy)
{
  if (d.restricted) {
    EXPECT_FALSE(node.inputs.contains(d.name));
    EXPECT_FALSE(node.color3_inputs.contains(d.name));
    EXPECT_FALSE(node.links.contains(d.name));
    return;
  }
  if (string(d.kind) == "color3") {
    ASSERT_TRUE(node.color3_inputs.contains(d.name)) << d.name;
    EXPECT_EQ(node.color3_inputs.at(d.name), legacy ? d.color100 : d.color101) << d.name;
  }
  else if (string(d.kind) == "boolean") {
    ASSERT_TRUE(node.int_inputs.contains(d.name));
    EXPECT_EQ(node.int_inputs.at(d.name), 0);
  }
  else {
    ASSERT_TRUE(node.inputs.contains(d.name)) << d.name;
    EXPECT_FLOAT_EQ(node.inputs.at(d.name), legacy ? d.scalar100 : d.scalar101) << d.name;
  }
  EXPECT_FALSE(node.links.contains(d.name));
}
PrincipledBsdfNode *principled(ShaderGraph &graph)
{
  for (ShaderNode *n : graph.nodes)
    if (auto *p = dynamic_cast<PrincipledBsdfNode *>(n))
      return p;
  return nullptr;
}
}  // namespace

TEST(materialx_usdshade_reader, standard_surface_all_sdk_defaults_missing_or_declared)
{
  for (const bool legacy : {false, true})
    for (const bool declared : {false, true}) {
      for (const Default &d : ss_completion_data::defaults) {
        SCOPED_TRACE(string(ss_id(legacy)) + ":" + d.name + (declared ? ":declared" : ":absent"));
        DefaultsFixture f(legacy);
        f.surface.GetPrim().RemoveProperty(pxr::TfToken(string("inputs:") + d.name));
        if (declared)
          f.surface.CreateInput(pxr::TfToken(d.name), ss_type(d));
        const string before = layer_bytes(f.stage->GetRootLayer());
        materialx::Graph graph;
        string error;
        ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
        ASSERT_EQ(graph.nodes.size(), 1u);
        EXPECT_EQ(graph.nodes[0].nodedef, "ND_standard_surface_surfaceshader");
        expect_default(graph.nodes[0], d, legacy);
        EXPECT_EQ(layer_bytes(f.stage->GetRootLayer()), before);
        ShaderGraph lowered;
        ASSERT_TRUE(materialx::lower(graph, &lowered, &error)) << error;
      }
    }
}

TEST(materialx_usdshade_reader, standard_surface_exact_broad_canonical_omissions_lower)
{
  for (const auto &fixture : ss_completion_data::fixtures) {
    SCOPED_TRACE(fixture.name);
    ASSERT_EQ(util_sha256_string(fixture.usda), fixture.sha);
    const auto stage = pxr::UsdStage::CreateInMemory();
    ASSERT_TRUE(stage->GetRootLayer()->ImportFromString(fixture.usda));
    const string before = layer_bytes(stage->GetRootLayer());
    const auto material = pxr::UsdShadeMaterial(
        stage->GetPrimAtPath(pxr::SdfPath("/Looks/Material")));
    materialx::Graph parsed;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(material, &parsed, &error)) << error;
    ASSERT_EQ(parsed.nodes.size(), 1u);
    for (const Default &d : ss_completion_data::defaults)
      expect_default(parsed.nodes[0], d, fixture.legacy);
    ShaderGraph lowered;
    ASSERT_TRUE(materialx::lower(parsed, &lowered, &error)) << error;
    auto *p = principled(lowered);
    ASSERT_NE(p, nullptr);
    EXPECT_FLOAT_EQ(p->get_base_weight(), fixture.legacy ? 0.8f : 1.0f);
    EXPECT_EQ(p->get_base_color(), fixture.legacy ? one_float3() : make_float3(0.8f, 0.8f, 0.8f));
    EXPECT_FLOAT_EQ(p->get_subsurface_scale(), 1.0f);
    EXPECT_EQ(p->get_subsurface_radius(), one_float3());
    EXPECT_FLOAT_EQ(p->get_thin_film_ior(), 1.5f);
    EXPECT_EQ(layer_bytes(stage->GetRootLayer()), before);
  }
}

TEST(materialx_usdshade_reader,
     standard_surface_remaining_defaults_reject_malformed_transactionally)
{
  for (const bool legacy : {false, true})
    for (const Default &d : ss_completion_data::defaults) {
      // Cover each type and supported/default-only path without repeating every scalar.
      if (string(d.name) != "base" && string(d.name) != "transmission_color" &&
          string(d.name) != "thin_walled" && string(d.name) != "transmission_depth" &&
          string(d.name) != "subsurface_color")
        continue;
      for (const string mode : {"wrong_type",
                                "blocked",
                                "sample",
                                "sample_default",
                                "dangling",
                                "dangling_default",
                                "multiple_default"})
      {
        SCOPED_TRACE(string(ss_id(legacy)) + ":" + d.name + ":" + mode);
        DefaultsFixture f(legacy);
        auto input = f.surface.CreateInput(pxr::TfToken(d.name),
                                           mode == "wrong_type" ? pxr::SdfValueTypeNames->String :
                                                                  ss_type(d));
        if (mode == "wrong_type")
          input.Set(string("not-a-value"));
        else if (mode == "blocked") {
          input.GetAttr().Block();
        }
        else if (mode == "sample" || mode == "sample_default") {
          ASSERT_TRUE(input.GetAttr().Set(ss_value(d, legacy), pxr::UsdTimeCode(1.0)));
          if (mode == "sample_default")
            ASSERT_TRUE(input.GetAttr().Set(ss_value(d, legacy)));
        }
        else {
          pxr::SdfPathVector paths = {pxr::SdfPath("/Missing.outputs:out")};
          if (mode == "multiple_default")
            paths.insert(paths.begin(), f.constant(d, legacy).GetAttr().GetPath());
          ASSERT_TRUE(input.GetAttr().SetConnections(paths));
          if (mode == "multiple_default" || mode == "dangling_default")
            input.GetAttr().Set(ss_value(d, legacy));
        }
        reject_unchanged(f);
      }
    }
}

TEST(materialx_usdshade_reader, standard_surface_restricted_defaults_remain_literal_only)
{
  for (const bool legacy : {false, true})
    for (const Default &d : ss_completion_data::defaults) {
      if (!d.restricted)
        continue;
      for (const bool connected : {false, true}) {
        SCOPED_TRACE(string(ss_id(legacy)) + ":" + d.name +
                     (connected ? ":linked_default" : ":nondefault"));
        DefaultsFixture f(legacy);
        auto input = f.surface.CreateInput(pxr::TfToken(d.name), ss_type(d));
        if (connected)
          ASSERT_TRUE(input.ConnectToSource(f.constant(d, legacy)));
        else
          ASSERT_TRUE(input.GetAttr().Set(ss_value(d, legacy, true)));
        reject_unchanged(f);
      }
    }
}

TEST(materialx_usdshade_reader, standard_surface_default_completion_keeps_connected_values)
{
  for (const bool legacy : {false, true})
    for (const Default &d : ss_completion_data::defaults) {
      if (string(d.name) != "base" && string(d.name) != "transmission_color")
        continue;
      DefaultsFixture f(legacy);
      auto input = f.surface.CreateInput(pxr::TfToken(d.name), ss_type(d));
      ASSERT_TRUE(input.ConnectToSource(f.constant(d, legacy, true)));
      materialx::Graph parsed;
      string error;
      ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &parsed, &error)) << error;
      ASSERT_EQ(parsed.nodes.size(), 2u);
      EXPECT_TRUE(parsed.nodes.back().links.contains(d.name));
      EXPECT_FALSE(parsed.nodes.back().inputs.contains(d.name));
      EXPECT_FALSE(parsed.nodes.back().color3_inputs.contains(d.name));
      if (string(d.kind) == "color3")
        EXPECT_EQ(parsed.nodes.front().color3_inputs.at("value"), make_float3(.2f, .4f, .6f));
      else
        EXPECT_FLOAT_EQ(parsed.nodes.front().inputs.at("value"), .375f);
      ShaderGraph lowered;
      ASSERT_TRUE(materialx::lower(parsed, &lowered, &error)) << error;
    }
}

TEST(materialx_usdshade_reader,
     standard_surface_default_completion_rejects_invalid_source_endpoints)
{
  for (const bool legacy : {false, true})
    for (const string mode : {"shader_input",
                              "wrong_output_type",
                              "unknown_output",
                              "unknown_id",
                              "unrecognized_namespace"})
    {
      SCOPED_TRACE(string(ss_id(legacy)) + ":" + mode);
      DefaultsFixture f(legacy);
      auto source = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/Color"));
      source.CreateIdAttr(pxr::VtValue(
          pxr::TfToken(mode == "unknown_id" ? "ND_absent_color3" : "ND_constant_color3")));
      source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(1));
      source.CreateOutput(pxr::TfToken("out"),
                          mode == "wrong_output_type" ? pxr::SdfValueTypeNames->Float :
                                                        pxr::SdfValueTypeNames->Color3f);
      auto input = f.surface.CreateInput(pxr::TfToken("transmission_color"),
                                         pxr::SdfValueTypeNames->Color3f);
      if (mode == "shader_input") {
        ASSERT_TRUE(input.ConnectToSource(
            source.ConnectableAPI(), pxr::TfToken("value"), pxr::UsdShadeAttributeType::Input));
      }
      else {
        const char *suffix = mode == "unknown_output"         ? ".outputs:missing" :
                             mode == "unrecognized_namespace" ? ".invalid:out" :
                                                                ".outputs:out";
        ASSERT_TRUE(
            input.GetAttr().SetConnections({pxr::SdfPath(string("/Material/Color") + suffix)}));
      }
      reject_unchanged(f);
    }
}

TEST(materialx_usdshade_reader, standard_surface_composed_disconnect_restores_versioned_defaults)
{
  for (const bool legacy : {false, true})
    for (const Default &d : ss_completion_data::defaults) {
      if (string(d.name) != "base" && string(d.name) != "base_color" &&
          string(d.name) != "thin_walled" && string(d.name) != "subsurface_color")
        continue;
      DefaultsFixture f(legacy);
      auto input = f.surface.CreateInput(pxr::TfToken(d.name), ss_type(d));
      input.GetAttr().SetConnections({pxr::SdfPath("/Missing.outputs:out")});
      f.stage->SetEditTarget(f.stage->GetSessionLayer());
      ASSERT_TRUE(input.GetAttr().SetConnections({}));
      materialx::Graph parsed;
      string error;
      ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &parsed, &error)) << error;
      expect_default(parsed.nodes.back(), d, legacy);
    }
}

TEST(materialx_graph, standard_surface_latent_sss_and_film_defaults_match_explicit)
{
  for (const bool transmission : {false, true})
    for (const bool authored : {false, true}) {
      materialx::Node surface;
      surface.name = "Standard";
      surface.nodedef = "ND_standard_surface_surfaceshader";
      surface.outputs["out"] = materialx::Type::SurfaceShader;
      surface.inputs["subsurface"] = 1.0f;
      surface.inputs["thin_film_thickness"] = 400.0f;
      if (transmission)
        surface.inputs["transmission"] = 1.0f;
      if (authored) {
        surface.inputs["subsurface_scale"] = 1.0f;
        surface.color3_inputs["subsurface_radius"] = one_float3();
        surface.inputs["thin_film_IOR"] = 1.5f;
      }
      ShaderGraph lowered;
      string error;
      ASSERT_TRUE(materialx::lower({{surface}}, &lowered, &error)) << error;
      auto *p = principled(lowered);
      ASSERT_NE(p, nullptr);
      EXPECT_FLOAT_EQ(p->get_subsurface_scale(), 1.0f);
      EXPECT_EQ(p->get_subsurface_radius(), one_float3());
      EXPECT_FLOAT_EQ(p->get_thin_film_ior(), 1.5f);
      bool saw_glass = false;
      for (ShaderNode *n : lowered.nodes)
        if (auto *g = dynamic_cast<GlassBsdfNode *>(n)) {
          saw_glass = true;
          EXPECT_FLOAT_EQ(g->get_thin_film_ior(), 1.5f);
        }
      EXPECT_EQ(saw_glass, transmission);
    }
}

TEST(materialx_usdshade_reader,
     standard_surface_default_completion_preserves_nodegraph_input_source_enum)
{
  for (const bool legacy : {false, true}) {
    DefaultsFixture f(legacy);
    const Default *color = nullptr;
    for (const Default &d : ss_completion_data::defaults)
      if (string(d.name) == "transmission_color")
        color = &d;
    ASSERT_NE(color, nullptr);
    auto boundary = pxr::UsdShadeNodeGraph::Define(f.stage, pxr::SdfPath("/Material/Interface"));
    auto interface = boundary.CreateInput(pxr::TfToken("tint"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(interface.ConnectToSource(f.constant(*color, legacy, true)));
    auto input = f.surface.CreateInput(pxr::TfToken("transmission_color"),
                                       pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(input.ConnectToSource(
        boundary.ConnectableAPI(), pxr::TfToken("tint"), pxr::UsdShadeAttributeType::Input));
    const string before = layer_bytes(f.stage->GetRootLayer());
    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
    ASSERT_EQ(graph.nodes.size(), 2u);
    EXPECT_TRUE(graph.nodes.back().links.contains("transmission_color"));
    EXPECT_EQ(graph.nodes.front().color3_inputs.at("value"), make_float3(.2f, .4f, .6f));
    EXPECT_EQ(layer_bytes(f.stage->GetRootLayer()), before);
  }
}

TEST(materialx_usdshade_reader,
     standard_surface_default_completion_does_not_admit_sourcecode_shader)
{
  for (const bool legacy : {false, true}) {
    DefaultsFixture f(legacy);
    auto source = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/Color"));
    source.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_constant_color3")));
    source.CreateImplementationSourceAttr(pxr::VtValue(pxr::TfToken("sourceCode")));
    source.CreateInput(pxr::TfToken("value"), pxr::SdfValueTypeNames->Color3f)
        .Set(pxr::GfVec3f(1));
    auto output = source.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f);
    ASSERT_TRUE(
        f.surface.CreateInput(pxr::TfToken("transmission_color"), pxr::SdfValueTypeNames->Color3f)
            .ConnectToSource(output));
    reject_unchanged(f);
  }
}

// Explicit reviewed contract correction: a static valid source takes precedence
// over shadowed local opinions, including the former four-scalar time restriction.
TEST(materialx_usdshade_reader, standard_surface_valid_sources_supersede_shadowed_local_opinions)
{
  for (const bool legacy : {false, true})
    for (const Default &d : ss_completion_data::defaults) {
      if (string(d.name) != "base" && string(d.name) != "transmission_color" &&
          string(d.name) != "coat")
        continue;
      for (const string mode : {"blocked", "sample", "sample_default"}) {
        SCOPED_TRACE(string(ss_id(legacy)) + ":" + d.name + ":" + mode);
        DefaultsFixture f(legacy);
        auto input = f.surface.CreateInput(pxr::TfToken(d.name), ss_type(d));
        if (mode == "blocked")
          input.GetAttr().Block();
        else {
          input.GetAttr().Set(ss_value(d, legacy), pxr::UsdTimeCode(1));
          if (mode == "sample_default")
            input.GetAttr().Set(ss_value(d, legacy));
        }
        ASSERT_TRUE(input.ConnectToSource(f.constant(d, legacy, true)));
        const string before = layer_bytes(f.stage->GetRootLayer());
        materialx::Graph graph;
        string error;
        ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
        ASSERT_EQ(graph.nodes.size(), 2u);
        EXPECT_TRUE(graph.nodes.back().links.contains(d.name));
        EXPECT_FALSE(graph.nodes.back().inputs.contains(d.name));
        EXPECT_FALSE(graph.nodes.back().color3_inputs.contains(d.name));
        EXPECT_EQ(layer_bytes(f.stage->GetRootLayer()), before);
      }
    }
}

CCL_NAMESPACE_END
