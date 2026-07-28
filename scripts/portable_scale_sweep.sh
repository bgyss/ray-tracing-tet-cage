#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 2 ]]; then
  printf 'usage: %s <asset.tetcage> <output.json>\n' "$0" >&2
  exit 2
fi

asset_path="$1"
output_path="$2"
benchmark_bin="${TETCAGE_BENCHMARK_BIN:-build/nix/tetcage_benchmark}"
tmp_dir="$(mktemp -d "${TMPDIR:-/tmp}/tetcage-portable-scale.XXXXXX")"
trap 'rm -rf "$tmp_dir"' EXIT

exit_codes=()
for copies in 1 4 16 64; do
  set +e
  "$benchmark_bin" "$asset_path" "$tmp_dir/run-${copies}.json" \
    --scene "portable-scale-${copies}" \
    --seed 81002718 \
    --copies "$copies" \
    --rays 256 \
    --warmup 1 \
    --iterations 5 \
    --motion 0.05 \
    --visible 1
  exit_codes+=("$?")
  set -e
done

mkdir -p "$(dirname "$output_path")"
printf '%s\n' "${exit_codes[@]}" | jq -R -s 'split("\n") | map(select(length > 0) | tonumber)' \
  >"$tmp_dir/exit-codes.json"
jq -sS --arg asset "$asset_path" \
  --slurpfile exit_codes "$tmp_dir/exit-codes.json" \
  '{schema_version: 1,
    report_kind: "portable_scale_sweep",
    evidence_class: "synthetic",
    asset: $asset,
    ray_count: 256,
    copies: [1, 4, 16, 64],
    runs: .,
    exit_codes: $exit_codes[0],
    claim_boundary: "CPU/stub scale evidence only; no GPU crossover or production-scale claim"}' \
  "$tmp_dir"/run-*.json \
  >"$output_path"
