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

The newer procedural-AABB/compensated kernel passes the declared 1,408-ray
corpus without CPU fallback and exposes `--gpu-only`, but its one-shot worktree
timings do not retroactively update this sweep. Regenerate the scale study with
the new kernel before making throughput or crossover claims. A meaningful M9
crossover still requires repeated runs, a production-style animated asset,
dense-deformation baseline, and NVIDIA comparison.

The portable control matrix can be regenerated with:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run portable-scale
```

It runs the CPU/stub benchmark at 1, 4, 16, and 64 copies with the same ray
count and motion amplitude. Its nested manifests are retained as synthetic
controls and are not a substitute for a Vulkan/NVIDIA crossover study.
