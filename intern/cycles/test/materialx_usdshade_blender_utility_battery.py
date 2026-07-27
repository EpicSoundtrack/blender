"""One-process canonical authority render battery for supported utility nodes."""

import bpy
import hashlib
import math
import os
import tempfile
import uuid


MATERIAL_PATH = "/Looks/SourceMaterial"


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def usda(body):
    return f'''#usda 1.0

def Scope "Looks"
{{
    def Material "SourceMaterial"
    {{
        token outputs:mtlx:surface.connect = </Looks/SourceMaterial/OpenPBR.outputs:out>

        def Shader "OpenPBR"
        {{
            uniform token info:id = "ND_open_pbr_surface_surfaceshader"
            color3f inputs:base_color.connect = </Looks/SourceMaterial/Result.outputs:out>
            token outputs:out
        }}
{body}
    }}
}}
'''


CASES = (
    (
        "scalar_utilities",
        usda('''
        def Shader "Input"
        {
            uniform token info:id = "ND_constant_float"
            float inputs:value = -1.25
            float outputs:out
        }
        def Shader "Absolute"
        {
            uniform token info:id = "ND_absval_float"
            float inputs:in.connect = </Looks/SourceMaterial/Input.outputs:out>
            float outputs:out
        }
        def Shader "Fraction"
        {
            uniform token info:id = "ND_fract_float"
            float inputs:in.connect = </Looks/SourceMaterial/Absolute.outputs:out>
            float outputs:out
        }
        def Shader "Result"
        {
            uniform token info:id = "ND_convert_float_color3"
            float inputs:in.connect = </Looks/SourceMaterial/Fraction.outputs:out>
            color3f outputs:out
        }'''),
        lambda pixel: min(pixel) > 0.01 and max(pixel) - min(pixel) < 0.05,
    ),
    (
        "color_arithmetic",
        usda('''
        def Shader "First"
        {
            uniform token info:id = "ND_constant_color3"
            color3f inputs:value = (0.02, 0.5, 0.04)
            color3f outputs:out
        }
        def Shader "Second"
        {
            uniform token info:id = "ND_constant_color3"
            color3f inputs:value = (0.03, 0.25, 0.05)
            color3f outputs:out
        }
        def Shader "Sum"
        {
            uniform token info:id = "ND_add_color3"
            color3f inputs:in1.connect = </Looks/SourceMaterial/First.outputs:out>
            color3f inputs:in2.connect = </Looks/SourceMaterial/Second.outputs:out>
            color3f outputs:out
        }
        def Shader "Identity"
        {
            uniform token info:id = "ND_constant_color3"
            color3f inputs:value = (1, 1, 1)
            color3f outputs:out
        }
        def Shader "Result"
        {
            uniform token info:id = "ND_multiply_color3"
            color3f inputs:in1.connect = </Looks/SourceMaterial/Sum.outputs:out>
            color3f inputs:in2.connect = </Looks/SourceMaterial/Identity.outputs:out>
            color3f outputs:out
        }'''),
        lambda pixel: pixel[1] > pixel[0] * 2.0 and pixel[1] > pixel[2] * 2.0,
    ),
    (
        "luminance_color3",
        usda('''
        def Shader "Input"
        {
            uniform token info:id = "ND_constant_color3"
            color3f inputs:value = (0.02, 0.5, 0.04)
            color3f outputs:out
        }
        def Shader "Luminance"
        {
            uniform token info:id = "ND_luminance_color3"
            color3f inputs:in.connect = </Looks/SourceMaterial/Input.outputs:out>
            color3f inputs:lumacoeffs = (0.2126, 0.7152, 0.0722)
            float outputs:out
        }
        def Shader "Result"
        {
            uniform token info:id = "ND_convert_float_color3"
            float inputs:in.connect = </Looks/SourceMaterial/Luminance.outputs:out>
            color3f outputs:out
        }'''),
        lambda pixel: min(pixel) > 0.05 and max(pixel) - min(pixel) < 0.05,
    ),
    (
        "vector3_extract",
        usda('''
        def Shader "First"
        {
            uniform token info:id = "ND_constant_vector3"
            float3 inputs:value = (0.1, 0.5, 0.2)
            float3 outputs:out
        }
        def Shader "Second"
        {
            uniform token info:id = "ND_constant_vector3"
            float3 inputs:value = (0.0, 0.25, 0.0)
            float3 outputs:out
        }
        def Shader "Added"
        {
            uniform token info:id = "ND_add_vector3"
            float3 inputs:in1.connect = </Looks/SourceMaterial/First.outputs:out>
            float3 inputs:in2.connect = </Looks/SourceMaterial/Second.outputs:out>
            float3 outputs:out
        }
        def Shader "Extract"
        {
            uniform token info:id = "ND_extract_vector3"
            int inputs:index = 1
            float3 inputs:in.connect = </Looks/SourceMaterial/Added.outputs:out>
            float outputs:out
        }
        def Shader "Result"
        {
            uniform token info:id = "ND_convert_float_color3"
            float inputs:in.connect = </Looks/SourceMaterial/Extract.outputs:out>
            color3f outputs:out
        }'''),
        lambda pixel: min(pixel) > 0.05 and max(pixel) - min(pixel) < 0.05,
    ),
)


