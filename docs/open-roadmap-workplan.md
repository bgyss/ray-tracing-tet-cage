# Open roadmap work plan

Status date: 2026-07-28

Baseline commit: `bfd6069`

Primary contracts:

- [`docs/reimplementation-roadmap.md`](reimplementation-roadmap.md)
- [`docs/requirements-matrix.md`](requirements-matrix.md)
- [`results/status/2026-07-28-goal-audit.md`](../results/status/2026-07-28-goal-audit.md)
- [`docs/integration-gates.md`](integration-gates.md)

This document surveys every roadmap milestone whose full exit gate is still
open, including milestones whose portable scaffolding is complete but whose
hardware, source, content, correctness, licensing, or upstream dependency gate
is not. It is an execution queue, not a claim that the remaining work can be
completed on the current machine.

## How to use this plan

Work one execution card at a time, in the order below unless a newly available
host or license makes an external gate temporarily cheaper to address.

For every card:

1. Run `mise run doctor` and `mise run check` before changing the implementation.
2. Preserve the current capability result rather than replacing an unavailable
   backend with a mock and calling the milestone complete.
3. Write raw measurements to a dated file under `results/`.
4. Record the exact commit, platform, device, driver, SDK, scene, ray corpus,
   warm-up count, measured repetitions, and fallback behavior.
5. Update the requirements matrix, reproducibility guide, and goal audit when
   the card's completion test passes.
6. Run `mise run check`, `mise run nix-check`, and `mise run report` before
   committing.

An implementation, compile check, schema check, size query, or synthetic smoke
test is useful evidence, but it does not close an exit gate that requires
guest-visible correctness, real hardware execution, a representative animated
asset, or an end-to-end renderer integration.

## Blocker classes

| Class | Meaning | Response |
| --- | --- | --- |
| Correctness | The implementation runs, but unexplained mismatches remain. | Minimize and fix the mismatch before scale or integration work. |
| Dependency | A later milestone needs evidence from an earlier one. | Complete the earliest failed dependency; do not build a second unvalidated stack. |
| Environment | Required hardware, driver, SDK, or operating system is absent. | Acquire and qualify a real host, then commit its capability manifest. |
| Source access | The binary is present, but the pinned source tree or SDK needed for integration is absent. | Obtain and pin the source revision before writing the adapter. |
| Content | Synthetic fixtures exist, but representative animated production inputs do not. | Acquire redistributable assets and add a hashed corpus manifest. |
| License or vendor API | The public interface does not expose the required operation, or use requires a license. | Run the smallest licensed feasibility experiment and seek a vendor answer before committing to the path. |
| Release | Core evidence exists, but conformance, legal, documentation, samples, or supported-host validation is incomplete. | Close only after the retained backends and integrations are known. |

## Full-roadmap status

| Milestone | Full exit gate | Current state | Blocker |
| --- | --- | --- | --- |
| M0 — environment and capability discovery | Both Apple and NVIDIA target environments identified and reproducible | Apple capability evidence exists; no qualified NVIDIA/Vulkan host | Environment |
| M1 — portable geometric oracle | Adversarial portable reference lane | Closed for the declared corpus | None; retain as the oracle |
| M2 — asset compiler | Deterministic compiler with topology, shading, ownership, malformed-input, migration, and image coverage | Core v1 compiler and tests exist; dense image and broader asset coverage remain | Content and test coverage |
| M3 — CPU deformed-surface baseline | Correct across a representative animated corpus | Correct on checked synthetic fixtures; production-scale corpus remains | Content |
| M4 — shared GPU contracts | Stable CPU/stub-tested layouts and policies | Closed for the current contract | None; update only through versioned migration |
| M5 — Metal fast path | All declared correctness rays pass, with dense animation, rebuild/refit policy, limits, and scale evidence | M1 Max corpus, dense refit/rebuild, and 65,536-instance standard/extended builds are measured; all 672 mismatch reductions preserve classification under real hardware replay, but the correctness-preserving CPU fallback handles 1,099/1,408 rays, so WP5 cost acceptance remains open | Fallback cost and method selection |
| M6 — Vulkan fast path | Validated Vulkan AS/ray-query path on NVIDIA | Capability probe only; Vulkan SDK/device and NVIDIA host absent | Environment, then implementation |
| M7 — CUDA/Vulkan interop | Measured interop advantage or explicit removal | No matched Vulkan/CUDA device; current decision is removal from production | M6 and environment |
| M8 — robustness and hybrid methods | Equal-work GPU comparison of hardware, exact 4D, and hybrid methods | Portable policy is tested; GPU experiments await retained GPU paths | M5 and M6 |
| M9 — scale, crossover, and method selection | Cross-platform, representative-asset crossover and optimization study | Synthetic Metal and portable selector evidence exists | M5, M6, M8, and content |
| M10 — cage authoring | Representative clips, visual quality, stable LOD transitions, and practical authoring workflow | Procedural clip, fitting, refinement, and parent-mapped LODs exist | Content and integration |
| M11 — Unreal Engine 5 | Pinned UE5 source integration on the target renderer/hardware | Only an unrelated Unreal 4.19.2 tree was discovered | Source access, Windows/NVIDIA host, M9 |
| M12 — Cycles | Pinned Cycles source integration for MetalRT and OptiX while retaining existing devices | Blender binary exists; no pinned source tree | Source access, M9, and NVIDIA for OptiX |
| M13 — RenderMan | Licensed feasibility result and explicit go/no-go | Public 26.2 headers expose Riley prototype/instance APIs but no proven custom-AS/intersection path | License or vendor API |
| M14 — production hardening | Supported backends and integrations pass conformance, CI, packaging, legal, samples, and documentation gates | Portable CI, fuzzing, deterministic reports, v1 golden/migration, cache smoke, and safety outcomes exist | All retained paths plus release work |

