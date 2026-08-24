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
revision `643723d1ee1ef43beb0cba4650ef561a8e2c0bf9` (based on
`1059d3e590045c008cb69e8e82c2b97554278c77`). Its
experimental `Mesh` adapter reaches the Cycles Metal AABB BLAS and static query
path, applies object visibility filtering, packs source-owner IDs for ShaderData,
and includes self-filtered opaque-shadow, static local projected-axis triangle-scan, and native
AABB volume-table seams.
Its motion-tagged MetalRT callback now carries accepted tet barycentrics through
the ray payload, and the motion narrow phase distinguishes object-transform
motion (static tet vertices plus TLAS transforms) from true geometry motion
(time-interpolated vertices). One- and two-object rigid-motion fixtures and a
native motion-AABB/time-interpolated shape-key fixture pass; broader deformation
semantics remain open. It remains an adapter rather than
a production tet-cage Geometry type; transparent/shadow traversal and the
zero-hit local-ray path are measured, while SSS multi-hit ordering, exact
volume precision, record-all, and full shading semantics remain open.

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
their affine object transforms when the cage changes. It records the
`tetcage_native_candidate`/`cycles_tetcage_v1` marker for the future native
Blender sync. The isolated Blender branch at `430b6987380` now recognizes that
marker in embedded Cycles and, with `CYCLES_TETCAGE_NATIVE=1`, activates a
static `Mesh` adapter, builds a Metal AABB BLAS, and routes triangle/AABB
intersection queries while preserving ordinary mesh fallback by default. Static and
object-transform-motion volume probes use the native AABB volume table. A static local AABB
`ift_local` callback is compiled for tet objects, and the Metal shadow callback helper is compiled; the
transparent and AO local probes are pixel-identical, while static SSS multi-hit now uses the native
AABB local table and the single/two-tet volume probes are numerically close but not exact under area sampling. Mirrored or near-singular
poses are rejected with `invalid_pose` and leave the last valid transforms in
place; the CPU-disabled Metal render proof is recorded in the entry manifest.
Its full Blender target, app-bundle install, direct importer probe, Cycles/UI
smoke, and tet-only, mixed, object-transform-motion, plus one native shape-key
deformation ordinary-triangle/native-AABB emission differentials pass in the
isolated build; broader deformation topology, SSS multi-hit/local ordering, exact
volume precision, Principled shading, and broader traversal semantics remain open. Mixed
transparent-closure motion explicitly retains ordinary Cycles fallback until its closure semantics
are qualified; pure-transparent motion uses the native path; animated
subsurface/BSSRDF motion uses the motion-aware native local narrow phase on the
declared closed-tetrahedron fixture (point-light numeric-close; area-light
sampling remains open). The open single-triangle SSS diagnostic is not promoted.
The older release-binary Python child-launch
path remains a separate host-startup limitation.

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

The standalone tree is sufficient for these core/device seams. The Blender
candidate branch proves C++ ingestion recognition and build compatibility, but
not native tet-cage Metal dispatch or UI/debug rendering. OptiX remains
unimplemented and unmeasured until a qualified NVIDIA host is available.
