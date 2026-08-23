# Developer environment

Nix is the authoritative source of portable build and validation tools. Mise is
the task front end: every mise task enters the pinned Nix environment, so there
is only one toolchain definition to maintain.

## Prerequisites

- Install Nix with flakes enabled.
- Install mise to use the short task names.
- On macOS, install Xcode or the Xcode Command Line Tools. The Nix shell
  deliberately selects `/usr/bin/clang` and `/usr/bin/clang++` because Apple
  frameworks, Objective-C++, and Metal are supplied by Xcode rather than
  nixpkgs.

The shell contains CMake, Ninja, clang-format/clang-tidy, Python, jq,
ShellCheck, pkg-config, Vulkan headers and loader, glslang, shaderc, and SPIR-V
tools. CUDA, GPU drivers, Blender/Cycles, RenderMan, and Xcode's optional
offline Metal compiler are host integrations: the doctor reports them but does
not pretend Nix can provide or validate the target hardware.

## Mise workflow

```sh
MISE_DISABLE_VERSION_CHECK=1 mise run doctor
MISE_DISABLE_VERSION_CHECK=1 mise run configure
MISE_DISABLE_VERSION_CHECK=1 mise run build
MISE_DISABLE_VERSION_CHECK=1 mise run test
MISE_DISABLE_VERSION_CHECK=1 mise run check
MISE_DISABLE_VERSION_CHECK=1 mise run report
```

Other tasks are `format`, `format-check`, `nix-check`, `report`,
`renderman-probe`, `integration-probe`, and `cycles-verify`. `check` is the
normal local gate: it validates formatting and shell scripts, builds with the
`nix` CMake preset, runs CTest, and checks the working diff. `nix-check` also
builds the Metal-disabled portable package in a pure Nix derivation.
`report` regenerates deterministic JSON/Markdown summaries from `results/` and
retains partial or synthetic evidence labels.

The pinned external Cycles/Blender lane is verified separately because those
source trees and their macOS arm64 dependency payloads are host integrations:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run cycles-verify
jq . results/integrations/2026-08-09-cycles-verification.json
```

The first MetalRT entry proof and the repository-side procedural tet-cage path
are recorded in
`results/integrations/2026-08-23-cycles-metal-entry.json`. This is a direct
Metal baseline plus standalone AABB/custom-intersection evidence; Cycles
renderer ingestion and Blender asset UI remain later gates.

`cycles-verify` is read-only. It checks the pinned source and dependency
revisions, clean Git/LFS state, hydrated LFS file counts, the standalone Cycles
CTest/runtime result, the Blender developer/debug CMake cache profile, the
full installed Blender executable, and an isolated Python/Cycles UI smoke. Use
the environment variables in `scripts/cycles_verify.py` to point at another
pinned host setup; a passing result still does not claim a rendered tet-cage
scene, MetalRT, OptiX, or device performance integration.

CTest writes a `Testing/Temporary` log even for this one-test Cycles build. If
the external build tree is read-only, the verifier records the permission
boundary and runs the same generated `cycles_version` test metadata in a
writable temporary mirror.

The macOS app-bundle lane is built and installed with:

```sh
cmake --build /Users/briangyss/src/build_blender_tetcage_debug_make \
  --target blender --parallel 8
cmake --install /Users/briangyss/src/build_blender_tetcage_debug_make \
  --config Debug --prefix /Users/briangyss/src/build_blender_tetcage_debug_make/bin
