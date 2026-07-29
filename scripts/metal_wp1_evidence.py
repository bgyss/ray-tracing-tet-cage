#!/usr/bin/env python3
"""Run the real Metal WP1 correctness, animation, and AS-build evidence matrix."""

from __future__ import annotations

import argparse
import copy
import json
import re
import subprocess
import tempfile
from pathlib import Path
from typing import Any


CORPUS = (
    ("smooth_closed", "assets/golden/smooth_closed.tetcage", 1, 0.0, 1),
    ("sharp_material_uv_seam", "assets/golden/sharp_material_uv_seam.tetcage", 1, 0.0, 1),
    ("face_edge_vertex_sharing", "assets/golden/face_edge_vertex_sharing.tetcage", 1, 0.0, 2),
    (
        "dense_animated_embedded_surface",
        "assets/golden/dense_animated_embedded_surface.tetcage",
        8,
        0.025,
        1,
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
        ]
        if extended:
            command.append("--extended-limits")
        completed = subprocess.run(command, cwd=root, check=False, capture_output=True, text=True)
        manifest = json.loads(Path(output.name).read_text(encoding="utf-8"))
        manifest["exit_code"] = completed.returncode
        manifest.setdefault("statistics", {})["stderr"] = completed.stderr.strip() or None
        return manifest


def require_direct_run(
    manifest: dict[str, Any],
    *,
    expected_frames: int,
    expected_rays: int,
    expected_instances: int,
) -> str:
    errors: list[str] = []
    backend = manifest.get("backend", {})
    source = manifest.get("source", {})
    correctness = manifest.get("correctness", {})
    statistics = manifest.get("statistics", {})
    timings = manifest.get("timings_ms", {})
    failures = manifest.get("failures")
    mismatches = statistics.get("mismatch_samples")
    final_hits = statistics.get("final_hit_records")
    comparator = statistics.get("same_work_comparator", {})

    if manifest.get("exit_code") != 0:
        errors.append("runner exit_code is not zero")
    if backend.get("status") != "measured":
        errors.append("backend.status is not measured")
    if manifest.get("evidence_class") != "direct":
        errors.append("evidence_class is not direct")
    if failures != []:
        errors.append("failures is not empty")
    revision = source.get("commit")
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        errors.append("source.commit is not an exact revision")
        revision = ""
    if source.get("dirty") is not False:
        errors.append("source tree was dirty when the measured binary was configured")
    if statistics.get("frames_completed") != expected_frames:
        errors.append("frames_completed does not match the requested work")
    if correctness.get("rays") != expected_rays:
        errors.append("correctness.rays does not match frames times requested rays")
    if statistics.get("instances") != expected_instances:
        errors.append("instance count does not match the requested build")
    if not isinstance(mismatches, list) or len(mismatches) != correctness.get(
        "hardware_mismatches"
    ):
        errors.append("every hardware mismatch is not represented")
    elif any(
        item.get("minimization", {}).get("status")
        != "hardware_replay_preserved_classification"
        for item in mismatches
    ):
        errors.append("a mismatch lacks classification-preserving hardware replay")
    elif any(
        item.get("classification") != item.get("minimization", {}).get("classification")
        for item in mismatches
    ):
        errors.append("a minimized mismatch changed classification")
    if statistics.get("minimized_regressions") != correctness.get("hardware_mismatches"):
        errors.append("minimized regression count does not match hardware mismatches")
    if statistics.get("minimization_failures") != 0:
        errors.append("hardware replay minimization reported failures")
    if not isinstance(final_hits, list) or len(final_hits) != expected_rays:
        errors.append("explicit final-hit stream does not contain every ray")
    elif sorted(record.get("ray_index") for record in final_hits) != list(
        range(expected_rays)
    ):
        errors.append("explicit final-hit stream does not identify every ray exactly once")
    elif any(
        record.get("selected_path") not in ("hardware", "cpu_fallback")
        or not isinstance(record.get("hit"), bool)
        for record in final_hits
    ):
        errors.append("explicit final-hit stream contains an invalid selected record")
    elif any(
        record["hit"]
        and not {
            "t",
            "position",
            "source_barycentric",
            "normal",
            "uv",
            "source_primitive",
            "material",
            "owner_tet",
            "micro_triangle",
        }.issubset(record)
        for record in final_hits
    ):
        errors.append("a selected final hit lacks reconstructed attributes")
    elif sum(record["selected_path"] == "cpu_fallback" for record in final_hits) != correctness.get(
        "cpu_fallback_rays"
    ):
        errors.append("final-hit selection does not match fallback accounting")
    if statistics.get("final_stream_records") != expected_rays:
        errors.append("final_stream_records does not match the requested work")
    if statistics.get("final_stream_errors") != 0 or statistics.get(
        "final_stream_validated"
    ) is not True:
        errors.append("merged final-hit stream did not validate")
    if comparator.get("rays") != expected_rays:
        errors.append("same-work comparator does not cover identical rays")
    for field in (
        "hardware_only_trace_ms",
        "selection_oracle_ms",
        "selected_fallback_ms",
        "merge_ms",
        "end_to_end_trace_selection_merge_ms",
    ):
        if not isinstance(comparator.get(field), (int, float)) or comparator[field] < 0:
            errors.append(f"same-work comparator field {field} is unavailable")
    if (
        comparator.get("selection_oracle_scope")
        != "cpu_trace_boundary_test_hardware_comparison_and_path_choice"
        or comparator.get("selected_fallback_cost_accounting")
        != "subset_of_selection_oracle_ms_reused_without_retrace"
        or comparator.get("end_to_end_formula")
        != "hardware_only_trace_ms + selection_oracle_ms + merge_ms"
    ):
        errors.append("same-work comparator cost accounting is ambiguous")
    if "shading" not in timings or timings["shading"] is not None:
        errors.append("CPU validation is incorrectly labeled as shading")
    if statistics.get("gpu_attribute_recovery_ms", "missing") is not None:
        errors.append("fused GPU attribute recovery must remain explicitly unavailable")
    if not isinstance(statistics.get("cpu_validation_ms"), (int, float)):
        errors.append("CPU validation timing is unavailable")

    if errors:
        raise RuntimeError("; ".join(errors))
    return revision


def validate_evidence(output: Path) -> str:
    correctness = json.loads(
        (output / "2026-07-28-correctness-corpus.json").read_text(encoding="utf-8")
    )
    dense = json.loads(
        (output / "2026-07-28-dense-animation.json").read_text(encoding="utf-8")
    )
    limits = json.loads(
        (output / "2026-07-28-limit-builds.json").read_text(encoding="utf-8")
    )
    revisions: set[str] = set()
    runs = correctness.get("runs", [])
    if len(runs) != len(CORPUS):
        raise RuntimeError("correctness corpus does not contain every declared WP0 asset")
    for run, (_, _, frames, _, instances) in zip(runs, CORPUS, strict=True):
        revisions.add(
            require_direct_run(
                run,
                expected_frames=frames,
                expected_rays=run["configuration"]["requested_rays"] * frames,
                expected_instances=instances,
            )
        )
    revisions.add(
        require_direct_run(
            dense,
            expected_frames=8,
            expected_rays=dense["configuration"]["requested_rays"] * 8,
            expected_instances=1,
        )
    )
    limit_runs = limits.get("runs", [])
    if len(limit_runs) != 2:
        raise RuntimeError("standard and extended limit runs are both required")
    if [run["configuration"].get("extended_limits") for run in limit_runs] != [False, True]:
        raise RuntimeError("limit evidence does not contain standard then extended mode")
    requested_instances = limits.get("requested_instances_per_mode")
    for run in limit_runs:
        revisions.add(
            require_direct_run(
                run,
                expected_frames=1,
                expected_rays=run["configuration"]["requested_rays"],
                expected_instances=requested_instances,
            )
        )
    if len(revisions) != 1:
        raise RuntimeError("evidence runs do not identify one exact code revision")
    revision = next(iter(revisions))
    if correctness.get("source_revision") != revision or limits.get("source_revision") != revision:
        raise RuntimeError("wrapper source revision does not match measured runs")
    return revision


def validate_regression_corpus(path: Path, revision: str, expected_cases: int) -> None:
    corpus = json.loads(path.read_text(encoding="utf-8"))
    if corpus.get("source_revision") != revision:
        raise RuntimeError("regression corpus source revision does not match measured runs")
    if corpus.get("algorithm") != "deterministic_coordinate_ddmin_with_metal_hardware_replay":
        raise RuntimeError("regression corpus does not identify the hardware replay minimizer")
    cases = corpus.get("cases")
    if not isinstance(cases, list) or len(cases) != expected_cases:
        raise RuntimeError("regression corpus does not represent every hardware mismatch")
    if any(
        case.get("minimization_status")
        != "hardware_replay_preserved_classification"
        for case in cases
    ):
        raise RuntimeError("regression corpus contains an unverified minimized ray")


def contract_smoke() -> None:
    valid = {
        "exit_code": 0,
        "backend": {"status": "measured"},
        "source": {"commit": "a" * 40, "dirty": False},
        "evidence_class": "direct",
        "failures": [],
        "configuration": {"requested_rays": 1},
        "timings_ms": {"shading": None},
        "correctness": {
            "rays": 1,
            "hardware_mismatches": 0,
            "cpu_fallback_rays": 0,
        },
        "statistics": {
            "frames_completed": 1,
            "instances": 1,
            "mismatch_samples": [],
            "minimized_regressions": 0,
            "minimization_failures": 0,
            "final_hit_records": [
                {"ray_index": 0, "selected_path": "hardware", "hit": False}
            ],
            "final_stream_records": 1,
            "final_stream_errors": 0,
            "final_stream_validated": True,
            "same_work_comparator": {
                "rays": 1,
                "hardware_only_trace_ms": 1.0,
                "selection_oracle_ms": 1.0,
                "selection_oracle_scope": (
                    "cpu_trace_boundary_test_hardware_comparison_and_path_choice"
                ),
                "selected_fallback_ms": 0.0,
                "selected_fallback_cost_accounting": (
                    "subset_of_selection_oracle_ms_reused_without_retrace"
                ),
                "merge_ms": 1.0,
                "end_to_end_trace_selection_merge_ms": 3.0,
                "end_to_end_formula": (
                    "hardware_only_trace_ms + selection_oracle_ms + merge_ms"
                ),
            },
            "gpu_attribute_recovery_ms": None,
            "cpu_validation_ms": 1.0,
        },
    }
    require_direct_run(valid, expected_frames=1, expected_rays=1, expected_instances=1)
    invalid = copy.deepcopy(valid)
    invalid["backend"]["status"] = "unverified"
    try:
        require_direct_run(invalid, expected_frames=1, expected_rays=1, expected_instances=1)
    except RuntimeError:
        return
    raise RuntimeError("fail-closed contract accepted unverified evidence")


def write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--runner", type=Path)
    parser.add_argument("--repo-root", type=Path, default=Path.cwd())
    parser.add_argument("--output-root", type=Path)
    parser.add_argument("--regression-output", type=Path)
    parser.add_argument("--rays", type=int, default=128)
    parser.add_argument("--limit-copies", type=int, default=65536)
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument("--contract-smoke", action="store_true")
    args = parser.parse_args()

    if args.contract_smoke:
        contract_smoke()
        return 0
    if args.output_root is None:
        parser.error("--output-root is required")
    if args.validate_only:
        output = args.output_root.resolve()
        revision = validate_evidence(output)
        correctness = json.loads(
            (output / "2026-07-28-correctness-corpus.json").read_text(encoding="utf-8")
        )
        expected_cases = sum(
            run["correctness"]["hardware_mismatches"] for run in correctness["runs"]
        )
        regression_output = (
            args.regression_output.resolve()
            if args.regression_output
            else args.repo_root.resolve()
            / "tests/assets/metal-regressions/2026-07-28-minimized-corpus.json"
        )
        validate_regression_corpus(regression_output, revision, expected_cases)
        return 0
    if args.runner is None:
        parser.error("--runner is required when generating evidence")

    root = args.repo_root.resolve()
    runner = args.runner.resolve()
    output = args.output_root.resolve()
    rays = args.rays
    limit_copies = args.limit_copies

    correctness_runs = []
    for case_id, asset, frames, motion, expected_instances in CORPUS:
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
        require_direct_run(
            run,
            expected_frames=frames,
            expected_rays=rays * frames,
            expected_instances=expected_instances,
        )
        correctness_runs.append(run)
    revisions = {run["source"]["commit"] for run in correctness_runs}
    if len(revisions) != 1:
        raise RuntimeError("correctness runs were not built from one exact revision")
    revision = next(iter(revisions))
    correctness = {
        "schema_version": 1,
        "kind": "metal_correctness_corpus",
        "date": "2026-07-28",
        "boundary_rule": "lowest stable source primitive and owner tet; CPU oracle supplies the final hit for classified boundary or observed hardware mismatches",
        "tolerance_policy": "existing 2.5e-5 absolute validation threshold retained; no tolerance broadening",
        "m5_status": "open_high_fallback_frequency_despite_minimized_hardware_regressions",
        "source_revision": revision,
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
    require_direct_run(
        dense, expected_frames=8, expected_rays=rays * 8, expected_instances=1
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
        require_direct_run(
            run, expected_frames=1, expected_rays=rays, expected_instances=limit_copies
        )
        limit_runs.append(run)
    limit_evidence = {
        "schema_version": 1,
        "kind": "metal_limit_builds",
        "date": "2026-07-28",
        "requested_instances_per_mode": limit_copies,
        "m5_status": "limit_build_gate_satisfied_if_both_runs_are_measured_and_failure_free",
        "source_revision": revision,
        "runs": limit_runs,
    }
    write_json(output / "2026-07-28-limit-builds.json", limit_evidence)

    regression_output = (
        args.regression_output.resolve()
        if args.regression_output
        else root
        / "tests/assets/metal-regressions/2026-07-28-minimized-corpus.json"
    )
    regressions = []
    for run, (case_id, asset, _, _, _) in zip(correctness_runs, CORPUS, strict=True):
        requested_rays = run["configuration"]["requested_rays"]
        for mismatch in run["statistics"]["mismatch_samples"]:
            regressions.append(
                {
                    "scene": case_id,
                    "asset": asset,
                    "frame": mismatch["ray_index"] // requested_rays,
                    "ray_index": mismatch["ray_index"] % requested_rays,
                    "classification": mismatch["classification"],
                    "original_ray": mismatch["ray"],
                    "minimized_ray": mismatch["minimization"]["ray"],
                    "minimization_status": mismatch["minimization"]["status"],
                }
            )
    write_json(
        regression_output,
        {
            "schema_version": 1,
            "source_revision": revision,
            "algorithm": "deterministic_coordinate_ddmin_with_metal_hardware_replay",
            "cases": regressions,
        },
    )
    validated_revision = validate_evidence(output)
    validate_regression_corpus(regression_output, validated_revision, len(regressions))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
