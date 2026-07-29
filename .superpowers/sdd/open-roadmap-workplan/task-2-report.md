# Task 2 report — WP1 Metal correctness and dense-animation evidence

## Outcome

Implemented the real Metal WP1 measurement path and recorded direct Apple M1
Max evidence. The fallback now supplies the CPU-oracle final hit and complete
attributes after hardware traversal instead of excluding a ray. The dense path
keeps canonical micro-BLAS geometry immutable across eight animated frames,
updates only cage transforms/instance descriptors, performs six TLAS refits and
one periodic rebuild, and reports zero dense CPU mesh regenerations. Standard
and extended modes each complete a real 65,536-instance TLAS build and trace.

M5 remains open. Across the four accepted WP0 assets, the hardware path records
672 classified mismatches over 1,408 rays. The correctness-preserving fallback
leaves zero final misses or ownership errors, but it runs for 1,099 rays
(78.1%; 87.5% on the dense animation) and currently validates every hardware
result with the CPU oracle. WP5 has not accepted that cost. Automated
hardware-replay minimization for every recorded mismatch and a separately timed
GPU attribute-recovery stage are also still pending.

## Implementation

- Added portable C++20 mismatch classes covering all seven required categories
  and a deterministic predicate-backed coordinate minimizer.
- Expanded each observed mismatch record with posed cage/tetrahedra, embedded
  source triangle, ray interval, expected primitive/owner/micro-triangle,
  hardware primitive/instance/material, CPU/GPU source barycentrics, CPU cage
  barycentrics, and GPU triangle barycentrics.
- Retained an observed dense shared-edge regression under
  `tests/assets/metal-regressions/`; it is explicitly marked pending hardware
  replay rather than falsely labeled minimized.
- Reworked boundary fallback accounting so the final result is the CPU hit,
  including position, normal, UV, material, ownership, and barycentrics.
  Manifests include final-result samples, fallback frequency, selected fallback
  time, and the full CPU decision-oracle time.
- Added `--frames` and `--rebuild-period`, animated per-frame instance
  descriptors, in-place TLAS refits, periodic TLAS rebuilds, per-frame tracing
  and correctness validation, transfer accounting, and immutable-BLAS policy
  reporting.
- Added a reproducible real-Metal evidence runner and CTest smoke coverage.

## Direct evidence

- `results/metal/2026-07-28-correctness-corpus.json`
  - four accepted WP0 assets; 1,408 rays; 672 hardware mismatches;
    1,099 final fallbacks; zero final misses/ownership errors.
- `results/metal/2026-07-28-dense-animation.json`
  - eight frames; six TLAS refits; one rebuild; zero dense CPU mesh
    regenerations; 1,024 rays; 896 fallbacks; zero final misses/ownership
    errors.
- `results/metal/2026-07-28-limit-builds.json`
  - standard and extended usage each completed a 65,536-instance TLAS build
    and trace on Apple M1 Max; this proves the tested points, not maximum
    supported limits.

## Requirement audit

1. Complete mismatch fixtures: implemented for every observed mismatch in the
   dated corpus.
2. Seven-way classification: implemented and portable-tested.
3. Deterministic minimizer: implemented and portable-tested; automated
   hardware replay and verified reduced fixtures remain open.
4. Declared ownership rule: lowest stable source primitive/owner tet is
   declared; hardware shared-edge disagreements remain and are not hidden by a
   broadened tolerance.
5. Correctness fallback: implemented as final CPU hit/attributes with samples,
   frequency, selected cost, full decision cost, and end-to-end total.
6. Dense animation without CPU dense-mesh regeneration: directly measured.
7. Measurements: BLAS build, TLAS build/refit/rebuild, traversal, fused
   attribute recovery, shared-memory transfers, peak memory, fallback frequency,
   and total time are present. Static canonical BLAS update is explicitly
   not-applicable; separate GPU attribute-recovery duration is unavailable.
8. Standard/extended limits: both real 65,536-instance builds completed; no
   descriptor-size-only claim and no maximum-limit claim.

## Verification

- Focused portable + Metal evidence CTests: pass.
- Real Metal evidence runner: exit 0 for all seven direct runs.
- `XDG_CACHE_HOME="$PWD/.cache" MISE_DISABLE_VERSION_CHECK=1 mise run check`:
  pass, 30/30 CTests.
- `XDG_CACHE_HOME="$PWD/.cache" MISE_DISABLE_VERSION_CHECK=1 mise run nix-check`:
  pass, portable package 27/27 tests plus tooling contract.
- `XDG_CACHE_HOME="$PWD/.cache" MISE_DISABLE_VERSION_CHECK=1 mise run report`:
  pass; reproducibility summaries regenerated.
- Known unrelated warning: the Vulkan capability probe emits its pre-existing
  initializer/function-pointer warnings during builds.
