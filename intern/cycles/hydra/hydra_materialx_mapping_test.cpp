/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <string>
#include <vector>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
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

HdContainerDataSourceHandle vector2_parameter(const pxr::GfVec2f &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<pxr::GfVec2f>::New(value))
      .Build();
}

HdContainerDataSourceHandle vector3_parameter(const pxr::GfVec3f &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<pxr::GfVec3f>::New(value))
      .Build();
}

HdContainerDataSourceHandle vector4_parameter(const pxr::GfVec4f &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<pxr::GfVec4f>::New(value))
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

HdContainerDataSourceHandle string_parameter(const std::string &value)
{
  return HdMaterialNodeParameterSchema::Builder()
      .SetValue(HdRetainedTypedSampledDataSource<std::string>::New(value))
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

HdDataSourceBaseHandle two_connections(const TfToken &first_node, const TfToken &second_node)
{
  const HdDataSourceBaseHandle sources[] = {
      HdMaterialConnectionSchema::Builder()
          .SetUpstreamNodePath(HdRetainedTypedSampledDataSource<TfToken>::New(first_node))
          .SetUpstreamNodeOutputName(
              HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("out")))
          .Build(),
      HdMaterialConnectionSchema::Builder()
          .SetUpstreamNodePath(HdRetainedTypedSampledDataSource<TfToken>::New(second_node))
          .SetUpstreamNodeOutputName(
              HdRetainedTypedSampledDataSource<TfToken>::New(TfToken("out")))
          .Build(),
  };
  return HdVectorSchema::BuildRetained(2, sources);
}

HdContainerDataSourceHandle node(const char *identifier,
                                  const float value,
                                  const HdContainerDataSourceHandle &connections = nullptr)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(value)))
      .SetInputConnections(connections)
      .Build();
}

HdContainerDataSourceHandle node(const char *identifier,
                                  const HdContainerDataSourceHandle &parameters,
                                  const HdContainerDataSourceHandle &connections = nullptr)
{
  return HdMaterialNodeSchema::Builder()
      .SetNodeIdentifier(HdRetainedTypedSampledDataSource<TfToken>::New(TfToken(identifier)))
      .SetParameters(parameters)
      .SetInputConnections(connections)
      .Build();
}

}  // namespace

