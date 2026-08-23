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

    bpy.ops.object.light_add(type="AREA", location=(1.2, 0.8, 2.0))
    light = bpy.context.object
    light.data.energy = 450.0
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


def _remove_non_tetcage_meshes() -> None:
    for obj in list(bpy.context.scene.objects):
        if obj.type == "MESH" and not obj.get("tetcage_native_candidate"):
            bpy.data.objects.remove(obj, do_unlink=True)


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
            "-- asset output.exr result.json [tet_only|mixed]"
        )

    asset, output_image, output_json = args[:3]
    probe_mode = args[3] if len(args) == 4 else "tet_only"
    if probe_mode not in {"tet_only", "mixed"}:
        raise RuntimeError("probe mode must be tet_only or mixed")
    mixed_scene = probe_mode == "mixed"
    payload = import_asset(asset)
    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    if not mixed_scene:
        _remove_non_tetcage_meshes()
    compute_device_type = _enable_metal(scene)
    _use_probe_emission_material(mixed_scene)
    _use_black_world(scene)
    scene.cycles.samples = 1
    scene.cycles.use_adaptive_sampling = False
    scene.cycles.use_denoising = False
    scene.cycles.seed = 0
    scene.cycles.use_animated_seed = False
    scene.cycles.max_bounces = 1
    scene.cycles.diffuse_bounces = 0
    scene.cycles.glossy_bounces = 0
    scene.cycles.transmission_bounces = 0
    scene.cycles.volume_bounces = 0
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