## Recommended one-at-a-time order

- [ ] WP0 — expand the compiler and CPU truth corpus (M2–M3)
- [ ] WP1 — close Metal correctness and dense-animation evidence (M5)
- [ ] WP2 — acquire and qualify one NVIDIA/Vulkan host (M0 prerequisite)
- [ ] WP3 — implement and validate the Vulkan fast path (M6)
- [ ] WP4 — make the CUDA/Vulkan interop keep-or-remove decision (M7)
- [ ] WP5 — compare hardware, exact 4D, and hybrid robustness methods (M8)
- [ ] WP6 — run the cross-platform scale and crossover study (M9)
- [ ] WP7 — validate production cage authoring and LOD transitions (M10)
- [ ] WP8 — integrate the retained method into Cycles (M12)
- [ ] WP9 — integrate the retained method into Unreal Engine 5 (M11)
- [ ] WP10 — close the RenderMan feasibility gate (M13)
- [ ] WP11 — harden and release the retained product surface (M14)

WP0 and WP1 can begin on the current Apple host. Preparatory source and asset
acquisition can occur in parallel, but implementation should still follow the
dependency order. In particular, do not start renderer-specific performance
integration before M9 establishes which method is worth integrating.

## WP0 — expand the compiler and CPU truth corpus

- **Roadmap coverage:** M2 and M3
- **Can start now:** Yes
- **Primary blocker:** Representative content and coverage

### Why it remains open

The v1 compiler is deterministic and already covers provenance, ownership,
malformed input, a golden artifact, and a migration harness. The CPU lane is
directly proven on the checked synthetic corpus. The full roadmap asks for
broader topology, shading, material, image, and animated-asset coverage.

### Work

1. Define a versioned corpus manifest containing:
   - smooth closed cage;
   - sharp feature and material seam;
   - adjacent tetrahedra sharing faces, edges, and vertices;
   - boundary and near-degenerate cases;
   - UV seams and normal discontinuities;
   - at least one dense animated cage and embedded surface;
   - malformed and unsupported topology cases.
2. Give every source asset a license, origin, SHA-256 digest, scale, coordinate
   convention, and expected compiler outcome.
3. Add deterministic golden compiler outputs for the accepted fixtures.
4. Add image-space differential views for position, normal, UV, material,
   ownership, and miss classification.
5. Run the CPU oracle over every animation frame and a fixed adversarial ray
   corpus. Treat any unexplained miss or ownership change as a failure.
6. Add corpus-size and runtime measurements so later GPU comparisons use the
   same work.

### Evidence to add

- `results/compiler/<date>-asset-corpus.json`
- `results/cpu/<date>-oracle-corpus.json`
- Dated differential images under `results/compiler/images/`
- Corpus licensing and hashes in `assets/manifest.json` or an equivalent
  versioned manifest

Use an ISO UTC date for `<date>`, for example `2026-07-28`.

### Completion test

M2 and M3 are complete only when a clean checkout reproduces identical compiler
artifacts and the CPU oracle reports no unexplained topology, shading,
ownership, material, image, or hit/miss differences over the declared corpus.

