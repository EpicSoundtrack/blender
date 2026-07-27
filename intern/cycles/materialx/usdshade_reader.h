/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <pxr/usd/usdShade/material.h>

#include "materialx/graph.h"
#include "util/string.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

/**
 * Read the supported portion of an authoritative USDShade MaterialX material
 * into the renderer-independent graph IR.
 *
 * The destination is replaced only after the complete source has validated.
 */
bool read_usdshade_graph(const pxr::UsdShadeMaterial &material,
                         Graph *graph,
                         string *error_message = nullptr);

}  // namespace materialx

CCL_NAMESPACE_END
