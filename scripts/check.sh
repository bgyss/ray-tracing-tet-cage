#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_dir}"

if [[ $# -gt 1 ]]; then
  printf 'usage: %s [cmake-preset]\n' "$0" >&2
  exit 2
fi

if [[ $# -eq 1 ]]; then
  TETCAGE_PRESET="$1"
elif [[ "${TETCAGE_NIX_SHELL:-0}" == "1" ]]; then
  TETCAGE_PRESET="nix"
elif [[ "$(uname -s)" == "Darwin" ]]; then
  TETCAGE_PRESET="dev"
else
  TETCAGE_PRESET="portable"
fi

export TETCAGE_PRESET

bash tests/tooling_contract.sh
bash scripts/format.sh --check
shellcheck scripts/*.sh tests/tooling_contract.sh
cmake --fresh --preset "${TETCAGE_PRESET}"
cmake --build --preset "${TETCAGE_PRESET}"
ctest --preset "${TETCAGE_PRESET}"
git diff --check
