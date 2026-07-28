# Unreal and Cycles integration gates

Run the host-only gate with:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run integration-probe
jq . results/integrations/2026-07-28-host-gates.json
```

The probe records what is actually installed and refuses to reinterpret an
older engine as the requested target. On this host it finds an Unreal
`4.19.2-release` checkout, which is useful for historical RHI inspection but
does not satisfy the M11 pinned UE5 requirement. It also finds Blender 5.2.0
LTS as an executable, but no Blender/Cycles source tree for M12's Metal and
OptiX device-layer changes.

These are environment gates, not claims that the integrations are impossible.
Closing them requires a pinned UE5 checkout/sample project and a pinned Blender
source revision; only then should importer, RDG, shader-table, Cycles device,
motion, and mixed-scene work be implemented or measured.
