# Production-hardening slice

The portable release lane includes a deterministic malformed-asset smoke
corpus. It mutates 256 bytes using a fixed seed, runs the real inspector in a
subprocess, rejects malformed inputs, and verifies that any accepted mutation
still emits schema-versioned JSON:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
jq . build/nix/asset-fuzz-report.json
```

The CTest `tetcage.result_manifest_smoke` check also parses every checked-in
shared result manifest, including each line of the Metal scale sweep, and
requires the common timing, memory, correctness, evidence, and provenance
fields. Feasibility and integration reports remain separate schemas by design.
The `tetcage.result_report_smoke` check exercises the deterministic aggregate
generator used by `mise run report`, so checked-in summaries can be regenerated
without hand-editing benchmark claims.

This is a parser safety gate, not a substitute for coverage-guided fuzzing or
device-loss testing. The remaining M14 work is explicitly platform-specific:
allocation/cancellation/device-reset behavior on retained GPU backends,
cross-device shader conformance, a future format migration beyond v1, and
release charts must follow the hardware and renderer gates.

The v1 parser now rejects assets over the 256 MiB safety limit, non-finite
floating-point payloads, impossible stream counts, out-of-range stream indices,
contradictory primitive provenance, and unsupported format versions before
exposing an asset to the runtime. The portable test suite exercises each
rejection path and checks the
checked-in one-tet fixture against its golden checksum. Version 1 is the
initial format, so there is no older version to migrate yet; future format
changes must add an explicit migration table and golden fixtures rather than
silently accepting a new version.

The backend-neutral frame policy also accepts an optional maximum instance
count. The portable stub preflights visible objects against that limit before
building transforms and returns an actionable error instead of allocating an
oversized frame. Hardware adapters must apply their queried device limit to the
same policy.
