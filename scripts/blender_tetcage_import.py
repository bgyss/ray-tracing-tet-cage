"""Blender-side import and debug visualization for a compiled tet-cage asset.

The importer deliberately creates ordinary triangle geometry and a wireframe
cage.  This is the safe Blender fallback while the procedural Cycles primitive
is still being integrated; it does not pretend to activate tet-cage MetalRT.
"""

from __future__ import annotations

import json
import pathlib
import sys
from typing import Any

import bpy

from tetcage_asset import load_asset, micro_triangle_points


def _surface_object(asset: dict[str, Any]) -> bpy.types.Object:
    vertices: list[tuple[float, float, float]] = []
    faces: list[tuple[int, int, int]] = []
    source_primitives: list[int] = []
    owner_tets: list[int] = []
    materials: list[int] = []
    for triangle in asset["micro_triangles"]:
        start = len(vertices)
        vertices.extend(micro_triangle_points(asset, triangle))
        faces.append((start, start + 1, start + 2))
        source_primitives.append(triangle["source_primitive"])
        owner_tets.append(triangle["owner_tet"])
        materials.append(triangle["material"])

    mesh = bpy.data.meshes.new("TetCage_Fallback_Surface")
    mesh.from_pydata(vertices, [], faces)
    mesh.update()
    primitive_attribute = mesh.attributes.new("tetcage_source_primitive", "INT", "FACE")
    owner_attribute = mesh.attributes.new("tetcage_owner_tet", "INT", "FACE")
    material_attribute = mesh.attributes.new("tetcage_material", "INT", "FACE")
    for index, polygon in enumerate(mesh.polygons):
        primitive_attribute.data[index].value = source_primitives[index]
        owner_attribute.data[index].value = owner_tets[index]
        material_attribute.data[index].value = materials[index]

    obj = bpy.data.objects.new("TetCage_Fallback_Surface", mesh)
    bpy.context.collection.objects.link(obj)
    obj["tetcage_fallback_mode"] = "conventional_mesh"
    obj["tetcage_generated_triangles"] = len(faces)
    obj["tetcage_source_triangles"] = len(asset["source_triangles"])
    return obj


def _cage_object(asset: dict[str, Any]) -> bpy.types.Object:
    vertices = [vertex["position"] for vertex in asset["cage_vertices"]]
    edges: set[tuple[int, int]] = set()
    for tet in asset["cage_tetrahedra"]:
        for first in range(4):
            for second in range(first + 1, 4):
                edges.add(tuple(sorted((tet[first], tet[second]))))
    mesh = bpy.data.meshes.new("TetCage_Debug_Cage")
    mesh.from_pydata(vertices, sorted(edges), [])
    mesh.update()
    obj = bpy.data.objects.new("TetCage_Debug_Cage", mesh)
    bpy.context.collection.objects.link(obj)
    obj.display_type = "WIRE"
    obj.show_in_front = True
    obj["tetcage_debug_view"] = True
    obj["tetcage_tetrahedra"] = len(asset["cage_tetrahedra"])
    return obj


def import_asset(
    asset_path: str | pathlib.Path,
    output_json: str | pathlib.Path | None = None,
    output_blend: str | pathlib.Path | None = None,
) -> dict[str, Any]:
    asset = load_asset(asset_path)
    scene = bpy.context.scene
    if not scene.render.engine.startswith("CYCLES"):
        scene.render.engine = "CYCLES"
    surface = _surface_object(asset)
    cage = _cage_object(asset)
    surface.parent = cage
    scene["tetcage_fallback_mode"] = "conventional_mesh"
    scene["tetcage_asset_format_version"] = asset["format_version"]
    scene["tetcage_source_triangles"] = len(asset["source_triangles"])
    scene["tetcage_generated_triangles"] = len(asset["micro_triangles"])
    scene["tetcage_cage_tetrahedra"] = len(asset["cage_tetrahedra"])
    payload = {
        "schema_version": 1,
        "kind": "blender_tetcage_import",
        "status": "passed",
        "fallback_mode": "conventional_mesh",
        "render_engine": scene.render.engine,
        "surface_triangles": len(asset["micro_triangles"]),
        "cage_tetrahedra": len(asset["cage_tetrahedra"]),
        "surface_object": surface.name,
        "cage_object": cage.name,
        "procedural_metalrt": False,
    }
    if output_json is not None:
        pathlib.Path(output_json).write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
    if output_blend is not None:
        bpy.ops.wm.save_as_mainfile(filepath=str(output_blend))
    print("TETCAGE_BLENDER_IMPORT_OK " + json.dumps(payload, sort_keys=True), flush=True)
    return payload


def _main() -> int:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) < 2 or len(args) > 3:
        raise RuntimeError("usage: blender --python blender_tetcage_import.py -- asset.tetcage output.json [output.blend]")
    import_asset(args[0], args[1], args[2] if len(args) == 3 else None)
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
