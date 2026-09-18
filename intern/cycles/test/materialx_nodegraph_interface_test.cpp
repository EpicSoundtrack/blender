/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */
#include "testing/testing.h"

#include "materialx/graph.h"
#include "materialx/usdshade_reader.h"
#include "scene/shader_graph.h"
#include "util/sha256.h"
#include <limits>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/nodeGraph.h>
#include <pxr/usd/usdShade/shader.h>

CCL_NAMESPACE_BEGIN
namespace {
string bytes(const pxr::SdfLayerHandle &layer)
{
  string s;
  EXPECT_TRUE(layer->ExportToString(&s));
  return s;
}
struct InterfaceFixture {
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Material"));
  pxr::UsdShadeNodeGraph graph = pxr::UsdShadeNodeGraph::Define(stage,
                                                                pxr::SdfPath("/Material/G"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/G/S"));
  pxr::UsdShadeInput value, consumer;
  pxr::SdfValueTypeName type;
  pxr::VtValue expected;
  InterfaceFixture(int role = 0, bool legacy = false)
  {
    type = role == 0 ? pxr::SdfValueTypeNames->Color3f :
           role == 1 ? pxr::SdfValueTypeNames->Float :
                       pxr::SdfValueTypeNames->Vector3f;
    expected = role == 1 ? pxr::VtValue(.42f) : pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f));
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken(
        legacy ? "ND_standard_surface_surfaceshader_100" : "ND_standard_surface_surfaceshader")));
    surface.CreateInput(pxr::TfToken("base"), pxr::SdfValueTypeNames->Float).Set(1.f);
    material.CreateSurfaceOutput(pxr::TfToken("mtlx"))
        .ConnectToSource(surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token));
    value = graph.CreateInput(pxr::TfToken("value"), type);
    value.GetAttr().Set(expected);
    consumer = surface.CreateInput(pxr::TfToken(role == 0 ? "base_color" :
                                                role == 1 ? "specular_roughness" :
                                                            "normal"),
                                   type);
    consumer.ConnectToSource(value);
    if (role == 2) {
      // Standard Surface normal accepts normalmap producers; test numeric vector
      // interfaces through an existing supported vector-to-color consumer.
      surface.GetPrim().RemoveProperty(pxr::TfToken("inputs:normal"));
      auto converter = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/VectorColor"));
      converter.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_convert_vector3_color3")));
      consumer = converter.CreateInput(pxr::TfToken("in"), type);
      consumer.ConnectToSource(value);
      surface.CreateInput(pxr::TfToken("base_color"), pxr::SdfValueTypeNames->Color3f)
          .ConnectToSource(
              converter.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Color3f));
    }
  }
  pxr::UsdShadeOutput constant()
  {
    auto s = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Constant"));
    s.CreateIdAttr(pxr::VtValue(
        pxr::TfToken(type == pxr::SdfValueTypeNames->Float   ? "ND_constant_float" :
                     type == pxr::SdfValueTypeNames->Color3f ? "ND_constant_color3" :
                                                               "ND_constant_vector3")));
    s.CreateInput(pxr::TfToken("value"), type).GetAttr().Set(expected);
    return s.CreateOutput(pxr::TfToken("out"), type);
  }
  void check(bool accept)
  {
    const string before = bytes(stage->GetRootLayer()), session = bytes(stage->GetSessionLayer());
    materialx::Graph result;
    string error;
    const bool ok = materialx::read_usdshade_graph(material, &result, &error);
    EXPECT_EQ(ok, accept) << error;
    if (ok && accept) {
      ShaderGraph native;
      EXPECT_TRUE(materialx::lower(result, &native, &error)) << error;
    }
    if (!ok)
      EXPECT_FALSE(error.empty());
    EXPECT_EQ(bytes(stage->GetRootLayer()), before);
    EXPECT_EQ(bytes(stage->GetSessionLayer()), session);
  }
};
const char retained_usda[] = R"MXIF(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            float inputs:in = 0.42 (
                customData = {
                    string audit_note = "existing typed parent input must survive utility creation"
                    string materialXType = "float"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                float inputs:audit_samples = 0.5 (
                    customData = {
                        string audit_note = "unused time opinions, never a consumed animated source"
                        string materialXType = "float"
                    }
                )
                float inputs:audit_samples.timeSamples = {
                    1: 0.2,
                    2: 0.8,
                }
                float inputs:roughness = 0.42 (
                    customData = {
                        string audit_note = "nested public interface literal"
                        string materialXType = "float"
                    }
                )
                color3f inputs:tint = (0.2, 0.4, 0.6) (
                    customData = {
                        string audit_note = "nested public interface literal"
                        string materialXType = "color3"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/standard_surface.outputs:out>

                def Shader "standard_surface" (
                    customData = {
                        string materialXNodeDef = "ND_standard_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_standard_surface_surfaceshader"
                    float inputs:base = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base"
                        }
                    )
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root/Nested.inputs:tint>
                    float inputs:coat = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat"
                        }
                    )
                    float inputs:coat_affect_color = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Color"
                        }
                    )
                    float inputs:coat_affect_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Coat Affect Roughness"
                        }
                    )
                    float inputs:coat_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    vector3f inputs:coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Coat"
                            string uiname = "Coat normal"
                        }
                    )
                    float inputs:coat_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Rotation"
                        }
                    )
                    float inputs:coat_roughness = 0.1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
   )MXIF"
                             R"MXIF(                         string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Diffuse Roughness"
                        }
                    )
                    float inputs:emission = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Metalness"
                        }
                    )
                    vector3f inputs:normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    color3f inputs:opacity = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Geometry"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Opacity"
                        }
                    )
                    float inputs:sheen = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen"
                        }
                    )
                    color3f inputs:sheen_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Sheen Color"
                        }
                    )
                    float inputs:sheen_roughness = 0.3 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Sheen"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Sheen Roughness"
                        }
                    )
                    float inputs:specular = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular"
                        }
                    )
                    float inputs:specular_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_IOR = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_rotation = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Rotation"
                        }
                    )
                    float inputs:specular_roughness = 0.2 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness.connect = </Looks/Material/Root/Nested.inputs:roughness>
                    )MXIF"
                             R"MXIF(float inputs:subsurface = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface"
                        }
                    )
                    float inputs:subsurface_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    color3f inputs:subsurface_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    color3f inputs:subsurface_radius = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius"
                        }
                    )
                    float inputs:subsurface_scale = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
                            string uiname = "Subsurface Scale"
                            string uisoftmax = "10.0"
                        }
                    )
                    vector3f inputs:tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent Input"
                        }
                    )
                    float inputs:thin_film_IOR = 1.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "2000.0"
                        }
                    )
                    bool inputs:thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    float inputs:transmission = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_dispersion = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion"
                            string uisoftmax = "100.0"
                        }
                    )
                    float inputs:transmission_extra_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Roughness"
                            string uisoftmin = "0.0"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                         )MXIF"
                             R"MXIF(   string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)MXIF";
}  // namespace
TEST(materialx_nodegraph_interface, actual_public_import_fixture_lowers_without_source_mutation)
{
  EXPECT_EQ(util_sha256_string(retained_usda),
            "7783e3e38eb74e4df70b5db520e415b050038a0dd420ea8edd4b52fa135e121c");
  auto layer = pxr::SdfLayer::CreateAnonymous();
  ASSERT_TRUE(layer->ImportFromString(retained_usda));
  auto stage = pxr::UsdStage::Open(layer);
  auto material = pxr::UsdShadeMaterial(stage->GetPrimAtPath(pxr::SdfPath("/Looks/Material")));
  const string before = bytes(layer);
  materialx::Graph result;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &result, &error)) << error;
  ShaderGraph native;
  EXPECT_TRUE(materialx::lower(result, &native, &error)) << error;
  bool tint = false, roughness = false;
  for (const auto &n : result.nodes) {
    for (const auto &[name, v] : n.color3_inputs)
      tint |= v == make_float3(.2f, .4f, .6f);
    for (const auto &[name, v] : n.inputs)
      roughness |= v == .42f;
  }
  EXPECT_TRUE(tint && roughness);
  EXPECT_EQ(bytes(layer), before);
}
TEST(materialx_nodegraph_interface, typed_authored_literals_direct_chained_and_output_forwarded)
{
  for (int role : {0, 1, 2})
    for (bool legacy : {false, true})
      for (int route : {0, 1, 2}) {
        SCOPED_TRACE(std::to_string(role) + ":" + std::to_string(route));
        InterfaceFixture f(role, legacy);
        if (route) {
          auto upper = pxr::UsdShadeNodeGraph::Define(f.stage, pxr::SdfPath("/Material/Upper"));
          auto input = upper.CreateInput(pxr::TfToken("value"), f.type);
          input.GetAttr().Set(f.expected);
          f.value.GetAttr().Clear();
          f.value.ConnectToSource(input);
          if (route == 2) {
            auto output = f.graph.CreateOutput(pxr::TfToken("alias"), f.type);
            output.ConnectToSource(f.value);
            f.consumer.ConnectToSource(output);
          }
        }
        f.check(true);
      }
}
TEST(materialx_nodegraph_interface, direct_literal_values_survive_in_reader_ir)
{
  for (int role : {0, 1, 2}) {
    InterfaceFixture f(role);
    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
    bool found = false;
    for (const auto &n : graph.nodes) {
      if (role == 0)
        for (const auto &[name, v] : n.color3_inputs)
          found |= v == make_float3(.2f, .4f, .6f);
      if (role == 1)
        for (const auto &[name, v] : n.inputs)
          found |= v == .42f;
      if (role == 2)
        for (const auto &[name, v] : n.vector3_inputs)
          found |= v == make_float3(.2f, .4f, .6f);
    }
    EXPECT_TRUE(found);
  }
}
TEST(materialx_nodegraph_interface, consumed_missing_blocked_nonfinite_and_animated_values_reject)
{
  for (int role : {0, 1, 2})
    for (const string mode : {"unset", "blocked", "nonfinite", "time_only", "time_default"}) {
      SCOPED_TRACE(mode + ":" + std::to_string(role));
      InterfaceFixture f(role);
      auto a = f.value.GetAttr();
      if (mode == "unset")
        a.Clear();
      if (mode == "blocked")
        a.Block();
      if (mode == "nonfinite") {
        // Clear first: the bundled USD build can compare NaN vectors equal to
        // an existing default while deciding whether an authored Set changed.
        ASSERT_TRUE(a.Clear());
        ASSERT_TRUE(a.Set(
            role == 1 ? pxr::VtValue(std::numeric_limits<float>::infinity()) :
                        pxr::VtValue(pxr::GfVec3f(std::numeric_limits<float>::quiet_NaN()))));
      }
      if (mode == "nonfinite" && role != 1) {
        pxr::GfVec3f observed;
        ASSERT_TRUE(f.value.Get(&observed));
        ASSERT_FALSE(isfinite_safe(observed[0]))
            << "The rejection fixture must actually retain NaN";
      }
      if (mode == "time_only" || mode == "time_default") {
        if (mode == "time_only")
          a.Clear();
        a.Set(f.expected, pxr::UsdTimeCode(1));
      }
      f.check(false);
    }
}
TEST(materialx_nodegraph_interface, upstream_connection_shadows_local_default_block_and_animation)
{
  for (int role : {0, 1, 2})
    for (const string mode : {"blocked", "nonfinite", "time_only", "time_default"}) {
      InterfaceFixture f(role);
      auto a = f.value.GetAttr();
      if (mode == "blocked")
        a.Block();
      if (mode == "nonfinite") {
        // Clear first: the bundled USD build can compare NaN vectors equal to
        // an existing default while deciding whether an authored Set changed.
        ASSERT_TRUE(a.Clear());
        ASSERT_TRUE(a.Set(
            role == 1 ? pxr::VtValue(std::numeric_limits<float>::infinity()) :
                        pxr::VtValue(pxr::GfVec3f(std::numeric_limits<float>::quiet_NaN()))));
      }
      if (mode == "time_only" || mode == "time_default") {
        if (mode == "time_only")
          a.Clear();
        a.Set(f.expected, pxr::UsdTimeCode(1));
      }
      ASSERT_TRUE(f.value.ConnectToSource(f.constant()));
      f.check(true);
    }
}
TEST(materialx_nodegraph_interface, stronger_empty_connection_uses_composed_static_default)
{
  for (int role : {0, 1, 2}) {
    InterfaceFixture f(role);
    f.value.ConnectToSource(f.constant());
    f.stage->SetEditTarget(f.stage->GetSessionLayer());
    ASSERT_TRUE(f.value.DisconnectSource());
    ASSERT_TRUE(f.value.GetAttr().HasAuthoredConnections());
    f.check(true);
    f.value.GetAttr().Block();
    f.check(false);
  }
}
TEST(materialx_nodegraph_interface, malformed_connections_never_fall_back_to_valid_local_value)
{
  for (const string mode : {"dangling", "multiple", "cycle", "role"}) {
    SCOPED_TRACE(mode);
    InterfaceFixture f;
    if (mode == "dangling")
      f.value.GetAttr().SetConnections({pxr::SdfPath("/Material/Missing.outputs:out")});
    if (mode == "multiple")
      f.value.GetAttr().SetConnections(
          {f.constant().GetAttr().GetPath(), pxr::SdfPath("/Material/Missing.outputs:out")});
    if (mode == "cycle")
      f.value.ConnectToSource(f.value);
    if (mode == "role") {
      auto v = f.graph.CreateInput(pxr::TfToken("vector"), pxr::SdfValueTypeNames->Vector3f);
      v.Set(pxr::GfVec3f(.2f, .4f, .6f));
      f.consumer.GetAttr().SetConnections({v.GetAttr().GetPath()});
    }
    f.check(false);
  }
}
TEST(materialx_nodegraph_interface, depth_budget_remains_fail_closed)
{
  InterfaceFixture f;
  auto current = f.value;
  for (int i = 0; i < 70; ++i) {
    auto next = f.graph.CreateInput(pxr::TfToken("chain" + std::to_string(i)), f.type);
    current.ConnectToSource(next);
    current = next;
  }
  current.GetAttr().Set(f.expected);
  f.check(false);
}
TEST(materialx_nodegraph_interface, interface_literal_through_identity_dot_is_static_and_typed)
{
  for (int role : {0, 1, 2}) {
    InterfaceFixture f(role);
    auto d = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/Dot"));
    d.CreateIdAttr(pxr::VtValue(pxr::TfToken(role == 0 ? "ND_dot_color3" :
                                             role == 1 ? "ND_dot_float" :
                                                         "ND_dot_vector3")));
    d.CreateInput(pxr::TfToken("in"), f.type).ConnectToSource(f.value);
    f.consumer.ConnectToSource(d.CreateOutput(pxr::TfToken("out"), f.type));
    f.check(true);
    f.value.GetAttr().Set(f.expected, pxr::UsdTimeCode(1));
    f.check(false);
  }
}

