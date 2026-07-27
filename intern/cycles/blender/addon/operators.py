# SPDX-FileCopyrightText: 2011-2022 Blender Foundation
#
# SPDX-License-Identifier: Apache-2.0

from __future__ import annotations

import bpy
import hashlib
from pathlib import Path
import tempfile
import uuid
from bpy.types import Operator
from bpy.props import StringProperty

from bpy.app.translations import pgettext_tip as tip_


class CYCLES_OT_use_shading_nodes(Operator):
    """Enable nodes on a light"""
    bl_idname = "cycles.use_shading_nodes"
    bl_label = "Use Nodes"

    @classmethod
    def poll(cls, context):
        return getattr(context, "light", False)

    def execute(self, context):
        if context.light:
            context.light.use_nodes = True

        return {'FINISHED'}


class CYCLES_OT_denoise_animation(Operator):
    "Denoise rendered animation sequence using current scene and view " \
        "layer settings. Requires denoising data passes and output to " \
        "OpenEXR multilayer files"
    bl_idname = "cycles.denoise_animation"
    bl_label = "Denoise Animation"

    input_filepath: StringProperty(
        name='Input Filepath',
        description='File path for image to denoise. If not specified, uses the render file path and frame range from the scene',
        default='',
        subtype='FILE_PATH')

    output_filepath: StringProperty(
        name='Output Filepath',
        description='If not specified, renders will be denoised in-place',
        default='',
        subtype='FILE_PATH')

    def execute(self, context):
        import os

        preferences = context.preferences
        scene = context.scene
        view_layer = context.view_layer

        in_filepath = self.input_filepath
        out_filepath = self.output_filepath

        in_filepaths = []
        out_filepaths = []

        if in_filepath != '':
            # Denoise a single file
            if out_filepath == '':
                out_filepath = in_filepath

            in_filepaths.append(in_filepath)
            out_filepaths.append(out_filepath)
        else:
            # Denoise animation sequence with expanded frames matching
            # Blender render output file naming.
            in_filepath = scene.render.filepath
            if out_filepath == '':
                out_filepath = in_filepath

            # Backup since we will overwrite the scene path temporarily
            original_filepath = scene.render.filepath

            for frame in range(scene.frame_start, scene.frame_end + 1):
                scene.render.filepath = in_filepath
                filepath = scene.render.frame_path(frame=frame)
                in_filepaths.append(filepath)

                if not os.path.isfile(filepath):
                    scene.render.filepath = original_filepath
                    err_msg = tip_("Frame '%s' not found, animation must be complete") % filepath
                    self.report({'ERROR'}, err_msg)
                    return {'CANCELLED'}

                scene.render.filepath = out_filepath
                filepath = scene.render.frame_path(frame=frame)
                out_filepaths.append(filepath)

            scene.render.filepath = original_filepath

        # Run denoiser
        # TODO: support cancel and progress reports.
        import _cycles
        try:
            _cycles.denoise(preferences.as_pointer(),
                            scene.as_pointer(),
                            view_layer.as_pointer(),
                            input=in_filepaths,
                            output=out_filepaths)
        except Exception as e:
            self.report({'ERROR'}, str(e))
            return {'FINISHED'}

        self.report({'INFO'}, "Denoising completed")
        return {'FINISHED'}


class CYCLES_OT_merge_images(Operator):
    "Combine OpenEXR multi-layer images rendered with different sample " \
        "ranges into one image with reduced noise"
    bl_idname = "cycles.merge_images"
    bl_label = "Merge Images"

    input_filepath1: StringProperty(
        name='Input Filepath',
        description='File path for image to merge',
        default='',
        subtype='FILE_PATH')

    input_filepath2: StringProperty(
        name='Input Filepath',
        description='File path for image to merge',
        default='',
        subtype='FILE_PATH')

    output_filepath: StringProperty(
        name='Output Filepath',
        description='File path for merged image',
        default='',
        subtype='FILE_PATH')

    def execute(self, context):
        in_filepaths = [self.input_filepath1, self.input_filepath2]
        out_filepath = self.output_filepath

        import _cycles
        try:
            _cycles.merge(input=in_filepaths, output=out_filepath)
        except Exception as e:
            self.report({'ERROR'}, str(e))
            return {'FINISHED'}

        return {'FINISHED'}


MATERIALX_AUTHORITY_KEYS = (
    "materialx_authoring.cycles_native_authority",
    "materialx_authoring.document_uuid",
    "materialx_authoring.document_digest",
    "materialx_authoring.document_usda_text_name",
    "materialx_authoring.document_material_path",
)


def materialx_authority_material(context):
    material = getattr(context, "material", None)
    if material is None and context.object is not None:
        material = context.object.active_material
    return material


def materialx_authority_export_object(context, material):
    obj = context.object
    if obj is None or obj.active_material is not material:
        raise RuntimeError("The active object must use the material being converted")

    assigned_materials = {slot.material for slot in obj.material_slots if slot.material is not None}
    if assigned_materials != {material}:
        raise RuntimeError("The active object must use only the material being converted")

    return obj


