"""Render a version-1 tet-cage candidate through Blender's opt-in native Cycles path.

The native path is enabled by the process environment variable
``CYCLES_TETCAGE_NATIVE=1``. Without it, the same scene remains on the ordinary
triangle fallback, which makes this script useful for an image differential.
"""

from __future__ import annotations

import json
import os
import pathlib
import sys

import bpy
from mathutils import Vector

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from blender_tetcage_import import import_asset


def _camera_and_light(scene: bpy.types.Scene) -> None:
    bpy.ops.object.camera_add(location=(0.5, 0.5, 2.5))
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 1.0
    camera.rotation_euler = (Vector((0.5, 0.5, 0.0)) - camera.location).to_track_quat(
        "-Z", "Y"
    ).to_euler()
    scene.camera = camera

    light_type = os.environ.get("TETCAGE_LIGHT_MODE", "area").upper()
    if light_type not in {"AREA", "SUN", "POINT"}:
        light_type = "AREA"
    bpy.ops.object.light_add(type=light_type, location=(1.2, 0.8, 2.0))
    light = bpy.context.object
    light.data.energy = 2.0 if light_type == "SUN" else (100.0 if light_type == "POINT" else 450.0)
    if light_type == "AREA":
        light.data.shape = "DISK"
        light.data.size = 2.0
    light.rotation_euler = (Vector((0.3, 0.3, 0.0)) - light.location).to_track_quat(
        "-Z", "Y"
    ).to_euler()


def _enable_metal(scene: bpy.types.Scene) -> str:
    scene.cycles.device = "GPU"
    addon = bpy.context.preferences.addons.get("cycles")
    if addon is None:
        raise RuntimeError("Cycles preferences are unavailable")
    preferences = addon.preferences
    preferences.compute_device_type = "METAL"
    preferences.get_devices()
    devices = preferences.devices
    for device in devices:
        device.use = device.type == "METAL"
    if not any(device.use and device.type == "METAL" for device in devices):
        raise RuntimeError("Metal device was not exposed by Cycles preferences")
    return preferences.compute_device_type