TEST(materialx_nodegraph_interface, all_supported_numeric_interface_roles_reach_real_consumers)
{
  struct Case {
    pxr::SdfValueTypeName type;
    const char *converter;
    pxr::VtValue value;
    bool scalar;
  };
  const Case cases[] = {
      {pxr::SdfValueTypeNames->Bool, "ND_convert_boolean_float", pxr::VtValue(true), true},
      {pxr::SdfValueTypeNames->Int, "ND_convert_integer_float", pxr::VtValue(1), true},
      {pxr::SdfValueTypeNames->Float2,
       "ND_convert_vector2_color3",
       pxr::VtValue(pxr::GfVec2f(.2f, .4f)),
       false},
      {pxr::SdfValueTypeNames->Color4f,
       "ND_convert_color4_color3",
       pxr::VtValue(pxr::GfVec4f(.2f, .4f, .6f, 1.f)),
       false},
      {pxr::SdfValueTypeNames->Float4,
       "ND_convert_vector4_color3",
       pxr::VtValue(pxr::GfVec4f(.2f, .4f, .6f, 1.f)),
       false},
      {pxr::SdfValueTypeNames->Vector3f,
       "ND_convert_vector3_color3",
       pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)),
       false},
      {pxr::SdfValueTypeNames->Float3,
       "ND_convert_vector3_color3",
       pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)),
       false},
  };
  for (const Case &c : cases) {
    SCOPED_TRACE(c.converter);
    InterfaceFixture f(c.scalar ? 1 : 0);
    ASSERT_TRUE(f.graph.GetPrim().RemoveProperty(pxr::TfToken("inputs:value")));
    f.value = f.graph.CreateInput(pxr::TfToken("value"), c.type);
    f.value.GetAttr().Set(c.value);
    auto converter = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/Convert"));
    converter.CreateIdAttr(pxr::VtValue(pxr::TfToken(c.converter)));
    auto in = converter.CreateInput(pxr::TfToken("in"), c.type);
    ASSERT_TRUE(in.ConnectToSource(f.value));
    ASSERT_TRUE(f.consumer.ConnectToSource(converter.CreateOutput(pxr::TfToken("out"), f.type)));
    f.check(true);
    f.value.GetAttr().Set(c.value, pxr::UsdTimeCode(1));
    f.check(false);
    f.value.GetAttr().Clear();
    f.value.GetAttr().Block();
    f.check(false);
    f.value.GetAttr().Clear();
    f.value.GetAttr().Set(c.value);
    auto wrong = f.graph.CreateInput(pxr::TfToken("wrong"), pxr::SdfValueTypeNames->Color3f);
    wrong.Set(pxr::GfVec3f(.2f, .4f, .6f));
    in.GetAttr().SetConnections({wrong.GetAttr().GetPath()});
    f.check(false);
  }
}

