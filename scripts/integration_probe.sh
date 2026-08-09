#!/usr/bin/env bash
set -euo pipefail

ue_root="${UNREAL_ROOT:-/Users/briangyss/src/UnrealEngine}"
blender_binary="${BLENDER_BINARY:-/Applications/Blender.app/Contents/MacOS/blender}"
blender_source="${BLENDER_SOURCE_ROOT:-}"
cycles_source="${CYCLES_SOURCE_ROOT:-}"

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
source_root=""
source_kind="none"
cycles_source_present=false
device_core_status="blocked"
blender_source_present=false
blender_source_clean=false
blender_revision=""
blender_ui_status="blocked"

if [[ -n "$cycles_source" && -f "$cycles_source/CMakeLists.txt" && \
    -d "$cycles_source/src/device/metal" && -d "$cycles_source/src/device/optix" ]]; then
  source_root="$cycles_source"
  source_kind="standalone_cycles"
  cycles_source_present=true
  device_core_status="candidate"
elif [[ -n "$blender_source" && -f "$blender_source/CMakeLists.txt" && \
      -d "$blender_source/intern/cycles" ]]; then
  source_root="$blender_source"
  source_kind="blender_embedded_cycles"
  cycles_source_present=true
  device_core_status="candidate"
fi

if [[ -n "$blender_source" && -f "$blender_source/CMakeLists.txt" && \
    -d "$blender_source/source/blender" && -d "$blender_source/intern/cycles" ]]; then
  blender_source_present=true
  if [[ -d "$blender_source/.git" ]]; then
    blender_revision="$(git -C "$blender_source" rev-parse HEAD 2>/dev/null || true)"
    if [[ -z "$(git -C "$blender_source" status --porcelain 2>/dev/null || true)" ]]; then
      blender_source_clean=true
      blender_ui_status="candidate"
    fi
  fi
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
    "source_root": "$source_root",
    "source_kind": "$source_kind",
    "cycles_source_present": $cycles_source_present,
    "device_core_status": "$device_core_status",
    "blender_source_root": "$blender_source",
    "blender_source_present": $blender_source_present,
    "blender_source_clean": $blender_source_clean,
    "blender_revision": "$blender_revision",
    "blender_ui_status": "$blender_ui_status",
    "status": "$device_core_status",
    "reason": "A standalone Cycles checkout can support core and device-layer setup; Blender UI/debug work requires a separately pinned, clean Blender source checkout. MetalRT and OptiX implementation remain deferred until M9 selects the retained method and an NVIDIA host qualifies OptiX."
  }
}
JSON