def _use_probe_emission_material(mixed_scene: bool) -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Emission")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    emission.inputs["Strength"].default_value = 1.0
    links.new(emission.outputs["Emission"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and (mixed_scene or obj.get("tetcage_native_candidate")):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_transparent_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Transparent")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    transparent = nodes.new("ShaderNodeBsdfTransparent")
    emission = nodes.new("ShaderNodeEmission")
    emission.inputs["Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    emission.inputs["Strength"].default_value = 1.0
    mix = nodes.new("ShaderNodeMixShader")
    mix.inputs[0].default_value = 0.5
    links.new(transparent.outputs["BSDF"], mix.inputs[1])
    links.new(emission.outputs["Emission"], mix.inputs[2])
    links.new(mix.outputs[0], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_local_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Subsurface")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    if principled is None:
        raise RuntimeError("Blender did not expose a Principled BSDF node")
    principled.inputs["Base Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    principled.inputs["Roughness"].default_value = 0.25
    principled.inputs["Subsurface Weight"].default_value = 1.0
    principled.inputs["Subsurface Radius"].default_value = (1.0, 0.3, 0.1)
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_ao_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_AO")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    emission = nodes.new("ShaderNodeEmission")
    ao = nodes.new("ShaderNodeAmbientOcclusion")
    ao.inputs["Distance"].default_value = 1.0
    ao.inputs["Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    links.new(ao.outputs["Color"], emission.inputs["Color"])
    links.new(emission.outputs["Emission"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_normal_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Normal")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    geometry = nodes.new("ShaderNodeNewGeometry")
    scale = nodes.new("ShaderNodeVectorMath")
    scale.operation = "SCALE"
    scale.inputs["Scale"].default_value = 0.5
    add = nodes.new("ShaderNodeVectorMath")
    add.operation = "ADD"
    add.inputs[1].default_value = (0.5, 0.5, 0.5)
    emission = nodes.new("ShaderNodeEmission")
    links.new(geometry.outputs["Normal"], scale.inputs[0])
    links.new(scale.outputs[0], add.inputs[0])
    links.new(add.outputs[0], emission.inputs["Color"])
    links.new(emission.outputs["Emission"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_position_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Position")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    geometry = nodes.new("ShaderNodeNewGeometry")
    emission = nodes.new("ShaderNodeEmission")
    links.new(geometry.outputs["Position"], emission.inputs["Color"])
    links.new(emission.outputs["Emission"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_uv_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_UV")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    uv_map = nodes.new("ShaderNodeUVMap")
    uv_map.uv_map = "UVMap"
    emission = nodes.new("ShaderNodeEmission")
    links.new(uv_map.outputs["UV"], emission.inputs["Color"])
    links.new(emission.outputs["Emission"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_diffuse_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Diffuse")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    diffuse = nodes.new("ShaderNodeBsdfDiffuse")
    diffuse.inputs["Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    diffuse.inputs["Roughness"].default_value = 0.25
    links.new(diffuse.outputs["BSDF"], output.inputs["Surface"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _use_volume_probe_material() -> None:
    material = bpy.data.materials.new("TetCage_Native_Probe_Volume")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()
    output = nodes.new("ShaderNodeOutputMaterial")
    volume = nodes.new("ShaderNodeVolumePrincipled")
    volume.inputs["Density"].default_value = 0.8
    volume.inputs["Color"].default_value = (0.7, 0.15, 0.03, 1.0)
    volume.inputs["Anisotropy"].default_value = 0.0
    links.new(volume.outputs["Volume"], output.inputs["Volume"])
    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.data.materials.clear()
            obj.data.materials.append(material)


def _remove_non_tetcage_meshes() -> None:
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH" and not obj.get("tetcage_native_candidate"):
            bpy.data.objects.remove(obj, do_unlink=True)


def _setup_motion(scene: bpy.types.Scene, delta: float) -> None:
    scene.render.use_motion_blur = True
    scene.render.motion_blur_shutter = 0.5
    scene.frame_start = 1
    scene.frame_end = 2
    scene.frame_set(1)
    for obj in scene.objects:
        if obj.type == "MESH" and obj.get("tetcage_native_candidate"):
            obj.keyframe_insert(data_path="location", frame=1)
            obj.location.x += delta
            obj.keyframe_insert(data_path="location", frame=2)
    scene.frame_set(1)


def _use_black_world(scene: bpy.types.Scene) -> None:
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs["Color"].default_value = (0.0, 0.0, 0.0, 1.0)
        background.inputs["Strength"].default_value = 0.0
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0


def main() -> int:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) not in {3, 4}:
        raise RuntimeError(
            "usage: blender --python blender_native_tetcage_render_probe.py "
            "-- asset output.exr result.json [tet_only|mixed|motion|mixed_motion|transparent|local|ao|normal|position|uv|diffuse|volume]"
        )

    asset, output_image, output_json = args[:3]
    probe_mode = args[3] if len(args) == 4 else "tet_only"
    if probe_mode not in {
        "tet_only",
        "mixed",
        "motion",
        "mixed_motion",
        "transparent",
        "local",
        "ao",
        "normal",
        "position",
        "uv",
        "diffuse",
        "volume",
    }:
        raise RuntimeError(
            "probe mode must be tet_only, mixed, motion, mixed_motion, transparent, local, ao, normal, position, uv, diffuse, or volume"
        )
    mixed_scene = probe_mode in {"mixed", "mixed_motion"}
    payload = import_asset(asset)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    if not mixed_scene:
        _remove_non_tetcage_meshes()
    compute_device_type = _enable_metal(scene)
    if probe_mode == "transparent":
        _use_transparent_probe_material()
    elif probe_mode == "local":
        _use_local_probe_material()
    elif probe_mode == "ao":
        _use_ao_probe_material()
    elif probe_mode == "normal":
        _use_normal_probe_material()
    elif probe_mode == "position":
        _use_position_probe_material()
    elif probe_mode == "uv":
        _use_uv_probe_material()
    elif probe_mode == "diffuse":
        _use_diffuse_probe_material()
    elif probe_mode == "volume":
        _use_volume_probe_material()
    else:
        _use_probe_emission_material(mixed_scene)
    _use_black_world(scene)
    if probe_mode in {"motion", "mixed_motion"}:
        _setup_motion(scene, float(os.environ.get("TETCAGE_MOTION_DELTA", "0.0")))
    scene.cycles.samples = int(os.environ.get("TETCAGE_SAMPLES", "1"))
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = False
    scene.cycles.seed = 0
    scene.cycles.use_animated_seed = False
    scene.cycles.max_bounces = 4 if probe_mode in {"transparent", "local"} else 1
    scene.cycles.diffuse_bounces = 0
    scene.cycles.glossy_bounces = 0
    scene.cycles.transmission_bounces = 0
    scene.cycles.volume_bounces = 1 if probe_mode == "volume" else 0
    scene.cycles.transparent_max_bounces = 8
    scene.render.resolution_x = 16
    scene.render.resolution_y = 16
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.filepath = str(pathlib.Path(output_image).resolve())
    scene.world.color = (0.02, 0.02, 0.02)
    _camera_and_light(scene)

    bpy.ops.render.render(write_still=True)
    result = {
        "schema_version": 1,
        "kind": "blender_native_tetcage_render",
        "status": "passed",
        "render_engine": scene.render.engine,
        "cycles_device": scene.cycles.device,
        "compute_device_type": compute_device_type,
        "native_gate_requested": os.environ.get("CYCLES_TETCAGE_NATIVE") == "1",
        "native_candidate": payload.get("native_candidate", False),
        "native_contract": payload.get("native_contract"),
        "scene_mode": "mixed" if mixed_scene else "tet_only",
        "probe_mode": probe_mode,
        "resolution": "16x16",
        "samples": 1,
        "output": str(pathlib.Path(output_image).resolve()),
    }
    pathlib.Path(output_json).write_text(json.dumps(result, sort_keys=True) + "\n")
    print("TETCAGE_BLENDER_NATIVE_RENDER_OK " + json.dumps(result, sort_keys=True), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