/* Typed NodeGraph output literals exercise the production reader and lowerer. */
namespace {
struct OutputLiteralCase {
  pxr::SdfValueTypeName type;
  const char *converter;
  const char *dot;
  pxr::VtValue value;
  bool scalar;
};
vector<OutputLiteralCase> output_literal_cases()
{
  return {
      {pxr::SdfValueTypeNames->Float, nullptr, "ND_dot_float", pxr::VtValue(.61f), true},
      {pxr::SdfValueTypeNames->Color3f,
       nullptr,
       "ND_dot_color3",
       pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)),
       false},
      {pxr::SdfValueTypeNames->Bool,
       "ND_convert_boolean_float",
       "ND_dot_boolean",
       pxr::VtValue(true),
       true},
      {pxr::SdfValueTypeNames->Int,
       "ND_convert_integer_float",
       "ND_dot_integer",
       pxr::VtValue(7),
       true},
      {pxr::SdfValueTypeNames->Float2,
       "ND_convert_vector2_color3",
       "ND_dot_vector2",
       pxr::VtValue(pxr::GfVec2f(.2f, .4f)),
       false},
      {pxr::SdfValueTypeNames->Color4f,
       "ND_convert_color4_color3",
       "ND_dot_color4",
       pxr::VtValue(pxr::GfVec4f(.2f, .4f, .6f, .8f)),
       false},
      {pxr::SdfValueTypeNames->Vector3f,
       "ND_convert_vector3_color3",
       "ND_dot_vector3",
       pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)),
       false},
      {pxr::SdfValueTypeNames->Float3,
       "ND_convert_vector3_color3",
       "ND_dot_vector3",
       pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)),
       false},
      {pxr::SdfValueTypeNames->Float4,
       "ND_convert_vector4_color3",
       "ND_dot_vector4",
       pxr::VtValue(pxr::GfVec4f(.2f, .4f, .6f, .8f)),
       false},
  };
}

