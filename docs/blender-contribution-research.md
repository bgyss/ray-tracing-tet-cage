# Preparing a contributor-ready Blender/Cycles patch

Research date: 2026-08-24. This note summarizes the official Blender guidance
that should shape an eventual tet-cage contribution. It is a submission-prep
contract, not a claim that the experimental adapter is ready for upstream.

## Upstream constraints that affect this work

- Discuss a significant feature with the relevant developers before investing in
  a large implementation. Blender asks contributors to establish that a feature
  is wanted and that its design aligns with existing plans; larger design
  questions should have a Design task before implementation review.
  [Contributing Code](https://developer.blender.org/docs/handbook/contributing/)
- A pull request needs a problem statement, proposed solution and rationale,
  alternatives and their drawbacks, limitations, and (where relevant) a UI
  mock-up/interaction description. These do not need to be long, but their
  ordering should lead the reviewer through the decision.
  [Ingredients of a Pull Request](https://developer.blender.org/docs/handbook/contributing/)
- Keep one topic per pull request. Separate functional changes from cleanup,
  reformatting, documentation-only work, and unrelated refactors; Blender's
  review playbook specifically calls this out as necessary for a tractable
  review.
  [Contributing Code](https://developer.blender.org/docs/handbook/contributing/),
  [Code Review Playbook](https://developer.blender.org/docs/handbook/contributing/review_playbook/)
- Blender requires the contributor agreement for contributors without direct
  commit rights. Contributions must be submitted for review and the relevant
  module owner is the usual final approver.
  [Contributing Code](https://developer.blender.org/docs/handbook/contributing/),
  [Modules](https://developer.blender.org/docs/handbook/organization/modules/)
- AI-assisted contributions have an additional explicit policy: a human must be
  the Git author (and any co-author), take responsibility for quality, license
  compliance and utility, understand and manually review every submitted
  change, and carefully test code. Significant AI-assisted changes should be
  discussed with the relevant maintainers first.
  [AI Contributions Policy](https://developer.blender.org/docs/handbook/contributing/ai_contributions/)

## Patch-shape recommendation

Do not attempt to upstream the current integration worktree, its build output,
or its research artifacts as one change. Preserve this repository as the
evidence and reproduction record, then produce a short series based directly
on a clean current `blender:main` checkout:

1. **Design and test fixture PR.** Establish the proposed Cycles geometry
   boundary, unsupported-case fallback rules, source-hit provenance contract,
   and minimal deterministic `.blend`/test assets. This is the right first
   reviewable unit because the current mixed-transparent-motion and
   large-footprint-area-volume cases are intentionally still fallback/open.
2. **Core Cycles representation PR.** Add only the CPU/device-agnostic scene
   data and hit-normalization pieces that have a stable design and tests. Avoid
   importing tet-cage compiler code, standalone-Cycles experiments, local
   probes, generated assets, or renderer-specific implementation at this
   stage.
3. **Metal backend PR.** Add Metal AABB/intersection-function work only after
   the core contract is accepted. It should preserve ordinary mesh behavior and
   keep unsupported material/traversal combinations on the established Cycles
   fallback. Include Metal-specific regression coverage and image comparisons.
4. **Blender/UI/debug PR, if still justified.** Add the smallest user-visible
   integration and debug inspection surface after the renderer representation
   is accepted. Keep UI work separate from the device implementation.

Each PR should be independently buildable, testable, bisectable, and safe if a
later PR is never merged. OptiX should be a separate future series after a
qualified NVIDIA validation environment exists.

## Clean extraction procedure

1. Start a new branch from a clean, current Blender `main` checkout; do not
   transplant the existing experimental branch wholesale.
2. Make one intentional change at a time, use Blender's in-tree formatting, and
   inspect `git diff --check`, `git diff --stat`, and the staged file list before
   every commit. Exclude build directories, app bundles, dependencies, cached
   test outputs, generated source, large binaries, and local research logs.
3. Recreate only the required source changes with `git format-patch` or
   `git diff` from that clean checkout. The resulting commits should touch only
   Blender-owned source, CMake/build metadata when necessary, and minimal test
   fixtures.
4. Write the PR description as the prospective final commit message plus a
   clearly separated testing/evidence section. The official workflow expects
   the PR text to be usable as the Git commit message.
   [Pull Requests](https://developer.blender.org/docs/handbook/contributing/pull_requests/)
5. Before submission, rebase onto current upstream `main`, rebuild, run focused
   tests and relevant manual scenes, then request a buildbot build through the
   reviewer workflow. The documented PR comment is `@blender-bot build`; GPU
   tests are not generally enabled by default, so request the appropriate
   coverage explicitly in the review.
   [Pull Requests](https://developer.blender.org/docs/handbook/contributing/pull_requests/),
   [Test Setup](https://developer.blender.org/docs/handbook/testing/setup/)

## Code, commits, and testing checklist

- Follow the surrounding code first; C/C++ uses two-space indentation and
  Blender's `clang-format` is required for C, C++, and GLSL. Document public
  interfaces at their declarations and explain non-obvious assumptions and
  design decisions in comments.
  [C/C++ Style Guide](https://developer.blender.org/docs/handbook/guidelines/c_cpp/)
- Use focused commits with an imperative, user-facing subject. Commit bodies are
  separated by a blank line, use American English, wrap at 72 columns, and use
  `Fix:`/`Fix #<task>:` only for actual bug fixes. Keep `Cleanup:` or
  `Refactor:` commits separate from functional behavior.
  [Commit Message Guidelines](https://developer.blender.org/docs/handbook/guidelines/commit_messages/)
- Test more than the local build: Blender states that code must build and tests
  must pass on all platforms before merge. Add focused automated tests where
  possible and small manual demo cases for paths that need visual/device
  validation. Buildbot catches cross-platform build and unit-test issues but
  does not replace the targeted tests.
  [Pull Requests](https://developer.blender.org/docs/handbook/contributing/pull_requests/),
  [Testing New Code & Refactors](https://developer.blender.org/docs/handbook/guidelines/testing_changes_refactors/)
- For a Cycles improvement, use the documented `Cycles:` commit subject form
  and state concrete measurements for any performance claim. Do not use the
  existing exploratory results as a generic speedup claim.
  [Commit Message Guidelines](https://developer.blender.org/docs/handbook/guidelines/commit_messages/)
- Prefer a minimal `.blend` render regression per behavior. Cycles render tests
  use frame 1 and live under `tests/files/render/<category>/`; update references
  with `BLENDER_TEST_UPDATE=1 ctest -R cycles`. Test CPU first and only add
  device baselines/tolerances after their cross-device variance is measured.
  [Render Tests](https://developer.blender.org/docs/handbook/testing/render/)
- Put focused Cycles unit tests alongside the Cycles source under
  `intern/cycles/test/`; keep the test asset and the source change together in
  the same upstream PR.
  [Cycles Source Layout](https://developer.blender.org/docs/features/cycles/source_layout/),
  [Test Setup](https://developer.blender.org/docs/handbook/testing/setup/)
- Where a C++ unit test is the appropriate regression, use Blender's GoogleTest
  setup (`WITH_GTESTS`) and the local naming/registration conventions rather
  than adding a project-specific test harness.
  [C/C++ Style Guide](https://developer.blender.org/docs/handbook/guidelines/c_cpp/),
  [GoogleTest](https://developer.blender.org/docs/handbook/testing/gtest/)
- Before a broad review, run the focused test set locally, then (where practical)
  a release build for performance/regression behavior and a debug+ASAN build
  for memory and threading issues. These are complementary checks, not a
  substitute for the buildbot's supported-platform coverage.
  [Testing New Code & Refactors](https://developer.blender.org/docs/handbook/guidelines/testing_changes_refactors/)

## Proposed reviewer packet for tet-cage work

For each eventual PR, prepare a compact packet in this repository, but submit
only the Blender patch and minimal fixtures upstream:

- exact upstream base SHA and `git format-patch` output;
- a one-page problem/design/alternatives/limitations statement;
- list of supported cases and explicit fallback cases;
- commands and machine/device details for focused verification;
- deterministic `.blend` test scenes and expected image metrics where useful;
- one short debug/UI capture only when it proves user-visible behavior; and
- a note identifying the appropriate Cycles and Metal module reviewers, sought
  before requesting a non-WIP review.

This keeps the experimental work auditable here while making an upstream review
small enough to understand, apply, test, and revise.
