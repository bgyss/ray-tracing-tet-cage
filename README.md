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

This repository contains planning and research only as of 2026-07-27. No
implementation or performance claim has been validated here yet.