pxr::UsdShadeInput output_literal_consumer(InterfaceFixture &f, const OutputLiteralCase &c)
{
  if (!c.converter) {
    return f.consumer;
  }
  auto converter = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/OutputConvert"));
  converter.CreateIdAttr(pxr::VtValue(pxr::TfToken(c.converter)));
  auto input = converter.CreateInput(pxr::TfToken("in"), c.type);
  EXPECT_TRUE(f.consumer.ConnectToSource(converter.CreateOutput(pxr::TfToken("out"), f.type)));
  return input;
}

void expect_output_value(const materialx::Node &n, const OutputLiteralCase &c)
{
  EXPECT_EQ(n.nodedef, c.dot);
  if (c.type == pxr::SdfValueTypeNames->Float) {
    ASSERT_EQ(n.inputs.count("in"), 1);
    EXPECT_FLOAT_EQ(n.inputs.at("in"), c.value.Get<float>());
  }
  else if (c.type == pxr::SdfValueTypeNames->Bool || c.type == pxr::SdfValueTypeNames->Int) {
    ASSERT_EQ(n.int_inputs.count("in"), 1);
    EXPECT_EQ(n.int_inputs.at("in"),
              c.type == pxr::SdfValueTypeNames->Bool ? int(c.value.Get<bool>()) :
                                                       c.value.Get<int>());
  }
  else if (c.type == pxr::SdfValueTypeNames->Float2) {
    ASSERT_EQ(n.vector2_inputs.count("in"), 1);
    const float2 actual = n.vector2_inputs.at("in");
    const auto expected = c.value.Get<pxr::GfVec2f>();
    EXPECT_FLOAT_EQ(actual.x, expected[0]);
    EXPECT_FLOAT_EQ(actual.y, expected[1]);
  }
  else if (c.type == pxr::SdfValueTypeNames->Color4f || c.type == pxr::SdfValueTypeNames->Float4) {
    const auto &values = c.type == pxr::SdfValueTypeNames->Color4f ? n.float4_inputs :
                                                                     n.vector4_inputs;
    ASSERT_EQ(values.count("in"), 1);
    const float4 actual = values.at("in");
    const auto expected = c.value.Get<pxr::GfVec4f>();
    EXPECT_FLOAT_EQ(actual.x, expected[0]);
    EXPECT_FLOAT_EQ(actual.y, expected[1]);
    EXPECT_FLOAT_EQ(actual.z, expected[2]);
    EXPECT_FLOAT_EQ(actual.w, expected[3]);
  }
  else {
    const auto &values = c.type == pxr::SdfValueTypeNames->Color3f ? n.color3_inputs :
                                                                     n.vector3_inputs;
    ASSERT_EQ(values.count("in"), 1);
    const float3 actual = values.at("in");
    const auto expected = c.value.Get<pxr::GfVec3f>();
    EXPECT_FLOAT_EQ(actual.x, expected[0]);
    EXPECT_FLOAT_EQ(actual.y, expected[1]);
    EXPECT_FLOAT_EQ(actual.z, expected[2]);
  }
}