def materialx_authority_export_usda(context, material):
    from pxr import Usd, UsdShade

    obj = materialx_authority_export_object(context, material)
    view_layer = context.view_layer
    selected_objects = [candidate for candidate in view_layer.objects if candidate.select_get()]
    active_object = view_layer.objects.active

    with tempfile.NamedTemporaryFile(suffix=".usda", delete=False) as temporary:
        filepath = temporary.name

    try:
        for candidate in view_layer.objects:
            candidate.select_set(False)
        obj.select_set(True)
        view_layer.objects.active = obj

        result = bpy.ops.wm.usd_export(
            filepath=filepath,
            selected_objects_only=True,
            export_animation=False,
            export_materials=True,
            generate_preview_surface=False,
            generate_materialx_network=True,
            export_textures_mode='KEEP')
        if result != {'FINISHED'}:
            raise RuntimeError("USD export did not finish")

        usda = Path(filepath).read_text(encoding="utf-8")
        stage = Usd.Stage.Open(filepath)
        if stage is None:
            raise RuntimeError("USD export did not produce a readable stage")
        materials = [UsdShade.Material(prim) for prim in stage.Traverse()
                     if prim.IsA(UsdShade.Material)]
        if len(materials) != 1:
            raise RuntimeError("USD export did not produce exactly one material")
        material = materials[0]
        mtlx_surface = material.GetOutput("mtlx:surface")
        if not mtlx_surface or not mtlx_surface.GetAttr().GetConnections():
            raise RuntimeError("USD export did not produce a MaterialX surface network")
        material_path = material.GetPath().pathString
        return usda, material_path
    finally:
        for candidate in view_layer.objects:
            candidate.select_set(False)
        for candidate in selected_objects:
            candidate.select_set(True)
        view_layer.objects.active = active_object
        Path(filepath).unlink(missing_ok=True)


class CYCLES_OT_materialx_convert_authority(Operator):
    """Create MaterialX USDShade authority by exporting the active material"""
    bl_idname = "cycles.materialx_convert_authority"
    bl_label = "Convert Material to MaterialX Authority"

    @classmethod
    def poll(cls, context):
        material = materialx_authority_material(context)
        return material is not None and not material.get(MATERIALX_AUTHORITY_KEYS[0], False)

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def draw(self, context):
        self.layout.label(text="This creates a MaterialX USDShade authority for this material.")
        self.layout.label(text="The existing node tree is preserved.")

    def execute(self, context):
        material = materialx_authority_material(context)
        if material is None:
            self.report({'ERROR'}, "No material is available for conversion")
            return {'CANCELLED'}

        previous = {key: material.get(key) for key in MATERIALX_AUTHORITY_KEYS}
        present = {key: key in material for key in MATERIALX_AUTHORITY_KEYS}
        source = None
        try:
            usda, material_path = materialx_authority_export_usda(context, material)
            document_uuid = str(uuid.uuid4())
            text_name = ".materialx_usdshade_" + document_uuid
            source = bpy.data.texts.new(text_name)
            source.write(usda)
            authority = {
                "materialx_authoring.cycles_native_authority": True,
                "materialx_authoring.document_uuid": document_uuid,
                "materialx_authoring.document_digest": "sha256:" + hashlib.sha256(
                    source.as_string().encode("utf-8")).hexdigest(),
                "materialx_authoring.document_usda_text_name": text_name,
                "materialx_authoring.document_material_path": material_path,
            }
            for key, value in authority.items():
                material[key] = value
        except Exception as error:
            if source is not None:
                bpy.data.texts.remove(source)
            for key, value in previous.items():
                if present[key]:
                    material[key] = value
                elif key in material:
                    del material[key]
            self.report({'ERROR'}, str(error))
            return {'CANCELLED'}

        return {'FINISHED'}


class CYCLES_OT_materialx_disable_authority(Operator):
    """Disable the MaterialX USDShade authority for this material"""
    bl_idname = "cycles.materialx_disable_authority"
    bl_label = "Disable MaterialX Authority"

    @classmethod
    def poll(cls, context):
        material = materialx_authority_material(context)
        return material is not None and material.get(MATERIALX_AUTHORITY_KEYS[0], False)

    def execute(self, context):
        material = materialx_authority_material(context)
        for key in MATERIALX_AUTHORITY_KEYS:
            if key in material:
                del material[key]
        return {'FINISHED'}


classes = (
    CYCLES_OT_use_shading_nodes,
    CYCLES_OT_denoise_animation,
    CYCLES_OT_merge_images,
    CYCLES_OT_materialx_convert_authority,
    CYCLES_OT_materialx_disable_authority,
)


def register():
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)


def unregister():
    from bpy.utils import unregister_class
    for cls in classes:
        unregister_class(cls)
