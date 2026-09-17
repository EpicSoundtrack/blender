"""Non-render contract test for _cycles.materialx_validate_authority."""

import hashlib
import uuid

import _cycles

DOCUMENT_UUID = str(uuid.uuid5(uuid.NAMESPACE_URL, "cycles/materialx/native-authority-validation"))
TEXT_NAME = f".materialx_usdshade_{DOCUMENT_UUID}"
MATERIAL_PATH = "/Looks/TestMaterial"


def digest(usda):
    return "sha256:" + hashlib.sha256(usda.encode("utf-8")).hexdigest()


def openpbr_usda(material_path=MATERIAL_PATH):
    return f'''#usda 1.0

def Scope "Looks"
{{
    def Material "{material_path.rsplit('/', 1)[-1]}"
    {{
        token outputs:mtlx:surface.connect = <{material_path}/OpenPBR.outputs:out>

        def Shader "OpenPBR"
        {{
            uniform token info:id = "ND_open_pbr_surface_surfaceshader"
            color3f inputs:base_color = (0.02, 0.8, 0.08)
            float inputs:specular_roughness = 0.35
            token outputs:out
        }}
    }}
}}
'''


def unsupported_shader_usda():
    return f'''#usda 1.0

def Scope "Looks"
{{
    def Material "TestMaterial"
    {{
        token outputs:mtlx:surface.connect = </Looks/TestMaterial/Unsupported.outputs:out>

        def Shader "Unsupported"
        {{
            uniform token info:id = "ND_not_a_supported_surfaceshader"
            token outputs:out
        }}
    }}
}}
'''


def validate(usda, **overrides):
    kwargs = {
        "document_uuid": DOCUMENT_UUID,
        "digest": digest(usda),
        "usda_text_name": TEXT_NAME,
        "material_path": MATERIAL_PATH,
        "usda": usda,
    }
    kwargs.update(overrides)
    result = _cycles.materialx_validate_authority(**kwargs)
    assert type(result["available"]) is bool, result
    assert type(result["valid"]) is bool, result
    assert isinstance(result["phase"], str), result
    assert isinstance(result["error"], str), result
    return result


capabilities = _cycles.materialx_capabilities()
assert capabilities.get("api_version") == 1, capabilities
assert hasattr(_cycles, "materialx_validate_authority")

valid_usda = openpbr_usda()
valid = validate(valid_usda)
assert valid == {"available": True, "valid": True, "phase": "lowered", "error": ""}, valid

mismatched_digest = validate(valid_usda, digest="sha256:" + ("0" * 64))
assert mismatched_digest["available"] is True, mismatched_digest
assert mismatched_digest["valid"] is False, mismatched_digest
assert mismatched_digest["phase"] == "contract", mismatched_digest
assert "contract" in mismatched_digest["error"], mismatched_digest

malformed_usda = validate("#usda 1.0\nnot valid USDA\n")
assert malformed_usda["available"] is True, malformed_usda
assert malformed_usda["valid"] is False, malformed_usda
assert malformed_usda["phase"] == "lowering", malformed_usda
assert "USDA" in malformed_usda["error"], malformed_usda

missing_material = validate(valid_usda, material_path="/Looks/MissingMaterial")
assert missing_material["available"] is True, missing_material
assert missing_material["valid"] is False, missing_material
assert missing_material["phase"] == "lowering", missing_material
assert "material path" in missing_material["error"], missing_material

unsupported_usda = unsupported_shader_usda()
unsupported = validate(unsupported_usda)
assert unsupported["available"] is True, unsupported
assert unsupported["valid"] is False, unsupported
assert unsupported["phase"] == "lowering", unsupported
assert unsupported["error"], unsupported

before = valid_usda[:]
again = validate(valid_usda)
assert again["valid"] is True, again
assert valid_usda == before, "materialx_validate_authority mutated caller USDA source text"

old_api = validate(valid_usda, api_version=0)
assert old_api["available"] is True, old_api
assert old_api["valid"] is False, old_api
assert old_api["phase"] == "contract", old_api
assert "API version" in old_api["error"], old_api

print("MATERIALX_NATIVE_AUTHORITY_VALIDATION_PASS")
