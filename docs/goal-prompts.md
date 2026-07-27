# Reimplementation Goal Prompts

## How to use these prompts

Each prompt is intended to be copied into a fresh Codex task rooted at this repository. Run them in milestone order except that Metal and Vulkan may proceed in parallel after M4, and renderer integrations may proceed in parallel after M9.

Every task must:

- read `README.md`, `docs/paper-analysis.md`, and `docs/reimplementation-roadmap.md` first;
- inspect existing work and preserve compatible decisions;
- implement code, tests, documentation, and reproducible evidence required by the milestone;
- distinguish directly verified results from inference;
- leave machine-readable results under `results/`;
- avoid claiming completion from compilation alone or from only a tiny synthetic scene;
- update the roadmap or a dated status artifact when evidence changes a premise;
- stop and document a real platform limitation rather than simulating success.

The prompts deliberately repeat critical exit gates so each can stand alone.

## Goal 0 - Contract, repository scaffold, and capability probes

```text
Work in the ray-tracing-tet-cage repository. Read README.md,
docs/paper-analysis.md, and docs/reimplementation-roadmap.md completely before
editing anything.

Implement Milestone M0: establish the contract, repository scaffold, build
system, test entry points, result schema, and real Metal/Vulkan capability
probes.

Required work:
1. Create a portable C++20/CMake project matching the roadmap's separation
   between core, compiler, CPU reference, Metal, Vulkan, and optional CUDA.
2. Add a small test executable and repo-native commands for configure, build,
   test, and formatting/lint checks. Prefer a declarative local environment if
   the repository already establishes one.
3. Write a requirements matrix mapping every core paper mechanism and every
   roadmap success criterion to a planned implementation and proof artifact.
4. Implement a macOS capability report that queries the actual Metal device for
   ray tracing, acceleration-structure limits/features, indirect instances,
   extended limits, and any relevant Metal version availability. Do not infer
   support from OS version alone.
5. Implement a Vulkan capability report that enumerates physical devices and
   records required KHR extensions, acceleration-structure properties including
   maxInstanceCount, buffer-device-address support, ray pipeline/ray query
   support, and NVIDIA PTLAS/cluster extensions when present.
6. Define a versioned JSON result manifest containing commit, dirty state,
   compiler, OS, API/driver/device, scene hash, configuration, timing categories,
   memory categories, and correctness counters.
7. Document supported, conditionally supported, and unavailable configurations,
   including what hardware is still needed for validation.

Acceptance evidence:
- A clean checkout configures, builds, and runs tests on the available host.
- Capability probes produce machine-readable output from real APIs where the
  APIs exist; a missing NVIDIA environment is reported as unverified, not
  mocked.
- The requirements matrix explicitly calls out instance scaling, AS nesting,
  hit provenance, degeneracy, fast-path cracks, and the 4D oracle.
- No production design assumes a third AS hierarchy level or a legal instance
  count without a recorded query.

Non-goals:
- Do not build the asset compiler or renderer yet.
- Do not download large SDKs or spend cloud resources without authorization.
- Do not treat a header-level compile check as a device capability result.
```

## Goal 1 - CPU tetrahedral mathematics and robust geometry

```text
Implement Milestone M1 in the ray-tracing-tet-cage repository. First read the
repository contract and the paper analysis. Treat the equations and robustness
requirements there as the specification.

Build the backend-independent CPU mathematics and geometry kernel:
- tetrahedral barycentric conversion in both directions;
- canonical-to-object/world affine transforms;
- the exact equivalence between per-vertex cage interpolation and the affine
  instance transform;
- inverse-transpose normal handling;
- signed determinant, condition estimate, mirror/inversion detection, and
  explicit near-singular policies;
- robust plane classification, polygon clipping, and triangle-to-tetrahedron
  clipping primitives;
- deterministic vertex/face identities independent of floating-point traversal
  order.

Use tests first for the highest-risk behavior. Add analytical fixtures and
random/property-based tests covering:
- all tetrahedron vertex permutations and orientations;
- identity, translation, rotation, scale, shear, reflection, and near-collapse;
- shared faces, edges, and vertices;
- scale ranges representative of centimeters through large worlds;
- comparison of matrix deformation against direct barycentric deformation.

Acceptance evidence:
- Tests state numeric tolerances and why they are appropriate.
- Randomized differential tests pass with reproducible seeds and emit a compact
  failure corpus when they do not.
- Degenerate inputs never silently yield NaNs or unbounded transforms.
- A short derivation in the docs ties the implemented storage convention to the
  equations in paper-analysis.md.

Do not implement a GPU backend or claim watertight rendering in this goal.
Commit fixtures small enough for CI and keep large generated corpora reproducible
from seeds.
```