### Stop condition

If an asset cannot be redistributed or reacquired from its manifest, do not make
it the only fixture for an exit gate. Replace it with an original or permissive
asset, or keep it as an explicitly optional local benchmark.

## WP1 — close Metal correctness and dense-animation evidence

- **Roadmap coverage:** M5
- **Can start now:** Yes
- **Primary blocker:** Correctness, followed by content

### Why it remains open

The M1 Max path builds BLAS/TLAS data, generates descriptors on the GPU, and
executes real Metal ray tracing. The pure hardware path still records 672
classified mismatches over the accepted corpus. The retained CPU-oracle
selection path produces a validated final result for every ray, but its 78.1%
overall fallback frequency and all-ray selection-oracle cost still need WP5
method-selection acceptance.

### Work

1. Convert every mismatch into a minimized fixture containing the cage,
   embedded triangle, ray, expected primitive/ownership, hardware result, and
   all relevant barycentric values.
2. Classify each mismatch as:
   - CPU-oracle defect;
   - floating-point tolerance policy error;
   - generated-triangle overlap or gap;
   - shared-edge ownership disagreement;
   - Metal intersection acceptance rule;
   - stale BLAS/TLAS data or synchronization defect;
   - provenance or attribute reconstruction defect.
3. Add a deterministic ray minimizer and preserve reduced regressions.
4. Fix boundary construction or ownership so adjacent generated triangles have
   one declared watertight rule. Do not hide residuals by broadening tolerances
   without ULP- and scale-based evidence.
5. If a boundary fallback is retained, make it produce the final hit and
   attributes, report when it ran, and compare its total cost with the same
   scenes and rays. Merely excluding a problematic ray is not a fallback.
6. Import the dense asset from WP0 and animate it without regenerating a dense
   surface mesh on the CPU each frame.
7. Measure BLAS build, update/refit, periodic rebuild, TLAS update, tracing,
   attribute recovery, transfers, peak memory, and fallback frequency.
8. Exercise standard and extended Metal limits with successful acceleration
   structure builds. A descriptor-size query alone does not prove the limit.

### Useful commands

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run doctor
XDG_CACHE_HOME="$PWD/.cache" mise run check
TASK_DATE="$(date -u +%F)"
METAL_SWEEP_RAYS=65536 METAL_SWEEP_COPIES="1 4 16 64" \
  ./scripts/metal_scale_sweep.sh \
  results/assets/two-tet-crossing.tetcage \
  "results/metal/${TASK_DATE}-metal-scale-sweep.json"
