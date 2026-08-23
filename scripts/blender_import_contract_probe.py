"""Run the tet-cage Blender import contract inside Blender's Python runtime."""

from __future__ import annotations

import json
import pathlib
import sys

import bpy

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from blender_tetcage_import import import_asset, update_surface_from_cage


def main() -> int:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 3:
        raise RuntimeError("usage: blender --python blender_import_contract_probe.py -- asset output blend")
    asset, output, blend = args
    payload = import_asset(asset, output, blend)
    cage = bpy.data.objects["TetCage_Debug_Cage"]
    surface = bpy.data.objects["TetCage_Tet_0000"]
    assert surface.get("tetcage_native_candidate") is True
    assert surface.data.get("tetcage_native_candidate") is True
    assert surface.data.uv_layers.get("UVMap") is not None
    before = tuple(surface.matrix_world.translation)
    cage.data.vertices[0].co.x += 0.1
    assert update_surface_from_cage(cage)
    payload["pose_updated"] = before != tuple(surface.matrix_world.translation)
    bpy.ops.wm.save_as_mainfile(filepath=blend)
    cage.data.vertices[1].co.x = -1.0
    payload["invalid_pose_rejected"] = not update_surface_from_cage(cage)
    payload["invalid_pose_reason"] = cage.get("tetcage_fallback_reason")
    pathlib.Path(output).write_text(json.dumps(payload, sort_keys=True) + "\n")
    print("TETCAGE_BLENDER_IMPORT_PROBE_OK", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
