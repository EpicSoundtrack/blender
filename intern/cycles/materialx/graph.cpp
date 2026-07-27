/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/graph.h"

#include <cmath>

#include "scene/shader_graph.h"
#include "scene/shader_nodes.h"
#include "util/colorspace.h"
#include "util/path.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

namespace {

constexpr const char *add_float_id = "ND_add_float";
constexpr const char *subtract_float_id = "ND_subtract_float";
constexpr const char *multiply_float_id = "ND_multiply_float";
constexpr const char *mix_float_id = "ND_mix_float";
constexpr const char *mix_color3_id = "ND_mix_color3";
constexpr const char *mix_vector3_id = "ND_mix_vector3";
constexpr const char *divide_float_id = "ND_divide_float";
constexpr const char *invert_float_id = "ND_invert_float";
constexpr const char *clamp_float_id = "ND_clamp_float";
constexpr const char *absval_float_id = "ND_absval_float";
constexpr const char *floor_float_id = "ND_floor_float";
constexpr const char *ceil_float_id = "ND_ceil_float";
constexpr const char *fract_float_id = "ND_fract_float";
constexpr const char *sign_float_id = "ND_sign_float";
constexpr const char *min_float_id = "ND_min_float";
constexpr const char *max_float_id = "ND_max_float";
constexpr const char *sin_float_id = "ND_sin_float";
constexpr const char *cos_float_id = "ND_cos_float";
constexpr const char *tan_float_id = "ND_tan_float";
constexpr const char *exp_float_id = "ND_exp_float";
constexpr const char *smoothstep_float_id = "ND_smoothstep_float";
constexpr const char *remap_float_id = "ND_remap_float";
constexpr const char *range_float_id = "ND_range_float";
constexpr const char *noise2d_float_id = "ND_noise2d_float";
constexpr const char *luminance_color3_id = "ND_luminance_color3";
constexpr const char *convert_float_color3_id = "ND_convert_float_color3";
constexpr const char *constant_float_id = "ND_constant_float";
constexpr const char *constant_color3_id = "ND_constant_color3";
constexpr const char *add_color3_id = "ND_add_color3";
constexpr const char *subtract_color3_id = "ND_subtract_color3";
constexpr const char *multiply_color3_id = "ND_multiply_color3";
constexpr const char *divide_color3_id = "ND_divide_color3";
constexpr const char *invert_color3_id = "ND_invert_color3";
constexpr const char *extract_color3_id = "ND_extract_color3";
constexpr const char *geompropvalue_vector2_id = "ND_geompropvalue_vector2";
constexpr const char *constant_vector2_id = "ND_constant_vector2";
constexpr const char *combine2_vector2_id = "ND_combine2_vector2";
constexpr const char *extract_vector2_id = "ND_extract_vector2";
constexpr const char *ramplr_color3_id = "ND_ramplr_color3";
constexpr const char *ramptb_color3_id = "ND_ramptb_color3";
constexpr const char *ramplr_float_id = "ND_ramplr_float";
constexpr const char *ramptb_float_id = "ND_ramptb_float";
constexpr const char *geompropvalue_vector3_id = "ND_geompropvalue_vector3";
constexpr const char *image_color3_id = "ND_image_color3";
constexpr const char *image_vector3_id = "ND_image_vector3";
constexpr const char *normalmap_float_id = "ND_normalmap_float";
constexpr const char *combine3_vector3_id = "ND_combine3_vector3";
constexpr const char *extract_vector3_id = "ND_extract_vector3";
constexpr const char *open_pbr_surface_id = "ND_open_pbr_surface_surfaceshader";

bool scalar_math_type(const string &nodedef, NodeMathType *math_type)
{
  if (nodedef == add_float_id) {
    *math_type = NODE_MATH_ADD;
  }
  else if (nodedef == subtract_float_id) {
    *math_type = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == multiply_float_id) {
    *math_type = NODE_MATH_MULTIPLY;
  }
  else if (nodedef == divide_float_id) {
    *math_type = NODE_MATH_DIVIDE;
  }
  else if (nodedef == invert_float_id) {
    *math_type = NODE_MATH_SUBTRACT;
  }
  else if (nodedef == absval_float_id) {
    *math_type = NODE_MATH_ABSOLUTE;
  }
  else if (nodedef == floor_float_id) {
    *math_type = NODE_MATH_FLOOR;
  }
  else if (nodedef == ceil_float_id) {
    *math_type = NODE_MATH_CEIL;
  }
  else if (nodedef == fract_float_id) {
    *math_type = NODE_MATH_FRACTION;
  }
  else if (nodedef == sign_float_id) {
    *math_type = NODE_MATH_SIGN;
  }
  else if (nodedef == min_float_id) {
    *math_type = NODE_MATH_MINIMUM;
  }
  else if (nodedef == max_float_id) {
    *math_type = NODE_MATH_MAXIMUM;
  }
  else if (nodedef == sin_float_id) {
    *math_type = NODE_MATH_SINE;
  }
  else if (nodedef == cos_float_id) {
    *math_type = NODE_MATH_COSINE;
  }
  else if (nodedef == tan_float_id) {
    *math_type = NODE_MATH_TANGENT;
  }
  else if (nodedef == exp_float_id) {
    *math_type = NODE_MATH_EXPONENT;
  }
  else {
    return false;
  }
  return true;
}

bool scalar_math_is_unary(const string &nodedef)
{
  return nodedef == absval_float_id || nodedef == floor_float_id || nodedef == ceil_float_id ||
         nodedef == fract_float_id || nodedef == sign_float_id || nodedef == sin_float_id ||
         nodedef == cos_float_id || nodedef == tan_float_id || nodedef == exp_float_id;
}

bool vector_math_type(const string &nodedef, NodeVectorMathType *math_type)
{
  NodeVectorMathType result;
  if (nodedef == "ND_add_vector3" || nodedef == "ND_add_vector3FA") result = NODE_VECTOR_MATH_ADD;
  else if (nodedef == "ND_subtract_vector3" || nodedef == "ND_subtract_vector3FA") result = NODE_VECTOR_MATH_SUBTRACT;
  else if (nodedef == "ND_multiply_vector3") result = NODE_VECTOR_MATH_MULTIPLY;
  else if (nodedef == "ND_multiply_vector3FA") result = NODE_VECTOR_MATH_SCALE;
  else if (nodedef == "ND_divide_vector3") result = NODE_VECTOR_MATH_DIVIDE;
  else if (nodedef == "ND_crossproduct_vector3") result = NODE_VECTOR_MATH_CROSS_PRODUCT;
  else if (nodedef == "ND_dotproduct_vector3") result = NODE_VECTOR_MATH_DOT_PRODUCT;
  else if (nodedef == "ND_distance_vector3") result = NODE_VECTOR_MATH_DISTANCE;
  else if (nodedef == "ND_magnitude_vector3") result = NODE_VECTOR_MATH_LENGTH;
  else if (nodedef == "ND_normalize_vector3") result = NODE_VECTOR_MATH_NORMALIZE;
  else if (nodedef == "ND_absval_vector3") result = NODE_VECTOR_MATH_ABSOLUTE;
  else if (nodedef == "ND_floor_vector3") result = NODE_VECTOR_MATH_FLOOR;
  else if (nodedef == "ND_ceil_vector3") result = NODE_VECTOR_MATH_CEIL;
  else if (nodedef == "ND_fract_vector3") result = NODE_VECTOR_MATH_FRACTION;
  else if (nodedef == "ND_sin_vector3") result = NODE_VECTOR_MATH_SINE;
  else if (nodedef == "ND_cos_vector3") result = NODE_VECTOR_MATH_COSINE;
  else if (nodedef == "ND_tan_vector3") result = NODE_VECTOR_MATH_TANGENT;
  else if (nodedef == "ND_min_vector3") result = NODE_VECTOR_MATH_MINIMUM;
  else if (nodedef == "ND_max_vector3") result = NODE_VECTOR_MATH_MAXIMUM;
  else if (nodedef == "ND_sign_vector3") result = NODE_VECTOR_MATH_SIGN;
  else return false;
  if (math_type) *math_type = result;
  return true;
}

bool vector2_math_type(const string &nodedef, NodeVectorMathType *math_type)
{
  if (nodedef == "ND_add_vector2" || nodedef == "ND_add_vector2FA") *math_type = NODE_VECTOR_MATH_ADD;
  else if (nodedef == "ND_subtract_vector2" || nodedef == "ND_subtract_vector2FA") *math_type = NODE_VECTOR_MATH_SUBTRACT;
  else if (nodedef == "ND_multiply_vector2") *math_type = NODE_VECTOR_MATH_MULTIPLY;
  else if (nodedef == "ND_multiply_vector2FA") *math_type = NODE_VECTOR_MATH_SCALE;
  else if (nodedef == "ND_min_vector2") *math_type = NODE_VECTOR_MATH_MINIMUM;
  else if (nodedef == "ND_max_vector2") *math_type = NODE_VECTOR_MATH_MAXIMUM;
  else if (nodedef == "ND_divide_vector2") *math_type = NODE_VECTOR_MATH_DIVIDE;
  else if (nodedef == "ND_magnitude_vector2") *math_type = NODE_VECTOR_MATH_LENGTH;
  else if (nodedef == "ND_dotproduct_vector2") *math_type = NODE_VECTOR_MATH_DOT_PRODUCT;
  else if (nodedef == "ND_distance_vector2") *math_type = NODE_VECTOR_MATH_DISTANCE;
  else if (nodedef == "ND_normalize_vector2") *math_type = NODE_VECTOR_MATH_NORMALIZE;
  else if (nodedef == "ND_absval_vector2") *math_type = NODE_VECTOR_MATH_ABSOLUTE;
  else if (nodedef == "ND_floor_vector2") *math_type = NODE_VECTOR_MATH_FLOOR;
  else if (nodedef == "ND_ceil_vector2") *math_type = NODE_VECTOR_MATH_CEIL;
  else if (nodedef == "ND_fract_vector2") *math_type = NODE_VECTOR_MATH_FRACTION;
  else if (nodedef == "ND_sin_vector2") *math_type = NODE_VECTOR_MATH_SINE;
  else if (nodedef == "ND_cos_vector2") *math_type = NODE_VECTOR_MATH_COSINE;
  else if (nodedef == "ND_tan_vector2") *math_type = NODE_VECTOR_MATH_TANGENT;
  else if (nodedef == "ND_sign_vector2") *math_type = NODE_VECTOR_MATH_SIGN;
  else return false;
  return true;
}

bool vector2_math_is_unary(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector2" || nodedef == "ND_normalize_vector2" ||
         nodedef == "ND_absval_vector2" || nodedef == "ND_floor_vector2" ||
         nodedef == "ND_ceil_vector2" || nodedef == "ND_fract_vector2" ||
         nodedef == "ND_sin_vector2" || nodedef == "ND_cos_vector2" ||
         nodedef == "ND_tan_vector2" || nodedef == "ND_sign_vector2";
}

bool vector2_math_returns_float(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector2" || nodedef == "ND_dotproduct_vector2" ||
         nodedef == "ND_distance_vector2";
}

bool vector2_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_multiply_vector2FA" || nodedef == "ND_add_vector2FA" ||
         nodedef == "ND_subtract_vector2FA";
}