## Goal 2 - Deterministic cage micro-mesh asset compiler

```text
Implement Milestone M2. Read the paper reconstruction and the canonical asset
format in the roadmap before changing code.

Create a deterministic offline compiler that accepts a triangle mesh plus a
tetrahedral cage and emits the versioned portable tet-cage asset:
1. Validate cage orientation, adjacency, coverage, conditioning, and source
   coordinate conventions.
2. Clip every source triangle against the four half-spaces of intersected
   tetrahedra.
3. Deduplicate introduced boundary vertices using canonical cage-feature and
   source-edge identities, not approximate position alone.
4. Express generated vertices in canonical barycentric coordinates.
5. Preserve source triangle, material, source barycentrics, and all data needed
   to reconstruct original renderer attributes at a hit.
6. Define deterministic ownership for fragments on shared cage boundaries.
7. Implement the fast variant's expanded clipping region as a versioned,
   inspectable tolerance policy.
8. Serialize, deserialize, checksum, and inspect the asset without embedding
   backend-specific acceleration structures.
9. Emit statistics for triangle/vertex expansion, occupied tets, boundary
   fragments, conditioning, and memory by stream.

Create fixtures including one tet, two tets sharing a face, a regular cube
subdivision, a dense smooth mesh, triangles exactly on features, and invalid
cages.

Acceptance evidence:
- Repeated builds are byte-identical for identical inputs and configuration.
- Rest-pose reconstruction passes topology, position, normal, UV/material, and
  image comparisons.
- Every emitted fragment has valid source provenance and a deterministic owner.
- Expansion and memory reports expose counterexamples instead of hiding them.
- Malformed or uncovered source geometry produces actionable diagnostics.

Do not build Metal/Vulkan AS objects or optimize by dropping provenance.
```

## Goal 3 - CPU rendering oracle and 4D watertight reference

```text
Implement Milestone M3 using the compiled assets from M2.

Build two independent CPU truth paths:
1. A straightforward renderer/intersector over the transformed 3D micro-meshes.
2. The paper's watertight representation in four barycentric dimensions,
   including globally ordered cage vertex IDs, exact feature snapping, 4D node
   bounds, projection to conservative world-space bounds, and manual traversal.

Add a deterministic ray-corpus generator that targets random interiors plus
shared faces, edges, vertices, grazing angles, large/small scales, mirrored
poses, and near-degenerate poses. Count misses, duplicate hits, wrong primitive
ownership, position error, and attribute error.

Implement and evaluate a tighter 4D-to-3D bound:
  min/max sum_i b_i*p_i
  subject to l_i <= b_i <= u_i and sum_i b_i = 1.
For four dimensions, solve this bounded-simplex linear program by assigning
remaining mass in sorted coefficient order. Prove or mechanically verify that
the result is conservative before measuring traversal.

Acceptance evidence:
- The 4D reference passes the declared watertight corpus with zero unexplained
  misses or duplicate surface ownership.
- The ordinary transformed path's failures are quantified over the same corpus.
- The tighter bound is compared against the paper's min-sum bound for node
  volume, visited nodes, correctness, build time, and trace time.
- All results include seeds, asset hashes, and exact commands.

Keep this as a correctness oracle even if it is too slow for production. Do not
weaken the ray corpus to obtain a clean result.
```

## Goal 4 - Runtime contract, baselines, and benchmark harness

