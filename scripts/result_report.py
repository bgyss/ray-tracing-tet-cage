#!/usr/bin/env python3
"""Build deterministic JSON and Markdown summaries from checked-in results."""

from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any


def load_documents(path: Path) -> list[dict[str, Any]]:
    text = path.read_text(encoding="utf-8")
    try:
        value = json.loads(text)
    except json.JSONDecodeError:
        documents: list[dict[str, Any]] = []
        for line_number, line in enumerate(text.splitlines(), 1):
            if not line.strip():
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as error:
                raise ValueError(f"{path}: line {line_number} is not JSON: {error}") from error
            if not isinstance(value, dict):
                raise ValueError(f"{path}: line {line_number} is not a JSON object")
            documents.append(value)
        return documents
    return [value] if isinstance(value, dict) else []


def number(value: Any) -> int | float | None:
    return value if isinstance(value, (int, float)) and not isinstance(value, bool) else None


def shared_record(path: str, document: dict[str, Any], run_index: int | None = None) -> dict[str, Any]:
    backend = document.get("backend")
    scene = document.get("scene")
    timings = document.get("timings_ms")
    memory = document.get("memory_bytes")
    correctness = document.get("correctness")
    source = document.get("source")
    if not isinstance(backend, dict):
        backend = {}
    if not isinstance(scene, dict):
        scene = {}
    if not isinstance(timings, dict):
        timings = {}
    if not isinstance(memory, dict):
        memory = {}
    if not isinstance(correctness, dict):
        correctness = {}
    if not isinstance(source, dict):
        source = {}
    record: dict[str, Any] = {
        "path": path,
        "run_index": run_index,
        "run_id": document.get("run_id"),
        "api": backend.get("api"),
        "status": backend.get("status"),
        "evidence_class": document.get("evidence_class"),
        "scene": scene.get("id"),
        "scene_hash": scene.get("hash"),
        "rays": number(correctness.get("rays")),
        "misses": number(correctness.get("misses")),
        "duplicate_hits": number(correctness.get("duplicate_hits")),
        "wrong_ownership": number(correctness.get("wrong_ownership")),
        "total_ms": number(timings.get("total")),
        "peak_total_bytes": number(memory.get("peak_total")),
        "source_commit": source.get("commit"),
        "source_dirty": source.get("dirty"),
        "failure_count": len(document.get("failures", []))
        if isinstance(document.get("failures"), list)
        else 0,
    }
    return record


def classify_document(path: str, document: dict[str, Any]) -> tuple[str, list[dict[str, Any]]]:
    if isinstance(document.get("backend"), dict):
        return "shared_manifest", [shared_record(path, document)]
    runs = document.get("runs")
    if isinstance(runs, list):
        records = [
            shared_record(path, run, index)
            for index, run in enumerate(runs)
            if isinstance(run, dict) and isinstance(run.get("backend"), dict)
        ]
        if records:
            return "scale_sweep", records
    if "probe_kind" in document:
        return "capability", []
    if "clip_kind" in document or "transition_policy" in document:
        return "authoring", []
    if "decision" in document or "render_root" in document:
        return "feasibility", []
    if "integrations" in path or "host-gates" in path:
        return "integration", []
    return "other", []


def build_summary(root: Path) -> dict[str, Any]:
    files: list[dict[str, Any]] = []
    records: list[dict[str, Any]] = []
    for path in sorted(root.rglob("*.json")):
        relative = path.relative_to(root).as_posix()
        if relative == "reproducibility-summary.json":
            continue
        documents = load_documents(path)
        kinds: list[str] = []
        file_records: list[dict[str, Any]] = []
        for document in documents:
            kind, discovered = classify_document(relative, document)
            kinds.append(kind)
            file_records.extend(discovered)
        records.extend(file_records)
        files.append(
            {
                "path": relative,
                "documents": len(documents),
                "kinds": sorted(set(kinds)),
                "shared_records": len(file_records),
            }
        )

    api_counts = Counter(record["api"] or "unavailable" for record in records)
    evidence_counts = Counter(record["evidence_class"] or "unavailable" for record in records)
    status_counts = Counter(record["status"] or "unavailable" for record in records)
    failures = sum(record["failure_count"] for record in records)
    return {
        "schema_version": 1,
        "generator": "scripts/result_report.py",
        "results_root": root.as_posix(),
        "file_count": len(files),
        "shared_manifest_count": len(records),
        "shared_manifest_failure_count": failures,
        "api_counts": dict(sorted(api_counts.items())),
        "evidence_class_counts": dict(sorted(evidence_counts.items())),
        "status_counts": dict(sorted(status_counts.items())),
        "shared_manifests": sorted(records, key=lambda record: (record["path"], record["run_index"] or -1)),
        "files": files,
    }


def markdown(summary: dict[str, Any]) -> str:
    lines = [
        "# Generated result summary",
        "",
        "This file is generated by `scripts/result_report.py`; regenerate it with",
        "`XDG_CACHE_HOME=\"$PWD/.cache\" mise run report`. It summarizes checked",
        "manifests and does not promote synthetic or partial evidence to a",
        "production claim.",
        "",
        f"- JSON files scanned: {summary['file_count']}",
        f"- Shared manifest records: {summary['shared_manifest_count']}",
        f"- Recorded failure entries: {summary['shared_manifest_failure_count']}",
        "",
        "## Shared manifests",
        "",
        "| File | API | Status | Evidence | Scene | Rays | Misses | Total ms | Peak bytes |",
        "| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for record in summary["shared_manifests"]:
        suffix = "" if record["run_index"] is None else f" (run {record['run_index']})"
        values = [
            f"`{record['path']}{suffix}`",
            str(record["api"] or "unavailable"),
            str(record["status"] or "unavailable"),
            str(record["evidence_class"] or "unavailable"),
            str(record["scene"] or "unavailable"),
            str(record["rays"] if record["rays"] is not None else "unavailable"),
            str(record["misses"] if record["misses"] is not None else "unavailable"),
            str(record["total_ms"] if record["total_ms"] is not None else "unavailable"),
            str(record["peak_total_bytes"] if record["peak_total_bytes"] is not None else "unavailable"),
        ]
        lines.append("| " + " | ".join(values) + " |")
    lines.extend(
        [
            "",
            "## Non-shared artifacts",
            "",
            "The JSON file inventory is retained in the companion machine-readable",
            "summary so capability, authoring, feasibility, and integration reports",
            "remain visible without being forced into the shared benchmark schema.",
            "",
        ]
    )
    for file in summary["files"]:
        if file["shared_records"] == 0:
            lines.append(f"- `{file['path']}` ({', '.join(file['kinds']) or 'other'})")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("results_root", type=Path)
    parser.add_argument("--json-out", type=Path)
    parser.add_argument("--markdown-out", type=Path)
    args = parser.parse_args()
    summary = build_summary(args.results_root)
    rendered_json = json.dumps(summary, indent=2, sort_keys=True) + "\n"
    rendered_markdown = markdown(summary)
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(rendered_json, encoding="utf-8")
    else:
        print(rendered_json, end="")
    if args.markdown_out:
        args.markdown_out.parent.mkdir(parents=True, exist_ok=True)
        args.markdown_out.write_text(rendered_markdown, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
