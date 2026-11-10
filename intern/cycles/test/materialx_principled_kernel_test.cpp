/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Actual SVM compiler and CPU closure allocation only; no integrator or pixels. */
#include "device/device.h"
#include "kernel/globals.h"
#include "materialx/graph.h"
#ifdef WITH_USD
#  include "materialx/usdshade_reader.h"
#  include <pxr/base/gf/vec3f.h>
#  include <pxr/usd/usd/stage.h>
#  include <pxr/usd/usdShade/shader.h>
#endif
#include "kernel/svm/closure.h"
#include "kernel/svm/attribute.h"
#include "scene/constant_fold.h"
#include "scene/scene.h"
#include "scene/shader.tables"  // Actual production LUTs; never synthetic zero tables.
#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "scene/svm.h"
#include "testing/testing.h"
#include "util/colorspace.h"
#include "util/profiling.h"
#include "util/progress.h"
#include "util/stats.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <vector>

CCL_NAMESPACE_BEGIN
namespace {
class CapturePrincipledSVM : public SVMCompiler {
 public:
  CapturePrincipledSVM(Scene *scene, Progress &progress, ShaderGraph *graph)
      : SVMCompiler(scene, progress)
  {
    current_graph = graph;
    memset(&active_stack, 0, sizeof(active_stack));
  }
  void compile_source(ShaderNode *node)
  {
    current_node = node;
    node->added_to_svm = false;
    node->compile(*this);
  }
  int assign_source_output(ShaderOutput *output)
  {
    return stack_assign(output);
  }
  std::vector<uint> capture(ShaderNode *node)
  {
    const size_t begin = current_svm_nodes.size();
    compile_source(node);
    std::vector<uint> words;
    for (size_t i = begin; i < current_svm_nodes.size(); ++i) {
      words.push_back(uint(current_svm_nodes[i]));
    }
    return words;
  }
};
struct ClosureObservation {
  Spectrum diffuse = zero_spectrum(), sss = zero_spectrum();
  Spectrum sss_albedo = zero_spectrum(), emission = zero_spectrum();
  Spectrum transparent = zero_spectrum(), dielectric_f0 = zero_spectrum();
  Spectrum metal_f0 = zero_spectrum(), thin_sss_transmission = zero_spectrum();
  OrenNayarParam oren{};
  bool has_oren = false, has_sss = false, has_dielectric = false, has_metal = false;
};
void expect_spectrum(const Spectrum actual, const Spectrum expected)
{
  EXPECT_NEAR(actual.x, expected.x, 1e-6f);
  EXPECT_NEAR(actual.y, expected.y, 1e-6f);
  EXPECT_NEAR(actual.z, expected.z, 1e-6f);
}
}  // namespace
class materialx_principled_kernel : public testing::Test {
 protected:
  Stats stats;
  Profiler profiler;
  Progress progress;
  unique_ptr<Device> device;
  unique_ptr<Scene> scene;
  std::vector<float> tables;
  KernelGlobalsCPU globals;
  void SetUp() override
  {
    ColorSpaceManager::init_fallback_config();
    DeviceInfo info;
    device = Device::create(info, stats, profiler, true);
    scene = make_unique<Scene>(SceneParams(), device.get());
    auto append = [&](const auto &table) {
      const int offset = int(tables.size());
      tables.insert(tables.end(), std::begin(table), std::end(table));
      return offset;
    };
    globals.data.tables.ggx_E = append(table_ggx_E);
    globals.data.tables.ggx_Eavg = append(table_ggx_Eavg);
    globals.data.tables.ggx_glass_E = append(table_ggx_glass_E);
    globals.data.tables.ggx_glass_Eavg = append(table_ggx_glass_Eavg);
    globals.data.tables.ggx_glass_inv_E = append(table_ggx_glass_inv_E);
    globals.data.tables.ggx_glass_inv_Eavg = append(table_ggx_glass_inv_Eavg);
    globals.data.tables.sheen_ltc = append(table_sheen_ltc);
    globals.data.tables.ggx_gen_schlick_ior_s = append(table_ggx_gen_schlick_ior_s);
    globals.data.tables.ggx_gen_schlick_s = append(table_ggx_gen_schlick_s);
    globals.lookup_table.data = tables.data();
    globals.lookup_table.width = int(tables.size());
    globals.data.integrator.caustics_reflective = true;
    globals.data.integrator.caustics_refractive = true;
    globals.data.film.is_rec709 = true;
  }
  ClosureObservation evaluate(ShaderGraph &graph,
                              PrincipledBsdfNode *node,
                              ColorNode *linked_color = nullptr,
                              ValueNode *linked_weight = nullptr)
  {
    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    std::array<float, SVM_STACK_SIZE> stack{};
    if (linked_color) {
      compiler.compile_source(linked_color);
      const auto offset = linked_color->output("Color")->stack_offset;
      if (offset + 2 >= SVM_STACK_SIZE) {
        ADD_FAILURE() << "Compiled color source has no valid stack range";
        return {};
      }
      const float3 c = linked_color->get_value();
      stack[offset] = c.x;
      stack[offset + 1] = c.y;
      stack[offset + 2] = c.z;
    }
    if (linked_weight) {
      compiler.compile_source(linked_weight);
      const auto offset = linked_weight->output("Value")->stack_offset;
      if (offset >= SVM_STACK_SIZE) {
        ADD_FAILURE() << "Compiled scalar source has no valid stack range";
        return {};
      }
      stack[offset] = linked_weight->get_value();
    }
    const std::vector<uint> words = compiler.capture(node);
    const size_t header_words = 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
    if (words.size() != header_words + sizeof(SVMNodePrincipledBsdfData) / sizeof(uint)) {
      ADD_FAILURE() << "Actual compiled Principled bytecode size does not match its wire struct";
      return {};
    }
    EXPECT_EQ(words.at(0), uint(NODE_CLOSURE_BSDF));
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + 1, sizeof(header));
    EXPECT_EQ(header.closure_type, CLOSURE_BSDF_PRINCIPLED_ID);
    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    ShaderData sd{};
    sd.N = sd.Ng = sd.wi = make_float3(0.0f, 0.0f, 1.0f);
    sd.num_closure_left = MAX_CLOSURE;
    const int end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
        &kg,
        &sd,
        stack.data(),
        one_spectrum(),
        header,
        PATH_RAY_VISIBILITY_CAMERA,
        0,
        int(header_words));
    EXPECT_EQ(end, words.size());
    ClosureObservation out;
    out.emission = sd.closure_emission_background;
    out.transparent = sd.closure_transparent_extinction;
    for (int i = 0; i < sd.num_closure; ++i) {
      const ShaderClosure *closure = &sd.closure[i];
      if (closure->type == CLOSURE_BSDF_DIFFUSE_ID || closure->type == CLOSURE_BSDF_OREN_NAYAR_ID)
      {
        out.diffuse += closure->weight;
        if (closure->type == CLOSURE_BSDF_OREN_NAYAR_ID) {
          out.has_oren = true;
          out.oren = reinterpret_cast<const OrenNayarBsdf *>(closure)->param;
        }
      }
      if (closure->type == CLOSURE_BSDF_TRANSLUCENT_ID ||
          closure->type == CLOSURE_BSDF_ROUGH_TRANSLUCENT_ID)
      {
        out.thin_sss_transmission += closure->weight;
      }
      if (CLOSURE_IS_BSSRDF(closure->type)) {
        out.has_sss = true;
        out.sss += closure->weight;
        out.sss_albedo = reinterpret_cast<const Bssrdf *>(closure)->albedo;
      }
      if (CLOSURE_IS_BSDF_MICROFACET(closure->type)) {
        const auto *bsdf = reinterpret_cast<const MicrofacetBsdf *>(closure);
        if (bsdf->fresnel_type == MicrofacetFresnel::GENERALIZED_SCHLICK) {
          out.has_dielectric = true;
          out.dielectric_f0 =
              reinterpret_cast<const FresnelGeneralizedSchlick *>(bsdf->fresnel)->f0;
        }
        if (bsdf->fresnel_type == MicrofacetFresnel::F82_TINT) {
          out.has_metal = true;
          out.metal_f0 = reinterpret_cast<const FresnelF82Tint *>(bsdf->fresnel)->f0;
        }
      }
    }
    return out;
  }
  PrincipledBsdfNode *make_node(ShaderGraph &graph)
  {
    auto *node = graph.create_node<PrincipledBsdfNode>();
    node->set_base_color(make_float3(0.2f, 0.4f, 0.8f));
    node->set_diffuse_roughness(0.6f);
    node->set_roughness(0.2f);
    node->set_alpha(0.6f);
    node->set_emission_strength(0.7f);
    node->set_emission_color(make_float3(0.3f, 0.5f, 0.1f));
    graph.connect(node->output("BSDF"), graph.output()->input("Surface"));
    return node;
  }
};
TEST_F(materialx_principled_kernel, weight_changes_actual_diffuse_not_compensation_or_specular)
{
  ClosureObservation reference;
  for (const float weight : {1.0f, 0.25f, 2.0f, 0.0f, -0.5f}) {
    ShaderGraph graph;
    auto *node = make_node(graph);
    auto *base = node->input("Base Weight");
    ASSERT_NE(base, nullptr);  // Baseline failure is a real missing contract, not a compile error.
    node->set(base->socket_type, weight);
    const auto got = evaluate(graph, node);
    if (weight == 1.0f) {
      reference = got;
      EXPECT_GT(average(reference.emission), 0.0f);
      expect_spectrum(reference.transparent, make_spectrum(0.4f));
      expect_spectrum(reference.dielectric_f0, make_spectrum(0.04f));
    }
    const float expected_weight = std::max(weight, 0.0f);
    expect_spectrum(got.diffuse, reference.diffuse * expected_weight);
    expect_spectrum(got.dielectric_f0, reference.dielectric_f0);
    expect_spectrum(got.emission, reference.emission);
    expect_spectrum(got.transparent, reference.transparent);
    ASSERT_TRUE(got.has_dielectric);
    if (expected_weight > 0.0f) {
      ASSERT_TRUE(got.has_oren);
      EXPECT_FLOAT_EQ(got.oren.a, reference.oren.a);
      EXPECT_FLOAT_EQ(got.oren.b, reference.oren.b);
      expect_spectrum(got.oren.multiscatter_term, reference.oren.multiscatter_term);
    }
  }
}
TEST_F(materialx_principled_kernel, independent_sss_survives_zero_base_and_actual_writer_selection)
{
  for (const bool independent : {false, true}) {
    for (const bool fold : {false, true}) {
      ClosureObservation volumetric;
      for (const bool thin_wall : {false, true}) {
        ShaderGraph graph;
        auto *node = make_node(graph);
        auto *base = node->input("Base Weight");
        auto *color = node->input("Subsurface Color");
        const auto *flag = node->type->find_input(ustring("use_independent_subsurface_color"));
        ASSERT_NE(base, nullptr);
        ASSERT_NE(color, nullptr);
        ASSERT_NE(flag, nullptr);
        node->set(base->socket_type, 0.0f);
        node->set(color->socket_type, one_float3());
        node->set(*flag, independent);
        node->set_subsurface_weight(1.0f);
        node->set_subsurface_scale(1.0f);
        node->set_subsurface_radius(one_float3());
        node->set_thin_wall(int(thin_wall));
        auto *source = graph.create_node<ColorNode>();
        source->set_value(make_float3(0.2f, 0.4f, 0.8f));
        graph.connect(source->output("Color"), node->input("Base Color"));
        if (fold) {
          const ConstantFolder folder(&graph, source, source->output("Color"), scene.get());
          source->constant_fold(folder);
          ASSERT_EQ(node->input("Base Color")->link, nullptr);
          ASSERT_TRUE(node->input("Base Color")->constant_folded_in);
          source = nullptr;
        }
        const auto got = evaluate(graph, node, source);
        const Spectrum expected_color = independent ? one_spectrum() :
                                                      make_float3(0.2f, 0.4f, 0.8f);
        if (!thin_wall) {
          volumetric = got;
          ASSERT_TRUE(got.has_sss);
          expect_spectrum(got.sss_albedo, expected_color);
          EXPECT_GT(average(got.sss), 0.0f);
          expect_spectrum(got.diffuse, zero_spectrum());
        }
        else {
          EXPECT_FALSE(got.has_sss);
          ASSERT_TRUE(got.has_oren);
          EXPECT_GT(average(got.thin_sss_transmission), 0.0f);
          /* Thin SSS splits the same selected weighted color into reflection/transmission. */
          expect_spectrum(got.diffuse + got.thin_sss_transmission, volumetric.sss);
          const OrenNayarParam expected = bsdf_oren_nayar_param(expected_color, 1.0f, 0.6f);
          expect_spectrum(got.oren.multiscatter_term, expected.multiscatter_term);
        }
      }
    }
  }
}