```

Increase the sweep only after the correctness corpus is clean. Store the
unmodified raw result before producing summaries.

### Evidence to add

- `results/metal/<date>-correctness-corpus.json`
- `results/metal/<date>-dense-animation.json`
- `results/metal/<date>-limit-builds.json`
- Minimized regressions in the repository test corpus

### Completion test

M5 is complete when every declared adversarial and asset ray is either correct
on the hardware path or resolved by a measured, correctness-preserving fallback;
the dense animated asset runs without per-frame CPU dense-mesh generation; and
build/update/rebuild, trace, transfer, memory, and limit results are reproducible
on the identified Apple device.

### Stop condition

If Metal cannot satisfy the boundary rule without a fallback, keep the fallback
only if WP5 shows that its end-to-end correctness and cost are acceptable.
Record this as a method-selection result rather than calling the pure hardware
path watertight.

### 2026-07-28 execution update

The dated WP1 artifacts now exercise all four WP0 accepted assets on the real
Apple M1 Max. The retained fallback returns the CPU-oracle hit and attributes
after the hardware trace instead of excluding the ray. This yields zero final
misses and ownership errors, while preserving 672 hardware mismatch fixtures
and measuring 1,099 fallback invocations across 1,408 corpus rays. The
eight-frame dense run keeps canonical micro-BLAS geometry immutable, records
six TLAS refits and one periodic rebuild, and reports zero CPU dense-mesh
regenerations. Standard and extended modes each successfully build and trace a
65,536-instance TLAS; these are real builds, not descriptor-size queries.

M5 remains open. The fallback rate is 78.1% overall and 87.5% for the dense
animation, which is not acceptable as a pure Metal fast-path claim without
WP5's equal-work cost decision. The current selector validates every hardware
result against the CPU oracle; the manifests compare hardware traversal,
all-ray selection, selected-fallback attribution, merge work, and end-to-end
time over identical rays. Selected fallback cost is labeled as a reused subset
of selection-oracle cost. The deterministic coordinate minimizer replays the
real Metal pipeline: all 672 reduced corpus mismatches preserve their original
classifications and are retained in one deterministic regression artifact.
GPU attribute recovery remains fused into the traversal kernel, so its separate
GPU duration and shading duration are `null`; CPU validation is timed
separately.

## WP2 — acquire and qualify one NVIDIA/Vulkan host

- **Roadmap coverage:** the NVIDIA half of M0; prerequisite for M6–M9 and
  M11–M12
- **Can start now:** Acquisition can start; execution needs another host
- **Primary blocker:** Hardware and environment

### Required host

Prefer a physical or remote workstation with a recent NVIDIA RTX GPU, supported
driver, Vulkan ray-tracing support, enough memory for the dense corpus, and a
stable OS installation. A cloud GPU is acceptable only if device pass-through
exposes the required Vulkan extensions and, for M7, Vulkan and CUDA refer to the
same physical device.

Install the Vulkan loader, tools, validation layers, and headers. Install the
CUDA Toolkit only when running the M7 experiment. Keep the repository's Nix
shell authoritative for its declared build tools; host GPU drivers and kernel
integration remain host responsibilities.

The required Vulkan building blocks and synchronization model are described by
the official [Khronos ray-tracing guide][khronos-rt-guide] and
[acceleration-structure specification][khronos-as-spec].

### Qualification commands

```sh
nvidia-smi
vulkaninfo --summary
nvcc --version
XDG_CACHE_HOME="$PWD/.cache" mise run doctor
XDG_CACHE_HOME="$PWD/.cache" mise run check
```

Run `nvcc --version` only when CUDA is installed. Also execute the repository's
Vulkan capability probe and save its complete JSON output under
`results/capabilities/`.

### Evidence to add

- `results/capabilities/<date>-vulkan-nvidia.json`
- Driver, Vulkan loader/API, SDK, validation-layer, CUDA, PCI/UUID, VRAM, and OS
  versions
- Required extension and feature decisions, including acceleration structures,
  ray query or ray-tracing pipelines, buffer device address, synchronization,
  and instance limits

### Completion test

The NVIDIA half of M0 is complete when a clean checkout builds on the identified
host and the checked-in capability report proves a real NVIDIA device exposes
the features selected by the Vulkan implementation. M7 additionally requires a
recorded Vulkan/CUDA UUID match.

### Stop condition

Reject a host that exposes CUDA but not Vulkan ray tracing, hides the GPU behind
an incompatible virtualization layer, lacks required device features, or cannot
run validation layers. Do not infer support from GPU model marketing alone.

## WP3 — implement and validate the Vulkan fast path

- **Roadmap coverage:** M6
- **Can start now:** No; needs WP2
- **Primary blocker:** Qualified host, then implementation

### Work

1. Pin the Vulkan headers/loader/tooling used by the Nix development shell.
2. Build generated embedded triangles into BLAS objects.
3. Generate per-tetrahedron instance transforms and custom indices in compute
   without a CPU round trip.
4. Build or update the TLAS with explicit synchronization and ownership
   transitions.
5. Trace the same fixed ray corpus used by the CPU and Metal lanes.
6. Recover provenance and attributes using the shared GPU contract.
7. Enable validation layers and treat relevant errors and warnings as failures.
8. Run correctness, dense animation, refit/rebuild, limit, scale, memory, and
   transfer experiments matching WP1.
9. Preserve a capability-negative result when a device lacks a required feature.

The implementation should follow the official
[Vulkan acceleration-structure lifecycle][khronos-as-spec] and
[ray-tracing command rules][khronos-rt-spec], rather than assuming the API owns
build lifetime or synchronization.

### Evidence to add

- `results/vulkan/<date>-correctness-corpus.json`
- `results/vulkan/<date>-dense-animation.json`
- `results/vulkan/<date>-validation.json`
- `results/vulkan/<date>-scale.json`
- Updated capability manifest from WP2

### Completion test

M6 is complete when the NVIDIA path passes the declared correctness corpus with
no unexplained validation findings, runs the representative dense animation
without per-frame CPU dense-mesh generation, and reports reproducible build,
update/rebuild, trace, transfer, limit, scale, and memory measurements.

### Stop condition

If the required extension/limit set is not available, record a capability
failure. If correctness cannot be closed, do not proceed to CUDA interop or use
performance numbers from the incorrect path as a product decision.

## WP4 — make the CUDA/Vulkan interop keep-or-remove decision

- **Roadmap coverage:** M7
- **Can start now:** No; needs a correct M6 path and matched Vulkan/CUDA device
- **Primary blocker:** Dependency and environment

### Work

1. Confirm that Vulkan and CUDA report the same device UUID.
2. Implement only the minimum external-memory and external-semaphore path needed
   to compare equivalent instance-generation work.
3. Compare:
   - Vulkan compute generation;
   - CUDA generation plus external-memory synchronization;
   - total frame time including ownership transfer and synchronization;
   - CPU work, memory, complexity, and failure behavior.
4. Use identical assets, animation frames, rays, warm-ups, repetitions, and
   output validation.
5. Retain CUDA only if it delivers a material, repeatable end-to-end advantage
   that justifies a second toolchain and synchronization boundary.

### Evidence to add

- `results/interop/<date>-cuda-vulkan.json`
- A decision record containing `retain` or `remove_from_production_path`
- Exact device UUID match and driver/toolkit versions

### Completion test

M7 is complete when the decision is based on equivalent end-to-end measurements.
The current removal decision remains the correct production default until such
evidence exists.

### Stop condition

Stop immediately on UUID mismatch, unsupported external handle types, or
non-equivalent correctness. Preserve the simple Vulkan-compute path.

## WP5 — compare hardware, exact 4D, and hybrid robustness methods

- **Roadmap coverage:** M8
- **Can start now:** Portable preparation can start; GPU comparison needs WP1
  and WP3
- **Primary blocker:** Retained GPU paths

### Work

1. Port the CPU adversarial corpus to each retained GPU backend without changing
   expected ownership or tolerances.
2. Implement one complete GPU exact-4D prototype, including hit reconstruction,
   attributes, and reporting—not only a predicate microbenchmark.
3. Define hybrid triggers from measured quantities such as determinant
   conditioning, barycentric margin, boundary proximity, ULP scale, or hardware
   miss followed by a conservative candidate test.
4. Compare pure hardware, exact 4D, and each hybrid policy using identical
   scenes, frames, rays, outputs, and accounting boundaries.
5. Report false misses, false hits, ownership disagreements, maximum errors,
   fallback frequency, latency distribution, total frame time, and memory.
6. Select a method per platform only after correctness is satisfied.

### Evidence to add

- `results/robustness/<date>-gpu-corpus.json`
- `results/robustness/<date>-exact-4d.json`
- `results/robustness/<date>-hybrid-comparison.json`
- Updated method-selection rationale

### Completion test

M8 is complete when at least one GPU exact method and the retained hybrid
candidate have complete correctness and equal-work performance evidence on the
declared corpus, and every platform has an explicit robustness policy.

### Stop condition

Reject any method that improves timing by dropping hard rays, changing the
corpus, excluding transfer/build cost, or returning incomplete attributes.

## WP6 — run the cross-platform scale and crossover study

- **Roadmap coverage:** M9
- **Can start now:** No; needs WP1, WP3, WP5, and the representative corpus
- **Primary blocker:** Dependencies and content

### Work

1. Freeze a common benchmark matrix covering cage size, embedded-triangle count,
   instance count, animation magnitude, rays, hard-ray fraction, and frame count.
2. Run it on the identified Apple and NVIDIA targets with the same asset hashes
   and measurement protocol.
3. Include CPU, Metal, Vulkan, exact 4D, retained hybrid methods, and the
   renderer-native baseline where applicable.
4. Measure cold build, warm update/refit, periodic rebuild, trace, fallback,
   transfers, peak/resident memory, and total frame time.
5. Apply one optimization at a time—compaction, batching, layout, update policy,
   instance culling, or method switching—and preserve an ablation result.
6. Derive crossover thresholds from measured data and feed them into the method
   selector. Keep unsupported regions explicit.

### Evidence to add

- `results/scale/<date>-cross-platform-matrix.json`
- `results/scale/<date>-optimization-ablations.json`
- `results/selection/<date>-measured-policy.json`
- Generated charts and a machine-readable source manifest

### Completion test

M9 is complete when the selector and its fallback decisions are derived from
reproducible measurements on both target platforms and representative animated
content, not from the two-tetrahedron synthetic sweep alone.

### Stop condition

Do not combine results from different assets, ray counts, timing boundaries, or
correctness states into a crossover claim. Report an unmeasured region as
unknown.

## WP7 — validate production cage authoring and LOD transitions

- **Roadmap coverage:** M10
- **Can start now:** Asset acquisition and importer work can start; final
  policy needs M9
- **Primary blocker:** Representative content and renderer-visible validation

### Required corpus

Acquire original or permissively licensed clips representing:

- smooth organic deformation;
- sharp articulation;
- cloth-like folds;
- vegetation or thin structures;
- mostly rigid motion;
- topology-changing or intentionally unsupported content.

Keep a hash, license, source, coordinate convention, frame range, and
redistribution decision for every asset. Large or non-redistributable assets
should have reproducible acquisition instructions and must not be the only
evidence for an exit gate.

### Work

1. Add a documented interchange path for real meshes and animation; do not rely
   only on the procedural clip generator.
2. Fit weights with explicit residual, inversion, and conditioning constraints.
3. Apply conforming refinement and prove shared-face consistency.
4. Generate LOD parent maps and validate stable provenance through transitions.
5. Measure cage quality, embedding residuals, inversions, compiler time, file
   size, and runtime memory.
6. Render silhouette, normal, UV, material, and motion comparisons over whole
   clips.
7. Exercise LOD changes through the retained TLAS path and report popping,
   ownership changes, build/update cost, and peak memory.
8. Document unsupported topology and an artist-visible recovery workflow.

### Evidence to add

- `results/authoring/<date>-production-corpus.json`
- `results/authoring/<date>-visual-differentials.json`
- `results/authoring/<date>-lod-transitions.json`
- Updated authoring guide and asset manifest

### Completion test

M10 is complete when the workflow imports and processes the representative
corpus reproducibly, meets declared geometric and visual tolerances across full
clips, preserves stable LOD provenance, and exposes failures to the author
without silently producing an invalid cage.

### Stop condition

Declare topology-changing or poorly conditioned content unsupported when the
method cannot preserve the contract. A clear rejection and recovery path is
preferable to a visually plausible but untraceable result.

## WP8 — integrate the retained method into Cycles

- **Roadmap coverage:** M12
- **Can start now:** Source setup can start; device integration should follow M9
- **Primary blocker:** Pinned source tree and NVIDIA host for OptiX

### Access and setup

Pin one Blender/Cycles commit and use it for both Metal and NVIDIA work. Follow
the official [Cycles build instructions][cycles-building] to obtain dependencies
and prove a standalone CPU build before modifying device code. The upstream
build exposes CPU, CUDA, OptiX, Metal, and MetalRT test-device choices; preserve
existing device behavior.

```sh
BLENDER_SOURCE_ROOT=/absolute/path/to/pinned/blender \
  XDG_CACHE_HOME="$PWD/.cache" mise run integration-probe
