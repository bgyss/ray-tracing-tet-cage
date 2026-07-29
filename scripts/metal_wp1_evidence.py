#!/usr/bin/env python3
"""Run the real Metal WP1 correctness, animation, and AS-build evidence matrix."""

from __future__ import annotations

import argparse
import json
import subprocess
import tempfile
from pathlib import Path
from typing import Any


CORPUS = (
    ("smooth_closed", "assets/golden/smooth_closed.tetcage", 1, 0.0),
    ("sharp_material_uv_seam", "assets/golden/sharp_material_uv_seam.tetcage", 1, 0.0),
    ("face_edge_vertex_sharing", "assets/golden/face_edge_vertex_sharing.tetcage", 1, 0.0),
    (
        "dense_animated_embedded_surface",
        "assets/golden/dense_animated_embedded_surface.tetcage",
        8,
        0.025,
    ),
)


def run_metal(
    runner: Path,
    root: Path,
    asset: str,
    *,
    copies: int,
    rays: int,
    frames: int,
    motion: float,
    rebuild_period: int,
    extended: bool = False,
) -> dict[str, Any]:
    with tempfile.NamedTemporaryFile(suffix=".json") as output:
        command = [
            str(runner),
            str(root / asset),
            output.name,
            "--copies",
            str(copies),
            "--rays",
            str(rays),
            "--motion",
            str(motion),
            "--frames",
            str(frames),
            "--rebuild-period",
            str(rebuild_period),
            "--compact",
            "--boundary-fallback",
            "--gpu-instances",
            "--allow-unverified",
        ]
        if extended:
            command.append("--extended-limits")
        completed = subprocess.run(command, cwd=root, check=False, capture_output=True, text=True)
        manifest = json.loads(Path(output.name).read_text(encoding="utf-8"))
        manifest["exit_code"] = completed.returncode
        manifest.setdefault("statistics", {})["stderr"] = completed.stderr.strip() or None
        return manifest


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path, required=True)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--rays", type=int, default=128)
    parser.add_argument("--limit-copies", type=int, default=65536)
    parser.add_argument("--smoke", action="store_true")
    args = parser.parse_args()

    root = args.repo_root.resolve()
    runner = args.runner.resolve()
    output = args.output_root.resolve()
    rays = 16 if args.smoke else args.rays
    limit_copies = 2 if args.smoke else args.limit_copies

    correctness_runs = []
    for case_id, asset, frames, motion in CORPUS:
        run = run_metal(
            runner,
            root,
            asset,
            copies=1,
            rays=rays,
            frames=frames,
            motion=motion,
            rebuild_period=4 if frames > 1 else 0,
        )
        run["scene"]["id"] = case_id
        correctness_runs.append(run)
    correctness = {
        "schema_version": 1,
        "kind": "metal_correctness_corpus",
        "date": "2026-07-28",
        "boundary_rule": "lowest stable source primitive and owner tet; CPU oracle supplies the final hit for classified boundary or observed hardware mismatches",
        "tolerance_policy": "existing 2.5e-5 absolute validation threshold retained; no tolerance broadening",
        "m5_status": "open_high_fallback_frequency_and_hardware_replay_minimization_pending",
        "runs": correctness_runs,
    }
    write_json(output / "2026-07-28-correctness-corpus.json", correctness)

    dense = run_metal(
        runner,
        root,
        "assets/golden/dense_animated_embedded_surface.tetcage",
        copies=1,
        rays=rays,
        frames=8,
        motion=0.025,
        rebuild_period=4,
    )
    dense["scene"]["id"] = "dense_animated_embedded_surface"
    dense["statistics"]["animation_contract"] = (
        "immutable canonical micro-BLAS; per-frame cage transforms and instance descriptors only"
    )
    write_json(output / "2026-07-28-dense-animation.json", dense)

    limit_runs = []
    for extended in (False, True):
        run = run_metal(
            runner,
            root,
            "assets/golden/smooth_closed.tetcage",
            copies=limit_copies,
            rays=rays,
            frames=1,
            motion=0.0,
            rebuild_period=0,
            extended=extended,
        )
        run["scene"]["id"] = "standard_limit_build" if not extended else "extended_limit_build"
        run["statistics"]["proof_kind"] = "completed_tlas_build_and_trace_not_size_query"
        limit_runs.append(run)
    limit_evidence = {
        "schema_version": 1,
        "kind": "metal_limit_builds",
        "date": "2026-07-28",
        "requested_instances_per_mode": limit_copies,
        "m5_status": "limit_build_gate_satisfied_if_both_runs_are_measured_and_failure_free",
        "runs": limit_runs,
    }
    write_json(output / "2026-07-28-limit-builds.json", limit_evidence)

    manifests = correctness_runs + [dense] + limit_runs
    return 0 if all(run["exit_code"] == 0 for run in manifests) else 1


if __name__ == "__main__":
    raise SystemExit(main())
