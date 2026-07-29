#!/usr/bin/env python3
"""Check the portable WP0 corpus metadata and checked-in goldens."""

from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        return 2
    root = Path(sys.argv[1])
    manifest = json.loads((root / "assets/manifest.json").read_text(encoding="utf-8"))
    assert manifest["schema_version"] == 1
    assert manifest["version"] == "wp0-corpus-v1"
    assert manifest["coordinate_convention"]
    assert len(manifest["source_assets"]) >= 8
    for source in manifest["source_assets"]:
        for field in (
            "path",
            "sha256",
            "license",
            "origin",
            "scale",
            "coordinate_convention",
            "expected_compiler_outcome",
        ):
            assert source[field]
        path = root / source["path"]
        assert hashlib.sha256(path.read_bytes()).hexdigest() == source["sha256"]
    accepted = 0
    rejected = 0
    source_outcomes = {
        source["path"]: source["expected_compiler_outcome"] for source in manifest["source_assets"]
    }
    for case in manifest["cases"]:
        assert (root / case["mesh"]).is_file()
        assert (root / case["cage"]).is_file()
        assert case["expected_compiler_outcome"]
        assert source_outcomes[case["mesh"]] == "accepted"
        assert source_outcomes[case["cage"]] == case["expected_compiler_outcome"]
        if case["expected_compiler_outcome"] == "accepted":
            accepted += 1
            assert (root / case["golden"]).is_file()
            if "required_boundary_cases" in case:
                assert len(case["required_boundary_cases"]) == 4
                assert case["minimum_occupied_tetrahedra"] >= 2
                assert case["minimum_boundary_fragments"] >= 1
        else:
            rejected += 1
    assert accepted == 4
    assert rejected == 3
    print(json.dumps({"schema_version": 1, "cases": len(manifest["cases"]), "accepted": accepted}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