TEST(HdCyclesMaterialXMapping, rejects_absolute_path_node_key_cycles_atomically)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("/Looks/Graph/A"),
      node("ND_add_float",
           HdRetainedContainerDataSource::New(TfToken("in2"), float_parameter(1.0f)),
           HdRetainedContainerDataSource::New(TfToken("in1"),
                                              connection(TfToken("/Looks/Graph/B"), TfToken("out")))),
      TfToken("/Looks/Graph/B"),
      node("ND_multiply_float",
           HdRetainedContainerDataSource::New(TfToken("in2"), float_parameter(2.0f)),
           HdRetainedContainerDataSource::New(TfToken("in1"),
                                              connection(TfToken("/Looks/Graph/A"), TfToken("out")))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXAbsolutePathCycle"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int math_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    math_count += dynamic_cast<MathNode *>(node) != nullptr;
  }
  EXPECT_EQ(math_count, 0) << "rejected cyclic networks must not mutate the graph";

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_duplicate_basename_cycles_by_absolute_node_key)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("/Looks/GraphA/Shared"),
      node("ND_add_float",
           HdRetainedContainerDataSource::New(TfToken("in2"), float_parameter(1.0f)),
           HdRetainedContainerDataSource::New(
               TfToken("in1"), connection(TfToken("/Looks/GraphB/Shared"), TfToken("out")))),
      TfToken("/Looks/GraphB/Shared"),
      node("ND_multiply_float",
           HdRetainedContainerDataSource::New(TfToken("in2"), float_parameter(2.0f)),
           HdRetainedContainerDataSource::New(
               TfToken("in1"), connection(TfToken("/Looks/GraphA/Shared"), TfToken("out")))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXDuplicateBasenameCycle"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int math_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    math_count += dynamic_cast<MathNode *>(node) != nullptr;
  }
  EXPECT_EQ(math_count, 0) << "cycle detection must key nodes by full absolute path";

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_noise3d_structural_variants)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Float"), node("ND_noise3d_float", nullptr),
      TfToken("Color"), node("ND_noise3d_color3", nullptr),
      TfToken("ColorFA"), node("ND_noise3d_color3FA", nullptr));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXNoise3D"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);
  int noise_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (NoiseTextureNode *noise = dynamic_cast<NoiseTextureNode *>(node)) {
      EXPECT_EQ(noise->get_dimensions(), 3);
      ++noise_count;
    }
  }
  EXPECT_EQ(noise_count, 3);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_luminance_color3_with_literal_coefficients)
{
  const HdContainerDataSourceHandle parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)),
      TfToken("lumacoeffs"), vector3_parameter(pxr::GfVec3f(0.2126f, 0.7152f, 0.0722f)));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Luminance"), node("ND_luminance_color3", parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXLuminance"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateColorNode *separate = nullptr;
  CombineXYZNode *combine = nullptr;
  VectorMathNode *dot = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
    combine = combine ? combine : dynamic_cast<CombineXYZNode *>(node);
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
        math && math->get_math_type() == NODE_VECTOR_MATH_DOT_PRODUCT) {
      dot = math;
    }
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(dot, nullptr);
  EXPECT_EQ(separate->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(separate->get_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(dot->get_vector2(), make_float3(0.2126f, 0.7152f, 0.0722f));
  EXPECT_EQ(dot->input("Vector1")->link, combine->output("Vector"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_exact_scalar_color_and_vector3_extraction_utilities)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Convert"),
      node("ND_convert_float_color3",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.25f))),
      TfToken("ExtractX"),
      node("ND_extract_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
               TfToken("index"), int_parameter(0))),
      TfToken("ExtractY"),
      node("ND_extract_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
               TfToken("index"), int_parameter(1))),
      TfToken("ExtractZ"),
      node("ND_extract_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
               TfToken("index"), int_parameter(2))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXScalarColorVectorExtract"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  CombineColorNode *combine = nullptr;
  int separate_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    combine = combine ? combine : dynamic_cast<CombineColorNode *>(node);
    separate_count += dynamic_cast<SeparateXYZNode *>(node) != nullptr;
  }
  ASSERT_NE(combine, nullptr);
  EXPECT_EQ(combine->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_FLOAT_EQ(combine->get_r(), 0.25f);
  EXPECT_FLOAT_EQ(combine->get_g(), 0.25f);
  EXPECT_FLOAT_EQ(combine->get_b(), 0.25f);
  EXPECT_EQ(separate_count, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_separate3_vector3_with_exact_output_endpoints)
{
  const HdContainerDataSourceHandle parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Separate"), node("ND_separate3_vector3", parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSeparateVector3"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateXYZNode *separate = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateXYZNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  EXPECT_EQ(separate->get_vector(), make_float3(0.2f, 0.4f, 0.6f));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_exact_vector_component_utilities)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Constant"),
      node("ND_constant_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("value"), vector2_parameter(pxr::GfVec2f(0.2f, 0.4f)))),
      TfToken("Combine2"),
      node("ND_combine2_vector2",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(0.3f),
                                              TfToken("in2"), float_parameter(0.6f))),
      TfToken("Extract"),
      node("ND_extract_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.7f, 0.8f)),
               TfToken("index"), int_parameter(1))),
      TfToken("Combine3"),
      node("ND_combine3_vector3",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(0.1f),
                                              TfToken("in2"), float_parameter(0.2f),
                                              TfToken("in3"), float_parameter(0.3f))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorUtilities"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int separate_count = 0;
  int combine_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate_count += dynamic_cast<SeparateXYZNode *>(node) != nullptr;
    combine_count += dynamic_cast<CombineXYZNode *>(node) != nullptr;
  }
  EXPECT_GE(separate_count, 2);
  EXPECT_GE(combine_count, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_image_color3_to_a_cycles_image_texture)
{
  const HdContainerDataSourceHandle image_parameters = HdRetainedContainerDataSource::New(
      TfToken("file"), string_parameter("textures/albedo.exr"));
  const HdContainerDataSourceHandle image_connections = HdRetainedContainerDataSource::New(
      TfToken("texcoord"), connection(TfToken("UV"), TfToken("Vector")));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Color"), connection(TfToken("Image"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("UV"), node("mapping", nullptr),
      TfToken("Image"), node("ND_image_color3", image_parameters, image_connections),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXImageColor"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  ImageTextureNode *image = nullptr;
  MappingNode *uv = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    image = image ? image : dynamic_cast<ImageTextureNode *>(node);
    uv = uv ? uv : dynamic_cast<MappingNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(image, nullptr);
  ASSERT_NE(uv, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(image->get_filename(), ustring("textures/albedo.exr"));
  EXPECT_EQ(image->input("Vector")->link, uv->output("Vector"));
  EXPECT_EQ(sink->input("Color")->link, image->output("Color"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_color3_vector3_conversion_and_component_family)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("ColorToVector"),
      node("ND_convert_color3_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)))),
      TfToken("VectorToColor"),
      node("ND_convert_vector3_color3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.4f, 0.5f, 0.6f)))),
      TfToken("FloatToVector"),
      node("ND_convert_float_vector3",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.25f))),
      TfToken("Combine"),
      node("ND_combine3_color3",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(0.2f),
                                              TfToken("in2"), float_parameter(0.4f),
                                              TfToken("in3"), float_parameter(0.6f))),
      TfToken("Separate"),
      node("ND_separate3_color3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.3f, 0.6f, 0.9f)))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXColorVectorConversions"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int separate_color_count = 0;
  int combine_color_count = 0;
  int separate_xyz_count = 0;
  int combine_xyz_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate_color_count += dynamic_cast<SeparateColorNode *>(node) != nullptr;
    combine_color_count += dynamic_cast<CombineColorNode *>(node) != nullptr;
    separate_xyz_count += dynamic_cast<SeparateXYZNode *>(node) != nullptr;
    combine_xyz_count += dynamic_cast<CombineXYZNode *>(node) != nullptr;
  }
  EXPECT_GE(separate_color_count, 2);
  EXPECT_GE(combine_color_count, 2);
  EXPECT_GE(separate_xyz_count, 1);
  EXPECT_GE(combine_xyz_count, 2);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_extract_color3_with_a_literal_rgb_channel_selector)
{
  const HdContainerDataSourceHandle extract_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)),
      TfToken("index"), int_parameter(1));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Strength"), connection(TfToken("Extract"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Extract"), node("ND_extract_color3", extract_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXExtractColor"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateColorNode *separate = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(separate->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(separate->get_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(sink->input("Strength")->link, separate->output("Green"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, consumes_extract_color3_index_before_native_parameter_update)
{
  const HdContainerDataSourceHandle parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
      TfToken("index"), int_parameter(2));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Extract"), node("ND_extract_color3", parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXExtractColorConsumedIndex"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateColorNode *separate = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  EXPECT_EQ(separate->get_color(), make_float3(0.1f, 0.2f, 0.3f));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_constant_vector3_with_a_literal_vector_adapter)
{
  const HdContainerDataSourceHandle constant_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Vector"), connection(TfToken("Constant"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Constant"), node("ND_constant_vector3", constant_parameters),
      TfToken("Sink"), node("mapping", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConstantVector3"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateXYZNode *separate = nullptr;
  CombineXYZNode *combine = nullptr;
  MappingNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateXYZNode *>(node);
    sink = sink ? sink : dynamic_cast<MappingNode *>(node);
  }
  if (separate) {
    for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
      CombineXYZNode *candidate = dynamic_cast<CombineXYZNode *>(node);
      if (candidate && candidate->input("X")->link == separate->output("X") &&
          candidate->input("Y")->link == separate->output("Y") &&
          candidate->input("Z")->link == separate->output("Z")) {
        combine = candidate;
        break;
      }
    }
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(separate->get_vector(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(combine->input("X")->link, separate->output("X"));
  EXPECT_EQ(combine->input("Y")->link, separate->output("Y"));
  EXPECT_EQ(combine->input("Z")->link, separate->output("Z"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_constant_float_to_a_cycles_value_node)
{
  const HdContainerDataSourceHandle constant_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), float_parameter(0.375f));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Strength"), connection(TfToken("Constant"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Constant"), node("ND_constant_float", constant_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConstantFloat"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  ValueNode *constant = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    constant = constant ? constant : dynamic_cast<ValueNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(constant, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_FLOAT_EQ(constant->get_value(), 0.375f);
  EXPECT_EQ(sink->input("Strength")->link, constant->output("Value"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_rgbtohsv_color3_with_color_component_adapters)
{
  const HdContainerDataSourceHandle convert_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Color"), connection(TfToken("Convert"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Convert"), node("ND_rgbtohsv_color3", convert_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXRgbToHsv"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateColorNode *separate = nullptr;
  CombineColorNode *combine = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
    combine = combine ? combine : dynamic_cast<CombineColorNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(separate->get_color_type(), NODE_COMBSEP_COLOR_HSV);
  EXPECT_EQ(combine->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(separate->get_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(combine->input("Red")->link, separate->output("Red"));
  EXPECT_EQ(combine->input("Green")->link, separate->output("Green"));
  EXPECT_EQ(combine->input("Blue")->link, separate->output("Blue"));
  EXPECT_EQ(sink->input("Color")->link, combine->output("Color"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_hsvtorgb_color3_with_color_component_adapters)
{
  const HdContainerDataSourceHandle convert_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Color"), connection(TfToken("Convert"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Convert"), node("ND_hsvtorgb_color3", convert_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXHsvToRgb"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateColorNode *separate = nullptr;
  CombineColorNode *combine = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateColorNode *>(node);
    combine = combine ? combine : dynamic_cast<CombineColorNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(separate->get_color_type(), NODE_COMBSEP_COLOR_RGB);
  EXPECT_EQ(combine->get_color_type(), NODE_COMBSEP_COLOR_HSV);
  EXPECT_EQ(separate->get_color(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(combine->input("Red")->link, separate->output("Red"));
  EXPECT_EQ(combine->input("Green")->link, separate->output("Green"));
  EXPECT_EQ(combine->input("Blue")->link, separate->output("Blue"));
  EXPECT_EQ(sink->input("Color")->link, combine->output("Color"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_mix_color3_to_a_cycles_color_mix_node)
{
  const HdContainerDataSourceHandle mix_parameters = HdRetainedContainerDataSource::New(
      TfToken("mix"), float_parameter(0.25f),
      TfToken("bg"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)),
      TfToken("fg"), vector3_parameter(pxr::GfVec3f(0.7f, 0.8f, 0.9f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Color"), connection(TfToken("Mix"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Mix"), node("ND_mix_color3", mix_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXMixColor"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MixColorNode *mix = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    mix = mix ? mix : dynamic_cast<MixColorNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(mix, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_FLOAT_EQ(mix->get_fac(), 0.25f);
  EXPECT_EQ(mix->get_a(), make_float3(0.1f, 0.2f, 0.3f));
  EXPECT_EQ(mix->get_b(), make_float3(0.7f, 0.8f, 0.9f));
  EXPECT_EQ(sink->input("Color")->link, mix->output("Result"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_constant_color3_to_a_cycles_color_node)
{
  const HdContainerDataSourceHandle constant_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Color"), connection(TfToken("Constant"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Constant"), node("ND_constant_color3", constant_parameters),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConstantColor"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  ColorNode *constant = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    constant = constant ? constant : dynamic_cast<ColorNode *>(node);
    sink = sink ? sink : dynamic_cast<EmissionNode *>(node);
  }
  ASSERT_NE(constant, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(constant->get_value(), make_float3(0.2f, 0.4f, 0.6f));
  EXPECT_EQ(sink->input("Color")->link, constant->output("Color"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_exact_unary_math_with_materialx_socket_aliases)
{
  const HdContainerDataSourceHandle cos_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sin"), TfToken("out")));
  const HdContainerDataSourceHandle tan_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Cos"), TfToken("out")));
  const HdContainerDataSourceHandle exp_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Tan"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Sin"), node("ND_sin_float", 0.25f),
      TfToken("Cos"), node("ND_cos_float", 0.5f, cos_connections),
      TfToken("Tan"), node("ND_tan_float", 0.75f, tan_connections),
      TfToken("Exp"), node("ND_exp_float", 1.0f, exp_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXUnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *sin = nullptr;
  MathNode *cos = nullptr;
  MathNode *tan = nullptr;
  MathNode *exp = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    MathNode *math = dynamic_cast<MathNode *>(node);
    if (!math) {
      continue;
    }
    switch (math->get_math_type()) {
      case NODE_MATH_SINE: sin = math; break;
      case NODE_MATH_COSINE: cos = math; break;
      case NODE_MATH_TANGENT: tan = math; break;
      case NODE_MATH_EXPONENT: exp = math; break;
      default: break;
    }
  }
  ASSERT_NE(sin, nullptr);
  ASSERT_NE(cos, nullptr);
  ASSERT_NE(tan, nullptr);
  ASSERT_NE(exp, nullptr);
  EXPECT_EQ(sin->get_math_type(), NODE_MATH_SINE);
  EXPECT_EQ(cos->get_math_type(), NODE_MATH_COSINE);
  EXPECT_EQ(tan->get_math_type(), NODE_MATH_TANGENT);
  EXPECT_EQ(exp->get_math_type(), NODE_MATH_EXPONENT);
  EXPECT_FLOAT_EQ(sin->get_value1(), 0.25f);
  EXPECT_EQ(cos->input("Value1")->link, sin->output("Value"));
  EXPECT_EQ(tan->input("Value1")->link, cos->output("Value"));
  EXPECT_EQ(exp->input("Value1")->link, tan->output("Value"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_direct_vector3_unary_math_with_materialx_socket_aliases)
{
  const HdContainerDataSourceHandle ceil_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Floor"), TfToken("out")));
  const HdContainerDataSourceHandle fract_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Ceil"), TfToken("out")));
  const HdContainerDataSourceHandle sin_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Fract"), TfToken("out")));
  const HdContainerDataSourceHandle cos_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sin"), TfToken("out")));
  const HdContainerDataSourceHandle tan_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Cos"), TfToken("out")));
  const HdContainerDataSourceHandle sign_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Tan"), TfToken("out")));
  const auto vector3_node = [](const char *identifier,
                               const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector3_parameter(pxr::GfVec3f(1.2f, -2.3f, 0.4f))),
                connections);
  };
  const std::array<TfToken, 7> node_names = {
      TfToken("Floor"), TfToken("Ceil"), TfToken("Fract"), TfToken("Sin"),
      TfToken("Cos"), TfToken("Tan"), TfToken("Sign")};
  const std::array<HdDataSourceBaseHandle, 7> node_values = {
      vector3_node("ND_floor_vector3"),
      vector3_node("ND_ceil_vector3", ceil_connections),
      vector3_node("ND_fract_vector3", fract_connections),
      vector3_node("ND_sin_vector3", sin_connections),
      vector3_node("ND_cos_vector3", cos_connections),
      vector3_node("ND_tan_vector3", tan_connections),
      vector3_node("ND_sign_vector3", sign_connections)};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXDirectVector3UnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::array<VectorMathNode *, 7> math_nodes = {};
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node)) {
      switch (math->get_math_type()) {
        case NODE_VECTOR_MATH_FLOOR: math_nodes[0] = math; break;
        case NODE_VECTOR_MATH_CEIL: math_nodes[1] = math; break;
        case NODE_VECTOR_MATH_FRACTION: math_nodes[2] = math; break;
        case NODE_VECTOR_MATH_SINE: math_nodes[3] = math; break;
        case NODE_VECTOR_MATH_COSINE: math_nodes[4] = math; break;
        case NODE_VECTOR_MATH_TANGENT: math_nodes[5] = math; break;
        case NODE_VECTOR_MATH_SIGN: math_nodes[6] = math; break;
        default: break;
      }
    }
  }
  for (VectorMathNode *math : math_nodes) {
    ASSERT_NE(math, nullptr);
  }
  EXPECT_EQ(math_nodes[0]->get_vector1(), make_float3(1.2f, -2.3f, 0.4f));
  for (size_t i = 1; i < math_nodes.size(); i++) {
    EXPECT_EQ(math_nodes[i]->input("Vector1")->link, math_nodes[i - 1]->output("Vector"));
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_direct_vector2_unary_math_with_xy_only_adapter)
{
  const HdContainerDataSourceHandle ceil_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Floor"), TfToken("out")));
  const HdContainerDataSourceHandle fract_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Ceil"), TfToken("out")));
  const HdContainerDataSourceHandle sin_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Fract"), TfToken("out")));
  const HdContainerDataSourceHandle cos_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sin"), TfToken("out")));
  const HdContainerDataSourceHandle tan_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Cos"), TfToken("out")));
  const HdContainerDataSourceHandle sign_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Tan"), TfToken("out")));
  const auto vector2_node = [](const char *identifier,
                               const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector2_parameter(pxr::GfVec2f(1.2f, -2.3f))),
                connections);
  };
  const std::array<TfToken, 7> node_names = {
      TfToken("Floor"), TfToken("Ceil"), TfToken("Fract"), TfToken("Sin"),
      TfToken("Cos"), TfToken("Tan"), TfToken("Sign")};
  const std::array<HdDataSourceBaseHandle, 7> node_values = {
      vector2_node("ND_floor_vector2"),
      vector2_node("ND_ceil_vector2", ceil_connections),
      vector2_node("ND_fract_vector2", fract_connections),
      vector2_node("ND_sin_vector2", sin_connections),
      vector2_node("ND_cos_vector2", cos_connections),
      vector2_node("ND_tan_vector2", tan_connections),
      vector2_node("ND_sign_vector2", sign_connections)};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXDirectVector2UnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  const std::array<NodeVectorMathType, 7> expected_types = {
      NODE_VECTOR_MATH_FLOOR,
      NODE_VECTOR_MATH_CEIL,
      NODE_VECTOR_MATH_FRACTION,
      NODE_VECTOR_MATH_SINE,
      NODE_VECTOR_MATH_COSINE,
      NODE_VECTOR_MATH_TANGENT,
      NODE_VECTOR_MATH_SIGN,
  };
  std::array<VectorMathNode *, 7> math_nodes = {};
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node);
    if (!math) {
      continue;
    }
    for (size_t i = 0; i < expected_types.size(); i++) {
      if (math->get_math_type() == expected_types[i]) {
        math_nodes[i] = math;
      }
    }
  }
  for (VectorMathNode *math : math_nodes) {
    ASSERT_NE(math, nullptr);
  }
  /* Vector2 is represented by the existing exact (x, y, 0) Cycles adapter.
   * Each supported unary operation preserves zero, so no third component is invented. */
  EXPECT_EQ(math_nodes[0]->get_vector1(), make_float3(1.2f, -2.3f, 0.0f));
  for (size_t i = 1; i < math_nodes.size(); i++) {
    EXPECT_EQ(math_nodes[i]->input("Vector1")->link, math_nodes[i - 1]->output("Vector"));
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_direct_vector2_arithmetic_and_bounds_without_z_fabrication)
{
  const auto vector2_binary = [](const char *identifier,
                                 const pxr::GfVec2f &in1,
                                 const pxr::GfVec2f &in2,
                                 const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector2_parameter(in1), TfToken("in2"), vector2_parameter(in2)),
                connections);
  };
  const HdContainerDataSourceHandle subtract_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Add"), TfToken("out")));
  const HdContainerDataSourceHandle multiply_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Subtract"), TfToken("out")));
  const HdContainerDataSourceHandle min_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Multiply"), TfToken("out")));
  const HdContainerDataSourceHandle max_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Min"), TfToken("out")));
  const HdContainerDataSourceHandle abs_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Max"), TfToken("out")));
  const std::array<TfToken, 6> node_names = {
      TfToken("Add"), TfToken("Subtract"), TfToken("Multiply"),
      TfToken("Min"), TfToken("Max"), TfToken("Abs")};
  const std::array<HdDataSourceBaseHandle, 6> node_values = {
      vector2_binary("ND_add_vector2", pxr::GfVec2f(1.0f, 2.0f), pxr::GfVec2f(3.0f, 4.0f)),
      vector2_binary(
          "ND_subtract_vector2", pxr::GfVec2f(), pxr::GfVec2f(0.5f, 1.5f), subtract_connections),
      vector2_binary(
          "ND_multiply_vector2", pxr::GfVec2f(), pxr::GfVec2f(2.0f, 3.0f), multiply_connections),
      vector2_binary("ND_min_vector2", pxr::GfVec2f(), pxr::GfVec2f(5.0f, 5.0f), min_connections),
      vector2_binary("ND_max_vector2", pxr::GfVec2f(), pxr::GfVec2f(-1.0f, -1.0f), max_connections),
      node("ND_absval_vector2", nullptr, abs_connections)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXDirectVector2Arithmetic"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  const std::array<NodeVectorMathType, 6> expected_types = {
      NODE_VECTOR_MATH_ADD,
      NODE_VECTOR_MATH_SUBTRACT,
      NODE_VECTOR_MATH_MULTIPLY,
      NODE_VECTOR_MATH_MINIMUM,
      NODE_VECTOR_MATH_MAXIMUM,
      NODE_VECTOR_MATH_ABSOLUTE,
  };
  std::array<VectorMathNode *, 6> math_nodes = {};
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node)) {
      for (size_t i = 0; i < expected_types.size(); i++) {
        if (math->get_math_type() == expected_types[i]) math_nodes[i] = math;
      }
    }
  }
  for (VectorMathNode *math : math_nodes) {
    ASSERT_NE(math, nullptr);
  }
  EXPECT_EQ(math_nodes[0]->get_vector1(), make_float3(1.0f, 2.0f, 0.0f));
  EXPECT_EQ(math_nodes[0]->get_vector2(), make_float3(3.0f, 4.0f, 0.0f));
  for (size_t i = 1; i < math_nodes.size(); i++) {
    EXPECT_EQ(math_nodes[i]->input("Vector1")->link, math_nodes[i - 1]->output("Vector"));
  }
  for (size_t i = 1; i < math_nodes.size() - 1; i++) {
    EXPECT_FLOAT_EQ(math_nodes[i]->get_vector2().z, 0.0f);
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector2_and_scalar_broadcast_divide_without_zero_over_zero_z)
{
  const HdContainerDataSourceHandle vector_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector2_parameter(pxr::GfVec2f(8.0f, 9.0f)),
      TfToken("in2"), vector2_parameter(pxr::GfVec2f(2.0f, 3.0f)));
  const HdContainerDataSourceHandle broadcast_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector2_parameter(pxr::GfVec2f(8.0f, 9.0f)), TfToken("in2"), float_parameter(2.0f));
  const std::array<TfToken, 2> node_names = {TfToken("DivideVector2"), TfToken("DivideVector2FA")};
  const std::array<HdDataSourceBaseHandle, 2> node_values = {
      node("ND_divide_vector2", vector_parameters), node("ND_divide_vector2FA", broadcast_parameters)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector2Divide"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int divide_count = 0;
  int scalar_broadcasts = 0;
  int vector2_outputs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_DIVIDE) {
        divide_count++;
        if (math->input("Value2")->link == nullptr && math->get_value2() == 2.0f) {
          scalar_broadcasts++;
        }
      }
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      EXPECT_EQ(combine->input("Z")->link, nullptr);
      EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
      vector2_outputs++;
    }
  }
  EXPECT_EQ(divide_count, 4);
  EXPECT_EQ(scalar_broadcasts, 2);
  EXPECT_EQ(vector2_outputs, 2);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector3_divide_and_scalar_broadcast_divide)
{
  const HdContainerDataSourceHandle vector_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector3_parameter(pxr::GfVec3f(8.0f, 9.0f, 16.0f)),
      TfToken("in2"), vector3_parameter(pxr::GfVec3f(2.0f, 3.0f, 4.0f)));
  const HdContainerDataSourceHandle broadcast_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector3_parameter(pxr::GfVec3f(8.0f, 9.0f, 16.0f)),
      TfToken("in2"), float_parameter(2.0f));
  const std::array<TfToken, 2> node_names = {TfToken("DivideVector3"), TfToken("DivideVector3FA")};
  const std::array<HdDataSourceBaseHandle, 2> node_values = {
      node("ND_divide_vector3", vector_parameters), node("ND_divide_vector3FA", broadcast_parameters)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector3Divide"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  VectorMathNode *vector_divide = nullptr;
  int scalar_divides = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_VECTOR_MATH_DIVIDE) {
        vector_divide = math;
      }
    }
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_DIVIDE) {
        EXPECT_EQ(math->input("Value2")->link, nullptr);
        EXPECT_FLOAT_EQ(math->get_value2(), 2.0f);
        scalar_divides++;
      }
    }
  }
  ASSERT_NE(vector_divide, nullptr);
  EXPECT_EQ(vector_divide->get_vector1(), make_float3(8.0f, 9.0f, 16.0f));
  EXPECT_EQ(vector_divide->get_vector2(), make_float3(2.0f, 3.0f, 4.0f));
  EXPECT_EQ(scalar_divides, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_safe_scalar_math_batch_with_exact_special_ordering)
{
  const std::array<TfToken, 6> node_names = {
      TfToken("Modulo"), TfToken("Minimum"), TfToken("Maximum"),
      TfToken("Atan2"), TfToken("Ln"), TfToken("Clamp")};
  const std::array<HdDataSourceBaseHandle, 6> node_values = {
      node("ND_modulo_float",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(9.0f),
                                              TfToken("in2"), float_parameter(4.0f))),
      node("ND_min_float",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(2.0f),
                                              TfToken("in2"), float_parameter(3.0f))),
      node("ND_max_float",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(2.0f),
                                              TfToken("in2"), float_parameter(3.0f))),
      node("ND_atan2_float",
           HdRetainedContainerDataSource::New(TfToken("iny"), float_parameter(5.0f),
                                              TfToken("inx"), float_parameter(7.0f))),
      node("ND_ln_float", HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(8.0f))),
      node("ND_clamp_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("low"), float_parameter(1.0f),
                                              TfToken("high"), float_parameter(2.0f)))};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSafeScalarMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *modulo = nullptr;
  MathNode *minimum = nullptr;
  MathNode *maximum = nullptr;
  MathNode *atan2 = nullptr;
  MathNode *ln = nullptr;
  MathNode *clamp_maximum = nullptr;
  MathNode *clamp_minimum = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    MathNode *math = dynamic_cast<MathNode *>(shader_node);
    if (!math) {
      continue;
    }
    if (math->get_math_type() == NODE_MATH_MODULO) modulo = math;
    if (math->get_math_type() == NODE_MATH_MINIMUM) {
      math->input("Value1")->link ? clamp_minimum = math : minimum = math;
    }
    if (math->get_math_type() == NODE_MATH_MAXIMUM) {
      (math->get_value1() == 0.5f) ? clamp_maximum = math : maximum = math;
    }
    if (math->get_math_type() == NODE_MATH_ARCTAN2) atan2 = math;
    if (math->get_math_type() == NODE_MATH_LOGARITHM) ln = math;
  }
  ASSERT_NE(modulo, nullptr);
  ASSERT_NE(minimum, nullptr);
  ASSERT_NE(maximum, nullptr);
  ASSERT_NE(atan2, nullptr);
  ASSERT_NE(ln, nullptr);
  ASSERT_NE(clamp_maximum, nullptr);
  ASSERT_NE(clamp_minimum, nullptr);
  EXPECT_FLOAT_EQ(modulo->get_value1(), 9.0f);
  EXPECT_FLOAT_EQ(modulo->get_value2(), 4.0f);
  EXPECT_FLOAT_EQ(minimum->get_value1(), 2.0f);
  EXPECT_FLOAT_EQ(minimum->get_value2(), 3.0f);
  EXPECT_FLOAT_EQ(maximum->get_value1(), 2.0f);
  EXPECT_FLOAT_EQ(maximum->get_value2(), 3.0f);
  EXPECT_FLOAT_EQ(atan2->get_value1(), 5.0f);
  EXPECT_FLOAT_EQ(atan2->get_value2(), 7.0f);
  EXPECT_FLOAT_EQ(ln->get_value1(), 8.0f);
  EXPECT_FLOAT_EQ(ln->get_value2(), 2.71828182845904523536f);
  EXPECT_EQ(clamp_minimum->input("Value1")->link, clamp_maximum->output("Value"));
  EXPECT_FLOAT_EQ(clamp_maximum->get_value2(), 1.0f);
  EXPECT_FLOAT_EQ(clamp_minimum->get_value2(), 2.0f);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector_clamp_and_scalar_broadcast_min_max_batch)
{
  const auto vector2_clamp = [](const char *identifier) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector2_parameter(pxr::GfVec2f(0.5f, -1.0f)),
                    TfToken("low"), vector2_parameter(pxr::GfVec2f(1.0f, 2.0f)),
                    TfToken("high"), vector2_parameter(pxr::GfVec2f(2.0f, 3.0f))));
  };
  const auto vector2_clamp_fa = [](const char *identifier) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector2_parameter(pxr::GfVec2f(0.5f, -1.0f)),
                    TfToken("low"), float_parameter(1.0f), TfToken("high"), float_parameter(2.0f)));
  };
  const auto vector_broadcast = [](const char *identifier, const pxr::GfVec2f &value, const float bound) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector2_parameter(value), TfToken("in2"), float_parameter(bound)));
  };
  const auto vector3_broadcast = [](const char *identifier, const float bound) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f)),
                    TfToken("in2"), float_parameter(bound)));
  };
  const std::array<TfToken, 6> node_names = {
      TfToken("ClampVector2"), TfToken("ClampVector2FA"), TfToken("MinVector2FA"),
      TfToken("MinVector3FA"), TfToken("MaxVector2FA"), TfToken("MaxVector3FA")};
  const std::array<HdDataSourceBaseHandle, 6> node_values = {
      vector2_clamp("ND_clamp_vector2"),
      vector2_clamp_fa("ND_clamp_vector2FA"),
      vector_broadcast("ND_min_vector2FA", pxr::GfVec2f(3.0f, 4.0f), 0.25f),
      vector3_broadcast("ND_min_vector3FA", 0.25f),
      vector_broadcast("ND_max_vector2FA", pxr::GfVec2f(3.0f, 4.0f), 0.75f),
      vector3_broadcast("ND_max_vector3FA", 0.75f)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorBoundBroadcast"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int scalar_minimums = 0;
  int scalar_maximums = 0;
  int vector2_combines = 0;
  int vector3_combines = 0;
  VectorMathNode *clamp_maximum = nullptr;
  VectorMathNode *clamp_minimum = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_MINIMUM) scalar_minimums++;
      if (math->get_math_type() == NODE_MATH_MAXIMUM) scalar_maximums++;
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      if (combine->input("Z")->link) {
        vector3_combines++;
      }
      else {
        EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
        vector2_combines++;
      }
    }
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM &&
          math->get_vector1() == make_float3(0.5f, -1.0f, 0.0f)) {
        clamp_maximum = math;
      }
      if (math->get_math_type() == NODE_VECTOR_MATH_MINIMUM) {
        clamp_minimum = math;
      }
    }
  }
  ASSERT_NE(clamp_maximum, nullptr);
  ASSERT_NE(clamp_minimum, nullptr);
  EXPECT_EQ(clamp_minimum->input("Vector1")->link, clamp_maximum->output("Vector"));
  EXPECT_EQ(clamp_maximum->get_vector2(), make_float3(1.0f, 2.0f, 0.0f));
  EXPECT_EQ(clamp_minimum->get_vector2(), make_float3(2.0f, 3.0f, 0.0f));
  /* ClampV2FA (2) + min V2FA/V3FA (2+3) and max V2FA/V3FA (2+3). */
  EXPECT_EQ(scalar_minimums, 7);
  EXPECT_EQ(scalar_maximums, 7);
  EXPECT_EQ(vector2_combines, 3);
  EXPECT_EQ(vector3_combines, 2);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_safe_color_vector_conversion_boundaries)
{
  const std::array<TfToken, 7> node_names = {TfToken("FloatToVector2"),
                                              TfToken("ColorToVector2"),
                                              TfToken("Vector2ToColor"),
                                              TfToken("Vector3ToVector2"),
                                              TfToken("Separate2"),
                                              TfToken("Separate2SinkX"),
                                              TfToken("Separate2SinkY")};
  const std::array<HdDataSourceBaseHandle, 7> node_values = {
      node("ND_convert_float_vector2", HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.25f))),
      node("ND_convert_color3_vector2", HdRetainedContainerDataSource::New(
          TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.2f, 0.3f)))),
      node("ND_convert_vector2_color3", HdRetainedContainerDataSource::New(
          TfToken("in"), vector2_parameter(pxr::GfVec2f(0.4f, 0.5f)))),
      node("ND_convert_vector3_vector2", HdRetainedContainerDataSource::New(
          TfToken("in"), vector3_parameter(pxr::GfVec3f(0.6f, 0.7f, 0.8f)))),
      node("ND_separate2_vector2", HdRetainedContainerDataSource::New(
          TfToken("in"), vector2_parameter(pxr::GfVec2f(0.9f, 1.0f)))),
      node("ND_absval_float", nullptr,
           HdRetainedContainerDataSource::New(TfToken("in"), connection(TfToken("Separate2"), TfToken("outx")))),
      node("ND_absval_float", nullptr,
           HdRetainedContainerDataSource::New(TfToken("in"), connection(TfToken("Separate2"), TfToken("outy"))))};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSafeConversionBoundaries"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int vector2_outputs = 0;
  int color_outputs = 0;
  int separate_vector_nodes = 0;
  int separate2_x_links = 0;
  int separate2_y_links = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      EXPECT_EQ(combine->input("Z")->link, nullptr);
      EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
      vector2_outputs++;
      if (combine->input("X")->link == nullptr) {
        EXPECT_FLOAT_EQ(combine->get_x(), 0.25f);
        EXPECT_FLOAT_EQ(combine->get_y(), 0.25f);
      }
      else if (dynamic_cast<SeparateColorNode *>(combine->input("X")->link->parent)) {
        EXPECT_EQ(combine->input("X")->link->name(), ustring("Red"));
        EXPECT_EQ(combine->input("Y")->link->name(), ustring("Green"));
      }
      else {
        EXPECT_EQ(combine->input("X")->link->name(), ustring("X"));
        EXPECT_EQ(combine->input("Y")->link->name(), ustring("Y"));
      }
    }
    if (CombineColorNode *combine = dynamic_cast<CombineColorNode *>(shader_node)) {
      EXPECT_EQ(combine->input("Blue")->link, nullptr);
      EXPECT_FLOAT_EQ(combine->get_b(), 0.0f);
      color_outputs++;
      EXPECT_EQ(combine->input("Red")->link->name(), ustring("X"));
      EXPECT_EQ(combine->input("Green")->link->name(), ustring("Y"));
    }
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ABSOLUTE && math->input("Value1")->link) {
        const ShaderOutput *source = math->input("Value1")->link;
        if (source->name() == ustring("X")) separate2_x_links++;
        if (source->name() == ustring("Y")) separate2_y_links++;
      }
    }
    if (dynamic_cast<SeparateXYZNode *>(shader_node)) separate_vector_nodes++;
  }
  EXPECT_EQ(vector2_outputs, 3);
  EXPECT_EQ(color_outputs, 1);
  EXPECT_EQ(separate_vector_nodes, 3);
  EXPECT_EQ(separate2_x_links, 1);
  EXPECT_EQ(separate2_y_links, 1);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_componentwise_color3_unary_math_batch)
{
  const std::array<TfToken, 6> node_names = {
      TfToken("Abs"), TfToken("Ceil"), TfToken("Floor"),
      TfToken("Fract"), TfToken("Round"), TfToken("Sign")};
  const auto color_node = [](const char *identifier) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector3_parameter(pxr::GfVec3f(-1.25f, 2.5f, 3.75f))));
  };
  const std::array<HdDataSourceBaseHandle, 6> node_values = {
      color_node("ND_absval_color3"), color_node("ND_ceil_color3"), color_node("ND_floor_color3"),
      color_node("ND_fract_color3"), color_node("ND_round_color3"), color_node("ND_sign_color3")};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXColor3Unary"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  const std::array<NodeMathType, 6> expected_types = {NODE_MATH_ABSOLUTE,
                                                        NODE_MATH_CEIL,
                                                        NODE_MATH_FLOOR,
                                                        NODE_MATH_FRACTION,
                                                        NODE_MATH_ROUND,
                                                        NODE_MATH_SIGN};
  std::array<int, 6> counts = {};
  int color_separates = 0;
  int color_combines = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      for (size_t i = 0; i < expected_types.size(); i++) {
        if (math->get_math_type() == expected_types[i]) counts[i]++;
      }
    }
    if (dynamic_cast<SeparateColorNode *>(shader_node)) color_separates++;
    if (dynamic_cast<CombineColorNode *>(shader_node)) color_combines++;
  }
  for (const int count : counts) EXPECT_EQ(count, 3);
  EXPECT_EQ(color_separates, 6);
  EXPECT_EQ(color_combines, 6);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_unclamped_componentwise_vector_remap)
{
  const auto parameters = [](const bool vector2, const bool scalar) {
    const auto v = vector2 ? vector2_parameter(pxr::GfVec2f(2.0f, 3.0f)) :
                             vector3_parameter(pxr::GfVec3f(2.0f, 3.0f, 4.0f));
    return HdRetainedContainerDataSource::New(
        TfToken("in"), v, TfToken("inlow"), scalar ? float_parameter(1.0f) : v,
        TfToken("inhigh"), scalar ? float_parameter(4.0f) : v,
        TfToken("outlow"), scalar ? float_parameter(10.0f) : v,
        TfToken("outhigh"), scalar ? float_parameter(20.0f) : v);
  };
  const std::array<TfToken, 4> names = {TfToken("V2"), TfToken("V3"), TfToken("V2FA"), TfToken("V3FA")};
  const std::array<HdDataSourceBaseHandle, 4> values = {
      node("ND_remap_vector2", parameters(true, false)), node("ND_remap_vector3", parameters(false, false)),
      node("ND_remap_vector2FA", parameters(true, true)), node("ND_remap_vector3FA", parameters(false, true))};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
  HdCyclesSession session{SessionParams()}; HdCyclesMaterial material(SdfPath("/MaterialXRemap"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);
  int subtract = 0, multiply = 0, divide = 0, add = 0, clamp = 0, v2 = 0, v3 = 0;
  for (ShaderNode *n : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *m = dynamic_cast<MathNode *>(n)) {
      subtract += m->get_math_type() == NODE_MATH_SUBTRACT;
      multiply += m->get_math_type() == NODE_MATH_MULTIPLY;
      divide += m->get_math_type() == NODE_MATH_DIVIDE;
      add += m->get_math_type() == NODE_MATH_ADD;
      clamp += m->get_math_type() == NODE_MATH_MINIMUM || m->get_math_type() == NODE_MATH_MAXIMUM;
    }
    if (CombineXYZNode *c = dynamic_cast<CombineXYZNode *>(n)) { c->input("Z")->link ? v3++ : v2++; }
  }
  EXPECT_EQ(subtract, 30); EXPECT_EQ(multiply, 10); EXPECT_EQ(divide, 10); EXPECT_EQ(add, 10);
  EXPECT_EQ(clamp, 0); EXPECT_EQ(v2, 2); EXPECT_EQ(v3, 2);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_componentwise_vector2_and_vector3_domain_math_batch)
{
  const HdContainerDataSourceHandle vector2_asin_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Acos"), TfToken("out")));
  const HdContainerDataSourceHandle vector2_sqrt_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Asin"), TfToken("out")));
  const HdContainerDataSourceHandle vector2_ln_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Sqrt"), TfToken("out")));
  const HdContainerDataSourceHandle vector2_exp_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Ln"), TfToken("out")));
  const HdContainerDataSourceHandle vector2_round_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Exp"), TfToken("out")));
  const HdContainerDataSourceHandle vector3_asin_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector3Acos"), TfToken("out")));
  const HdContainerDataSourceHandle vector3_round_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector3Asin"), TfToken("out")));
  const std::array<TfToken, 9> node_names = {
      TfToken("Vector2Acos"), TfToken("Vector2Asin"),  TfToken("Vector2Sqrt"),
      TfToken("Vector2Ln"),   TfToken("Vector2Exp"),   TfToken("Vector2Round"),
      TfToken("Vector3Acos"), TfToken("Vector3Asin"),  TfToken("Vector3Round")};
  const std::array<HdDataSourceBaseHandle, 9> node_values = {
      node("ND_acos_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.5f)))),
      node("ND_asin_vector2", nullptr, vector2_asin_connections),
      node("ND_sqrt_vector2", nullptr, vector2_sqrt_connections),
      node("ND_ln_vector2", nullptr, vector2_ln_connections),
      node("ND_exp_vector2", nullptr, vector2_exp_connections),
      node("ND_round_vector2", nullptr, vector2_round_connections),
      node("ND_acos_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)))),
      node("ND_asin_vector3", nullptr, vector3_asin_connections),
      node("ND_round_vector3", nullptr, vector3_round_connections)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXComponentwiseDomainMathBatch"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::array<int, 6> math_counts = {};
  int vector2_outputs = 0;
  int vector3_outputs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      switch (math->get_math_type()) {
        case NODE_MATH_ARCSINE: math_counts[0]++; break;
        case NODE_MATH_ARCCOSINE: math_counts[1]++; break;
        case NODE_MATH_SQRT: math_counts[2]++; break;
        case NODE_MATH_LOGARITHM:
          math_counts[3]++;
          EXPECT_FLOAT_EQ(math->get_value2(), 2.71828182845904523536f);
          break;
        case NODE_MATH_EXPONENT: math_counts[4]++; break;
        case NODE_MATH_ROUND: math_counts[5]++; break;
        default: break;
      }
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      if (combine->input("Z")->link) {
        vector3_outputs++;
      }
      else {
        EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
        vector2_outputs++;
      }
    }
  }
  /* asin, acos, and round each cover Vector2 + Vector3; sqrt, ln, and exp are Vector2. */
  EXPECT_EQ(math_counts[0], 5);
  EXPECT_EQ(math_counts[1], 5);
  EXPECT_EQ(math_counts[2], 2);
  EXPECT_EQ(math_counts[3], 2);
  EXPECT_EQ(math_counts[4], 2);
  EXPECT_EQ(math_counts[5], 5);
  EXPECT_EQ(vector2_outputs, 6);
  EXPECT_EQ(vector3_outputs, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector_invert_and_scalar_amount_broadcast_exactly)
{
  const HdContainerDataSourceHandle vector2_connections = HdRetainedContainerDataSource::New(
      TfToken("amount"), connection(TfToken("Vector2Amount"), TfToken("out")));
  const HdContainerDataSourceHandle vector3fa_connections = HdRetainedContainerDataSource::New(
      TfToken("amount"), connection(TfToken("ScalarAmount"), TfToken("out")));
  const std::array<TfToken, 6> node_names = {TfToken("Vector2Amount"),
                                             TfToken("InvertVector2"),
                                             TfToken("InvertVector2FA"),
                                             TfToken("ScalarAmount"),
                                             TfToken("InvertVector3"),
                                             TfToken("InvertVector3FA")};
  const std::array<HdDataSourceBaseHandle, 6> node_values = {
      node("ND_absval_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.9f, 0.7f)))),
      node("ND_invert_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.2f, 0.4f))),
           vector2_connections),
      node("ND_invert_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.1f, 0.3f)),
               TfToken("amount"), float_parameter(0.8f))),
      node("ND_absval_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.65f))),
      node("ND_invert_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, 0.4f, 0.6f)),
               TfToken("amount"), vector3_parameter(pxr::GfVec3f(0.9f, 0.8f, 0.7f)))),
      node("ND_invert_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.1f, 0.3f, 0.5f))),
           vector3fa_connections)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorInvert"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<VectorMathNode *> subtracts;
  std::vector<MathNode *> scalar_subtracts;
  MathNode *scalar_amount = nullptr;
  VectorMathNode *vector2_amount = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_VECTOR_MATH_SUBTRACT) {
        subtracts.push_back(math);
      }
    }
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_SUBTRACT) {
        scalar_subtracts.push_back(math);
      }
      if (math->get_math_type() == NODE_MATH_ABSOLUTE && math->get_value1() == 0.65f) {
        scalar_amount = math;
      }
    }
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node);
        math && math->get_math_type() == NODE_VECTOR_MATH_ABSOLUTE &&
        math->get_vector1() == make_float3(0.9f, 0.7f, 0.0f))
    {
      vector2_amount = math;
    }
  }

  ASSERT_EQ(subtracts.size(), 2);
  ASSERT_EQ(scalar_subtracts.size(), 5);
  ASSERT_NE(vector2_amount, nullptr);
  ASSERT_NE(scalar_amount, nullptr);

  int vector2_subtracts = 0;
  int vector3_subtracts = 0;
  for (VectorMathNode *subtract : subtracts) {
    if (subtract->input("Vector1")->link == vector2_amount->output("Vector")) {
      ++vector2_subtracts;
      EXPECT_EQ(subtract->get_vector2(), make_float3(0.2f, 0.4f, 0.0f));
    }
    else {
      ++vector3_subtracts;
      EXPECT_EQ(subtract->get_vector1(), make_float3(0.9f, 0.8f, 0.7f));
      EXPECT_EQ(subtract->get_vector2(), make_float3(0.2f, 0.4f, 0.6f));
    }
  }
  EXPECT_EQ(vector2_subtracts, 1);
  EXPECT_EQ(vector3_subtracts, 1);

  int literal_scalar_amount = 0;
  int linked_scalar_amount = 0;
  for (MathNode *subtract : scalar_subtracts) {
    ASSERT_NE(subtract->input("Value2")->link, nullptr);
    if (subtract->input("Value1")->link == nullptr) {
      EXPECT_FLOAT_EQ(subtract->get_value1(), 0.8f);
      ++literal_scalar_amount;
    }
    else {
      EXPECT_EQ(subtract->input("Value1")->link, scalar_amount->output("Value"));
      ++linked_scalar_amount;
    }
  }
  EXPECT_EQ(literal_scalar_amount, 2);
  EXPECT_EQ(linked_scalar_amount, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector3_clamp_and_float_bounds_with_exact_min_max_order)
{
  const HdContainerDataSourceHandle vector3_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(0.2f, -1.0f, 2.0f)),
      TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.1f, 0.2f)),
      TfToken("high"), vector3_parameter(pxr::GfVec3f(0.8f, 0.9f, 1.0f)));
  const HdContainerDataSourceHandle float_bound_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector3_parameter(pxr::GfVec3f(-2.0f, 0.5f, 2.0f)),
      TfToken("low"), float_parameter(-0.5f),
      TfToken("high"), float_parameter(0.75f));
  const std::array<TfToken, 2> node_names = {TfToken("ClampVector3"), TfToken("ClampVector3FA")};
  const std::array<HdDataSourceBaseHandle, 2> node_values = {
      node("ND_clamp_vector3", vector3_parameters), node("ND_clamp_vector3FA", float_bound_parameters)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXClampVector3"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<VectorMathNode *> maximums;
  std::vector<VectorMathNode *> minimums;
  std::vector<CombineXYZNode *> scalar_bounds;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_VECTOR_MATH_MAXIMUM) maximums.push_back(math);
      if (math->get_math_type() == NODE_VECTOR_MATH_MINIMUM) minimums.push_back(math);
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      scalar_bounds.push_back(combine);
    }
  }
  ASSERT_EQ(maximums.size(), 2);
  ASSERT_EQ(minimums.size(), 2);
  ASSERT_EQ(scalar_bounds.size(), 2);
  for (VectorMathNode *minimum : minimums) {
    bool receives_maximum = false;
    for (VectorMathNode *maximum : maximums) {
      receives_maximum |= minimum->input("Vector1")->link == maximum->output("Vector");
    }
    EXPECT_TRUE(receives_maximum);
  }
  int low_bound_count = 0;
  int high_bound_count = 0;
  CombineXYZNode *low_bound = nullptr;
  CombineXYZNode *high_bound = nullptr;
  for (CombineXYZNode *combine : scalar_bounds) {
    const float3 value = make_float3(combine->get_x(), combine->get_y(), combine->get_z());
    if (value == make_float3(-0.5f, -0.5f, -0.5f)) {
      low_bound_count++;
      low_bound = combine;
    }
    if (value == make_float3(0.75f, 0.75f, 0.75f)) {
      high_bound_count++;
      high_bound = combine;
    }
  }
  EXPECT_EQ(low_bound_count, 1);
  EXPECT_EQ(high_bound_count, 1);
  ASSERT_NE(low_bound, nullptr);
  ASSERT_NE(high_bound, nullptr);
  for (VectorMathNode *maximum : maximums) {
    VectorMathNode *minimum = nullptr;
    for (VectorMathNode *candidate : minimums) {
      if (candidate->input("Vector1")->link == maximum->output("Vector")) {
        minimum = candidate;
        break;
      }
    }
    ASSERT_NE(minimum, nullptr);
    if (maximum->get_vector1() == make_float3(0.2f, -1.0f, 2.0f)) {
      EXPECT_EQ(maximum->get_vector2(), make_float3(0.0f, 0.1f, 0.2f));
      EXPECT_EQ(minimum->get_vector2(), make_float3(0.8f, 0.9f, 1.0f));
    }
    else {
      EXPECT_EQ(maximum->get_vector1(), make_float3(-2.0f, 0.5f, 2.0f));
      EXPECT_EQ(maximum->input("Vector2")->link, low_bound->output("Vector"));
      EXPECT_EQ(minimum->input("Vector2")->link, high_bound->output("Vector"));
    }
  }

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_vector2_and_vector3_scalar_broadcast_add_subtract_multiply_batch)
{
  const HdContainerDataSourceHandle linked_vector2_scalar = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Scalar"), TfToken("out")));
  const HdContainerDataSourceHandle linked_vector3_scalar = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Scalar"), TfToken("out")));
  const std::array<TfToken, 7> node_names = {TfToken("Scalar"),
                                             TfToken("AddVector2FA"),
                                             TfToken("SubtractVector2FA"),
                                             TfToken("MultiplyVector2FA"),
                                             TfToken("AddVector3FA"),
                                             TfToken("SubtractVector3FA"),
                                             TfToken("MultiplyVector3FA")};
  const std::array<HdDataSourceBaseHandle, 7> node_values = {
      node("ND_absval_float", HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(2.0f))),
      node("ND_add_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector2_parameter(pxr::GfVec2f(1.0f, 2.0f)),
               TfToken("in2"), float_parameter(0.5f))),
      node("ND_subtract_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector2_parameter(pxr::GfVec2f(3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.0f)),
           linked_vector2_scalar),
      node("ND_multiply_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector2_parameter(pxr::GfVec2f(5.0f, 6.0f)),
               TfToken("in2"), float_parameter(3.0f))),
      node("ND_add_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f)),
               TfToken("in2"), float_parameter(0.25f))),
      node("ND_subtract_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector3_parameter(pxr::GfVec3f(4.0f, 5.0f, 6.0f)),
               TfToken("in2"), float_parameter(0.0f)),
           linked_vector3_scalar),
      node("ND_multiply_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector3_parameter(pxr::GfVec3f(7.0f, 8.0f, 9.0f)),
               TfToken("in2"), float_parameter(4.0f)))};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorScalarBroadcastArithmetic"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *scalar = nullptr;
  int add_count = 0;
  int subtract_count = 0;
  int multiply_count = 0;
  int linked_scalar_inputs = 0;
  int literal_add_inputs = 0;
  int literal_multiply_inputs = 0;
  int vector2_outputs = 0;
  int vector3_outputs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ABSOLUTE && math->get_value1() == 2.0f) {
        scalar = math;
      }
    }
  }
  ASSERT_NE(scalar, nullptr);
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      switch (math->get_math_type()) {
        case NODE_MATH_ADD:
          add_count++;
          if (math->input("Value2")->link == nullptr &&
              (math->get_value2() == 0.5f || math->get_value2() == 0.25f))
          {
            literal_add_inputs++;
          }
          break;
        case NODE_MATH_SUBTRACT:
          subtract_count++;
          if (math->input("Value2")->link == scalar->output("Value")) {
            linked_scalar_inputs++;
          }
          break;
        case NODE_MATH_MULTIPLY:
          multiply_count++;
          if (math->input("Value2")->link == nullptr &&
              (math->get_value2() == 3.0f || math->get_value2() == 4.0f))
          {
            literal_multiply_inputs++;
          }
          break;
        default:
          break;
      }
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      if (combine->input("Z")->link) {
        vector3_outputs++;
      }
      else {
        EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
        vector2_outputs++;
      }
    }
  }

  EXPECT_EQ(add_count, 5);
  EXPECT_EQ(subtract_count, 5);
  EXPECT_EQ(multiply_count, 5);
  EXPECT_EQ(linked_scalar_inputs, 5);
  EXPECT_EQ(literal_add_inputs, 5);
  EXPECT_EQ(literal_multiply_inputs, 5);
  EXPECT_EQ(vector2_outputs, 3);
  EXPECT_EQ(vector3_outputs, 3);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_vector23fa_scalar_broadcast_wrong_shapes_atomically)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector23FAPreserveGraph"));
  const HdContainerDataSourceHandle valid_nodes = HdRetainedContainerDataSource::New(
      TfToken("Add"),
      node("ND_add_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector2_parameter(pxr::GfVec2f(1.0f, 2.0f)),
               TfToken("in2"), float_parameter(0.5f))));
  HdCyclesMaterialTestAccess::Populate(
      &material,
      &session,
      HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(valid_nodes).Build()));
  ShaderGraph *preexisting_graph = material.GetCyclesShader()->graph.get();
  ASSERT_NE(preexisting_graph, nullptr);

  const HdContainerDataSourceHandle scalar_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), float_parameter(0.25f), TfToken("in2"), float_parameter(0.5f));
  const HdContainerDataSourceHandle invalid_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Scalar"), TfToken("out")));
  const HdContainerDataSourceHandle invalid_nodes = HdRetainedContainerDataSource::New(
      TfToken("Scalar"), node("ND_add_float", scalar_parameters),
      TfToken("Multiply"),
      node("ND_multiply_vector2FA",
           HdRetainedContainerDataSource::New(TfToken("in2"), float_parameter(2.0f)),
           invalid_connections));
  HdCyclesMaterialTestAccess::Populate(
      &material,
      &session,
      HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(invalid_nodes).Build()));

  EXPECT_EQ(material.GetCyclesShader()->graph.get(), preexisting_graph);
  int add_count = 0;
  int multiply_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      add_count += math->get_math_type() == NODE_MATH_ADD;
      multiply_count += math->get_math_type() == NODE_MATH_MULTIPLY;
    }
  }
  EXPECT_EQ(add_count, 2);
  EXPECT_EQ(multiply_count, 0);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_nonfinite_vector23fa_scalar_broadcast_literals_atomically)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector23FAPreserveFiniteGraph"));
  const HdContainerDataSourceHandle valid_nodes = HdRetainedContainerDataSource::New(
      TfToken("Multiply"),
      node("ND_multiply_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f)),
               TfToken("in2"), float_parameter(2.0f))));
  HdCyclesMaterialTestAccess::Populate(
      &material,
      &session,
      HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(valid_nodes).Build()));
  ShaderGraph *preexisting_graph = material.GetCyclesShader()->graph.get();
  ASSERT_NE(preexisting_graph, nullptr);

  const HdContainerDataSourceHandle invalid_nodes = HdRetainedContainerDataSource::New(
      TfToken("Add"),
      node("ND_add_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"),
               vector2_parameter(pxr::GfVec2f(1.0f, std::numeric_limits<float>::infinity())),
               TfToken("in2"), float_parameter(0.5f))));
  HdCyclesMaterialTestAccess::Populate(
      &material,
      &session,
      HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(invalid_nodes).Build()));

  EXPECT_EQ(material.GetCyclesShader()->graph.get(), preexisting_graph);
  int add_count = 0;
  int multiply_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      add_count += math->get_math_type() == NODE_MATH_ADD;
      multiply_count += math->get_math_type() == NODE_MATH_MULTIPLY;
    }
  }
  EXPECT_EQ(add_count, 0);
  EXPECT_EQ(multiply_count, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_componentwise_vector_and_broadcast_modulo_power)
{
  const std::array<TfToken, 8> node_names = {
      TfToken("ModuloVector2"),   TfToken("ModuloVector3"), TfToken("ModuloVector2FA"),
      TfToken("ModuloVector3FA"), TfToken("PowerVector2"),  TfToken("PowerVector3"),
      TfToken("PowerVector2FA"),  TfToken("PowerVector3FA")};
  const auto vector2_pair = [](const char *identifier) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector2_parameter(pxr::GfVec2f(6.0f, 9.0f)),
                    TfToken("in2"), vector2_parameter(pxr::GfVec2f(2.0f, 3.0f))));
  };
  const auto vector3_pair = [](const char *identifier) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector3_parameter(pxr::GfVec3f(8.0f, 9.0f, 16.0f)),
                    TfToken("in2"), vector3_parameter(pxr::GfVec3f(2.0f, 3.0f, 4.0f))));
  };
  const auto vector2_broadcast = [](const char *identifier, const float exponent) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector2_parameter(pxr::GfVec2f(6.0f, 9.0f)),
                    TfToken("in2"), float_parameter(exponent)));
  };
  const auto vector3_broadcast = [](const char *identifier, const float exponent) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector3_parameter(pxr::GfVec3f(8.0f, 9.0f, 16.0f)),
                    TfToken("in2"), float_parameter(exponent)));
  };
  const std::array<HdDataSourceBaseHandle, 8> node_values = {
      vector2_pair("ND_modulo_vector2"),
      vector3_pair("ND_modulo_vector3"),
      vector2_broadcast("ND_modulo_vector2FA", 2.0f),
      vector3_broadcast("ND_modulo_vector3FA", 2.0f),
      vector2_pair("ND_power_vector2"),
      vector3_pair("ND_power_vector3"),
      vector2_broadcast("ND_power_vector2FA", 3.0f),
      vector3_broadcast("ND_power_vector3FA", 3.0f)};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorModuloPower"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int modulo_count = 0;
  int power_count = 0;
  int modulo_scalar_broadcasts = 0;
  int power_scalar_broadcasts = 0;
  int vector2_outputs = 0;
  int vector3_outputs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_MODULO) {
        modulo_count++;
        if (math->input("Value2")->link == nullptr && math->get_value2() == 2.0f) {
          modulo_scalar_broadcasts++;
        }
      }
      if (math->get_math_type() == NODE_MATH_POWER) {
        power_count++;
        if (math->input("Value2")->link == nullptr && math->get_value2() == 3.0f) {
          power_scalar_broadcasts++;
        }
      }
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      if (combine->input("Z")->link) {
        vector3_outputs++;
      }
      else {
        EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
        vector2_outputs++;
      }
    }
  }
  /* Vector2 + Vector3 + Vector2FA + Vector3FA = 2 + 3 + 2 + 3 components per operation. */
  EXPECT_EQ(modulo_count, 10);
  EXPECT_EQ(power_count, 10);
  EXPECT_EQ(modulo_scalar_broadcasts, 5);
  EXPECT_EQ(power_scalar_broadcasts, 5);
  EXPECT_EQ(vector2_outputs, 4);
  EXPECT_EQ(vector3_outputs, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector2_and_vector3_atan2_with_materialx_y_x_order)
{
  const HdContainerDataSourceHandle vector2_parameters = HdRetainedContainerDataSource::New(
      TfToken("iny"), vector2_parameter(pxr::GfVec2f(1.0f, -2.0f)),
      TfToken("inx"), vector2_parameter(pxr::GfVec2f(3.0f, 4.0f)));
  const HdContainerDataSourceHandle vector3_parameters = HdRetainedContainerDataSource::New(
      TfToken("iny"), vector3_parameter(pxr::GfVec3f(5.0f, -6.0f, 7.0f)),
      TfToken("inx"), vector3_parameter(pxr::GfVec3f(-8.0f, 9.0f, -10.0f)));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Atan2Vector2"), node("ND_atan2_vector2", vector2_parameters),
      TfToken("Atan2Vector3"), node("ND_atan2_vector3", vector3_parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVectorAtan2"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<MathNode *> atan2_nodes;
  std::vector<SeparateXYZNode *> separates;
  std::vector<CombineXYZNode *> combines;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ARCTAN2) {
        atan2_nodes.push_back(math);
      }
    }
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(shader_node)) {
      separates.push_back(separate);
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      combines.push_back(combine);
    }
  }
  /* Two exact Vector2 components plus three Vector3 components. */
  ASSERT_EQ(atan2_nodes.size(), 5);
  ASSERT_EQ(separates.size(), 4);
  ASSERT_EQ(combines.size(), 2);
  int exact_vector2_outputs = 0;
  int vector3_outputs = 0;
  for (CombineXYZNode *combine : combines) {
    if (combine->input("Z")->link == nullptr) {
      EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
      exact_vector2_outputs++;
    }
    else {
      vector3_outputs++;
    }
  }
  EXPECT_EQ(exact_vector2_outputs, 1);
  EXPECT_EQ(vector3_outputs, 1);

  SeparateXYZNode *vector2_y = nullptr;
  SeparateXYZNode *vector2_x = nullptr;
  SeparateXYZNode *vector3_y = nullptr;
  SeparateXYZNode *vector3_x = nullptr;
  for (SeparateXYZNode *separate : separates) {
    if (separate->get_vector() == make_float3(1.0f, -2.0f, 0.0f)) vector2_y = separate;
    if (separate->get_vector() == make_float3(3.0f, 4.0f, 0.0f)) vector2_x = separate;
    if (separate->get_vector() == make_float3(5.0f, -6.0f, 7.0f)) vector3_y = separate;
    if (separate->get_vector() == make_float3(-8.0f, 9.0f, -10.0f)) vector3_x = separate;
  }
  ASSERT_NE(vector2_y, nullptr);
  ASSERT_NE(vector2_x, nullptr);
  ASSERT_NE(vector3_y, nullptr);
  ASSERT_NE(vector3_x, nullptr);

  int vector2_components = 0;
  int vector3_components = 0;
  for (MathNode *math : atan2_nodes) {
    const ShaderOutput *const y = math->input("Value1")->link;
    const ShaderOutput *const x = math->input("Value2")->link;
    if ((y == vector2_y->output("X") && x == vector2_x->output("X")) ||
        (y == vector2_y->output("Y") && x == vector2_x->output("Y")))
    {
      vector2_components++;
    }
    if ((y == vector3_y->output("X") && x == vector3_x->output("X")) ||
        (y == vector3_y->output("Y") && x == vector3_x->output("Y")) ||
        (y == vector3_y->output("Z") && x == vector3_x->output("Z")))
    {
      vector3_components++;
    }
  }
  EXPECT_EQ(vector2_components, 2);
  EXPECT_EQ(vector3_components, 3);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_componentwise_vector3_sqrt_ln_and_exp)
{
  const HdContainerDataSourceHandle ln_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sqrt"), TfToken("out")));
  const HdContainerDataSourceHandle exp_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Ln"), TfToken("out")));
  const auto vector3_node = [](const char *identifier,
                               const HdContainerDataSourceHandle &connections = nullptr) {
    /* Keep every component positive: the MaterialX ln domain is x > 0. */
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 1.0f, 4.0f))),
                connections);
  };
  const std::array<TfToken, 3> node_names = {TfToken("Sqrt"), TfToken("Ln"), TfToken("Exp")};
  const std::array<HdDataSourceBaseHandle, 3> node_values = {
      vector3_node("ND_sqrt_vector3"),
      vector3_node("ND_ln_vector3", ln_connections),
      vector3_node("ND_exp_vector3", exp_connections)};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXComponentwiseVector3UnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int separate_count = 0;
  int combine_count = 0;
  std::array<std::vector<MathNode *>, 3> math_nodes;
  std::vector<SeparateXYZNode *> separates;
  std::vector<CombineXYZNode *> combines;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(node)) {
      ++separate_count;
      separates.push_back(separate);
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(node)) {
      ++combine_count;
      combines.push_back(combine);
    }
    if (MathNode *math = dynamic_cast<MathNode *>(node)) {
      const int operation = math->get_math_type() == NODE_MATH_SQRT ? 0 :
                            math->get_math_type() == NODE_MATH_LOGARITHM ? 1 :
                            math->get_math_type() == NODE_MATH_EXPONENT ? 2 : -1;
      if (operation >= 0) {
        math_nodes[operation].push_back(math);
      }
    }
  }

  EXPECT_EQ(separate_count, 3);
  EXPECT_EQ(combine_count, 3);
  for (const std::vector<MathNode *> &operation : math_nodes) {
    EXPECT_EQ(operation.size(), 3);
  }
  const auto is_linked_from = [](ShaderInput *consumer, const std::vector<ShaderOutput *> &producers) {
    for (ShaderOutput *producer : producers) {
      if (consumer->link == producer) {
        return true;
      }
    }
    return false;
  };
  std::vector<ShaderOutput *> separate_outputs;
  for (SeparateXYZNode *separate : separates) {
    for (const char *channel : {"X", "Y", "Z"}) {
      separate_outputs.push_back(separate->output(channel));
    }
  }
  for (const std::vector<MathNode *> &operation : math_nodes) {
    for (MathNode *math : operation) {
      EXPECT_TRUE(is_linked_from(math->input("Value1"), separate_outputs));
    }
  }
  for (MathNode *math : math_nodes[1]) {
    /* Cycles logarithm is log(Value1, Value2); MaterialX ln is base e. */
    EXPECT_FLOAT_EQ(math->get_value2(), 2.71828182845904523536f);
  }
  material.Finalize(&session);
}


