# Reproducibility report

This report is intentionally proof-oriented. It records what a clean checkout
can reproduce locally and separates that evidence from hardware, source, and
license gates that cannot be closed on the current host.

## Portable lane

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
XDG_CACHE_HOME="$PWD/.cache" mise run nix-check
XDG_CACHE_HOME="$PWD/.cache" mise run report
```

The first command runs formatting, shell checks, a fresh Nix CMake build, and
the portable/authoring/fuzz/probe CTest suite. The second evaluates the flake
and builds the Metal-disabled package in a pure Nix derivation. The v1 format
golden checksum is recorded in
`results/formats/2026-07-28-v1-golden.json`.
The report task deterministically regenerates
`results/reproducibility-summary.json` and
`results/reproducibility-summary.md`; it summarizes shared manifests while
retaining capability, authoring, feasibility, and integration artifacts as
separate evidence classes.

CI invokes the same first command through
`.github/workflows/portable.yml` on Ubuntu. No hardware-specific result is
promoted by that job.

## Direct evidence already retained

| Area | Manifest or artifact | Evidence class |
| --- | --- | --- |
| CPU oracles | `results/cpu/2026-07-27-two-tet-oracle.json` | direct portable |
| CPU/stub benchmark and corruption gate | `results/benchmarks/2026-07-27-cpu-stub.json` and `results/benchmarks/2026-07-27-cpu-stub-corrupt.json` | synthetic differential |
| Cage animation suitability diagnostic | `results/authoring/2026-07-28-procedural-clip.json` | deterministic procedural clip, not production proof |
| Cage LOD parent maps | `results/authoring/2026-07-28-lods.json` plus `results/authoring/lod-cage-lod*.cage` | deterministic topology/refinement mapping, no visual transition proof |
| Generated result summary | `results/reproducibility-summary.json` and `.md` | deterministic manifest inventory and aggregate, not a claim promotion |
| v1 migration round-trip | `results/formats/2026-07-28-v1-migration.json` | portable identity adapter with byte/checksum equality |
| Metal capability | `results/capabilities/2026-07-27-metal.json` | direct device query |
| Metal fast path | `results/metal/2026-07-28-metal-gpu-instances.json` | direct device, partial correctness |
| Metal negative scale sweep | `results/metal/2026-07-28-metal-scale-sweep.json` | direct device, negative result |
| Vulkan capability | `results/capabilities/2026-07-27-vulkan.json` | unverified/host-limited |
| CUDA/Vulkan interop gate | `results/capabilities/2026-07-28-cuda-vulkan-interop.json` | host capability gate; no interop benchmark claimed |
| Representation selection policy | `results/selection/2026-07-28-two-tet-policy.json` | deterministic fallback evaluation from measured inputs; not performance evidence |
| Robust tolerance policy | `results/robustness/2026-07-28-tolerance-policy.json` | scale/edge/conditioning-aware portable policy; no GPU watertightness claim |
| Unreal/Cycles gates | `results/integrations/2026-07-28-host-gates.json` | blocked host inspection |
| RenderMan public API gate | `results/renderman-feasibility.json` | local header feasibility |

## Explicitly unproven

The repository does not claim completion of M5, M6, M7, M8, M9, M10, M11, M12,
or M14. The new clip evaluator advances M10 authoring diagnostics but does not
close its representative-production-clip or LOD gates. Closing the remaining
milestones requires the real NVIDIA/Vulkan environment, representative
animation clips, pinned UE5 and Cycles source trees, a licensed RenderMan
runtime, or additional production hardening beyond the portable parser gate. A
green portable build is not substituted for any of those requirements.
The v1 identity round-trip is a compatibility harness, not historical-version
translation; a future format version must add an explicit migration table and
golden fixtures before it is considered supported.
