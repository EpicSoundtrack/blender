/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/string.h"

CCL_NAMESPACE_BEGIN

/** Return the lowercase hexadecimal SHA-256 digest of the exact string bytes. */
string util_sha256_string(const string &str);

CCL_NAMESPACE_END