/* Preserved local Hydra MaterialX tests from integration base. */TEST(HdCyclesMaterialXMapping, lowers_integer_conversion_literals_to_broadcast_outputs)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("IntegerToColor"),
      node("ND_convert_integer_color3",
           HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(3))),
      TfToken("IntegerToVector2"),
      node("ND_convert_integer_vector2",
           HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(4))),
      TfToken("IntegerToVector3"),
      node("ND_convert_integer_vector3",
           HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(5))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerConversions"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  ColorNode *color = nullptr;
  CombineXYZNode *vector2 = nullptr;
  CombineXYZNode *vector3 = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    color = color ? color : dynamic_cast<ColorNode *>(node);
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(node)) {
      if (combine->get_z() == 0.0f) {
        vector2 = combine;
      }
      else {
        vector3 = combine;
      }
    }
  }
  ASSERT_NE(color, nullptr);
  ASSERT_NE(vector2, nullptr);
  ASSERT_NE(vector3, nullptr);
  EXPECT_EQ(color->get_value(), make_float3(3.0f));
  EXPECT_FLOAT_EQ(vector2->get_x(), 4.0f);
  EXPECT_FLOAT_EQ(vector2->get_y(), 4.0f);
  EXPECT_FLOAT_EQ(vector2->get_z(), 0.0f);
  EXPECT_FLOAT_EQ(vector3->get_x(), 5.0f);
  EXPECT_FLOAT_EQ(vector3->get_y(), 5.0f);
  EXPECT_FLOAT_EQ(vector3->get_z(), 5.0f);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_linked_integer_conversion_inputs)
{
  const HdContainerDataSourceHandle convert_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Source"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Source"), node("ND_absval_float", 6.0f),
      TfToken("IntegerToColor"),
      node("ND_convert_integer_color3", nullptr, convert_connections),
      TfToken("IntegerToVector2"),
      node("ND_convert_integer_vector2", nullptr, convert_connections),
      TfToken("IntegerToVector3"),
      node("ND_convert_integer_vector3", nullptr, convert_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXRejectLinkedIntegerConversions"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *source = nullptr;
  int color_conversion_count = 0;
  int vector_conversion_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_ABSOLUTE)
    {
      source = math;
    }
    color_conversion_count += dynamic_cast<CombineColorNode *>(shader_node) != nullptr;
    vector_conversion_count += dynamic_cast<CombineXYZNode *>(shader_node) != nullptr;
  }
  ASSERT_NE(source, nullptr);
  EXPECT_EQ(color_conversion_count, 0);
  EXPECT_EQ(vector_conversion_count, 0);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_invert_float_as_amount_minus_input)
{
  const HdContainerDataSourceHandle invert_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), float_parameter(0.25f), TfToken("amount"), float_parameter(0.75f));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Invert"), node("ND_invert_float", invert_parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXInvertFloat"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *invert = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(node);
        math && math->get_math_type() == NODE_MATH_SUBTRACT)
    {
      invert = math;
      break;
    }
  }
  ASSERT_NE(invert, nullptr);
  EXPECT_FLOAT_EQ(invert->get_value1(), 0.75f);
  EXPECT_FLOAT_EQ(invert->get_value2(), 0.25f);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_linked_atan_power_distance_and_invert_endpoints)
{
  const HdContainerDataSourceHandle atan_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("FloatA"), TfToken("out")));
  const HdContainerDataSourceHandle power_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Atan"), TfToken("out")),
      TfToken("in2"), connection(TfToken("FloatB"), TfToken("out")));
  const HdContainerDataSourceHandle invert_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Power"), TfToken("out")),
      TfToken("amount"), connection(TfToken("FloatA"), TfToken("out")));
  const HdContainerDataSourceHandle distance_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("VectorA"), TfToken("out")),
      TfToken("in2"), connection(TfToken("VectorB"), TfToken("out")));
  const HdContainerDataSourceHandle distance_sink_connections =
      HdRetainedContainerDataSource::New(
          TfToken("in"), connection(TfToken("Distance"), TfToken("out")));
  const std::array<TfToken, 9> node_names = {
      TfToken("FloatA"),
      TfToken("FloatB"),
      TfToken("VectorA"),
      TfToken("VectorB"),
      TfToken("Atan"),
      TfToken("Power"),
      TfToken("Invert"),
      TfToken("Distance"),
      TfToken("DistanceSink"),
  };
  const std::array<HdDataSourceBaseHandle, 9> node_values = {
      node("ND_absval_float", 0.25f),
      node("ND_absval_float", 0.75f),
      node("ND_constant_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("value"), vector3_parameter(pxr::GfVec3f(1.0f, 2.0f, 3.0f)))),
      node("ND_constant_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("value"), vector3_parameter(pxr::GfVec3f(4.0f, 5.0f, 6.0f)))),
      node("ND_atan_float", nullptr, atan_connections),
      node("ND_power_float", nullptr, power_connections),
      node("ND_invert_float", nullptr, invert_connections),
      node("ND_distance_vector3", nullptr, distance_connections),
      node("ND_sqrt_float", nullptr, distance_sink_connections),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXLinkedExactMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<MathNode *> absolute_nodes;
  MathNode *atan = nullptr;
  MathNode *power = nullptr;
  MathNode *invert = nullptr;
  MathNode *distance_sink = nullptr;
  VectorMathNode *distance = nullptr;
  std::vector<CombineXYZNode *> vector_constants;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      switch (math->get_math_type()) {
        case NODE_MATH_ABSOLUTE:
          absolute_nodes.push_back(math);
          break;
        case NODE_MATH_ARCTANGENT:
          atan = math;
          break;
        case NODE_MATH_POWER:
          power = math;
          break;
        case NODE_MATH_SUBTRACT:
          invert = math;
          break;
        case NODE_MATH_SQRT:
          distance_sink = math;
          break;
        default:
          break;
      }
    }
    if (VectorMathNode *vector_math = dynamic_cast<VectorMathNode *>(shader_node);
        vector_math && vector_math->get_math_type() == NODE_VECTOR_MATH_DISTANCE)
    {
      distance = vector_math;
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      vector_constants.push_back(combine);
    }
  }

  ASSERT_EQ(absolute_nodes.size(), 2);
  ASSERT_NE(atan, nullptr);
  ASSERT_NE(power, nullptr);
  ASSERT_NE(invert, nullptr);
  ASSERT_NE(distance, nullptr);
  ASSERT_NE(distance_sink, nullptr);
  ASSERT_EQ(vector_constants.size(), 2);

  ShaderOutput *float_a = nullptr;
  ShaderOutput *float_b = nullptr;
  for (MathNode *absolute : absolute_nodes) {
    if (absolute->get_value1() == 0.25f) {
      float_a = absolute->output("Value");
    }
    else if (absolute->get_value1() == 0.75f) {
      float_b = absolute->output("Value");
    }
  }
  ASSERT_NE(float_a, nullptr);
  ASSERT_NE(float_b, nullptr);
  EXPECT_EQ(atan->input("Value1")->link, float_a);
  EXPECT_EQ(power->input("Value1")->link, atan->output("Value"));
  EXPECT_EQ(power->input("Value2")->link, float_b);
  EXPECT_EQ(invert->input("Value1")->link, float_a);
  EXPECT_EQ(invert->input("Value2")->link, power->output("Value"));

  std::vector<ShaderOutput *> vector_outputs;
  for (CombineXYZNode *combine : vector_constants) {
    vector_outputs.push_back(combine->output("Vector"));
  }
  EXPECT_TRUE((distance->input("Vector1")->link == vector_outputs[0] &&
               distance->input("Vector2")->link == vector_outputs[1]) ||
              (distance->input("Vector1")->link == vector_outputs[1] &&
               distance->input("Vector2")->link == vector_outputs[0]));
  EXPECT_EQ(distance_sink->input("Value1")->link, distance->output("Value"));

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_exact_smoothstep_literals)
{
  const std::array<TfToken, 5> names = {
      TfToken("Float"),
      TfToken("Vector2"),
      TfToken("Vector2FA"),
      TfToken("Vector3"),
      TfToken("Vector3FA"),
  };
  const std::array<HdDataSourceBaseHandle, 5> values = {
      node("ND_smoothstep_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("low"), float_parameter(0.25f),
                                              TfToken("high"), float_parameter(0.75f))),
      node("ND_smoothstep_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), vector2_parameter(pxr::GfVec2f(0.0f, 0.25f)),
               TfToken("high"), vector2_parameter(pxr::GfVec2f(1.0f, 1.25f)))),
      node("ND_smoothstep_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), float_parameter(0.0f),
               TfToken("high"), float_parameter(1.0f))),
      node("ND_smoothstep_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.25f, 0.5f)),
               TfToken("high"), vector3_parameter(pxr::GfVec3f(1.0f, 1.25f, 1.5f)))),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), float_parameter(0.0f),
               TfToken("high"), float_parameter(1.0f))),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSmoothstepLiterals"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int scalar_count = 0;
  int vector2_count = 0;
  int vector3_count = 0;
  int smoothstep_math_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MapRangeNode *range = dynamic_cast<MapRangeNode *>(shader_node)) {
      scalar_count++;
      EXPECT_EQ(range->get_range_type(), NODE_MAP_RANGE_SMOOTHSTEP);
      EXPECT_FALSE(range->get_clamp());
      EXPECT_FLOAT_EQ(range->get_value(), 0.5f);
      EXPECT_FLOAT_EQ(range->get_from_min(), 0.25f);
      EXPECT_FLOAT_EQ(range->get_from_max(), 0.75f);
      EXPECT_FLOAT_EQ(range->get_to_min(), 0.0f);
      EXPECT_FLOAT_EQ(range->get_to_max(), 1.0f);
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(shader_node)) {
      ASSERT_NE(combine->input("X")->link, nullptr);
      ASSERT_NE(combine->input("Y")->link, nullptr);
      if (combine->input("Z")->link) {
        vector3_count++;
      }
      else {
        EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
        vector2_count++;
      }
    }
    smoothstep_math_count += dynamic_cast<MathNode *>(shader_node) != nullptr;
  }
  EXPECT_EQ(scalar_count, 1);
  EXPECT_EQ(vector2_count, 2);
  EXPECT_EQ(vector3_count, 2);
  EXPECT_EQ(smoothstep_math_count, 90);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_smoothstep_vector3fa_with_exact_operator_topology)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Smoothstep"),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), float_parameter(0.25f),
               TfToken("high"), float_parameter(0.75f))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSmoothstepVector3FATopology"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  CombineXYZNode *combine = nullptr;
  SeparateXYZNode *input = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    combine = combine ? combine : dynamic_cast<CombineXYZNode *>(shader_node);
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(shader_node)) {
      input = separate;
    }
  }
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(input, nullptr);
  /* Three graph baseline nodes plus one input separator, one output combiner, and
   * nine scalar operators for each of the three vector components. */
  EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 32);

  const auto linked_math = [](ShaderInput *socket, const NodeMathType type) {
    EXPECT_NE(socket, nullptr);
    EXPECT_NE(socket ? socket->link : nullptr, nullptr);
    MathNode *math = socket && socket->link ?
                         dynamic_cast<MathNode *>(socket->link->parent) :
                         nullptr;
    EXPECT_NE(math, nullptr);
    if (math) {
      EXPECT_EQ(math->get_math_type(), type);
    }
    return math;
  };
  for (const char *channel : {"X", "Y", "Z"}) {
    MathNode *result = linked_math(combine->input(channel), NODE_MATH_MULTIPLY);
    ASSERT_NE(result, nullptr);
    MathNode *square = linked_math(result->input("Value1"), NODE_MATH_MULTIPLY);
    MathNode *cubic = linked_math(result->input("Value2"), NODE_MATH_SUBTRACT);
    ASSERT_NE(square, nullptr);
    ASSERT_NE(cubic, nullptr);
    EXPECT_FLOAT_EQ(cubic->get_value1(), 3.0f);

    MathNode *minimum = linked_math(square->input("Value1"), NODE_MATH_MINIMUM);
    ASSERT_NE(minimum, nullptr);
    EXPECT_EQ(square->input("Value2")->link, minimum->output("Value"));
    EXPECT_FLOAT_EQ(minimum->get_value2(), 1.0f);
    MathNode *twice = linked_math(cubic->input("Value2"), NODE_MATH_MULTIPLY);
    ASSERT_NE(twice, nullptr);
    EXPECT_EQ(twice->input("Value1")->link, minimum->output("Value"));
    EXPECT_FLOAT_EQ(twice->get_value2(), 2.0f);

    MathNode *maximum = linked_math(minimum->input("Value1"), NODE_MATH_MAXIMUM);
    ASSERT_NE(maximum, nullptr);
    EXPECT_FLOAT_EQ(maximum->get_value2(), 0.0f);
    MathNode *divide = linked_math(maximum->input("Value1"), NODE_MATH_DIVIDE);
    ASSERT_NE(divide, nullptr);
    MathNode *numerator = linked_math(divide->input("Value1"), NODE_MATH_SUBTRACT);
    MathNode *denominator = linked_math(divide->input("Value2"), NODE_MATH_SUBTRACT);
    ASSERT_NE(numerator, nullptr);
    ASSERT_NE(denominator, nullptr);

    EXPECT_EQ(numerator->input("Value1")->link, input->output(channel));
    EXPECT_EQ(numerator->input("Value2")->link, nullptr);
    EXPECT_FLOAT_EQ(numerator->get_value2(), 0.25f);
    EXPECT_EQ(denominator->input("Value1")->link, nullptr);
    EXPECT_FLOAT_EQ(denominator->get_value1(), 0.75f);
    EXPECT_EQ(denominator->input("Value2")->link, nullptr);
    EXPECT_FLOAT_EQ(denominator->get_value2(), 0.25f);
  }

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_smoothstep_with_linked_values_and_literal_edges)
{
  const HdContainerDataSourceHandle float_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("FloatSource"), TfToken("out")));
  const HdContainerDataSourceHandle vector2_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector2Source"), TfToken("out")));
  const HdContainerDataSourceHandle vector3_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Vector3Source"), TfToken("out")));
  const std::array<TfToken, 8> names = {
      TfToken("FloatSource"),
      TfToken("Float"),
      TfToken("Vector2Source"),
      TfToken("Vector2"),
      TfToken("Vector2FA"),
      TfToken("Vector3Source"),
      TfToken("Vector3"),
      TfToken("Vector3FA"),
  };
  const std::array<HdDataSourceBaseHandle, 8> values = {
      node("ND_absval_float", 0.5f),
      node("ND_smoothstep_float",
           HdRetainedContainerDataSource::New(TfToken("low"), float_parameter(0.25f),
                                              TfToken("high"), float_parameter(0.75f)),
           float_connections),
      node("ND_absval_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)))),
      node("ND_smoothstep_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("low"), vector2_parameter(pxr::GfVec2f(0.0f, 0.25f)),
               TfToken("high"), vector2_parameter(pxr::GfVec2f(1.0f, 1.25f))),
           vector2_connections),
      node("ND_smoothstep_vector2FA",
           HdRetainedContainerDataSource::New(TfToken("low"), float_parameter(0.0f),
                                              TfToken("high"), float_parameter(1.0f)),
           vector2_connections),
      node("ND_absval_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)))),
      node("ND_smoothstep_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.25f, 0.5f)),
               TfToken("high"), vector3_parameter(pxr::GfVec3f(1.0f, 1.25f, 1.5f))),
           vector3_connections),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(TfToken("low"), float_parameter(0.0f),
                                              TfToken("high"), float_parameter(1.0f)),
           vector3_connections),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXSmoothstepLinkedValues"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int linked_scalar_count = 0;
  int linked_vector_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MapRangeNode *range = dynamic_cast<MapRangeNode *>(shader_node)) {
      linked_scalar_count += range->input("Value")->link != nullptr;
    }
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(shader_node)) {
      linked_vector_count += separate->input("Vector")->link != nullptr;
    }
  }
  EXPECT_EQ(linked_scalar_count, 1);
  EXPECT_EQ(linked_vector_count, 4);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_nonincreasing_smoothstep_edges)
{
  const std::array<TfToken, 5> names = {
      TfToken("Float"),
      TfToken("Vector2"),
      TfToken("Vector2FA"),
      TfToken("Vector3"),
      TfToken("Vector3FA"),
  };
  const std::array<HdDataSourceBaseHandle, 5> values = {
      node("ND_smoothstep_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("low"), float_parameter(0.5f),
                                              TfToken("high"), float_parameter(0.5f))),
      node("ND_smoothstep_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), vector2_parameter(pxr::GfVec2f(0.0f, 1.0f)),
               TfToken("high"), vector2_parameter(pxr::GfVec2f(1.0f, 1.0f)))),
      node("ND_smoothstep_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), float_parameter(1.0f),
               TfToken("high"), float_parameter(0.0f))),
      node("ND_smoothstep_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.5f, 1.0f)),
               TfToken("high"), vector3_parameter(pxr::GfVec3f(1.0f, 1.0f, 1.0f)))),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), float_parameter(1.0f),
               TfToken("high"), float_parameter(1.0f))),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXRejectSmoothstepEdges"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int output_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    output_count += dynamic_cast<MapRangeNode *>(shader_node) != nullptr;
    output_count += dynamic_cast<CombineXYZNode *>(shader_node) != nullptr;
  }
  EXPECT_EQ(output_count, 0);
  EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_nonfinite_smoothstep_edges)
{
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  const std::array<TfToken, 5> names = {
      TfToken("Float"),
      TfToken("Vector2"),
      TfToken("Vector2FA"),
      TfToken("Vector3"),
      TfToken("Vector3FA"),
  };
  const std::array<HdDataSourceBaseHandle, 5> values = {
      node("ND_smoothstep_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("low"), float_parameter(nan),
                                              TfToken("high"), float_parameter(1.0f))),
      node("ND_smoothstep_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), vector2_parameter(pxr::GfVec2f(0.0f, infinity)),
               TfToken("high"), vector2_parameter(pxr::GfVec2f(1.0f, 1.0f)))),
      node("ND_smoothstep_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), float_parameter(0.0f),
               TfToken("high"), float_parameter(infinity))),
      node("ND_smoothstep_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.0f, 0.0f)),
               TfToken("high"), vector3_parameter(pxr::GfVec3f(1.0f, nan, 1.0f)))),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), float_parameter(-infinity),
               TfToken("high"), float_parameter(1.0f))),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXRejectNonfiniteSmoothstepEdges"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    EXPECT_EQ(dynamic_cast<MapRangeNode *>(shader_node), nullptr);
    EXPECT_EQ(dynamic_cast<CombineXYZNode *>(shader_node), nullptr);
    EXPECT_EQ(dynamic_cast<SeparateXYZNode *>(shader_node), nullptr);
    EXPECT_EQ(dynamic_cast<MathNode *>(shader_node), nullptr);
  }

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, rejects_linked_smoothstep_edges)
{
  const HdContainerDataSourceHandle edge_connections = HdRetainedContainerDataSource::New(
      TfToken("low"), connection(TfToken("FloatEdge"), TfToken("out")));
  const std::array<TfToken, 6> names = {
      TfToken("FloatEdge"),
      TfToken("Float"),
      TfToken("Vector2"),
      TfToken("Vector2FA"),
      TfToken("Vector3"),
      TfToken("Vector3FA"),
  };
  const std::array<HdDataSourceBaseHandle, 6> values = {
      node("ND_absval_float", 0.0f),
      node("ND_smoothstep_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("high"), float_parameter(1.0f)),
           edge_connections),
      node("ND_smoothstep_vector2",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("low"), vector2_parameter(pxr::GfVec2f(0.0f, 0.0f)),
               TfToken("high"), vector2_parameter(pxr::GfVec2f(1.0f, 1.0f))),
           edge_connections),
      node("ND_smoothstep_vector2FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)),
               TfToken("high"), float_parameter(1.0f)),
           edge_connections),
      node("ND_smoothstep_vector3",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("low"), vector3_parameter(pxr::GfVec3f(0.0f, 0.0f, 0.0f)),
               TfToken("high"), vector3_parameter(pxr::GfVec3f(1.0f, 1.0f, 1.0f))),
           edge_connections),
      node("ND_smoothstep_vector3FA",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector3_parameter(pxr::GfVec3f(0.25f, 0.5f, 0.75f)),
               TfToken("high"), float_parameter(1.0f)),
           edge_connections),
  };
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      names.size(), names.data(), values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXRejectLinkedSmoothstepEdges"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int output_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    output_count += dynamic_cast<MapRangeNode *>(shader_node) != nullptr;
    output_count += dynamic_cast<CombineXYZNode *>(shader_node) != nullptr;
  }
  EXPECT_EQ(output_count, 0);
  /* Three graph baseline nodes plus the one valid upstream edge source. */
  EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 4);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_exact_vector4_unary_math_as_rgba_components)
{
  const HdContainerDataSourceHandle cos_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Abs"), TfToken("out")));
  const HdContainerDataSourceHandle exp_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Cos"), TfToken("out")));
  const HdContainerDataSourceHandle sin_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Exp"), TfToken("out")));
  const auto vector4_node = [](const char *identifier,
                               const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in"), vector4_parameter(pxr::GfVec4f(0.25f, 0.5f, 0.75f, 1.0f))),
                connections);
  };
  const std::array<TfToken, 8> node_names = {TfToken("Abs"),
                                             TfToken("Acos"),
                                             TfToken("Asin"),
                                             TfToken("Cos"),
                                             TfToken("Exp"),
                                             TfToken("Ln"),
                                             TfToken("Sin"),
                                             TfToken("Sqrt")};
  const std::array<HdDataSourceBaseHandle, 8> node_values = {
      vector4_node("ND_absval_vector4"),
      vector4_node("ND_acos_vector4"),
      vector4_node("ND_asin_vector4"),
      vector4_node("ND_cos_vector4", cos_connections),
      vector4_node("ND_exp_vector4", exp_connections),
      vector4_node("ND_ln_vector4"),
      vector4_node("ND_sin_vector4", sin_connections),
      vector4_node("ND_sqrt_vector4")};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXExactVector4UnaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  const std::array<NodeMathType, 8> expected_types = {NODE_MATH_ABSOLUTE,
                                                       NODE_MATH_ARCCOSINE,
                                                       NODE_MATH_ARCSINE,
                                                       NODE_MATH_COSINE,
                                                       NODE_MATH_EXPONENT,
                                                       NODE_MATH_LOGARITHM,
                                                       NODE_MATH_SINE,
                                                       NODE_MATH_SQRT};
  std::array<std::vector<MathNode *>, 8> math_nodes;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      for (size_t i = 0; i < expected_types.size(); i++) {
        if (math->get_math_type() == expected_types[i]) {
          math_nodes[i].push_back(math);
        }
      }
    }
  }

  for (const std::vector<MathNode *> &operation : math_nodes) {
    EXPECT_EQ(operation.size(), 4);
  }
  std::array<bool, 4> saw_absval_literals = {};
  for (MathNode *math : math_nodes[0]) {
    if (math->get_value1() == 0.25f) saw_absval_literals[0] = true;
    if (math->get_value1() == 0.5f) saw_absval_literals[1] = true;
    if (math->get_value1() == 0.75f) saw_absval_literals[2] = true;
    if (math->get_value1() == 1.0f) saw_absval_literals[3] = true;
  }
  for (const bool saw_literal : saw_absval_literals) {
    EXPECT_TRUE(saw_literal);
  }
  for (MathNode *math : math_nodes[5]) {
    /* Cycles logarithm is log(Value1, Value2); MaterialX ln is base e. */
    EXPECT_FLOAT_EQ(math->get_value2(), 2.71828182845904523536f);
  }
  const std::array<std::pair<size_t, size_t>, 3> expected_links = {
      std::pair<size_t, size_t>{3, 0},
      std::pair<size_t, size_t>{4, 3},
      std::pair<size_t, size_t>{6, 4}};
  for (const auto &[operation, previous_operation] : expected_links) {
    int linked_components = 0;
    for (MathNode *math : math_nodes[operation]) {
      for (MathNode *previous : math_nodes[previous_operation]) {
        if (math->input("Value1")->link == previous->output("Value")) {
          linked_components++;
        }
      }
    }
    EXPECT_EQ(linked_components, 4);
  }

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_exact_vector4fa_binary_math_as_scalar_broadcast_components)
{
  const HdContainerDataSourceHandle subtract_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Add"), TfToken("out")));
  const HdContainerDataSourceHandle multiply_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Subtract"), TfToken("out")));
  const HdContainerDataSourceHandle min_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Multiply"), TfToken("out")));
  const HdContainerDataSourceHandle max_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Min"), TfToken("out")));
  const auto vector4fa_node = [](const char *identifier,
                                 const float scalar,
                                 const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(
                    TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
                    TfToken("in2"), float_parameter(scalar)),
                connections);
  };
  const std::array<TfToken, 8> node_names = {TfToken("Add"),
                                             TfToken("Subtract"),
                                             TfToken("Multiply"),
                                             TfToken("Divide"),
                                             TfToken("Min"),
                                             TfToken("Max"),
                                             TfToken("Modulo"),
                                             TfToken("Power")};
  const std::array<HdDataSourceBaseHandle, 8> node_values = {
      vector4fa_node("ND_add_vector4FA", 0.5f),
      vector4fa_node("ND_subtract_vector4FA", 0.25f, subtract_connections),
      vector4fa_node("ND_multiply_vector4FA", 2.0f, multiply_connections),
      vector4fa_node("ND_divide_vector4FA", 2.0f),
      vector4fa_node("ND_min_vector4FA", 3.0f, min_connections),
      vector4fa_node("ND_max_vector4FA", -1.0f, max_connections),
      vector4fa_node("ND_modulo_vector4FA", 2.0f),
      vector4fa_node("ND_power_vector4FA", 2.0f)};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXExactVector4FABinaryMath"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  const std::array<NodeMathType, 8> expected_types = {NODE_MATH_ADD,
                                                      NODE_MATH_SUBTRACT,
                                                      NODE_MATH_MULTIPLY,
                                                      NODE_MATH_DIVIDE,
                                                      NODE_MATH_MINIMUM,
                                                      NODE_MATH_MAXIMUM,
                                                      NODE_MATH_MODULO,
                                                      NODE_MATH_POWER};
  std::array<std::vector<MathNode *>, 8> math_nodes;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      for (size_t i = 0; i < expected_types.size(); i++) {
        if (math->get_math_type() == expected_types[i]) {
          math_nodes[i].push_back(math);
        }
      }
    }
  }

  for (const std::vector<MathNode *> &operation : math_nodes) {
    EXPECT_EQ(operation.size(), 4);
  }
  std::array<bool, 4> saw_add_literals = {};
  for (MathNode *math : math_nodes[0]) {
    if (math->get_value1() == 1.0f) saw_add_literals[0] = true;
    if (math->get_value1() == 2.0f) saw_add_literals[1] = true;
    if (math->get_value1() == 3.0f) saw_add_literals[2] = true;
    if (math->get_value1() == 4.0f) saw_add_literals[3] = true;
    EXPECT_FLOAT_EQ(math->get_value2(), 0.5f);
  }
  for (const bool saw_literal : saw_add_literals) {
    EXPECT_TRUE(saw_literal);
  }
  const std::array<std::pair<size_t, size_t>, 4> linked_operations = {
      std::pair<size_t, size_t>{1, 0},
      std::pair<size_t, size_t>{2, 1},
      std::pair<size_t, size_t>{4, 2},
      std::pair<size_t, size_t>{5, 4},
  };
  for (const auto &[operation, previous_operation] : linked_operations) {
    int linked_components = 0;
    for (MathNode *math : math_nodes[operation]) {
      for (MathNode *previous : math_nodes[previous_operation]) {
        if (math->input("Value1")->link == previous->output("Value")) {
          linked_components++;
        }
      }
    }
    EXPECT_EQ(linked_components, 4);
  }
  for (size_t operation : {3, 6, 7}) {
    for (MathNode *math : math_nodes[operation]) {
      EXPECT_TRUE(math->input("Value1")->link == nullptr);
    }
  }
  for (const auto &operation : math_nodes) {
    for (MathNode *math : operation) {
      EXPECT_TRUE(math->input("Value2")->link == nullptr);
    }
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_unsafe_vector4fa_binary_math_without_graph_mutation)
{
  struct Case {
    const char *identifier;
    pxr::GfVec4f in1;
    float in2;
  };
  const auto vector4fa_math_network = [](const char *identifier,
                                         const pxr::GfVec4f &in1,
                                         const float in2) {
    return HdMaterialNetworkSchema(
        HdMaterialNetworkSchema::Builder()
            .SetNodes(HdRetainedContainerDataSource::New(
                TfToken("Vector4FA"),
                node(identifier,
                     HdRetainedContainerDataSource::New(TfToken("in1"), vector4_parameter(in1),
                                                        TfToken("in2"), float_parameter(in2)))))
            .Build());
  };
  const std::array<Case, 7> cases = {{{"ND_add_vector4FA",
                                       pxr::GfVec4f(1.0f, 2.0f, std::numeric_limits<float>::infinity(), 4.0f),
                                       1.0f},
                                      {"ND_subtract_vector4FA",
                                       pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                       std::numeric_limits<float>::quiet_NaN()},
                                      {"ND_divide_vector4FA",
                                       pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                       0.0f},
                                      {"ND_modulo_vector4FA",
                                       pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                       0.0f},
                                      {"ND_power_vector4FA",
                                       pxr::GfVec4f(-1.0f, 2.0f, 3.0f, 4.0f),
                                       0.5f},
                                      {"ND_power_vector4FA",
                                       pxr::GfVec4f(0.0f, 2.0f, 3.0f, 4.0f),
                                       -1.0f},
                                      {"ND_power_vector4FA",
                                       pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                                       std::numeric_limits<float>::infinity()}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXUnsafeVector4FABinaryMath"));
    HdCyclesMaterialTestAccess::Populate(
        &material, &session, vector4fa_math_network(test.identifier, test.in1, test.in2));

    EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);

    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXMapping, rejects_guarded_vector4fa_binary_math_links_without_mutation)
{
  const auto vector4fa_guarded_network = [](const char *identifier, const float in2) {
    const HdContainerDataSourceHandle guarded_connections = HdRetainedContainerDataSource::New(
        TfToken("in1"), connection(TfToken("Source"), TfToken("out")));
    return HdMaterialNetworkSchema(
        HdMaterialNetworkSchema::Builder()
            .SetNodes(HdRetainedContainerDataSource::New(
                TfToken("Source"),
                node("ND_absval_vector4",
                     HdRetainedContainerDataSource::New(
                         TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
                TfToken("Guarded"),
                node(identifier,
                     HdRetainedContainerDataSource::New(
                         TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
                         TfToken("in2"), float_parameter(in2)),
                     guarded_connections)))
            .Build());
  };

  for (const char *identifier : {"ND_divide_vector4FA", "ND_modulo_vector4FA", "ND_power_vector4FA"}) {
    SCOPED_TRACE(identifier);
    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXGuardedVector4FABinaryMath"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, vector4fa_guarded_network(identifier, 2.0f));

    EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);

    material.Finalize(&session);
  }
}



TEST(HdCyclesMaterialXMapping, lowers_vector4fa_binary_math_with_connected_safe_rhs)
{
  const HdContainerDataSourceHandle add_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Source"), TfToken("out")),
      TfToken("in2"), connection(TfToken("Scalar"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Source"),
      node("ND_absval_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
      TfToken("Scalar"),
      node("ND_constant_float",
           HdRetainedContainerDataSource::New(TfToken("value"), float_parameter(2.0f))),
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(0.0f)),
               TfToken("in2"), float_parameter(1.0f)),
           add_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConnectedVector4FASafeRhs"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  ValueNode *scalar = nullptr;
  std::vector<MathNode *> add_nodes;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (ValueNode *value = dynamic_cast<ValueNode *>(shader_node)) {
      scalar = value;
    }
    else if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
             math && math->get_math_type() == NODE_MATH_ADD)
    {
      add_nodes.push_back(math);
    }
  }
  ASSERT_NE(scalar, nullptr);
  EXPECT_EQ(add_nodes.size(), 4);
  for (MathNode *math : add_nodes) {
    EXPECT_NE(math->input("Value1")->link, nullptr);
    EXPECT_EQ(math->input("Value2")->link, scalar->output("Value"));
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_connected_guarded_vector4fa_rhs_without_mutation)
{
  const HdContainerDataSourceHandle divide_connections = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Scalar"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Scalar"),
      node("ND_constant_float",
           HdRetainedContainerDataSource::New(TfToken("value"), float_parameter(2.0f))),
      TfToken("Divide"),
      node("ND_divide_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(2.0f)),
           divide_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConnectedGuardedVector4FARhs"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int divide_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
             math && math->get_math_type() == NODE_MATH_DIVIDE)
    {
      divide_count++;
    }
  }
  EXPECT_EQ(divide_count, 0);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_zero_power_zero_exponent_without_mutation)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Power"),
      node("ND_power_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(0.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.0f))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXZeroPowerZeroVector4FA"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int power_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_POWER)
    {
      power_count++;
    }
  }
  EXPECT_EQ(power_count, 0);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, vector4fa_output_exposes_w_endpoint_to_downstream_connection)
{
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Strength"), connection(TfToken("Add"), TfToken("outw")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.5f))),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FAOutputW"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *add_w = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ADD && math->get_value1() == 4.0f) {
        add_w = math;
      }
    }
    sink = sink ? sink : dynamic_cast<EmissionNode *>(shader_node);
  }
  ASSERT_NE(add_w, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(sink->input("Strength")->link, add_w->output("Value"));

  material.Finalize(&session);
}

