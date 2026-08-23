#!/usr/bin/env python3
"""Guard the shared projected-axis narrow phase across native Cycles lanes."""

from __future__ import annotations

import os
import pathlib


def source_root(name: str, default: str) -> pathlib.Path:
    root = pathlib.Path(os.environ.get(name, default)).expanduser()
    if not root.is_dir():
        raise AssertionError(f"{name} is missing: {root}")
    return root


def assert_local_branch_uses_projected_helper(root: pathlib.Path, relative: str) -> None:
    source = root / relative
    text = source.read_text()
    helper = "metal_tetcage_triangle_intersect_local"
    assert helper in text, f"{source} does not define {helper}"

    branch_start = text.index("if (kernel_data_fetch(object_flag, local_object) & SD_OBJECT_TETCAGE)")
    branch_end = text.index("#    endif", branch_start)
    branch = text[branch_start:branch_end]
    assert f"{helper}(" in branch, f"{source} tet local branch bypasses {helper}"
    assert "const bool hit = triangle_intersect_local(" not in branch, (
        f"{source} tet local branch still uses the historical triangle helper"
    )
    assert "ray->self.prim == prim" in branch, (
        f"{source} tet local branch does not use Metal-safe self-hit filtering"
    )


def assert_local_runtime_table(root: pathlib.Path, kernel_relative: str, table_relative: str) -> None:
    kernel = (root / kernel_relative).read_text()
    table = (root / table_relative).read_text()
    assert "__intersection__local_tetcage" in kernel
    assert "__intersection__local_tetcage_mblur" in kernel
    assert "add_intersection_functions(METALRT_TABLE_LOCAL" in table
    assert '"__intersection__local_tetcage"' in table
    assert '"__intersection__local_tetcage_mblur"' in table


def main() -> int:
    cycles_path = pathlib.Path(
        os.environ.get("CYCLES_NATIVE_WORKTREE", "/Users/briangyss/src/cycles-tetcage-metalrt")
    ).expanduser()
    blender_path = pathlib.Path(
        os.environ.get("BLENDER_NATIVE_WORKTREE", "/Users/briangyss/src/blender-tetcage-native")
    ).expanduser()
    if not cycles_path.is_dir() or not blender_path.is_dir():
        print("Cycles native local-kernel contract: skipped (native worktrees unavailable)")
        return 77
    cycles = source_root("CYCLES_NATIVE_WORKTREE", str(cycles_path))
    blender = source_root("BLENDER_NATIVE_WORKTREE", str(blender_path))
    assert_local_branch_uses_projected_helper(cycles, "src/kernel/device/metal/bvh.h")
    assert_local_branch_uses_projected_helper(blender, "intern/cycles/kernel/device/metal/bvh.h")
    assert_local_runtime_table(
        cycles, "src/kernel/device/metal/kernel.metal", "src/device/metal/kernel.mm"
    )
    assert_local_runtime_table(
        blender,
        "intern/cycles/kernel/device/metal/kernel.metal",
        "intern/cycles/device/metal/kernel.mm",
    )
    assert "motion_triangle_vertices(kg, local_object, prim, ray->time" in (
        (cycles / "src/kernel/device/metal/bvh.h").read_text()
    )
    assert "motion_triangle_vertices(kg, local_object, prim, ray->time" in (
        (blender / "intern/cycles/kernel/device/metal/bvh.h").read_text()
    )
    for root, relative in (
        (cycles, "src/kernel/device/metal/bvh.h"),
        (blender, "intern/cycles/kernel/device/metal/bvh.h"),
    ):
        text = (root / relative).read_text()
        assert "metal_tetcage_triangle_intersect_local_vertices" in text
        assert "bvh_instance_motion_push(nullptr, local_object, ray" in text
    print("Cycles native local-kernel contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
