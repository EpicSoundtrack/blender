"""Headless authority-contract test for the minimal Cycles MaterialX conversion operator."""

import bpy
import hashlib
from pathlib import Path
import tempfile
import uuid
from pxr import Usd, UsdShade


AUTHORITY_KEYS = {
    "materialx_authoring.cycles_native_authority",
    "materialx_authoring.document_uuid",
    "materialx_authoring.document_digest",
    "materialx_authoring.document_usda_text_name",
    "materialx_authoring.document_material_path",
}


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


bpy.ops.wm.read_factory_settings(use_empty=True)

import cycles.operators
import cycles.ui
installed_addon_path = Path(cycles.operators.__file__).resolve()
require("addons_core" in installed_addon_path.parts,
        "Conversion smoke did not load the installed Cycles addon runtime")


material = bpy.data.materials.new("ConvertedMaterialXAuthority")
material.diffuse_color = (0.02, 0.8, 0.08, 1.0)
material["unrelated_property"] = "preserve me"

bpy.context.scene.render.engine = "CYCLES"
bpy.ops.mesh.primitive_cube_add()
bpy.context.object.data.materials.append(material)
require(hasattr(bpy.types, "CYCLES_MATERIAL_PT_materialx_authoring"),
        "MaterialX authoring panel was not registered")
require(hasattr(bpy.ops.cycles, "materialx_disable_authority"),
        "MaterialX authority disable operator was not registered")
require(cycles.ui.materialx_authority_status(material) == 'inactive',
        "MaterialX authoring panel did not report an inactive authority")


class CancelWindowManager:
    def __init__(self):
        self.calls = 0

    def invoke_confirm(self, _operator, _event):
        self.calls += 1
        return {"CANCELLED"}


class ConfirmWindowManager:
    def __init__(self, context):
        self.calls = 0
        self.context = context

    def invoke_confirm(self, operator, _event):
        self.calls += 1
        return cycles.operators.CYCLES_OT_materialx_convert_authority.execute(operator, self.context)


class ConfirmContext:
    def __init__(self, window_manager):
        self.object = bpy.context.object
        self.material = None
        self.view_layer = bpy.context.view_layer
        self.window_manager = window_manager


class ReportingOperator:
    def report(self, _types, _message):
        pass


material.use_nodes = True
nodes = material.node_tree.nodes
nodes.clear()
output = nodes.new("ShaderNodeOutputMaterial")
principled = nodes.new("ShaderNodeBsdfPrincipled")
principled.inputs["Base Color"].default_value = (0.02, 0.8, 0.08, 1.0)
principled.inputs["Roughness"].default_value = 0.23
material.node_tree.links.new(principled.outputs["BSDF"], output.inputs["Surface"])

unassigned = bpy.data.materials.new("UnassignedMaterialXAuthority")
failure_context = type("FailureContext", (), {
    "material": unassigned,
    "object": bpy.context.object,
    "view_layer": bpy.context.view_layer,
})()
source_texts_before = {text.name for text in bpy.data.texts}
result = cycles.operators.CYCLES_OT_materialx_convert_authority.execute(
    ReportingOperator(), failure_context)
require(result == {"CANCELLED"}, "Invalid conversion context did not cancel")
require(not {key for key in unassigned.keys() if key.startswith("materialx_authoring.")},
        "Failed conversion wrote an authority contract")
require({text.name for text in bpy.data.texts} == source_texts_before,
        "Failed conversion created a USDShade Text datablock")
unassigned["materialx_authoring.cycles_native_authority"] = True
require(cycles.ui.materialx_authority_status(unassigned) == 'invalid',
        "MaterialX authoring panel did not distinguish invalid authority")
del unassigned["materialx_authoring.cycles_native_authority"]

target_object = bpy.context.object
bpy.ops.mesh.primitive_plane_add()
ambiguous_object = bpy.context.object
ambiguous_object.data.materials.append(material)
ambiguous_object.data.materials.append(bpy.data.materials.new("OtherMaterial"))
target_object.select_set(True)
ambiguous_object.select_set(True)
bpy.context.view_layer.objects.active = ambiguous_object
selection_before = {obj.name for obj in bpy.context.view_layer.objects if obj.select_get()}
active_before = bpy.context.view_layer.objects.active
source_texts_before = {text.name for text in bpy.data.texts}
temporary_usda_before = set(Path(tempfile.gettempdir()).glob("tmp*.usda"))
ambiguous_context = type("AmbiguousContext", (), {
    "material": material,
    "object": ambiguous_object,
    "view_layer": bpy.context.view_layer,
})()
result = cycles.operators.CYCLES_OT_materialx_convert_authority.execute(
    ReportingOperator(), ambiguous_context)
require(result == {"CANCELLED"}, "Ambiguous multi-material conversion did not cancel")
require({obj.name for obj in bpy.context.view_layer.objects if obj.select_get()} == selection_before,
        "Ambiguous conversion did not restore the original selection")
require(bpy.context.view_layer.objects.active is active_before,
        "Ambiguous conversion did not restore the original active object")
require(set(Path(tempfile.gettempdir()).glob("tmp*.usda")) == temporary_usda_before,
        "Ambiguous conversion left a temporary USDA file")
