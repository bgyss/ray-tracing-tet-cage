#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_dir}"

if ! command -v clang-format >/dev/null 2>&1; then
  printf 'format: clang-format is required; run through nix develop or mise\n' >&2
  exit 1
fi
if ! command -v nixfmt >/dev/null 2>&1; then
  printf 'format: nixfmt is required; run through nix develop or mise\n' >&2
  exit 1
fi

mode="${1:-apply}"
case "${mode}" in
  apply)
    find include src tests tools \
      -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o \
      -name '*.hpp' -o -name '*.m' -o -name '*.mm' -o -name '*.metal' \) \
      -print0 | xargs -0 clang-format -i
    nixfmt flake.nix
    printf 'format: applied\n'
    ;;
  --check)
    find include src tests tools \
      -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.h' -o \
      -name '*.hpp' -o -name '*.m' -o -name '*.mm' -o -name '*.metal' \) \
      -print0 | xargs -0 clang-format --dry-run --Werror
    nixfmt --check flake.nix
    printf 'format: pass\n'
    ;;
  *)
    printf 'usage: %s [--check]\n' "$0" >&2
    exit 2
    ;;
esac
