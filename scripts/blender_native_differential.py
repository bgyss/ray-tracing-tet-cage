"""Run native and ordinary Blender tet-cage probes and record a differential."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import subprocess
import tempfile
import time
from typing import Any


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def run_probe(
    blender: pathlib.Path,
    probe: pathlib.Path,
    kernel_root: pathlib.Path,
    asset: pathlib.Path,
    output_image: pathlib.Path,
    output_json: pathlib.Path,
    mode: str,
    native: bool,
    options: dict[str, str],
) -> dict[str, Any]:
    command = [
        str(blender),
        "--background",
        "--factory-startup",
        "--python",
        str(probe),
        "--",
        str(asset),
        str(output_image),
        str(output_json),
        mode,
    ]
    environment = os.environ.copy()
    environment.update(
        {
            "CYCLES_KERNEL_PATH": str(kernel_root),
            "CYCLES_METALRT": "1",
            "CYCLES_TETCAGE_NATIVE": "1" if native else "0",
            **options,
        }
    )
    started = time.perf_counter()
    completed = subprocess.run(command, env=environment, capture_output=True, text=True)
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if completed.returncode != 0:
        raise RuntimeError(
            f"Blender probe failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout[-4000:]}\nstderr:\n{completed.stderr[-4000:]}"
        )
    payload = json.loads(output_json.read_text())
    payload["elapsed_ms"] = elapsed_ms
    payload["command"] = command
    payload["stdout_tail"] = completed.stdout[-2000:]
    return payload


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--blender", required=True, type=pathlib.Path)
    parser.add_argument("--kernel-root", required=True, type=pathlib.Path)
    parser.add_argument("--asset", required=True, type=pathlib.Path)
    parser.add_argument("--mode", required=True)
    parser.add_argument("--output-dir", required=True, type=pathlib.Path)
    parser.add_argument("--probe", type=pathlib.Path, default=pathlib.Path(__file__).with_name("blender_native_tetcage_render_probe.py"))
    parser.add_argument("--compare", type=pathlib.Path, default=pathlib.Path(__file__).with_name("blender_compare_exr.py"))
    parser.add_argument("--motion-delta", default=None)
    parser.add_argument("--transparent-mix", default=None)
    parser.add_argument("--light-mode", default=None)
    parser.add_argument("--area-size", default=None)
    parser.add_argument("--samples", default=None)
    args = parser.parse_args()

    args.output_dir.mkdir(parents=True, exist_ok=True)
    options = {
        key: value
        for key, value in {
            "TETCAGE_MOTION_DELTA": args.motion_delta,
            "TETCAGE_TRANSPARENT_MIX": args.transparent_mix,
            "TETCAGE_LIGHT_MODE": args.light_mode,
            "TETCAGE_AREA_SIZE": args.area_size,
            "TETCAGE_SAMPLES": args.samples,
        }.items()
        if value is not None
    }
    native_image = args.output_dir / "native.exr"
    fallback_image = args.output_dir / "fallback.exr"
    native_json = args.output_dir / "native.json"
    fallback_json = args.output_dir / "fallback.json"
    native = run_probe(args.blender, args.probe, args.kernel_root, args.asset, native_image, native_json, args.mode, True, options)
    fallback = run_probe(args.blender, args.probe, args.kernel_root, args.asset, fallback_image, fallback_json, args.mode, False, options)

    with tempfile.TemporaryDirectory(prefix="tetcage-blender-compare-") as temporary:
        compare_json = pathlib.Path(temporary) / "compare.json"
        compare_command = [
            str(args.blender),
            "--background",
            "--factory-startup",
            "--python",
            str(args.compare),
            "--",
            str(native_image),
            str(fallback_image),
            str(compare_json),
        ]
        completed = subprocess.run(compare_command, capture_output=True, text=True)
        if completed.returncode != 0:
            raise RuntimeError(f"EXR comparison failed:\n{completed.stdout}\n{completed.stderr}")
        comparison = json.loads(compare_json.read_text())

    result = {
        "schema_version": 1,
        "kind": "blender_native_tetcage_differential",
        "status": "passed",
        "asset": str(args.asset),
        "mode": args.mode,
        "options": options,
        "native": {**native, "image_sha256": sha256(native_image)},
        "fallback": {**fallback, "image_sha256": sha256(fallback_image)},
        "comparison": comparison,
        "result": "exact" if comparison["pixel_differences"] == 0 else "numeric_or_open",
    }
    output_json = args.output_dir / "differential.json"
    output_json.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
