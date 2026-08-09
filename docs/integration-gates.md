# Unreal and Cycles integration gates

Run the host-only gate with:

```sh
CYCLES_SOURCE_ROOT=/Users/briangyss/src/cycles \
BLENDER_SOURCE_ROOT=/Users/briangyss/src/blender \
XDG_CACHE_HOME="$PWD/.cache" mise run integration-probe
jq . results/integrations/2026-08-08-host-gates.json
```

Verify the prepared source/build environment with:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run cycles-verify
jq . results/integrations/2026-08-08-cycles-verification.json
```

The probe records what is actually installed and refuses to reinterpret an
older engine as the requested target. On this host it finds an Unreal
`4.19.2-release` checkout, which is useful for historical RHI inspection but
does not satisfy the M11 pinned UE5 requirement. It recognizes the pinned
standalone Cycles checkout as adequate for core/device setup, while separately
reporting whether a clean Blender source checkout is available for UI and debug
views.

The pinned macOS setup is recorded separately in
`results/integrations/2026-08-08-cycles-setup.json`. It proves a standalone
Cycles CPU build and upstream test, plus a Blender developer/debug configure
with the UI and Cycles enabled against Blender's precompiled arm64 dependency
bundle, and a focused `bf_intern_cycles` library compile. Keeping this evidence
separate prevents a later host probe from overwriting the build record.

These are environment gates, not claims that the integrations are impossible.
The standalone checkout does not close M12: device implementation remains
deferred until M9 selects the retained representation, and OptiX needs a
qualified NVIDIA host. The clean pinned Blender checkout and successful debug
configure make Blender UI/debug-view scaffolding a candidate now; they do not
select a retained device method or prove a renderer result. Importer, RDG,
shader-table, Cycles device, motion, and mixed-scene work still require their
roadmap gates and measured proof.
