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
`results/integrations/2026-08-08-cycles-setup.json`. The current full-build and
runtime evidence is in
`results/integrations/2026-08-09-blender-ui-build.json`: it proves the complete
`blender` target, an explicit app-bundle install (including `Resources/lib`),
and an isolated Python/Cycles smoke in both the background and windowed launch
paths. The Xcode 26 SDK emits availability diagnostics for Blender's pinned
11.2 deployment target, so the developer cache explicitly downgrades
`unguarded-availability-new` from an error; this is a build-environment
compatibility setting, not a renderer/device claim.

The current candidate-profile rerun is recorded in
`results/integrations/2026-08-23-cycles-verification-candidate.json`. It uses
the clean pinned source trees for revision/LFS checks and the isolated native
candidate app bundle for the normalized Debug cache, CPU/UI smoke, and
tet-cage import/save/reload contract.

These are environment gates, not claims that the integrations are impossible.
The standalone checkout does not close M12: device implementation remains
deferred until M9 selects the retained representation, and OptiX needs a
qualified NVIDIA host. The clean pinned Blender checkout and successful full
executable/UI smoke make Blender UI/debug-view scaffolding executable now; they
do not select a retained device method or prove a renderer result. Importer, RDG,
shader-table, Cycles device, motion, and mixed-scene work still require their
roadmap gates and measured proof.
