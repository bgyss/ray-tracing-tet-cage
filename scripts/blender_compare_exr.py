"""Compare two EXR images from inside Blender without external image packages."""

from __future__ import annotations

import json
import pathlib
import sys

import bpy


def main() -> int:
    args = sys.argv[sys.argv.index("--") + 1 :] if "--" in sys.argv else []
    if len(args) != 3:
        raise RuntimeError("usage: blender --python blender_compare_exr.py -- native.exr fallback.exr result.json")

    native = bpy.data.images.load(str(pathlib.Path(args[0]).resolve()))
    fallback = bpy.data.images.load(str(pathlib.Path(args[1]).resolve()))
    if native.size[:] != fallback.size[:]:
        raise RuntimeError(f"image dimensions differ: {tuple(native.size)} != {tuple(fallback.size)}")

    native_pixels = list(native.pixels)
    fallback_pixels = list(fallback.pixels)
    if len(native_pixels) != len(fallback_pixels):
        raise RuntimeError("image channel counts differ")

    differences = [abs(a - b) for a, b in zip(native_pixels, fallback_pixels)]
    rgb_ranges = range(0, len(differences), 4)
    pixel_differences = sum(
        any(value > 0.0 for value in differences[index : index + 3]) for index in rgb_ranges
    )
    max_abs_rgb_difference = max(
        (max(differences[index : index + 3]) for index in rgb_ranges), default=0.0
    )
    sum_abs_rgb_difference = sum(
        sum(differences[index : index + 3]) for index in rgb_ranges
    )
    payload = {
        "schema_version": 1,
        "kind": "blender_exr_differential",
        "status": "passed",
        "width": int(native.size[0]),
        "height": int(native.size[1]),
        "channels": len(native_pixels) // max(1, int(native.size[0]) * int(native.size[1])),
        "pixel_differences": pixel_differences,
        "max_abs_rgb_difference": max_abs_rgb_difference,
        "sum_abs_rgb_difference": sum_abs_rgb_difference,
    }
    pathlib.Path(args[2]).resolve().write_text(json.dumps(payload, indent=2) + "\n")
    print(json.dumps(payload, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
