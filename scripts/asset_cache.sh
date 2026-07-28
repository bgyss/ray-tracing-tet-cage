#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 5 ]]; then
  printf 'usage: %s <mesh.obj> <cage.cage> <output.tetcage> <cache-dir> <report.json>\n' "$0" >&2
  exit 2
fi

mesh_path="$1"
cage_path="$2"
output_path="$3"
cache_dir="$4"
report_path="$5"
compiler_bin="${TETCAGE_ASSET_COMPILER_BIN:-build/nix/tetcage_asset_compiler}"
epsilon="${TETCAGE_CACHE_EPSILON:-}"

mesh_hash="$(sha256sum "$mesh_path" | awk '{print $1}')"
cage_hash="$(sha256sum "$cage_path" | awk '{print $1}')"
key_material="asset-cache-v1\n${mesh_hash}\n${cage_hash}\nepsilon=${epsilon}"
cache_key="$(printf '%b' "$key_material" | sha256sum | awk '{print $1}')"
cache_asset="$cache_dir/$cache_key.tetcage"

mkdir -p "$cache_dir" "$(dirname "$output_path")" "$(dirname "$report_path")"
cache_hit=false
if [[ -f "$cache_asset" ]]; then
  cp "$cache_asset" "$output_path"
  cache_hit=true
else
  if [[ -n "$epsilon" ]]; then
    "$compiler_bin" "$mesh_path" "$cage_path" "$output_path" --epsilon "$epsilon" >/dev/null
  else
    "$compiler_bin" "$mesh_path" "$cage_path" "$output_path" >/dev/null
  fi
  cp "$output_path" "$cache_asset"
fi

output_hash="$(sha256sum "$output_path" | awk '{print $1}')"
jq -nS \
  --arg mesh "$mesh_path" \
  --arg cage "$cage_path" \
  --arg cache_key "$cache_key" \
  --arg output_hash "$output_hash" \
  --arg mesh_hash "$mesh_hash" \
  --arg cage_hash "$cage_hash" \
  --arg epsilon "$epsilon" \
  --argjson cache_hit "$cache_hit" \
  '{schema_version: 1,
    report_kind: "asset_cache",
    evidence_class: "portable_build",
    mesh: $mesh,
    cage: $cage,
    mesh_sha256: $mesh_hash,
    cage_sha256: $cage_hash,
    cache_key: $cache_key,
    cache_hit: $cache_hit,
    epsilon: (if $epsilon == "" then null else ($epsilon | tonumber) end),
    output_sha256: $output_hash,
    claim_boundary: "Cache identity covers input bytes and compiler tolerance; runtime performance is not inferred"}' \
  >"$report_path"
