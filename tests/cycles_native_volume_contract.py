#!/usr/bin/env python3
"""Guard the native tet-cage volume intersection-table seam."""

from __future__ import annotations

import os
import pathlib


def main() -> int:
    root = pathlib.Path(
        os.environ.get("CYCLES_NATIVE_WORKTREE", "/Users/briangyss/src/cycles-tetcage-metalrt")
    ).expanduser()
    blender = pathlib.Path(
        os.environ.get("BLENDER_NATIVE_WORKTREE", "/Users/briangyss/src/blender-tetcage-native")
    ).expanduser()
    if not root.is_dir() or not blender.is_dir():
        print("Cycles native volume contract: skipped (native worktrees unavailable)")
        return 77

    for source_root, relative in (
        (root, "src/kernel/device/metal/kernel.metal"),
        (blender, "intern/cycles/kernel/device/metal/kernel.metal"),
    ):
        text = (source_root / relative).read_text()
        assert "__intersection__volume_tetcage" in text
        assert "metalrt_tetcage_intersect" in text

    for source_root, relative in (
        (root, "src/device/metal/kernel.mm"),
        (blender, "intern/cycles/device/metal/kernel.mm"),
    ):
        text = (source_root / relative).read_text()
        assert "add_intersection_functions(METALRT_TABLE_VOLUME" in text
        assert '"__intersection__volume_tri"' in text
        assert '"__intersection__volume_tetcage"' in text

    for source_root, relative in (
        (root, "src/kernel/device/metal/bvh.h"),
        (blender, "intern/cycles/kernel/device/metal/bvh.h"),
    ):
        text = (source_root / relative).read_text()
        assert "intersection_type::bounding_box" in text
        assert "payload.tet_object" in text

    print("Cycles native volume contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