TEST_F(materialx_principled_kernel, linked_hdr_weight_and_metal_f0_use_actual_svm_inputs)
{
  for (const float weight : {0.25f, 2.0f, -0.5f}) {
    for (const bool linked : {false, true}) {
      ShaderGraph graph;
      auto *node = make_node(graph);
      auto *base = node->input("Base Weight");
      ASSERT_NE(base, nullptr);
      node->set_metallic(1.0f);
      ValueNode *source = nullptr;
      if (linked) {
        source = graph.create_node<ValueNode>();
        source->set_value(weight);
        graph.connect(source->output("Value"), base);
      }
      else {
        node->set(base->socket_type, weight);
      }
      const auto got = evaluate(graph, node, nullptr, source);
      ASSERT_TRUE(got.has_metal);
      const float b = std::max(weight, 0.0f);
      expect_spectrum(got.metal_f0, min(node->get_base_color() * b, one_float3()));
    }
  }
}

TEST_F(materialx_principled_kernel, physical_metallic_tint_weights_actual_svm_closure)
{
  const float3 tint = make_float3(0.72f, 0.12f, 0.04f);
  for (const ClosureType fresnel_type : {CLOSURE_BSDF_PHYSICAL_CONDUCTOR,
                                         CLOSURE_BSDF_F82_CONDUCTOR})
  {
    ShaderGraph graph;
    auto *metallic = graph.create_node<MetallicBsdfNode>();
    metallic->set_fresnel_type(fresnel_type);
    metallic->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
    metallic->set_roughness(0.3f);
    metallic->set_tint(tint);

    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    const std::vector<uint> words = compiler.capture(metallic);
    const bool physical = fresnel_type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR;
    size_t offset = 0;
    Spectrum closure_weight = one_spectrum();
    if (physical) {
      ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_SET_WEIGHT));
      SVMNodeClosureSetWeight weight_data;
      memcpy(&weight_data, words.data() + offset, sizeof(weight_data));
      offset += sizeof(weight_data) / sizeof(uint);
      svm_node_closure_set_weight(&closure_weight, weight_data);
      expect_spectrum(closure_weight, rgb_to_spectrum(tint));
    }
    ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_BSDF));
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + offset, sizeof(header));
    offset += sizeof(header) / sizeof(uint);
    ASSERT_EQ(header.closure_type, fresnel_type);
    ASSERT_EQ(words.size(), offset + sizeof(SVMNodeMetallicBsdfData) / sizeof(uint));

    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    std::array<float, SVM_STACK_SIZE> stack{};
    ShaderData sd{};
    sd.N = sd.Ng = sd.wi = make_float3(0.0f, 0.0f, 1.0f);
    sd.num_closure_left = MAX_CLOSURE;
    const int end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
        &kg,
        &sd,
        stack.data(),
        closure_weight,
        header,
        PATH_RAY_VISIBILITY_CAMERA,
        0,
        int(offset));
    EXPECT_EQ(end, words.size());
    ASSERT_EQ(sd.num_closure, 1);
    expect_spectrum(sd.closure[0].weight,
                    physical ? rgb_to_spectrum(tint) : rgb_to_spectrum(one_float3()));
  }
}

TEST_F(materialx_principled_kernel,
       metallic_physical_alpha_direct_linked_and_stock_sentinel_actual_svm)
{
  struct Case {
    float x, y;
    bool linked;
    double expected_x, expected_y;
    float anisotropy = 0;
  };
  const Case cases[] = {{-1, -1, false, .09, .09},
                        {.0025f, -1, false, .09, .09},
                        {-1, .1849f, false, .09, .09},
                        {.0025f, .1849f, false, .0025, .1849},
                        {.1849f, .0025f, false, .1849, .0025},
                        {.0025f, .1849f, true, .0025, .1849},
                        {-1, -1, false, .2846049894151541, .028460498941515414, 1},
                        {.0025f, -1, false, .2846049894151541, .028460498941515414, 1},
                        {-1, .1849f, true, .2846049894151541, .028460498941515414, 1},
                        {.0025f, -1, true, .2846049894151541, .028460498941515414, 1}};
  for (const Case &c : cases) {
    const float3 tint = make_float3(0.72f, 0.12f, 0.04f);
    for (const ClosureType fresnel_type :
         {CLOSURE_BSDF_PHYSICAL_CONDUCTOR, CLOSURE_BSDF_F82_CONDUCTOR})
    {
      ShaderGraph graph;
      auto *metallic = graph.create_node<MetallicBsdfNode>();
      metallic->set_fresnel_type(fresnel_type);
      metallic->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_ID);
      metallic->set_roughness(0.3f);
      metallic->set_tint(tint);
      metallic->set_alpha_x(c.x);
      metallic->set_alpha_y(c.y);
      metallic->set_tangent(make_float3(1, 0, 0));
      metallic->set_rotation(.2f);
      metallic->set_anisotropy(c.anisotropy);
      TangentNode *t_source = nullptr;
      if (c.anisotropy > 0) {
        t_source = graph.create_node<TangentNode>();
        graph.connect(t_source->output("Tangent"), metallic->input("Tangent"));
      }
      ValueNode *x_source = nullptr, *y_source = nullptr;
      if (c.linked) {
        x_source = graph.create_node<ValueNode>();
        x_source->set_value(c.x);
        y_source = graph.create_node<ValueNode>();
        y_source->set_value(c.y);
        graph.connect(x_source->output("Value"), metallic->input("Alpha X"));
        graph.connect(y_source->output("Value"), metallic->input("Alpha Y"));
      }
      EXPECT_EQ(metallic->is_isotropic(),
                !c.linked && (c.x >= 0 && c.y >= 0 ? c.x == c.y : c.anisotropy == 0));

      CapturePrincipledSVM compiler(scene.get(), progress, &graph);
      std::array<float, SVM_STACK_SIZE> stack{};
      if (c.linked) {
        compiler.assign_source_output(x_source->output("Value"));
        compiler.compile_source(x_source);
        compiler.assign_source_output(y_source->output("Value"));
        compiler.compile_source(y_source);
        stack[compiler.stack_assign(metallic->input("Alpha X"))] = c.x;
        stack[compiler.stack_assign(metallic->input("Alpha Y"))] = c.y;
      }
      if (t_source) {
        compiler.assign_source_output(t_source->output("Tangent"));
        compiler.compile_source(t_source);
        ShaderOutput *consumer_tangent = metallic->input("Tangent")->link;
        ASSERT_NE(consumer_tangent, nullptr);
        compiler.assign_source_output(consumer_tangent);
        compiler.compile_source(consumer_tangent->parent);
        const int t_offset = compiler.stack_assign(metallic->input("Tangent"));
        ASSERT_NE(t_offset, SVM_STACK_INVALID);
        ASSERT_GE(t_offset, 0);
        ASSERT_LT(t_offset + 2, SVM_STACK_SIZE);
        stack[t_offset] = 1;
        stack[t_offset + 1] = 0;
        stack[t_offset + 2] = 0;
      }
      const std::vector<uint> words = compiler.capture(metallic);
      const bool physical = fresnel_type == CLOSURE_BSDF_PHYSICAL_CONDUCTOR;
      size_t offset = 0;
      Spectrum closure_weight = one_spectrum();
      if (physical) {
        ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_SET_WEIGHT));
        SVMNodeClosureSetWeight weight_data;
        memcpy(&weight_data, words.data() + offset, sizeof(weight_data));
        offset += sizeof(weight_data) / sizeof(uint);
        svm_node_closure_set_weight(&closure_weight, weight_data);
        expect_spectrum(closure_weight, rgb_to_spectrum(tint));
      }
      ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_BSDF));
      SVMNodeClosureBsdf header;
      memcpy(&header, words.data() + offset, sizeof(header));
      offset += sizeof(header) / sizeof(uint);
      ASSERT_EQ(header.closure_type, fresnel_type);
      ASSERT_EQ(words.size(), offset + sizeof(SVMNodeMetallicBsdfData) / sizeof(uint));

      globals.svm_nodes.data = words.data();
      globals.svm_nodes.width = int(words.size());
      ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
      ShaderData sd{};
      sd.N = sd.Ng = sd.wi = make_float3(0.0f, 0.0f, 1.0f);
      sd.num_closure_left = MAX_CLOSURE;
      const int end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
          &kg,
          &sd,
          stack.data(),
          closure_weight,
          header,
          PATH_RAY_VISIBILITY_CAMERA,
          0,
          int(offset));
      EXPECT_EQ(end, words.size());
      EXPECT_EQ(svm_node_closure_bsdf_skip(int(offset), header.closure_type), int(words.size()));
      ASSERT_EQ(sd.num_closure, 1);
      const auto *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
      EXPECT_NEAR(bsdf->alpha_x, c.expected_x, 1e-7);
      EXPECT_NEAR(bsdf->alpha_y, c.expected_y, 1e-7);
      if ((c.x >= 0 && c.y >= 0 && c.x != c.y) || c.anisotropy > 0) {
        EXPECT_NEAR(bsdf->T.x, .30901699437494745, 1e-6);
        EXPECT_NEAR(bsdf->T.y, .9510565162951535, 1e-6);
        EXPECT_FLOAT_EQ(bsdf->T.z, 0);
      }
      expect_spectrum(sd.closure[0].weight,
                      physical ? rgb_to_spectrum(tint) : rgb_to_spectrum(one_float3()));
    }
  }
}

