# Reproducibility report

This report is intentionally proof-oriented. It records what a clean checkout
can reproduce locally and separates that evidence from hardware, source, and
license gates that cannot be closed on the current host.

## Portable lane

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
XDG_CACHE_HOME="$PWD/.cache" mise run nix-check
```

The first command runs formatting, shell checks, a fresh Nix CMake build, and
the portable/authoring/fuzz/probe CTest suite. The second evaluates the flake
and builds the Metal-disabled package in a pure Nix derivation. The v1 format
golden checksum is recorded in
`results/formats/2026-07-28-v1-golden.json`.

## Direct evidence already retained

| Area | Manifest or artifact | Evidence class |
| --- | --- | --- |
| CPU oracles | `results/cpu/2026-07-27-two-tet-oracle.json` | direct portable |
| CPU/stub benchmark and corruption gate | `results/benchmarks/2026-07-27-cpu-stub.json` and `results/benchmarks/2026-07-27-cpu-stub-corrupt.json` | synthetic differential |
| Metal capability | `results/capabilities/2026-07-27-metal.json` | direct device query |
| Metal fast path | `results/metal/2026-07-28-metal-gpu-instances.json` | direct device, partial correctness |
| Metal negative scale sweep | `results/metal/2026-07-28-metal-scale-sweep.json` | direct device, negative result |
| Vulkan capability | `results/capabilities/2026-07-27-vulkan.json` | unverified/host-limited |
| Unreal/Cycles gates | `results/integrations/2026-07-28-host-gates.json` | blocked host inspection |
| RenderMan public API gate | `results/renderman-feasibility.json` | local header feasibility |

## Explicitly unproven

The repository does not claim completion of M5, M6, M7, M8, M9, M10, M11, M12,
or M14. Closing those milestones requires the real NVIDIA/Vulkan environment,
representative animation clips, pinned UE5 and Cycles source trees, a licensed
RenderMan runtime, or additional production hardening beyond the portable
parser gate. A green portable build is not substituted for any of those
requirements.