```

### Work

1. Record the exact Blender/Cycles commit, dependency revision, compiler, SDK,
   and device versions.
2. Build and run the upstream CPU tests before adding an adapter.
3. Add a narrow internal representation boundary between tet-cage assets and
   Cycles device backends.
4. Implement and validate the MetalRT path first on the qualified Apple host.
5. Implement and validate the OptiX path on the qualified NVIDIA host using the
   same scene, camera, samples, assets, and expected images.
6. Preserve CPU, CUDA, OptiX, HIP, oneAPI, and Metal behavior that the pinned
   revision already supports; do not globally replace its acceleration path.
7. Compare image differences, render time, first-frame build, animation update,
   memory, and failure/fallback behavior.
8. Produce a minimal upstreamable patch or a documented maintained fork
   boundary.

### Evidence to add

- `results/integrations/<date>-cycles-build.json`
- `results/integrations/<date>-cycles-metalrt.json`
- `results/integrations/<date>-cycles-optix.json`
- Reference and differential images

### Completion test

M12 is complete when the pinned source revision renders the representative
animated corpus through both retained target backends with declared image
correctness, reproducible performance/memory data, and no regression of the
existing device selection path.

### Stop condition

If the change requires an invasive global fork before the standalone device
experiment is correct and useful, stop and reduce the adapter surface. Do not
count a Blender binary add-on or an out-of-process mock as device integration.

## WP9 — integrate the retained method into Unreal Engine 5

- **Roadmap coverage:** M11
- **Can start now:** Access/setup can start; performance integration should
  follow M9
- **Primary blocker:** Pinned UE5 source, Windows/NVIDIA host, and upstream
  evidence

### Access and setup

Link the authorized Epic and GitHub accounts, obtain the private Unreal Engine
source repository, and pin a supported UE5 release in a separate checkout from
the discovered Unreal 4.19.2 tree. Epic's official documentation covers
[source access][ue-source-access], [building from source][ue-build-source], and
[current hardware/software requirements][ue-hardware].

```sh
UNREAL_ROOT=/absolute/path/to/pinned/UnrealEngine-5.x \
  XDG_CACHE_HOME="$PWD/.cache" mise run integration-probe
