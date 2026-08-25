# Blender/Cycles contribution cut plan

## Decision

Preserve the current experimental adapter as a small, replayable WIP patch
series in [`patches/blender/`](../patches/blender/). Do not submit that series
upstream unchanged. The correct contribution path is to recreate a small,
independently testable series from current Blender `main` after a Cycles design
discussion and the M9 retained-method decision.

This keeps the experimental work auditable in this repository while avoiding a
large, unstable upstream review that combines a new representation, a Blender
import marker, Metal device code, motion work, fallback experiments, and UI
assumptions.

## What is preserved

The WIP series replays cleanly from Blender base
`4a09c19bea7bd2800d85f018280b2dc62e654e51` to the experimental candidate tree
at `d92d347fdc754094e814e0f10fc314e82dc1d031`. It contains only 21
`intern/cycles/...` files and no generated or machine-local content. Its three
phases are deliberately separated into scene transport, static Metal, and
motion/fallback behavior so the experiment can be inspected or reapplied
without retaining the external worktree.

Apply and verification instructions are in
[`patches/blender/README.md`](../patches/blender/README.md).

## Why this is not yet an upstream pull request

The current code proves useful seams, but not a settled Blender/Cycles design:

- The Blender bridge uses custom ID-property markers and the
  `CYCLES_TETCAGE_NATIVE` environment switch. Those are local experiment
  controls, not an agreed asset/import or user-facing API.
- The adapter routes through `Mesh`, with a single-tetrahedron identity-frame
  bridge. It is not yet the dedicated Cycles geometry representation that a
  reviewer can evaluate as a durable design.
- Tests and evidence live primarily in this repository. A contribution needs
  minimal in-tree Cycles unit tests and Blender render fixtures that exercise
  the supported behavior without depending on this project’s importer or
  external build setup.
- Mixed transparent/non-transparent motion intentionally remains on ordinary
  Cycles fallback. Large-footprint area-light volume precision is still open at
  64 samples, including the shadow-disabled control. Neither behavior should
  be promoted by an upstream patch.
- The Metal work is Apple-host evidence only. OptiX requires a qualified NVIDIA
  host, and renderer-specific performance integration is deferred until M9
  chooses the retained representation from cross-platform, representative-asset
  measurements.

These are contribution gates, not reasons to discard the experiment.

## Proposed upstream sequence

### 1. Design task and minimal fixture

Discuss the proposed geometry boundary with Cycles maintainers before a large
implementation review. Provide a compact problem statement, source-hit
provenance contract, supported cases, fallback cases, alternatives, and one
minimal deterministic test asset. Keep this repository as the detailed
reproduction record.

### 2. Cycles core representation

After the design is accepted, add only CPU/device-agnostic scene data,
provenance normalization, explicit fallback state, and focused unit coverage.
Do not bring in the local environment switch, custom Blender candidate markers,
or Metal implementation in this change.

### 3. Metal implementation

Add the Metal AABB/intersection-function path after the core boundary is
accepted. Preserve ordinary mesh and existing-device behavior, and keep every
unsupported material or traversal combination on established fallback. Include
small image regressions plus focused device evidence without claiming general
performance from the Apple fixture.

### 4. Blender import and debug surface

Add the smallest `.tetcage` import/binding and fallback-inspection UI only when
the renderer representation is accepted. Keep it in a separate change from the
Metal backend so reviewer feedback on either surface stays tractable.

### 5. OptiX parity and performance decision

Use a qualified NVIDIA host to validate the same supported scenes through
OptiX. M9 must select the retained method using representative assets,
cross-platform image correctness, memory, and time-to-first-pixel/frame data
before performance-oriented integration is proposed upstream.

## Contribution checklist

Before a non-WIP Blender pull request:

1. Rebase the new contribution branch onto current Blender `main`.
2. Keep the pull request to one topic and explain the problem, solution,
   alternatives, limitations, tests, and user-visible behavior.
3. Use the `Cycles:` commit subject convention and Blender’s C/C++ formatting.
4. Add in-tree Cycles tests under `intern/cycles/test/` and minimal render
   fixtures under the documented Cycles render-test layout when image behavior
   is involved.
5. Run focused tests locally, request the documented buildbot build during
   review, and explicitly request relevant GPU/device coverage.
6. Verify that no experimental build output, dependencies, generated content,
   local paths, or unrelated cleanup is in the final diff.
7. Ensure a human author has reviewed and takes responsibility for every
   AI-assisted change, as required by Blender’s contribution policy.

The official-source details and links are collected in
[`docs/blender-contribution-research.md`](blender-contribution-research.md).

## Prospective pull-request summary

Use this as a starting point only after the preceding gates are satisfied:

> Cycles: Add tet-cage geometry support for the accepted supported-case set
>
> Problem: [state the renderer-visible limitation on representative assets].
>
> Solution: [state the approved scene representation and backend behavior].
>
> Alternatives: [ordinary mesh fallback, other retained methods, and why this
> change is justified].
>
> Limitations: [unsupported material, motion, volume, or device cases and their
> explicit fallback].
>
> Tests: [in-tree unit tests, render tests, host/device details, and relevant
> buildbot results].

Do not replace the bracketed items with claims based solely on the current WIP
series or synthetic fixtures.
