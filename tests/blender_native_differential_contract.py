#!/usr/bin/env python3
"""Check the reproducible Blender native/fallback differential harness."""

from __future__ import annotations

import pathlib


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    runner = (root / "scripts/blender_native_differential.py").read_text()
    compare = (root / "scripts/blender_compare_exr.py").read_text()
    probe = (root / "scripts/blender_native_tetcage_render_probe.py").read_text()
    assert "CYCLES_TETCAGE_NATIVE" in runner
    assert "elapsed_ms" in runner
    assert "image_sha256" in runner
    assert "numeric_close" in runner
    assert "--samples" in runner
    assert 'sample_count = int(os.environ.get("TETCAGE_SAMPLES", "1"))' in probe
    assert '"samples": sample_count' in probe
    assert "pixel_differences" in compare
    assert "max_abs_rgb_difference" in compare
    print("Blender native differential contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