```text
Implement Milestone M4. Do not start a full GPU backend until this harness can
judge one.

Create:
- backend-neutral asset, pose, transform, build-policy, and hit contracts;
- an asset cache and lifecycle model that can own immutable per-tet micro-BLAS
  handles without serializing opaque API objects;
- a conventional dense CPU deformation baseline and interfaces for GPU
  update/rebuild baselines;
- reproducible procedural scenes spanning geometry, cage density, copy count,
  deformation, visibility, and ray workload;
- result manifests and CSV/JSON reports for stage timing, memory, correctness,
  and failures;
- image, hit, and attribute differential tooling against the CPU oracles;
- benchmark commands that warm up, repeat, and report median/p95/variance.

Define timing categories for cage deformation, transform and instance
generation, BLAS work, TLAS work, synchronization, traversal, shading, and total.
Define memory categories for source/canonical geometry, provenance, cage data,
BLAS/TLAS, scratch, instance buffers, API object overhead, and renderer-specific
state.

Acceptance evidence:
- One command runs a scene definition through the CPU oracle and a stub backend,
  writes a complete manifest, and detects an intentionally corrupted result.
- Dense update/rebuild and tet-instancing results will be directly comparable.
- Missing counters or unsupported queries are marked unavailable, never zero.
- The harness can scale scenes without checking huge generated assets into git.

Do not publish performance conclusions from the stub or CPU reference.
```

## Goal 5 - Metal ray-tracing fast path

```text
Implement Milestone M5 on real Metal-capable macOS hardware.

Build the fast transformed-micro-mesh path:
1. Load canonical assets and create one immutable triangle BLAS per occupied tet,
   or a measured grouping if object overhead makes that superior.
2. Use appropriate static build flags and compact BLAS objects where measured.
3. Deform cages and generate affine transforms, inverse-transpose data, instance
   metadata, visibility, and indirect instance descriptors on the GPU.
4. Build or update the visible TLAS with explicit synchronization and pooled
   scratch storage.
5. Trace hardware triangles and reconstruct original primitive, material, UV,
   normal, and custom attribute data.
6. Handle determinant sign, culling, near-singular fallback, standard instance
   limits, extended limits, and unsupported feature sets explicitly.
7. Compare rebuild, refit, and periodic-rebuild strategies and record traversal
   degradation over an animation.
8. Add GPU captures/counter instructions and integrate timestamps into the
   shared result schema.

Run the full correctness suite plus a scale sweep over triangles, cage tets,
copies, total instances, ray counts, and motion amplitude. Compare against
conventional dense deformation plus Metal BLAS update/rebuild.

Acceptance evidence:
- GPU hits and attributes agree with the CPU fast oracle within declared
  tolerances.
- The rendered dense asset animates without per-frame dense vertex updates.
- Real-device manifests record feature/limit queries, all stage timings, memory,
  and failures.
- The report identifies a measured crossover and the first instance/memory/time
  bottleneck; it does not extrapolate a tiny test as proof of massive scale.
- Standard versus extended-limit behavior is exercised where hardware permits.

Do not claim engine or Cycles integration. Do not assume Metal 4-only features
exist on all supported Apple GPUs.
```

## Goal 6 - Vulkan ray-tracing fast path on NVIDIA

```text
Implement Milestone M6 using Vulkan compute and KHR ray tracing on a real NVIDIA
GPU. Read the shared runtime and benchmark contract first.

Required work:
1. Query and record all required Vulkan features, extensions, AS limits, device
   addresses, scratch alignment, and update capabilities.
2. Build and compact immutable triangle BLAS objects for occupied micro-meshes.
3. Deform cages and generate VkAccelerationStructureInstanceKHR data in Vulkan
   compute with no host readback on the frame path.
4. Build/update the TLAS with validation-clean synchronization between compute,
   AS build, and ray traversal.
5. Implement a ray pipeline or ray-query renderer with full source hit
   provenance and attribute reconstruction.
6. Pool scratch and transient buffers and make lifetime/device-address stability
   explicit.
7. Compare TLAS rebuild, update, and periodic rebuild over the same scale and
   deformation matrix used by Metal.
8. Compare conventional dense deformation plus BLAS update/rebuild.

Acceptance evidence:
- Vulkan validation layers are clean on correctness and representative scale
  scenes.
- GPU results match CPU oracles within declared tolerances.
- Result manifests contain real NVIDIA driver/device/API information, stage
  timings, memory, and max-instance/extension data.
- A measured crossover and bottleneck analysis exists.
- CPU submission, synchronization, and allocation costs are included.

Do not add CUDA in this milestone. A single-API Vulkan baseline is required so
the value and cost of later interop can be measured honestly.
```

## Goal 7 - CUDA/Vulkan interop retain-or-remove experiment