```

### Work

1. Record the exact UE tag/commit, Windows SDK, Visual Studio toolset, RHI,
   driver, GPU, and project settings.
2. Add an importer/editor diagnostic view for the compiler artifact and
   provenance.
3. Build an isolated RDG experiment with a separate generated BLAS/TLAS path;
   do not initially alter the engine's production scene TLAS.
4. Validate transforms, custom instance indices, hit provenance, material
   mapping, and frame synchronization with the common corpus.
5. Compare the separate path with the native mesh baseline in the Path Tracer
   first, then evaluate Lumen only if the feature contract is compatible.
6. Measure instance count, BLAS/TLAS build/update, render time, memory, editor
   latency, fallback behavior, and packaging.
7. Account explicitly for the current Unreal warning that very high instance
   counts can impose significant scene-update cost in hardware ray tracing.
8. Decide whether to retain a plugin/RDG boundary, maintain an engine patch, or
   stop the integration.

### Evidence to add

- `results/integrations/<date>-ue5-build.json`
- `results/integrations/<date>-ue5-path-tracer.json`
- `results/integrations/<date>-ue5-lumen.json`, only if Lumen is actually tested
- Reference/differential frames and Unreal Insights traces

### Completion test

M11 is complete when a pinned UE5 source build on the target Windows/NVIDIA host
imports the compiler artifact, renders the representative animation correctly,
reports full RDG/RHI timing and memory evidence, and has an explicit
plugin-versus-engine-patch maintenance decision.

### Stop condition

Stop if instance/update costs erase the measured M9 advantage, material
semantics cannot be preserved, or integration requires unsupported engine
internals with no maintainable boundary. Preserve the evidence as a no-go
result.

## WP10 — close the RenderMan feasibility gate

- **Roadmap coverage:** M13
- **Can start now:** Only with a runnable licensed environment
- **Primary blocker:** Licensed runtime and possibly private/vendor API

### Why it remains open

The local 26.2 header/runtime survey found Riley prototype and instance APIs but
did not prove public control of a custom acceleration structure or intersection
program. This is a feasibility gate, not an assumption that the Metal/Vulkan
architecture maps directly onto RenderMan.

### Work

1. Choose and pin one installed RenderMan version; do not mix 26.2 headers with
   27 documentation or runtime results.
2. Confirm the license permits the experiment and that the renderer can execute
   a minimal scene.
3. Build a bounded Riley or procedural prototype using one static prototype and
   per-frame instance transforms.
4. Capture API calls, scene expansion, build/update time, memory, render time,
   image correctness, and failure behavior.
5. Determine whether RIS and XPU expose the same usable primitive/intersection
   contract.
6. Ask Pixar the following questions if the public SDK does not answer them:
   - Can a plugin provide a custom user-primitive intersection implementation in
     RIS and XPU?
   - Can a plugin retain an immutable prototype/BLAS while updating only
     per-instance transforms?
   - What supported instance limits and update costs apply?
   - Is a private SDK, source license, or partnership required?
   - Which macOS GPU or XPU configurations support the required path?
7. Record one of: `go_public_sdk`, `go_vendor_partnership`,
   `instances_only_baseline`, or `no_go`.

```sh
RENDERMAN_ROOT=/absolute/path/to/RenderManProServer \
  XDG_CACHE_HOME="$PWD/.cache" mise run renderman-probe
