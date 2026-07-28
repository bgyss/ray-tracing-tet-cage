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
`renderman-probe`, and `integration-probe`. `check` is the
normal local gate: it validates formatting and shell scripts, builds with the
`nix` CMake preset, runs CTest, and checks the working diff. `nix-check` also
builds the Metal-disabled portable package in a pure Nix derivation.
`report` regenerates deterministic JSON/Markdown summaries from `results/` and
retains partial or synthetic evidence labels.

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