namespace {

HdMaterialNetworkSchema vector4fa_valid_add_network()
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.5f))));
  return HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
}

void expect_invalid_network_preserves_preexisting_vector4fa_graph(
    const HdMaterialNetworkSchema &invalid_network)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FAPreserveGraph"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, vector4fa_valid_add_network());
  ShaderGraph *preexisting_graph = material.GetCyclesShader()->graph.get();
  ASSERT_NE(preexisting_graph, nullptr);

  HdCyclesMaterialTestAccess::Populate(&material, &session, invalid_network);

  EXPECT_EQ(material.GetCyclesShader()->graph.get(), preexisting_graph);
  int add_count = 0;
  int non_add_math_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ADD) {
        add_count++;
      }
      else {
        non_add_math_count++;
      }
    }
  }
  EXPECT_EQ(add_count, 4);
  EXPECT_EQ(non_add_math_count, 0);

  material.Finalize(&session);
}

HdMaterialNetworkSchema vector4fa_network_with_parameters(
    const char *identifier, const HdContainerDataSourceHandle &parameters)
{
  return HdMaterialNetworkSchema(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Math"), node(identifier, parameters)))
          .Build());
}

}  // namespace

TEST(HdCyclesMaterialXMapping, rejects_invalid_vector4fa_network_preserving_preexisting_graph)
{
  expect_invalid_network_preserves_preexisting_vector4fa_graph(vector4fa_network_with_parameters(
      "ND_divide_vector4FA",
      HdRetainedContainerDataSource::New(
          TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
          TfToken("in2"), float_parameter(0.0f))));
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_link_from_vector4_endpoint_into_scalar_in2_atomically)
{
  const HdContainerDataSourceHandle add_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
      TfToken("in2"), float_parameter(0.5f));
  const HdContainerDataSourceHandle multiply_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), vector4_parameter(pxr::GfVec4f(5.0f, 6.0f, 7.0f, 8.0f)));
  const HdContainerDataSourceHandle multiply_connections = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Add"), TfToken("out")));
  const HdMaterialNetworkSchema invalid_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Add"), node("ND_add_vector4FA", add_parameters),
              TfToken("Multiply"),
              node("ND_multiply_vector4FA", multiply_parameters, multiply_connections)))
          .Build());

  expect_invalid_network_preserves_preexisting_vector4fa_graph(invalid_network);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_link_from_scalar_endpoint_into_vector4_in1_atomically)
{
  const HdContainerDataSourceHandle scalar_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), float_parameter(0.25f), TfToken("in2"), float_parameter(0.5f));
  const HdContainerDataSourceHandle add_parameters = HdRetainedContainerDataSource::New(
      TfToken("in2"), float_parameter(0.5f));
  const HdContainerDataSourceHandle add_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Scalar"), TfToken("out")));
  const HdMaterialNetworkSchema invalid_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Scalar"), node("ND_add_float", scalar_parameters),
              TfToken("Add"), node("ND_add_vector4FA", add_parameters, add_connections)))
          .Build());

  expect_invalid_network_preserves_preexisting_vector4fa_graph(invalid_network);
}

