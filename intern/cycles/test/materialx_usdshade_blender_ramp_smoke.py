"""Headless authority smoke for the exact MaterialX top-to-bottom color ramp."""

import bpy
import hashlib
import math
import os
import tempfile


DOCUMENT_UUID = "556a8170-23bd-4765-8f38-c6d073d3d509"
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
            color3f inputs:base_color.connect = </Looks/SourceMaterial/Ramp.outputs:out>
            token outputs:out
        }

        def Shader "UV"
        {
            uniform token info:id = "ND_geompropvalue_vector2"
            string inputs:geomprop = "st"
            float2 outputs:out
        }

        def Shader "UVShift"
        {
            uniform token info:id = "ND_constant_vector2"
            float2 inputs:value = (0.25, 0.5)
            float2 outputs:out
        }

        def Shader "ShiftedUV"
        {
            uniform token info:id = "ND_add_vector2"
            float2 inputs:in1.connect = </Looks/SourceMaterial/UV.outputs:out>
            float2 inputs:in2.connect = </Looks/SourceMaterial/UVShift.outputs:out>
            float2 outputs:out
        }

        def Shader "Ramp"
        {
            uniform token info:id = "ND_ramptb_color3"
            color3f inputs:valuet = (0.02, 0.08, 0.8)
            color3f inputs:valueb = (0.02, 0.8, 0.08)
            float2 inputs:texcoord.connect = </Looks/SourceMaterial/ShiftedUV.outputs:out>
            color3f outputs:out
        }
    }
}
"""


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def render_pixel(scene, path):
    if os.path.exists(path):
        os.remove(path)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    require(os.path.exists(path), "Cycles did not write the ramp smoke render")
    image = bpy.data.images.load(path, check_existing=False)
    offset = ((16 * 32) + 16) * 4
    require(tuple(image.size) == (32, 32), "Cycles did not produce the expected 32x32 render")
    require(len(image.pixels) >= offset + 3, "Ramp render has no readable center pixel")
    return image.pixels[offset : offset + 3]


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
    scene.world = bpy.data.worlds.new("MaterialXRampAuthorityWorld")
scene.world.color = (0.0, 0.0, 0.0)

source = bpy.data.texts.new(TEXT_NAME)
source.write(SOURCE_USDA)
material = bpy.data.materials.new("CanonicalMaterialXRampAuthority")
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
    SOURCE_USDA.encode("utf-8")
).hexdigest()
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

canonical = render_pixel(scene, os.path.join(tempfile.gettempdir(), "materialx_ramp_canonical.png"))
print("MATERIALX_RAMP_CANONICAL_PIXEL red=%.6f green=%.6f blue=%.6f" % tuple(canonical))
require(max(canonical[1], canonical[2]) > canonical[0] * 1.5,
        "Cycles rendered the red Blender decoy instead of the canonical MaterialX ramp")

source.write("# tampered after canonical digest\n")
material.update_tag()
tampered = render_pixel(scene, os.path.join(tempfile.gettempdir(), "materialx_ramp_tampered.png"))
print("MATERIALX_RAMP_TAMPERED_PIXEL red=%.6f green=%.6f blue=%.6f" % tuple(tampered))
require(max(tampered) < 0.01, "Cycles did not fail closed for tampered ramp authority")

source.clear()
source.write(SOURCE_USDA)
material["materialx_authoring.document_digest"] = "sha256:not-a-canonical-digest"
material.update_tag()
malformed = render_pixel(scene, os.path.join(tempfile.gettempdir(), "materialx_ramp_malformed.png"))
print("MATERIALX_RAMP_MALFORMED_PIXEL red=%.6f green=%.6f blue=%.6f" % tuple(malformed))
require(max(malformed) < 0.01, "Cycles did not fail closed for malformed ramp authority")
print("MATERIALX_RAMP_AUTHORITY_CYCLES_SMOKE_PASS")