void expect_output_literal_rejected(InterfaceFixture &f, const string &message = "")
{
  const string root_before = bytes(f.stage->GetRootLayer()),
               session_before = bytes(f.stage->GetSessionLayer());
  materialx::Graph graph;
  materialx::Node sentinel;
  sentinel.name = "preserved-output";
  sentinel.nodedef = "preserved-nodedef";
  sentinel.inputs["pin"] = 123.f;
  graph.nodes.push_back(sentinel);
  graph.has_displacement = true;
  graph.displacement.value = 9.f;
  string error;
  EXPECT_FALSE(materialx::read_usdshade_graph(f.material, &graph, &error));
  EXPECT_FALSE(error.empty());
  if (!message.empty())
    EXPECT_NE(error.find(message), string::npos) << error;
  ASSERT_EQ(graph.nodes.size(), 1);
  EXPECT_EQ(graph.nodes[0].name, "preserved-output");
  EXPECT_EQ(graph.nodes[0].nodedef, "preserved-nodedef");
  EXPECT_EQ(graph.nodes[0].inputs.at("pin"), 123.f);
  EXPECT_TRUE(graph.has_displacement);
  EXPECT_EQ(graph.displacement.value, 9.f);
  EXPECT_EQ(bytes(f.stage->GetRootLayer()), root_before);
  EXPECT_EQ(bytes(f.stage->GetSessionLayer()), session_before);
}
}  // namespace

TEST(materialx_nodegraph_interface, output_numeric_defaults_survive_typed_consumers_and_forwarding)
{
  for (const auto &c : output_literal_cases()) {
    for (int route : {0, 1, 2, 3}) {
      SCOPED_TRACE(string(c.dot) + ":" + std::to_string(route));
      InterfaceFixture f(c.scalar ? 1 : 0);
      auto out = f.graph.CreateOutput(pxr::TfToken("literal"), c.type);
      ASSERT_TRUE(out.GetAttr().Set(c.value));
      auto consumer = output_literal_consumer(f, c);
      if (route == 0)
        ASSERT_TRUE(consumer.ConnectToSource(out));
      if (route == 1) {
        auto alias = f.graph.CreateOutput(pxr::TfToken("alias"), c.type);
        ASSERT_TRUE(alias.ConnectToSource(out));
        ASSERT_TRUE(consumer.ConnectToSource(alias));
      }
      if (route == 2) {
        auto alias = f.graph.CreateInput(pxr::TfToken("alias"), c.type);
        ASSERT_TRUE(alias.ConnectToSource(out));
        ASSERT_TRUE(consumer.ConnectToSource(alias));
      }
      if (route == 3) {
        auto dot = pxr::UsdShadeShader::Define(f.stage, pxr::SdfPath("/Material/ExplicitDot"));
        dot.CreateIdAttr(pxr::VtValue(pxr::TfToken(c.dot)));
        ASSERT_TRUE(dot.CreateInput(pxr::TfToken("in"), c.type).ConnectToSource(out));
        ASSERT_TRUE(consumer.ConnectToSource(dot.CreateOutput(pxr::TfToken("out"), c.type)));
      }
      const string root_before = bytes(f.stage->GetRootLayer()),
                   session_before = bytes(f.stage->GetSessionLayer());
      materialx::Graph graph;
      string error;
      ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
      int found = 0;
      for (const auto &node : graph.nodes)
        if (node.nodedef == c.dot) {
          expect_output_value(node, c);
          ++found;
        }
      EXPECT_EQ(found, 1);
      ShaderGraph native;
      EXPECT_TRUE(materialx::lower(graph, &native, &error)) << error;
      EXPECT_EQ(bytes(f.stage->GetRootLayer()), root_before);
      EXPECT_EQ(bytes(f.stage->GetSessionLayer()), session_before);
    }
  }
}

TEST(materialx_nodegraph_interface,
     output_consumed_missing_blocked_animated_nonfinite_values_reject_transactionally)
{
  for (int role : {0, 1, 2})
    for (const string mode : {"unset", "block", "time_only", "time_default", "nonfinite"}) {
      SCOPED_TRACE(mode + ":" + std::to_string(role));
      InterfaceFixture f(role);
      auto out = f.graph.CreateOutput(pxr::TfToken("literal"), f.type);
      ASSERT_TRUE(out.GetAttr().Set(f.expected));
      ASSERT_TRUE(f.consumer.ConnectToSource(out));
      auto attr = out.GetAttr();
      if (mode == "unset" || mode == "time_only")
        ASSERT_TRUE(attr.Clear());
      if (mode == "block")
        attr.Block();
      if (mode == "time_only" || mode == "time_default")
        ASSERT_TRUE(attr.Set(f.expected, pxr::UsdTimeCode(1)));
      if (mode == "nonfinite") {
        ASSERT_TRUE(attr.Clear());
        ASSERT_TRUE(attr.Set(
            role == 1 ? pxr::VtValue(std::numeric_limits<float>::infinity()) :
                        pxr::VtValue(pxr::GfVec3f(std::numeric_limits<float>::quiet_NaN()))));
        if (role != 1) {
          pxr::GfVec3f observed;
          ASSERT_TRUE(attr.Get(&observed));
          ASSERT_FALSE(isfinite_safe(observed[0]));
        }
      }
      expect_output_literal_rejected(f);
    }
}

