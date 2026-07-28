#!/usr/bin/env python3
"""Validate checked-in result manifests without requiring a third-party JSON-schema package."""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path
from typing import Any


REQUIRED_TOP_LEVEL = {
    "schema_version",
    "run_id",
    "timestamp_utc",
    "source",
    "build",
    "host",
    "backend",
    "scene",
    "configuration",
    "timings_ms",
    "memory_bytes",
    "correctness",
    "command",
    "evidence_class",
}
TIMING_KEYS = {
    "cage_deformation",
    "transform_generation",
    "instance_generation",
    "blas",
    "tlas",
    "synchronization",
    "traversal",
    "shading",
    "total",
    "warmup_iterations",
    "measured_iterations",
}
MEMORY_KEYS = {
    "source_geometry",
    "canonical_geometry",
    "provenance",
    "cage",
    "blas",
    "tlas",
    "scratch",
    "instance_buffers",
    "api_objects",
    "renderer_state",
    "peak_total",
}
CORRECTNESS_KEYS = {
    "rays",
    "misses",
    "duplicate_hits",
    "wrong_ownership",
    "position_error_max",
    "normal_error_max",
    "attribute_error_max",
    "image_error",
}
HEX40 = re.compile(r"^[0-9a-f]{40}$")


def fail(path: Path, message: str) -> None:
    raise ValueError(f"{path}: {message}")


def nonnegative_measurements(path: Path, values: dict[str, Any], keys: set[str]) -> None:
    for key in keys:
        value = values.get(key)
        if value is not None and (not isinstance(value, (int, float)) or value < 0):
            fail(path, f"{key} must be a non-negative number or null")


def validate_manifest(path: Path, manifest: dict[str, Any]) -> None:
    missing = REQUIRED_TOP_LEVEL - manifest.keys()
    if missing:
        fail(path, f"missing shared fields: {sorted(missing)}")
    if manifest["schema_version"] != 1:
        fail(path, "schema_version is not 1")
    source = manifest["source"]
    if not isinstance(source, dict) or not HEX40.fullmatch(source.get("commit", "")):
        fail(path, "source.commit is not a 40-character hexadecimal commit")
    if not isinstance(source.get("dirty"), bool):
        fail(path, "source.dirty is not boolean")
    for section, keys in (
        ("timings_ms", TIMING_KEYS),
        ("memory_bytes", MEMORY_KEYS),
        ("correctness", CORRECTNESS_KEYS),
    ):
        value = manifest[section]
        if not isinstance(value, dict) or not keys <= value.keys():
            fail(path, f"{section} does not contain the shared required fields")
    nonnegative_measurements(path, manifest["timings_ms"], TIMING_KEYS - {"warmup_iterations", "measured_iterations"})
    nonnegative_measurements(path, manifest["memory_bytes"], MEMORY_KEYS)
    for key in ("warmup_iterations", "measured_iterations"):
        if not isinstance(manifest["timings_ms"][key], int) or manifest["timings_ms"][key] < 0:
            fail(path, f"timings_ms.{key} must be a non-negative integer")
    nonnegative_measurements(path, manifest["correctness"], CORRECTNESS_KEYS)
    backend = manifest["backend"]
    if not isinstance(backend, dict):
        fail(path, "backend is not an object")
    if backend.get("status") not in {"measured", "unsupported", "unverified", "error"}:
        fail(path, "backend.status is not a supported status")
    if manifest["evidence_class"] not in {"direct", "partial", "synthetic", "schema_only", "blocked"}:
        fail(path, "evidence_class is not supported")


def load_documents(path: Path) -> list[dict[str, Any]]:
    text = path.read_text(encoding="utf-8")
    try:
        value = json.loads(text)
        return [value] if isinstance(value, dict) else []
    except json.JSONDecodeError:
        documents = []
        for line_number, line in enumerate(text.splitlines(), 1):
            if not line.strip():
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as error:
                fail(path, f"line {line_number} is not JSON: {error}")
            if not isinstance(value, dict):
                fail(path, f"line {line_number} is not a JSON object")
            documents.append(value)
        return documents


def shared_manifests(document: dict[str, Any]) -> list[dict[str, Any]]:
    """Return shared manifests at this level and in nested scale runs."""
    manifests: list[dict[str, Any]] = []
    if isinstance(document.get("backend"), dict):
        manifests.append(document)
    runs = document.get("runs")
    if isinstance(runs, list):
        for run in runs:
            if isinstance(run, dict):
                manifests.extend(shared_manifests(run))
    return manifests


def main() -> int:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <results-dir>", file=sys.stderr)
        return 2
    root = Path(sys.argv[1])
    checked = 0
    try:
        for path in sorted(root.rglob("*.json")):
            documents = load_documents(path)
            for manifest in documents:
                # RenderMan and integration reports deliberately use their own
                # schemas; shared manifests may also be nested in scale-sweep
                # report wrappers.
                for shared in shared_manifests(manifest):
                    validate_manifest(path, shared)
                    checked += 1
    except (OSError, ValueError) as error:
        print(error, file=sys.stderr)
        return 1
    print(json.dumps({"schema_version": 1, "checked_manifests": checked}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
