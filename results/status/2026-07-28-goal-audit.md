# Goal-prompt completion audit — 2026-07-28

This audit maps every prompt in `docs/goal-prompts.md` to current evidence. A
green portable check proves repository plumbing and CPU behavior only; it does
not promote unavailable GPU, renderer, scale, or production-content gates.

| Goal | Classification | Evidence or blocker |
| --- | --- | --- |
| 0 — contract and probes | directly proven | C++20/CMake/Nix/mise contract, result schema, real Metal/Vulkan probe executables, and [requirements matrix](../../docs/requirements-matrix.md); Vulkan/NVIDIA is host-limited |
| 1 — CPU mathematics | directly proven on portable lane | permutation, scale, mirror, singular, normal, clipping, and seeded differential tests in `tetcage.portable`; broader property corpus remains future work |
| 2 — asset compiler | partially proven | deterministic v1 compiler, provenance, ownership, malformed tests, golden, and migration harness; dense image corpus and expanded coverage remain open |
| 3 — CPU/4D oracles | directly proven for checked synthetic corpus | `results/cpu/2026-07-27-two-tet-oracle.json` records zero unexplained misses/duplicates for its seeded corpus and projection comparison; production-scale corpus remains open |
| 4 — runtime and benchmark harness | directly proven for CPU/stub contract | `results/benchmarks/2026-07-27-cpu-stub.json` plus deliberate corruption result and nullable timing/memory fields; no GPU conclusion is implied |
| 5 — Metal fast path | partially proven / contradicted for all-rays gate | direct M1 Max BLAS/TLAS/tracing and GPU descriptor generation exist; hardware-all-rays has residual failures, while boundary fallback is experimental (`results/metal/2026-07-28-metal-boundary-fallback.json`) |
| 6 — Vulkan fast path | blocked by environment | capability probe is present, but this host has no Vulkan SDK/device; no Vulkan AS backend or NVIDIA validation evidence is claimed |
| 7 — CUDA/Vulkan interop | blocked by dependency on M6/environment | `results/capabilities/2026-07-28-cuda-vulkan-interop.json` records absent `vulkaninfo`, `nvcc`, and `nvidia-smi`, with `remove_from_production_path` |
| 8 — GPU robustness/watertight/hybrid | partially proven on portable lane; GPU portion blocked | CPU 4D oracle and new scale/ULP/edge/conditioning policy are tested (`results/robustness/2026-07-28-tolerance-policy.json`); equal-work GPU hybrid comparison awaits M5/M6 completion |
| 9 — scale and method selection | partially proven | direct synthetic Metal sweep and the portable 1/4/16/64-copy control matrix retain negative/CPU results; measured-input selector and fallback artifact are in `results/selection/2026-07-28-two-tet-policy.json`; cross-platform crossover remains open |
| 10 — cage authoring | partially proven | clip-wide procedural residuals, constrained weight fitting, conforming refinement, and parent-mapped LODs are checked in; representative production clips and visual/TLAS transitions remain open |
| 11 — Unreal Engine 5 | blocked by source/target mismatch | `results/integrations/2026-07-28-host-gates.json` finds Unreal 4.19.2, not the required pinned UE5 checkout |
| 12 — Cycles | blocked by source checkout | Blender executable exists, but no pinned Blender/Cycles source tree for Metal/OptiX device-layer work |
| 13 — RenderMan | partial feasibility / no-go for public fast path | local 26.2 header/runtime study records Riley prototype/instance APIs but no public custom AS/intersection control; licensed runtime prototype remains required |
| 14 — production hardening | partially proven | portable CI, fuzz/malformed checks, v1 golden/migration, deterministic report, content-addressed cache smoke, and policy fallbacks exist; historical migration, shader/device conformance, sample release, device loss/reset, retained backends, and renderer gates remain |

## Reproduction commands

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
XDG_CACHE_HOME="$PWD/.cache" mise run nix-check
XDG_CACHE_HOME="$PWD/.cache" mise run report
```

The exact remaining exit gates are intentionally preserved in the roadmap and
requirements matrix rather than being downgraded to “implemented” by scaffolds
or host-limited probes.
