# MetalRT tet-cage feasibility in Cycles on macOS

**Finding date:** 2026-08-22
**Decision:** technically feasible as a scoped Cycles Metal backend extension, but not
implemented or production-proven. The viable implementation is a *procedural
MetalRT* path: immutable compiled tet-cage micro-geometry/provenance plus
per-frame cage pose data, conservative AABB BLAS primitives, and Metal
intersection functions that perform the tet-cage narrow phase and reconstruct
the ordinary Cycles hit. It is not a zero-touch import of the existing tet-cage
Metal prototype, nor does it establish a general Cycles/Blender integration.

This report uses the pinned local Cycles and Blender checkouts, this project's
direct evidence, and Apple primary documentation only. Local source paths are
intentional reproducibility references for this development machine.

## Scope and evidence snapshot

| Item | Observation |
| --- | --- |
| Standalone Cycles source | `$CYCLES_SOURCE_ROOT` at `97dbe6f57cdf4ede2d2b75ebdda507c8712edb7a` (2026-07-13), clean when inspected. |
| Blender source | `$BLENDER_SOURCE_ROOT` at `4a09c19bea7bd2800d85f018280b2dc62e654e51` (2026-08-09), clean when inspected. Its embedded Cycles code has the same relevant Metal implementation. |
| Current development hardware | `system_profiler` identifies a 32-core Apple M1 Max with Metal support. This is host availability, not a claim that Cycles MetalRT or a tet-cage render was run today. |
| Current toolchain boundary | Xcode 26.6 is installed, but `xcrun metal -v` fails because the optional Metal Toolchain component is missing. A new shader-bearing Cycles build cannot be claimed until that component is installed and a clean build succeeds. |
| Existing project Metal evidence | The repository's clean M5 record, `results/metal/2026-07-29-m5-clean-rerun.json`, identifies Apple M1 Max and clean commit `32af558`; it is evidence for this project's standalone declared corpus, not evidence that Cycles renders tet-cages. |

