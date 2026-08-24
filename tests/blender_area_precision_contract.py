#!/usr/bin/env python3
"""Guard the explicit large-area volume precision boundary."""

from __future__ import annotations

import json
import pathlib


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    probe = (root / "scripts/blender_native_tetcage_render_probe.py").read_text()
    differential = (root / "scripts/blender_native_differential.py").read_text()
    manifest = json.loads(
        (root / "results/integrations/2026-08-23-cycles-metal-entry.json").read_text()
    )
    artifact_path = root / "results/integrations/2026-08-23-cycles-area-volume-precision.json"
    artifact = json.loads(artifact_path.read_text())
    assert manifest["area_precision_artifact"] == artifact_path.relative_to(root).as_posix()
    assert artifact["status"] == "measured_open"

    assert "TETCAGE_AREA_SIZE" in probe
    assert "TETCAGE_DISABLE_SHADOWS" in probe
    assert "--area-size" in differential
    assert "--samples" in differential

    volume = manifest["blender_fallback_import"]["native_volume_render"]
    assert volume["static_query_dispatch"] is True
    sweep = volume["area_size_sweep"]
    assert sweep["2.0"]["result"] == "open_area_sampling_precision"
    assert sweep["2.0"]["max_abs_rgb_difference"] > 1.0e-3
    assert sweep["0.5"]["result"] == "numeric_close"
    assert sweep["0.5"]["max_abs_rgb_difference"] < 1.0e-4
    assert "large-footprint area-light seam remains open" in sweep["claim_boundary"]

    high_sample = volume["sample_sweep_64"]
    assert high_sample["samples"] == 64
    assert high_sample["area_size_2.0"]["result"] == "open_area_sampling_precision"
    assert high_sample["area_size_0.5"]["result"] == "open_area_sampling_precision"
    assert high_sample["point"]["result"] == "numeric_close"
    assert high_sample["area_size_2.0_shadows_disabled"]["result"] == (
        "open_area_sampling_precision"
    )
    assert "not explained by shadow traversal alone" in high_sample["claim_boundary"]
    assert artifact["controls"]["area_size_2.0"]["result"] == "open_area_sampling_precision"
    assert artifact["controls"]["point"]["result"] == "numeric_close"

    print("Blender area-light precision contract: pass")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
