#!/usr/bin/env python3
"""Deterministic malformed-asset smoke corpus for the CLI parser."""

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import time


def next_state(state: int) -> int:
    return (state * 6364136223846793005 + 1442695040888963407) & ((1 << 64) - 1)


def main() -> int:
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <inspect-executable> <asset> <report.json>", file=sys.stderr)
        return 2
    inspector = pathlib.Path(sys.argv[1])
    asset_path = pathlib.Path(sys.argv[2])
    report_path = pathlib.Path(sys.argv[3])
    original = bytearray(asset_path.read_bytes())
    if len(original) < 32:
        print("asset is too small for fuzz smoke test", file=sys.stderr)
        return 2

    state = 0x5EEDC0DE
    accepted = 0
    rejected = 0
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="tetcage-fuzz-") as directory:
        mutated_path = pathlib.Path(directory) / "mutated.tetcage"
        for case in range(256):
            state = next_state(state)
            mutated = bytearray(original)
            offset = 8 + state % (len(mutated) - 8)
            state = next_state(state)
            mutated[offset] ^= (state & 0xFF) or 1
            mutated_path.write_bytes(mutated)
            try:
                result = subprocess.run(
                    [str(inspector), str(mutated_path)],
                    capture_output=True,
                    text=True,
                    timeout=2.0,
                    check=False,
                )
            except subprocess.TimeoutExpired:
                print(f"parser timed out on case {case}", file=sys.stderr)
                return 1
            if result.returncode < 0:
                print(f"parser terminated by signal on case {case}", file=sys.stderr)
                return 1
            if result.returncode == 0:
                try:
                    parsed = json.loads(result.stdout)
                    if parsed.get("schema_version") != 1:
                        print(f"accepted case {case} emitted an invalid schema", file=sys.stderr)
                        return 1
                except json.JSONDecodeError:
                    print(f"accepted case {case} emitted invalid JSON", file=sys.stderr)
                    return 1
                accepted += 1
            else:
                rejected += 1

    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "seed": "0x5eedc0de",
                "cases": accepted + rejected,
                "accepted_valid": accepted,
                "rejected_invalid": rejected,
                "elapsed_ms": (time.monotonic() - started) * 1000.0,
                "inspector": str(inspector),
            },
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