```

### Evidence to add

- `results/renderman/<date>-licensed-prototype.json`
- Renderer logs and reference/differential images
- Vendor response or dated unanswered-question record when it changes the gate
- Updated `docs/renderman-feasibility.md`

### Completion test

M13 is complete when a licensed runtime experiment and the available SDK/vendor
answer establish a reproducible go/no-go decision. A no-go is a valid completion
if the unsupported boundary and retained alternative are explicit.

### Stop condition

Do not reverse-engineer a private interface or claim a custom fast path from
prototype/instance APIs alone. If the needed hook is private, stop at the vendor
partnership decision.

## WP11 — harden and release the retained product surface

- **Roadmap coverage:** M14
- **Can start now:** Some inventory work can start; final closure needs all
  retained paths
- **Primary blocker:** Retained-backend decision and release obligations

### Work that can start early

1. Maintain a third-party code, dependency, model, paper, and asset inventory.
2. Put redistributable sample licenses and notices beside their manifests.
3. Prepare a factual freedom-to-operate packet for qualified counsel; do not
   represent the repository's technical survey as a legal opinion.
4. Keep security, fuzzing, malformed-input, deterministic-build, cache, and
   report checks running on the portable lane.
5. Document how to acquire non-redistributable SDKs and source trees without
   copying them into the repository.

### Work after method and integration selection

1. Delete or clearly quarantine experimental backends that were not retained.
2. Add cross-device conformance vectors for every retained CPU/GPU shader and
   renderer adapter.
3. Add hardware CI for the supported Apple and NVIDIA configurations, or a
   documented, signed manual release qualification when secure runners are not
   available.
4. Add an actual historical artifact migration only when a v2 format exists;
   test v1-to-v2 conversion and preserve the v1 golden.
5. Ship small redistributable sample assets covering the supported authoring
   cases and at least one intentional rejection.
6. Write installation, authoring, integration, troubleshooting, limitations,
   and result-reproduction guides.
7. Produce a release report from machine-readable results with correctness,
   performance, memory, platform, and method-selection evidence.
8. Test a clean checkout on every supported host without undeclared local
   dependencies.

### Evidence to add

- `THIRD_PARTY.md` and a repository license/notice location
- `assets/manifest.json` with complete licensing and hashes
- `results/conformance/<date>-cross-device.json`
- `results/releases/<version>-qualification.json`
- Hardware CI logs or signed manual qualification records
- Versioned format migration and golden artifacts

### Completion test

M14 is complete when a clean checkout builds and runs on every supported host,
all retained backends and integrations pass the common conformance corpus,
release assets and notices are redistributable, documentation reproduces the
results, and the final report contains no unsupported claim.

### Stop condition

Do not declare production readiness while a retained backend, renderer adapter,
legal/license obligation, supported-host qualification, sample, or
documentation gate remains open.

## External inputs to arrange

- [ ] Access to a qualified NVIDIA RTX Vulkan host
- [ ] Vulkan tools, validation layers, and driver on that host
- [ ] CUDA Toolkit on the same host only for WP4
- [ ] Original or permissively licensed representative animated assets
- [ ] Pinned Blender/Cycles source checkout
- [ ] NVIDIA developer access and compatible OptiX SDK/driver for Cycles OptiX
- [ ] Linked Epic/GitHub access to a pinned UE5 source release
- [ ] Windows/NVIDIA UE5 build host and supported Visual Studio/Windows SDK
- [ ] Runnable RenderMan license for the pinned version
- [ ] Pixar support/vendor contact if the public API gate remains unanswered
- [ ] Qualified counsel review before public production claims where desired

## Suggested next task

Start with WP0. It improves the truth corpus required by every later backend,
scale study, and renderer integration and does not require new hardware or
private source access.

Copy-ready task:

> Implement WP0 from `docs/open-roadmap-workplan.md`. Expand the versioned asset
> and adversarial ray corpus, add deterministic compiler and CPU-oracle evidence,
> update the requirements matrix and goal audit only where the new evidence
> supports it, run all repository checks, and commit the result. Do not begin
> WP1.

After WP0, proceed to WP1 on the current Apple host while arranging the external
inputs for WP2, WP8, WP9, and WP10.

[cycles-building]: https://github.com/blender/cycles/blob/main/BUILDING.md
[khronos-as-spec]: https://docs.vulkan.org/spec/latest/chapters/accelstructures.html
[khronos-rt-guide]: https://docs.vulkan.org/guide/latest/extensions/ray_tracing.html
[khronos-rt-spec]: https://docs.vulkan.org/spec/latest/chapters/raytracing.html
[ue-build-source]: https://dev.epicgames.com/documentation/en-us/unreal-engine/building-unreal-engine-from-source
[ue-hardware]: https://dev.epicgames.com/documentation/en-us/unreal-engine/hardware-and-software-specifications-for-unreal-engine
[ue-source-access]: https://dev.epicgames.com/documentation/en-us/unreal-engine/downloading-source-code-in-unreal-engine