TEST(HdCyclesMaterialXMapping, lowers_vector4fa_add_missing_in1_to_nodedef_default)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FADefaultIn1"));
  HdCyclesMaterialTestAccess::Populate(&material,
                                       &session,
                                       vector4fa_network_with_parameters(
                                           "ND_add_vector4FA",
                                           HdRetainedContainerDataSource::New(
                                               TfToken("in2"), float_parameter(0.5f))));

  int add_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_ADD)
    {
      EXPECT_FLOAT_EQ(math->get_value1(), 0.0f);
      EXPECT_FLOAT_EQ(math->get_value2(), 0.5f);
      add_count++;
    }
  }
  EXPECT_EQ(add_count, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_vector4fa_subtract_missing_in2_to_nodedef_default)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FADefaultIn2"));
  HdCyclesMaterialTestAccess::Populate(
      &material,
      &session,
      vector4fa_network_with_parameters(
          "ND_subtract_vector4FA",
          HdRetainedContainerDataSource::New(
              TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))));

  int subtract_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_SUBTRACT)
    {
      EXPECT_FLOAT_EQ(math->get_value2(), 0.0f);
      subtract_count++;
    }
  }
  EXPECT_EQ(subtract_count, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_multiply_wrong_typed_in1_parameter_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4fa_graph(vector4fa_network_with_parameters(
      "ND_multiply_vector4FA",
      HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(1.0f),
                                         TfToken("in2"), float_parameter(0.5f))));
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_min_wrong_typed_in2_parameter_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4fa_graph(vector4fa_network_with_parameters(
      "ND_min_vector4FA",
      HdRetainedContainerDataSource::New(
          TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
          TfToken("in2"), vector4_parameter(pxr::GfVec4f(0.5f, 0.5f, 0.5f, 0.5f)))));
}

