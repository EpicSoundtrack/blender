"""Headless end-to-end texture smoke for canonical USDShade MaterialX authority.

This deliberately uses a red Blender node graph as a decoy.  The render must
instead be dominated by the green image reached through the authoritative
hidden-Text USDShade document and its typed vector2/UV image path.
"""

import bpy
import hashlib
import math
import os
import tempfile


DOCUMENT_UUID = "8c4c610f-7f1e-4111-b3f6-9cc6076671e9"
TEXT_NAME = f".materialx_usdshade_{DOCUMENT_UUID}"
MATERIAL_PATH = "/Looks/SourceMaterial"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write_green_texture(path):
    if os.path.exists(path):
        os.remove(path)

    image = bpy.data.images.new("USDShadeTextureSmokeGreen", width=1, height=1, alpha=False)
    image.pixels.foreach_set((0.02, 0.8, 0.08, 1.0))
    image.filepath_raw = path
    image.file_format = 'PNG'
    image.save()
    require(os.path.exists(path), "Could not create texture fixture for the USDShade smoke test")


def source_usda(texture_path):
    # USDA asset paths use forward slashes even when the test runs on Windows.
    asset_path = texture_path.replace("\\", "/")
    return f'''#usda 1.0

def Scope "Looks"
{{
    def Material "SourceMaterial"
    {{
        token outputs:mtlx:surface.connect = </Looks/SourceMaterial/OpenPBR.outputs:out>

        def Shader "OpenPBR"
        {{
            uniform token info:id = "ND_open_pbr_surface_surfaceshader"
            color3f inputs:base_color.connect = </Looks/SourceMaterial/BaseColorImage.outputs:out>
            float inputs:base_metalness = 0
            float inputs:specular_roughness = 1
            token outputs:out
        }}

        def Shader "BaseColorImage"
        {{
            uniform token info:id = "ND_image_color3"
            asset inputs:file = @{asset_path}@
            float2 inputs:texcoord.connect = </Looks/SourceMaterial/UVMultiply.outputs:out>
            color3f outputs:out
        }}

        def Shader "UV"
        {{
            uniform token info:id = "ND_geompropvalue_vector2"
            string inputs:geomprop = "st"
            float2 outputs:out
        }}

        def Shader "UVScale"
        {{
            uniform token info:id = "ND_constant_vector2"
            float2 inputs:value = (1, 1)
            float2 outputs:out
        }}

        def Shader "UVMultiply"
        {{
            uniform token info:id = "ND_multiply_vector2"
            float2 inputs:in1.connect = </Looks/SourceMaterial/UV.outputs:out>
            float2 inputs:in2.connect = </Looks/SourceMaterial/UVScale.outputs:out>
            float2 outputs:out
        }}
    }}
}}
'''


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = 'CYCLES'
scene.cycles.samples = 16
scene.render.resolution_x = 32
scene.render.resolution_y = 32
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = 'PNG'
scene.view_settings.look = 'None'
scene.view_settings.view_transform = 'Standard'
if scene.world is None:
    scene.world = bpy.data.worlds.new("USDShadeTextureSmokeWorld")
scene.world.color = (0.0, 0.0, 0.0)

texture_path = os.path.join(tempfile.gettempdir(), "materialx_usdshade_texture_smoke.png")
render_path = os.path.join(tempfile.gettempdir(), "materialx_usdshade_texture_render.png")
write_green_texture(texture_path)
if os.path.exists(render_path):
    os.remove(render_path)
scene.render.filepath = render_path

source_usda_text = source_usda(texture_path)
source = bpy.data.texts.new(TEXT_NAME)
source.write(source_usda_text)
require(source.name == TEXT_NAME, "Canonical hidden Text name was changed")

material = bpy.data.materials.new("USDShadeTextureSourceMaterial")
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
    "sha256:" + hashlib.sha256(source_usda_text.encode("utf-8")).hexdigest()
)
material["materialx_authoring.document_usda_text_name"] = TEXT_NAME
material["materialx_authoring.document_material_path"] = MATERIAL_PATH

bpy.ops.mesh.primitive_uv_sphere_add(segments=24, ring_count=12, radius=1.0)
sphere = bpy.context.object
sphere.data.materials.append(material)
require(len(sphere.data.uv_layers) > 0, "UV sphere has no UV layer")
sphere.data.uv_layers.active.name = "st"

bpy.ops.object.camera_add(location=(0.0, -3.0, 0.0), rotation=(math.pi / 2.0, 0.0, 0.0))
scene.camera = bpy.context.object
bpy.ops.object.light_add(type='POINT', location=(0.0, -3.0, 0.0))
bpy.context.object.data.energy = 800.0

bpy.ops.render.render(write_still=True)
require(os.path.exists(render_path), "Cycles did not write the texture smoke-test render")
result = bpy.data.images.load(render_path, check_existing=False)
pixel_offset = ((16 * 32) + 16) * 4
print(f"MATERIALX_USDSHADE_TEXTURE_RENDER_RESULT size={tuple(result.size)} pixels={len(result.pixels)}")
require(tuple(result.size) == (32, 32), "Cycles did not produce the expected 32x32 texture render")
require(len(result.pixels) >= pixel_offset + 3, "Texture render result has no readable pixel buffer")
red, green, blue = result.pixels[pixel_offset : pixel_offset + 3]
print(f"MATERIALX_USDSHADE_TEXTURE_PIXEL red={red:.6f} green={green:.6f} blue={blue:.6f}")
require(green > 0.05, "USDShade texture did not produce visible green base color")
require(green > red * 1.3 and green > blue * 1.3,
        "Cycles did not render the authoritative USDShade image instead of the red Blender decoy")

source.write("# tampered after canonical digest\n")
material.update_tag()
tampered_path = os.path.join(
    tempfile.gettempdir(), "materialx_usdshade_texture_tampered_smoke.png"
)
if os.path.exists(tampered_path):
    os.remove(tampered_path)
scene.render.filepath = tampered_path
bpy.ops.render.render(write_still=True)
tampered = bpy.data.images.load(tampered_path, check_existing=False)
tampered_red, tampered_green, tampered_blue = tampered.pixels[pixel_offset : pixel_offset + 3]
print(
    "MATERIALX_USDSHADE_TEXTURE_TAMPERED_PIXEL "
    f"red={tampered_red:.6f} green={tampered_green:.6f} blue={tampered_blue:.6f}"
)
require(
    max(tampered_red, tampered_green, tampered_blue) < 0.01,
    "Cycles did not fail closed for tampered USDShade texture authority",
)

source.clear()
source.write(source_usda_text)
material["materialx_authoring.document_digest"] = "sha256:not-a-canonical-digest"
material.update_tag()
malformed_path = os.path.join(
    tempfile.gettempdir(), "materialx_usdshade_texture_malformed_digest_smoke.png"
)
if os.path.exists(malformed_path):
    os.remove(malformed_path)
scene.render.filepath = malformed_path
bpy.ops.render.render(write_still=True)
malformed = bpy.data.images.load(malformed_path, check_existing=False)
malformed_red, malformed_green, malformed_blue = malformed.pixels[pixel_offset : pixel_offset + 3]
print(
    "MATERIALX_USDSHADE_TEXTURE_MALFORMED_DIGEST_PIXEL "
    f"red={malformed_red:.6f} green={malformed_green:.6f} blue={malformed_blue:.6f}"
)
require(
    max(malformed_red, malformed_green, malformed_blue) < 0.01,
    "Cycles did not fail closed for malformed USDShade texture authority digest",
)
print("MATERIALX_USDSHADE_CYCLES_TEXTURE_SMOKE_PASS")
