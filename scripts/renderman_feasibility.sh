#!/usr/bin/env bash
set -euo pipefail

render_root="${RENDERMAN_ROOT:-/Applications/Pixar/RenderManProServer-26.2}"
header="$render_root/include/Riley.h"
if [[ ! -f "$header" ]]; then
  cat <<JSON
{"schema_version":1,"status":"blocked","evidence_class":"unavailable","render_root":"$render_root","reason":"Riley.h was not found"}
JSON
  exit 2
fi

count_matches() {
  local pattern="$1"
  rg -i -c "$pattern" "$header" 2>/dev/null || printf '0'
}

geometry_prototype="$(count_matches 'CreateGeometryPrototype')"
geometry_instance="$(count_matches 'CreateGeometryInstance')"
modify_instance="$(count_matches 'ModifyGeometryInstance')"
procedural_terms="$(count_matches 'procedural|procedural')"
intersection_terms="$(count_matches 'intersection|acceleration structure')"
ptrender_available=false
if [[ -x "$render_root/bin/ptrender" ]]; then
  ptrender_available=true
fi

cat <<JSON
{
  "schema_version": 1,
  "status": "partial",
  "evidence_class": "local_headers",
  "render_root": "$render_root",
  "header": "$header",
  "header_version": "26.2",
  "riley": {
    "create_geometry_prototype_matches": $geometry_prototype,
    "create_geometry_instance_matches": $geometry_instance,
    "modify_geometry_instance_matches": $modify_instance,
    "procedural_or_geometry_terms": $procedural_terms,
    "intersection_or_acceleration_terms": $intersection_terms
  },
  "runtime": {"ptrender_present": $ptrender_available},
  "decision": "no_go_for_public_api_fast_path_until_licensed_runtime_and_custom_intersection_control_are_proven",
  "limitations": [
    "Header inspection proves scene/prototype/instance transform concepts, not renderer execution.",
    "No public-header evidence here establishes direct BLAS/TLAS ownership or custom intersection shaders.",
    "A procedural bridge that expands dense triangles would not preserve the tet-instancing performance claim."
  ]
}
JSON