bool vector2_math_uses_scalar_broadcast_second(const string &nodedef)
{
  return nodedef == "ND_add_vector2FA" || nodedef == "ND_subtract_vector2FA";
}

bool vector_math_returns_float(const string &nodedef)
{
  return nodedef == "ND_dotproduct_vector3" || nodedef == "ND_distance_vector3" ||
         nodedef == "ND_magnitude_vector3";
}

bool vector_math_is_unary(const string &nodedef)
{
  return nodedef == "ND_magnitude_vector3" || nodedef == "ND_normalize_vector3" ||
         nodedef == "ND_absval_vector3" || nodedef == "ND_floor_vector3" ||
         nodedef == "ND_ceil_vector3" || nodedef == "ND_fract_vector3" ||
         nodedef == "ND_sin_vector3" || nodedef == "ND_cos_vector3" ||
         nodedef == "ND_tan_vector3" || nodedef == "ND_sign_vector3";
}

bool vector_math_uses_scalar_second(const string &nodedef)
{
  return nodedef == "ND_multiply_vector3FA" || nodedef == "ND_add_vector3FA" ||
         nodedef == "ND_subtract_vector3FA";
}

bool vector_math_uses_scalar_broadcast_second(const string &nodedef)
{
  return nodedef == "ND_add_vector3FA" || nodedef == "ND_subtract_vector3FA";
}

bool color_mix_type(const string &nodedef, NodeMix *mix_type)
{
  if (nodedef == add_color3_id) {
    *mix_type = NODE_MIX_ADD;
  }
  else if (nodedef == subtract_color3_id) {
    *mix_type = NODE_MIX_SUB;
  }
  else if (nodedef == multiply_color3_id) {
    *mix_type = NODE_MIX_MUL;
  }
  else if (nodedef == divide_color3_id) {
    *mix_type = NODE_MIX_DIV;
  }
  else if (nodedef == invert_color3_id) {
    *mix_type = NODE_MIX_SUB;
  }
  else {
    return false;
  }
  return true;
}

Type mix_value_type(const string &nodedef)
{
  if (nodedef == mix_float_id) return Type::Float;
  if (nodedef == mix_color3_id) return Type::Color3;
  return Type::Vector3;
}

bool is_mix(const string &nodedef)
{
  return nodedef == mix_float_id || nodedef == mix_color3_id || nodedef == mix_vector3_id;
}

bool is_scalar_ramp(const string &nodedef)
{
  return nodedef == ramplr_float_id || nodedef == ramptb_float_id;
}

bool is_smoothstep_float(const string &nodedef)
{
  return nodedef == smoothstep_float_id;
}

bool is_linear_range_float(const string &nodedef)
{
  return nodedef == remap_float_id || nodedef == range_float_id;
}

bool is_luminance_color3(const string &nodedef)
{
  return nodedef == luminance_color3_id;
}

bool validate_link(const Link &link,
                   const Type expected_type,
                   const unordered_map<string, const Node *> &nodes_by_name)
{
  const auto source_node = nodes_by_name.find(link.source_node);
  if (link.type != expected_type || source_node == nodes_by_name.end()) {
    return false;
  }
  const auto source_output = source_node->second->outputs.find(link.source_output);
  return source_output != source_node->second->outputs.end() &&
         source_output->second == expected_type;
}

enum class VisitState {
  Visiting,
  Visited,
};

bool validate_acyclic(const Node &node,
                      const unordered_map<string, const Node *> &nodes_by_name,
                      unordered_map<string, VisitState> *visit_states)
{
  const auto [state, inserted] = visit_states->emplace(node.name, VisitState::Visiting);
  if (!inserted) {
    return state->second == VisitState::Visited;
  }

  for (const auto &item : node.links) {
    const Link &link = item.second;
    const auto source = nodes_by_name.find(link.source_node);
    if (source == nodes_by_name.end() ||
        !validate_acyclic(*source->second, nodes_by_name, visit_states))
    {
      return false;
    }
  }

  state->second = VisitState::Visited;
  return true;
}

