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
| Full inverse-transpose normals | `transform_normal` | nonuniform scale/shear orthogonality tests and renderer differential | CPU and Blender native/fallback nonuniform-scale/rotation differential pass; broader shear corpus pending |
| Determinant, mirror, inversion, and degeneration | tet diagnostics plus backend fallback policy | adversarial pose corpus with no NaN/singular API submission | CPU policy plus native/fallback nonuniform, sheared, and negative-scale renderer differentials pass; broader degenerate GPU corpus pending |
| Robust clipping | deterministic half-space clipping with explicit tolerance policy | containment, area, feature, and scale tests | implemented for portable baseline; broader corpus pending |
| Shared-boundary identity and ownership | stable cage/source feature IDs and lowest-owner rule | two-tet fixtures plus watertight boundary corpus | implemented for coplanar ownership; full corpus pending |
| Portable canonical asset | versioned little-endian `.tetcage` stream | byte-identical repeated builds, migration/golden and malformed-input tests | format v1 implemented; golden checksum, explicit v1 identity migration, and malformed/safety tests measured; historical-version migration remains pending |
| Hit provenance | source primitive/material plus source barycentrics | position, UV, normal, material and custom-attribute differential | position, normal, UV, a UV->Checker Texture->Emission node graph, shader-visible source-normal corner attribute, source material-slot IDs, and integer source-primitive/owner/material face attributes implemented and probed, including owner-tet parity across a two-tet fixture; broader texture/custom-node graphs remain |
| Fast-path crack policy | versioned expanded clipping tolerance, later scale/ULP/conditioning policy | Metal/Vulkan boundary ray corpus with miss/duplicate rates | closed for the declared Metal gate at clean commit `32af558`: procedural AABB plus compensated projected-edge acceptance passes all 1,408 audited rays with zero mismatches, misses, wrong owners, or CPU final hits; paired GPU-only runs invoke no oracle; Vulkan coverage remains |
| Exact 4D oracle | globally ordered barycentrics, feature snapping, 4D BVH/manual traversal | zero unexplained misses/duplicates on adversarial corpus | implemented and seeded corpus-tested |
| Tight bounded-simplex projection | four-variable greedy min/max solver | exhaustive vertex enumeration plus randomized conservativeness test | implemented and property-tested |
| Runtime contracts | backend-neutral pose/transform/build/hit/cache interfaces | identical scene through CPU oracle and corruptible stub | implemented and benchmark-tested; explicit per-frame instance-limit rejection added |
| Result manifests | JSON Schema v1 and benchmark writer | schema-valid manifests with null unavailable counters | implemented and machine-checked, including checked-in manifest smoke validation |
| Acceleration-structure nesting | flattened two-level production contract | real API/engine queries; no inferred third level | Metal exposes no numeric property; UE/Vulkan blocked |
| Instance scaling | per-device query/probe and scale sweep | actual build/update/trace at increasing counts with full memory/time | direct Metal standard and extended modes each complete a 65,536-instance TLAS build and trace; this proves those tested points, not the device maximum or a crossover |
| Metal device capability | Objective-C++ device probe | real `MTLDevice` query output | implemented; requires unsandboxed execution |
| Vulkan/NVIDIA capability | Vulkan physical-device probe | KHR/NV features, limits, driver/device from real NVIDIA loader | implementation present; host has no Vulkan SDK/device |
| Metal fast backend | immutable micro-BLAS, GPU transforms/descriptors, TLAS, tracing | M5 correctness and scale sweep | M5 closed for clean commit `32af558`: 1,408/1,408 audited Metal final hits with zero CPU fallback, 8-frame dense refit/rebuild, standard/extended 65,536-instance builds, and five repeated 1/4/16/64-copy GPU-only measurements; broader production content remains |
| Vulkan fast backend | KHR AS/ray tracing and Vulkan compute | validation-clean NVIDIA results | blocked by absent SDK/NVIDIA hardware |
| CUDA interop | UUID-matched memory/semaphore experiment | end-to-end retain/remove measurement | blocked until M6 exists |
| GPU watertight/hybrid experiments | procedural/manual 4D path | equal-work correctness/time/memory comparison | Metal procedural projected-edge path implemented and correct on the declared M1 Max corpus; repeated equal-work scale comparison and Vulkan counterpart remain |
| Crossover/optimization policy | common benchmark matrix | measured Metal and NVIDIA crossover surfaces | blocked until both GPU backends |
| Cage authoring and LOD | baseline cage generator, conditioning/residual analyzer, suitability policy | representative clip-wide quality evidence | portable baseline/analyzer, deterministic procedural clip residuals, constrained weight fitting, conforming 8-way refinement, and parent-mapped LOD levels implemented; production clips and visual/TLAS transition evidence pending |
| Unreal Engine | importer, RDG proof, material bridge, source changes if needed | pinned Windows/NVIDIA sample and million-instance report | blocked: no UE source/install |
| Cycles | canonical loader, MetalRT and OptiX device integration | pinned source build and same-scene device comparison | native Cycles branch `643723d1ee1e` builds a tet-cage AABB/query path with object visibility filtering, mixed triangle/AABB candidates, projected-axis tet intersection, projected-axis local multi-hit, motion-time local-hit normals, and motion-aware native local scans, native AABB volume-table hits, compiled self-filtered opaque-shadow seams, the motion payload handoff, cancellation/error propagation, and the local self-hit filter; Blender branch `b5c8ff2fceeb` builds the full app, recognizes `cycles_tetcage_v1`, and passes opt-in tet-only, mixed ordinary-triangle/native-AABB, object-transform-motion, static transparent, pure-transparent motion, two-object shape-key deformation and local SSS, AO-local, UV, source-normal, two-material, one native shape-key deformation, volume-table, and local-table differentials; mixed transparent-closure motion remains an explicit ordinary-Cycles fallback; area-light precision, broader deformation topology, exact volume/closure semantics, Principled shading semantics, M9, and NVIDIA/OptiX remain gates |
| RenderMan | public-API procedural and capability matrix | licensed pinned-runtime prototype plus go/no-go | RenderMan 26.2 headers/runtime present; M13 study pending |
| Production release | fuzzing, CI, migration, samples, reproducibility report | every supported claim linked to exact manifest | portable CI, parser/fuzz/golden foundation, explicit v1 migration harness, deterministic result-summary generation, measured-input representation fallback policy, and neutral runtime safety outcomes implemented; historical migration, hardware conformance, samples, and retained-backend/integration gates remain |

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