TEST(materialx_nodegraph_interface, output_connected_source_shadows_local_bad_values)
{
  for (int role : {0, 1, 2})
    for (const string mode : {"block", "time_only", "time_default", "nonfinite"}) {
      InterfaceFixture f(role);
      auto out = f.graph.CreateOutput(pxr::TfToken("literal"), f.type);
      auto attr = out.GetAttr();
      ASSERT_TRUE(attr.Set(f.expected));
      ASSERT_TRUE(f.consumer.ConnectToSource(out));
      if (mode == "block")
        attr.Block();
      if (mode == "time_only")
        ASSERT_TRUE(attr.Clear());
      if (mode == "time_only" || mode == "time_default")
        ASSERT_TRUE(attr.Set(f.expected, pxr::UsdTimeCode(1)));
      if (mode == "nonfinite") {
        ASSERT_TRUE(attr.Clear());
        ASSERT_TRUE(attr.Set(
            role == 1 ? pxr::VtValue(std::numeric_limits<float>::infinity()) :
                        pxr::VtValue(pxr::GfVec3f(std::numeric_limits<float>::quiet_NaN()))));
      }
      ASSERT_TRUE(out.ConnectToSource(f.constant()));
      f.check(true);
    }
}

TEST(materialx_nodegraph_interface,
     output_stronger_disconnect_uses_static_default_then_rejects_block)
{
  for (int role : {0, 1, 2}) {
    InterfaceFixture f(role);
    auto out = f.graph.CreateOutput(pxr::TfToken("literal"), f.type);
    ASSERT_TRUE(out.GetAttr().Set(f.expected));
    ASSERT_TRUE(out.ConnectToSource(f.constant()));
    ASSERT_TRUE(f.consumer.ConnectToSource(out));
    f.stage->SetEditTarget(f.stage->GetSessionLayer());
    ASSERT_TRUE(out.DisconnectSource());
    pxr::SdfPathVector paths;
    ASSERT_TRUE(out.GetAttr().GetConnections(&paths));
    ASSERT_TRUE(paths.empty());
    ASSERT_TRUE(out.GetAttr().HasAuthoredConnections());
    f.check(true);
    out.GetAttr().Block();
    expect_output_literal_rejected(f);
  }
}

TEST(materialx_nodegraph_interface, output_malformed_edges_do_not_fall_back_to_authored_literal)
{
  for (int role : {0, 1, 2})
    for (const string mode : {"dangling",
                              "valid_invalid",
                              "two_valid",
                              "self_cycle",
                              "output_cycle",
                              "input_cycle",
                              "wrong_role"})
    {
      SCOPED_TRACE(mode + ":" + std::to_string(role));
      InterfaceFixture f(role);
      auto out = f.graph.CreateOutput(pxr::TfToken("literal"), f.type);
      ASSERT_TRUE(out.GetAttr().Set(f.expected));
      ASSERT_TRUE(f.consumer.ConnectToSource(out));
      if (mode == "dangling")
        out.GetAttr().SetConnections({pxr::SdfPath("/Material/Missing.outputs:out")});
      if (mode == "valid_invalid")
        out.GetAttr().SetConnections(
            {f.constant().GetAttr().GetPath(), pxr::SdfPath("/Material/Missing.outputs:out")});
      if (mode == "two_valid") {
        auto alias = f.graph.CreateInput(pxr::TfToken("other"), f.type);
        alias.GetAttr().Set(f.expected);
        out.GetAttr().SetConnections(
            {f.constant().GetAttr().GetPath(), alias.GetAttr().GetPath()});
      }
      if (mode == "self_cycle")
        ASSERT_TRUE(out.ConnectToSource(out));
      if (mode == "output_cycle") {
        auto alias = f.graph.CreateOutput(pxr::TfToken("other"), f.type);
        ASSERT_TRUE(alias.ConnectToSource(out));
        ASSERT_TRUE(out.ConnectToSource(alias));
      }
      if (mode == "input_cycle") {
        ASSERT_TRUE(f.value.ConnectToSource(out));
        ASSERT_TRUE(out.ConnectToSource(f.value));
      }
      if (mode == "wrong_role") {
        auto wrong = f.graph.CreateOutput(pxr::TfToken("wrong"),
                                          role == 2 ? pxr::SdfValueTypeNames->Color3f :
                                                      pxr::SdfValueTypeNames->Vector3f);
        wrong.GetAttr().Set(pxr::GfVec3f(.2f, .4f, .6f));
        ASSERT_TRUE(out.ConnectToSource(wrong));
      }
      expect_output_literal_rejected(f);
    }
}

TEST(materialx_nodegraph_interface,
     output_literal_never_substitutes_for_shader_or_closure_terminal)
{
  InterfaceFixture f;
  auto terminal = f.graph.CreateOutput(pxr::TfToken("closure"), pxr::SdfValueTypeNames->Token);
  ASSERT_TRUE(terminal.GetAttr().Set(pxr::TfToken("surfaceshader")));
  ASSERT_TRUE(f.material.GetSurfaceOutput(pxr::TfToken("mtlx")).ConnectToSource(terminal));
  expect_output_literal_rejected(f, "must connect exactly one source");
  InterfaceFixture numeric(1);
  auto shader = pxr::UsdShadeShader::Define(numeric.stage, pxr::SdfPath("/Material/Unknown"));
  shader.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_unregistered_float")));
  auto value = shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Float);
  ASSERT_TRUE(value.GetAttr().Set(.61f));
  ASSERT_TRUE(numeric.consumer.ConnectToSource(value));
  expect_output_literal_rejected(numeric);
}

TEST(materialx_nodegraph_interface, output_chain_depth_remains_bounded)
{
  InterfaceFixture f(1);
  auto first = f.graph.CreateOutput(pxr::TfToken("first"), f.type);
  auto previous = first;
  for (int i = 0; i < 70; ++i) {
    auto next = f.graph.CreateOutput(pxr::TfToken("chain" + std::to_string(i)), f.type);
    ASSERT_TRUE(previous.ConnectToSource(next));
    previous = next;
  }
  ASSERT_TRUE(previous.GetAttr().Set(f.expected));
  ASSERT_TRUE(f.consumer.ConnectToSource(first));
  expect_output_literal_rejected(f, "maximum depth");
}