TEST(HdCyclesMaterialXMapping, lowers_vector4fa_max_missing_parameters_to_nodedef_defaults)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FADefaultAll"));
  HdCyclesMaterialTestAccess::Populate(
      &material, &session, vector4fa_network_with_parameters("ND_max_vector4FA", nullptr));

  int max_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_MAXIMUM)
    {
      EXPECT_FLOAT_EQ(math->get_value1(), 0.0f);
      EXPECT_FLOAT_EQ(math->get_value2(), 0.0f);
      max_count++;
    }
  }
  EXPECT_EQ(max_count, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4fa_add_wrong_typed_in2_parameter_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4fa_graph(vector4fa_network_with_parameters(
      "ND_add_vector4FA",
      HdRetainedContainerDataSource::New(
          TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
          TfToken("in2"), vector4_parameter(pxr::GfVec4f(0.5f, 0.5f, 0.5f, 0.5f)))));
}

TEST(HdCyclesMaterialXMapping, rejects_nonfinite_vector4_unary_math_without_graph_mutation)
{
  const auto vector4_math_network = [](const char *identifier, const pxr::GfVec4f &value) {
    return HdMaterialNetworkSchema(
        HdMaterialNetworkSchema::Builder()
            .SetNodes(HdRetainedContainerDataSource::New(
                TfToken("Vector4"),
                node(identifier,
                     HdRetainedContainerDataSource::New(TfToken("in"), vector4_parameter(value)))))
            .Build());
  };

  for (const char *identifier : {"ND_absval_vector4",
                                 "ND_acos_vector4",
                                 "ND_asin_vector4",
                                 "ND_cos_vector4",
                                 "ND_exp_vector4",
                                 "ND_ln_vector4",
                                 "ND_sin_vector4",
                                 "ND_sqrt_vector4"})
  {
    SCOPED_TRACE(identifier);
    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXNonfiniteVector4UnaryMath"));
    HdCyclesMaterialTestAccess::Populate(
        &material,
        &session,
        vector4_math_network(
            identifier, pxr::GfVec4f(0.25f, 0.5f, std::numeric_limits<float>::infinity(), 1.0f)));

    EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);

    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXMapping, rejects_out_of_domain_vector4_unary_math_without_graph_mutation)
{
  struct Case {
    const char *identifier;
    pxr::GfVec4f value;
  };
  const auto vector4_math_network = [](const char *identifier, const pxr::GfVec4f &value) {
    return HdMaterialNetworkSchema(
        HdMaterialNetworkSchema::Builder()
            .SetNodes(HdRetainedContainerDataSource::New(
                TfToken("Vector4"),
                node(identifier,
                     HdRetainedContainerDataSource::New(TfToken("in"), vector4_parameter(value)))))
            .Build());
  };
  const std::array<Case, 4> cases = {{{"ND_acos_vector4", pxr::GfVec4f(0.25f, 1.25f, 0.5f, 1.0f)},
                                      {"ND_asin_vector4", pxr::GfVec4f(0.25f, -1.25f, 0.5f, 1.0f)},
                                      {"ND_ln_vector4", pxr::GfVec4f(0.25f, 0.5f, 0.0f, 1.0f)},
                                      {"ND_sqrt_vector4", pxr::GfVec4f(0.25f, 0.5f, -0.25f, 1.0f)}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXDomainVector4UnaryMath"));
    HdCyclesMaterialTestAccess::Populate(
        &material, &session, vector4_math_network(test.identifier, test.value));

    EXPECT_EQ(material.GetCyclesShader()->graph->nodes.size(), 3);

    material.Finalize(&session);
  }
}


TEST(HdCyclesMaterialXMapping, lowers_true_vector4_homogeneous_unary_math_batch)
{
  const auto vector4_node = [](const char *identifier,
                               const pxr::GfVec4f &value,
                               const HdContainerDataSourceHandle &connections = nullptr) {
    return node(identifier,
                HdRetainedContainerDataSource::New(TfToken("in"), vector4_parameter(value)),
                connections);
  };
  const auto invert_node = [](const pxr::GfVec4f &in,
                              const pxr::GfVec4f &amount,
                              const HdContainerDataSourceHandle &connections = nullptr) {
    return node("ND_invert_vector4",
                HdRetainedContainerDataSource::New(TfToken("in"), vector4_parameter(in),
                                                   TfToken("amount"), vector4_parameter(amount)),
                connections);
  };
  const auto safepower_node = [](const pxr::GfVec4f &in1,
                                 const pxr::GfVec4f &in2,
                                 const HdContainerDataSourceHandle &connections = nullptr) {
    return node("ND_safepower_vector4",
                HdRetainedContainerDataSource::New(TfToken("in1"), vector4_parameter(in1),
                                                   TfToken("in2"), vector4_parameter(in2)),
                connections);
  };
  const HdContainerDataSourceHandle ceil_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Abs"), TfToken("out")));
  const HdContainerDataSourceHandle floor_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Ceil"), TfToken("out")));
  const HdContainerDataSourceHandle fract_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Floor"), TfToken("out")));
  const HdContainerDataSourceHandle round_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Fract"), TfToken("out")));
  const HdContainerDataSourceHandle sign_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Round"), TfToken("out")));
  const HdContainerDataSourceHandle invert_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Sign"), TfToken("out")));
  const HdContainerDataSourceHandle safepower_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Invert"), TfToken("out")));
  const std::array<TfToken, 8> node_names = {TfToken("Abs"),
                                             TfToken("Ceil"),
                                             TfToken("Floor"),
                                             TfToken("Fract"),
                                             TfToken("Round"),
                                             TfToken("Sign"),
                                             TfToken("Invert"),
                                             TfToken("SafePower")};
  const std::array<HdDataSourceBaseHandle, 8> node_values = {
      vector4_node("ND_absval_vector4", pxr::GfVec4f(-0.25f, 0.5f, -0.75f, 1.25f)),
      vector4_node("ND_ceil_vector4", pxr::GfVec4f(0.25f), ceil_connections),
      vector4_node("ND_floor_vector4", pxr::GfVec4f(0.25f), floor_connections),
      vector4_node("ND_fract_vector4", pxr::GfVec4f(0.25f), fract_connections),
      vector4_node("ND_round_vector4", pxr::GfVec4f(0.25f), round_connections),
      vector4_node("ND_sign_vector4", pxr::GfVec4f(0.25f), sign_connections),
      invert_node(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f),
                  pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f),
                  invert_connections),
      safepower_node(pxr::GfVec4f(-1.0f, -2.0f, 3.0f, -4.0f),
                     pxr::GfVec4f(2.0f, 3.0f, 0.5f, 1.0f))};
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder()
                                            .SetNodes(HdRetainedContainerDataSource::New(
                                                node_names.size(), node_names.data(), node_values.data()))
                                            .Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXTrueVector4HomogeneousUnaryBatch"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  struct Expected {
    NodeMathType math_type;
    int count;
  };
  const std::array<Expected, 11> expected = {{{NODE_MATH_ABSOLUTE, 8},
                                             {NODE_MATH_CEIL, 4},
                                             {NODE_MATH_FLOOR, 4},
                                             {NODE_MATH_FRACTION, 4},
                                             {NODE_MATH_ROUND, 4},
                                             {NODE_MATH_SIGN, 8},
                                             {NODE_MATH_SUBTRACT, 4},
                                             {NODE_MATH_POWER, 4},
                                             {NODE_MATH_MULTIPLY, 4},
                                             {NODE_MATH_ARCCOSINE, 0},
                                             {NODE_MATH_ARCSINE, 0}}};
  std::array<std::vector<MathNode *>, expected.size()> math_nodes;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      for (size_t i = 0; i < expected.size(); i++) {
        if (math->get_math_type() == expected[i].math_type) {
          math_nodes[i].push_back(math);
        }
      }
    }
  }
  for (size_t i = 0; i < expected.size(); i++) {
    EXPECT_EQ(math_nodes[i].size(), expected[i].count) << i;
  }

  std::array<bool, 4> saw_abs_literals = {};
  for (MathNode *math : math_nodes[0]) {
    if (math->input("Value1")->link == nullptr) {
      if (math->get_value1() == -0.25f) saw_abs_literals[0] = true;
      if (math->get_value1() == 0.5f) saw_abs_literals[1] = true;
      if (math->get_value1() == -0.75f) saw_abs_literals[2] = true;
      if (math->get_value1() == 1.25f) saw_abs_literals[3] = true;
    }
  }
  for (const bool saw_literal : saw_abs_literals) {
    EXPECT_TRUE(saw_literal);
  }
  for (size_t operation = 1; operation <= 5; operation++) {
    int linked_components = 0;
    for (MathNode *math : math_nodes[operation]) {
      for (MathNode *previous : math_nodes[operation - 1]) {
        if (math->input("Value1")->link == previous->output("Value")) {
          linked_components++;
        }
      }
    }
    EXPECT_EQ(linked_components, 4) << operation;
  }
  for (MathNode *subtract : math_nodes[6]) {
    bool in_is_linked_from_sign = false;
    bool amount_literal_present = subtract->get_value1() == 1.0f || subtract->get_value1() == 2.0f ||
                                  subtract->get_value1() == 3.0f || subtract->get_value1() == 4.0f;
    for (MathNode *sign : math_nodes[5]) {
      in_is_linked_from_sign = in_is_linked_from_sign ||
                               subtract->input("Value2")->link == sign->output("Value");
    }
    EXPECT_TRUE(in_is_linked_from_sign);
    EXPECT_TRUE(amount_literal_present);
  }
  for (MathNode *multiply : math_nodes[8]) {
    bool links_from_sign = false;
    bool links_from_power = false;
    for (MathNode *sign : math_nodes[5]) {
      links_from_sign = links_from_sign || multiply->input("Value1")->link == sign->output("Value");
    }
    for (MathNode *power : math_nodes[7]) {
      links_from_power = links_from_power || multiply->input("Value2")->link == power->output("Value");
    }
    EXPECT_TRUE(links_from_sign);
    EXPECT_TRUE(links_from_power);
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, vector4_unary_output_exposes_float4_endpoints)
{
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Strength"), connection(TfToken("Round"), TfToken("outw")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Round"),
      node("ND_round_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(1.25f, 2.25f, 3.25f, 4.25f)))),
      TfToken("Sink"), node("emission", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4UnaryFloat4Endpoints"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *round_w = nullptr;
  EmissionNode *sink = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ROUND && math->get_value1() == 4.25f) {
        round_w = math;
      }
    }
    sink = sink ? sink : dynamic_cast<EmissionNode *>(shader_node);
  }
  ASSERT_NE(round_w, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(sink->input("Strength")->link, round_w->output("Value"));

  material.Finalize(&session);
}

namespace {

HdMaterialNetworkSchema vector4_valid_round_network()
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Abs"),
      node("ND_absval_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(1.25f, 2.25f, 3.25f, 4.25f)))));
  return HdMaterialNetworkSchema(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
}

void expect_invalid_network_preserves_preexisting_vector4_unary_graph(
    const HdMaterialNetworkSchema &invalid_network)
{
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4UnaryPreserveGraph"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, vector4_valid_round_network());
  ShaderGraph *preexisting_graph = material.GetCyclesShader()->graph.get();
  ASSERT_NE(preexisting_graph, nullptr);

  HdCyclesMaterialTestAccess::Populate(&material, &session, invalid_network);

  EXPECT_EQ(material.GetCyclesShader()->graph.get(), preexisting_graph);
  int abs_count = 0;
  int non_abs_math_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ABSOLUTE) {
        abs_count++;
      }
      else {
        non_abs_math_count++;
      }
    }
  }
  EXPECT_EQ(abs_count, 4);
  EXPECT_EQ(non_abs_math_count, 0);

  material.Finalize(&session);
}

