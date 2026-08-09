#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
scratch_dir="$(mktemp -d "${TMPDIR:-/tmp}/tetcage-integration-probe.XXXXXX")"
trap 'rm -rf -- "$scratch_dir"' EXIT

cycles_root="$scratch_dir/cycles"
mkdir -p "$cycles_root/src/device/metal" "$cycles_root/src/device/optix" "$cycles_root/.git"
touch "$cycles_root/CMakeLists.txt"

output="$scratch_dir/probe.json"
CYCLES_SOURCE_ROOT="$cycles_root" BLENDER_SOURCE_ROOT="" \
  bash "$repo_dir/scripts/integration_probe.sh" >"$output"

python3 - "$output" "$cycles_root" <<'PY'
import json
import pathlib
import sys

payload = json.loads(pathlib.Path(sys.argv[1]).read_text())
cycles = payload["blender_cycles"]
assert cycles["source_root"] == sys.argv[2]
assert cycles["source_kind"] == "standalone_cycles"
assert cycles["cycles_source_present"] is True
assert cycles["device_core_status"] == "candidate"
assert cycles["blender_ui_status"] == "blocked"
PY

printf 'integration probe contract: pass\n'
