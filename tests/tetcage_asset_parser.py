#!/usr/bin/env python3
"""Contract test for the Blender-side .tetcage reader."""

from __future__ import annotations

import os
import pathlib
import subprocess
import sys
import tempfile

REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from tetcage_asset import load_asset  # noqa: E402


def main() -> int:
    compiler = pathlib.Path(
        os.environ.get(
            "TETCAGE_ASSET_COMPILER_BIN",
            sys.argv[1] if len(sys.argv) == 2 else str(REPO_ROOT / "build/dev/tetcage_asset_compiler"),
        )
    )
    with tempfile.TemporaryDirectory(prefix="tetcage-parser-") as raw:
        output = pathlib.Path(raw) / "one-tet.tetcage"
        subprocess.run(
            [
                str(compiler),
                str(REPO_ROOT / "tests/assets/one-tet.obj"),
                str(REPO_ROOT / "tests/assets/one-tet.cage"),
                str(output),
            ],
            check=True,
            stdout=subprocess.PIPE,
            text=True,
        )
        asset = load_asset(output)
    assert asset["format_version"] == 1
    assert len(asset["source_triangles"]) == 1
    assert len(asset["cage_tetrahedra"]) == 1
    assert len(asset["generated_vertices"]) == 3
    assert len(asset["micro_triangles"]) == 1
    assert asset["micro_triangles"][0]["source_primitive"] == 0
    print("tetcage asset parser contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
