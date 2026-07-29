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
};

struct Link {
  string source_node;
  string source_output;
  Type type;
};

struct FloatInput {
  float value = 0.0f;
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
};

bool validate(const Graph &source);
bool lower(const Graph &source, ShaderGraph *graph);

}  // namespace materialx

CCL_NAMESPACE_END