TEST_F(materialx_principled_kernel, materialx_conductor_multiscatter_actual_svm_real_lut)
{
  /* Production LUT interpolation independently performed in double precision.
   * This checks native compensation transport, not MDL distribution equality. */
  const float3 wi = normalize(make_float3(0.67805003f, -0.63814403f, 0.36471956f));
  const double rough = std::sqrt(std::sqrt(0.22 * 0.34));
  const double x = rough * 31.0, y = double(wi.z) * 31.0;
  const int ix = int(x), iy = int(y);
  const double fx = x - ix, fy = y - iy;
  const double E =
      (1 - fy) * ((1 - fx) * table_ggx_E[iy * 32 + ix] + fx * table_ggx_E[iy * 32 + ix + 1]) +
      fy * ((1 - fx) * table_ggx_E[(iy + 1) * 32 + ix] +
            fx * table_ggx_E[(iy + 1) * 32 + ix + 1]);
  ASSERT_GT(1.0 / E, 1.20);
  for (int variant = 0; variant < 3; ++variant) {
    materialx::Node source;
    source.name = "SDKConductor";
    source.nodedef = variant == 2 ? "ND_lama_conductor" : "ND_conductor_bsdf";
    source.outputs["out"] = variant == 0 ? materialx::Type::BSDF : materialx::Type::SurfaceShader;
    source.vector2_inputs["roughness"] = make_float2(0.22f, 0.34f);
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source}}, &graph));
    MetallicBsdfNode *metallic = nullptr;
    for (ShaderNode *node : graph.nodes) {
      if (node->name == source.name) {
        metallic = dynamic_cast<MetallicBsdfNode *>(node);
      }
    }
    ASSERT_NE(metallic, nullptr);
    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    ShaderOutput *tangent = metallic->input("Tangent")->link;
    ASSERT_NE(tangent, nullptr);
    const int tangent_offset = compiler.assign_source_output(tangent);
    ASSERT_GE(tangent_offset, 0);
    const std::vector<uint> words = compiler.capture(metallic);
    size_t offset = 0;
    ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_SET_WEIGHT));
    SVMNodeClosureSetWeight weight_data;
    memcpy(&weight_data, words.data() + offset, sizeof(weight_data));
    offset += sizeof(weight_data) / sizeof(uint);
    Spectrum closure_weight = one_spectrum();
    svm_node_closure_set_weight(&closure_weight, weight_data);
    ASSERT_EQ(words.at(offset++), uint(NODE_CLOSURE_BSDF));
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + offset, sizeof(header));
    offset += sizeof(header) / sizeof(uint);
    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    std::array<float, SVM_STACK_SIZE> stack{};
    /* Known UV tangent injected at the actual packed consumer input; no UV fetch or pixels. */
    stack_store_float3(stack.data(), tangent_offset, make_float3(-1, 0, 0));
    ShaderData sd{};
    sd.N = sd.Ng = make_float3(0, 0, 1);
    sd.wi = wi;
    sd.num_closure_left = MAX_CLOSURE;
    svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
        &kg, &sd, stack.data(), closure_weight, header, PATH_RAY_VISIBILITY_CAMERA, 0, int(offset));
    ASSERT_EQ(sd.num_closure, 1);
    const auto *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
    EXPECT_NEAR(bsdf->alpha_x, 0.22f, 1e-6f);
    EXPECT_NEAR(bsdf->alpha_y, 0.34f, 1e-6f);
    EXPECT_NEAR(bsdf->energy_scale, 1.0 / E, 2e-6) << "variant=" << variant;
  }
}

TEST_F(materialx_principled_kernel, burley_full_actual_svm_and_angular_oracle)
{
  BurleyBsdf legacy{};
  bsdf_burley_setup(&legacy, 2.0f);
  EXPECT_FLOAT_EQ(legacy.roughness, 1.0f);
  for (float roughness : {0.0f, 0.25f, 0.75f, 1.0f, 2.0f}) {
    for (float weight : {0.0f, 0.5f, 2.0f}) {
      ShaderGraph graph;
      DiffuseBsdfNode *node = graph.create_node<DiffuseBsdfNode>();
      node->set_method(CLOSURE_BSDF_BURLEY_ID);
      node->set_color(make_float3(0.2f, 0.4f, 0.8f));
      node->set_closure_weight(weight);
      node->set_roughness(roughness);
      CapturePrincipledSVM compiler(scene.get(), progress, &graph);
      const std::vector<uint> words = compiler.capture(node);
      const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
      const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
      ASSERT_EQ(words.size(), data_offset + sizeof(SVMNodeDiffuseBsdfData) / sizeof(uint));
      ASSERT_EQ(words.at(0), uint(NODE_CLOSURE_SET_WEIGHT));
      ASSERT_EQ(words.at(header_offset), uint(NODE_CLOSURE_BSDF));
      SVMNodeClosureBsdf header;
      memcpy(&header, words.data() + header_offset + 1, sizeof(header));
      globals.svm_nodes.data = words.data();
      globals.svm_nodes.width = int(words.size());
      KernelObject object{};
      globals.objects.data = &object;
      globals.objects.width = 1;
      ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
      ShaderData sd{};
      sd.N = sd.Ng = make_float3(0.0f, 0.0f, 1.0f);
      sd.wi = make_float3(0.8660254f, 0.0f, 0.5f);
      sd.num_closure_left = MAX_CLOSURE;
      std::array<float, SVM_STACK_SIZE> stack{};
      const int end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
          &kg, &sd, stack.data(), rgb_to_spectrum(node->get_color()), header,
          PATH_RAY_VISIBILITY_CAMERA, 0, data_offset);
      EXPECT_EQ(end, int(words.size()));
      EXPECT_EQ(svm_node_closure_bsdf_skip(data_offset, header.closure_type), int(words.size()));
      if (weight == 0.0f) {
        EXPECT_EQ(sd.num_closure, 0);
        continue;
      }
      ASSERT_EQ(sd.num_closure, 1);
      EXPECT_EQ(sd.closure[0].type, CLOSURE_BSDF_BURLEY_ID);
      expect_spectrum(sd.closure[0].weight, rgb_to_spectrum(node->get_color() * weight));
      const float3 wo = make_float3(-0.8660254f, 0.0f, 0.5f);
      float pdf;
      const Spectrum value = bsdf_eval(&kg, &sd, &sd.closure[0], wo, &pdf);
      /* Independently substitute NdV=NdL=LdotH=.5 into pinned MaterialX GLSL. */
      const float f90 = 0.5f + 0.5f * roughness;
      const float factor = 1.0f + (f90 - 1.0f) / 32.0f;
      expect_spectrum(value, make_spectrum(0.5f * M_1_PI_F * factor * factor));
      EXPECT_FLOAT_EQ(pdf, 0.5f * M_1_PI_F);
      if (roughness == 0.0f) { EXPECT_FLOAT_EQ(factor * factor, 0.968994140625f); }
      float3 wo_sample;
      Spectrum eval_sample;
      float pdf_sample, pdf_eval;
      float2 sampled_roughness;
      float eta;
      const int label = bsdf_sample(&kg, &sd, &sd.closure[0], make_float3(0.3f, 0.7f, 0.0f),
                                    &eval_sample, &wo_sample, &pdf_sample, &sampled_roughness, &eta);
      EXPECT_FLOAT_EQ(sampled_roughness.x, 1.0f);
      EXPECT_FLOAT_EQ(eta, 1.0f);
      float2 metadata_roughness;
      bsdf_roughness_eta(&sd.closure[0], wo_sample, &metadata_roughness, &eta);
      EXPECT_FLOAT_EQ(metadata_roughness.x, 1.0f);
      EXPECT_EQ(bsdf_label(&kg, &sd.closure[0], wo_sample), LABEL_REFLECT | LABEL_DIFFUSE);
      EXPECT_EQ(label, LABEL_REFLECT | LABEL_DIFFUSE);
      expect_spectrum(eval_sample, bsdf_eval(&kg, &sd, &sd.closure[0], wo_sample, &pdf_eval));
      EXPECT_NEAR(pdf_sample, pdf_eval, 1e-6f);
    }
  }
}