def authority_material(case_name, source_usda):
    document_uuid = str(uuid.uuid5(uuid.NAMESPACE_URL, f"materialx-utility-battery/{case_name}"))
    text_name = f".materialx_usdshade_{document_uuid}"
    source = bpy.data.texts.new(text_name)
    source.write(source_usda)
    material = bpy.data.materials.new(f"MaterialXUtilityBattery_{case_name}")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    decoy = nodes.new("ShaderNodeEmission")
    decoy.inputs["Color"].default_value = (1.0, 0.0, 0.0, 1.0)
    decoy.inputs["Strength"].default_value = 5.0
    material.node_tree.links.new(decoy.outputs["Emission"], output.inputs["Surface"])
    material["materialx_authoring.cycles_native_authority"] = True
    material["materialx_authoring.document_uuid"] = document_uuid
    material["materialx_authoring.document_digest"] = "sha256:" + hashlib.sha256(
        source_usda.encode("utf-8")).hexdigest()
    material["materialx_authoring.document_usda_text_name"] = text_name
    material["materialx_authoring.document_material_path"] = MATERIAL_PATH
    return material, source


def render_pixel(scene, case_name):
    path = os.path.join(tempfile.gettempdir(), f"materialx_utility_battery_{case_name}.png")
    if os.path.exists(path):
        os.remove(path)
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    require(os.path.exists(path), f"{case_name}: Cycles did not write a render")
    image = bpy.data.images.load(path, check_existing=False)
    offset = ((16 * 32) + 16) * 4
    require(tuple(image.size) == (32, 32), f"{case_name}: unexpected render size")
    pixel = tuple(image.pixels[offset : offset + 3])
    bpy.data.images.remove(image)
    return pixel


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
scene.world = bpy.data.worlds.new("MaterialXUtilityBatteryWorld")
scene.world.color = (0.0, 0.0, 0.0)

bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1.0)
sphere = bpy.context.object
bpy.ops.object.shade_smooth()
bpy.ops.object.camera_add(location=(0.0, -4.0, 0.0), rotation=(math.pi / 2.0, 0.0, 0.0))
scene.camera = bpy.context.object
bpy.ops.object.light_add(type="POINT", location=(1.5, -2.5, 2.0))
bpy.context.object.data.energy = 1000.0

tamper_source = None
tamper_material = None
for case_name, source_usda, expectation in CASES:
    material, source = authority_material(case_name, source_usda)
    sphere.data.materials.clear()
    sphere.data.materials.append(material)
    pixel = render_pixel(scene, case_name)
    print("MATERIALX_UTILITY_BATTERY_PIXEL %s red=%.6f green=%.6f blue=%.6f" %
          ((case_name,) + pixel))
    require(expectation(pixel), f"{case_name}: canonical authority result was unexpected")
    if case_name == "scalar_utilities":
        tamper_source = source
        tamper_material = material

require(tamper_source is not None and tamper_material is not None, "Battery has no tamper case")
tamper_source.write("# tampered after canonical digest\n")
tamper_material.update_tag()
sphere.data.materials.clear()
sphere.data.materials.append(tamper_material)
tampered = render_pixel(scene, "tampered")
print("MATERIALX_UTILITY_BATTERY_TAMPERED_PIXEL red=%.6f green=%.6f blue=%.6f" % tampered)
require(max(tampered) < 0.01, "Utility battery did not fail closed for tampered authority")
print("MATERIALX_UTILITY_BATTERY_CYCLES_SMOKE_PASS")
