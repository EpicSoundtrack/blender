/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/map.h"
#include "util/string.h"
#include "util/types.h"
#include "util/vector.h"

CCL_NAMESPACE_BEGIN

class ShaderGraph;

namespace materialx {

enum class Type {
  Float,
  Vector2,
  Vector3,
  Color3,
  Color4,
  SurfaceShader,
  /** Task 3: metadata-driven terminal routing. A volume terminal closure
   *  (packaged through ND_volume, or a VDF connected directly to the
   *  material volume terminal). */
  VolumeShader,
  /** Task 3: a lightshader terminal. Never lowered into a Surface/Volume
   *  material output -- see Graph::has_light. */
  LightShader,
};

struct Link {
  string source_node;
  string source_output;
  Type type;
};

/**
 * A single manifest-bound output-port descriptor (Phase 1: generic admission
 * and typed output selection).
 *
 * Replaces per-NodeDef admission allowlists: any NodeDef reachable from an
 * authored material terminal is admissible as long as its selected output
 * authenticates exactly against this descriptor (node path, NodeDef
 * identifier, output name, and declared type). Only float, color3, vector2,
 * and vector3 are supported in Phase 1; wider device ABI (Color4,
 * SurfaceShader, ...) is out of scope here.
 */
struct SelectedOutput {
  string node_path;
  string nodedef;
  string output_name;
  Type type;
};

struct FloatInput {
  float value = 0.0f;
  Link link;
  bool is_linked = false;
};

/** Task 3: a color3 terminal input that is either a literal value or a
 *  connected sub-graph, mirroring FloatInput for volume closure inputs
 *  (ND_absorption_vdf/ND_anisotropic_vdf "absorption"/"scattering"). */
struct Color3Input {
  float3 value = make_float3(0.0f, 0.0f, 0.0f);
  Link link;
  bool is_linked = false;
};

struct Node {
  string name;
  string nodedef;
  unordered_map<string, float> inputs;
  unordered_map<string, int> int_inputs;
  unordered_map<string, float3> color3_inputs;
  unordered_map<string, float4> float4_inputs;
  unordered_map<string, float2> vector2_inputs;
  unordered_map<string, float3> vector3_inputs;
  unordered_map<string, string> string_inputs;
  unordered_map<string, string> asset_inputs;
  unordered_map<string, Link> links;
  unordered_map<string, Type> outputs;
};

struct Graph {
  vector<Node> nodes;
  FloatInput displacement;
  FloatInput displacement_scale = {1.0f};
  bool has_displacement = false;

  /**
   * Task 3: metadata-driven terminal routing.
   *
   * Volume terminal, packaged from ND_volume's "vdf" input (or a VDF node
   * connected directly to the material volume terminal) -- one of
   * ND_absorption_vdf (absorption only) or ND_anisotropic_vdf (absorption +
   * scattering + anisotropy). Preserved atomically alongside any co-authored
   * surface/displacement terminals: read_usdshade_graph() only commits
   * `has_volume` (and any other authored slot) after every authored slot on
   * the material has independently validated.
   */
  Color3Input volume_absorption;
  Color3Input volume_scattering;
  FloatInput volume_anisotropy;
  bool has_volume = false;

  /**
   * Task 3: a discovered lightshader terminal. Lightshaders are never
   * folded into the Surface material output -- they are routed through the
   * light path, which is outside the Cycles ShaderGraph this compiler
   * lowers into. `lower()` never connects a light terminal to
   * `graph->output()`; the descriptor here exists so a caller can bind it
   * to the correct Light-object shading slot instead.
   */
  bool has_light = false;
  string light_node_name;
  string light_nodedef;
};

bool validate(const Graph &source);
bool lower(const Graph &source, ShaderGraph *graph);

}  // namespace materialx

CCL_NAMESPACE_END
