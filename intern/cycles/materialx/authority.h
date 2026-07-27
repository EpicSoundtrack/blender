/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"

CCL_NAMESPACE_BEGIN

namespace materialx {

inline constexpr char authority_enabled_property[] = "materialx_authoring.cycles_native_authority";
inline constexpr char document_uuid_property[] = "materialx_authoring.document_uuid";
inline constexpr char document_digest_property[] = "materialx_authoring.document_digest";
inline constexpr char document_usda_text_property[] =
    "materialx_authoring.document_usda_text_name";
inline constexpr char material_path_property[] = "materialx_authoring.document_material_path";
inline constexpr char usda_text_prefix[] = ".materialx_usdshade_";

struct Authority {
  string document_uuid;
  string digest;
  string usda_text_name;
  string material_path;
  string usda;
};

/** Compute the canonical digest property for the exact USDA Text bytes. */
string usda_sha256_digest(const string &usda);

/**
 * Validate the renderer-neutral ownership contract passed from Blender.
 *
 * The digest binds the authority metadata to the exact USDA Text bytes. The
 * USD parser remains responsible for validating the actual stage and graph.
 */
bool is_valid(const Authority &authority);

}  // namespace materialx

CCL_NAMESPACE_END