```text
Implement Milestone M7 only after the Vulkan compute path is correct and
profiled.

Start by writing a falsifiable hypothesis naming the exact stage CUDA may
improve and the expected reason. Then:
- export/import the necessary Vulkan memory using matched physical-device UUIDs;
- synchronize Vulkan and CUDA with external binary or timeline semaphores;
- implement functionally equivalent CUDA and Vulkan compute versions;
- keep the TLAS build and ray traversal in Vulkan;
- measure kernel, synchronization, ownership transition, CPU submission, and
  total frame time;
- run correctness differentials and repeated scale sweeps.

Acceptance evidence:
- No host copy or implicit device-wide synchronization hides in the interop path.
- CUDA and Vulkan outputs are byte- or tolerance-equivalent.
- The report includes end-to-end results, not kernel time alone.
- Make an explicit retain/remove decision. Retain CUDA only for a material
  measured benefit or a documented ecosystem requirement. If it loses or ties,
  remove it from the production critical path while preserving the experiment
  and negative result.

Do not redefine success as merely importing a buffer.
```

## Goal 8 - GPU robustness, watertight traversal, and hybrid experiments

```text
Implement Milestone M8 after both fast GPU backends are correct.

Harden the production fast path:
- replace a single global epsilon with a scale-, ULP-, edge-length-, and
  conditioning-aware policy;
- implement deterministic shared-boundary ownership;
- test mirrors, culling, inversions, near singularities, long ray distances, and
  large/small scene scales;
- run the CPU boundary ray corpus on Metal and Vulkan and report residual miss,
  duplicate, and provenance error rates.

Implement a GPU 4D reference traversal on at least one backend using procedural
bounding boxes/custom intersection or an equivalent manual traversal. Port the
tighter bounded-simplex projection if M3 retained it.

Evaluate, independently:
- hardware traversal for interior fragments plus special boundary handling;
- conservative boundary shells;
- replay of ambiguous boundary hits;
- coarse hardware AS feeding custom 4D traversal;
- higher precision only in transform/boundary setup.

Acceptance evidence:
- The fast path has a declared domain and quantified residual failure rate.
- The exact path passes the adversarial watertight corpus.
- Every hybrid is compared for correctness, traversal, build, total time, and
  memory at equal rays and scenes.
- Reject experiments that improve a microbenchmark but harm frame time or
  introduce biased visibility.

Do not make the exact path the default solely because it is correct; retain the
paper's performance reality in the decision.
```

## Goal 9 - Scale reproduction and optimization study

```text
Implement Milestone M9 as a full evidence-backed optimization study.

Run a common benchmark matrix on Metal and NVIDIA comparing:
- conventional dense deformation with BLAS update and rebuild;
- fast tet instancing;
- the 4D reference on feasible subsets;
- NVIDIA partitioned TLAS where supported and where partial updates exist;
- NVIDIA animated cluster acceleration structures where supported;
- rigid instancing and static mesh negative controls.

Implement and measure:
- GPU descriptor generation and compaction;
- object/cage/tet visibility culling;
- cage LOD;
- staggered updates;
- identical-pose sharing;
- scratch/AS allocation pools and batching;
- BLAS compaction and possible tet grouping;
- update/refit with periodic rebuild;
- canonical/provenance quantization with an FP32 baseline;
- asynchronous or one-frame-latency operation as an explicit quality mode.

Produce crossover surfaces over triangle count, tet count, copies/instances,
motion, ray workload, and visibility. Include median/p95/variance, peak memory,
API object count, CPU cost, and all GPU stages. Compare independently against
the paper's reported qualitative and quantitative outcomes without forcing a
match.

Acceptance evidence:
- A reproducible report says which claims were reproduced, contradicted, or
  remain untested.
- At least one meaningful aggregate scene demonstrates the largest validated
  scale with full timing and memory accounting.
- A per-asset selection policy chooses rigid, conventional dynamic, tet-cage, or
  NVIDIA cluster representation from measured inputs.
- Rejected optimizations and negative results are retained.

Do not call generated copies "production proof" without at least one
production-style animated asset and ray workload.
```

## Goal 10 - Cage generation, animation quality, and LOD authoring

