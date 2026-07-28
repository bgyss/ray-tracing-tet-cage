# Ray tracing animated geometry with tetrahedral cages

This repository is a reimplementation planning workspace for:

> Holger Gruen, Carsten Benthin, Michael Kern, and David McAllister.
> "Ray Tracing Massive Amounts of Animated Geometry."
> *Proceedings of the ACM on Computer Graphics and Interactive Techniques* 9,
> 4, Article 49 (July 2026).
> [DOI: 10.1145/3820014](https://doi.org/10.1145/3820014)

The paper describes a two-level ray-tracing representation that clips dense
rest-pose geometry into a low-resolution tetrahedral cage. Static acceleration
structures hold the clipped geometry, while unique animations update only cage
vertices and top-level instance transforms.

## Planning packet

- [Paper analysis and reconstruction](docs/paper-analysis.md) explains the
  algorithm, equations, evidence, omissions, and correctness risks.
- [Architecture and reimplementation roadmap](docs/reimplementation-roadmap.md)
  maps the method to Metal, Vulkan/CUDA, Unreal Engine, Cycles, and RenderMan;
  it also defines milestones, validation, benchmarks, and optimization
  experiments.
- [Copy-ready milestone goal prompts](docs/goal-prompts.md) contains ordered
  implementation contracts intended for separate Codex tasks.

## Recommended execution order

Start with the CPU geometry pipeline and watertight oracle. Do not begin with
an engine plug-in: the expensive and failure-prone parts are clipping,
provenance, numerical consistency, deformation validation, and acceleration
structure scaling. Once those contracts pass, implement the Metal and Vulkan
backends independently, then use measured results to choose engine targets.

## Current implementation

Portable implementation work now includes:

- a C++20/CMake core with tetrahedral barycentrics, affine deformation,
  inverse-transpose normals, condition/mirror/singularity classification, and
  deterministic clipping;
- a versioned canonical asset compiler with source provenance, deterministic
  shared-boundary ownership, serialization, checksums, and inspection;
- real Metal and Vulkan capability-report executables (Vulkan reports
  unverified when no SDK/loader is present);
- a versioned result-manifest schema and proof-oriented requirements matrix;
- an independent CPU fast/4D oracle, runtime/stub benchmark manifests, and
  deliberate-corruption detection;
- reproducible cage-generation and cage-quality analysis tools that report
  conditioning, residuals, explicit fallback reasons, and deterministic
  conforming cage refinement, plus a clip-wide non-affine animation evaluator
  with constrained cage-weight fitting and parent-mapped cage LOD generation;
- a bounded, finite-value-checked asset loader with malformed-stream tests and
  a v1 golden serialization fixture;
- a measured-input representation-selection policy that chooses rigid,
  conventional dynamic, tet-cage, or hybrid fallback modes without turning
  synthetic or unproven GPU results into performance claims;
- a scale/ULP/edge/conditioning-aware tolerance policy with explicit
  conservative-boundary and conventional-fallback decisions;
- analytical, all-permutation, malformed-input, deterministic-build, and seeded
  differential tests.

The recommended developer workflow uses the pinned Nix environment through
mise:

```sh
MISE_DISABLE_VERSION_CHECK=1 mise run doctor
MISE_DISABLE_VERSION_CHECK=1 mise run check
MISE_DISABLE_VERSION_CHECK=1 mise run report
```

Nix owns CMake, Ninja, formatting, shell/JSON/Python utilities, and
Vulkan/SPIR-V tooling. On macOS it deliberately delegates Objective-C++, Apple
frameworks, and Metal to Xcode clang. See
[the developer environment guide](docs/development.md) for direct Nix commands,
tool ownership, formatting, result-summary regeneration, and optional GPU/DCC
dependencies.

Configure, build, and test directly on macOS:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Use `cmake --preset portable` on other hosts. Run `bash scripts/check.sh` for
the complete repo-native check. The current proof state and unresolved hardware
gates are recorded in [the requirements matrix](docs/requirements-matrix.md)
and [platform support policy](docs/platform-support.md).

The portable proof inventory is summarized in the
[reproducibility report](docs/reproducibility.md), which links checked-in
manifests and identifies hardware, source-tree, and license gates that remain
unproven.

The [authoring guide](docs/authoring.md) shows how to run the deterministic
clip evaluator. Its procedural clip is a diagnostic and does not substitute for
representative production animation.

The standalone Metal fast path now builds real immutable micro-BLAS objects,
compacts them, builds a TLAS, runs a runtime MSL ray-query kernel, and emits
direct-device manifests. Its current one-tet probe matches interior rays but
still reports boundary/grazing-ray differences, so it remains a measured
partial result rather than a completed M5 gate. Vulkan/NVIDIA, CUDA, Unreal,
Cycles, and RenderMan remain explicitly hardware/source/license gated. No
production-scale performance, watertightness, or renderer integration claim is
made until its original roadmap exit gate has direct evidence under `results/`.
