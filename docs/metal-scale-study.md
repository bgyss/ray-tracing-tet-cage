# Metal scale-study slice

The fast path has a reproducible scale command, but the current checked-in
scene is intentionally small and does not prove the paper's massive-instance
claims:

```sh
XDG_CACHE_HOME="$PWD/.cache" nix develop path:. --command \
  bash scripts/metal_scale_sweep.sh \
  results/assets/two-tet-crossing.tetcage \
  results/metal/2026-07-28-metal-scale-sweep.json
```

The checked-in sweep varies copies/instances at a fixed ray count, uses
GPU-generated instance descriptors, and was recorded with the older
intersection-query/fallback kernel. Each embedded manifest retains
BLAS/TLAS/scratch/instance memory and stage timings.
`evidence_class: direct_synthetic_scale_subset` is deliberate.

The clean committed procedural-AABB/compensated kernel now has a retained
scale artifact at
`results/metal/2026-07-29-m5-clean-rerun.json`. It runs five GPU-only
repetitions at 1, 4, 16, and 64 copies with 256 rays per run. Median traversal
GPU time stays between approximately 0.766 and 0.777 ms across this small
fixture (the five-run range is 0.765–0.936 ms), while reported peak memory
grows from 100,292 to 160,632 bytes. These are real measurements of the
retained kernel, not descriptor-size estimates. A
production-scale crossover still requires broader animated content and an
NVIDIA/Vulkan comparison.

The portable control matrix can be regenerated with:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run portable-scale
```

It runs the CPU/stub benchmark at 1, 4, 16, and 64 copies with the same ray
count and motion amplitude. Its nested manifests are retained as synthetic
controls and are not a substitute for a Vulkan/NVIDIA crossover study.