```text
Implement Milestone M10. The objective is not just to make a cage; it is to
decide automatically when a cage represents production animation well enough.

Build an import/generation and analysis workflow that:
- accepts artist-authored tetrahedral cages and can generate a baseline cage;
- visualizes occupancy, conditioning, inversions, curvature, boundary fragment
  density, and per-tet animation residual;
- measures source dense deformation versus cage-driven reconstruction over full
  representative clips;
- optimizes cage vertex skin weights over sampled poses, following the research
  direction identified in the paper;
- refines cages conformingly near curvature, silhouettes, material/displacement
  boundaries, poor residuals, and ill-conditioned regions;
- creates cage LODs with stable provenance and transition policy;
- recommends rigid, ordinary skinned, tet-cage, or hybrid representation per
  asset/region.

Measure position, normal, silhouette, shading, temporal, memory, and performance
error versus cage density. Include sharp articulation, loose cloth-like motion,
smooth creatures, vegetation, rigid parts, and topology-changing negative
controls.

Acceptance evidence:
- Representative clips meet explicit quality thresholds, not visual judgment on
  one pose.
- The tool detects unsuitable assets and emits actionable fallback reasons.
- Cage refinement remains conforming and preserves correct compiler ownership.
- LOD transitions are tested for cracks, popping, temporal effects, and TLAS
  cost.
- Authoring documentation lets another person reproduce the workflow.
```

## Goal 11 - Unreal Engine 5 integration

```text
Implement Milestone M11 against a pinned Unreal Engine version. First verify the
actual RHI and renderer extension points in that version; do not design from
API-name memory.

Stage the work:
1. Create an asset importer/editor module for the canonical tet-cage format,
   cage visualization, validation, animation binding, and diagnostics.
2. Build a standalone Render Dependency Graph diagnostic pass that creates and
   traces the representation with correct resource lifetime and synchronization.
3. Reconstruct original primitive/material attributes and shade a constrained
   engine material in a mixed scene.
4. Determine with code evidence whether the method can participate through the
   engine scene TLAS or requires a separate TLAS. Measure flattened per-tet
   instances and do not assume a third AS level.
5. In an engine-source fork if required, integrate the representation with
   hardware ray-traced effects and the path tracer. Investigate Lumen separately
   and state exactly which Lumen path, if any, traces it.
6. Compare against UE native static, skinned, WPO, and Nanite/ray-tracing
   representations, including scene-update, shader-table, culling, and memory
   costs.
7. Keep raster and ray representations synchronized for animation, materials,
   visibility, LOD, and motion vectors.

Target Windows/NVIDIA first for the full UE ray-tracing proof. Scope macOS/Metal
to capabilities actually exposed by the pinned UE version, while retaining the
standalone Metal backend as the platform proof.

Acceptance evidence:
- Import/editor and diagnostic render work in a clean sample project.
- A real engine material receives correct source attributes.
- Reports clearly distinguish separate diagnostic TLAS, engine scene TLAS,
  path-tracer integration, hardware effects, and Lumen support.
- Million-instance behavior is measured against UE's native scene update; a
  tiny plugin demo is not a scale result.
- Packaging/build instructions and engine patches are reproducible.

If public/plugin APIs cannot support the core representation, document the
smallest engine-source changes instead of expanding dense geometry each frame
and calling the method integrated.
```

## Goal 12 - Cycles Metal and OptiX integration

```text
Implement Milestone M12 in a pinned Cycles/Blender revision while preserving
other Cycles devices.

Required work:
- add a canonical tet-cage asset/import path and animation binding;
- extend the Cycles Metal acceleration-structure layer to cache immutable
  micro-BLAS objects and generate per-frame cage instances;
- extend the OptiX layer to construct equivalent GAS/IAS geometry for NVIDIA;
- introduce the smallest shared kernel abstraction needed to map hits back to
  original Cycles primitive, shader, and source barycentrics;
- make standard Cycles attribute, texture, normal, motion, light-linking, and
  visibility semantics work rather than adding a separate toy shader;
- validate motion blur, mixed ordinary/tet geometry, instancing, cancellation,
  device reset, and out-of-memory behavior;
- provide Blender UI/debug views for cage state and fallback selection;
- benchmark full Cycles renders, not isolated AS traversal.

Acceptance evidence:
- The same .blend scene renders through Metal and OptiX with explained,
  tolerance-bounded differences.
- Source materials and attributes survive clipping.
- CPU and unrelated GPU devices continue to build and run their existing tests.
- Results compare native Cycles deformation/AS behavior with the tet path,
  including memory and time to first pixel/frame.
- Motion interpolation is proven equivalent to the intended cage interpolation
  for supported cases.

Keep platform-specific AS code inside the device layers and avoid distributing
tet-specific branches throughout the shading kernel.
```