bool validate(const Graph &source, unordered_map<string, const Node *> *nodes_by_name)
{
  for (const Node &node : source.nodes) {
    if (node.name.empty() || !nodes_by_name->emplace(node.name, &node).second) {
      return false;
    }
  }

  for (const Node &node : source.nodes) {
    if (is_mix(node.nodedef)) {
      const Type value_type = mix_value_type(node.nodedef);
      const auto output = node.outputs.find("out");
      const auto valid_value = [&](const char *name) {
        const bool literal = value_type == Type::Float ? node.inputs.contains(name) :
                             value_type == Type::Color3 ? node.color3_inputs.contains(name) :
                                                          node.vector3_inputs.contains(name);
        const auto link = node.links.find(name);
        return literal != (link != node.links.end()) &&
               (!literal || (value_type != Type::Float || std::isfinite(node.inputs.at(name)))) &&
               (link == node.links.end() || validate_link(link->second, value_type, *nodes_by_name));
      };
      const auto valid_factor = [&]() {
        const auto literal = node.inputs.find("mix");
        const auto link = node.links.find("mix");
        return (literal != node.inputs.end()) != (link != node.links.end()) &&
               (literal == node.inputs.end() || std::isfinite(literal->second)) &&
               (link == node.links.end() || validate_link(link->second, Type::Float, *nodes_by_name));
      };
      if (!valid_value("bg") || !valid_value("fg") || !valid_factor() ||
          node.links.size() + node.inputs.size() + node.color3_inputs.size() +
                  node.vector3_inputs.size() !=
              3 ||
          !node.int_inputs.empty() || !node.vector2_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != value_type ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }
    NodeMathType unused_math_type;
    if (scalar_math_type(node.nodedef, &unused_math_type)) {
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      const bool is_invert = node.nodedef == invert_float_id;
      const char *first_input = is_unary ? "in" : (is_invert ? "amount" : "in1");
      const char *second_input = is_invert ? "in" : "in2";
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if ((literal != node.inputs.end()) == (link != node.links.end())) {
          return false;
        }
        return literal != node.inputs.end() ?
                   std::isfinite(literal->second) :
                   validate_link(link->second, Type::Float, *nodes_by_name);
      };
      if (!valid_operand(first_input) || (!is_unary && !valid_operand(second_input)) ||
          (node.nodedef == divide_float_id &&
           (node.inputs.find("in2") == node.inputs.end() || node.inputs.at("in2") == 0.0f)) ||
          node.inputs.size() + node.links.size() != (is_unary ? 1 : 2) || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == convert_float_color3_id) {
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() || !validate_link(input->second, Type::Float, *nodes_by_name) ||
          node.links.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() || output->second != Type::Color3 ||
          node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == clamp_float_id) {
      const auto output = node.outputs.find("out");
      const auto valid_operand = [&](const char *name) {
        const auto literal = node.inputs.find(name);
        const auto link = node.links.find(name);
        if ((literal != node.inputs.end()) == (link != node.links.end())) {
          return false;
        }
        return literal != node.inputs.end() ?
                   std::isfinite(literal->second) :
                   validate_link(link->second, Type::Float, *nodes_by_name);
      };
      if (!valid_operand("in") || !valid_operand("low") || !valid_operand("high") ||
          node.inputs.size() + node.links.size() != 3 || !node.int_inputs.empty() ||
          !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_smoothstep_float(node.nodedef)) {
      const auto input = node.inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto low = node.inputs.find("low");
      const auto high = node.inputs.find("high");
      const auto output = node.outputs.find("out");
      /* MaterialX delegates smoothstep to target shader languages, where non-increasing
       * edges are undefined.  Reject them rather than choosing a Cycles-specific result. */
      if ((input == node.inputs.end()) == (input_link == node.links.end()) ||
          (input != node.inputs.end() && !std::isfinite(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Float, *nodes_by_name)) ||
          low == node.inputs.end() || high == node.inputs.end() || !std::isfinite(low->second) ||
          !std::isfinite(high->second) || low->second >= high->second ||
          node.inputs.size() != (input == node.inputs.end() ? 2 : 3) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (is_linear_range_float(node.nodedef)) {
      const auto input = node.inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto valid_finite_input = [&](const char *name) {
        const auto value = node.inputs.find(name);
        return value != node.inputs.end() && std::isfinite(value->second);
      };
      if ((input == node.inputs.end()) == (input_link == node.links.end()) ||
          (input != node.inputs.end() && !std::isfinite(input->second)) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Float, *nodes_by_name)) ||
          !valid_finite_input("inlow") || !valid_finite_input("inhigh") ||
          !valid_finite_input("outlow") || !valid_finite_input("outhigh") ||
          node.inputs.at("inlow") == node.inputs.at("inhigh") ||
          node.inputs.size() != (input == node.inputs.end() ? 4 : 5) ||
          node.links.size() != (input_link == node.links.end() ? 0 : 1) ||
          !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || output == node.outputs.end() ||
          output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      if (node.nodedef == remap_float_id) {
        if (!node.int_inputs.empty()) return false;
      }
      else {
        const auto doclamp = node.int_inputs.find("doclamp");
        if (doclamp == node.int_inputs.end() || (doclamp->second != 0 && doclamp->second != 1) ||
            node.int_inputs.size() != 1 ||
            (doclamp->second && node.inputs.at("outlow") > node.inputs.at("outhigh")))
        {
          return false;
        }
      }
      continue;
    }

    if (node.nodedef == noise2d_float_id) {
      const auto amplitude = node.inputs.find("amplitude");
      const auto pivot = node.inputs.find("pivot");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (amplitude == node.inputs.end() || pivot == node.inputs.end() ||
          !std::isfinite(amplitude->second) || !std::isfinite(pivot->second) ||
          texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float ||
          node.inputs.size() != 2 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_luminance_color3(node.nodedef)) {
      const auto input = node.links.find("in");
      const auto coefficients = node.color3_inputs.find("lumacoeffs");
      const auto output = node.outputs.find("out");
      if (input == node.links.end() ||
          !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          coefficients == node.color3_inputs.end() ||
          !std::isfinite(coefficients->second.x) || !std::isfinite(coefficients->second.y) ||
          !std::isfinite(coefficients->second.z) || node.links.size() != 1 ||
          node.color3_inputs.size() != 1 || !node.inputs.empty() || !node.int_inputs.empty() ||
          !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty() ||
          output == node.outputs.end() || output->second != Type::Float || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_float_id) {
      const auto value = node.inputs.find("value");
      const auto output = node.outputs.find("out");
      if (value == node.inputs.end() || output == node.outputs.end() ||
          output->second != Type::Float)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_color3_id) {
      const auto value = node.color3_inputs.find("value");
      const auto output = node.outputs.find("out");
      if (value == node.color3_inputs.end() || output == node.outputs.end() ||
          output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    NodeMix unused_mix_type;
    if (color_mix_type(node.nodedef, &unused_mix_type)) {
      const bool is_invert = node.nodedef == invert_color3_id;
      const auto first = node.links.find(is_invert ? "amount" : "in1");
      const auto second = node.links.find(is_invert ? "in" : "in2");
      const auto output = node.outputs.find("out");
      if (first == node.links.end() || second == node.links.end() ||
          !validate_link(first->second, Type::Color3, *nodes_by_name) ||
          !validate_link(second->second, Type::Color3, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == extract_color3_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      const auto output = node.outputs.find("out");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 2 ||
          input == node.links.end() ||
          !validate_link(input->second, Type::Color3, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == constant_vector2_id) {
      if (node.vector2_inputs.size() != 1 || node.vector2_inputs.find("value") == node.vector2_inputs.end() ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == combine2_vector2_id) {
      for (const char *component : {"in1", "in2"}) {
        const bool has_value = node.inputs.find(component) != node.inputs.end();
        const bool has_link = node.links.find(component) != node.links.end();
        if (has_value == has_link || (has_link && !validate_link(node.links.at(component), Type::Float, *nodes_by_name))) return false;
      }
      if (node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector2 || node.links.size() + node.inputs.size() != 2 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == extract_vector2_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 1 || input == node.links.end() ||
          !validate_link(input->second, Type::Vector2, *nodes_by_name) || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Float || node.links.size() != 1 || node.int_inputs.size() != 1 ||
          !node.inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    NodeVectorMathType vector2_type;
    if (vector2_math_type(node.nodedef, &vector2_type)) {
      const bool unary = vector2_math_is_unary(node.nodedef);
      const bool scalar_second = vector2_math_uses_scalar_second(node.nodedef);
      const char *first_name = unary ? "in" : "in1";
      for (const char *input_name : {first_name, unary ? nullptr : "in2"}) {
        if (input_name == nullptr) {
          continue;
        }
        const bool has_value = scalar_second && string(input_name) == "in2" ?
                                   node.inputs.find(input_name) != node.inputs.end() :
                                   node.vector2_inputs.find(input_name) != node.vector2_inputs.end();
        const bool has_link = node.links.find(input_name) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(input_name),
                                        scalar_second && string(input_name) == "in2" ? Type::Float : Type::Vector2,
                                        *nodes_by_name))) return false;
      }
      const size_t input_count = unary ? 1 : 2;
      const Type output_type = vector2_math_returns_float(node.nodedef) ? Type::Float : Type::Vector2;
      if (node.outputs.size() != 1 || node.outputs.at("out") != output_type ||
          node.links.size() + node.vector2_inputs.size() + node.inputs.size() != input_count ||
          (!scalar_second && !node.inputs.empty()) || (scalar_second && node.inputs.size() > 1) ||
          (node.nodedef == "ND_divide_vector2" &&
           (node.links.find("in2") != node.links.end() || node.vector2_inputs.find("in2") == node.vector2_inputs.end() ||
            !std::isfinite(node.vector2_inputs.at("in2").x) ||
            !std::isfinite(node.vector2_inputs.at("in2").y) ||
            node.vector2_inputs.at("in2").x == 0.0f || node.vector2_inputs.at("in2").y == 0.0f)) ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == geompropvalue_vector2_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second.empty() ||
          output == node.outputs.end() || output->second != Type::Vector2)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == geompropvalue_vector3_id) {
      const auto geomprop = node.string_inputs.find("geomprop");
      const auto output = node.outputs.find("out");
      if (geomprop == node.string_inputs.end() || geomprop->second != "Nworld" ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.string_inputs.size() != 1 || node.outputs.size() != 1 || !node.inputs.empty() ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.asset_inputs.empty() || !node.links.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_color3_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3)
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id) {
      const bool top_to_bottom = node.nodedef == ramptb_color3_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      const auto first = node.color3_inputs.find(first_name);
      const auto second = node.color3_inputs.find(second_name);
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (first == node.color3_inputs.end() || second == node.color3_inputs.end() ||
          texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Color3 ||
          node.color3_inputs.size() != 2 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      const auto first = node.inputs.find(first_name);
      const auto second = node.inputs.find(second_name);
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (first == node.inputs.end() || second == node.inputs.end() ||
          !std::isfinite(first->second) || !std::isfinite(second->second) ||
          texcoord == node.links.end() || !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Float ||
          node.inputs.size() != 2 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector2_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == image_vector3_id) {
      const auto file = node.asset_inputs.find("file");
      const auto texcoord = node.links.find("texcoord");
      const auto output = node.outputs.find("out");
      if (file == node.asset_inputs.end() || file->second.empty() ||
          path_is_relative(file->second) || !path_is_file(file->second) ||
          path_file_size(file->second) == 0 || texcoord == node.links.end() ||
          !validate_link(texcoord->second, Type::Vector2, *nodes_by_name) ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.asset_inputs.size() != 1 || node.links.size() != 1 || node.outputs.size() != 1 ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == normalmap_float_id) {
      const auto scale = node.inputs.find("scale");
      const auto input_value = node.vector3_inputs.find("in");
      const auto input_link = node.links.find("in");
      const auto output = node.outputs.find("out");
      const auto linked_source = input_link == node.links.end() ?
                                     nodes_by_name->end() :
                                     nodes_by_name->find(input_link->second.source_node);
      const bool linked_image = linked_source != nodes_by_name->end() &&
                                linked_source->second->nodedef == image_vector3_id;
      if ((scale != node.inputs.end() &&
           (!std::isfinite(scale->second) || scale->second != 1.0f)) ||
          ((input_value != node.vector3_inputs.end()) == (input_link != node.links.end())) ||
          (input_link != node.links.end() &&
           !validate_link(input_link->second, Type::Vector3, *nodes_by_name)) ||
          (input_link != node.links.end() && !linked_image &&
           linked_source->second->nodedef != "ND_constant_vector3" &&
           linked_source->second->nodedef != "ND_normalize_vector3" &&
           linked_source->second->nodedef != "ND_add_vector3" &&
           linked_source->second->nodedef != "ND_subtract_vector3" &&
           linked_source->second->nodedef != "ND_multiply_vector3" &&
           linked_source->second->nodedef != "ND_divide_vector3" &&
           linked_source->second->nodedef != "ND_crossproduct_vector3" &&
           linked_source->second->nodedef != "ND_absval_vector3" &&
           linked_source->second->nodedef != "ND_floor_vector3" &&
           linked_source->second->nodedef != "ND_ceil_vector3" &&
           linked_source->second->nodedef != "ND_fract_vector3" &&
           linked_source->second->nodedef != "ND_sin_vector3" &&
           linked_source->second->nodedef != "ND_cos_vector3" &&
           linked_source->second->nodedef != "ND_tan_vector3" &&
           linked_source->second->nodedef != "ND_min_vector3" &&
           linked_source->second->nodedef != "ND_max_vector3" &&
           linked_source->second->nodedef != "ND_sign_vector3" &&
           linked_source->second->nodedef != "ND_multiply_vector3FA" &&
           linked_source->second->nodedef != "ND_add_vector3FA" &&
           linked_source->second->nodedef != "ND_subtract_vector3FA" &&
           linked_source->second->nodedef != mix_vector3_id &&
           linked_source->second->nodedef != combine3_vector3_id) ||
          output == node.outputs.end() || output->second != Type::Vector3 ||
          node.inputs.size() > 1 || node.vector3_inputs.size() > 1 || node.links.size() > 1 ||
          node.outputs.size() != 1 || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty())
      {
        return false;
      }
      continue;
    }

    if (node.nodedef == "ND_constant_vector3") {
      if (node.vector3_inputs.size() != 1 || node.vector3_inputs.find("value") == node.vector3_inputs.end() ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Vector3 || !node.links.empty() ||
          !node.inputs.empty() || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == combine3_vector3_id) {
      size_t component_count = 0;
      for (const char *component : {"in1", "in2", "in3"}) {
        const bool has_value = node.inputs.find(component) != node.inputs.end();
        const bool has_link = node.links.find(component) != node.links.end();
        if (has_value == has_link ||
            (has_link && !validate_link(node.links.at(component), Type::Float, *nodes_by_name))) {
          return false;
        }
        component_count += size_t(has_value || has_link);
      }
      if (component_count != 3 || node.outputs.size() != 1 ||
          node.outputs.at("out") != Type::Vector3 || node.links.size() + node.inputs.size() != 3 ||
          !node.int_inputs.empty() || !node.color3_inputs.empty() || !node.vector3_inputs.empty() ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    if (node.nodedef == extract_vector3_id) {
      const auto index = node.int_inputs.find("index");
      const auto input = node.links.find("in");
      if (index == node.int_inputs.end() || index->second < 0 || index->second > 2 ||
          input == node.links.end() || !validate_link(input->second, Type::Vector3, *nodes_by_name) ||
          node.outputs.size() != 1 || node.outputs.at("out") != Type::Float || node.links.size() != 1 ||
          node.int_inputs.size() != 1 || !node.inputs.empty() || !node.color3_inputs.empty() ||
          !node.vector3_inputs.empty() || !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }
    NodeVectorMathType vector_type;
    if (vector_math_type(node.nodedef, &vector_type)) {
      const bool unary = vector_math_is_unary(node.nodedef);
      const bool scalar_second = vector_math_uses_scalar_second(node.nodedef);
      const char *first = unary ? "in" : "in1";
      const Type output_type = vector_math_returns_float(node.nodedef) ? Type::Float : Type::Vector3;
      const bool has_first_link = node.links.find(first) != node.links.end();
      const bool has_first_value = node.vector3_inputs.find(first) != node.vector3_inputs.end();
      const bool needs_second = !unary;
      const bool has_second_link = node.links.find("in2") != node.links.end();
      const bool has_second_value = scalar_second ? node.inputs.find("in2") != node.inputs.end() :
                                                   node.vector3_inputs.find("in2") != node.vector3_inputs.end();
      if (node.outputs.size() != 1 || node.outputs.at("out") != output_type ||
          has_first_link == has_first_value || (needs_second && has_second_link == has_second_value) ||
          (!needs_second && (has_second_link || has_second_value)) ||
          (has_first_link && !validate_link(node.links.at(first), Type::Vector3, *nodes_by_name)) ||
          (has_second_link && !validate_link(node.links.at("in2"), scalar_second ? Type::Float : Type::Vector3, *nodes_by_name)) ||
          node.links.size() != size_t(has_first_link) + size_t(has_second_link) ||
          node.vector3_inputs.size() != size_t(has_first_value) +
                                            size_t(!scalar_second && has_second_value) ||
          node.inputs.size() != size_t(scalar_second && has_second_value) || !node.int_inputs.empty() || !node.color3_inputs.empty() ||
          (node.nodedef == "ND_divide_vector3" &&
           (has_second_link || !has_second_value || node.vector3_inputs.at("in2").x == 0.0f ||
            !std::isfinite(node.vector3_inputs.at("in2").x) ||
            !std::isfinite(node.vector3_inputs.at("in2").y) ||
            !std::isfinite(node.vector3_inputs.at("in2").z) ||
            node.vector3_inputs.at("in2").y == 0.0f || node.vector3_inputs.at("in2").z == 0.0f)) ||
          !node.string_inputs.empty() || !node.asset_inputs.empty()) return false;
      continue;
    }

    if (node.nodedef == open_pbr_surface_id) {
      const auto output = node.outputs.find("out");
      const auto base_color = node.links.find("base_color");
      const auto base_weight = node.links.find("base_weight");
      const auto metalness = node.links.find("base_metalness");
      const auto roughness = node.links.find("specular_roughness");
      const auto ior = node.links.find("specular_ior");
      const auto opacity = node.links.find("geometry_opacity");
      const auto emission_color = node.links.find("emission_color");
      const auto emission_luminance = node.links.find("emission_luminance");
      const auto normal = node.links.find("geometry_normal");
      const auto coat_normal = node.links.find("geometry_coat_normal");
      const auto coat_weight = node.links.find("coat_weight");
      const auto coat_color = node.links.find("coat_color");
      const auto coat_roughness = node.links.find("coat_roughness");
      const auto coat_ior = node.links.find("coat_ior");
      const auto fuzz_weight = node.links.find("fuzz_weight");
      const auto fuzz_color = node.links.find("fuzz_color");
      const auto fuzz_roughness = node.links.find("fuzz_roughness");
      if (output == node.outputs.end() || output->second != Type::SurfaceShader) {
        return false;
      }

      const bool has_base_color_value = node.color3_inputs.find("base_color") !=
                                        node.color3_inputs.end();
      const bool has_base_weight_value = node.inputs.find("base_weight") != node.inputs.end();
      const bool has_metalness_value = node.inputs.find("base_metalness") != node.inputs.end();
      const bool has_roughness_value = node.inputs.find("specular_roughness") != node.inputs.end();
      const bool has_ior_value = node.inputs.find("specular_ior") != node.inputs.end();
      const bool has_opacity_value = node.inputs.find("geometry_opacity") != node.inputs.end();
      const bool has_emission_color_value = node.color3_inputs.find("emission_color") !=
                                            node.color3_inputs.end();
      const bool has_emission_luminance_value = node.inputs.find("emission_luminance") !=
                                                node.inputs.end();
      const bool has_coat_weight_value = node.inputs.find("coat_weight") != node.inputs.end();
      const bool has_coat_color_value = node.color3_inputs.find("coat_color") !=
                                        node.color3_inputs.end();
      const bool has_coat_roughness_value = node.inputs.find("coat_roughness") != node.inputs.end();
      const bool has_coat_ior_value = node.inputs.find("coat_ior") != node.inputs.end();
      const bool has_fuzz_weight_value = node.inputs.find("fuzz_weight") != node.inputs.end();
      const bool has_fuzz_color_value = node.color3_inputs.find("fuzz_color") !=
                                        node.color3_inputs.end();
      const bool has_fuzz_roughness_value = node.inputs.find("fuzz_roughness") != node.inputs.end();
      if (base_color == node.links.end() && base_weight == node.links.end() &&
          metalness == node.links.end() && roughness == node.links.end() &&
          ior == node.links.end() && opacity == node.links.end() &&
          emission_color == node.links.end() && emission_luminance == node.links.end() &&
          normal == node.links.end() && coat_normal == node.links.end() && !has_base_color_value &&
          !has_base_weight_value && !has_metalness_value && !has_roughness_value &&
          !has_ior_value && !has_opacity_value && !has_emission_color_value &&
          !has_emission_luminance_value && !has_coat_weight_value && !has_coat_color_value &&
          !has_coat_roughness_value && !has_coat_ior_value && !has_fuzz_weight_value &&
          !has_fuzz_color_value && !has_fuzz_roughness_value)
      {
        return false;
      }

      for (const auto link :
           {base_color == node.links.end() ? nullptr : &base_color->second,
            base_weight == node.links.end() ? nullptr : &base_weight->second,
            metalness == node.links.end() ? nullptr : &metalness->second,
            roughness == node.links.end() ? nullptr : &roughness->second,
            ior == node.links.end() ? nullptr : &ior->second,
            opacity == node.links.end() ? nullptr : &opacity->second,
            emission_color == node.links.end() ? nullptr : &emission_color->second,
            emission_luminance == node.links.end() ? nullptr : &emission_luminance->second,
            normal == node.links.end() ? nullptr : &normal->second,
            coat_normal == node.links.end() ? nullptr : &coat_normal->second,
            coat_weight == node.links.end() ? nullptr : &coat_weight->second,
            coat_color == node.links.end() ? nullptr : &coat_color->second,
            coat_roughness == node.links.end() ? nullptr : &coat_roughness->second,
            coat_ior == node.links.end() ? nullptr : &coat_ior->second,
            fuzz_weight == node.links.end() ? nullptr : &fuzz_weight->second,
            fuzz_color == node.links.end() ? nullptr : &fuzz_color->second,
            fuzz_roughness == node.links.end() ? nullptr : &fuzz_roughness->second})
      {
        if (link == nullptr) {
          continue;
        }
        if (!validate_link(*link, link->type, *nodes_by_name)) {
          return false;
        }
      }
      if ((base_color != node.links.end() && base_color->second.type != Type::Color3) ||
          (base_weight != node.links.end() && base_weight->second.type != Type::Float) ||
          (metalness != node.links.end() && metalness->second.type != Type::Float) ||
          (roughness != node.links.end() && roughness->second.type != Type::Float) ||
          (ior != node.links.end() && ior->second.type != Type::Float) ||
          (opacity != node.links.end() && opacity->second.type != Type::Float) ||
          (emission_color != node.links.end() && emission_color->second.type != Type::Color3) ||
          (emission_luminance != node.links.end() &&
           emission_luminance->second.type != Type::Float) ||
          (normal != node.links.end() && normal->second.type != Type::Vector3) ||
          (coat_normal != node.links.end() && coat_normal->second.type != Type::Vector3) ||
          (coat_weight != node.links.end() && coat_weight->second.type != Type::Float) ||
          (coat_color != node.links.end() && coat_color->second.type != Type::Color3) ||
          (coat_roughness != node.links.end() && coat_roughness->second.type != Type::Float) ||
          (coat_ior != node.links.end() && coat_ior->second.type != Type::Float) ||
          (fuzz_weight != node.links.end() && fuzz_weight->second.type != Type::Float) ||
          (fuzz_color != node.links.end() && fuzz_color->second.type != Type::Color3) ||
          (fuzz_roughness != node.links.end() && fuzz_roughness->second.type != Type::Float) ||
          (base_color != node.links.end() && has_base_color_value) ||
          (base_weight != node.links.end() && has_base_weight_value) ||
          (metalness != node.links.end() && has_metalness_value) ||
          (roughness != node.links.end() && has_roughness_value) ||
          (ior != node.links.end() && has_ior_value) ||
          (opacity != node.links.end() && has_opacity_value) ||
          (emission_color != node.links.end() && has_emission_color_value) ||
          (emission_luminance != node.links.end() && has_emission_luminance_value) ||
          (coat_weight != node.links.end() && has_coat_weight_value) ||
          (coat_color != node.links.end() && has_coat_color_value) ||
          (coat_roughness != node.links.end() && has_coat_roughness_value) ||
          (coat_ior != node.links.end() && has_coat_ior_value) ||
          (fuzz_weight != node.links.end() && has_fuzz_weight_value) ||
          (fuzz_color != node.links.end() && has_fuzz_color_value) ||
          (fuzz_roughness != node.links.end() && has_fuzz_roughness_value) ||
          node.links.size() != size_t(base_color != node.links.end()) +
                                   size_t(base_weight != node.links.end()) +
                                   size_t(metalness != node.links.end()) +
                                   size_t(roughness != node.links.end()) + size_t(ior != node.links.end()) +
                                   size_t(opacity != node.links.end()) +
                                   size_t(emission_color != node.links.end()) +
                                   size_t(emission_luminance != node.links.end()) +
                                   size_t(normal != node.links.end()) +
                                   size_t(coat_normal != node.links.end()) +
                                   size_t(coat_weight != node.links.end()) +
                                   size_t(coat_color != node.links.end()) +
                                   size_t(coat_roughness != node.links.end()) +
                                   size_t(coat_ior != node.links.end()) +
                                   size_t(fuzz_weight != node.links.end()) +
                                   size_t(fuzz_color != node.links.end()) +
                                   size_t(fuzz_roughness != node.links.end()) ||
          node.inputs.size() != size_t(has_base_weight_value) + size_t(has_metalness_value) +
                                    size_t(has_roughness_value) + size_t(has_ior_value) +
                                    size_t(has_opacity_value) + size_t(has_emission_luminance_value) +
                                    size_t(has_coat_weight_value) + size_t(has_coat_roughness_value) +
                                    size_t(has_coat_ior_value) + size_t(has_fuzz_weight_value) +
                                    size_t(has_fuzz_roughness_value) ||
          node.color3_inputs.size() != size_t(has_base_color_value) +
                                         size_t(has_emission_color_value) + size_t(has_coat_color_value) +
                                         size_t(has_fuzz_color_value) ||
          !node.int_inputs.empty() || !node.vector3_inputs.empty() || !node.string_inputs.empty() ||
          !node.asset_inputs.empty() || node.outputs.size() != 1)
      {
        return false;
      }
      continue;
    }

    return false;
  }

  unordered_map<string, VisitState> visit_states;
  for (const Node &node : source.nodes) {
    if (!validate_acyclic(node, *nodes_by_name, &visit_states)) {
      return false;
    }
  }
  if (source.has_displacement &&
      ((source.displacement.is_linked &&
        (source.displacement.link.type != Type::Float ||
         !validate_link(source.displacement.link, Type::Float, *nodes_by_name))) ||
       (source.displacement_scale.is_linked &&
        (source.displacement_scale.link.type != Type::Float ||
         !validate_link(source.displacement_scale.link, Type::Float, *nodes_by_name)))))
  {
    return false;
  }

  return true;
}

ShaderOutput *lowered_output(const Link &link,
                             const unordered_map<string, const Node *> &nodes_by_name,
                             const unordered_map<string, ShaderNode *> &lowered_nodes)
{
  const Node &source = *nodes_by_name.at(link.source_node);
  ShaderNode *lowered = lowered_nodes.at(link.source_node);
  if (source.nodedef == extract_color3_id) {
    static const char *channels[] = {"Red", "Green", "Blue"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (source.nodedef == extract_vector3_id) {
    static const char *channels[] = {"X", "Y", "Z"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (source.nodedef == extract_vector2_id) {
    static const char *channels[] = {"X", "Y"};
    return lowered->output(channels[source.int_inputs.at("index")]);
  }
  if (link.type == Type::Color3) {
    return lowered->output("Color");
  }
  if (link.type == Type::Float) {
    if (source.nodedef == clamp_float_id || is_smoothstep_float(source.nodedef) ||
        is_linear_range_float(source.nodedef))
    {
      return lowered->output("Result");
    }
    if (source.nodedef == convert_float_color3_id) {
      return lowered->output("Color");
    }
    return lowered->output("Value");
  }
  if (link.type == Type::Vector2) {
    if (source.nodedef == geompropvalue_vector2_id) {
      return lowered->output("UV");
    }
    return lowered->output("Vector");
  }
  if (link.type == Type::Vector3) {
    if (source.nodedef == normalmap_float_id) {
      return lowered->output("Normal");
    }
    if (source.nodedef == "ND_constant_vector3" || source.nodedef == mix_vector3_id ||
        vector_math_type(source.nodedef, nullptr)) {
      return lowered->output("Vector");
    }
    if (source.nodedef == image_vector3_id) {
      return lowered->output("Color");
    }
    if (source.nodedef == geompropvalue_vector3_id) {
      return lowered->output("Normal");
    }
  }
  return nullptr;
}

}  // namespace

bool lower(const Graph &source, ShaderGraph *graph)
{
  if (graph == nullptr) {
    return false;
  }

  unordered_map<string, const Node *> nodes_by_name;
  if (!validate(source, &nodes_by_name)) {
    return false;
  }

  unordered_map<string, ShaderNode *> lowered_nodes;
  for (const Node &node : source.nodes) {
    ShaderNode *lowered = nullptr;
    bool preserve_lowered_name = false;
    if (is_mix(node.nodedef)) {
      const Type value_type = mix_value_type(node.nodedef);
      if (value_type == Type::Float) {
        MathNode *delta = graph->create_node<MathNode>();
        delta->name = node.name + ".delta";
        delta->set_math_type(NODE_MATH_SUBTRACT);
        if (const auto foreground = node.inputs.find("fg"); foreground != node.inputs.end()) delta->set_value1(foreground->second);
        if (const auto background = node.inputs.find("bg"); background != node.inputs.end()) delta->set_value2(background->second);
        MathNode *product = graph->create_node<MathNode>();
        product->name = node.name + ".product";
        product->set_math_type(NODE_MATH_MULTIPLY);
        if (const auto factor = node.inputs.find("mix"); factor != node.inputs.end()) {
          product->set_value2(factor->second);
        }
        MathNode *sum = graph->create_node<MathNode>();
        sum->set_math_type(NODE_MATH_ADD);
        if (const auto background = node.inputs.find("bg"); background != node.inputs.end()) sum->set_value1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
      else if (value_type == Type::Color3) {
        MixNode *delta = graph->create_node<MixNode>();
        delta->name = node.name + ".delta";
        delta->set_mix_type(NODE_MIX_SUB);
        delta->set_fac(1.0f);
        if (const auto foreground = node.color3_inputs.find("fg"); foreground != node.color3_inputs.end()) delta->set_color1(foreground->second);
        if (const auto background = node.color3_inputs.find("bg"); background != node.color3_inputs.end()) delta->set_color2(background->second);
        CombineColorNode *factor = graph->create_node<CombineColorNode>();
        factor->name = node.name + ".factor";
        factor->set_color_type(NODE_COMBSEP_COLOR_RGB);
        if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
          factor->set_r(mix->second); factor->set_g(mix->second); factor->set_b(mix->second);
        }
        MixNode *product = graph->create_node<MixNode>();
        product->name = node.name + ".product";
        product->set_mix_type(NODE_MIX_MUL);
        product->set_fac(1.0f);
        MixNode *sum = graph->create_node<MixNode>();
        sum->set_mix_type(NODE_MIX_ADD);
        sum->set_fac(1.0f);
        if (const auto background = node.color3_inputs.find("bg"); background != node.color3_inputs.end()) sum->set_color1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(factor->name, factor);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
      else {
        VectorMathNode *delta = graph->create_node<VectorMathNode>();
        delta->name = node.name + ".delta";
        delta->set_math_type(NODE_VECTOR_MATH_SUBTRACT);
        if (const auto foreground = node.vector3_inputs.find("fg"); foreground != node.vector3_inputs.end()) delta->set_vector1(foreground->second);
        if (const auto background = node.vector3_inputs.find("bg"); background != node.vector3_inputs.end()) delta->set_vector2(background->second);
        CombineXYZNode *factor = graph->create_node<CombineXYZNode>();
        factor->name = node.name + ".factor";
        if (const auto mix = node.inputs.find("mix"); mix != node.inputs.end()) {
          factor->set_x(mix->second); factor->set_y(mix->second); factor->set_z(mix->second);
        }
        VectorMathNode *product = graph->create_node<VectorMathNode>();
        product->name = node.name + ".product";
        product->set_math_type(NODE_VECTOR_MATH_MULTIPLY);
        VectorMathNode *sum = graph->create_node<VectorMathNode>();
        sum->set_math_type(NODE_VECTOR_MATH_ADD);
        if (const auto background = node.vector3_inputs.find("bg"); background != node.vector3_inputs.end()) sum->set_vector1(background->second);
        lowered_nodes.emplace(delta->name, delta);
        lowered_nodes.emplace(factor->name, factor);
        lowered_nodes.emplace(product->name, product);
        lowered = sum;
      }
    }
    else if (NodeMathType math_type; scalar_math_type(node.nodedef, &math_type)) {
      MathNode *math = graph->create_node<MathNode>();
      math->set_math_type(math_type);
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      if (const auto input = node.inputs.find(
              is_unary ? "in" : (node.nodedef == invert_float_id ? "amount" : "in1"));
          input != node.inputs.end()) {
        math->set_value1(input->second);
      }
      if (!is_unary) {
        if (const auto input = node.inputs.find(node.nodedef == invert_float_id ? "in" : "in2");
            input != node.inputs.end())
        {
          math->set_value2(input->second);
        }
      }
      lowered = math;
    }
    else if (node.nodedef == convert_float_color3_id) {
      CombineColorNode *combine = graph->create_node<CombineColorNode>();
      combine->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = combine;
    }
    else if (is_smoothstep_float(node.nodedef)) {
      MapRangeNode *range = graph->create_node<MapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_SMOOTHSTEP);
      range->set_clamp(false);
      range->set_from_min(node.inputs.at("low"));
      range->set_from_max(node.inputs.at("high"));
      range->set_to_min(0.0f);
      range->set_to_max(1.0f);
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        range->set_value(input->second);
      }
      lowered = range;
    }
    else if (is_linear_range_float(node.nodedef)) {
      MapRangeNode *range = graph->create_node<MapRangeNode>();
      range->set_range_type(NODE_MAP_RANGE_LINEAR);
      range->set_clamp(node.nodedef == range_float_id && node.int_inputs.at("doclamp") != 0);
      range->set_from_min(node.inputs.at("inlow"));
      range->set_from_max(node.inputs.at("inhigh"));
      range->set_to_min(node.inputs.at("outlow"));
      range->set_to_max(node.inputs.at("outhigh"));
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        range->set_value(input->second);
      }
      lowered = range;
    }
    else if (node.nodedef == noise2d_float_id) {
      NoiseTextureNode *noise = graph->create_node<NoiseTextureNode>();
      noise->name = node.name + ".noise";
      noise->set_dimensions(2);
      MathNode *amplitude = graph->create_node<MathNode>();
      amplitude->name = node.name + ".amplitude";
      amplitude->set_math_type(NODE_MATH_MULTIPLY);
      amplitude->set_value2(node.inputs.at("amplitude"));
      MathNode *pivot = graph->create_node<MathNode>();
      pivot->set_math_type(NODE_MATH_ADD);
      pivot->set_value2(node.inputs.at("pivot"));
      lowered_nodes.emplace(noise->name, noise);
      lowered_nodes.emplace(amplitude->name, amplitude);
      lowered = pivot;
    }
    else if (is_luminance_color3(node.nodedef)) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->name = node.name + ".separate";
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->name = node.name + ".vector";
      VectorMathNode *dot = graph->create_node<VectorMathNode>();
      dot->set_math_type(NODE_VECTOR_MATH_DOT_PRODUCT);
      dot->set_vector2(node.color3_inputs.at("lumacoeffs"));
      lowered_nodes.emplace(separate->name, separate);
      lowered_nodes.emplace(combine->name, combine);
      lowered = dot;
    }
    else if (node.nodedef == clamp_float_id) {
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      if (const auto input = node.inputs.find("in"); input != node.inputs.end()) {
        clamp->set_value(input->second);
      }
      if (const auto input = node.inputs.find("low"); input != node.inputs.end()) {
        clamp->set_min(input->second);
      }
      if (const auto input = node.inputs.find("high"); input != node.inputs.end()) {
        clamp->set_max(input->second);
      }
      lowered = clamp;
    }
    else if (node.nodedef == constant_float_id) {
      ValueNode *value = graph->create_node<ValueNode>();
      value->set_value(node.inputs.at("value"));
      lowered = value;
    }
    else if (node.nodedef == constant_color3_id) {
      ColorNode *color = graph->create_node<ColorNode>();
      color->set_value(node.color3_inputs.at("value"));
      lowered = color;
    }
    else if (NodeMix mix_type; color_mix_type(node.nodedef, &mix_type)) {
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(mix_type);
      mix->set_fac(1.0f);
      lowered = mix;
    }
    else if (node.nodedef == extract_color3_id) {
      SeparateColorNode *separate = graph->create_node<SeparateColorNode>();
      separate->set_color_type(NODE_COMBSEP_COLOR_RGB);
      lowered = separate;
    }
    else if (node.nodedef == extract_vector3_id) {
      lowered = graph->create_node<SeparateXYZNode>();
    }
    else if (node.nodedef == extract_vector2_id) {
      lowered = graph->create_node<SeparateXYZNode>();
    }
    else if (node.nodedef == geompropvalue_vector2_id) {
      UVMapNode *uv_map = graph->create_node<UVMapNode>();
      uv_map->set_attribute(ustring(node.string_inputs.at("geomprop")));
      lowered = uv_map;
    }
    else if (node.nodedef == geompropvalue_vector3_id) {
      lowered = graph->create_node<GeometryNode>();
    }
    else if (node.nodedef == image_color3_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      lowered = image;
    }
    else if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id) {
      MixNode *mix = graph->create_node<MixNode>();
      mix->set_mix_type(NODE_MIX_BLEND);
      const bool top_to_bottom = node.nodedef == ramptb_color3_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      mix->set_color1(node.color3_inputs.at(first_name));
      mix->set_color2(node.color3_inputs.at(second_name));
      SeparateXYZNode *coordinate = graph->create_node<SeparateXYZNode>();
      coordinate->name = node.name + ".coordinate";
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->name = node.name + ".factor";
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      clamp->set_min(0.0f);
      clamp->set_max(1.0f);
      lowered_nodes.emplace(coordinate->name, coordinate);
      lowered_nodes.emplace(clamp->name, clamp);
      lowered = mix;
    }
    else if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      const char *first_name = top_to_bottom ? "valuet" : "valuel";
      const char *second_name = top_to_bottom ? "valueb" : "valuer";
      SeparateXYZNode *coordinate = graph->create_node<SeparateXYZNode>();
      coordinate->name = node.name + ".coordinate";
      ClampNode *clamp = graph->create_node<ClampNode>();
      clamp->name = node.name + ".factor";
      clamp->set_clamp_type(NODE_CLAMP_MINMAX);
      clamp->set_min(0.0f);
      clamp->set_max(1.0f);
      MathNode *delta = graph->create_node<MathNode>();
      delta->name = node.name + ".delta";
      delta->set_math_type(NODE_MATH_SUBTRACT);
      delta->set_value1(node.inputs.at(second_name));
      delta->set_value2(node.inputs.at(first_name));
      MathNode *product = graph->create_node<MathNode>();
      product->name = node.name + ".product";
      product->set_math_type(NODE_MATH_MULTIPLY);
      MathNode *sum = graph->create_node<MathNode>();
      sum->set_math_type(NODE_MATH_ADD);
      sum->set_value1(node.inputs.at(first_name));
      lowered_nodes.emplace(coordinate->name, coordinate);
      lowered_nodes.emplace(clamp->name, clamp);
      lowered_nodes.emplace(delta->name, delta);
      lowered_nodes.emplace(product->name, product);
      lowered = sum;
    }
    else if (node.nodedef == image_vector3_id) {
      ImageTextureNode *image = graph->create_node<ImageTextureNode>();
      image->set_filename(ustring(node.asset_inputs.at("file")));
      image->set_colorspace(u_colorspace_data);
      lowered = image;
    }
    else if (node.nodedef == normalmap_float_id) {
      NormalMapNode *normalmap = graph->create_node<NormalMapNode>();
      normalmap->set_space(NODE_NORMAL_MAP_TANGENT);
      normalmap->set_convention(NODE_NORMAL_MAP_CONVENTION_OPENGL);
      normalmap->set_base(NODE_NORMAL_MAP_BASE_DISPLACED);
      normalmap->set_strength(1.0f);
      if (const auto input = node.vector3_inputs.find("in"); input != node.vector3_inputs.end()) {
        normalmap->set_color(input->second);
      }
      lowered = normalmap;
    }
    else if (node.nodedef == "ND_constant_vector3") {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      combine->set_x(node.vector3_inputs.at("value").x);
      combine->set_y(node.vector3_inputs.at("value").y);
      combine->set_z(node.vector3_inputs.at("value").z);
      lowered = combine;
    }
    else if (node.nodedef == constant_vector2_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      const float2 value = node.vector2_inputs.at("value");
      combine->set_x(value.x);
      combine->set_y(value.y);
      combine->set_z(0.0f);
      lowered = combine;
    }
    else if (node.nodedef == combine3_vector3_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) {
        combine->set_x(input->second);
      }
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
        combine->set_y(input->second);
      }
      if (const auto input = node.inputs.find("in3"); input != node.inputs.end()) {
        combine->set_z(input->second);
      }
      lowered = combine;
    }
    else if (node.nodedef == combine2_vector2_id) {
      CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
      if (const auto input = node.inputs.find("in1"); input != node.inputs.end()) combine->set_x(input->second);
      if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) combine->set_y(input->second);
      combine->set_z(0.0f);
      lowered = combine;
    }
    else {
      NodeVectorMathType vector_type;
      if (vector_math_type(node.nodedef, &vector_type)) {
        VectorMathNode *math = graph->create_node<VectorMathNode>();
        math->set_math_type(vector_type);
        if (const auto input = node.vector3_inputs.find(
                vector_math_is_unary(node.nodedef) ? "in" : "in1");
            input != node.vector3_inputs.end())
        {
          math->set_vector1(input->second);
        }
        if (const auto input = node.vector3_inputs.find("in2");
            input != node.vector3_inputs.end())
        {
          math->set_vector2(input->second);
        }
        if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
          if (vector_math_uses_scalar_broadcast_second(node.nodedef)) {
            math->set_vector2(make_float3(input->second, input->second, input->second));
          }
          else {
            math->set_scale(input->second);
          }
        }
        if (vector_math_uses_scalar_broadcast_second(node.nodedef) &&
            node.links.find("in2") != node.links.end())
        {
          CombineXYZNode *broadcast = graph->create_node<CombineXYZNode>();
          broadcast->name = node.name + ".broadcast";
          lowered_nodes.emplace(broadcast->name, broadcast);
        }
        lowered = math;
      }
      else if (vector2_math_type(node.nodedef, &vector_type)) {
        VectorMathNode *math = graph->create_node<VectorMathNode>();
        math->set_math_type(vector_type);
        const char *first_name = vector2_math_is_unary(node.nodedef) ? "in" : "in1";
        if (const auto input = node.vector2_inputs.find(first_name); input != node.vector2_inputs.end()) {
          math->set_vector1(make_float3(input->second.x, input->second.y, 0.0f));
        }
        if (const auto input = node.vector2_inputs.find("in2"); input != node.vector2_inputs.end()) {
          math->set_vector2(make_float3(input->second.x, input->second.y, 0.0f));
        }
        if (const auto input = node.inputs.find("in2"); input != node.inputs.end()) {
          if (vector2_math_uses_scalar_broadcast_second(node.nodedef)) {
            math->set_vector2(make_float3(input->second, input->second, 0.0f));
          }
          else {
            math->set_scale(input->second);
          }
        }
        if (vector2_math_uses_scalar_broadcast_second(node.nodedef) &&
            node.links.find("in2") != node.links.end())
        {
          CombineXYZNode *broadcast = graph->create_node<CombineXYZNode>();
          broadcast->name = node.name + ".broadcast";
          broadcast->set_z(0.0f);
          lowered_nodes.emplace(broadcast->name, broadcast);
        }
        if (vector2_math_returns_float(node.nodedef)) {
          lowered = math;
        }
        else {
          math->name = node.name;
          SeparateXYZNode *separate = graph->create_node<SeparateXYZNode>();
          separate->name = node.name + ".vector2.separate";
          CombineXYZNode *combine = graph->create_node<CombineXYZNode>();
          combine->name = node.name + ".vector2";
          combine->set_z(0.0f);
          lowered_nodes.emplace(node.name + ".vector2.math", math);
          lowered_nodes.emplace(separate->name, separate);
          lowered = combine;
          preserve_lowered_name = true;
        }
      }
      else {
        PrincipledBsdfNode *principled = graph->create_node<PrincipledBsdfNode>();
      if (const auto input = node.color3_inputs.find("base_color");
          input != node.color3_inputs.end())
      {
        principled->set_base_color(input->second);
      }
      if (const auto input = node.inputs.find("base_weight"); input != node.inputs.end()) {
        /* ShaderGraph::finalize() adds its implicit unit closure weight. Store the delta so
         * the finalized closure weight equals the MaterialX base_weight. */
        principled->set_surface_mix_weight(input->second - 1.0f);
      }
      if (const auto input = node.inputs.find("base_metalness"); input != node.inputs.end()) {
        principled->set_metallic(input->second);
      }
      if (const auto input = node.inputs.find("specular_roughness"); input != node.inputs.end()) {
        principled->set_roughness(input->second);
      }
      if (const auto input = node.inputs.find("specular_ior"); input != node.inputs.end()) {
        principled->set_ior(input->second);
      }
      if (const auto input = node.inputs.find("geometry_opacity"); input != node.inputs.end()) {
        principled->set_alpha(input->second);
      }
      if (const auto input = node.color3_inputs.find("emission_color");
          input != node.color3_inputs.end())
      {
        principled->set_emission_color(input->second);
      }
      if (const auto input = node.inputs.find("emission_luminance"); input != node.inputs.end()) {
        principled->set_emission_strength(input->second);
      }
      if (const auto input = node.inputs.find("coat_weight"); input != node.inputs.end()) {
        principled->set_coat_weight(input->second);
      }
      if (const auto input = node.color3_inputs.find("coat_color");
          input != node.color3_inputs.end())
      {
        principled->set_coat_tint(input->second);
      }
      if (const auto input = node.inputs.find("coat_roughness"); input != node.inputs.end()) {
        principled->set_coat_roughness(input->second);
      }
      if (const auto input = node.inputs.find("coat_ior"); input != node.inputs.end()) {
        principled->set_coat_ior(input->second);
      }
      if (const auto input = node.inputs.find("fuzz_weight"); input != node.inputs.end()) {
        principled->set_sheen_weight(input->second);
      }
      if (const auto input = node.color3_inputs.find("fuzz_color");
          input != node.color3_inputs.end())
      {
        principled->set_sheen_tint(input->second);
      }
      if (const auto input = node.inputs.find("fuzz_roughness"); input != node.inputs.end()) {
        principled->set_sheen_roughness(input->second);
      }
      lowered = principled;
      }
    }
    if (!preserve_lowered_name) {
      lowered->name = node.name;
    }
    lowered_nodes.emplace(node.name, lowered);
  }

  for (const Node &node : source.nodes) {
    if (is_mix(node.nodedef)) {
      const Type value_type = mix_value_type(node.nodedef);
      ShaderNode *delta = lowered_nodes.at(node.name + ".delta");
      ShaderNode *factor = value_type == Type::Float ? nullptr : lowered_nodes.at(node.name + ".factor");
      ShaderNode *product = lowered_nodes.at(node.name + ".product");
      ShaderNode *sum = lowered_nodes.at(node.name);
      const auto connect_if_linked = [&](const char *input, ShaderInput *socket) {
        if (const auto link = node.links.find(input); link != node.links.end()) {
          graph->connect(lowered_output(link->second, nodes_by_name, lowered_nodes), socket);
        }
      };
      if (value_type == Type::Float) {
        connect_if_linked("fg", delta->input("Value1"));
        connect_if_linked("bg", delta->input("Value2"));
        graph->connect(delta->output("Value"), product->input("Value1"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) graph->connect(lowered_output(mix->second, nodes_by_name, lowered_nodes), product->input("Value2"));
        connect_if_linked("bg", sum->input("Value1"));
        graph->connect(product->output("Value"), sum->input("Value2"));
      }
      else if (value_type == Type::Color3) {
        connect_if_linked("fg", delta->input("Color1")); connect_if_linked("bg", delta->input("Color2"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) {
          ShaderOutput *mix_output = lowered_output(mix->second, nodes_by_name, lowered_nodes);
          graph->connect(mix_output, factor->input("Red")); graph->connect(mix_output, factor->input("Green")); graph->connect(mix_output, factor->input("Blue"));
        }
        graph->connect(delta->output("Color"), product->input("Color1")); graph->connect(factor->output("Color"), product->input("Color2"));
        connect_if_linked("bg", sum->input("Color1")); graph->connect(product->output("Color"), sum->input("Color2"));
      }
      else {
        connect_if_linked("fg", delta->input("Vector1")); connect_if_linked("bg", delta->input("Vector2"));
        if (const auto mix = node.links.find("mix"); mix != node.links.end()) {
          ShaderOutput *mix_output = lowered_output(mix->second, nodes_by_name, lowered_nodes);
          graph->connect(mix_output, factor->input("X")); graph->connect(mix_output, factor->input("Y")); graph->connect(mix_output, factor->input("Z"));
        }
        graph->connect(delta->output("Vector"), product->input("Vector1")); graph->connect(factor->output("Vector"), product->input("Vector2"));
        connect_if_linked("bg", sum->input("Vector1")); graph->connect(product->output("Vector"), sum->input("Vector2"));
      }
      continue;
    }
    NodeMathType unused_math_type;
    if (scalar_math_type(node.nodedef, &unused_math_type)) {
      ShaderNode *math = lowered_nodes.at(node.name);
      const bool is_unary = scalar_math_is_unary(node.nodedef);
      if (const auto input = node.links.find(
              is_unary ? "in" : (node.nodedef == invert_float_id ? "amount" : "in1"));
          input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       math->input("Value1"));
      }
      if (!is_unary) {
        if (const auto input = node.links.find(node.nodedef == invert_float_id ? "in" : "in2");
            input != node.links.end())
        {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         math->input("Value2"));
        }
      }
      continue;
    }

    if (node.nodedef == convert_float_color3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      ShaderOutput *input = lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes);
      graph->connect(input, combine->input("Red"));
      graph->connect(input, combine->input("Green"));
      graph->connect(input, combine->input("Blue"));
      continue;
    }

    if (node.nodedef == clamp_float_id) {
      ShaderNode *clamp = lowered_nodes.at(node.name);
      for (const auto &[input_name, socket] :
           {std::pair{"in", "Value"}, std::pair{"low", "Min"}, std::pair{"high", "Max"}})
      {
        if (const auto input = node.links.find(input_name); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         clamp->input(socket));
        }
      }
      continue;
    }

    if (is_smoothstep_float(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Value"));
      }
      continue;
    }

    if (is_linear_range_float(node.nodedef)) {
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       lowered_nodes.at(node.name)->input("Value"));
      }
      continue;
    }

    if (node.nodedef == noise2d_float_id) {
      ShaderNode *noise = lowered_nodes.at(node.name + ".noise");
      ShaderNode *amplitude = lowered_nodes.at(node.name + ".amplitude");
      ShaderNode *pivot = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     noise->input("Vector"));
      graph->connect(noise->output("Fac"), amplitude->input("Value1"));
      graph->connect(amplitude->output("Value"), pivot->input("Value1"));
      continue;
    }

    if (is_luminance_color3(node.nodedef)) {
      ShaderNode *separate = lowered_nodes.at(node.name + ".separate");
      ShaderNode *combine = lowered_nodes.at(node.name + ".vector");
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      graph->connect(separate->output("Red"), combine->input("X"));
      graph->connect(separate->output("Green"), combine->input("Y"));
      graph->connect(separate->output("Blue"), combine->input("Z"));
      graph->connect(combine->output("Vector"), lowered_nodes.at(node.name)->input("Vector1"));
      continue;
    }

    NodeMix unused_mix_type;
    if (color_mix_type(node.nodedef, &unused_mix_type)) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      const bool is_invert = node.nodedef == invert_color3_id;
      graph->connect(lowered_output(node.links.at(is_invert ? "amount" : "in1"), nodes_by_name, lowered_nodes),
                     mix->input("Color1"));
      graph->connect(lowered_output(node.links.at(is_invert ? "in" : "in2"), nodes_by_name, lowered_nodes),
                     mix->input("Color2"));
      continue;
    }

    if (node.nodedef == extract_color3_id) {
      ShaderNode *separate = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Color"));
      continue;
    }

    if (node.nodedef == extract_vector3_id || node.nodedef == extract_vector2_id) {
      ShaderNode *separate = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("in"), nodes_by_name, lowered_nodes),
                     separate->input("Vector"));
      continue;
    }

    if (node.nodedef == combine3_vector3_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[component, socket] :
           {std::pair{"in1", "X"}, std::pair{"in2", "Y"}, std::pair{"in3", "Z"}})
      {
        if (const auto input = node.links.find(component); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                         combine->input(socket));
        }
      }
      continue;
    }

    if (node.nodedef == combine2_vector2_id) {
      ShaderNode *combine = lowered_nodes.at(node.name);
      for (const auto &[component, socket] : {std::pair{"in1", "X"}, std::pair{"in2", "Y"}}) {
        if (const auto input = node.links.find(component); input != node.links.end()) {
          graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), combine->input(socket));
        }
      }
      continue;
    }

    if (node.nodedef == image_color3_id || node.nodedef == image_vector3_id) {
      ShaderNode *image_node = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     image_node->input("Vector"));
      continue;
    }

    if (node.nodedef == ramplr_color3_id || node.nodedef == ramptb_color3_id) {
      ShaderNode *mix = lowered_nodes.at(node.name);
      ShaderNode *coordinate = lowered_nodes.at(node.name + ".coordinate");
      ShaderNode *clamp = lowered_nodes.at(node.name + ".factor");
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     coordinate->input("Vector"));
      graph->connect(coordinate->output(node.nodedef == ramptb_color3_id ? "Y" : "X"),
                     clamp->input("Value"));
      graph->connect(clamp->output("Result"), mix->input("Fac"));
      continue;
    }
    if (is_scalar_ramp(node.nodedef)) {
      const bool top_to_bottom = node.nodedef == ramptb_float_id;
      ShaderNode *coordinate = lowered_nodes.at(node.name + ".coordinate");
      ShaderNode *clamp = lowered_nodes.at(node.name + ".factor");
      ShaderNode *delta = lowered_nodes.at(node.name + ".delta");
      ShaderNode *product = lowered_nodes.at(node.name + ".product");
      ShaderNode *sum = lowered_nodes.at(node.name);
      graph->connect(lowered_output(node.links.at("texcoord"), nodes_by_name, lowered_nodes),
                     coordinate->input("Vector"));
      graph->connect(coordinate->output(top_to_bottom ? "Y" : "X"), clamp->input("Value"));
      graph->connect(clamp->output("Result"), product->input("Value2"));
      graph->connect(delta->output("Value"), product->input("Value1"));
      graph->connect(product->output("Value"), sum->input("Value2"));
      continue;
    }

    if (node.nodedef == normalmap_float_id) {
      ShaderNode *normalmap = lowered_nodes.at(node.name);
      if (const auto input = node.links.find("in"); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes),
                       normalmap->input("Color"));
      }
      continue;
    }
    NodeVectorMathType vector_type;
    if (vector_math_type(node.nodedef, &vector_type)) {
      ShaderNode *math = lowered_nodes.at(node.name);
      const char *first = vector_math_is_unary(node.nodedef) ? "in" : "in1";
      if (const auto input = node.links.find(first); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Vector1"));
      }
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        ShaderOutput *output = lowered_output(input->second, nodes_by_name, lowered_nodes);
        if (vector_math_uses_scalar_broadcast_second(node.nodedef)) {
          ShaderNode *broadcast = lowered_nodes.at(node.name + ".broadcast");
          graph->connect(output, broadcast->input("X"));
          graph->connect(output, broadcast->input("Y"));
          graph->connect(output, broadcast->input("Z"));
          graph->connect(broadcast->output("Vector"), math->input("Vector2"));
        }
        else {
          graph->connect(output, math->input(vector_math_uses_scalar_second(node.nodedef) ? "Scale" : "Vector2"));
        }
      }
      continue;
    }
    if (vector2_math_type(node.nodedef, &vector_type)) {
      const bool returns_float = vector2_math_returns_float(node.nodedef);
      ShaderNode *math = returns_float ? lowered_nodes.at(node.name) :
                                        lowered_nodes.at(node.name + ".vector2.math");
      const char *first_name = vector2_math_is_unary(node.nodedef) ? "in" : "in1";
      if (const auto input = node.links.find(first_name); input != node.links.end()) {
        graph->connect(lowered_output(input->second, nodes_by_name, lowered_nodes), math->input("Vector1"));
      }
      if (const auto input = node.links.find("in2"); input != node.links.end()) {
        ShaderOutput *output = lowered_output(input->second, nodes_by_name, lowered_nodes);
        if (vector2_math_uses_scalar_broadcast_second(node.nodedef)) {
          ShaderNode *broadcast = lowered_nodes.at(node.name + ".broadcast");
          graph->connect(output, broadcast->input("X"));
          graph->connect(output, broadcast->input("Y"));
          graph->connect(broadcast->output("Vector"), math->input("Vector2"));
        }
        else {
          graph->connect(output,
                         math->input(vector2_math_uses_scalar_second(node.nodedef) ? "Scale" : "Vector2"));
        }
      }
      if (!returns_float) {
        ShaderNode *separate = lowered_nodes.at(node.name + ".vector2.separate");
        ShaderNode *combine = lowered_nodes.at(node.name);
        graph->connect(math->output("Vector"), separate->input("Vector"));
        graph->connect(separate->output("X"), combine->input("X"));
        graph->connect(separate->output("Y"), combine->input("Y"));
      }
      continue;
    }

    if (node.nodedef != open_pbr_surface_id) {
      continue;
    }

    ShaderNode *surface_node = lowered_nodes.at(node.name);
    if (const auto base_color = node.links.find("base_color"); base_color != node.links.end()) {
      graph->connect(lowered_output(base_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Base Color"));
    }
    if (const auto base_weight = node.links.find("base_weight"); base_weight != node.links.end()) {
      MathNode *weight_delta = graph->create_node<MathNode>();
      weight_delta->name = node.name + ".base_weight_delta";
      weight_delta->set_math_type(NODE_MATH_SUBTRACT);
      weight_delta->set_value2(1.0f);
      graph->connect(lowered_output(base_weight->second, nodes_by_name, lowered_nodes),
                     weight_delta->input("Value1"));
      graph->connect(weight_delta->output("Value"), surface_node->input("SurfaceMixWeight"));
    }
    if (const auto metalness = node.links.find("base_metalness"); metalness != node.links.end()) {
      graph->connect(lowered_output(metalness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Metallic"));
    }
    if (const auto roughness = node.links.find("specular_roughness");
        roughness != node.links.end())
    {
      graph->connect(lowered_output(roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Roughness"));
    }
    if (const auto ior = node.links.find("specular_ior"); ior != node.links.end()) {
      graph->connect(lowered_output(ior->second, nodes_by_name, lowered_nodes),
                     surface_node->input("IOR"));
    }
    if (const auto opacity = node.links.find("geometry_opacity"); opacity != node.links.end()) {
      graph->connect(lowered_output(opacity->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Alpha"));
    }
    if (const auto emission_color = node.links.find("emission_color");
        emission_color != node.links.end())
    {
      graph->connect(lowered_output(emission_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Emission Color"));
    }
    if (const auto emission_luminance = node.links.find("emission_luminance");
        emission_luminance != node.links.end())
    {
      graph->connect(lowered_output(emission_luminance->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Emission Strength"));
    }
    if (const auto coat_weight = node.links.find("coat_weight"); coat_weight != node.links.end()) {
      graph->connect(lowered_output(coat_weight->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Weight"));
    }
    if (const auto coat_color = node.links.find("coat_color"); coat_color != node.links.end()) {
      graph->connect(lowered_output(coat_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Tint"));
    }
    if (const auto coat_roughness = node.links.find("coat_roughness");
        coat_roughness != node.links.end())
    {
      graph->connect(lowered_output(coat_roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Roughness"));
    }
    if (const auto coat_ior = node.links.find("coat_ior"); coat_ior != node.links.end()) {
      graph->connect(lowered_output(coat_ior->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat IOR"));
    }
    if (const auto fuzz_weight = node.links.find("fuzz_weight"); fuzz_weight != node.links.end()) {
      graph->connect(lowered_output(fuzz_weight->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Weight"));
    }
    if (const auto fuzz_color = node.links.find("fuzz_color"); fuzz_color != node.links.end()) {
      graph->connect(lowered_output(fuzz_color->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Tint"));
    }
    if (const auto fuzz_roughness = node.links.find("fuzz_roughness");
        fuzz_roughness != node.links.end())
    {
      graph->connect(lowered_output(fuzz_roughness->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Sheen Roughness"));
    }
    if (const auto normal = node.links.find("geometry_normal"); normal != node.links.end()) {
      graph->connect(lowered_output(normal->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Normal"));
    }
    if (const auto coat_normal = node.links.find("geometry_coat_normal");
        coat_normal != node.links.end())
    {
      graph->connect(lowered_output(coat_normal->second, nodes_by_name, lowered_nodes),
                     surface_node->input("Coat Normal"));
    }
    graph->connect(surface_node->output("BSDF"), graph->output()->input("Surface"));
  }

  if (source.has_displacement) {
    DisplacementNode *displacement = graph->create_node<DisplacementNode>();
    displacement->name = "Displacement";
    displacement->set_midlevel(0.0f);
    if (source.displacement.is_linked) {
      graph->connect(lowered_output(source.displacement.link, nodes_by_name, lowered_nodes),
                     displacement->input("Height"));
    }
    else {
      displacement->set_height(source.displacement.value);
    }
    if (source.displacement_scale.is_linked) {
      graph->connect(lowered_output(source.displacement_scale.link, nodes_by_name, lowered_nodes),
                     displacement->input("Scale"));
    }
    else {
      displacement->set_scale(source.displacement_scale.value);
    }
    graph->connect(displacement->output("Displacement"), graph->output()->input("Displacement"));
  }

  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
