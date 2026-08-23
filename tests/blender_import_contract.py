#!/usr/bin/env python3
"""Run the Blender-side tet-cage import contract in background mode."""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]


def main() -> int:
    blender = pathlib.Path(
        os.environ.get(
            "BLENDER_BINARY",
            "/Users/briangyss/src/build_blender_tetcage_debug_make/bin/Blender.app/Contents/MacOS/Blender",
        )
    )
    compiler = pathlib.Path(
        os.environ.get("TETCAGE_ASSET_COMPILER_BIN", str(REPO_ROOT / "build/dev/tetcage_asset_compiler"))
    )
    with tempfile.TemporaryDirectory(prefix="tetcage-blender-import-") as raw:
        root = pathlib.Path(raw)
        asset = root / "one-tet.tetcage"
        output = root / "import.json"
        blend = root / "one-tet.blend"
        config = root / "config"
        scripts = root / "scripts"
        config.mkdir()
        scripts.mkdir()
        subprocess.run(
            [
                str(compiler),
                str(REPO_ROOT / "tests/assets/one-tet.obj"),
                str(REPO_ROOT / "tests/assets/one-tet.cage"),
                str(asset),
            ],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        expression = (
            "import sys; "
            f"sys.path.insert(0, {str(REPO_ROOT / 'scripts')!r}); "
            "from blender_tetcage_import import import_asset, update_surface_from_cage; "
            f"payload=import_asset({str(asset)!r}, {str(output)!r}, {str(blend)!r}); "
            "import bpy, json; "
            "cage=bpy.data.objects['TetCage_Debug_Cage']; "
            "surface=bpy.data.objects['TetCage_Tet_0000']; "
            "before=tuple(surface.matrix_world.translation); "
            "cage.data.vertices[0].co.x += 0.1; "
            "assert update_surface_from_cage(cage); "
            "after=tuple(surface.matrix_world.translation); "
            f"payload['pose_updated'] = before != after; bpy.ops.wm.save_as_mainfile(filepath={str(blend)!r}); "
            f"pathlib=__import__('pathlib'); pathlib.Path({str(output)!r}).write_text(json.dumps(payload))"
        )
        environment = os.environ.copy()
        environment.update(
            {
                "BLENDER_USER_CONFIG": str(config),
                "BLENDER_USER_SCRIPTS": str(scripts),
            }
        )
        result = subprocess.run(
            [
                str(blender),
                "--background",
                "--factory-startup",
                "--python-expr",
                expression,
                "--python-exit-code",
                "7",
            ],
            check=False,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if result.returncode != 0:
            raise AssertionError(f"Blender importer failed ({result.returncode}):\n{result.stdout}")
        payload = json.loads(output.read_text())
        reload_expression = (
            "import bpy; "
            "assert bpy.context.scene.get('tetcage_fallback_mode') == 'conventional_mesh'; "
            "assert bpy.data.objects.get('TetCage_Debug_Cage') is not None; "
            "print('TETCAGE_BLENDER_RELOAD_OK', flush=True)"
        )
        reload_result = subprocess.run(
            [str(blender), "--background", "--factory-startup", str(blend), "--python-expr", reload_expression],
            check=False,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        if reload_result.returncode != 0:
            raise AssertionError(f"Blender reload failed ({reload_result.returncode}):\n{reload_result.stdout}")
        assert "TETCAGE_BLENDER_RELOAD_OK" in reload_result.stdout
    assert payload["status"] == "passed"
    assert payload["render_engine"] == "CYCLES"
    assert payload["surface_triangles"] == 1
    assert payload["cage_tetrahedra"] == 1
    assert payload["fallback_mode"] == "conventional_mesh"
    assert payload["render_mode"] == "per_tet_triangle_instances"
    assert payload["tet_objects"] == 1
    assert payload["pose_update_handler"] is True
    assert payload["pose_updated"] is True
    print("Blender tet-cage import contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
