/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>

#include "materialx/graph.h"
#include "materialx/usdshade_reader.h"

CCL_NAMESPACE_BEGIN
namespace {
struct AnimatedConstantFixture {
  pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
  pxr::UsdShadeMaterial material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Material"));
  pxr::UsdShadeShader surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/S"));
  pxr::UsdShadeInput source_value, destination;
  pxr::VtValue value, other;
  AnimatedConstantFixture(bool legacy, bool color, bool indirect = false, bool connected = true)
  {
    const auto type = color ? pxr::SdfValueTypeNames->Color3f : pxr::SdfValueTypeNames->Float;
    value = color ? pxr::VtValue(pxr::GfVec3f(.2f, .4f, .6f)) : pxr::VtValue(.375f);
    other = color ? pxr::VtValue(pxr::GfVec3f(.7f, .5f, .3f)) : pxr::VtValue(.875f);
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken(
        legacy ? "ND_standard_surface_surfaceshader_100" : "ND_standard_surface_surfaceshader")));
    auto out = surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    material.CreateSurfaceOutput(pxr::TfToken("mtlx")).ConnectToSource(out);
    surface.CreateInput(pxr::TfToken("specular"), pxr::SdfValueTypeNames->Float).Set(1.f);
    auto constant = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/C"));
    constant.CreateIdAttr(
        pxr::VtValue(pxr::TfToken(color ? "ND_constant_color3" : "ND_constant_float")));
    source_value = constant.CreateInput(pxr::TfToken("value"), type);
    source_value.GetAttr().Set(value);
    auto endpoint = constant.CreateOutput(pxr::TfToken("out"), type);
    if (indirect) {
      auto mid = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/M"));
      mid.CreateIdAttr(
          pxr::VtValue(pxr::TfToken(color ? "ND_multiply_color3" : "ND_multiply_float")));
      mid.CreateInput(pxr::TfToken("in1"), type).ConnectToSource(endpoint);
      mid.CreateInput(pxr::TfToken("in2"), type)
          .GetAttr()
          .Set(color ? pxr::VtValue(pxr::GfVec3f(1.f)) : pxr::VtValue(1.f));
      endpoint = mid.CreateOutput(pxr::TfToken("out"), type);
    }
    destination = surface.CreateInput(pxr::TfToken(color ? "base_color" : "base"), type);
    if (connected)
      destination.ConnectToSource(endpoint);
    else
      destination.GetAttr().Set(value);
  }
  void animate(pxr::UsdAttribute attr, bool default_value)
  {
    ASSERT_TRUE(attr.Clear());
    ASSERT_TRUE(attr.Set(value, pxr::UsdTimeCode(1)));
    ASSERT_TRUE(attr.Set(other, pxr::UsdTimeCode(2)));
    if (default_value)
      ASSERT_TRUE(attr.Set(value));
  }
};
string bytes(const pxr::SdfLayerHandle &layer)
{
  string s;
  EXPECT_TRUE(layer->ExportToString(&s));
  return s;
}
void read_expected(AnimatedConstantFixture &f, bool accepted)
{
  const string before = bytes(f.stage->GetRootLayer()),
               session = bytes(f.stage->GetSessionLayer());
  materialx::Graph parsed;
  parsed.nodes.push_back({"sentinel", "unsupported"});
  parsed.has_displacement = true;
  parsed.displacement.value = .125f;
  string error;
  EXPECT_EQ(materialx::read_usdshade_graph(f.material, &parsed, &error), accepted) << error;
  if (!accepted) {
    EXPECT_FALSE(error.empty());
    ASSERT_EQ(parsed.nodes.size(), 1u);
    EXPECT_EQ(parsed.nodes[0].name, "sentinel");
    EXPECT_TRUE(parsed.has_displacement);
    EXPECT_FLOAT_EQ(parsed.displacement.value, .125f);
  }
  EXPECT_EQ(bytes(f.stage->GetRootLayer()), before);
  EXPECT_EQ(bytes(f.stage->GetSessionLayer()), session);
}
}  // namespace
TEST(materialx_usdshade_reader, animated_consumed_constant_rejects_without_changing_authority)
{
  for (bool legacy : {false, true})
    for (bool color : {false, true})
      for (bool indirect : {false, true})
        for (bool has_default : {false, true}) {
          SCOPED_TRACE(string(legacy ? "100" : "101") + (color ? ":color" : ":float") +
                       (indirect ? ":math" : ":direct") +
                       (has_default ? ":default+times" : ":times-only"));
          AnimatedConstantFixture f(legacy, color, indirect);
          f.animate(f.source_value.GetAttr(), has_default);
          read_expected(f, false);
        }
}
TEST(materialx_usdshade_reader, static_constant_direct_and_folded_controls)
{
  for (bool legacy : {false, true})
    for (bool color : {false, true})
      for (bool indirect : {false, true}) {
        AnimatedConstantFixture f(legacy, color, indirect);
        read_expected(f, true);
      }
}
TEST(materialx_usdshade_reader, shadowed_destination_opinions_and_unused_constant_do_not_reject)
{
  for (bool legacy : {false, true})
    for (bool color : {false, true}) {
      for (int mode = 0; mode < 3; ++mode) {
        AnimatedConstantFixture f(legacy, color);
        if (mode == 2)
          f.destination.GetAttr().Block();
        else
          f.animate(f.destination.GetAttr(), mode == 1);
        read_expected(f, true);
      }
      AnimatedConstantFixture unused(legacy, color, false, false);
      unused.animate(unused.source_value.GetAttr(), true);
      read_expected(unused, true);
    }
}
CCL_NAMESPACE_END
