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

The sweep varies copies/instances at a fixed ray count, uses GPU-generated
instance descriptors, and keeps the explicit experimental boundary fallback.
Boundary-sensitive rays are routed through the standalone CPU fallback and
reported as `cpu_fallback_rays`; they are not silently counted as GPU
correctness. Each embedded manifest retains BLAS/TLAS/scratch/instance memory
and stage timings. `evidence_class: direct_synthetic_scale_subset` is deliberate: a
meaningful M9 crossover still requires a production-style animated asset,
dense-deformation baseline, and NVIDIA comparison.
