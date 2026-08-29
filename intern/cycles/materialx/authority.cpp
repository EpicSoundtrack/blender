/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "materialx/authority.h"

#include "util/sha256.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

namespace {

bool is_lower_hex(const char value)
{
  return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
}

bool is_document_uuid(const string &value)
{
  if (value.size() != 36) {
    return false;
  }

  for (size_t index = 0; index < value.size(); index++) {
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      if (value[index] != '-') {
        return false;
      }
    }
    else if (!is_lower_hex(value[index])) {
      return false;
    }
  }
  return true;
}

bool is_digest(const string &value)
{
  constexpr size_t prefix_size = 7;
  if (value.size() != prefix_size + 64 || value.compare(0, prefix_size, "sha256:") != 0) {
    return false;
  }

  for (size_t index = prefix_size; index < value.size(); index++) {
    if (!is_lower_hex(value[index])) {
      return false;
    }
  }
  return true;
}

}  // namespace

string usda_sha256_digest(const string &usda)
{
  return "sha256:" + util_sha256_string(usda);
}

bool is_valid(const Authority &authority)
{
  if (!is_document_uuid(authority.document_uuid) || !is_digest(authority.digest)) {
    return false;
  }
  if (authority.digest != usda_sha256_digest(authority.usda)) {
    return false;
  }
  if (authority.usda_text_name != string(usda_text_prefix) + authority.document_uuid) {
    return false;
  }
  if (authority.material_path.empty() || authority.material_path[0] != '/') {
    return false;
  }
  if (!authority.usda.starts_with("#usda ")) {
    return false;
  }
  return is_valid_manifest(authority.selected_outputs);
}

bool is_valid_manifest(const vector<SelectedOutput> &selected_outputs)
{
  if (selected_outputs.empty()) {
    return true;
  }
  for (const SelectedOutput &selected : selected_outputs) {
    if (selected.node_path.empty() || selected.node_path[0] != '/') {
      return false;
    }
    if (selected.nodedef.empty() || selected.output_name.empty()) {
      return false;
    }
    switch (selected.type) {
      case Type::Float:
      case Type::Color3:
      case Type::Vector2:
      case Type::Vector3:
      /* Task 4: four-component observation -- Color4/Vector4 are now a
       * structurally admissible manifest selection type. */
      case Type::Color4:
      case Type::Vector4:
      /* Task 5: boolean/integer exact-domain observation. */
      case Type::Boolean:
      case Type::Integer:
      /* Task 6: matrix boundary (affine-only Matrix44, full Matrix33). */
      case Type::Matrix33:
      case Type::Matrix44:
        break;
      default:
        /* Phase 1/4/5/6 supports only float/color3/vector2/vector3/
         * color4/vector4/boolean/integer/matrix33/matrix44; SurfaceShader
         * and any future type remain an explicit boundary, not a silent
         * widening of the device ABI. */
        return false;
    }
  }
  return true;
}

}  // namespace materialx

CCL_NAMESPACE_END