HdMaterialNetworkSchema vector4_unary_network_with_parameters(
    const char *identifier, const HdContainerDataSourceHandle &parameters)
{
  return HdMaterialNetworkSchema(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(TfToken("Math"), node(identifier, parameters)))
          .Build());
}

}  // namespace

TEST(HdCyclesMaterialXMapping, rejects_malformed_true_vector4_unary_inputs_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(vector4_unary_network_with_parameters(
      "ND_floor_vector4", HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(1.0f))));

  const HdContainerDataSourceHandle scalar_parameters = HdRetainedContainerDataSource::New(
      TfToken("in1"), float_parameter(0.25f), TfToken("in2"), float_parameter(0.5f));
  const HdContainerDataSourceHandle sign_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Scalar"), TfToken("out")));
  const HdMaterialNetworkSchema invalid_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Scalar"), node("ND_add_float", scalar_parameters),
              TfToken("Sign"), node("ND_sign_vector4", nullptr, sign_connections)))
          .Build());
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(invalid_network);
}

TEST(HdCyclesMaterialXMapping, rejects_guarded_true_vector4_unary_domains_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(vector4_unary_network_with_parameters(
      "ND_safepower_vector4",
      HdRetainedContainerDataSource::New(
          TfToken("in1"), vector4_parameter(pxr::GfVec4f(0.0f, 2.0f, 3.0f, 4.0f)),
          TfToken("in2"), vector4_parameter(pxr::GfVec4f(-1.0f, 2.0f, 3.0f, 4.0f)))));

  const HdContainerDataSourceHandle safepower_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("Source"), TfToken("out")));
  const HdMaterialNetworkSchema guarded_link_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Source"),
              node("ND_absval_vector4",
                   HdRetainedContainerDataSource::New(
                       TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
              TfToken("SafePower"),
              node("ND_safepower_vector4",
                   HdRetainedContainerDataSource::New(
                       TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
                       TfToken("in2"), vector4_parameter(pxr::GfVec4f(2.0f, 2.0f, 2.0f, 2.0f))),
                   safepower_connections)))
          .Build());
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(guarded_link_network);

  for (const char *identifier : {"ND_acos_vector4",
                                 "ND_asin_vector4",
                                 "ND_ln_vector4",
                                 "ND_sqrt_vector4"})
  {
    SCOPED_TRACE(identifier);
    const HdContainerDataSourceHandle guarded_unary_connections = HdRetainedContainerDataSource::New(
        TfToken("in"), connection(TfToken("Source"), TfToken("out")));
    const HdMaterialNetworkSchema guarded_unary_link_network(
        HdMaterialNetworkSchema::Builder()
            .SetNodes(HdRetainedContainerDataSource::New(
                TfToken("Source"),
                node("ND_absval_vector4",
                     HdRetainedContainerDataSource::New(
                         TfToken("in"), vector4_parameter(pxr::GfVec4f(0.25f, 0.5f, 0.75f, 1.0f)))),
                TfToken("Guarded"),
                node(identifier,
                     HdRetainedContainerDataSource::New(
                         TfToken("in"), vector4_parameter(pxr::GfVec4f(0.25f, 0.5f, 0.75f, 1.0f))),
                     guarded_unary_connections)))
            .Build());
    expect_invalid_network_preserves_preexisting_vector4_unary_graph(guarded_unary_link_network);
  }
}

TEST(HdCyclesMaterialXMapping, lowers_vector4_nodedef_default_inputs)
{
  const std::array<TfToken, 21> node_names = {TfToken("Abs"),
                                              TfToken("Acos"),
                                              TfToken("Asin"),
                                              TfToken("Ceil"),
                                              TfToken("Cos"),
                                              TfToken("Exp"),
                                              TfToken("Floor"),
                                              TfToken("Fract"),
                                              TfToken("Ln"),
                                              TfToken("Round"),
                                              TfToken("Sign"),
                                              TfToken("Sin"),
                                              TfToken("Sqrt"),
                                              TfToken("MultiplyFA"),
                                              TfToken("DivideFA"),
                                              TfToken("ModuloFA"),
                                              TfToken("PowerFA"),
                                              TfToken("SafePower"),
                                              TfToken("Invert"),
                                              TfToken("Clamp"),
                                              TfToken("Extract")};
  const std::array<HdDataSourceBaseHandle, 21> node_values = {
      node("ND_absval_vector4", nullptr),
      node("ND_acos_vector4", nullptr),
      node("ND_asin_vector4", nullptr),
      node("ND_ceil_vector4", nullptr),
      node("ND_cos_vector4", nullptr),
      node("ND_exp_vector4", nullptr),
      node("ND_floor_vector4", nullptr),
      node("ND_fract_vector4", nullptr),
      node("ND_ln_vector4", nullptr),
      node("ND_round_vector4", nullptr),
      node("ND_sign_vector4", nullptr),
      node("ND_sin_vector4", nullptr),
      node("ND_sqrt_vector4", nullptr),
      node("ND_multiply_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(2.0f, 3.0f, 4.0f, 5.0f)))),
      node("ND_divide_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(2.0f, 4.0f, 6.0f, 8.0f)))),
      node("ND_modulo_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(2.0f, 4.0f, 6.0f, 8.0f)))),
      node("ND_power_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(2.0f, 4.0f, 6.0f, 8.0f)))),
      node("ND_safepower_vector4", nullptr),
      node("ND_invert_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f)))),
      node("ND_clamp_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(0.25f, 0.5f, 0.75f, 1.25f)))),
      node("ND_extract_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(8.0f, 9.0f, 10.0f, 11.0f))))};
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      node_names.size(), node_names.data(), node_values.data());
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4NodeDefDefaults"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int unary_default_zero_count = 0;
  int unary_default_one_count = 0;
  int multiply_count = 0;
  int divide_count = 0;
  int modulo_count = 0;
  int power_count = 0;
  int safepower_sign_count = 0;
  int invert_count = 0;
  int clamp_low_count = 0;
  int clamp_high_count = 0;
  int extract_default_index_count = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      switch (math->get_math_type()) {
        case NODE_MATH_ABSOLUTE:
        case NODE_MATH_ARCCOSINE:
        case NODE_MATH_ARCSINE:
        case NODE_MATH_CEIL:
        case NODE_MATH_COSINE:
        case NODE_MATH_EXPONENT:
        case NODE_MATH_FLOOR:
        case NODE_MATH_FRACTION:
        case NODE_MATH_ROUND:
        case NODE_MATH_SINE:
        case NODE_MATH_SQRT:
          if (math->get_value1() == 0.0f) {
            unary_default_zero_count++;
          }
          break;
        case NODE_MATH_LOGARITHM:
          if (math->get_value1() == 1.0f) {
            unary_default_one_count++;
          }
          EXPECT_FLOAT_EQ(math->get_value2(), 2.71828182845904523536f);
          break;
        case NODE_MATH_MULTIPLY:
          if (math->get_value2() == 1.0f) {
            multiply_count++;
          }
          break;
        case NODE_MATH_DIVIDE:
          if (math->get_value2() == 1.0f) {
            divide_count++;
          }
          break;
        case NODE_MATH_MODULO:
          if (math->get_value2() == 1.0f) {
            modulo_count++;
          }
          break;
        case NODE_MATH_POWER:
          if (math->get_value2() == 1.0f) {
            power_count++;
          }
          break;
        case NODE_MATH_SIGN:
          if (math->get_value1() == 0.0f) {
            safepower_sign_count++;
          }
          break;
        case NODE_MATH_SUBTRACT:
          if (math->get_value1() == 1.0f) {
            invert_count++;
          }
          break;
        case NODE_MATH_MAXIMUM:
          if (math->get_value2() == 0.0f) {
            clamp_low_count++;
          }
          break;
        case NODE_MATH_MINIMUM:
          if (math->get_value2() == 1.0f) {
            clamp_high_count++;
          }
          break;
        case NODE_MATH_ADD:
          if (math->get_value1() == 8.0f) {
            extract_default_index_count++;
          }
          break;
        default:
          break;
      }
    }
  }
  EXPECT_EQ(unary_default_zero_count, 48);
  EXPECT_EQ(unary_default_one_count, 4);
  EXPECT_EQ(multiply_count, 4);
  EXPECT_EQ(divide_count, 4);
  EXPECT_EQ(modulo_count, 4);
  EXPECT_EQ(power_count, 8);
  EXPECT_EQ(safepower_sign_count, 8);
  EXPECT_EQ(invert_count, 4);
  EXPECT_EQ(clamp_low_count, 4);
  EXPECT_EQ(clamp_high_count, 4);
  EXPECT_EQ(extract_default_index_count, 1);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_invalid_extract_vector4_indices_atomically)
{
  for (const int invalid_index : {-1, 4}) {
    SCOPED_TRACE(invalid_index);
    expect_invalid_network_preserves_preexisting_vector4_unary_graph(vector4_unary_network_with_parameters(
        "ND_extract_vector4",
        HdRetainedContainerDataSource::New(
            TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
            TfToken("index"), int_parameter(invalid_index))));
  }
}

TEST(HdCyclesMaterialXMapping, accepts_vector4fa_rhs_from_power_float_producer)
{
  const HdContainerDataSourceHandle add_connections = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Power"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Power"),
      node("ND_power_float",
           HdRetainedContainerDataSource::New(TfToken("in1"), float_parameter(2.0f),
                                              TfToken("in2"), float_parameter(3.0f))),
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.5f)),
           add_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FAPowerFloatProducer"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *power = nullptr;
  std::vector<MathNode *> adds;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_POWER) {
        power = math;
      }
      else if (math->get_math_type() == NODE_MATH_ADD) {
        adds.push_back(math);
      }
    }
  }
  ASSERT_NE(power, nullptr);
  EXPECT_EQ(adds.size(), 4);
  for (MathNode *add : adds) {
    EXPECT_EQ(add->input("Value2")->link, power->output("Value"));
  }

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, accepts_vector4fa_rhs_from_extract_vector4_producer)
{
  const HdContainerDataSourceHandle extract_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Source"), TfToken("out")));
  const HdContainerDataSourceHandle add_connections = HdRetainedContainerDataSource::New(
      TfToken("in2"), connection(TfToken("Extract"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Source"),
      node("ND_absval_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
      TfToken("Extract"),
      node("ND_extract_vector4",
           HdRetainedContainerDataSource::New(TfToken("in"), vector4_parameter(pxr::GfVec4f(0.0f)),
                                              TfToken("index"), int_parameter(2)),
           extract_connections),
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.5f)),
           add_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FAExtractVector4Producer"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<MathNode *> add_nodes;
  ShaderOutput *extract_output = nullptr;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() != NODE_MATH_ADD || math->input("Value2")->link == nullptr) {
        continue;
      }
      add_nodes.push_back(math);
      extract_output = extract_output ? extract_output : math->input("Value2")->link;
      EXPECT_EQ(math->input("Value2")->link, extract_output);
    }
  }
  ASSERT_NE(extract_output, nullptr);
  EXPECT_EQ(add_nodes.size(), 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, accepts_true_vector4_unary_input_from_add_vector4_producer)
{
  const HdContainerDataSourceHandle ceil_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Add"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Add"),
      node("ND_add_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.1f, 2.2f, 3.3f, 4.4f)),
               TfToken("in2"), vector4_parameter(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f)))),
      TfToken("Ceil"),
      node("ND_ceil_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(0.0f))),
           ceil_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4UnaryAddVector4Producer"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  std::vector<MathNode *> add_nodes;
  std::vector<MathNode *> ceil_nodes;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      if (math->get_math_type() == NODE_MATH_ADD) {
        add_nodes.push_back(math);
      }
      else if (math->get_math_type() == NODE_MATH_CEIL) {
        ceil_nodes.push_back(math);
      }
    }
  }
  EXPECT_EQ(add_nodes.size(), 4);
  EXPECT_EQ(ceil_nodes.size(), 4);
  int linked_components = 0;
  for (MathNode *ceil : ceil_nodes) {
    for (MathNode *add : add_nodes) {
      if (ceil->input("Value1")->link == add->output("Value")) {
        linked_components++;
      }
    }
  }
  EXPECT_EQ(linked_components, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, accepts_vector4fa_from_clamp_vector4_and_unary_float_producers)
{
  const HdContainerDataSourceHandle add_connections = HdRetainedContainerDataSource::New(
      TfToken("in1"), connection(TfToken("ClampVector"), TfToken("out")),
      TfToken("in2"), connection(TfToken("ClampFloat"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("ClampVector"),
      node("ND_clamp_vector4",
           HdRetainedContainerDataSource::New(
               TfToken("in"), vector4_parameter(pxr::GfVec4f(0.1f, 0.2f, 0.3f, 0.4f)),
               TfToken("low"), vector4_parameter(pxr::GfVec4f(0.0f)),
               TfToken("high"), vector4_parameter(pxr::GfVec4f(1.0f)))),
      TfToken("ClampFloat"),
      node("ND_clamp_float",
           HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f),
                                              TfToken("low"), float_parameter(0.0f),
                                              TfToken("high"), float_parameter(1.0f))),
      TfToken("Add"),
      node("ND_add_vector4FA",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
               TfToken("in2"), float_parameter(0.5f)),
           add_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXVector4FAClampProducers"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int vector4fa_adds = 0;
  int linked_scalar_rhs = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node);
        math && math->get_math_type() == NODE_MATH_ADD)
    {
      vector4fa_adds++;
      linked_scalar_rhs += math->input("Value2")->link != nullptr;
    }
  }
  EXPECT_EQ(vector4fa_adds, 4);
  EXPECT_EQ(linked_scalar_rhs, 4);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4_wrong_and_missing_upstream_endpoints_atomically)
{
  const HdContainerDataSourceHandle wrong_endpoint_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), connection(TfToken("Source"), TfToken("outq")));
  const HdMaterialNetworkSchema wrong_endpoint_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Source"),
              node("ND_absval_vector4",
                   HdRetainedContainerDataSource::New(
                       TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
              TfToken("Ceil"),
              node("ND_ceil_vector4", nullptr, wrong_endpoint_connections)))
          .Build());
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(wrong_endpoint_network);

  const HdDataSourceBaseHandle missing_output_source = HdMaterialConnectionSchema::Builder()
                                                     .SetUpstreamNodePath(
                                                         HdRetainedTypedSampledDataSource<TfToken>::New(
                                                             TfToken("Source")))
                                                     .Build();
  const HdDataSourceBaseHandle missing_output_connection = HdVectorSchema::BuildRetained(
      1, &missing_output_source);
  const HdContainerDataSourceHandle missing_output_connections = HdRetainedContainerDataSource::New(
      TfToken("in"), missing_output_connection);
  const HdMaterialNetworkSchema missing_endpoint_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Source"),
              node("ND_absval_vector4",
                   HdRetainedContainerDataSource::New(
                       TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)))),
              TfToken("Ceil"), node("ND_ceil_vector4", nullptr, missing_output_connections)))
          .Build());
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(missing_endpoint_network);
}

TEST(HdCyclesMaterialXMapping, rejects_vector4_and_vector4fa_extra_sockets_atomically)
{
  expect_invalid_network_preserves_preexisting_vector4_unary_graph(vector4_unary_network_with_parameters(
      "ND_ceil_vector4",
      HdRetainedContainerDataSource::New(
          TfToken("in"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
          TfToken("extra"), float_parameter(1.0f))));

  const HdContainerDataSourceHandle extra_connection = HdRetainedContainerDataSource::New(
      TfToken("extra"), connection(TfToken("Source"), TfToken("out")));
  const HdMaterialNetworkSchema vector4fa_extra_connection_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Source"),
              node("ND_constant_float",
                   HdRetainedContainerDataSource::New(TfToken("value"), float_parameter(1.0f))),
              TfToken("Add"),
              node("ND_add_vector4FA",
                   HdRetainedContainerDataSource::New(
                       TfToken("in1"), vector4_parameter(pxr::GfVec4f(1.0f, 2.0f, 3.0f, 4.0f)),
                       TfToken("in2"), float_parameter(0.5f)),
                   extra_connection)))
          .Build());
  expect_invalid_network_preserves_preexisting_vector4fa_graph(vector4fa_extra_connection_network);
}

