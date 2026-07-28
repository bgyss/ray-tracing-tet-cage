#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_dir}"

fail() {
  printf 'tooling contract: %s\n' "$*" >&2
  exit 1
}

require_file() {
  [[ -f "$1" ]] || fail "missing $1"
}

require_text() {
  local file="$1"
  local pattern="$2"
  grep -Eq "${pattern}" "${file}" || fail "${file} does not match ${pattern}"
}

require_file flake.nix
require_file mise.toml
require_file docs/development.md
require_file scripts/doctor.sh
require_file scripts/format.sh

for tool in cmake ninja clang-tools python3 jq shellcheck pkg-config \
  vulkan-headers vulkan-loader glslang spirv-tools shaderc; do
  require_text flake.nix "${tool}"
done
require_text flake.nix 'devShells'
require_text flake.nix 'checks'
require_text flake.nix 'packages'
require_text flake.nix '/usr/bin/clang'
require_text flake.nix 'TETCAGE_NIX_SHELL'

for task in doctor configure build test check format format-check; do
  require_text mise.toml "\\[tasks\\.${task}\\]"
done
require_text mise.toml 'nix develop path:\.'

require_text CMakePresets.json '"name":[[:space:]]*"nix"'
require_text CMakePresets.json '"generator":[[:space:]]*"Ninja"'
require_text scripts/check.sh 'TETCAGE_PRESET'
require_text docs/development.md 'mise run check'
require_text docs/development.md 'nix flake check path:\.'

printf 'tooling contract: pass\n'
