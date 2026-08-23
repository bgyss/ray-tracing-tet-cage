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

    assert "!has_deformation_motion && !has_shape_keys" not in mesh
    assert "MTLAccelerationStructureMotionBoundingBoxGeometryDescriptor" in bvh
    assert "attr_P->data_at_time_step<packed_float3>" in bvh
    assert "const float ray_time" in kernel
    assert "metalrt_tetcage_motion_vertices" in kernel
    print("Blender native deformation contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