TEST_F(materialx_principled_kernel, burley_full_default_diffuse_and_allocator_guards)
{
  for (bool burley : {false, true}) {
    for (float roughness : {0.0f, 0.75f}) {
      ShaderGraph graph;
      DiffuseBsdfNode *node = graph.create_node<DiffuseBsdfNode>();
      EXPECT_EQ(node->get_method(), CLOSURE_BSDF_DIFFUSE_ID);
      EXPECT_FLOAT_EQ(node->get_closure_weight(), 1.0f);
      if (burley) { node->set_method(CLOSURE_BSDF_BURLEY_ID); }
      node->set_color(make_float3(0.2f, 0.4f, 0.8f));
      node->set_roughness(roughness);
      for (float weight : {0.0f, 1.0f}) {
        node->set_closure_weight(weight);
        CapturePrincipledSVM compiler(scene.get(), progress, &graph);
        const std::vector<uint> words = compiler.capture(node);
        const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
        const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
        ASSERT_EQ(words.size(), data_offset + sizeof(SVMNodeDiffuseBsdfData) / sizeof(uint));
        SVMNodeClosureBsdf header;
        memcpy(&header, words.data() + header_offset + 1, sizeof(header));
        EXPECT_EQ(header.closure_type, burley ? CLOSURE_BSDF_BURLEY_ID : CLOSURE_BSDF_DIFFUSE_ID);
        globals.svm_nodes.data = words.data();
        globals.svm_nodes.width = int(words.size());
        ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
        std::array<float, SVM_STACK_SIZE> stack{};
        /* Every gate must consume exactly the new payload without allocation. */
        for (int gate : {0, 1, 2, 3}) {
          ShaderData sd{};
          sd.N = sd.Ng = make_float3(0.0f, 0.0f, 1.0f);
          sd.wi = sd.N;
          sd.num_closure_left = MAX_CLOSURE;
          SVMNodeClosureBsdf actual_header = header;
          if (gate == 2) { actual_header.mix_weight_offset = 0; }
          int end;
          if (gate == 0) {
            end = svm_node_closure_bsdf<0, SHADER_TYPE_SURFACE>(
                &kg, &sd, stack.data(), rgb_to_spectrum(node->get_color()), actual_header,
                PATH_RAY_VISIBILITY_CAMERA, 0, data_offset);
          }
          else if (gate == 1) {
            end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_VOLUME>(
                &kg, &sd, stack.data(), rgb_to_spectrum(node->get_color()), actual_header,
                PATH_RAY_VISIBILITY_CAMERA, 0, data_offset);
          }
          else {
            end = svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
                &kg, &sd, stack.data(), rgb_to_spectrum(node->get_color()), actual_header,
                PATH_RAY_VISIBILITY_CAMERA, 0, data_offset);
          }
          EXPECT_EQ(end, int(words.size()));
          if (gate != 3 || weight == 0.0f) {
            EXPECT_EQ(sd.num_closure, 0);
            continue;
          }
          ASSERT_EQ(sd.num_closure, 1);
          EXPECT_EQ(sd.closure[0].type, burley ? CLOSURE_BSDF_BURLEY_ID :
                        (roughness == 0.0f ? CLOSURE_BSDF_DIFFUSE_ID : CLOSURE_BSDF_OREN_NAYAR_ID));
          expect_spectrum(sd.closure[0].weight, rgb_to_spectrum(node->get_color()));
          if (!burley && roughness != 0.0f) {
            const auto *oren = reinterpret_cast<const OrenNayarBsdf *>(&sd.closure[0]);
            EXPECT_FLOAT_EQ(oren->param.roughness, roughness);
            const OrenNayarParam expected = bsdf_oren_nayar_param(
                rgb_to_spectrum(node->get_color()), 1.0f, roughness);
            EXPECT_FLOAT_EQ(oren->param.a, expected.a);
            EXPECT_FLOAT_EQ(oren->param.b, expected.b);
            expect_spectrum(oren->param.multiscatter_term, expected.multiscatter_term);
          }
        }
      }
    }
  }
}

TEST_F(materialx_principled_kernel, burley_full_literal_normal_survives_finalization)
{
  for (int mode : {0, 1, 2}) {
    materialx::Node source;
    source.name = "Burley";
    source.nodedef = "ND_burley_diffuse_bsdf";
    source.inputs["roughness"] = 0.75f;
    if (mode != 0) {
      source.vector3_inputs["normal"] = mode == 1 ? zero_float3() : make_float3(0.0f, 1.0f, 0.0f);
    }
    source.outputs["out"] = materialx::Type::BSDF;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower({{source}}, &graph));
    ShaderNode *bsdf = nullptr;
    for (ShaderNode *node : graph.nodes) { bsdf = node->name == "Burley" ? node : bsdf; }
    ASSERT_NE(bsdf, nullptr);
    graph.connect(bsdf->output("BSDF"), graph.output()->input("Surface"));
    graph.finalize(scene.get());
    ShaderInput *normal = bsdf->input("Normal");
    ASSERT_NE(normal, nullptr);
    if (mode == 2) {
      /* Constant folding is after default geometry insertion. */
      EXPECT_EQ(normal->link, nullptr);
      EXPECT_FLOAT_EQ(bsdf->get_float3(normal->socket_type).y, 1.0f);
    }
    else {
      ASSERT_NE(normal->link, nullptr);
      EXPECT_EQ(normal->link->parent->type->name, "geometry");
      EXPECT_EQ(normal->link->name(), "Normal");
    }
  }
}

TEST_F(materialx_principled_kernel, glass_original_anisotropic_actual_svm)
{
  struct Case {
    float input_alpha_x, input_alpha_y, rotation, ior, thickness, film_ior;
    float alpha_x, alpha_y, tangent_x, tangent_y;
  };
  /* SDK original dielectric (.18,.28), and Lama (.0256,.1369).
   * Physical axes are passed directly; .82 explicitly tests a negative-angle native control.
   * Expectations are independent physical alpha/frame literals. */
  const Case cases[] = {
      {.18f, .28f, 0.0f, 1.46f, 360.0f, 1.38f,
       .18f, .28f, 1.0f, 0.0f},
      {.0256f, .1369f, .82f, 1.0f, 612.0f, 1.32f,
       .0256f, .1369f, .4257792916f, -.9048270525f},
      {.000001f, .81f, 0.0f, 1.46f, 360.0f, 1.38f,
       .000001f, .81f, 1.0f, 0.0f}};
  for (const Case &c : cases) {
    for (const bool linked : {false, true}) {
    SCOPED_TRACE(linked);
    ShaderGraph graph;
    GlassBsdfNode *glass = graph.create_node<GlassBsdfNode>();
    glass->set_distribution(CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID);
    glass->set_IOR(c.ior);
    glass->set_thin_film_thickness(c.thickness);
    glass->set_thin_film_ior(c.film_ior);
    ASSERT_NE(glass->input("Alpha X"), nullptr);
    ASSERT_NE(glass->input("Alpha Y"), nullptr);
    ASSERT_NE(glass->input("Rotation"), nullptr);
    ASSERT_NE(glass->input("Tangent"), nullptr);
    glass->input("Alpha X")->set(c.input_alpha_x);
    glass->input("Alpha Y")->set(c.input_alpha_y);
    glass->input("Rotation")->set(c.rotation);
    TangentNode *tangent = graph.create_node<TangentNode>();
    tangent->set_direction_type(NODE_TANGENT_UVMAP);
    if (linked) { graph.connect(tangent->output("Tangent"), glass->input("Tangent")); }
    else { glass->set_tangent(make_float3(1.0f, 0.0f, 0.0f)); }
    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    compiler.compile_source(tangent);
    std::array<float, SVM_STACK_SIZE> stack{};
    const int tangent_offset = tangent->output("Tangent")->stack_offset;
    if (linked) {
      ASSERT_GE(tangent_offset, 0);
      ASSERT_LT(tangent_offset + 2, SVM_STACK_SIZE);
      stack[tangent_offset] = 1.0f;
    }
    const std::vector<uint> words = compiler.capture(glass);
    const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
    const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
    ASSERT_EQ(words.at(0), uint(NODE_CLOSURE_SET_WEIGHT));
    ASSERT_EQ(words.at(header_offset), uint(NODE_CLOSURE_BSDF));
    ASSERT_EQ(words.size(), data_offset + sizeof(SVMNodeGlassBsdfData) / sizeof(uint));
    SVMNodeGlassBsdfData packed;
    memcpy(&packed, words.data() + data_offset, sizeof(packed));
    EXPECT_FLOAT_EQ(stack_load(stack.data(), packed.tangent).x, 1.0f)
        << "packed tangent bits " << packed.tangent.x.bits << " offset " << tangent_offset;
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + header_offset + 1, sizeof(header));
    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    ShaderData sd{};
    sd.N = sd.Ng = sd.wi = make_float3(0.0f, 0.0f, 1.0f);
    sd.num_closure_left = MAX_CLOSURE;
    EXPECT_EQ((svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
                  &kg, &sd, stack.data(), one_spectrum(), header,
                  PATH_RAY_VISIBILITY_CAMERA, 0, data_offset)), words.size());
    EXPECT_EQ(svm_node_closure_bsdf_skip(data_offset, header.closure_type), words.size());
    ASSERT_EQ(sd.num_closure, 1);
    const MicrofacetBsdf *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
    EXPECT_NEAR(bsdf->alpha_x, c.alpha_x, 1e-6f);
    EXPECT_NEAR(bsdf->alpha_y, c.alpha_y, 1e-6f);
    EXPECT_NEAR(bsdf->T.x, c.tangent_x, 1e-6f);
    EXPECT_NEAR(bsdf->T.y, c.tangent_y, 1e-6f);
    EXPECT_FLOAT_EQ(bsdf->T.z, 0.0f);
    EXPECT_FLOAT_EQ(bsdf->ior, c.ior);
    const FresnelGeneralizedSchlick *fresnel =
        reinterpret_cast<const FresnelGeneralizedSchlick *>(bsdf->fresnel);
    ASSERT_NE(fresnel, nullptr);
    EXPECT_FLOAT_EQ(fresnel->thin_film.thickness, c.thickness);
    EXPECT_FLOAT_EQ(fresnel->thin_film.ior, c.film_ior);
  }
  }
}

