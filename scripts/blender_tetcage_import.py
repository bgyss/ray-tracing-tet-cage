"""Blender-side import and debug visualization for a compiled tet-cage asset.

The importer deliberately creates ordinary triangle geometry and a wireframe
cage.  This is the safe Blender fallback while the procedural Cycles primitive
is still being integrated; it does not pretend to activate tet-cage MetalRT.
"""

from __future__ import annotations

import json
import math
import pathlib
import sys
from typing import Any

import bpy
from mathutils import Matrix

from tetcage_asset import load_asset, micro_triangle_points


_IMPORTED_ASSETS: dict[str, dict[str, Any]] = {}


def _tet_matrix(asset: dict[str, Any], cage_vertices: list[dict[str, Any]], tet_id: int) -> Any:
    tet = asset["cage_tetrahedra"][tet_id]
    p0, p1, p2, p3 = (cage_vertices[index]["position"] for index in tet)
    matrix = Matrix(
        (
            (p1[0] - p0[0], p2[0] - p0[0], p3[0] - p0[0], p0[0]),
            (p1[1] - p0[1], p2[1] - p0[1], p3[1] - p0[1], p0[1]),
            (p1[2] - p0[2], p2[2] - p0[2], p3[2] - p0[2], p0[2]),
            (0.0, 0.0, 0.0, 1.0),
        )
    )
    determinant = matrix.to_3x3().determinant()
    if not math.isfinite(determinant) or determinant <= 1.0e-12:
        raise ValueError(f"tet {tet_id} is mirrored or near singular")
    return matrix


def _surface_objects(
    asset: dict[str, Any], cage_vertices: list[dict[str, Any]] | None = None
) -> list[bpy.types.Object]:
    vertices_by_tet: dict[int, list[tuple[float, float, float]]] = {}
    indices_by_tet: dict[int, dict[int, int]] = {}
    faces_by_tet: dict[int, list[tuple[int, int, int]]] = {}
    source_by_tet: dict[int, list[int]] = {}
    owner_by_tet: dict[int, list[int]] = {}
    materials_by_tet: dict[int, list[int]] = {}
    for triangle in asset["micro_triangles"]:
        tet_id = triangle["tet_id"]
        vertices = vertices_by_tet.setdefault(tet_id, [])
        indices = indices_by_tet.setdefault(tet_id, {})
        face: list[int] = []
        for vertex_index in triangle["vertex_indices"]:
            mesh_index = indices.get(vertex_index)
            if mesh_index is None:
                barycentric = asset["generated_vertices"][vertex_index]["cage_barycentric"]
                mesh_index = len(vertices)
                vertices.append((barycentric[1], barycentric[2], barycentric[3]))
                indices[vertex_index] = mesh_index
            face.append(mesh_index)
        faces_by_tet.setdefault(tet_id, []).append(tuple(face))
        source_by_tet.setdefault(tet_id, []).append(triangle["source_primitive"])
        owner_by_tet.setdefault(tet_id, []).append(triangle["owner_tet"])
        materials_by_tet.setdefault(tet_id, []).append(triangle["material"])

    surfaces: list[bpy.types.Object] = []
    pose = cage_vertices if cage_vertices is not None else asset["cage_vertices"]
    for tet_id in sorted(vertices_by_tet):
        vertices = vertices_by_tet[tet_id]
        faces = faces_by_tet[tet_id]
        mesh = bpy.data.meshes.new(f"TetCage_Tet_{tet_id:04d}")
        mesh.from_pydata(vertices, [], faces)
        mesh.update()
        primitive_attribute = mesh.attributes.new("tetcage_source_primitive", "INT", "FACE")
        owner_attribute = mesh.attributes.new("tetcage_owner_tet", "INT", "FACE")
        material_attribute = mesh.attributes.new("tetcage_material", "INT", "FACE")
        for index, polygon in enumerate(mesh.polygons):
            primitive_attribute.data[index].value = source_by_tet[tet_id][index]
            owner_attribute.data[index].value = owner_by_tet[tet_id][index]
            material_attribute.data[index].value = materials_by_tet[tet_id][index]

        obj = bpy.data.objects.new(f"TetCage_Tet_{tet_id:04d}", mesh)
        bpy.context.collection.objects.link(obj)
        obj.matrix_world = _tet_matrix(asset, pose, tet_id)
        obj["tetcage_fallback_mode"] = "conventional_mesh"
        obj["tetcage_render_mode"] = "per_tet_triangle_instances"
        obj["tetcage_tet_id"] = tet_id
        obj["tetcage_generated_triangles"] = len(faces)
        obj["tetcage_source_triangles"] = len(asset["source_triangles"])
        surfaces.append(obj)
    return surfaces


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


