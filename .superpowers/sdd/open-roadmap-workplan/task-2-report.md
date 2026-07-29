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
hardware-replay minimization now preserves every recorded mismatch
classification. GPU attribute recovery remains fused with traversal and is
therefore explicitly unavailable as a separate duration.

## Implementation

- Added portable C++20 mismatch classes covering all seven required categories
  and a deterministic predicate-backed coordinate minimizer.
- Expanded each observed mismatch record with posed cage/tetrahedra, embedded
  source triangle, ray interval, expected primitive/owner/micro-triangle,
  hardware primitive/instance/material, CPU/GPU source barycentrics, CPU cage
  barycentrics, and GPU triangle barycentrics.
- Retained all 672 minimized, classification-preserving hardware regressions
  under `tests/assets/metal-regressions/`.
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
3. Deterministic minimizer: implemented and portable-tested; every recorded
   mismatch was reduced and re-verified by real Metal replay with the same
   classification.
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

## Review-fix round

The review fixes were measured from exact clean revision
`15677d0afc107ed171721f3d768b5371033d9fa8`.

- The deterministic minimizer now invokes the real Metal tracing pipeline as
  its predicate. All 672 correctness-corpus hardware mismatches retain their
  original classification after reduction, with zero minimization failures.
  The deterministic aggregate is
  `tests/assets/metal-regressions/2026-07-28-minimized-corpus.json`.
- Every ray now emits one explicit selected final-hit record. The four-asset
  corpus contains 1,408 records and the independent evidence validator confirms
  complete ray-index coverage, selected CPU/GPU path accounting, complete hit
  attributes, and zero merged-stream errors.
- The same-work comparator reports hardware traversal, the all-ray CPU
  selection oracle, selected-fallback attribution, merge work, and actual
  trace-plus-selection-plus-merge time over identical rays. Selected fallback
  time is explicitly labeled as a reused subset of the selection-oracle time,
  so it is not double-counted in the end-to-end duration.
- `timings_ms.shading` is `null`. CPU validation is reported separately as
  `statistics.cpu_validation_ms`; separate GPU attribute-recovery time is
  `null` because recovery is fused into the traversal kernel.
- The evidence runner has no unverified bypass. It rejects non-zero exits,
  non-measured/non-direct results, failures, incomplete frame/ray/instance
  counts, dirty or inexact source revisions, incomplete minimizations, and
  invalid final streams before accepting the bundle.

M5 remains open: the pure hardware path still produces 672 mismatches and the
correctness-preserving selector still routes 1,099 of 1,408 rays through the
CPU result. The review fixes close the evidence-integrity gaps; they do not
make that fallback frequency or method-selection cost acceptable.

## Selection-timer re-review

The final timing correction was measured from exact clean revision
`7ffab58d22f6327ce15aabf37497641136b9c052`.

`selection_oracle_ms` now begins before the CPU selector trace and ends only
after the boundary-sensitive test, CPU-versus-hardware comparison, and
`MetalFinalPath` choice. Exact-oracle classification, mismatch diagnostics, and
all hardware-replay minimization work occur after the timer and are excluded.
Every same-work comparator declares this scope as
`cpu_trace_boundary_test_hardware_comparison_and_path_choice`, and the strict
evidence validator rejects runs without that declaration.

All seven direct runs passed again with the clean revision and unchanged
correctness counts. For the 1,024-ray dense correctness run, the complete
selection path measured 17.382248 ms; hardware traversal measured 4.224498 ms,
merge measured 10.317767 ms, and corrected trace-plus-selection-plus-merge
measured 31.924513 ms. The separate dense-animation run measured 16.658660 ms
for selection and 28.733967 ms end to end. These are single recorded runs, not
distribution or production-performance claims.

M5 remains open for the same substantive reason: 672 pure-hardware mismatches
and 1,099 CPU-selected results across the 1,408-ray correctness corpus still
require WP5 method-selection acceptance.