## Goal 13 - RenderMan public-API feasibility gate

```text
Execute Milestone M13 as a capability study with a working procedural prototype,
not as an assumed full renderer integration.

Pin the RenderMan version and verify current public documentation and installed
headers. Produce a code-cited matrix for Riley, procedurals, RIS, XPU, motion,
instancing, user geometry/intersection, and acceleration-structure control.

Build the strongest public-API prototype possible:
- load and validate the canonical tet-cage asset;
- bind cage animation and materials;
- retain immutable geometry and update transforms if the API supports the needed
  granularity;
- otherwise emit a deliberately bounded diagnostic subset and measure the cost;
- demonstrate source primitive/material/attribute mapping.

Answer explicitly:
- Can public APIs preserve the immutable micro-geometry plus per-tet-transform
  design?
- Can they represent the required instance count efficiently?
- Can a plugin provide custom intersection/AS behavior in RIS and XPU?
- What differs on NVIDIA and macOS?
- What requires Pixar source access, a private SDK, or engineering partnership?

Acceptance evidence:
- The prototype and commands are reproducible on a licensed installation.
- The report distinguishes an asset/procedural bridge from preservation of the
  paper's acceleration-structure benefit.
- Make a go/no-go decision for public integration and list the exact vendor
  questions for any partnership.

Do not claim a true implementation if the procedural expands dense triangles
per frame or if XPU never traces the custom representation.
```

## Goal 14 - Production hardening and reproducible release

```text
Complete Milestone M14 only after the retained backends and integrations meet
their earlier exit gates.

Productionize:
- stabilize and version the canonical asset format with migration and golden
  tests;
- fuzz parsers, clipping, cage validation, provenance, and malformed assets;
- make asset compilation deterministic and cacheable;
- run CPU/shader conformance vectors across Metal, Vulkan, CUDA if retained,
  OptiX, and engine adapters;
- add CI for portable CPU work and documented hardware CI/benchmark procedures;
- implement safe allocation limits, cancellation, device loss/reset, and
  unsupported-feature fallbacks;
- finish licensing, third-party attribution, and a focused patent/FTO review
  appropriate to the intended release;
- publish sample assets, authoring guidance, troubleshooting, and renderer
  integration docs;
- produce a paper-style reproducibility report with commands and manifests
  behind every result and claim.

Acceptance evidence:
- A clean checkout reproduces all declared CPU checks and supported hardware
  proofs from documented commands.
- Every supported configuration and fallback is explicit.
- Performance charts are regenerated from checked result manifests.
- Headline claims link to exact commit/configuration/device/scene evidence.
- Known limitations include topology change, high-frequency sub-cage motion,
  sharp skinning, degeneration, instance limits, and renderer-specific gaps.

Do not call the project complete because one backend is fast. The release scope
must state exactly which backends, renderers, content domains, and robustness
mode are production supported.
```

## Cross-cutting audit prompt

Use this after any milestone or before accepting a major performance claim.

```text
Perform a requirement-by-requirement adversarial completion audit of the current
ray-tracing-tet-cage milestone. Read the paper analysis, roadmap, the milestone
prompt, implementation, tests, and result manifests.

For every required deliverable and exit gate, classify it as:
- directly proven;
- partially proven;
- contradicted;
- not tested;
- blocked by unavailable hardware/API/access.

Do not accept compilation, mocked capability output, a synthetic image, or a
schema-only report as proof of runtime correctness, watertightness, massive
scale, engine participation, or performance. Re-run safe repo-native checks,
inspect manifests for missing timing/memory categories, and look for comparisons
that use unequal scenes/rays/settings.

Turn actionable findings into tests, code, documentation, or corrected result
claims within the milestone scope. Leave a concise dated audit artifact naming
the exact remaining exit gates and the commands/evidence needed to close them.
Do not weaken the original contract to declare success.
```
