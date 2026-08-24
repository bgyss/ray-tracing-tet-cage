# Social announcement posts

## M5 Metal correctness and GPU-only closure

These drafts announce the specific clean-commit Metal milestone. They claim
only the measured Apple M1 Max corpus and configuration; broader production
content and Vulkan comparison remain open.

Project: <https://github.com/bgyss/ray-tracing-tet-cage>

Evidence: [clean M5 result manifest](../results/metal/2026-07-29-m5-clean-rerun.json)

### LinkedIn

The Metal M5 milestone is now closed for the declared Apple M1 Max corpus in my tetrahedral-cage ray-tracing reimplementation study.

The final committed path replaces fixed-function triangle acceptance with conservative procedural AABBs and a compensated projected-edge narrow phase. That lets Metal handle candidate traversal, intersection acceptance, stable shared-boundary ownership, hit distance, provenance, and attributes without a CPU final-hit oracle.

Clean-commit results:

- 1,408/1,408 audited correctness rays passed;
- zero hardware mismatches, misses, wrong owners, or CPU fallback hits;
- GPU-only mode emitted 1,408/1,408 complete final records with zero CPU-oracle rays;
- an 8-frame dense animation completed six TLAS refits and one periodic rebuild with zero dense CPU mesh regenerations;
- standard and extended 65,536-instance limit builds both passed; and
- five repeated GPU-only scale runs covered 1, 4, 16, and 64 copies.

The result closes the declared M5 gate for this device and corpus—not the entire research program. Broader production content, cross-platform Vulkan validation, and massive-scale crossover work remain open.

Repository: <https://github.com/bgyss/ray-tracing-tet-cage>

The implementation and evidence are documented in the repository’s Metal mismatch study, roadmap, and dated result manifest.

#RayTracing #ComputerGraphics #GPUProgramming #Metal #AppleSilicon #Rendering #NumericalRobustness

### X/Twitter

Metal M5 is closed for the declared Apple M1 Max corpus.

The committed path uses procedural AABBs + compensated projected-edge intersection, so Metal handles traversal, acceptance, ownership, provenance, and attributes without the CPU final-hit oracle.

Results: 1,408/1,408 audited rays passed; 0 mismatches, misses, wrong owners, or CPU fallback hits. GPU-only mode produced 1,408/1,408 final records with 0 oracle rays.

Also verified: 8-frame dense refit/rebuild, standard + extended 65,536-instance builds, and five repetitions at 1/4/16/64 copies.

Repo: https://github.com/bgyss/ray-tracing-tet-cage

Broader production content and Vulkan comparison remain open.

#Graphics #RayTracing #Metal #GPUProgramming

#### Optional X/Twitter follow-up

The important fix was not just “use more precision.” The CPU owner resolver and Metal shader now share the same near-equal-distance boundary bucket and stable source/owner ordering. A focused regression caught the case where binary64 sorting disagreed with the documented ownership contract.

### Reddit

#### Title

Metal M5 correctness milestone closed: 1,408 audited rays, zero CPU fallback

#### Body

I’ve completed the clean committed Metal M5 gate for my tetrahedral-cage ray-tracing reimplementation:

https://github.com/bgyss/ray-tracing-tet-cage

The retained path uses Metal acceleration structures over conservative procedural AABBs. The Metal kernel performs the narrow-phase projected-edge ray/triangle test using compensated three-component float expansions, then resolves near-equal boundary candidates with the stable source/owner rule and reconstructs provenance and attributes.

The clean Apple M1 Max rerun reports:

1. 1,408/1,408 CPU-audited correctness rays passed.
2. Zero hardware mismatches, misses, wrong ownership, or CPU fallback hits.
3. GPU-only mode produced 1,408 complete final records without constructing or invoking the CPU intersection oracle.
4. An 8-frame dense animation completed six TLAS refits and one periodic rebuild without regenerating a dense CPU mesh.
5. Standard and extended 65,536-instance limit builds both completed.
6. Five repeated GPU-only measurements covered 1, 4, 16, and 64 copies.

This closes M5 for the declared device, fixtures, and measurement matrix. It does not claim production-scale content coverage, a Vulkan result, or a cross-platform performance crossover. Those remain later roadmap work.

The dated result manifest records the source commit, clean-tree status, correctness totals, dense-animation counters, limit checks, and repeated scale statistics.

## Short Reddit version

The Metal M5 gate is now closed for the declared Apple M1 Max corpus:

https://github.com/bgyss/ray-tracing-tet-cage

The procedural-AABB Metal path passes 1,408/1,408 audited rays with zero mismatches and zero CPU fallback. GPU-only mode emits all final records without the CPU intersection oracle. I also verified dense refit/rebuild behavior, standard/extended 65,536-instance builds, and repeated 1/4/16/64-copy measurements.

