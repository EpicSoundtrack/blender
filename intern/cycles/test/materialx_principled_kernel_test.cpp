/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* Actual SVM compiler and CPU closure allocation only; no integrator or pixels. */
#include "device/device.h"
#include "kernel/globals.h"
#include "kernel/svm/closure.h"
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
  std::vector<uint> capture(PrincipledBsdfNode *node)
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
          graph.finalize(scene.get());
          source = nullptr;  // Constant folding may delete the source node.
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

CCL_NAMESPACE_END
