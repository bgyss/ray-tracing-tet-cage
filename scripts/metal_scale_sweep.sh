#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  printf 'usage: %s <asset.tetcage> <output.json>\n' "$0" >&2
  exit 2
fi

asset="$1"
output="$2"
ray_count="${METAL_SWEEP_RAYS:-256}"
copy_counts="${METAL_SWEEP_COPIES:-1 4 16 64}"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/tetcage-metal-sweep.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

mkdir -p "$(dirname "$output")"
asset_json="$(jq -Rn --arg value "$asset" '$value')"
printf '{\n  "schema_version": 1,\n  "asset": %s,\n  "rays": %s,\n  "boundary_policy": "cpu_fallback_experimental",\n  "instance_generation": "gpu_compute_descriptor",\n  "runs": [' "$asset_json" "$ray_count" >"$output"
first=true
for copies in $copy_counts; do
  manifest="$tmp_dir/$copies.json"
  set +e
  ./build/dev/tetcage_metal_fast_path "$asset" "$manifest" \
    --copies "$copies" --rays "$ray_count" --motion 0.05 --compact \
    --gpu-instances --boundary-fallback
  exit_code=$?
  set -e
  if [[ "$first" == true ]]; then
    first=false
  else
    printf ',' >>"$output"
  fi
  jq -c --argjson exit_code "$exit_code" '. + {exit_code: $exit_code}' "$manifest" >>"$output"
done
printf '],\n  "evidence_class": "direct_synthetic_scale_subset"\n}\n' >>"$output"
