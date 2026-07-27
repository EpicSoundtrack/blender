/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "materialx/authority.h"

CCL_NAMESPACE_BEGIN

class ShaderGraph;

namespace materialx {

/**
 * Validate and lower one canonical in-memory USDShade authority source through
 * the shared MaterialX graph IR into a transient Cycles graph.
 */
bool lower_usdshade_authority(const Authority &authority,
                              ShaderGraph *graph,
                              string *error_message = nullptr);

}  // namespace materialx

CCL_NAMESPACE_END