TEST(materialx_nodegraph_interface, output_actual_public_canonical_float_fixture)
{
  string text;
  text += R"OUTPUT_FIXTURE(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            float inputs:in = 0.42 (
                customData = {
                    string audit_note = "retain typed Root interface"
                    string materialXType = "float"
                }
            )
            float outputs:audit = 0.75 (
                customData = {
                    string keep = "original"
                    string materialXType = "float"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                float inputs:amount = 0.5 (
                    customData = {
                        string materialXType = "float"
                    }
                )
                color3f inputs:tint (
                    customData = {
                        string materialXType = "color3"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/open_pbr_surface.outputs:out>

                def Shader "open_pbr_surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminance"
                            string uisoftmax = "1000.0"
                        }
                    )
)OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness.connect = </Looks/Material/Root.outputs:audit>
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
         )OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(                   string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
                )OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(    )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)OUTPUT_FIXTURE";
  EXPECT_EQ(util_sha256_string(text),
            "e673bc4c09e6939fce172e7d7aed56e6cfe19946b7615620cda110d5434f3792");
  auto layer = pxr::SdfLayer::CreateAnonymous();
  ASSERT_TRUE(layer->ImportFromString(text));
  auto stage = pxr::UsdStage::Open(layer);
  const string before = bytes(layer);
  auto material = pxr::UsdShadeMaterial(stage->GetPrimAtPath(pxr::SdfPath("/Looks/Material")));
  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  bool found = false;
  for (const auto &n : graph.nodes)
    if (n.nodedef == "ND_dot_float" && n.inputs.count("in")) {
      EXPECT_FLOAT_EQ(n.inputs.at("in"), .75f);
      found = true;
    }
  EXPECT_TRUE(found);
  ShaderGraph native;
  EXPECT_TRUE(materialx::lower(graph, &native, &error)) << error;
  EXPECT_EQ(bytes(layer), before);
}

TEST(materialx_nodegraph_interface, output_actual_public_canonical_color3_fixture)
{
  string text;
  text += R"OUTPUT_FIXTURE(#usda 1.0

def Scope "Looks"
{
    def Material "Material"
    {
        token outputs:mtlx:surface.connect = </Looks/Material/Root.outputs:surface>

        def NodeGraph "Root"
        {
            float inputs:in = 0.42 (
                customData = {
                    string audit_note = "retain typed Root interface"
                    string materialXType = "float"
                }
            )
            color3f outputs:audit = (0.7, 0.6, 0.5) (
                customData = {
                    string keep = "original"
                    string materialXType = "color3"
                }
            )
            token outputs:surface.connect = </Looks/Material/Root/Nested.outputs:surface>

            def NodeGraph "Nested"
            {
                float inputs:amount = 0.5 (
                    customData = {
                        string materialXType = "float"
                    }
                )
                color3f inputs:tint (
                    customData = {
                        string materialXType = "color3"
                    }
                )
                token outputs:surface.connect = </Looks/Material/Root/Nested/open_pbr_surface.outputs:out>

                def Shader "open_pbr_surface" (
                    customData = {
                        string materialXNodeDef = "ND_open_pbr_surface_surfaceshader"
                    }
                )
                {
                    uniform token info:id = "ND_open_pbr_surface_surfaceshader"
                    color3f inputs:base_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Base Color"
                        }
                    )
                    color3f inputs:base_color.connect = </Looks/Material/Root.outputs:audit>
                    float inputs:base_diffuse_roughness = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Diffuse Roughness"
                        }
                    )
                    float inputs:base_metalness = 0 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Metalness"
                        }
                    )
                    float inputs:base_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Base"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Base Weight"
                        }
                    )
                    color3f inputs:coat_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Coat Color"
                        }
                    )
                    float inputs:coat_darkening = 1 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Darkening"
                        }
                    )
                    float inputs:coat_ior = 1.6 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimin = "0.0"
                            string uiname = "Coat Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:coat_roughness = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Roughness"
                        }
                    )
                    float inputs:coat_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Anisotropy"
                        }
                    )
                    float inputs:coat_weight = 0 (
                        sdrMetadata = {
                            string uifolder = "Coat"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Coat Weight"
                        }
                    )
                    color3f inputs:emission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Emission Color"
                        }
                    )
                    float inputs:emission_luminance = 0 (
                        sdrMetadata = {
                            string uifolder = "Emission"
                            string uimin = "0.0"
                            string uiname = "Emission Luminanc)OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(e"
                            string uisoftmax = "1000.0"
                        }
                    )
                    color3f inputs:fuzz_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Fuzz Color"
                        }
                    )
                    float inputs:fuzz_roughness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Roughness"
                        }
                    )
                    float inputs:fuzz_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Fuzz"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Fuzz Weight"
                        }
                    )
                    vector3f inputs:geometry_coat_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Normal"
                        }
                    )
                    vector3f inputs:geometry_coat_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Coat Tangent"
                        }
                    )
                    vector3f inputs:geometry_normal (
                        sdrMetadata = {
                            string defaultgeomprop = "Nworld"
                            string uifolder = "Geometry"
                            string uiname = "Normal"
                        }
                    )
                    float inputs:geometry_opacity = 1 (
                        sdrMetadata = {
                            string hint = "opacity"
                            string uifolder = "Geometry"
                            string uimax = "1"
                            string uimin = "0"
                            string uiname = "Opacity"
                        }
                    )
                    vector3f inputs:geometry_tangent (
                        sdrMetadata = {
                            string defaultgeomprop = "Tworld"
                            string uifolder = "Geometry"
                            string uiname = "Tangent"
                        }
                    )
                    bool inputs:geometry_thin_walled = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Geometry"
                            string uiname = "Thin Walled"
                        }
                    )
                    color3f inputs:specular_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Specular Color"
                        }
                    )
                    float inputs:specular_ior = 1.5 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:specular_roughness = 0.3 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Roughness"
                        }
                    )
                    float inputs:specular_roughness_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Specular"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Specular Anisotropy"
                        }
                    )
                    float inputs:specular_weight = 1 (
                        sdrMetadata = {
                            string uifolder = "Specular"
                            string uimin = "0.0"
                            string uiname = "Specular Weight"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_color = (0.8, 0.8, 0.8) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Color"
                        }
                    )
                    float inputs:subsurface_radius = 1 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimin = "0.0"
 )OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(                           string uiname = "Subsurface Radius"
                            string uisoftmax = "1.0"
                        }
                    )
                    color3f inputs:subsurface_radius_scale = (1, 0.5, 0.25) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Subsurface Radius Scale"
                        }
                    )
                    float inputs:subsurface_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Subsurface Anisotropy"
                        }
                    )
                    float inputs:subsurface_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Subsurface"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Subsurface Weight"
                        }
                    )
                    float inputs:thin_film_ior = 1.4 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Index of Refraction"
                            string uisoftmax = "3.0"
                            string uisoftmin = "1.0"
                        }
                    )
                    float inputs:thin_film_thickness = 0.5 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimin = "0.0"
                            string uiname = "Thin Film Thickness"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:thin_film_weight = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Thin Film"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Thin Film Weight"
                        }
                    )
                    color3f inputs:transmission_color = (1, 1, 1) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Color"
                        }
                    )
                    float inputs:transmission_depth = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Depth"
                            string uisoftmax = "1.0"
                        }
                    )
                    float inputs:transmission_dispersion_abbe_number = 20 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Abbe Number"
                            string uisoftmax = "91.0"
                            string uisoftmin = "9.0"
                        }
                    )
                    float inputs:transmission_dispersion_scale = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Dispersion Scale"
                        }
                    )
                    color3f inputs:transmission_scatter = (0, 0, 0) (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1,1,1"
                            string uimin = "0,0,0"
                            string uiname = "Transmission Scatter"
                        }
                    )
                    float inputs:transmission_scatter_anisotropy = 0 (
                        sdrMetadata = {
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "-1.0"
                            string uiname = "Transmission Anisotropy"
                        }
                    )
                    float inputs:transmission_weight = 0 (
                        sdrMetadata = {
                            string hint = "transparency"
                            string uiadvanced = "true"
                            string uifolder = "Transmission"
                            string uimax = "1.0"
                            string uimin = "0.0"
                            string uiname = "Transmission Weight"
                        }
        )OUTPUT_FIXTURE";
  text += R"OUTPUT_FIXTURE(            )
                    token outputs:out
                }
            }
        }
    }
}

