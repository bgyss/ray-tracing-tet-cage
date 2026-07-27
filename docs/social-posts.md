# Social announcement posts

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