def _ensure_surface_material() -> bpy.types.Material:
    material = bpy.data.materials.get("TetCage_Fallback_Material")
    if material is None:
        material = bpy.data.materials.new("TetCage_Fallback_Material")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    if principled is not None:
        principled.inputs["Base Color"].default_value = (0.15, 0.45, 0.8, 1.0)
        principled.inputs["Roughness"].default_value = 0.35
    return material


def update_surface_from_cage(cage: bpy.types.Object) -> bool:
    """Update fallback surface vertices after a cage edit.

    The handler intentionally updates only the compiled micro-triangle surface
    representation. It is a debug/fallback path, not the future procedural
    MetalRT update contract.
    """

    asset_path = str(cage.get("tetcage_asset_path", ""))
    asset = _IMPORTED_ASSETS.get(asset_path)
    if asset is None:
        return False
    posed_vertices = [
        {"position": tuple(vertex.co), "stable_id": 0}
        for vertex in cage.data.vertices
    ]
    surfaces = [
        obj
        for obj in bpy.data.objects
        if obj.get("tetcage_asset_path") == asset_path and obj.get("tetcage_tet_id") is not None
    ]
    if not surfaces:
        return False
    generation = int(cage.get("tetcage_pose_generation", 0)) + 1
    try:
        transforms = {
            int(surface["tetcage_tet_id"]): _tet_matrix(
                asset, posed_vertices, int(surface["tetcage_tet_id"])
            )
            for surface in surfaces
        }
    except ValueError:
        cage["tetcage_fallback_reason"] = "invalid_pose"
        return False
    for surface in surfaces:
        surface.matrix_world = transforms[int(surface["tetcage_tet_id"])]
        surface["tetcage_pose_generation"] = generation
    cage["tetcage_pose_generation"] = generation
    cage["tetcage_fallback_reason"] = "none"
    return True


def _depsgraph_update(_scene: bpy.types.Scene, depsgraph: bpy.types.Depsgraph) -> None:
    for update in depsgraph.updates:
        obj = update.id
        if isinstance(obj, bpy.types.Object) and obj.get("tetcage_debug_view"):
            update_surface_from_cage(obj)


def register_handlers() -> None:
    if _depsgraph_update not in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.append(_depsgraph_update)


def import_asset(
    asset_path: str | pathlib.Path,
    output_json: str | pathlib.Path | None = None,
    output_blend: str | pathlib.Path | None = None,
) -> dict[str, Any]:
    asset = load_asset(asset_path)
    scene = bpy.context.scene
    if not scene.render.engine.startswith("CYCLES"):
        scene.render.engine = "CYCLES"
    surfaces = _surface_objects(asset)
    cage = _cage_object(asset)
    material = _ensure_surface_material()
    for surface in surfaces:
        surface.data.materials.append(material)
    asset_path = str(pathlib.Path(asset_path).resolve())
    _IMPORTED_ASSETS[asset_path] = asset
    cage["tetcage_asset_path"] = asset_path
    cage["tetcage_fallback_reason"] = "none"
    for surface in surfaces:
        surface["tetcage_asset_path"] = asset_path
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
        "surface_objects": [surface.name for surface in surfaces],
        "cage_object": cage.name,
        "tet_objects": len(surfaces),
        "render_mode": "per_tet_triangle_instances",
        "procedural_metalrt": False,
        "pose_update_handler": True,
    }
    register_handlers()
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
