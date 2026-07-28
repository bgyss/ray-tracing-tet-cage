# Production-hardening slice

The portable release lane includes a deterministic malformed-asset smoke
corpus. It mutates 256 bytes using a fixed seed, runs the real inspector in a
subprocess, rejects malformed inputs, and verifies that any accepted mutation
still emits schema-versioned JSON:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
jq . build/nix/asset-fuzz-report.json
```

`tests/asset_cache_smoke.sh` also proves the content-addressed compiler cache:
the first build records a miss, the second records a hit, and both canonical
asset outputs are byte-identical. The cache key includes the input bytes and
compiler tolerance policy; it does not imply runtime performance.

The CTest `tetcage.result_manifest_smoke` check also parses every checked-in
shared result manifest, including nested runs in the Metal and portable scale
sweeps, and requires the common timing, memory, correctness, evidence, and
provenance fields. Feasibility and integration reports remain separate schemas
by design.
The `tetcage.result_report_smoke` check exercises the deterministic aggregate
generator used by `mise run report`, so checked-in summaries can be regenerated
without hand-editing benchmark claims.
Portable CI regenerates both summary files and fails if the checked-in copies
would change, making report drift visible during review.

This is a parser safety gate, not a substitute for coverage-guided fuzzing.
The neutral runtime frame contract now has explicit portable outcomes for
allocation limits, cancellation, unsupported update strategies, device loss,
and reset requests. Re-run the policy manifest with:

```sh
tetcage_runtime_policy build/nix/one-tet.tetcage \
  results/runtime/2026-07-28-safe-frame-policy.json
jq . results/runtime/2026-07-28-safe-frame-policy.json
```

The result is a CPU/stub contract proof: cancellation retains the last valid
frame when possible, while resource exhaustion, device loss, reset, and
unsupported updates choose conventional dynamic fallback. Hardware adapters
must map their API errors and queried allocation limits to the same statuses.
Cross-device shader conformance, historical format migration beyond v1,
licensed renderer gates, and actual GPU device-reset recovery remain
platform-specific M14 work.

The optional CUDA/Vulkan interop experiment has its own host gate. The
`interop-probe` task records tool/device prerequisites and defaults to
`remove_from_production_path`; importing a buffer or finding `nvcc` is not
treated as evidence of a useful end-to-end interop path.

The v1 parser now rejects assets over the 256 MiB safety limit, non-finite
floating-point payloads, impossible stream counts, out-of-range stream indices,
contradictory primitive provenance, and unsupported format versions before
exposing an asset to the runtime. The portable test suite exercises each
rejection path and checks the
checked-in one-tet fixture against its golden checksum. The
`tetcage_asset_migrate` identity adapter records byte/checksum equality in
`results/formats/2026-07-28-v1-migration.json`. Version 1 is the initial
format, so there is no older version to translate yet; future format changes
must add an explicit migration table and golden fixtures rather than silently
accepting a new version.

The backend-neutral frame policy also accepts an optional maximum instance
count. The portable stub preflights visible objects against that limit before
building transforms and returns an actionable error instead of allocating an
oversized frame. Hardware adapters must apply their queried device limit to the
same policy.

The shared math layer also derives scale-, ULP-, edge-length-, and
conditioning-aware tolerance decisions. `tetcage_tolerance_probe` records the
portable policy case in `results/robustness/2026-07-28-tolerance-policy.json`;
GPU boundary residuals remain an M8 hardware gate.
