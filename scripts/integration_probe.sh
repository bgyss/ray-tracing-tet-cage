#!/usr/bin/env bash
set -euo pipefail

ue_root="${UNREAL_ROOT:-/Users/briangyss/src/UnrealEngine}"
blender_binary="${BLENDER_BINARY:-/Applications/Blender.app/Contents/MacOS/blender}"
blender_source="${BLENDER_SOURCE_ROOT:-}"

ue_present=false
ue_revision=""
ue_target="blocked"
if [[ -d "$ue_root/.git" ]]; then
  ue_present=true
  ue_revision="$(git -C "$ue_root" describe --tags --always 2>/dev/null || true)"
  if [[ "$ue_revision" == 5.* || "$ue_revision" == ue5-* ]]; then
    ue_target="candidate"
  fi
fi

blender_present=false
blender_version=""
if [[ -x "$blender_binary" ]]; then
  blender_present=true
  blender_version="$("$blender_binary" --version 2>/dev/null | head -n 1 || true)"
fi
cycles_source_present=false
if [[ -n "$blender_source" && -d "$blender_source/source/blender" ]]; then
  cycles_source_present=true
fi

cat <<JSON
{
  "schema_version": 1,
  "unreal": {
    "root": "$ue_root",
    "checkout_present": $ue_present,
    "revision": "$ue_revision",
    "ue5_target": "$ue_target",
    "status": "blocked",
    "reason": "The discovered checkout is Unreal 4.19.2; M11 requires a pinned UE5 RHI/renderer proof."
  },
  "blender_cycles": {
    "binary": "$blender_binary",
    "binary_present": $blender_present,
    "version": "$blender_version",
    "source_root": "$blender_source",
    "cycles_source_present": $cycles_source_present,
    "status": "blocked",
    "reason": "A Blender binary is present, but no pinned Blender/Cycles source checkout was supplied for device-layer integration."
  }
}
JSON
