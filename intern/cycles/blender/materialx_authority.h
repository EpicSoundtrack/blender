/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "materialx/authority.h"

namespace blender {
struct Main;
struct Material;
}  // namespace blender

CCL_NAMESPACE_BEGIN

struct BlenderMaterialXAuthority {
  bool selected = false;
  materialx::Authority source;
  string error;

  bool is_valid() const
  {
    return selected && error.empty();
  }
};

/**
 * Read the explicit Material ID-property authority contract and its referenced
 * in-memory USDA Text datablock.
 */
BlenderMaterialXAuthority find_blender_materialx_authority(const blender::Material &material,
                                                           blender::Main &b_data);

CCL_NAMESPACE_END
