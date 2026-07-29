#!/usr/bin/env python3
"""Compile and validate the versioned asset corpus with portable project tools."""

from __future__ import annotations

import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path


def run(command: list[str], expected_success: bool = True) -> tuple[subprocess.CompletedProcess[str], float]:
    started = time.perf_counter()
    completed = subprocess.run(command, check=False, text=True, capture_output=True)
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if (completed.returncode == 0) != expected_success:
        raise RuntimeError(
            f"unexpected exit status {completed.returncode}: {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed, elapsed_ms


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require_clean_oracle(case_id: str, report: dict[str, object]) -> None:
    correctness = report["correctness"]
    blockers = (
        "fast_misses",
        "exact_misses",
        "primitive_mismatches",
        "fast_duplicate_ownership",
        "exact_duplicate_ownership",
    )
    failures = {name: correctness[name] for name in blockers if correctness[name] != 0}
    for frame in report["frame_correctness"]:
        for name in (
            "fast_misses",
            "exact_misses",
            "primitive_ownership_mismatches",
            "fast_duplicate_ownership",
            "exact_duplicate_ownership",
        ):
            if frame[name] != 0:
                failures[f"frame_{frame['frame']}_{name}"] = frame[name]
    if failures:
        raise RuntimeError(f"{case_id}: CPU oracle correctness gate failed: {failures}")


def main() -> int:
    if len(sys.argv) not in {6, 12}:
        print(
            f"usage: {sys.argv[0]} <compiler> <oracle> <image-differential> <source-dir> <output-dir> "
            "[--compiler-report <path> --oracle-report <path> --images-dir <path>]",
            file=sys.stderr,
        )
        return 2
    if len(sys.argv) == 12:
        compiler, oracle, image_tool, source_text, output_text = sys.argv[1:6]
        if sys.argv[6:12:2] != ["--compiler-report", "--oracle-report", "--images-dir"]:
            print("invalid corpus report output options", file=sys.stderr)
            return 2
        compiler_report_path = Path(sys.argv[7])
        oracle_report_path = Path(sys.argv[9])
        images_root = Path(sys.argv[11])
    else:
        compiler, oracle, image_tool, source_text, output_text = sys.argv[1:6]
        compiler_report_path = Path(output_text) / "compiler.json"
        oracle_report_path = Path(output_text) / "oracle.json"
        images_root = Path(output_text) / "images"
    source = Path(source_text)
    output = Path(output_text)
    output.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((source / "assets/manifest.json").read_text(encoding="utf-8"))
    accepted: list[dict[str, object]] = []
    rejected: list[dict[str, object]] = []
    oracle_reports: list[dict[str, object]] = []
    for case in manifest["cases"]:
        case_id = case["id"]
        mesh = source / case["mesh"]
        cage = source / case["cage"]
        artifact = output / "artifacts" / f"{case_id}.tetcage"
        artifact.parent.mkdir(exist_ok=True)
        command = [compiler, str(mesh), str(cage), str(artifact)]
        if case["expected_compiler_outcome"] == "accepted":
            completed, elapsed_ms = run(command)
            inspection = json.loads(completed.stdout)
            if inspection["occupied_tetrahedra"] < case.get("minimum_occupied_tetrahedra", 0):
                raise RuntimeError(f"{case_id}: insufficient occupied tetrahedra: {inspection}")
            if inspection["boundary_fragments"] < case.get("minimum_boundary_fragments", 0):
                raise RuntimeError(f"{case_id}: missing required boundary fragments: {inspection}")
            golden = source / case["golden"]
            if not golden.exists() or artifact.read_bytes() != golden.read_bytes():
                raise RuntimeError(f"{case_id}: compiler output differs from {golden}")
            accepted.append(
                {
                    "id": case_id,
                    "compiler_ms": elapsed_ms,
                    "artifact_sha256": digest(artifact),
                    "golden": case["golden"],
                    "statistics": inspection.get("statistics", inspection),
                    "boundary_coverage": case.get("required_boundary_cases", []),
                }
            )
            frames = int(case.get("animation_frames", 1))
            motion = float(case.get("motion", 0.0))
            oracle_path = output / "oracle" / f"{case_id}.json"
            oracle_path.parent.mkdir(exist_ok=True)
            oracle_command = [
                oracle,
                str(artifact),
                str(oracle_path),
                "--seed",
                "81002718",
                "--random-rays",
                "256",
                "--frames",
                str(frames),
                "--motion",
                str(motion),
            ]
            _, oracle_ms = run(oracle_command)
            oracle_report = json.loads(oracle_path.read_text(encoding="utf-8"))
            require_clean_oracle(case_id, oracle_report)
            oracle_report["wall_clock_ms"] = oracle_ms
            oracle_reports.append({"id": case_id, "report": oracle_report})
            image_dir = images_root / case_id
            image_completed, image_ms = run(
                [image_tool, str(artifact), str(image_dir), "--width", "48", "--height", "48"]
            )
            image_report = json.loads(image_completed.stdout)
            image_report["wall_clock_ms"] = image_ms
            (image_dir / "report.json").write_text(
                json.dumps(image_report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
            )
        else:
            completed, elapsed_ms = run(command, expected_success=False)
            rejected.append(
                {
                    "id": case_id,
                    "compiler_ms": elapsed_ms,
                    "expected": case["expected_compiler_outcome"],
                    "diagnostic": completed.stderr.strip(),
                }
            )
    compiler_report = {
        "schema_version": 1,
        "report_kind": "compiler_asset_corpus",
        "evidence_class": "synthetic",
        "manifest_version": manifest["version"],
        "accepted_assets": len(accepted),
        "rejected_assets": len(rejected),
        "accepted": accepted,
        "rejected": rejected,
        "reproducibility": "golden binary outputs are byte-for-byte compared before oracle execution",
    }
    oracle_report = {
        "schema_version": 1,
        "report_kind": "cpu_oracle_asset_corpus",
        "evidence_class": "synthetic",
        "manifest_version": manifest["version"],
        "fixed_adversarial_seed": 81002718,
        "reports": oracle_reports,
        "failure_policy": "any fast/exact miss, primitive ownership mismatch, or fast/exact duplicate ownership fails",
    }
    compiler_report_path.parent.mkdir(parents=True, exist_ok=True)
    oracle_report_path.parent.mkdir(parents=True, exist_ok=True)
    compiler_report_path.write_text(
        json.dumps(compiler_report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    oracle_report_path.write_text(
        json.dumps(oracle_report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(json.dumps({"accepted_assets": len(accepted), "rejected_assets": len(rejected)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
