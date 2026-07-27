"""Composed MaterialX authority smoke for Cycles CPU/CUDA parity."""

import bpy
import hashlib
import math
import os
import tempfile


DOCUMENT_UUID = "bad2d3d7-f39b-4e7d-9f09-555c4a35b410"
TEXT_NAME = f".materialx_usdshade_{DOCUMENT_UUID}"
MATERIAL_PATH = "/Looks/SourceMaterial"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def write_green_texture(path):
    if os.path.exists(path):
        os.remove(path)
    image = bpy.data.images.new("MaterialXComposedGreen", width=1, height=1, alpha=False)
    image.pixels.foreach_set((0.02, 0.8, 0.08, 1.0))
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    require(os.path.exists(path), "Could not create composed MaterialX texture")


def source_usda(texture_path):
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
            color3f inputs:base_color.connect = </Looks/SourceMaterial/ColorMix.outputs:out>
            float inputs:base_metalness = 0
            float inputs:specular_roughness = 1
            token outputs:out
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

        def Shader "ScaledUV"
        {{
            uniform token info:id = "ND_multiply_vector2"
            float2 inputs:in1.connect = </Looks/SourceMaterial/UV.outputs:out>
            float2 inputs:in2.connect = </Looks/SourceMaterial/UVScale.outputs:out>
            float2 outputs:out
        }}

        def Shader "Image"
        {{
            uniform token info:id = "ND_image_color3"
            asset inputs:file = @{asset_path}@
            float2 inputs:texcoord.connect = </Looks/SourceMaterial/ScaledUV.outputs:out>
            color3f outputs:out
        }}

        def Shader "Scalar"
        {{
            uniform token info:id = "ND_constant_float"
            float inputs:value = 0.5
            float outputs:out
        }}

        def Shader "Sine"
        {{
            uniform token info:id = "ND_sin_float"
            float inputs:in.connect = </Looks/SourceMaterial/Scalar.outputs:out>
            float outputs:out
        }}

        def Shader "Range"
        {{
            uniform token info:id = "ND_range_float"
            float inputs:in.connect = </Looks/SourceMaterial/Sine.outputs:out>
            float inputs:inlow = 0
            float inputs:inhigh = 1
            float inputs:outlow = 0.25
            float inputs:outhigh = 0.75
            float inputs:gamma = 1
            bool inputs:doclamp = true
            float outputs:out
        }}

        def Shader "ColorMix"
        {{
            uniform token info:id = "ND_mix_color3"
            color3f inputs:bg = (0.02, 0.03, 0.8)
            color3f inputs:fg.connect = </Looks/SourceMaterial/Image.outputs:out>
            float inputs:mix.connect = </Looks/SourceMaterial/Range.outputs:out>
            color3f outputs:out
        }}
    }}
}}
'''


def render_pixel(scene, path):
    if os.path.exists(path):
        os.remove(path)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    require(os.path.exists(path), "Cycles did not write composed MaterialX render")
    image = bpy.data.images.load(path, check_existing=False)
    offset = ((16 * 32) + 16) * 4
    require(tuple(image.size) == (32, 32), "Composed MaterialX render has unexpected size")
    pixel = tuple(image.pixels[offset:offset + 3])
    bpy.data.images.remove(image)
    return pixel


bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.render.engine = "CYCLES"
scene.cycles.device = os.environ.get("CYCLES_TEST_DEVICE", "CPU")
scene.cycles.samples = 16
scene.render.resolution_x = 32
scene.render.resolution_y = 32
scene.render.resolution_percentage = 100
scene.render.image_settings.file_format = "PNG"
scene.view_settings.look = "None"
scene.view_settings.view_transform = "Standard"
scene.world = bpy.data.worlds.new("MaterialXComposedWorld")
scene.world.color = (0.0, 0.0, 0.0)

texture_path = os.path.join(tempfile.gettempdir(), "materialx_composed_cuda_texture.png")
render_path = os.path.join(tempfile.gettempdir(), "materialx_composed_cuda_render.png")
write_green_texture(texture_path)
source_text = source_usda(texture_path)
source = bpy.data.texts.new(TEXT_NAME)
source.write(source_text)

material = bpy.data.materials.new("MaterialXComposedAuthority")
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
material["materialx_authoring.document_digest"] = "sha256:" + hashlib.sha256(
    source_text.encode("utf-8")).hexdigest()
material["materialx_authoring.document_usda_text_name"] = TEXT_NAME
material["materialx_authoring.document_material_path"] = MATERIAL_PATH

bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1.0)
sphere = bpy.context.object
sphere.data.materials.append(material)
require(len(sphere.data.uv_layers) > 0, "UV sphere has no UV layer")
sphere.data.uv_layers.active.name = "st"
bpy.ops.object.shade_smooth()
bpy.ops.object.camera_add(location=(0.0, -4.0, 0.0), rotation=(math.pi / 2.0, 0.0, 0.0))
scene.camera = bpy.context.object
bpy.ops.object.light_add(type="POINT", location=(1.5, -2.5, 2.0))
bpy.context.object.data.energy = 1000.0

red, green, blue = render_pixel(scene, render_path)
print("MATERIALX_COMPOSED_PIXEL red=%.6f green=%.6f blue=%.6f" % (red, green, blue))
require(green > red * 2.0 and blue > red * 2.0,
        "Composed MaterialX graph did not produce the expected image/scalar-mix color")
print("MATERIALX_COMPOSED_AUTHORITY_CYCLES_SMOKE_PASS")