This is a scoped milestone result, not a claim of production-scale or cross-platform performance.

### Posting notes

- LinkedIn: attach a compact diagram showing procedural AABB traversal, compensated narrow phase, and GPU-only final-hit output.
- X/Twitter: post the main version, then use the ownership-bucket regression as the follow-up.
- Reddit: include the dated result manifest and explain that the gate is closed only for the declared Apple device/corpus.
- Keep the distinction between CPU-audited correctness and GPU-only execution explicit; GPU-only oracle-derived error fields are intentionally unavailable rather than inferred.

These drafts announce the public planning/reimplementation repository. They intentionally do not claim that the implementation or performance has been validated yet.

Project: <https://github.com/bgyss/ray-tracing-tet-cage>

Paper: <https://doi.org/10.1145/3820014>

## LinkedIn

I’m opening a new project to investigate and re-implement a fascinating idea from *Ray Tracing Massive Amounts of Animated Geometry* by Holger Gruen, Carsten Benthin, Michael Kern, and David McAllister.

The core idea is to clip a dense rest-pose mesh into a coarse tetrahedral cage, keep the clipped micro-geometry static, and animate the scene by updating cage vertices and top-level instance transforms. In the right workload, that could replace repeated dense geometry updates with a much smaller deformation and acceleration-structure update path.

This repository is the research and implementation plan for:

- a portable CPU reference and watertightness oracle;
- Metal ray tracing on macOS;
- Vulkan ray tracing and an optional CUDA/Vulkan interop path on NVIDIA;
- Unreal Engine 5 integration;
- Cycles Metal and OptiX integration; and
- a feasibility study for RenderMan.

The hard questions are as interesting as the algorithm: clipping and attribute provenance, cracks at shared tetrahedral boundaries, degenerate animated tetrahedra, millions of top-level instances, memory overhead, and whether a renderer’s existing acceleration-structure hierarchy can support the representation efficiently.

I’ve published a detailed paper analysis, a proof-gated M0–M14 roadmap, optimization opportunities, benchmark design, and copy-ready implementation prompts.

Repository: <https://github.com/bgyss/ray-tracing-tet-cage>

This is planning and research at the moment—not a claim of finished implementation or reproduced performance. Contributions, platform-specific feedback, and informed criticism are welcome.

#RayTracing #ComputerGraphics #GPUProgramming #Metal #Vulkan #CUDA #UnrealEngine #Blender #Cycles

## X/Twitter

I’m starting a reimplementation study of *Ray Tracing Massive Amounts of Animated Geometry*.

The idea: clip dense meshes into a tetrahedral cage, keep micro-geometry static, and animate with cage deformation + top-level instance transforms.

Roadmap covers Metal, Vulkan/CUDA, UE5, Cycles, and RenderMan.

Repo: https://github.com/bgyss/ray-tracing-tet-cage

Paper: https://doi.org/10.1145/3820014

Planning/research so far—no unverified performance claims.

#Graphics #RayTracing #Vulkan #Metal

### Optional X/Twitter follow-up

The main engineering risk is not the affine mapping. It’s the system around it: deterministic clipping and hit provenance, watertight shared boundaries, degenerate cages, TLAS scaling, memory, and renderer scene-hierarchy limits.

The first milestone is a CPU oracle; GPU backends come after correctness.

## Reddit

### Title

I’m building a proof-gated reimplementation plan for “Ray Tracing Massive Amounts of Animated Geometry”

### Body