TEST_F(materialx_principled_kernel, glass_default_isotropic_actual_svm_unchanged)
{
  for (const ClosureType distribution : {CLOSURE_BSDF_MICROFACET_GGX_GLASS_ID,
                                         CLOSURE_BSDF_MICROFACET_BECKMANN_GLASS_ID}) {
  ShaderGraph graph;
  GlassBsdfNode *glass = graph.create_node<GlassBsdfNode>();
  glass->set_distribution(distribution);
  glass->set_roughness(.2f);
  CapturePrincipledSVM compiler(scene.get(), progress, &graph);
  const std::vector<uint> words = compiler.capture(glass);
  const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
  const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
  ASSERT_EQ(words.at(header_offset), uint(NODE_CLOSURE_BSDF));
  ASSERT_EQ(words.size(), data_offset + sizeof(SVMNodeGlassBsdfData) / sizeof(uint));
  SVMNodeClosureBsdf header;
  memcpy(&header, words.data() + header_offset + 1, sizeof(header));
  globals.svm_nodes.data = words.data();
  globals.svm_nodes.width = int(words.size());
  ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
  std::array<float, SVM_STACK_SIZE> stack{};
  ShaderData sd{};
  sd.N = sd.Ng = sd.wi = make_float3(0.0f, 0.0f, 1.0f);
  sd.num_closure_left = MAX_CLOSURE;
  EXPECT_EQ((svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
                &kg, &sd, stack.data(), one_spectrum(), header,
                PATH_RAY_VISIBILITY_CAMERA, 0, data_offset)), words.size());
  EXPECT_EQ(svm_node_closure_bsdf_skip(data_offset, header.closure_type), words.size());
  ASSERT_EQ(sd.num_closure, 1);
  const MicrofacetBsdf *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
  EXPECT_FLOAT_EQ(bsdf->alpha_x, .04f);
  EXPECT_FLOAT_EQ(bsdf->alpha_y, .04f);
  EXPECT_EQ(bsdf->T, zero_float3());
  EXPECT_FLOAT_EQ(bsdf->ior, 1.5f);
  }
}

TEST_F(materialx_principled_kernel, glass_anisotropic_rt_angular_and_sample_eval_oracle)
{
  MicrofacetBsdf bsdf{};
  bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
  bsdf.T = make_float3(1.0f, 0.0f, 0.0f);
  bsdf.alpha_x = .18f;
  bsdf.alpha_y = .28f;
  bsdf.ior = 1.46f;
  bsdf.weight = one_spectrum();
  bsdf_microfacet_ggx_glass_setup(&bsdf);
  ASSERT_FLOAT_EQ(bsdf.alpha_x, .18f);
  ASSERT_FLOAT_EQ(bsdf.alpha_y, .28f);
  ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
  const float3 wi = make_float3(0.0f, 0.0f, 1.0f);
  /* Independent double-precision Walter BTDF / Smith GGX oracle, not
   * computed with the production D/lambda/Fresnel helpers under test. */
  const struct { float x, y, pdf, eval; } angles[] = {
      {.2f, 0.0f, .3375631031f, .3374492524f},
      {0.0f, .2f, 1.6532260534f, 1.6518781195f}};
  for (const auto &c : angles) {
    float pdf = 0.0f;
    const Spectrum value = bsdf_microfacet_ggx_eval(
        &kg, reinterpret_cast<const ShaderClosure *>(&bsdf), wi,
        make_float3(c.x, c.y, -std::sqrt(.96f)), &pdf);
    EXPECT_NEAR(pdf, c.pdf, 2e-5f);
    EXPECT_NEAR(value.x, c.eval, 2e-5f);
    EXPECT_NEAR(value.y, c.eval, 2e-5f);
    EXPECT_NEAR(value.z, c.eval, 2e-5f);
  }
  for (const float3 random : {make_float3(.13f, .27f, .999f),
                             make_float3(.63f, .42f, .999f),
                             make_float3(.37f, .71f, 0.0f)}) {
    Spectrum sampled = zero_spectrum();
    float3 wo;
    float sample_pdf = 0.0f, eta = 0.0f;
    float2 roughness;
    const int label = bsdf_microfacet_sample<MicrofacetType::GGX>(
        &kg, reinterpret_cast<const ShaderClosure *>(&bsdf), bsdf.N, wi, random,
        &sampled, &wo, &sample_pdf, &roughness, &eta);
    ASSERT_NE(label, LABEL_NONE);
    EXPECT_EQ((label & LABEL_TRANSMIT) != 0, random.z > 0.0f);
    EXPECT_FLOAT_EQ(roughness.x, .18f);
    EXPECT_FLOAT_EQ(roughness.y, .28f);
    float eval_pdf = 0.0f;
    const Spectrum evaluated = bsdf_microfacet_ggx_eval(
        &kg, reinterpret_cast<const ShaderClosure *>(&bsdf), wi, wo, &eval_pdf);
    EXPECT_NEAR(sample_pdf, eval_pdf, 2e-5f * std::max(1.0f, eval_pdf));
    EXPECT_NEAR(sampled.x, evaluated.x, 2e-5f * std::max(1.0f, evaluated.x));
  }
}


TEST_F(materialx_principled_kernel, glass_anisotropic_oblique_rotated_tir_sample_eval)
{
  ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
  for (const bool beckmann : {false, true}) {
    for (const float ior : {1.46f, .66f}) {
      for (const float2 axes : {make_float2(.18f, .28f), make_float2(.000001f, .81f)}) {
        for (const float angle : {0.0f, .43f}) {
          MicrofacetBsdf bsdf{};
          bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
          bsdf.T = make_float3(std::cos(angle), std::sin(angle), 0.0f);
          bsdf.alpha_x = axes.x;
          bsdf.alpha_y = axes.y;
          bsdf.ior = ior;
          bsdf.weight = one_spectrum();
          if (beckmann) { bsdf_microfacet_beckmann_glass_setup(&bsdf); }
          else { bsdf_microfacet_ggx_glass_setup(&bsdf); }
          ASSERT_FLOAT_EQ(bsdf.alpha_x, axes.x);
          ASSERT_FLOAT_EQ(bsdf.alpha_y, axes.y);
          SCOPED_TRACE(testing::Message() << "beckmann=" << beckmann << " ior=" << ior
                       << " alpha=" << axes.x << "," << axes.y << " rotation=" << angle);
          for (const float3 wi : {normalize(make_float3(.3f, .2f, 1.0f)),
                                  normalize(make_float3(.95f, .1f, .2f))}) {
            int valid = 0;
            for (int i = 0; i < 32; ++i) {
              const float3 random = make_float3((i + .5f) / 32.0f,
                                                ((i * 13) % 32 + .5f) / 32.0f,
                                                i % 2 ? .999f : 0.0f);
              Spectrum sampled = zero_spectrum();
              float3 wo;
              float sample_pdf = 0.0f, eta = 0.0f;
              float2 roughness;
              const auto *closure = reinterpret_cast<const ShaderClosure *>(&bsdf);
              const int label = beckmann ?
                  bsdf_microfacet_sample<MicrofacetType::BECKMANN>(
                      &kg, closure, bsdf.N, wi, random, &sampled, &wo,
                      &sample_pdf, &roughness, &eta) :
                  bsdf_microfacet_sample<MicrofacetType::GGX>(
                      &kg, closure, bsdf.N, wi, random, &sampled, &wo,
                      &sample_pdf, &roughness, &eta);
              if (label == LABEL_NONE) { continue; }
              ++valid;
              ASSERT_TRUE(std::isfinite(sample_pdf));
              ASSERT_GT(sample_pdf, 0.0f);
              float eval_pdf = 0.0f;
              const Spectrum evaluated = beckmann ?
                  bsdf_microfacet_beckmann_eval(&kg, closure, wi, wo, &eval_pdf) :
                  bsdf_microfacet_ggx_eval(&kg, closure, wi, wo, &eval_pdf);
              ASSERT_TRUE(std::isfinite(eval_pdf));
              const double hx = (label & LABEL_TRANSMIT) ?
                                    -(double(ior) * wo.x + wi.x) : double(wi.x) + wo.x;
              const double hy = (label & LABEL_TRANSMIT) ?
                                    -(double(ior) * wo.y + wi.y) : double(wi.y) + wo.y;
              const double hz = (label & LABEL_TRANSMIT) ?
                                    -(double(ior) * wo.z + wi.z) : double(wi.z) + wo.z;
              const double length_h = std::sqrt(hx * hx + hy * hy + hz * hz);
              const double local_hx = (double(bsdf.T.x) * hx + double(bsdf.T.y) * hy) / length_h;
              const double local_hy = (-double(bsdf.T.y) * hx + double(bsdf.T.x) * hy) / length_h;
              const double local_hz = hz / length_h;
              const double slope = (local_hx / axes.x) * (local_hx / axes.x) +
                                   (local_hy / axes.y) * (local_hy / axes.y);
              const double independent_d = beckmann ?
                  std::exp(-slope / (local_hz * local_hz)) /
                      (M_PI * axes.x * axes.y * std::pow(local_hz, 4)) :
                  1.0 / (M_PI * axes.x * axes.y *
                         std::pow(slope + local_hz * local_hz, 2));
              SCOPED_TRACE(testing::Message() << "label=" << label << " wi_z=" << wi.z
                           << " sample=" << i << " double_stored_direction_D=" << independent_d);
              EXPECT_NEAR(sample_pdf, eval_pdf, 3e-4f * std::max(1.0f, eval_pdf));
              EXPECT_NEAR(sampled.x, evaluated.x, 3e-4f * std::max(1.0f, evaluated.x));
              EXPECT_FLOAT_EQ(roughness.x, axes.x);
              EXPECT_FLOAT_EQ(roughness.y, axes.y);
            }
            EXPECT_GT(valid, 0) << beckmann << ":" << ior << ":" << axes.x << ":" << angle;
          }
        }
      }
    }
  }
}