TEST(HdCyclesMaterialXMapping, lowers_vector2_to_vector3_with_explicit_xy_and_zero_z)
{
  const HdContainerDataSourceHandle convert_parameters = HdRetainedContainerDataSource::New(
      TfToken("in"), vector2_parameter(pxr::GfVec2f(0.25f, 0.75f)));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Vector"), connection(TfToken("Convert"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Convert"), node("ND_convert_vector2_vector3", convert_parameters),
      TfToken("Sink"), node("mapping", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXConvertVector2Vector3"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateXYZNode *separate = nullptr;
  CombineXYZNode *combine = nullptr;
  ConvertNode *convert = nullptr;
  MappingNode *sink = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    separate = separate ? separate : dynamic_cast<SeparateXYZNode *>(node);
    combine = combine ? combine : dynamic_cast<CombineXYZNode *>(node);
    convert = convert ? convert : dynamic_cast<ConvertNode *>(node);
    sink = sink ? sink : dynamic_cast<MappingNode *>(node);
  }
  ASSERT_NE(separate, nullptr);
  ASSERT_NE(combine, nullptr);
  ASSERT_NE(convert, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_EQ(separate->get_vector(), make_float3(0.25f, 0.75f, 0.0f));
  EXPECT_EQ(combine->input("X")->link, separate->output("X"));
  EXPECT_EQ(combine->input("Y")->link, separate->output("Y"));
  EXPECT_FLOAT_EQ(combine->get_z(), 0.0f);
  EXPECT_EQ(convert->inputs[0]->link, combine->output("Vector"));
  EXPECT_EQ(sink->input("Vector")->link, convert->outputs[0]);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_place2d_with_explicit_literals_and_srt_trs_routing)
{
  const HdContainerDataSourceHandle place_parameters = HdRetainedContainerDataSource::New(
      TfToken("texcoord"), vector2_parameter(pxr::GfVec2f(0.4f, 0.6f)),
      TfToken("pivot"), vector2_parameter(pxr::GfVec2f(0.2f, 0.7f)),
      TfToken("scale"), vector2_parameter(pxr::GfVec2f(2.0f, 3.0f)),
      TfToken("rotate"), float_parameter(30.0f),
      TfToken("offset"), vector2_parameter(pxr::GfVec2f(0.1f, -0.2f)),
      TfToken("operationorder"), int_parameter(1));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Vector"), connection(TfToken("Place"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Place"), node("ND_place2d_vector2", place_parameters),
      TfToken("Sink"), node("mapping", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXPlace2d"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  MathNode *radians = nullptr;
  ConvertNode *convert = nullptr;
  MixVectorNode *operation_order = nullptr;
  MappingNode *sink = nullptr;
  std::vector<VectorRotateNode *> rotates;
  int rotate_count = 0;
  int subtract_count = 0;
  int divide_count = 0;
  int add_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(node);
        math && math->get_math_type() == NODE_MATH_RADIANS)
    {
      radians = math;
    }
    if (VectorRotateNode *rotate = dynamic_cast<VectorRotateNode *>(node)) {
      rotates.push_back(rotate);
      ++rotate_count;
      EXPECT_EQ(rotate->get_rotate_type(), NODE_VECTOR_ROTATE_TYPE_AXIS_Z);
      EXPECT_TRUE(rotate->get_invert());
    }
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node)) {
      subtract_count += math->get_math_type() == NODE_VECTOR_MATH_SUBTRACT;
      divide_count += math->get_math_type() == NODE_VECTOR_MATH_DIVIDE;
      add_count += math->get_math_type() == NODE_VECTOR_MATH_ADD;
    }
    convert = convert ? convert : dynamic_cast<ConvertNode *>(node);
    operation_order = operation_order ? operation_order : dynamic_cast<MixVectorNode *>(node);
    sink = sink ? sink : dynamic_cast<MappingNode *>(node);
  }
  ASSERT_NE(radians, nullptr);
  ASSERT_NE(convert, nullptr);
  ASSERT_NE(operation_order, nullptr);
  ASSERT_NE(sink, nullptr);
  EXPECT_FLOAT_EQ(radians->get_value1(), 30.0f);
  EXPECT_FLOAT_EQ(operation_order->get_fac(), 1.0f);
  EXPECT_EQ(rotate_count, 2);
  for (VectorRotateNode *rotate : rotates) {
    EXPECT_EQ(rotate->input("Angle")->link, radians->output("Value"));
  }
  EXPECT_EQ(subtract_count, 3);
  EXPECT_EQ(divide_count, 2);
  EXPECT_EQ(add_count, 2);
  EXPECT_EQ(convert->inputs[0]->link, operation_order->output("Result"));
  EXPECT_EQ(sink->input("Vector")->link, convert->outputs[0]);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_place2d_omitted_scale_to_a_unit_scale_adapter)
{
  const HdContainerDataSourceHandle place_parameters = HdRetainedContainerDataSource::New(
      TfToken("texcoord"), vector2_parameter(pxr::GfVec2f(0.4f, 0.6f)),
      TfToken("pivot"), vector2_parameter(pxr::GfVec2f(0.2f, 0.7f)),
      TfToken("rotate"), float_parameter(30.0f),
      TfToken("offset"), vector2_parameter(pxr::GfVec2f(0.1f, -0.2f)),
      TfToken("operationorder"), int_parameter(0));
  const HdContainerDataSourceHandle sink_connections = HdRetainedContainerDataSource::New(
      TfToken("Vector"), connection(TfToken("Place"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Place"), node("ND_place2d_vector2", place_parameters),
      TfToken("Sink"), node("mapping", nullptr, sink_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXPlace2dDefaultScale"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  SeparateXYZNode *unit_scale_source = nullptr;
  CombineXYZNode *unit_scale = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(node);
        separate && separate->get_vector() == make_float3(1.0f, 1.0f, 1.0f))
    {
      for (ShaderNode *candidate : material.GetCyclesShader()->graph->nodes) {
        CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(candidate);
        if (combine && combine->get_z() == 1.0f &&
            combine->input("X")->link == separate->output("X") &&
            combine->input("Y")->link == separate->output("Y")) {
          unit_scale_source = separate;
          unit_scale = combine;
          break;
        }
      }
    }
  }
  ASSERT_NE(unit_scale_source, nullptr);
  ASSERT_NE(unit_scale, nullptr);

  material.Finalize(&session);
}


TEST(HdCyclesMaterialXMapping, lowers_scalar_integer_boolean_utility_batch)
{
  struct Case {
    const char *name;
    const char *identifier;
    NodeMathType math_type;
    HdContainerDataSourceHandle parameters;
  };
  const std::array<Case, 34> cases = {{{"AddInteger", "ND_add_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in1"), int_parameter(2), TfToken("in2"), int_parameter(3))},
                                       {"SubtractInteger", "ND_subtract_integer", NODE_MATH_SUBTRACT,
                                        HdRetainedContainerDataSource::New(TfToken("in1"), int_parameter(7), TfToken("in2"), int_parameter(4))},
                                       {"FloorInteger", "ND_floor_integer", NODE_MATH_FLOOR,
                                        HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(2.75f))},
                                       {"CeilInteger", "ND_ceil_integer", NODE_MATH_CEIL,
                                        HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(2.25f))},
                                       {"RoundInteger", "ND_round_integer", NODE_MATH_ROUND,
                                        HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(2.5f))},
                                       {"IfGreaterInteger", "ND_ifgreater_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(2.0f), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfGreaterEqInteger", "ND_ifgreatereq_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(3.0f), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfEqualInteger", "ND_ifequal_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(3.0f), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfGreaterBoolean", "ND_ifgreater_boolean", NODE_MATH_GREATER_THAN,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(2.0f))},
                                       {"IfGreaterEqBoolean", "ND_ifgreatereq_boolean", NODE_MATH_MAXIMUM,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(3.0f))},
                                       {"IfEqualBoolean", "ND_ifequal_boolean", NODE_MATH_COMPARE,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), float_parameter(3.0f), TfToken("value2"), float_parameter(3.0f))},
                                       {"IfGreaterFloatI", "ND_ifgreater_floatI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(2), TfToken("in1"), float_parameter(0.9f), TfToken("in2"), float_parameter(0.1f))},
                                       {"IfGreaterIntegerI", "ND_ifgreater_integerI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(2), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfGreaterBooleanI", "ND_ifgreater_booleanI", NODE_MATH_GREATER_THAN,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(2))},
                                       {"IfEqualFloatB", "ND_ifequal_floatB", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), bool_parameter(true), TfToken("value2"), bool_parameter(true), TfToken("in1"), float_parameter(0.9f), TfToken("in2"), float_parameter(0.1f))},
                                       {"IfEqualIntegerB", "ND_ifequal_integerB", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), bool_parameter(true), TfToken("value2"), bool_parameter(true), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfEqualBooleanB", "ND_ifequal_booleanB", NODE_MATH_COMPARE,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), bool_parameter(true), TfToken("value2"), bool_parameter(true))},
                                       {"LogicalAnd", "ND_logical_and", NODE_MATH_MULTIPLY,
                                        HdRetainedContainerDataSource::New(TfToken("in1"), bool_parameter(true), TfToken("in2"), bool_parameter(false))},
                                       {"LogicalOr", "ND_logical_or", NODE_MATH_MAXIMUM,
                                        HdRetainedContainerDataSource::New(TfToken("in1"), bool_parameter(true), TfToken("in2"), bool_parameter(false))},
                                       {"IfGreaterEqFloatI", "ND_ifgreatereq_floatI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3), TfToken("in1"), float_parameter(0.9f), TfToken("in2"), float_parameter(0.1f))},
                                       {"IfEqualFloatI", "ND_ifequal_floatI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3), TfToken("in1"), float_parameter(0.9f), TfToken("in2"), float_parameter(0.1f))},
                                       {"IfGreaterEqIntegerI", "ND_ifgreatereq_integerI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfEqualIntegerI", "ND_ifequal_integerI", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3), TfToken("in1"), int_parameter(9), TfToken("in2"), int_parameter(1))},
                                       {"IfGreaterEqBooleanI", "ND_ifgreatereq_booleanI", NODE_MATH_MAXIMUM,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3))},
                                       {"IfEqualBooleanI", "ND_ifequal_booleanI", NODE_MATH_COMPARE,
                                        HdRetainedContainerDataSource::New(TfToken("value1"), int_parameter(3), TfToken("value2"), int_parameter(3))},
                                       {"LogicalXor", "ND_logical_xor", NODE_MATH_MODULO,
                                        HdRetainedContainerDataSource::New(TfToken("in1"), bool_parameter(true), TfToken("in2"), bool_parameter(false))},
                                       {"LogicalNot", "ND_logical_not", NODE_MATH_SUBTRACT,
                                        HdRetainedContainerDataSource::New(TfToken("in"), bool_parameter(false))},
                                       {"ConvertBooleanFloat", "ND_convert_boolean_float", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), bool_parameter(true))},
                                       {"ConvertBooleanInteger", "ND_convert_boolean_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), bool_parameter(true))},
                                       {"ConvertIntegerFloat", "ND_convert_integer_float", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(4))},
                                       {"ConvertIntegerBoolean", "ND_convert_integer_boolean", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(1))},
                                       {"DotFloat", "ND_dot_float", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), float_parameter(0.5f), TfToken("note"), string_parameter("note"))},
                                       {"DotInteger", "ND_dot_integer", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), int_parameter(2), TfToken("note"), string_parameter("note"))},
                                       {"DotBoolean", "ND_dot_boolean", NODE_MATH_ADD,
                                        HdRetainedContainerDataSource::New(TfToken("in"), bool_parameter(true), TfToken("note"), string_parameter("note"))}}};

  for (const Case &test : cases) {
    SCOPED_TRACE(test.identifier);
    const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
        TfToken(test.name), node(test.identifier, test.parameters));
    const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
    HdCyclesSession session{SessionParams()};
    HdCyclesMaterial material(SdfPath("/MaterialXScalarIntegerBoolean"));
    HdCyclesMaterialTestAccess::Populate(&material, &session, network);
    const std::string identifier(test.identifier);
    const bool exact_integer_literal = identifier == "ND_add_integer" ||
                                       identifier == "ND_subtract_integer" ||
                                       identifier == "ND_floor_integer" ||
                                       identifier == "ND_ceil_integer" ||
                                       identifier == "ND_round_integer";
    bool found = false;
    for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
      if (exact_integer_literal) {
        found |= dynamic_cast<ValueNode *>(shader_node) != nullptr;
      }
      else if (const MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
        found |= math->get_math_type() == test.math_type;
      }
    }
    EXPECT_TRUE(found) << test.identifier;
    material.Finalize(&session);
  }
}

TEST(HdCyclesMaterialXMapping, lowers_scalar_integer_boolean_generic_links)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("True"), node("ND_constant_boolean", HdRetainedContainerDataSource::New(TfToken("value"), bool_parameter(true))),
      TfToken("False"), node("ND_constant_boolean", HdRetainedContainerDataSource::New(TfToken("value"), bool_parameter(false))),
      TfToken("One"), node("ND_constant_integer", HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(1))),
      TfToken("Two"), node("ND_constant_integer", HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(2))),
      TfToken("And"), node("ND_logical_and", HdRetainedContainerDataSource::New(TfToken("in1"), bool_parameter(true), TfToken("in2"), bool_parameter(false)),
                            HdRetainedContainerDataSource::New(TfToken("in1"), connection(TfToken("True"), TfToken("out")), TfToken("in2"), connection(TfToken("False"), TfToken("out")))),
      TfToken("Add"), node("ND_add_integer", HdRetainedContainerDataSource::New(TfToken("in1"), int_parameter(0), TfToken("in2"), int_parameter(0)),
                            HdRetainedContainerDataSource::New(TfToken("in1"), connection(TfToken("One"), TfToken("out")), TfToken("in2"), connection(TfToken("Two"), TfToken("out")))));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXScalarIntegerBooleanLinks"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);
  int values = 0, adds = 0, products = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    values += dynamic_cast<ValueNode *>(shader_node) != nullptr;
    if (const MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      adds += math->get_math_type() == NODE_MATH_ADD;
      products += math->get_math_type() == NODE_MATH_MULTIPLY;
    }
  }
  EXPECT_GE(values, 4);
  EXPECT_GE(adds, 1);
  EXPECT_GE(products, 1);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, reconciles_exact_integer_literals_and_validated_links)
{
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("One"),
      node("ND_constant_integer",
           HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(1))),
      TfToken("Two"),
      node("ND_constant_integer",
           HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(2))),
      TfToken("LinkedAdd"),
      node("ND_add_integer",
           nullptr,
           HdRetainedContainerDataSource::New(TfToken("in1"),
                                              connection(TfToken("One"), TfToken("out")),
                                              TfToken("in2"),
                                              connection(TfToken("Two"), TfToken("out")))),
      TfToken("LiteralAdd"),
      node("ND_add_integer",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), int_parameter(7), TfToken("in2"), int_parameter(9))));
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerHybrid"));
  HdCyclesMaterialTestAccess::Populate(
      &material, &session, HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  int linked_adds = 0;
  int exact_sixteen = 0;
  for (ShaderNode *shader_node : material.GetCyclesShader()->graph->nodes) {
    if (MathNode *math = dynamic_cast<MathNode *>(shader_node)) {
      linked_adds += math->get_math_type() == NODE_MATH_ADD &&
                     math->input("Value1")->link != nullptr &&
                     math->input("Value2")->link != nullptr;
    }
    if (ValueNode *value = dynamic_cast<ValueNode *>(shader_node)) {
      exact_sixteen += value->get_value() == 16.0f;
    }
  }
  EXPECT_EQ(linked_adds, 1);
  EXPECT_EQ(exact_sixteen, 1);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_nonrepresentable_integer_results_atomically)
{
  const auto network = [](const int in1, const int in2) {
    return HdMaterialNetworkSchema::Builder()
        .SetNodes(HdRetainedContainerDataSource::New(
            TfToken("Add"),
            node("ND_add_integer",
                 HdRetainedContainerDataSource::New(
                     TfToken("in1"), int_parameter(in1), TfToken("in2"), int_parameter(in2)))))
        .Build();
  };
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerAtomic"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network(1, 2));
  ShaderGraph *const original = material.GetCyclesShader()->graph.get();

  HdCyclesMaterialTestAccess::Populate(
      &material, &session, network(std::numeric_limits<int>::max(), 1));
  EXPECT_EQ(material.GetCyclesShader()->graph.get(), original);
  HdCyclesMaterialTestAccess::Populate(&material, &session, network(16777216, 1));
  EXPECT_EQ(material.GetCyclesShader()->graph.get(), original);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_integer_link_multiplicity_atomically)
{
  const HdContainerDataSourceHandle valid = HdRetainedContainerDataSource::New(
      TfToken("Literal"),
      node("ND_add_integer",
           HdRetainedContainerDataSource::New(
               TfToken("in1"), int_parameter(1), TfToken("in2"), int_parameter(2))));
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXIntegerMultiplicity"));
  HdCyclesMaterialTestAccess::Populate(
      &material, &session, HdMaterialNetworkSchema::Builder().SetNodes(valid).Build());
  ShaderGraph *const original = material.GetCyclesShader()->graph.get();

  const HdContainerDataSourceHandle invalid = HdRetainedContainerDataSource::New(
      TfToken("One"),
      node("ND_constant_integer",
           HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(1))),
      TfToken("Two"),
      node("ND_constant_integer",
           HdRetainedContainerDataSource::New(TfToken("value"), int_parameter(2))),
      TfToken("Add"),
      node("ND_add_integer",
           nullptr,
           HdRetainedContainerDataSource::New(TfToken("in1"),
                                              two_connections(TfToken("One"), TfToken("Two")),
                                              TfToken("in2"),
                                              connection(TfToken("One"), TfToken("out")))));
  HdCyclesMaterialTestAccess::Populate(
      &material, &session, HdMaterialNetworkSchema::Builder().SetNodes(invalid).Build());
  EXPECT_EQ(material.GetCyclesShader()->graph.get(), original);
  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_noise2d_sampling_family_with_exact_scalar_and_color_forms)
{
  const HdContainerDataSourceHandle scalar_parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), float_parameter(0.75f),
      TfToken("pivot"), float_parameter(0.1f));
  const HdContainerDataSourceHandle color_parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), vector3_parameter(pxr::GfVec3f(0.5f, 0.75f, 1.0f)),
      TfToken("pivot"), float_parameter(0.2f));
  const HdContainerDataSourceHandle colorfa_parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), float_parameter(0.5f),
      TfToken("pivot"), float_parameter(0.25f));
  const HdContainerDataSourceHandle coord_parameters = HdRetainedContainerDataSource::New(
      TfToken("value"), vector2_parameter(pxr::GfVec2f(0.125f, 0.875f)));
  const HdContainerDataSourceHandle texcoord_connections = HdRetainedContainerDataSource::New(
      TfToken("texcoord"), connection(TfToken("Texcoord"), TfToken("out")));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Texcoord"), node("ND_constant_vector2", coord_parameters),
      TfToken("Scalar"), node("ND_noise2d_float", scalar_parameters, texcoord_connections),
      TfToken("Color"), node("ND_noise2d_color3", color_parameters, texcoord_connections),
      TfToken("ColorFA"), node("ND_noise2d_color3FA", colorfa_parameters, texcoord_connections));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXNoise2DSampling"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  int noise_count = 0;
  int scalar_amplitude_count = 0;
  int vector_amplitude_count = 0;
  int broadcast_amplitude_count = 0;
  int default_vector_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (NoiseTextureNode *noise = dynamic_cast<NoiseTextureNode *>(node)) {
      EXPECT_EQ(noise->get_dimensions(), 2);
      ++noise_count;
    }
    if (MathNode *math = dynamic_cast<MathNode *>(node);
        math && math->get_math_type() == NODE_MATH_MULTIPLY)
    {
      scalar_amplitude_count += (math->get_value2() == 0.75f || math->get_value2() == 0.5f);
    }
    if (VectorMathNode *math = dynamic_cast<VectorMathNode *>(node);
        math && math->get_math_type() == NODE_VECTOR_MATH_MULTIPLY)
    {
      if (math->get_vector2() == make_float3(0.5f, 0.75f, 1.0f)) {
        ++vector_amplitude_count;
      }
    }
    if (CombineXYZNode *combine = dynamic_cast<CombineXYZNode *>(node);
        combine && combine->get_x() == 0.5f && combine->get_y() == 0.5f &&
        combine->get_z() == 0.5f)
    {
      ++broadcast_amplitude_count;
    }
    if (SeparateXYZNode *separate = dynamic_cast<SeparateXYZNode *>(node);
        separate && separate->get_vector() == make_float3(0.125f, 0.875f, 0.0f))
    {
      ++default_vector_count;
    }
  }
  EXPECT_EQ(noise_count, 3);
  EXPECT_EQ(scalar_amplitude_count, 1);
  EXPECT_EQ(vector_amplitude_count, 1);
  EXPECT_EQ(broadcast_amplitude_count, 1);
  EXPECT_GE(default_vector_count, 1);

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, lowers_noise2d_omitted_texcoord_to_uv0_attribute_default)
{
  const HdContainerDataSourceHandle parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), float_parameter(0.75f),
      TfToken("pivot"), float_parameter(0.1f));
  const HdContainerDataSourceHandle nodes = HdRetainedContainerDataSource::New(
      TfToken("Noise"), node("ND_noise2d_float", parameters));
  const HdMaterialNetworkSchema network(HdMaterialNetworkSchema::Builder().SetNodes(nodes).Build());

  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXNoise2DUV0Default"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, network);

  NoiseTextureNode *noise = nullptr;
  UVMapNode *uv = nullptr;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    noise = noise ? noise : dynamic_cast<NoiseTextureNode *>(node);
    uv = uv ? uv : dynamic_cast<UVMapNode *>(node);
  }
  ASSERT_NE(noise, nullptr);
  ASSERT_NE(uv, nullptr);
  EXPECT_EQ(noise->get_dimensions(), 2);
  EXPECT_EQ(uv->get_attribute(), ustring("UV0"));
  EXPECT_EQ(noise->input("Vector")->link, uv->output("UV"));

  material.Finalize(&session);
}

TEST(HdCyclesMaterialXMapping, rejects_noise2d_wrong_and_extra_inputs_atomically)
{
  const HdContainerDataSourceHandle valid_parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), float_parameter(0.75f),
      TfToken("pivot"), float_parameter(0.1f));
  const HdMaterialNetworkSchema valid_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Noise"), node("ND_noise2d_float", valid_parameters)))
          .Build());
  HdCyclesSession session{SessionParams()};
  HdCyclesMaterial material(SdfPath("/MaterialXNoise2DPreserveGraph"));
  HdCyclesMaterialTestAccess::Populate(&material, &session, valid_network);
  ShaderGraph *preexisting_graph = material.GetCyclesShader()->graph.get();
  ASSERT_NE(preexisting_graph, nullptr);

  const HdContainerDataSourceHandle invalid_parameters = HdRetainedContainerDataSource::New(
      TfToken("amplitude"), vector3_parameter(pxr::GfVec3f(1.0f)),
      TfToken("pivot"), float_parameter(0.1f),
      TfToken("extra"), float_parameter(1.0f));
  const HdMaterialNetworkSchema invalid_network(
      HdMaterialNetworkSchema::Builder()
          .SetNodes(HdRetainedContainerDataSource::New(
              TfToken("Noise"), node("ND_noise2d_float", invalid_parameters)))
          .Build());
  HdCyclesMaterialTestAccess::Populate(&material, &session, invalid_network);

  EXPECT_EQ(material.GetCyclesShader()->graph.get(), preexisting_graph);
  int noise_count = 0;
  for (ShaderNode *node : material.GetCyclesShader()->graph->nodes) {
    if (NoiseTextureNode *noise = dynamic_cast<NoiseTextureNode *>(node)) {
      EXPECT_EQ(noise->get_dimensions(), 2);
      ++noise_count;
    }
  }
  EXPECT_EQ(noise_count, 1);

  material.Finalize(&session);
}

HDCYCLES_NAMESPACE_CLOSE_SCOPE
