#!/usr/bin/env python3
"""Guard the production fallback boundary for mixed transparent motion."""

from __future__ import annotations

import json
import os
import pathlib


def main() -> int:
    root = pathlib.Path(
        os.environ.get("BLENDER_NATIVE_WORKTREE", "/Users/briangyss/src/blender-tetcage-native")
    ).expanduser()
    if not root.is_dir():
        print("Blender mixed-transparent motion contract: skipped (native worktree unavailable)")
        return 77

    mesh = (root / "intern/cycles/blender/mesh.cpp").read_text()
    assert "const bool mixed_transparent_motion" in mesh
    dispatch = mesh[mesh.index("const bool mixed_transparent_motion") :]
    dispatch = dispatch[: dispatch.index("LOG_DEBUG")]
    assert "!mixed_transparent_motion" in dispatch
    assert "has_transparent_surface" in dispatch
    assert "has_transparent_closure" in dispatch
    assert "has_nontransparent_surface" in dispatch
    assert "Scene::MOTION_BLUR" in dispatch

    fallback = mesh[mesh.index("LOG_DEBUG") :]
    assert "mixed transparent closure motion is not yet native" in fallback

    probe = pathlib.Path(__file__).resolve().parents[1] / "scripts/blender_native_tetcage_render_probe.py"
    probe_text = probe.read_text()
    assert "transparent_motion" in probe_text
    assert "transparent_diffuse_motion" in probe_text
    assert "transparent_pure_motion" in probe_text

    manifest = json.loads(
        (pathlib.Path(__file__).resolve().parents[1]
         / "results/integrations/2026-08-23-cycles-metal-entry.json").read_text()
    )["blender_fallback_import"]
    for key in ("native_transparent_motion_render", "native_transparent_diffuse_motion_render"):
        record = manifest[key]
        assert record["native_adapter_selected"] is False
        assert record["fallback_reason"] == "mixed transparent-closure motion is not yet native"
        assert record["production_gate_contract"]["result"] == "passed"
        assert record["latest_default_differential"]["result"] == "exact"

    print("Blender mixed-transparent motion contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