TEST_F(materialx_principled_kernel, glass_anisotropic_critical_angle_tir_and_ng_domain)
{
  ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
  for (const bool beckmann : {false, true}) {
    for (const bool tir : {false, true}) {
      MicrofacetBsdf bsdf{};
      bsdf.N = make_float3(0.0f, 0.0f, 1.0f);
      bsdf.T = make_float3(std::cos(.43f), std::sin(.43f), 0.0f);
      bsdf.alpha_x = .0001f;
      bsdf.alpha_y = .0002f;
      bsdf.ior = .66f;
      bsdf.weight = one_spectrum();
      if (beckmann) { bsdf_microfacet_beckmann_glass_setup(&bsdf); }
      else { bsdf_microfacet_ggx_glass_setup(&bsdf); }
      /* Critical cosine sqrt(1-eta^2): straddle it by .01, comfortably
       * above the 1e-4 normal spread. Random z requests transmission;
       * physical total internal reflection must override that request. */
      const float cosine = std::sqrt(1.0f - .66f * .66f) + (tir ? -.01f : .01f);
      const float3 wi = make_float3(std::sqrt(1.0f - cosine * cosine), 0.0f, cosine);
      const float3 random = make_float3(.5f, .5f, .999f);
      Spectrum sampled = zero_spectrum();
      float3 wo;
      float pdf = 0.0f, eta = 0.0f;
      float2 roughness;
      const auto *closure = reinterpret_cast<const ShaderClosure *>(&bsdf);
      const auto sample = [&](const float3 ng) {
        return beckmann ? bsdf_microfacet_sample<MicrofacetType::BECKMANN>(
                              &kg, closure, ng, wi, random, &sampled, &wo,
                              &pdf, &roughness, &eta) :
                          bsdf_microfacet_sample<MicrofacetType::GGX>(
                              &kg, closure, ng, wi, random, &sampled, &wo,
                              &pdf, &roughness, &eta);
      };
      SCOPED_TRACE(testing::Message() << "beckmann=" << beckmann << " tir=" << tir);
      const int label = sample(bsdf.N);
      ASSERT_NE(label, LABEL_NONE);
      EXPECT_EQ(label, (tir ? LABEL_REFLECT : LABEL_TRANSMIT) | LABEL_GLOSSY);
      EXPECT_EQ(wo.z > 0.0f, tir);
      EXPECT_FLOAT_EQ(eta, tir ? 1.0f : .66f);
      EXPECT_FLOAT_EQ(roughness.x, .0001f);
      EXPECT_FLOAT_EQ(roughness.y, .0002f);
      ASSERT_TRUE(std::isfinite(pdf));
      ASSERT_GT(pdf, 0.0f);
      float evaluated_pdf = 0.0f;
      const Spectrum evaluated = beckmann ?
          bsdf_microfacet_beckmann_eval(&kg, closure, wi, wo, &evaluated_pdf) :
          bsdf_microfacet_ggx_eval(&kg, closure, wi, wo, &evaluated_pdf);
      EXPECT_NEAR(pdf, evaluated_pdf, 3e-4f * std::max(1.0f, evaluated_pdf));
      EXPECT_NEAR(sampled.x, evaluated.x, 3e-4f * std::max(1.0f, evaluated.x));
      EXPECT_NEAR(sampled.y, evaluated.y, 3e-4f * std::max(1.0f, evaluated.y));
      EXPECT_NEAR(sampled.z, evaluated.z, 3e-4f * std::max(1.0f, evaluated.z));
      /* The common evaluator must never bypass the sampling Ng domain. */
      EXPECT_EQ(sample(-bsdf.N), LABEL_NONE);
    }
  }
}


#ifdef WITH_USD
TEST_F(materialx_principled_kernel, lama_sdk_column_major_rotation_reader_to_actual_svm)
{
  struct Case { bool conductor; float rotation; float3 normal, uv_tangent, half_vector; };
  const Case cases[] = {
      {true, .2f, make_float3(0, 0, 1), make_float3(-1, 0, 0),
       make_float3(.7306974715f, -.2691791142f, .6273944609f)},
      {false, .18f, make_float3(0, 0, 1), make_float3(1, 0, 0),
       normalize(make_float3(.3f, -.2f, 1.0f))},
      {true, -.2f, make_float3(1, 0, 0), make_float3(0, 0, 1),
       make_float3(.6273944609f, -.2691791142f, .7306974715f)},
      {false, 1.18f, make_float3(1, 0, 0), make_float3(0, 0, 1),
       normalize(make_float3(1.0f, -.2f, .3f))}};
  for (const Case &c : cases) {
    SCOPED_TRACE(testing::Message() << c.conductor << ":" << c.rotation);
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Material"));
    auto surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Surface"));
    auto shader = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Original"));
    shader.CreateIdAttr(pxr::VtValue(pxr::TfToken(c.conductor ?
        "ND_lama_conductor" : "ND_lama_iridescence")));
    shader.CreateInput(pxr::TfToken("roughness"), pxr::SdfValueTypeNames->Float)
        .Set(c.conductor ? .24f : .16f);
    shader.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float)
        .Set(c.conductor ? .35f : .25f);
    shader.CreateInput(pxr::TfToken("anisotropyRotation"), pxr::SdfValueTypeNames->Float)
        .Set(c.rotation);
    shader.CreateInput(pxr::TfToken("anisotropyDirection"), pxr::SdfValueTypeNames->Vector3f)
        .SetSdrMetadataByKey(pxr::TfToken("defaultgeomprop"), "Tworld");
    if (c.conductor) {
      shader.CreateInput(pxr::TfToken("normal"), pxr::SdfValueTypeNames->Vector3f)
          .SetSdrMetadataByKey(pxr::TfToken("defaultgeomprop"), "Nworld");
      shader.CreateInput(pxr::TfToken("fresnelMode"), pxr::SdfValueTypeNames->Int).Set(1);
      shader.CreateInput(pxr::TfToken("tint"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.72f, .12f, .04f));
      /* Retained original Scientific-mode values, authored before reader/lowering. */
      shader.CreateInput(pxr::TfToken("IOR"), pxr::SdfValueTypeNames->Vector3f)
          .Set(pxr::GfVec3f(0.18f, 0.42f, 1.37f));
      shader.CreateInput(pxr::TfToken("extinction"), pxr::SdfValueTypeNames->Vector3f)
          .Set(pxr::GfVec3f(3.42f, 2.35f, 1.77f));
      shader.CreateInput(pxr::TfToken("reflectivity"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.92f, .55f, .12f));
      shader.CreateInput(pxr::TfToken("edgeColor"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.2f, .65f, 1.0f));
    }
    else {
      shader.CreateInput(pxr::TfToken("relativeFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(.58f);
      shader.CreateInput(pxr::TfToken("minFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(380.0f);
      shader.CreateInput(pxr::TfToken("maxFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(780.0f);
      shader.CreateInput(pxr::TfToken("filmIOR"), pxr::SdfValueTypeNames->Float).Set(1.32f);
    }
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("bsdf"), pxr::SdfValueTypeNames->Token)
                    .ConnectToSource(shader.ConnectableAPI(), pxr::TfToken("out")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    ASSERT_TRUE(material.CreateSurfaceOutput().ConnectToSource(
        surface.ConnectableAPI(), pxr::TfToken("out")));
    materialx::Graph source;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower(source, &graph, &error)) << error;
    ShaderNode *node = nullptr;
    for (ShaderNode *candidate : graph.nodes) {
      if (candidate->name == "Original") { node = candidate; }
    }
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->input("Tangent")->link, nullptr);
    ShaderOutput *tangent = node->input("Tangent")->link;
    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    compiler.assign_source_output(tangent);
    compiler.compile_source(tangent->parent);
    /* Inject the retained CubeUV frame at the actual linked consumer input.
     * This checks reader-to-SVM transport and closure frame, not UV fetching. */
    std::array<float, SVM_STACK_SIZE> stack{};
    const int tangent_offset = compiler.stack_assign(node->input("Tangent"));
    ASSERT_GE(tangent_offset, 0);
    ASSERT_LT(tangent_offset + 2, SVM_STACK_SIZE);
    stack[tangent_offset] = c.uv_tangent.x;
    stack[tangent_offset + 1] = c.uv_tangent.y;
    stack[tangent_offset + 2] = c.uv_tangent.z;
    if (ShaderOutput *normal = node->input("Normal")->link) {
      compiler.assign_source_output(normal);
      compiler.compile_source(normal->parent);
      const int normal_offset = compiler.stack_assign(node->input("Normal"));
      ASSERT_GE(normal_offset, 0);
      ASSERT_LT(normal_offset + 2, SVM_STACK_SIZE);
      stack[normal_offset] = c.normal.x;
      stack[normal_offset + 1] = c.normal.y;
      stack[normal_offset + 2] = c.normal.z;
    }
    const auto words = compiler.capture(node);
    Spectrum closure_weight = one_spectrum();
    if (c.conductor) {
      ASSERT_EQ(words.at(0), uint(NODE_CLOSURE_SET_WEIGHT));
      ASSERT_GE(words.size(), 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint));
      SVMNodeClosureSetWeight weight_data;
      memcpy(&weight_data, words.data() + 1, sizeof(weight_data));
      svm_node_closure_set_weight(&closure_weight, weight_data);
      /* Tint transport precedes the separate multi-bounce energy compensation. */
      expect_spectrum(closure_weight, make_float3(0.72f, 0.12f, 0.04f));
    }
    const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
    const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + header_offset + 1, sizeof(header));
    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    ShaderData sd{};
    sd.N = sd.Ng = sd.wi = c.normal;
    sd.num_closure_left = MAX_CLOSURE;
    EXPECT_EQ((svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
                  &kg, &sd, stack.data(), closure_weight, header,
                  PATH_RAY_VISIBILITY_CAMERA, 0, data_offset)), words.size());
    ASSERT_EQ(sd.num_closure, 1);
    const auto *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
    if (c.conductor) {
      ASSERT_EQ(header.closure_type, CLOSURE_BSDF_PHYSICAL_CONDUCTOR);
      ASSERT_EQ(bsdf->fresnel_type, MicrofacetFresnel::CONDUCTOR);
      ASSERT_NE(bsdf->fresnel, nullptr);
      const auto *physical = reinterpret_cast<const FresnelConductor *>(bsdf->fresnel);
      expect_spectrum(physical->ior.re, make_float3(0.18f, 0.42f, 1.37f));
      expect_spectrum(physical->ior.im, make_float3(3.42f, 2.35f, 1.77f));
      EXPECT_FLOAT_EQ(physical->thin_film.thickness, 1e-5f);
      EXPECT_LE(physical->thin_film.thickness, THINFILM_THICKNESS_CUTOFF);
      /* Independent complex-plane polarized Fresnel literals for incident IOR=1.
       * These are bare Fresnel values: no tint or LUT energy is folded into them. */
      const double expected_fresnel[3][3] = {
          {0.9449911374610354, 0.7771558184881083, 0.3736999702850351},
          {0.9395398409334174, 0.7812025071734869, 0.4133678305411447},
          {0.9647601564556485, 0.9102038372415080, 0.7357142614895014}};
      const float cosines[] = {1.0f, 0.5f, 0.1f};
      for (int i = 0; i < 3; ++i) {
        const Spectrum f = fresnel_conductor(cosines[i], physical->ior);
        EXPECT_NEAR(f.x, expected_fresnel[i][0], 2e-6);
        EXPECT_NEAR(f.y, expected_fresnel[i][1], 2e-6);
        EXPECT_NEAR(f.z, expected_fresnel[i][2], 2e-6);
      }
    }
    const double alpha_u = c.conductor ? .0576 : .0256;
    const double alpha_v = c.conductor ? .256036 : .1369;
    EXPECT_NEAR(bsdf->alpha_x, alpha_u, 1e-6);
    EXPECT_NEAR(bsdf->alpha_y, alpha_v, 1e-6);
    /* Independent SDK scalar-constructor oracle. The source spells Rodrigues
     * ROWS, but MDL/GLSL scalar constructors consume COLUMNS. Multiplication
     * below follows actual compiler col-outer,row-inner semantics; never call
     * the native rotate_around_axis routine being tested. */
    const double angle = -2.0 * M_PI * c.rotation;
    const double co = std::cos(angle), si = std::sin(angle), oc = 1.0 - co;
    const double x = c.normal.x, y = c.normal.y, z = c.normal.z;
    const double scalar[9] = {co+oc*x*x, oc*x*y-z*si, oc*x*z+y*si,
                             oc*y*x+z*si, co+oc*y*y, oc*y*z-x*si,
                             oc*z*x-y*si, oc*z*y+x*si, co+oc*z*z};
    double expected[3] = {};
    const double t[3] = {c.uv_tangent.x, c.uv_tangent.y, c.uv_tangent.z};
    for (int col = 0; col < 3; ++col) {
      for (int row = 0; row < 3; ++row) { expected[row] += scalar[col*3+row] * t[col]; }
    }
    const float3 sdk_t = make_float3(expected[0], expected[1], expected[2]);
    const float3 sdk_b = cross(c.normal, sdk_t);
    const float3 expected_native_t = sdk_t;
    EXPECT_NEAR(bsdf->T.x, expected_native_t.x, 2e-6f);
    EXPECT_NEAR(bsdf->T.y, expected_native_t.y, 2e-6f);
    EXPECT_NEAR(bsdf->T.z, expected_native_t.z, 2e-6f);
    const double hx = dot(c.half_vector, sdk_t), hy = dot(c.half_vector, sdk_b);
    const double hz = dot(c.half_vector, c.normal);
    const double denominator = sqr(hx/alpha_u) + sqr(hy/alpha_v) + hz*hz;
    const double sdk_d = 1.0 / (M_PI * alpha_u * alpha_v * denominator * denominator);
    float3 native_t, native_b;
    make_orthonormals_tangent(c.normal, bsdf->T, &native_t, &native_b);
    const float actual_d = bsdf_aniso_D<MicrofacetType::GGX>(bsdf->alpha_x, bsdf->alpha_y,
        make_float3(dot(c.half_vector, native_t), dot(c.half_vector, native_b), hz));
    EXPECT_NEAR(actual_d, sdk_d, 2e-5 * std::max(1.0, sdk_d));
    if (c.conductor && c.rotation == .2f) { EXPECT_NEAR(sdk_d, .22001138124, 2e-8); }
  }
}

