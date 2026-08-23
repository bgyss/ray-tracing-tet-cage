# Cycles tet-cage representation boundary

This is the narrow boundary for the pinned standalone Cycles revision
`97dbe6f57cdf4ede2d2b75ebdda507c8712edb7a`. It is a design map, not a device
patch: M9 must select the retained method before MetalRT or OptiX code changes.

The repository-side bridge contract is declared in
`include/tetcage/cycles_bridge.h`. It turns a compiled asset plus a posed cage
into deterministic per-tet transforms, conservative bounds, and a stable
micro-triangle index stream. Its hit-normalization function maps a Metal-style
`instance/tet/primitive/u/v` result back to the source primitive and Cycles'
ordinary triangle `u/v` coordinates; invalid topology or poses select the
conventional-mesh fallback.

The first live entry proof is recorded in
`results/integrations/2026-08-23-cycles-metal-entry.json`. It proves the
unmodified pinned Cycles MetalRT backend on the Apple M1 Max and the project's
standalone procedural AABB path using this contract. It does not yet claim
that Cycles itself loads `.tetcage` assets.

The isolated Cycles branch also contains a scene-side transport contract at
revision `5efddfd32` (based on `1059d3e590045c008cb69e8e82c2b97554278c77`). Its static experimental
adapter now reaches the Cycles Metal AABB BLAS and query path, but it is
intentionally not a `Geometry` subclass and does not yet carry source-owner
provenance through shading or motion/visibility semantics.

The same manifest includes an ordinary Cycles glass/caustics comparison. That
closes only the upstream MetalRT non-opaque baseline; the tet-cage path still
needs its own shadow, transparent, local-intersection, and mixed-scene tests.

`tetcage_cycles_xml` now provides the explicit conventional-geometry fallback:
it emits a small Cycles XML scene from the compiled micro-triangles, preserving
the ordinary renderer path while the procedural primitive is incomplete. The
fallback is intentionally labeled in the XML and is not evidence that the
tet-cage AABB primitive is active inside Cycles.

`scripts/blender_tetcage_import.py` provides the corresponding Blender-side
fallback/debug view: it parses the version-1 asset, creates an ordinary surface
mesh plus a wireframe cage, attaches source/tet face attributes, and selects
Cycles as the scene engine. Its contract test accepts an explicit
`BLENDER_BINARY` so it can use a release binary when the large pinned Debug
executable cannot start under current host memory pressure. The importer now
creates one immutable canonical mesh object per occupied tet and updates only
their affine object transforms when the cage changes. Mirrored or near-singular
poses are rejected with `invalid_pose` and leave the last valid transforms in
place; the CPU-disabled Metal render proof is recorded in the entry manifest.

| Boundary | Tet-cage side owns | Cycles side consumes or reconstructs |
| --- | --- | --- |
| Immutable payload | compiled clipped micro-triangles, source primitive ID, source triangle barycentrics, material/shader ID | one immutable geometry/BLAS payload per compatible source mesh |
| Per-frame payload | cage vertices, affine tet transforms, enabled/fallback state, stable owner-tet ID | ordinary object/instance transform and a backend instance record; no dense mesh regeneration |
| Intersection result | source primitive, source barycentrics, owning tet, object transform state | standard `ShaderData` object/primitive/UV fields before normal attribute and shader lookup |
| Fallback | unsupported, ill-conditioned, inverted, or capacity-rejected tet assets | ordinary Cycles mesh path with unchanged visibility, motion, light-linking, and shader semantics |

The shared kernel seam is deliberately limited to an internal hit-normalization
operation: device-specific MetalRT/OptiX code must turn a tet hit into the
original Cycles object index, primitive index, triangle barycentrics, and
primitive type before `triangle_shader_setup()` and normal attribute fetches.
It must not add tet-specific branches through SVM/OSL attribute evaluation.

## Pinned upstream landing points

- Metal: `src/device/metal/bvh.mm` builds mesh BLAS and later encodes TLAS
  descriptors. Its `userID` already carries a primitive offset; a retained tet
  path should preserve that convention and keep the standard mesh path intact.
- OptiX: `src/device/optix/device_impl.cpp` creates triangle/custom-primitive
  GAS inputs and a single instance TLAS. A tet experiment belongs beside those
  build-input conversions, not in generic scene synchronization.
- Kernel: `src/kernel/geom/triangle_intersect.h` populates `Intersection`
  object, primitive, type, and triangle coordinates, then
  `triangle_shader_setup()` loads the original shader, position, normal, and
  derivatives. The adapter's only shared output is this normalized source-hit
  identity.

The standalone tree is sufficient for these core/device seams. It cannot prove
Blender import, UI, or debug views; those stay scoped to a clean pinned Blender
source checkout. OptiX remains unimplemented and unmeasured until a qualified
NVIDIA host is available.