```

For an isolated windowed UI launch, point `BLENDER_USER_CONFIG` and
`BLENDER_USER_SCRIPTS` at temporary directories and run the installed
`Blender.app/Contents/MacOS/Blender` with `--factory-startup` and
`scripts/blender_ui_smoke.py`. The smoke switches a fresh scene to the Cycles
engine and verifies the Cycles/OSL build options without touching user files.

To exercise the Blender-side asset parser and ordinary mesh/wireframe fallback:

```sh
BLENDER_BINARY=/path/to/Blender \
python3 tests/blender_import_contract.py
```

The importer creates a normal Cycles mesh and a visible wireframe cage, installs
a dependency-graph update handler for cage edits, and supports save/reload of
the fallback scene. It is an authoring/debug fallback; it does not claim that
the procedural MetalRT primitive is active in Blender.

## Direct Nix workflow

Enter an interactive shell:

```sh
nix develop path:.
```

Or run the complete repository check without entering one:

```sh
nix develop path:. --command bash scripts/check.sh nix
nix flake check path:.
```

`path:.` is intentional during active development: unlike the implicit Git
flake form, it includes untracked files in the source tree. The committed
`flake.lock` pins nixpkgs. Refresh it only as an explicit dependency update:

```sh
nix flake update path:.
```

## CMake without mise

Inside `nix develop`, the equivalent commands are:

```sh
cmake --preset nix
cmake --build --preset nix
ctest --preset nix
```

The existing `dev` preset remains the direct Xcode-clang lane on macOS, and
`portable` remains the default-compiler fallback. Direct GPU evidence still
requires a matching physical device, driver, runtime, and SDK; successful Nix
builds do not satisfy those roadmap gates.

## Cage authoring and quality checks

The portable authoring tools provide a reproducible baseline and a proof-gated
quality report. Generate an AABB-plus-center starter cage (the result is a
starting point, not a production-quality deformation cage), compile it, and
sample its conditioning and affine residuals:

```sh
tetcage_cage_generate mesh.obj build/generated-baseline.cage
tetcage_cage_refine build/generated-baseline.cage build/refined.cage --levels 1
tetcage_asset_compiler mesh.obj build/generated-baseline.cage build/asset.tetcage
tetcage_cage_quality build/asset.tetcage results/authoring/cage-quality.json \
  --samples 32 --motion 0.05
```

The quality report records per-sample minimum edge length, condition estimate,
mirrors/near-singular tetrahedra, boundary-fragment counts, and the direct
barycentric-versus-affine residual. `suitable: false` is an explicit fallback
recommendation; it does not claim that a baseline cage represents an animation
well. Refinement is conforming: shared edge midpoints are emitted once with
stable IDs and each tet is split into eight orientation-preserving children.
`tetcage_cage_animation` provides a deterministic non-affine diagnostic clip,
and `tetcage_cage_lods` emits parent-mapped whole-level cages; neither
substitutes for production clip transition evidence. Full clip residuals still
require an animation-aware source deformation input.

If the sandbox makes the global Nix cache read-only, keep the cache local to the
checkout for a task invocation:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
```

The checked-in `.github/workflows/portable.yml` runs the same portable check
on Ubuntu using the flake as the authoritative environment. That CI job proves
portable build/test reproducibility only; device-specific Metal, NVIDIA, and
renderer gates still require their documented hosts.

The optional CUDA/Vulkan interop gate is recorded without treating tool
presence as a performance result:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run interop-probe
jq . results/capabilities/2026-07-28-cuda-vulkan-interop.json
```

The probe keeps the M7 decision at `remove_from_production_path` until matched
device UUIDs, external memory/semaphore ownership, equivalent kernels, and
end-to-end timings are measured on NVIDIA hardware.

The M9 representation policy is also available as a deterministic CLI. It
chooses rigid, conventional dynamic, tet-cage, or hybrid treatment from
measured asset quality, workload, boundary-fallback, and backend-correctness
inputs:

```sh
tetcage_method_select out.json 1000 16 64 1000 0.0001 0.001 0.05 1 1 1
```

The checked-in two-tet evaluation deliberately selects conventional dynamic
geometry because the direct Metal residual is outside the declared threshold;
the policy is a fallback decision, not a performance claim.

The neutral runtime safety policy can be exercised against a compiled asset:

```sh
tetcage_runtime_policy build/nix/one-tet.tetcage \
  results/runtime/runtime-policy.json
```

It records explicit cancellation, allocation-limit, unsupported-update,
device-loss, and reset fallbacks. This does not claim that a real GPU driver
has been reset successfully.

For deterministic offline builds with a content-addressed cache:

```sh
XDG_CACHE_HOME="$PWD/.cache" nix develop path:. --command bash \
  scripts/asset_cache.sh tests/assets/one-tet.obj tests/assets/one-tet.cage \
  /tmp/one-tet.tetcage .cache/asset-cache /tmp/one-tet-cache-report.json
```