TEST_F(materialx_principled_kernel, lama_sdk_extreme_anisotropy_reader_to_actual_svm_physical_axes)
{
  struct Case {
    bool conductor;
    float rotation, roughness, anisotropy;
    double alpha_x, alpha_y;
    float3 normal, uv_tangent, half_vector;
  };
  /* Independent pinned SDK literals: positive selects delta=1-r; negative
   * selects delta=r. No perceptual aspect-ratio or production helper oracle. */
  const Case cases[] = {{true,
                         0.0f,
                         .05f,
                         .4f,
                         .0025,
                         .1849,
                         make_float3(0, 0, 1),
                         make_float3(1, 0, 0),
                         normalize(make_float3(.3f, -.2f, 1))},
                        {true,
                         .2f,
                         .05f,
                         .4f,
                         .0025,
                         .1849,
                         make_float3(0, 0, 1),
                         make_float3(-1, 0, 0),
                         normalize(make_float3(.3f, -.2f, 1))},
                        {true,
                         -.2f,
                         .05f,
                         .4f,
                         .0025,
                         .1849,
                         make_float3(1, 0, 0),
                         make_float3(0, 0, 1),
                         normalize(make_float3(1, -.2f, .3f))},
                        {true,
                         1.2f,
                         .05f,
                         .4f,
                         .0025,
                         .1849,
                         make_float3(1, 0, 0),
                         make_float3(0, 0, 1),
                         normalize(make_float3(1, -.2f, .3f))},
                        {true,
                         .2f,
                         .05f,
                         -.4f,
                         .0025,
                         .0009,
                         make_float3(0, 0, 1),
                         make_float3(1, 0, 0),
                         normalize(make_float3(.3f, -.2f, 1))},
                        /* Unchanged Glass consumer transports the same extreme SDK axes. */
                        {false,
                         .2f,
                         .05f,
                         .4f,
                         .0025,
                         .1849,
                         make_float3(0, 0, 1),
                         make_float3(1, 0, 0),
                         normalize(make_float3(.3f, -.2f, 1))}};
  for (const Case &c : cases) {
    SCOPED_TRACE(testing::Message() << c.conductor << ":" << c.rotation);
    const auto stage = pxr::UsdStage::CreateInMemory();
    const auto material = pxr::UsdShadeMaterial::Define(stage, pxr::SdfPath("/Material"));
    auto surface = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Surface"));
    auto shader = pxr::UsdShadeShader::Define(stage, pxr::SdfPath("/Material/Original"));
    shader.CreateIdAttr(
        pxr::VtValue(pxr::TfToken(c.conductor ? "ND_lama_conductor" : "ND_lama_iridescence")));
    shader.CreateInput(pxr::TfToken("roughness"), pxr::SdfValueTypeNames->Float).Set(c.roughness);
    shader.CreateInput(pxr::TfToken("anisotropy"), pxr::SdfValueTypeNames->Float)
        .Set(c.anisotropy);
    shader.CreateInput(pxr::TfToken("anisotropyRotation"), pxr::SdfValueTypeNames->Float)
        .Set(c.rotation);
    shader.CreateInput(pxr::TfToken("anisotropyDirection"), pxr::SdfValueTypeNames->Vector3f)
        .SetSdrMetadataByKey(pxr::TfToken("defaultgeomprop"), "Tworld");
    if (c.conductor) {
      shader.CreateInput(pxr::TfToken("normal"), pxr::SdfValueTypeNames->Vector3f)
          .SetSdrMetadataByKey(pxr::TfToken("defaultgeomprop"), "Nworld");
      shader.CreateInput(pxr::TfToken("fresnelMode"), pxr::SdfValueTypeNames->Int).Set(1);
      shader.CreateInput(pxr::TfToken("tint"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.72f, .12f, .04f));
      shader.CreateInput(pxr::TfToken("reflectivity"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.92f, .55f, .12f));
      shader.CreateInput(pxr::TfToken("edgeColor"), pxr::SdfValueTypeNames->Color3f)
          .Set(pxr::GfVec3f(.2f, .65f, 1.0f));
    }
    else {
      shader.CreateInput(pxr::TfToken("relativeFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(.58f);
      shader.CreateInput(pxr::TfToken("minFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(380.0f);
      shader.CreateInput(pxr::TfToken("maxFilmThickness"), pxr::SdfValueTypeNames->Float)
          .Set(780.0f);
      shader.CreateInput(pxr::TfToken("filmIOR"), pxr::SdfValueTypeNames->Float).Set(1.32f);
    }
    shader.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    surface.CreateIdAttr(pxr::VtValue(pxr::TfToken("ND_surface")));
    ASSERT_TRUE(surface.CreateInput(pxr::TfToken("bsdf"), pxr::SdfValueTypeNames->Token)
                    .ConnectToSource(shader.ConnectableAPI(), pxr::TfToken("out")));
    surface.CreateOutput(pxr::TfToken("out"), pxr::SdfValueTypeNames->Token);
    ASSERT_TRUE(material.CreateSurfaceOutput().ConnectToSource(surface.ConnectableAPI(),
                                                               pxr::TfToken("out")));
    materialx::Graph source;
    string error;
    ASSERT_TRUE(materialx::read_usdshade_graph(material, &source, &error)) << error;
    const auto original = std::find_if(
        source.nodes.begin(), source.nodes.end(), [](const materialx::Node &n) {
          return n.name == "Original";
        });
    ASSERT_NE(original, source.nodes.end());
    ASSERT_TRUE(original->vector2_inputs.contains("roughness"));
    EXPECT_NEAR(original->vector2_inputs.at("roughness").x, c.alpha_x, 1e-7);
    EXPECT_NEAR(original->vector2_inputs.at("roughness").y, c.alpha_y, 1e-7);
    ASSERT_TRUE(materialx::validate(source)) << error;
    ShaderGraph graph;
    ASSERT_TRUE(materialx::lower(source, &graph, &error)) << error;
    ShaderNode *node = nullptr;
    for (ShaderNode *candidate : graph.nodes) {
      if (candidate->name == "Original") {
        node = candidate;
      }
    }
    ASSERT_NE(node, nullptr);
    ASSERT_NE(node->input("Tangent")->link, nullptr);
    ShaderOutput *tangent = node->input("Tangent")->link;
    CapturePrincipledSVM compiler(scene.get(), progress, &graph);
    compiler.assign_source_output(tangent);
    compiler.compile_source(tangent->parent);
    /* Inject the retained CubeUV frame at the actual linked consumer input.
     * This checks reader-to-SVM transport and closure frame, not UV fetching. */
    std::array<float, SVM_STACK_SIZE> stack{};
    const int tangent_offset = compiler.stack_assign(node->input("Tangent"));
    ASSERT_GE(tangent_offset, 0);
    ASSERT_LT(tangent_offset + 2, SVM_STACK_SIZE);
    stack[tangent_offset] = c.uv_tangent.x;
    stack[tangent_offset + 1] = c.uv_tangent.y;
    stack[tangent_offset + 2] = c.uv_tangent.z;
    if (ShaderOutput *normal = node->input("Normal")->link) {
      compiler.assign_source_output(normal);
      compiler.compile_source(normal->parent);
      const int normal_offset = compiler.stack_assign(node->input("Normal"));
      ASSERT_GE(normal_offset, 0);
      ASSERT_LT(normal_offset + 2, SVM_STACK_SIZE);
      stack[normal_offset] = c.normal.x;
      stack[normal_offset + 1] = c.normal.y;
      stack[normal_offset + 2] = c.normal.z;
    }
    const auto words = compiler.capture(node);
    const int header_offset = 1 + sizeof(SVMNodeClosureSetWeight) / sizeof(uint);
    const int data_offset = header_offset + 1 + sizeof(SVMNodeClosureBsdf) / sizeof(uint);
    SVMNodeClosureBsdf header;
    memcpy(&header, words.data() + header_offset + 1, sizeof(header));
    globals.svm_nodes.data = words.data();
    globals.svm_nodes.width = int(words.size());
    ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
    ShaderData sd{};
    sd.N = sd.Ng = sd.wi = c.normal;
    sd.num_closure_left = MAX_CLOSURE;
    EXPECT_EQ((svm_node_closure_bsdf<KERNEL_FEATURE_NODE_BSDF, SHADER_TYPE_SURFACE>(
                  &kg,
                  &sd,
                  stack.data(),
                  one_spectrum(),
                  header,
                  PATH_RAY_VISIBILITY_CAMERA,
                  0,
                  data_offset)),
              words.size());
    ASSERT_EQ(sd.num_closure, 1);
    const auto *bsdf = reinterpret_cast<const MicrofacetBsdf *>(&sd.closure[0]);
    const double alpha_u = c.alpha_x;
    const double alpha_v = c.alpha_y;
    EXPECT_NEAR(bsdf->alpha_x, alpha_u, 1e-6);
    EXPECT_NEAR(bsdf->alpha_y, alpha_v, 1e-6);
    /* Independent SDK scalar-constructor oracle. The source spells Rodrigues
     * ROWS, but MDL/GLSL scalar constructors consume COLUMNS. Multiplication
     * below follows actual compiler col-outer,row-inner semantics; never call
     * the native rotate_around_axis routine being tested. */
    const double angle = -2.0 * M_PI * c.rotation;
    const double co = std::cos(angle), si = std::sin(angle), oc = 1.0 - co;
    const double x = c.normal.x, y = c.normal.y, z = c.normal.z;
    const double scalar[9] = {co + oc * x * x,
                              oc * x * y - z * si,
                              oc * x * z + y * si,
                              oc * y * x + z * si,
                              co + oc * y * y,
                              oc * y * z - x * si,
                              oc * z * x - y * si,
                              oc * z * y + x * si,
                              co + oc * z * z};
    double expected[3] = {};
    const double t[3] = {c.uv_tangent.x, c.uv_tangent.y, c.uv_tangent.z};
    for (int col = 0; col < 3; ++col) {
      for (int row = 0; row < 3; ++row) {
        expected[row] += scalar[col * 3 + row] * t[col];
      }
    }
    const float3 sdk_t = make_float3(expected[0], expected[1], expected[2]);
    const float3 sdk_b = cross(c.normal, sdk_t);
    const float3 expected_native_t = sdk_t;
    EXPECT_NEAR(bsdf->T.x, expected_native_t.x, 2e-6f);
    EXPECT_NEAR(bsdf->T.y, expected_native_t.y, 2e-6f);
    EXPECT_NEAR(bsdf->T.z, expected_native_t.z, 2e-6f);
    const double hx = dot(c.half_vector, sdk_t), hy = dot(c.half_vector, sdk_b);
    const double hz = dot(c.half_vector, c.normal);
    const double denominator = sqr(hx / alpha_u) + sqr(hy / alpha_v) + hz * hz;
    const double sdk_d = 1.0 / (M_PI * alpha_u * alpha_v * denominator * denominator);
    float3 native_t, native_b;
    make_orthonormals_tangent(c.normal, bsdf->T, &native_t, &native_b);
    const float actual_d = bsdf_aniso_D<MicrofacetType::GGX>(
        bsdf->alpha_x,
        bsdf->alpha_y,
        make_float3(dot(c.half_vector, native_t), dot(c.half_vector, native_b), hz));
    EXPECT_NEAR(actual_d, sdk_d, 2e-5 * std::max(1e-6, sdk_d));
  }
}


#endif


TEST(materialx_attribute_store, preserves_scalar_float_bits_and_modulo_nextafter)
{
  auto bits = [](float v) { uint32_t u; memcpy(&u, &v, sizeof(u)); return u; };
  const uint32_t patterns[] = {0x40000001u, 0xc0000001u, 0u, 0x80000000u,
                              1u, 0x00800000u, 0x7f7fffffu, 0x7f800000u, 0x7fc00001u};
  for (const uint32_t pattern : patterns) {
    float v; memcpy(&v, &pattern, sizeof(v));
    float stack[SVM_STACK_SIZE] = {};
    svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT, stack, 0, make_float3(v));
    EXPECT_EQ(bits(stack[0]), pattern);
  }
  const float next = 2.0000002384185791015625f;
  float stack[SVM_STACK_SIZE] = {};
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT, stack, 0, make_float3(next));
  EXPECT_EQ(bits(safe_floored_modulo(stack[0], 2.0f)), 0x34800000u);
  const dual3 derivative(make_float3(next), make_float3(-0.0f), make_float3(next));
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT, stack, 0, derivative);
  EXPECT_EQ(bits(stack[0]), 0x40000001u);
  EXPECT_EQ(bits(stack[1]), 0x80000000u);
  EXPECT_EQ(bits(stack[2]), 0x40000001u);
}

TEST(materialx_attribute_store, preserves_preconverted_color_mean_fallback_alpha_and_rgb)
{
  auto bits = [](float v) { uint32_t u; memcpy(&u, &v, sizeof(u)); return u; };
  float stack[SVM_STACK_SIZE] = {};
  const float3 rgb = make_float3(1.0f, 2.0f, 4.0f);
  const float mean = average(rgb);  // Surface conversion already averages Color3 once.
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT, stack, 0, make_float3(mean));
  EXPECT_EQ(bits(stack[0]), bits(mean));
  const float4 fallback = make_float4(1.0f, 2.0f, 4.0f, 0.375f);
  const float3 scalar = svm_node_attr_fallback_value<float3>(NODE_ATTR_OUTPUT_FLOAT, fallback);
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT, stack, 0, scalar);
  EXPECT_EQ(bits(stack[0]), bits(scalar.x));
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT_ALPHA, stack, 0,
      svm_node_attr_fallback_value<float3>(NODE_ATTR_OUTPUT_FLOAT_ALPHA, fallback));
  EXPECT_EQ(bits(stack[0]), bits(fallback.w));
  svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT3, stack, 0, rgb);
  EXPECT_EQ(bits(stack[0]), bits(rgb.x));
  EXPECT_EQ(bits(stack[1]), bits(rgb.y));
  EXPECT_EQ(bits(stack[2]), bits(rgb.z));
}