def "World"
{
    def Mesh "BindingTarget" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        rel material:binding = </Looks/Material>
    }
}

)OUTPUT_FIXTURE";
  EXPECT_EQ(util_sha256_string(text),
            "35456b0dc6a28874ea0f73170623fab98759bfe057711d1ee85e0e8efe3c1f1b");
  auto layer = pxr::SdfLayer::CreateAnonymous();
  ASSERT_TRUE(layer->ImportFromString(text));
  auto stage = pxr::UsdStage::Open(layer);
  const string before = bytes(layer);
  auto material = pxr::UsdShadeMaterial(stage->GetPrimAtPath(pxr::SdfPath("/Looks/Material")));
  materialx::Graph graph;
  string error;
  ASSERT_TRUE(materialx::read_usdshade_graph(material, &graph, &error)) << error;
  bool found = false;
  for (const auto &n : graph.nodes)
    if (n.nodedef == "ND_dot_color3" && n.color3_inputs.count("in")) {
      const auto v = n.color3_inputs.at("in");
      EXPECT_FLOAT_EQ(v.x, .7f);
      EXPECT_FLOAT_EQ(v.y, .6f);
      EXPECT_FLOAT_EQ(v.z, .5f);
      found = true;
    }
  EXPECT_TRUE(found);
  ShaderGraph native;
  EXPECT_TRUE(materialx::lower(graph, &native, &error)) << error;
  EXPECT_EQ(bytes(layer), before);
}

TEST(materialx_nodegraph_interface, numeric_literal_namespaces_preserved_for_inputs_and_outputs)
{
  for (const bool output : {false, true}) {
    InterfaceFixture f(1);
    auto attribute = output ?
                         f.graph.CreateOutput(pxr::TfToken("channel:value"), f.type).GetAttr() :
                         f.graph.CreateInput(pxr::TfToken("channel:value"), f.type).GetAttr();
    ASSERT_TRUE(attribute.Set(.61f));
    ASSERT_TRUE(f.consumer.GetAttr().SetConnections({attribute.GetPath()}));
    materialx::Graph graph;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(f.material, &graph, &error)) << error;
    bool found = false;
    for (const auto &node : graph.nodes) {
      if (node.nodedef == "ND_dot_float") {
        EXPECT_EQ(node.name, "G_channel:value");
        ASSERT_EQ(node.inputs.count("in"), 1);
        EXPECT_FLOAT_EQ(node.inputs.at("in"), .61f);
        found = true;
      }
    }
    EXPECT_TRUE(found);
    ShaderGraph native;
    EXPECT_TRUE(materialx::lower(graph, &native, &error)) << error;
  }
}

CCL_NAMESPACE_END
