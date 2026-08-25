# Blender/Cycles tet-cage WIP patch series

This directory preserves an applyable, source-only snapshot of the current
Blender/Cycles tet-cage experiment. It is a review baseline and reproduction
artifact, not a patch series to submit upstream as-is.

The patch series is based on Blender commit
`4a09c19bea7bd2800d85f018280b2dc62e654e51`. Applied in order, it reconstructs
the tree of the clean experimental candidate commit
`d92d347fdc754094e814e0f10fc314e82dc1d031`.

No build directory, app bundle, dependency checkout, cache, generated source,
binary fixture, or local result artifact is included. The patches touch only
the 21 `intern/cycles/...` source and build-metadata files recorded in the
candidate branch.

## Patch order

1. `0001-wip-cycles-tetcage-scene-contract.patch` adds the initial scene-side
   transport contract and Blender candidate bridge.
2. `0002-wip-cycles-metal-tetcage-static.patch` adds the static Metal AABB
   build, primary, shadow, local, and volume paths.
3. `0003-wip-cycles-metal-tetcage-motion-and-fallbacks.patch` adds motion
   handling and narrows unsupported behavior to ordinary Cycles fallback.

SHA-256 digests of the exported files:

```text
5e0394dd9d70a4eb484d15afef65db91d7caa8106216626df26aaf30a08a5ac9  0001-wip-cycles-tetcage-scene-contract.patch
3fed37790fa1c7d24b28aa66716214a80d552ea191cdcc98c3c0218cb039eb97  0002-wip-cycles-metal-tetcage-static.patch
580472d900f47c3242c43c4fa26f405c1ab96054a146818f61bef6b50f03e16b  0003-wip-cycles-metal-tetcage-motion-and-fallbacks.patch
```

## Applying the snapshot

Start from a clean checkout at the exact base commit. Set `PATCH_DIR` to this
directory in the tet-cage repository.

```sh
git checkout 4a09c19bea7bd2800d85f018280b2dc62e654e51
git status --short

while read -r patch; do
  git apply --check "$PATCH_DIR/$patch"
  git apply --index "$PATCH_DIR/$patch"
done < "$PATCH_DIR/series"

git diff --cached --stat
git write-tree
```

The final `git write-tree` value should be
`a1fd999e3410cddaefe5338d1846c083060312f6`. This replay was checked in a
temporary clean worktree on 2026-08-24 and produced the same tree as the
candidate branch.

The patches are plain Git diffs, so `git apply --index` stages each phase but
does not invent an upstream commit history. After the contribution cut has been
recreated and reviewed, make new focused commits on current `blender:main` and
export those with `git format-patch` if an email-style series is needed.

## Submission boundary

Do not send this WIP series to Blender unchanged. It deliberately retains local
prototype choices that need design review or replacement: the
`CYCLES_TETCAGE_NATIVE` environment switch, Blender custom-property candidate
markers, an identity/single-tetrahedron bridge, a `Mesh` adapter rather than a
settled geometry boundary, and incomplete device semantics.

The current semantic gates are intentional: mixed transparent/non-transparent
motion stays on ordinary Cycles fallback, and large-footprint area-light volume
precision remains open. See [the contribution cut plan](../../docs/blender-contribution-prep.md)
and [the official-guidance research note](../../docs/blender-contribution-research.md)
before turning this snapshot into a pull request.
