# Production-hardening slice

The portable release lane includes a deterministic malformed-asset smoke
corpus. It mutates 256 bytes using a fixed seed, runs the real inspector in a
subprocess, rejects malformed inputs, and verifies that any accepted mutation
still emits schema-versioned JSON:

```sh
XDG_CACHE_HOME="$PWD/.cache" mise run check
jq . build/nix/asset-fuzz-report.json
```

This is a parser safety gate, not a substitute for coverage-guided fuzzing or
device-loss testing. The remaining M14 work is explicitly platform-specific:
allocation/cancellation/device-reset behavior on retained GPU backends,
cross-device shader conformance, migration/golden assets, and release charts
must follow the hardware and renderer gates.
