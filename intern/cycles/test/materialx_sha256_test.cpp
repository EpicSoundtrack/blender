/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <gtest/gtest.h>

#include "materialx/sha256.h"

CCL_NAMESPACE_BEGIN

TEST(materialx_sha256, digest_matches_fips_180_vectors)
{
  EXPECT_EQ(materialx_sha256_string(""),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  EXPECT_EQ(materialx_sha256_string("abc"),
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  EXPECT_EQ(materialx_sha256_string("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
  EXPECT_EQ(materialx_sha256_string(string(1, '\0')),
            "6e340b9cffb37a989ca544e6bb780a2c78901d3fb33738768511a30617afa01d");
}

CCL_NAMESPACE_END