TEST_F(materialx_principled_kernel, scalar_attribute_special_branches_preserve_fac_alpha_and_derivatives)
{
  auto bits = [](float v) { uint32_t u; memcpy(&u, &v, sizeof(u)); return u; };
  ThreadKernelGlobalsCPU kg(globals, nullptr, profiler, 0);
  ShaderData sd{};
  sd.object = OBJECT_NONE;
  sd.u = 0.25f; sd.v = 0.125f;
  sd.du.dx = 0.125f; sd.du.dy = 0.25f;
  sd.dv.dx = 0.5f; sd.dv.dy = 0.125f;
  sd.P = make_float3(1.0f, 2.0f, 4.0f);
  const AttributeDescriptor missing = attribute_not_found();
  for (const int attr : {int(ATTR_STD_UV), int(ATTR_STD_GENERATED)}) {
    SVMNodeAttr node{};
    node.attr = attr;
    sd.type = attr == ATTR_STD_UV ? PRIMITIVE_LAMP : PRIMITIVE_TRIANGLE;
    const float3 raw = svm_node_attr_surface_eval<float3>(&kg, &sd, node, NODE_ATTR_OUTPUT_FLOAT3, missing);
    const dual3 raw_dual = svm_node_attr_surface_eval<dual3>(&kg, &sd, node, NODE_ATTR_OUTPUT_FLOAT3, missing);
    const float historical_scalar = average(raw);
    const dual1 historical_dual = average(raw_dual);
    for (const auto output : {NODE_ATTR_OUTPUT_FLOAT, NODE_ATTR_OUTPUT_FLOAT_ALPHA}) {
      float stack[SVM_STACK_SIZE] = {};
      const float3 converted = svm_node_attr_surface_eval<float3>(&kg, &sd, node, output, missing);
      svm_node_attr_store(output, stack, 0, converted);
      EXPECT_EQ(bits(stack[0]), bits(historical_scalar));
      const dual3 converted_dual = svm_node_attr_surface_eval<dual3>(&kg, &sd, node, output, missing);
      svm_node_attr_store(output, stack, 0, converted_dual);
      EXPECT_EQ(bits(stack[0]), bits(historical_dual.val));
      EXPECT_EQ(bits(stack[1]), bits(historical_dual.dx));
      EXPECT_EQ(bits(stack[2]), bits(historical_dual.dy));
    }
    float stack[SVM_STACK_SIZE] = {};
    svm_node_attr_store(NODE_ATTR_OUTPUT_FLOAT3, stack, 0, raw);
    EXPECT_EQ(bits(stack[0]), bits(raw.x));
    EXPECT_EQ(bits(stack[1]), bits(raw.y));
    EXPECT_EQ(bits(stack[2]), bits(raw.z));
  }
}

CCL_NAMESPACE_END
