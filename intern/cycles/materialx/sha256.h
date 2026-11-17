/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"

CCL_NAMESPACE_BEGIN

/** Lowercase hexadecimal SHA-256 digest of the exact string bytes. Used only by the
 * MaterialX authority digest (materialx/authority*.cpp). */
string materialx_sha256_string(const string &str);

CCL_NAMESPACE_END