I’ve published a new repository for studying and eventually re-implementing the method from [*Ray Tracing Massive Amounts of Animated Geometry*](https://doi.org/10.1145/3820014):

https://github.com/bgyss/ray-tracing-tet-cage

The basic representation is:

1. Clip a dense rest-pose mesh into the tetrahedra of a coarse deforming cage.
2. Build static triangle acceleration structures for the clipped micro-meshes.
3. Animate the cage and derive one affine transform per tetrahedron.
4. Trace through top-level instances that reuse the static micro-geometry.

The potential payoff is reducing repeated dense vertex and BLAS work for large amounts of smoothly deforming, fixed-topology geometry. The tradeoffs are substantial: clipping expands geometry, the number of top-level instances can become very large, and a fast hardware-triangle path needs careful treatment of cracks and duplicate hits at shared cage boundaries.

The repository currently contains a detailed analysis of the paper, including the affine mapping, clipping/provenance requirements, fast and 4D watertight variants, reported measurements, and unspecified implementation details. It also contains a 15-milestone roadmap and standalone prompts for driving the work.

The planned implementation tracks are:

- CPU geometry/compiler and a brute-force plus 4D watertight correctness oracle;
- Metal ray tracing on macOS;
- Vulkan ray tracing on NVIDIA, with CUDA/Vulkan interop treated as an experiment;
- direct comparison against NVIDIA animated-cluster approaches;
- Unreal Engine 5 integration;
- Cycles Metal and OptiX integration; and
- a public-API feasibility gate for RenderMan.

One important constraint is that this is not being presented as finished software. The repository is a research and implementation plan. The first exit gates are deterministic clipping, source-attribute reconstruction, adversarial boundary-ray tests, and measured comparisons against ordinary dense deformation. Small synthetic scenes will not count as proof of massive-scale performance.

I’d especially appreciate feedback from people who have dealt with:

- very large TLAS instance counts;
- Metal or Vulkan acceleration-structure update/refit behavior;
- custom geometry and hit provenance in UE5, Cycles, OptiX, or RenderMan; and
- robust tetrahedral cages for production animation.

## Short Reddit version

I published a planning repository for re-implementing a paper on ray tracing massive animated geometry with tetrahedral cages:

https://github.com/bgyss/ray-tracing-tet-cage

The method keeps clipped micro-geometry static and updates cage vertices plus top-level instance transforms. The roadmap targets CPU correctness, Metal, Vulkan/CUDA, UE5, Cycles, and RenderMan.

It’s explicitly a research plan—not finished implementation or reproduced benchmark results yet. The hard parts are TLAS scaling, clipping/provenance, watertight shared boundaries, degenerate cages, and renderer integration.

## Posting notes

- LinkedIn: attach a clean diagram of the cage → static micro-mesh → animated instance flow if one is available.
- X/Twitter: post the main version, then use the follow-up as a second post or reply with a short diagram/GIF.
- Reddit: use the long version in a graphics, rendering, GPU programming, or engine-specific community only where project/research posts are welcome; keep the paper link and repository link visible.
- If implementation results become available later, replace the “planning/research” language with measured numbers and link the result manifest rather than relying on an unqualified speedup claim.

## Cycles/Blender integration progress and path forward (2026-08-24)

These drafts describe the measured Cycles/Blender integration work without
presenting the exploratory adapter as an upstream merge or production feature.

Project: <https://github.com/bgyss/ray-tracing-tet-cage>

Paper: <https://doi.org/10.1145/3820014>

Evidence: [Cycles/Blender entry manifest](../results/integrations/2026-08-23-cycles-metal-entry.json), [area-volume precision record](../results/integrations/2026-08-23-cycles-area-volume-precision.json), and [representation boundary](cycles-representation-boundary.md)

### LinkedIn

Cycles/Blender integration is now a real, measured workstream for my tetrahedral-cage ray-tracing reimplementation.

The project is based on the representation described in [*Ray Tracing Massive Amounts of Animated Geometry*](https://doi.org/10.1145/3820014): keep clipped micro-geometry static, animate through cage/tetrahedron transforms, and preserve source-hit provenance. The goal here is not to claim a finished Blender feature, but to find the smallest honest path from that representation into an existing renderer.

What is now proven on an Apple M1 Max:

- pinned standalone Cycles and Blender source/dependency checkouts, a full Blender developer build, app install, UI/Cycles smoke, and tet-cage import/save/reload/debug checks;
- an opt-in candidate Cycles/Blender adapter that builds conservative AABB candidates and reconstructs ordinary Cycles hits;
- static tet-only and mixed triangle/AABB traversal, rigid object-transform motion, shape-key motion, source primitive/material/UV/normal attributes, transparent camera/shadow, AO/local, and volume probes; and
- 41/41 repository tests plus the pinned source/dependency verifier.

The boundaries matter. Mixed transparent plus non-transparent motion remains ordinary Cycles fallback by default. Pure-transparent motion is eligible for the native experiment. Large-footprint area-light volume precision is still open: at 64 samples the size-2 control differs by `0.00766` max RGB, while the point-light control is float-noise-only. Those are gates, not footnotes.

The path to formal integration is:

1. Close renderer semantics on a representative animated corpus.
2. Run M9's cross-platform method/scale study and select the retained representation.
3. Turn the exploratory adapter into a dedicated, reviewable Cycles geometry contract with explicit provenance and fallback behavior.
4. Validate Metal and OptiX against the same scenes while preserving existing Cycles devices.
5. Submit a focused Blender/Cycles patch series with reproducible tests, docs, UI/debug views, and evidence that survives upstream review.

This is an integration candidate, not an upstream merge or production performance claim. Feedback from Cycles/Blender, Metal, OptiX, and rendering folks is welcome.

Repository: <https://github.com/bgyss/ray-tracing-tet-cage>

#Blender3D #Cycles #RayTracing #ComputerGraphics #Metal #OpenSource #Rendering

### X/Twitter thread

#### Post 1

Cycles/Blender integration is a measured workstream for my tetrahedral-cage ray-tracing reimplementation. Pinned macOS builds, Blender UI/debug smoke, and tet-cage import/save/reload pass. This is groundwork—not an upstream merge: https://github.com/bgyss/ray-tracing-tet-cage

#### Post 2

The opt-in candidate covers static tet + ordinary-triangle traversal, rigid/shape-key motion, source attributes, transparent/AO/local/volume probes on an Apple M1 Max. Mixed transparent motion stays ordinary Cycles fallback by default.

#### Post 3

Next: close semantic parity on representative assets, run M9 cross-platform method/scale selection, then submit a focused Cycles/Blender patch series. OptiX still needs a qualified NVIDIA host; area-light volume precision is open.

### Reddit

#### Title

Cycles/Blender tet-cage integration is measurable now, but still experimental

#### Body

I’m continuing a proof-gated reimplementation of the tetrahedral-cage representation from [*Ray Tracing Massive Amounts of Animated Geometry*](https://doi.org/10.1145/3820014): clip dense rest-pose geometry into a coarse tetrahedral cage, keep the clipped micro-geometry static, and animate through cage/tet transforms and top-level instances.

The new work is an exploratory Cycles/Blender integration lane, not an official Blender or Cycles feature. The pinned standalone Cycles source is `97dbe6f`; the pinned Blender source is `4a09c19`. The candidate branches are intentionally separate from those clean upstream checkouts.
The formal upstream target is Cycles as maintained in the [Blender repository](https://github.com/blender/blender); the standalone checkout is only a core/device prototyping lane.

On an Apple M1 Max, the current evidence includes:

- a full Blender developer build and install, Cycles/OSL/UI smoke, and tet-cage import/save/reload/debug checks;
- an opt-in candidate adapter that uses conservative AABB candidates and maps hits back to ordinary Cycles identity/provenance;
- static tet-only and mixed triangle/AABB traversal;
- rigid object-transform motion and animated shape-key motion;
- source primitive/material/UV/normal attributes;
- transparent camera/shadow, AO/local, SSS/local, and volume probes; and
- 41/41 repository tests plus the pinned source/dependency verifier.

The limitations are part of the result:

- mixed transparent plus non-transparent motion stays on ordinary Cycles fallback by default;
- pure-transparent motion is the narrower native candidate;
- the large-footprint area-light volume seam remains open (`0.00766` max RGB at 64 samples for the size-2 control), even with shadows disabled; and
- this is not yet a production `TetCageGeometry`, an upstream patch, an OptiX result, or a cross-platform performance claim.

The path to formal integration follows the roadmap rather than skipping ahead:

1. Build a representative animated corpus and close camera, shadow, transparent, volume, local/SSS, motion, and attribute parity.
2. Complete M9's cross-platform scale/crossover study and choose the retained method.
3. Replace the exploratory adapter with a dedicated Cycles geometry boundary that preserves provenance, existing device behavior, and explicit fallback.
4. Validate Metal and OptiX on the same scenes, with a qualified NVIDIA host for the OptiX side.
5. Prepare a small, reviewable Blender/Cycles patch series with source tests, reproducible build instructions, UI/debug views, and image/time/memory evidence.

The repository, roadmap, representation boundary, and dated manifests are here:

- <https://github.com/bgyss/ray-tracing-tet-cage>
- [`docs/open-roadmap-workplan.md`](open-roadmap-workplan.md)
- [`docs/cycles-representation-boundary.md`](cycles-representation-boundary.md)
- [`results/integrations/2026-08-23-cycles-metal-entry.json`](../results/integrations/2026-08-23-cycles-metal-entry.json)
- [`results/integrations/2026-08-23-cycles-area-volume-precision.json`](../results/integrations/2026-08-23-cycles-area-volume-precision.json)

I’d especially appreciate feedback from Cycles/Blender maintainers and people who have dealt with custom geometry, Metal intersection-function tables, OptiX GAS/IAS updates, or renderer-visible hit provenance.

### Posting notes

- LinkedIn: attach a simple diagram of the immutable micro-geometry → AABB candidate → normalized Cycles hit boundary, plus a small “proven / open / next” panel.
- X/Twitter: post the three-part thread; the first post is the link-bearing opener and the third post makes the formal-integration boundary explicit.
- Reddit: keep the long body intact so the fallback gates and upstream relationship are visible; include the dated manifests rather than a benchmark screenshot without context.
- Do not describe the candidate branch as “merged into Cycles,” “production-ready,” or “formally integrated” until the M9/M12 acceptance evidence is complete and an upstream review path exists.