Apple documents the needed primitives: acceleration structures can contain
triangles and bounding volumes; an application supplies either intersection
functions through a table or uses intersection queries. Apple also documents
that intersectors work in compute kernels on all GPUs, while render-shader
intersectors are limited to Apple silicon. Cycles uses the compute-kernel
route. [Apple: Ray tracing with acceleration structures](https://developer.apple.com/documentation/metal/ray-tracing-with-acceleration-structures)

## What already exists

### MetalRT in Cycles

The pinned source is not missing MetalRT. It discovers `MTLDevice` support at
runtime on macOS 14 or later and treats MetalRT as a default preference only on
Apple M3-and-newer architectures:

- `$CYCLES_SOURCE_ROOT/src/device/metal/device.mm:88-100`
- `$CYCLES_SOURCE_ROOT/src/device/metal/device_impl.mm:101-118`

The second condition is important: `use_hardware_raytracing` is based on
`device.supportsRaytracing`, whereas `use_metalrt_by_default` has the M3
threshold. A supported M1 Max is therefore a valid feasibility target only
after a runtime probe/build confirms the actual selected Cycles device and
configuration; it must not be inferred from its model name.

Cycles already creates the Metal resources needed for an AS-backed renderer:

- triangle and motion-triangle BLAS descriptors in
  `$CYCLES_SOURCE_ROOT/src/device/metal/bvh.mm:200-289`;
- instanced TLAS descriptors, including user IDs and optional motion
  transforms, in `bvh.mm:1182-1356`;
- compiled Metal intersection-function tables in
  `$CYCLES_SOURCE_ROOT/src/device/metal/kernel.mm:600-642`; and
- AS/table binding and explicit resource residency in
  `$CYCLES_SOURCE_ROOT/src/device/metal/queue.mm:499-540`.

The generated Metal kernel has separate table entries for normal, shadow,
shadow-all, volume, and local intersections. That is a useful seam but also a
completeness obligation: a tet-cage primitive must preserve the corresponding
visibility, self-intersection, shadow, local, and record-all semantics rather
than only implementing camera rays. The existing registrations are at
`kernel.mm:617-635`.

### A close procedural precedent

Cycles already uses bounding-box geometry plus custom Metal functions for
point clouds. The BLAS code builds `MTLAxisAlignedBoundingBox` buffers,
creates bounding-box descriptors, assigns intersection-function-table offset
2, and supports refit/compaction in
`$CYCLES_SOURCE_ROOT/src/device/metal/bvh.mm:740-925`. The shader
functions `__intersection__point`, `__intersection__point_shadow`, and
`__intersection__point_shadow_all` execute the Cycles point narrow phase after
Metal supplies an AABB candidate:
`$CYCLES_SOURCE_ROOT/src/kernel/device/metal/kernel.metal:568-743`.
The dispatch code then reconstructs the point hit after a bounding-box result:
`$CYCLES_SOURCE_ROOT/src/kernel/device/metal/bvh.h:245-278`.

That is direct evidence that the API and the existing backend can host a
procedural tet-cage narrow phase. It is **not** reusable unchanged: the point
path is hard-wired to `PRIMITIVE_POINT` and `point_intersect`, while the
currently enumerated primitive kinds are triangle, curve, point, volume, and
lamp (`$CYCLES_SOURCE_ROOT/src/kernel/types.h:691-715`). A tet-cage
implementation needs either a new primitive/geometry kind or a deliberately
isolated temporary adapter; it must not misrepresent cages as point clouds in
the shipped architecture.

### Tet-cage representation and a compatible mapping

The project already separates immutable micro-geometry/provenance from
per-frame cage transforms/instances and requires normalized source-hit
identity; see [`cycles-representation-boundary.md`](cycles-representation-boundary.md).
That split maps naturally to Cycles MetalRT:

| Tet-cage contract | Cycles/MetalRT mapping |
| --- | --- |
| Immutable compiled micro-geometry and provenance | A per-asset BLAS input buffer plus device-resident tet-cage/provenance buffers. Build once, or rebuild only when topology changes. |
| Posed cage vertices / per-tet affine transforms | Per-frame GPU buffer update and either conservative AABB BLAS refit or a TLAS update, according to a measured deformation policy. |
| Source primitive, owner, material/UV/normal reconstruction | `Intersection` fields plus a tet-cage-specific device lookup; the narrow phase must return the stable source owner and reconstruct Cycles attributes before shading. |
| Conservative broad phase with exact boundary policy | `MTLAccelerationStructureBoundingBoxGeometryDescriptor` and custom intersection functions, following the point-cloud precedent. |

The repository's standalone M5 work has already made the same high-level
choice—procedural AABBs with a project-specific narrow phase—to retain its
declared boundary/ownership policy. This supports the design direction, but it
does not demonstrate compatibility with Cycles' shading, motion, transparency,
or Blender sync contracts.

### Blender-facing gap

Blender currently converts a `blender::Mesh` into a Cycles `Mesh`, copies
positions, triangulates faces, and uploads ordinary triangle indices:
`$BLENDER_SOURCE_ROOT/intern/cycles/blender/mesh.cpp:614-726` and
`:929-1018`. Its object classifier recognizes mesh, curves, point cloud, and
volume data, not a tet-cage data model
(`$BLENDER_SOURCE_ROOT/intern/cycles/blender/object.cpp:64-100`).
Consequently, a user-visible Blender workflow requires an explicit ingest
contract (for example, a project-owned asset/cache attached to a mesh object),
dependency-graph invalidation, and an ordinary-mesh fallback. It cannot emerge
solely from a Metal BVH patch.

## Device and API constraints

| Constraint | Consequence for this work |
| --- | --- |
| Runtime capability is per device | Gate the path on macOS availability *and* `MTLDevice.supportsRaytracing`, exactly as Cycles does. Do not treat SDK headers, the M1 Max label, or a successful CPU render as capability proof. |
| Apple-family availability | Apple's May 2026 feature tables list M1 as Apple7 and list ray tracing in compute/render pipelines starting at Apple6; Apple9 adds per-component motion interpolation. Use an Apple7/M1 baseline for the procedural path; treat PCMI as an Apple9/macOS 15.6 optimization only. [Apple: Metal Feature Set Tables](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf) |
| OS/Xcode gate in Cycles | This checkout compiles the runtime support check only when the SDK exposes `MAC_OS_VERSION_14_0` and runs it under macOS 14 availability (`device.mm:90-100`). Its current shader compiler is blocked by the missing Xcode Metal Toolchain component. |
| AS hierarchy | The known Cycles construction is BLAS plus TLAS. The proposed mapping must flatten per-tet/asset records into that model unless a minimal real device build proves another hierarchy. |
| Standard limits | Cycles enables MetalRT extended limits past `2^28` primitives or `2^24` instances and requires rebuilding BVHs when the mode changes (`device_impl.mm:937-963`). A tet-cage path must measure real builds in both applicable modes; a descriptor-size query is not enough. |
| Visibility | The present TLAS truncates the MetalRT visibility mask to eight bits (`bvh.mm:1153-1159`). Preserve Cycles' established visibility behavior or add a reviewed mapping—do not silently widen the tet-cage semantic contract. |
| Intersection tables | Apple requires a table per pipeline; Cycles builds them per dispatch pipeline. New custom functions must be linked into every relevant table and bound through the existing ancillary-resource path. [Apple: `MTLIntersectionFunctionTable`](https://developer.apple.com/documentation/metal/mtlintersectionfunctiontable?changes=_3&language=objc) |

## Implementation plan and proof gates

The project roadmap deliberately places renderer integration after M9
method-selection work ([`open-roadmap-workplan.md`](open-roadmap-workplan.md)).
The following stages respect that boundary; stages 0–1 are design/probe work,
not authorization to claim the renderer path complete.

1. **Close the entry gate.** Pin these two source revisions and record the
   Xcode/SDK version; install the missing Metal Toolchain component; configure
   and build the existing Cycles Metal target; record `supportsRaytracing`,
   selected backend, GPU family, and the MetalRT on/off state. Stop if the
   device cannot compile and dispatch the unmodified MetalRT kernel.
2. **Define the Cycle-facing data contract.** Specify a dedicated
   `TetCageGeometry` (or an equally explicit internal abstraction), its
   `PrimitiveType`, device buffers, stable source-owner ID, material and
   attribute reconstruction rules, update tags, and fallback to ordinary
   `Mesh`. Do not overload `PointCloud` beyond a throwaway proof-of-concept.
   Add CPU/reference tests first for hit, miss, edge/vertex ownership,
   attributes, shadow, local, and motion behavior.
3. **Implement the minimal MetalRT procedural path.** Add a tet-cage BLAS
   builder beside `build_BLAS_pointcloud`, upload conservative AABBs and the
   immutable/provenance and posed-cage buffers, then add tet-specific
   intersection functions to `kernel.metal` and registrations to `kernel.mm`.
   Extend `kernel/device/metal/bvh.h` to turn a bounding-box result into a
   normal Cycles `Intersection`, including source identity and reconstructed
   barycentrics. Begin with static, opaque camera and shadow rays only, with a
   clearly labeled fallback for unsupported cases.
4. **Close semantic coverage.** Implement transparent/shadow-all, local,
   volume interaction where applicable, object/primitive offset handling,
   motion, transform, instancing, visibility, cancellation, and resource
   residency. Compare every declared ray and image against the portable
   oracle; retain minimized failures. Add refit-versus-rebuild rules for posed
   cages and prove that no per-frame dense mesh expansion is hidden in the
   update path.
5. **Add Blender ingestion and safe fallback.** Define how a Blender object
   supplies compiled tet-cage data, how cache/provenance is validated, and
   which dependency-graph changes invalidate BLAS/TLAS data. Make unsupported
   devices or assets use ordinary Cycles geometry with explicit diagnostics;
   test save/reload and render/background modes.
6. **Qualify release scope.** Run clean builds and CTest, focused Metal render
   comparisons, image regression tests, repeated build/refit/trace/memory
   measurements, standard and extended limit builds, and a separate Apple
   family/device matrix. A production decision also requires the roadmap's
   M9 crossover evidence; do not infer performance from the standalone M5
   corpus.

## Claim boundaries

- **Supported conclusion:** the pinned Cycles Metal backend and Apple's API
  expose a technically credible procedural-AABB/custom-intersection integration
  route for tet-cage rendering on a capable macOS Metal device.
- **Not established:** that a tet-cage primitive compiles in Cycles, that the
  current M1 Max selects/runs Cycles MetalRT, that the Blender bridge can load
  tet-cage assets, or that generated images/shading match the portable oracle.
- **Not established:** watertightness, stable ownership, transparent-shadow
  correctness, motion correctness, memory behavior, real AS limits, or
  performance on Cycles scenes. The repository's standalone Metal evidence is
  not a renderer benchmark or cross-device result.
- **Out of scope:** OptiX/NVIDIA implementation and any claim of an
  integration-ready cross-platform backend. Those need a qualified NVIDIA host
  and their own device evidence.

## Primary sources

- [Apple, Ray tracing with acceleration structures](https://developer.apple.com/documentation/metal/ray-tracing-with-acceleration-structures)
- [Apple, MTLIntersectionFunctionTable](https://developer.apple.com/documentation/metal/mtlintersectionfunctiontable?changes=_3&language=objc)
- [Apple, Metal Feature Set Tables, May 2026](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)
- Pinned Cycles source at `$CYCLES_SOURCE_ROOT`, revision `97dbe6f57cdf4ede2d2b75ebdda507c8712edb7a`; exact files and lines are cited above.
- Pinned Blender source at `$BLENDER_SOURCE_ROOT`, revision `4a09c19bea7bd2800d85f018280b2dc62e654e51`; exact files and lines are cited above.
