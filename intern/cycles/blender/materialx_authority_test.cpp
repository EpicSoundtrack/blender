/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "testing/testing.h"
#include "blender/materialx_authority.h"
#include "BKE_gtest_base.hh"
#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_text.h"
#include "BLI_listbase.hh"
#include "DNA_material_types.h"
#include "DNA_text_types.h"

CCL_NAMESPACE_BEGIN
namespace {
using namespace blender;

class MaterialXTextDependency : public bke::BlenderGTestBase {
 protected:
  Main *main = nullptr;
  Main *preview_main = nullptr;
  Material *material = nullptr;
  Text *text = nullptr;
  const string document_uuid = "00000000-0000-0000-0000-000000000007";
  const string usda = "#usda 1.0\ndef Material \"Material\" {}\n";

  void SetUp() override
  {
    main = BKE_main_new();
    preview_main = BKE_main_new();
    material = static_cast<Material *>(BKE_id_new(main, ID_MA, "Authority"));
    text = BKE_text_add(main, (string(materialx::usda_text_prefix) + document_uuid).c_str());
    txt_insert_buf(text, usda.c_str(), int(usda.size()));
    material->id.properties = IDP_New(IDP_GROUP, nullptr, "properties");
    IDPropertyTemplate enabled = {};
    enabled.i = 1;
    IDP_AddToGroup(material->id.properties,
                   IDP_New(IDP_BOOLEAN, &enabled, materialx::authority_enabled_property));
    set_string(materialx::document_uuid_property, document_uuid);
    set_string(materialx::document_digest_property, materialx::usda_sha256_digest(usda));
    set_string(materialx::document_usda_text_property, text->id.name + 2);
    set_string(materialx::material_path_property, "/Material");
  }
  void TearDown() override
  {
    BKE_main_free(preview_main);
    BKE_main_free(main);
  }
  void set_string(const char *key, const string &value)
  {
    IDP_ReplaceInGroup(material->id.properties, IDP_NewString(value.c_str(), key));
  }
  void set_pointer(ID *id)
  {
    IDPropertyTemplate value = {};
    value.id = id;
    IDP_ReplaceInGroup(material->id.properties,
                       IDP_New(IDP_ID, &value, "materialx_authoring.document_usda_text"));
  }
  BlenderMaterialXAuthority localized_authority()
  {
    /* Same localization flags as editors/render/render_preview.cc:duplicate_ids. */
    ID *copy = BKE_id_copy_ex(nullptr, &material->id, nullptr,
                             LIB_ID_CREATE_LOCAL | LIB_ID_COPY_LOCALIZE | LIB_ID_COPY_NO_ANIMDATA);
    EXPECT_NE(copy, nullptr);
    EXPECT_TRUE(preview_main->texts.is_empty());
    BLI_addtail(&preview_main->materials, copy);
    auto result = find_blender_materialx_authority(*reinterpret_cast<Material *>(copy), *preview_main);
    BLI_remlink(&preview_main->materials, copy);
    BKE_id_free(nullptr, copy);
    return result;
  }
};

TEST_F(MaterialXTextDependency, LocalizedMaterialCarriesTextOutsidePreviewMain)
{
  set_pointer(&text->id);
  const auto result = localized_authority();
  EXPECT_TRUE(result.is_valid()) << result.error;
  EXPECT_EQ(result.source.usda, usda);
  EXPECT_EQ(BKE_libblock_find_name(main, ID_TXT, text->id.name + 2), &text->id);
}

TEST_F(MaterialXTextDependency, LegacyNameWorksOnlyWhenTextIsInSuppliedMain)
{
  EXPECT_TRUE(find_blender_materialx_authority(*material, *main).is_valid());
  EXPECT_FALSE(localized_authority().is_valid());
}

TEST_F(MaterialXTextDependency, MalformedPresentPointerNeverFallsBackToValidName)
{
  set_string("materialx_authoring.document_usda_text", text->id.name + 2);
  EXPECT_FALSE(find_blender_materialx_authority(*material, *main).is_valid());
  set_pointer(nullptr);
  EXPECT_FALSE(find_blender_materialx_authority(*material, *main).is_valid());
  set_pointer(&material->id);
  EXPECT_FALSE(find_blender_materialx_authority(*material, *main).is_valid());
}

TEST_F(MaterialXTextDependency, PointerNameAndBytesRemainAuthenticated)
{
  Text *wrong = BKE_text_add(main, "OtherText");
  txt_insert_buf(wrong, usda.c_str(), int(usda.size()));
  set_pointer(&wrong->id);
  EXPECT_FALSE(find_blender_materialx_authority(*material, *main).is_valid());
  set_pointer(&text->id);
  txt_insert_buf(text, "# tampered\n", 11);
  EXPECT_FALSE(find_blender_materialx_authority(*material, *main).is_valid());
}
}  // namespace
CCL_NAMESPACE_END
