#!/usr/bin/env bash
set -euo pipefail

if [[ "$#" -ne 7 ]]; then
  printf 'usage: %s <compiler> <mesh> <cage> <first-output> <second-output> <cache-dir> <report-dir>\n' "$0" >&2
  exit 2
fi

compiler="$1"
mesh="$2"
cage="$3"
first_output="$4"
second_output="$5"
cache_dir="$6"
report_dir="$7"
# CTest reuses a build directory across invocations. Reset only the explicit
# test-owned paths so the first invocation is always a cache miss.
rm -rf -- "$cache_dir" "$report_dir"
rm -f -- "$first_output" "$second_output"
mkdir -p "$report_dir"
script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TETCAGE_ASSET_COMPILER_BIN="$compiler" bash "$script_dir/../scripts/asset_cache.sh" \
  "$mesh" "$cage" "$first_output" "$cache_dir" "$report_dir/first.json"
TETCAGE_ASSET_COMPILER_BIN="$compiler" bash "$script_dir/../scripts/asset_cache.sh" \
  "$mesh" "$cage" "$second_output" "$cache_dir" "$report_dir/second.json"

jq -e '.cache_hit == false' "$report_dir/first.json" >/dev/null
jq -e '.cache_hit == true' "$report_dir/second.json" >/dev/null
cmp -s "$first_output" "$second_output"
printf '{"schema_version":1,"cache_miss":true,"cache_hit":true,"byte_identical":true}\n'