require({text.name for text in bpy.data.texts} == source_texts_before,
        "Ambiguous conversion created a USDShade Text datablock")
require(not {key for key in material.keys() if key.startswith("materialx_authoring.")},
        "Ambiguous conversion wrote an authority contract")

for obj in bpy.context.view_layer.objects:
    obj.select_set(False)
target_object.select_set(True)
bpy.context.view_layer.objects.active = target_object

stage_without_materialx = Usd.Stage.CreateInMemory()
UsdShade.Material.Define(stage_without_materialx, "/root/_materials/ConvertedMaterialXAuthority")
stage_open = Usd.Stage.Open
Usd.Stage.Open = lambda _filepath: stage_without_materialx
selection_before = {obj.name for obj in bpy.context.view_layer.objects if obj.select_get()}
active_before = bpy.context.view_layer.objects.active
source_texts_before = {text.name for text in bpy.data.texts}
temporary_usda_before = set(Path(tempfile.gettempdir()).glob("tmp*.usda"))
try:
    result = cycles.operators.CYCLES_OT_materialx_convert_authority.execute(
        ReportingOperator(), bpy.context)
finally:
    Usd.Stage.Open = stage_open
require(result == {"CANCELLED"},
        "Conversion accepted a USD material without a MaterialX surface network")
require({obj.name for obj in bpy.context.view_layer.objects if obj.select_get()} == selection_before,
        "Missing MaterialX conversion did not restore the original selection")
require(bpy.context.view_layer.objects.active is active_before,
        "Missing MaterialX conversion did not restore the original active object")
require(set(Path(tempfile.gettempdir()).glob("tmp*.usda")) == temporary_usda_before,
        "Missing MaterialX conversion left a temporary USDA file")
require({text.name for text in bpy.data.texts} == source_texts_before,
        "Missing MaterialX conversion created a USDShade Text datablock")
require(not {key for key in material.keys() if key.startswith("materialx_authoring.")},
        "Missing MaterialX conversion wrote an authority contract")

source_texts_before = {text.name for text in bpy.data.texts}
cancel_context = ConfirmContext(CancelWindowManager())
result = cycles.operators.CYCLES_OT_materialx_convert_authority.invoke(
    ReportingOperator(), cancel_context, object())
require(result == {"CANCELLED"} and cancel_context.window_manager.calls == 1,
        "Conversion cancellation was not returned by Blender's confirmation dialog")
actual_keys = {key for key in material.keys() if key.startswith("materialx_authoring.")}
require(not actual_keys, "Cancelled conversion wrote an authority contract")
require({text.name for text in bpy.data.texts} == source_texts_before,
        "Cancelled conversion created a USDShade Text datablock")

confirm_context = ConfirmContext(None)
confirm_context.window_manager = ConfirmWindowManager(confirm_context)
result = cycles.operators.CYCLES_OT_materialx_convert_authority.invoke(
    ReportingOperator(), confirm_context, object())
require(result == {"FINISHED"} and confirm_context.window_manager.calls == 1,
        "Confirmed conversion did not delegate to execute exactly once")

actual_keys = {key for key in material.keys() if key.startswith("materialx_authoring.")}
require(actual_keys == AUTHORITY_KEYS, "Conversion did not write exactly the authority contract")
require(material["unrelated_property"] == "preserve me", "Conversion mutated unrelated properties")
require(material["materialx_authoring.cycles_native_authority"] is True,
        "Conversion did not enable native authority")
require(cycles.ui.materialx_authority_status(material) == 'active',
        "MaterialX authoring panel did not report an active authority")

document_uuid = material["materialx_authoring.document_uuid"]
uuid.UUID(document_uuid)
text_name = material["materialx_authoring.document_usda_text_name"]
require(text_name == ".materialx_usdshade_" + document_uuid,
        "Authority Text name is not bound to the document UUID")
source = bpy.data.texts.get(text_name)
require(source is not None, "Conversion did not create the authority Text datablock")
usda = source.as_string()
require(usda.startswith("#usda 1.0\n") and "ND_open_pbr_surface_surfaceshader" in usda,
        "Conversion did not capture native MaterialX USDShade source")
require("inputs:roughness" in usda and "0.23" in usda,
        "Captured USDShade source lost the nontrivial Principled roughness")
require(material["materialx_authoring.document_digest"] ==
        "sha256:" + hashlib.sha256(usda.encode("utf-8")).hexdigest(),
        "Authority digest is not bound to exact Text bytes")
require(material["materialx_authoring.document_material_path"] ==
        "/root/_materials/ConvertedMaterialXAuthority",
        "Conversion did not bind the exported material path: " +
        material["materialx_authoring.document_material_path"])

result = bpy.ops.cycles.materialx_disable_authority()
require(result == {"FINISHED"}, "Disable authority operator did not finish")
actual_keys = {key for key in material.keys() if key.startswith("materialx_authoring.")}
require(not actual_keys, "Disable authority did not clear the authority contract")
require(bpy.data.texts.get(text_name) is source,
        "Disable authority should preserve the authored USDShade Text datablock")

print("MATERIALX_CONVERSION_AUTHORITY_OPERATOR_PASS")
