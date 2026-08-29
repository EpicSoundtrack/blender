/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "blender/materialx_authority.h"

#include "MEM_guardedalloc.h"

#include "BKE_idprop.hh"
#include "BKE_lib_id.hh"
#include "BKE_text.h"

#include "DNA_material_types.h"
#include "DNA_text_types.h"

CCL_NAMESPACE_BEGIN

/* BKE_idprop.hh's IDP_*_get macros expand with unqualified `eIDPropertyType`
 * enumerators (e.g. `IDP_BOOLEAN`), which only resolve via ordinary
 * unqualified lookup inside `namespace blender` (or a `using` bringing them
 * into scope). This translation unit lives in `ccl`, so pull the specific
 * enumerators used by the macros below into scope explicitly rather than
 * `using namespace blender;`, to avoid widening lookup for the whole file. */
using blender::IDP_BOOLEAN;
using blender::IDP_INT;
using blender::IDP_STRING;

namespace {

const blender::IDProperty *material_id_property(const blender::Material &material,
                                                const char *name)
{
  if (material.id.properties == nullptr) {
    return nullptr;
  }
  return blender::IDP_GetPropertyFromGroup(material.id.properties, name);
}

bool material_id_property_bool(const blender::Material &material, const char *name)
{
  const blender::IDProperty *property = material_id_property(material, name);
  if (property == nullptr) {
    return false;
  }
  if (property->type == blender::IDP_BOOLEAN) {
    return IDP_bool_get(property);
  }
  /* Older Blender files can encode Python Boolean custom properties as ints. */
  return property->type == blender::IDP_INT && IDP_int_get(property) != 0;
}

bool material_id_property_string(const blender::Material &material,
                                 const char *name,
                                 string *value)
{
  const blender::IDProperty *property = material_id_property(material, name);
  if (property == nullptr || property->type != blender::IDP_STRING) {
    return false;
  }
  *value = IDP_string_get(property);
  return true;
}

}  // namespace

BlenderMaterialXAuthority find_blender_materialx_authority(const blender::Material &material,
                                                           blender::Main &b_data)
{
  BlenderMaterialXAuthority result;
  if (!material_id_property_bool(material, materialx::authority_enabled_property)) {
    return result;
  }
  result.selected = true;

  materialx::Authority &authority = result.source;
  if (!material_id_property_string(
          material, materialx::document_uuid_property, &authority.document_uuid) ||
      !material_id_property_string(
          material, materialx::document_digest_property, &authority.digest) ||
      !material_id_property_string(
          material, materialx::document_usda_text_property, &authority.usda_text_name) ||
      !material_id_property_string(
          material, materialx::material_path_property, &authority.material_path))
  {
    result.error = "Material ID-property contract is incomplete";
    return result;
  }

  blender::ID *const source_id = blender::BKE_libblock_find_name(
      &b_data, blender::ID_TXT, authority.usda_text_name.c_str());
  if (source_id == nullptr) {
    result.error = "referenced USDA Text datablock is missing";
    return result;
  }

  size_t source_size = 0;
  char *source = blender::txt_to_buf(reinterpret_cast<blender::Text *>(source_id), &source_size);
  authority.usda.assign(source, source_size);
  MEM_delete_void(static_cast<void *>(source));

  if (!materialx::is_valid(authority)) {
    result.error = "identity, USDA digest, or representation is malformed";
  }
  return result;
}

CCL_NAMESPACE_END
