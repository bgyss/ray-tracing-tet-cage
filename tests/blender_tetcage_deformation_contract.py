#!/usr/bin/env python3
"""Guard the native Blender deformation-motion seams."""

from __future__ import annotations

import os
import pathlib


def main() -> int:
    root = pathlib.Path(
        os.environ.get("BLENDER_NATIVE_WORKTREE", "/Users/briangyss/src/blender-tetcage-native")
    ).expanduser()
    if not root.is_dir():
        print("Blender native deformation contract: skipped (native worktree unavailable)")
        return 77

    mesh = (root / "intern/cycles/blender/mesh.cpp").read_text()
    bvh = (root / "intern/cycles/device/metal/bvh.mm").read_text()
    kernel = (root / "intern/cycles/kernel/device/metal/kernel.metal").read_text()
    probe = pathlib.Path(__file__).resolve().parents[1] / "scripts/blender_native_tetcage_render_probe.py"

    assert "!has_deformation_motion && !has_shape_keys" not in mesh
    assert "MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor" in bvh
    assert "attr_P->data_at_time_step<packed_float3>" in bvh
    assert "const float ray_time" in kernel
    assert "metalrt_tetcage_motion_vertices" in kernel
    assert "has_transparent_surface" in mesh
    assert "has_surface_shadow_transparency" in mesh
    assert "has_surface_emission" in mesh
    assert "shader->graph" in mesh
    assert "Scene::MOTION_BLUR" in mesh
    assert "transparent/emission motion" in mesh
    assert "transparent_pure_motion" in probe.read_text()
    print("Blender native deformation contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
