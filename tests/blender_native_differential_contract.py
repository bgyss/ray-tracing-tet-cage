#!/usr/bin/env python3
"""Check the reproducible Blender native/fallback differential harness."""

from __future__ import annotations

import pathlib


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    runner = (root / "scripts/blender_native_differential.py").read_text()
    compare = (root / "scripts/blender_compare_exr.py").read_text()
    assert "CYCLES_TETCAGE_NATIVE" in runner
    assert "elapsed_ms" in runner
    assert "image_sha256" in runner
    assert "pixel_differences" in compare
    assert "max_abs_rgb_difference" in compare
    print("Blender native differential contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
