#!/usr/bin/env python3
"""Run a deterministic Blender/Cycles integration smoke inside Blender."""

from __future__ import annotations

import json
import os
import pathlib

import bpy


def main() -> int:
    build_options = bpy.app.build_options
    required_options = {
        "cycles": bool(getattr(build_options, "cycles", False)),
        "cycles_osl": bool(getattr(build_options, "cycles_osl", False)),
    }
    missing_options = [name for name, enabled in required_options.items() if not enabled]

    scene = bpy.context.scene
    scene.render.engine = "CYCLES"
    payload = {
        "schema_version": 1,
        "kind": "blender_ui_smoke",
        "status": "failed" if missing_options else "passed",
        "passed": not missing_options,
        "blender_version": bpy.app.version_string,
        "background": bool(bpy.app.background),
        "window_context": bool(bpy.context.window),
        "build_options": required_options,
        "render_engine": scene.render.engine,
        "cycles_device": scene.cycles.device,
        "missing_options": missing_options,
    }
    encoded = json.dumps(payload, sort_keys=True)
    print(f"TETCAGE_BLENDER_SMOKE_OK {encoded}", flush=True)

    output_path = os.environ.get("TETCAGE_BLENDER_SMOKE_OUTPUT")
    if output_path:
        pathlib.Path(output_path).expanduser().write_text(encoded + "\n")

    return 1 if missing_options else 0


if __name__ == "__main__":
    if main():
        raise RuntimeError("Blender UI/Cycles smoke requirements are not enabled")
