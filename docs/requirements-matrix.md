# Requirements and proof matrix

Status values are deliberately proof-oriented:

- **implemented** means source and portable tests exist;
- **measured** means a checked machine-readable result exists from the named
  runtime;
- **prepared** means a contract or adapter exists, but its runtime exit gate is
  not proven;
- **blocked** means the required hardware, source tree, SDK, license, or vendor
  access is absent.

Compilation alone never promotes a row to measured.

| Requirement | Planned implementation | Required proof | Current status |
| --- | --- | --- | --- |
| Canonical tet barycentrics and affine deformation | `tetcage_core` math kernel | analytical, all-permutation, seeded differential tests | implemented |
| Full inverse-transpose normals | `transform_normal` | nonuniform scale/shear orthogonality tests and renderer differential | implemented on CPU; renderer proof pending |
| Determinant, mirror, inversion, and degeneration | tet diagnostics plus backend fallback policy | adversarial pose corpus with no NaN/singular API submission | implemented on CPU; GPU proof pending |
| Robust clipping | deterministic half-space clipping with explicit tolerance policy | containment, area, feature, and scale tests | implemented for portable baseline; broader corpus pending |
| Shared-boundary identity and ownership | stable cage/source feature IDs and lowest-owner rule | two-tet fixtures plus watertight boundary corpus | implemented for coplanar ownership; full corpus pending |
| Portable canonical asset | versioned little-endian `.tetcage` stream | byte-identical repeated builds, migration/golden and malformed-input tests | format v1 implemented; golden checksum and malformed/safety tests measured; migration beyond initial v1 pending |
| Hit provenance | source primitive/material plus source barycentrics | position, UV, normal, material and custom-attribute differential | position/material implemented; full renderer attributes pending |
| Fast-path crack policy | versioned expanded clipping tolerance, later scale/ULP/conditioning policy | Metal/Vulkan boundary ray corpus with miss/duplicate rates | Metal boundary-policy experiment measured: 7/128 rays classified boundary-sensitive and routed through explicit experimental CPU fallback (121 GPU-eligible); default hardware-all-rays still fails |
| Exact 4D oracle | globally ordered barycentrics, feature snapping, 4D BVH/manual traversal | zero unexplained misses/duplicates on adversarial corpus | implemented and seeded corpus-tested |
| Tight bounded-simplex projection | four-variable greedy min/max solver | exhaustive vertex enumeration plus randomized conservativeness test | implemented and property-tested |
| Runtime contracts | backend-neutral pose/transform/build/hit/cache interfaces | identical scene through CPU oracle and corruptible stub | implemented and benchmark-tested; explicit per-frame instance-limit rejection added |
| Result manifests | JSON Schema v1 and benchmark writer | schema-valid manifests with null unavailable counters | implemented and machine-checked, including checked-in manifest smoke validation |
| Acceleration-structure nesting | flattened two-level production contract | real API/engine queries; no inferred third level | Metal exposes no numeric property; UE/Vulkan blocked |
| Instance scaling | per-device query/probe and scale sweep | actual build/update/trace at increasing counts with full memory/time | Metal synthetic 2-tet sweep measured through 128 instances; correctness residuals remain, so no crossover claim |
| Metal device capability | Objective-C++ device probe | real `MTLDevice` query output | implemented; requires unsandboxed execution |
| Vulkan/NVIDIA capability | Vulkan physical-device probe | KHR/NV features, limits, driver/device from real NVIDIA loader | implementation present; host has no Vulkan SDK/device |
| Metal fast backend | immutable micro-BLAS, GPU transforms/descriptors, TLAS, tracing | M5 correctness and scale sweep | measured partial: direct BLAS/TLAS/tracing works; opt-in GPU descriptor generation is direct-device validated; hardware-all-rays has 1 miss and 0.05 position error, while explicit boundary fallback makes 121/128 rays GPU-eligible with zero residuals |
| Vulkan fast backend | KHR AS/ray tracing and Vulkan compute | validation-clean NVIDIA results | blocked by absent SDK/NVIDIA hardware |
| CUDA interop | UUID-matched memory/semaphore experiment | end-to-end retain/remove measurement | blocked until M6 exists |
| GPU watertight/hybrid experiments | procedural/manual 4D path | equal-work correctness/time/memory comparison | blocked until M5/M6 and M3 exist |
| Crossover/optimization policy | common benchmark matrix | measured Metal and NVIDIA crossover surfaces | blocked until both GPU backends |
| Cage authoring and LOD | baseline cage generator, conditioning/residual analyzer, suitability policy | representative clip-wide quality evidence | portable baseline/analyzer, deterministic procedural clip residuals, constrained weight fitting, conforming 8-way refinement, and parent-mapped LOD levels implemented; production clips and visual/TLAS transition evidence pending |
| Unreal Engine | importer, RDG proof, material bridge, source changes if needed | pinned Windows/NVIDIA sample and million-instance report | blocked: no UE source/install |
| Cycles | canonical loader, MetalRT and OptiX device integration | pinned source build and same-scene device comparison | blocked: Blender binary exists, Cycles source checkout absent |
| RenderMan | public-API procedural and capability matrix | licensed pinned-runtime prototype plus go/no-go | RenderMan 26.2 headers/runtime present; M13 study pending |
| Production release | fuzzing, CI, migration, samples, reproducibility report | every supported claim linked to exact manifest | portable CI plus parser/fuzz/golden foundation implemented; migration, hardware conformance, samples, and retained-backend/integration gates remain |

## Overall roadmap success criteria

| Criterion | Evidence needed |
| --- | --- |
| Rest-pose topology, position, normal, UV/material continuity | deterministic compiler fixtures plus dense-vs-compiled image/hit differentials |
| Cage deformation equivalence | CPU conformance vectors and the same vectors executed in each retained shader/device |
| Closed adjacency in fast mode | boundary-targeted ray corpus on each supported backend |
| Exact mode watertightness | independent 4D traversal with adversarial corpus, not image inspection |
| Eliminate dense per-frame deformation | captures and manifests showing only cage/instance buffers change |
| Honest performance crossover | equal scenes/rays/settings and all timing/memory categories on each platform |
| Stable renderer attribution | native material/attribute tests inside each claimed renderer |
| Mixed-scene coexistence | static, rigid, skinned, procedural, and tet geometry in one renderer scene |
| No hidden third AS level | code/API evidence for the actual flattened or nested hierarchy |
| Reproducible release | clean-checkout commands, pinned dependencies, manifests, chart regeneration, and explicit unsupported paths |
