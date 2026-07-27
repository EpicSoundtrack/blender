"""Headless Blender smoke for canonical USDShade MaterialX authority in Cycles.

The Blender node tree is deliberately red. The canonical hidden-Text authority
is green, so the rendered center pixel proves which material source Cycles used.
The USDA also carries a roughness multiply covered by the C++ graph assertions.
"""

import bpy
import hashlib
import math
import os
import tempfile


DOCUMENT_UUID = "9c37e82e-63a1-470d-a704-e0daf9cfd814"
TEXT_NAME = f".materialx_usdshade_{DOCUMENT_UUID}"
MATERIAL_PATH = "/Looks/SourceMaterial"
SOURCE_USDA = """#usda 1.0

def Scope "Looks"
{
    def Material "SourceMaterial"
    {
        token outputs:mtlx:surface.connect = </Looks/SourceMaterial/OpenPBR.outputs:out>

        def Shader "OpenPBR"
        {
            uniform token info:id = "ND_open_pbr_surface_surfaceshader"
            color3f inputs:base_color.connect = </Looks/SourceMaterial/BaseColor.outputs:out>
            float inputs:specular_roughness.connect = </Looks/SourceMaterial/RoughnessMultiply.outputs:out>
            token outputs:out
        }

        def Shader "BaseColor"
        {
            uniform token info:id = "ND_constant_color3"
            color3f inputs:value = (0.02, 0.8, 0.08)
            color3f outputs:out
        }

        def Shader "RoughnessMultiply"
        {
            uniform token info:id = "ND_multiply_float"
            float inputs:in1.connect = </Looks/SourceMaterial/RoughnessFirst.outputs:out>
            float inputs:in2.connect = </Looks/SourceMaterial/RoughnessSecond.outputs:out>
            float outputs:out
        }

        def Shader "RoughnessFirst"
        {
            uniform token info:id = "ND_constant_float"
            float inputs:value = 0.8
            float outputs:out
        }

        def Shader "RoughnessSecond"
        {
            uniform token info:id = "ND_constant_float"
            float inputs:value = 0.9
            float outputs:out
        }
    }
}
"""


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 16
scene.render.resolution_x = 32
scene.render.resolution_y = 32
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.view_settings.view_transform = "Standard"
if scene.world is None:
    scene.world = bpy.data.worlds.new("MaterialXAuthorityWorld")
scene.world.color = (0.0, 0.0, 0.0)

render_path = os.path.join(tempfile.gettempdir(), "materialx_authority_cycles_smoke.png")
if os.path.exists(render_path):
    os.remove(render_path)
scene.render.filepath = render_path

source = bpy.data.texts.new(TEXT_NAME)
source.write(SOURCE_USDA)
require(source.name == TEXT_NAME, "Canonical hidden Text name was changed")

material = bpy.data.materials.new("CanonicalMaterialXAuthority")
material.use_nodes = True
nodes = material.node_tree.nodes
nodes.clear()
output = nodes.new("ShaderNodeOutputMaterial")
decoy = nodes.new("ShaderNodeEmission")
decoy.inputs["Color"].default_value = (1.0, 0.0, 0.0, 1.0)
decoy.inputs["Strength"].default_value = 5.0
material.node_tree.links.new(decoy.outputs["Emission"], output.inputs["Surface"])

material["materialx_authoring.cycles_native_authority"] = True
material["materialx_authoring.document_uuid"] = DOCUMENT_UUID
material["materialx_authoring.document_digest"] = (
    "sha256:" + hashlib.sha256(SOURCE_USDA.encode("utf-8")).hexdigest()
)
material["materialx_authoring.document_usda_text_name"] = TEXT_NAME
material["materialx_authoring.document_material_path"] = MATERIAL_PATH

bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1.0)
sphere = bpy.context.object
sphere.data.materials.append(material)
bpy.ops.object.shade_smooth()

bpy.ops.object.camera_add(location=(0.0, -4.0, 0.0), rotation=(math.pi / 2.0, 0.0, 0.0))
scene.camera = bpy.context.object
bpy.ops.object.light_add(type="POINT", location=(1.5, -2.5, 2.0))
bpy.context.object.data.energy = 1000.0

bpy.ops.render.render(write_still=True)
require(os.path.exists(render_path), "Cycles did not write the authority smoke render")

result = bpy.data.images.load(render_path, check_existing=False)
pixel_offset = ((16 * 32) + 16) * 4
require(tuple(result.size) == (32, 32), "Cycles did not produce the expected 32x32 render")
require(len(result.pixels) >= pixel_offset + 3, "Cycles render has no readable center pixel")
red, green, blue = result.pixels[pixel_offset : pixel_offset + 3]
print(
    "MATERIALX_CANONICAL_AUTHORITY_PIXEL "
    f"red={red:.6f} green={green:.6f} blue={blue:.6f}"
)
require(green > 0.02, "Canonical MaterialX authority produced no visible green response")
require(
    green > red * 1.5 and green > blue * 1.5,
    "Cycles rendered the red Blender decoy instead of canonical MaterialX authority",
)

source.write("# tampered after canonical digest\n")
material.update_tag()
tampered_path = os.path.join(
    tempfile.gettempdir(), "materialx_authority_cycles_tampered_smoke.png"
)
if os.path.exists(tampered_path):
    os.remove(tampered_path)
scene.render.filepath = tampered_path
bpy.ops.render.render(write_still=True)
tampered = bpy.data.images.load(tampered_path, check_existing=False)
tampered_red, tampered_green, tampered_blue = tampered.pixels[pixel_offset : pixel_offset + 3]
print(
    "MATERIALX_TAMPERED_AUTHORITY_PIXEL "
    f"red={tampered_red:.6f} green={tampered_green:.6f} blue={tampered_blue:.6f}"
)
require(
    max(tampered_red, tampered_green, tampered_blue) < 0.01,
    "Cycles did not fail closed for tampered USDA authority",
)

source.clear()
source.write(SOURCE_USDA)
material["materialx_authoring.document_digest"] = "sha256:not-a-canonical-digest"
material.update_tag()
malformed_path = os.path.join(
    tempfile.gettempdir(), "materialx_authority_cycles_malformed_digest_smoke.png"
)
if os.path.exists(malformed_path):
    os.remove(malformed_path)
scene.render.filepath = malformed_path
bpy.ops.render.render(write_still=True)
malformed = bpy.data.images.load(malformed_path, check_existing=False)
malformed_red, malformed_green, malformed_blue = malformed.pixels[
    pixel_offset : pixel_offset + 3
]
print(
    "MATERIALX_MALFORMED_DIGEST_PIXEL "
    f"red={malformed_red:.6f} green={malformed_green:.6f} blue={malformed_blue:.6f}"
)
require(
    max(malformed_red, malformed_green, malformed_blue) < 0.01,
    "Cycles did not fail closed for malformed authority digest",
)
print("MATERIALX_CANONICAL_AUTHORITY_CYCLES_SMOKE_PASS")
