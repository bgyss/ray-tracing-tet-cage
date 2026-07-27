# Paper analysis: ray tracing massive animated geometry

Research snapshot: 2026-07-27

## Executive assessment

The method is implementable on current Metal and Vulkan ray-tracing APIs
without native tetrahedron intersection hardware. Its fast form maps each
tetrahedron to one top-level acceleration-structure instance and each clipped
static micro-mesh to a reusable bottom-level triangle acceleration structure.
The per-frame instance transform maps the rest-pose micro-mesh into the
deformed tetrahedron.

The method is most promising when all of the following are true:

- The source mesh is very dense.
- Triangle connectivity is fixed.
- Many instances share one rest mesh but have unique smooth deformations.
- A coarse piecewise-affine deformation is visually acceptable.
- Dense vertex animation, unique BLAS storage, or BLAS updates are the current
  bottleneck.
- The platform can build or update a top-level structure containing the
  resulting number of tetrahedra within the frame budget.

It is not a universal replacement for skinning or ordinary instancing. The
paper exchanges dense geometry update work for:

- offline clipping and a 1.3x to 2.3x triangle expansion;
- one static micro-BLAS per occupied tetrahedron;
- one top-level instance per animated tetrahedron;
- slightly higher traversal cost;
- approximation error controlled by cage resolution; and
- either a small non-watertightness risk or an expensive software traversal.

The paper's combined benchmark makes the main optimization target explicit:
animation plus the top-level rebuild consumes 10.01 ms of a 12.43 ms frame,
and the acceleration-structure update portion alone is 9.66 ms. A successful
reimplementation therefore needs a measured crossover model. It must compare
the cage path against ordinary animated triangles, not merely demonstrate that
the cage path renders.

## Paper identity and evidence status

The supplied 18-page PDF is the July 2026 paper
[DOI 10.1145/3820014](https://doi.org/10.1145/3820014). It states a Creative
Commons Attribution 4.0 license for the paper. That license does not supply or
license an implementation.

No public source implementation or supplemental code was found in title,
author, GitHub, AMD, and GPUOpen searches performed for this review. Treat the
project as an independent implementation from the published description.
That search is not a legal freedom-to-operate opinion, and it should be
repeated before a commercial release.

## Terminology

| Term | Meaning in this packet |
| --- | --- |
| `tetMesh` | Rest or animated tetrahedral cage. Cage vertices are shared by adjacent tetrahedra. |
| `muMesh` | The portion of the source triangle mesh clipped to one tetrahedron. |
| `muBLAS` | Static triangle acceleration structure built over one `muMesh`. |
| `tetAnimCopy` | Per-animation copy of cage state. It does not duplicate the dense mesh or `muBLAS` objects. |
| `tetLAS` | Top-level acceleration structure containing one instance per animated tetrahedron. |
| Fast variant | Hardware triangle traversal through an affine instance transform. It uses overlap to mitigate, but not prove, watertightness. |
| Watertight variant | A custom traversal that stores vertices and BVH bounds in four-component tetrahedral barycentric coordinates and reconstructs them in world space. |
| Ground truth | The dense mesh after its original vertex animation or skinning, built and traced normally. |

`mu` is written as `mu` in identifiers so source files and tools do not depend
on Unicode names.

## Algorithm reconstructed from the paper

### Offline preprocessing

For each rest-pose mesh:

1. Fit a regular voxel grid to the mesh.
2. Mark voxels intersected by source triangles and discard empty voxels.
3. Split each retained voxel into six conforming tetrahedra.
4. Discard tetrahedra that intersect no source triangle.
5. For every remaining tetrahedron, clip every overlapping source triangle
   against its four face half-spaces.
6. Triangulate the resulting convex polygon.
7. Deduplicate vertices, including vertices introduced on faces shared by
   adjacent tetrahedra.
8. Preserve source primitive, material, and interpolated attribute provenance.
9. Build one immutable triangle `muBLAS` for each nonempty `muMesh`.
10. Optionally compact each static `muBLAS`.
11. Store cage adjacency, rest transforms, animation bindings, and mappings
    from each top-level instance to its `muBLAS`.

Clipping is essential. A source triangle that straddles two tetrahedra cannot
simply be referenced by both because the two tetrahedra induce different
affine deformations. Each clipped fragment must lie inside its owning
tetrahedron.

The paper uses a regular grid and six tetrahedra per voxel. A reimplementation
should use a globally consistent Freudenthal-style split, including across
voxel boundaries. Choosing the six tetrahedra independently per voxel can
create nonconforming shared faces.

### Per-frame update

For every unique animated copy:

1. Deform only the cage vertices, using a procedural deformation or cage
   skinning.
2. Build the affine transform for every animated tetrahedron.
3. Write one hardware instance descriptor per tetrahedron. All animated copies
   of a base mesh reference the same immutable `muBLAS` set.
4. Rebuild or update the `tetLAS`.
5. Trace rays through the `tetLAS`; hardware transforms each candidate ray into
   the local space of the referenced `muBLAS`.

For bone animation, the paper assigns each cage vertex the bone IDs and weights
of the closest original mesh vertex. For clipped mesh attributes it unions up
to 12 bone IDs across a source triangle, clips the associated scalar weights,
keeps the largest four weights after clipping, and renormalizes them. The paper
uses rest normals plus the deformation at shading time rather than storing a
fully animated dense normal buffer.

## Affine mapping

Let a nondegenerate rest tetrahedron have vertices
`v0`, `v1`, `v2`, and `v3`. Define:

```text
M = [ v1 - v0 | v2 - v0 | v3 - v0 ]
```

For any rest-space point `p` in the tetrahedron:

```text
q = inverse(M) * (p - v0)
p = v0 + M * q
```

`q` contains the first three independent tetrahedral coordinates; the fourth
barycentric coordinate is `1 - q.x - q.y - q.z`.

Let the animated tetrahedron vertices be `a0`, `a1`, `a2`, and `a3`:

```text
A3 = [ a1 - a0 | a2 - a0 | a3 - a0 ]
A  = affine(A3, a0)
```

If the `muBLAS` stores `(p - v0)` in the original rest basis, its object-to-world
transform is:

```text
T = O * A * affine(inverse(M), 0)
```

`O` is an optional object-to-world transform. This is the paper's instance
mapping. The API or shader may ask for object-to-world or world-to-object; the
implementation must name both explicitly and test the convention rather than
copying matrix memory blindly.

The local deformation gradient is:

```text
F = A3 * inverse(M)
```

A rest-space shading normal `n` transforms as:

```text
n_deformed = normalize(transpose(inverse(F)) * n)
```

The object-to-world normal transform must then also be applied. Negative
determinants require a consistent winding/culling policy.

### Recommended canonical-space optimization

Pretransform every `muMesh` vertex to `q` during preprocessing and build the
`muBLAS` in the canonical unit tetrahedron. The runtime instance transform then
becomes `O * A`; it no longer multiplies `inverse(M)` per tetrahedron per frame.

This is algebraically equivalent for positions and makes every local vertex
lie in a compact `[0, 1]` barycentric domain. It also makes quantization
experiments easier. It changes how stored normals and tangents are interpreted,
so the baseline should retain rest attributes and apply `F^-T` at shading until
a canonical dual-space encoding is separately validated.

## Fast instance-transform variant

The real-time variant uses ordinary hardware triangle BLASes:

- one static `muBLAS` per rest-pose tetrahedron;
- one top-level instance per animated tetrahedron;
- a per-frame affine transform per instance; and
- hardware ray/triangle intersection after the API transforms the ray.

Adjacent tetrahedra transform the same world-space ray differently. Floating
point error can therefore make their shared clipped edge disagree. The paper
mitigates this by moving all four tetrahedron planes outward before clipping,
creating a small overlap. It reports an epsilon of `2.5e-6` for its scenes. In
an isolated sphere test (radius 100, about one million original triangles,
`9 x 12 x 9` voxel cage), about 0.01% of rays escaped without the mitigation
and none escaped with it.

That experiment does not prove watertightness:

- the ray count and sampling pattern are not reported;
- epsilon is user-defined and scene-dependent;
- overlapping fragments can produce duplicate hits;
- any-hit, alpha-tested, and transmissive materials are more sensitive to
  duplicates than opaque closest-hit shading; and
- extreme or ill-conditioned deformations are not characterized.

The implementation should define epsilon relative to local coordinate scale,
floating-point ULPs, and tetrahedron condition number. The paper's absolute
number is a regression seed, not a universal default.

## Watertight four-dimensional variant

The reference variant stores every clipped vertex as four tetrahedral
barycentric coordinates:

```text
p = b0*v0 + b1*v1 + b2*v2 + b3*v3
b0 + b1 + b2 + b3 = 1
```

The preprocessing rules are:

- Give all cage vertices stable global IDs.
- Sort the four vertices of every tetrahedron by those IDs before evaluating
  barycentric reconstruction.
- Globally deduplicate clipped vertices.
- Snap a point close to a cage vertex, edge, or face onto that feature.
- Set coordinates known to be zero or one exactly for snapped features.
- Evaluate shared points in the same vertex order.

This provides the same reconstructed world-space point on either side of a
shared cage face. A binary 4D BVH stores min/max intervals for all four
barycentric coordinates. During traversal, the current animated tetrahedron
projects each 4D node bound to a conservative 3D world-space AABB; leaf
vertices are reconstructed in world space and intersected with the unchanged
world-space ray.

The paper's appendix proves the conservative bound by interval arithmetic:
for each world axis, sum the per-coordinate minimum and maximum of
`b_i * animated_vertex_i`. This remains conservative under inversion,
mirroring, and collapse.

The result is a valuable correctness oracle but not a real-time baseline. The
paper reports 2.3x to 3.2x the memory and 19x to 80x the render time of the fast
variant because BVH traversal and triangle intersection run in software.

### Tighter exact bound opportunity

The paper's interval sum ignores the constraint `sum(b_i) = 1`, so its 3D
bounds can be loose. For each world axis, the exact minimum over:

```text
b_i_min <= b_i <= b_i_max
sum(b_i) = 1
```

is a four-variable bounded linear program. It can be solved without a general
solver: start at every lower bound, then distribute the remaining barycentric
mass to coordinates in ascending coefficient order up to their upper bounds.
Use descending order for the maximum. Sorting four values or using a small
sorting network is cheap. This should be implemented as an optional watertight
BVH experiment and compared against the paper's conservative min-sum bound.

## Hit and attribute provenance

Every generated triangle needs:

- stable base asset ID;
- original triangle ID;
- material/geometry segment ID;
- owning tetrahedron ID;
- three source-barycentric coordinates, one for each generated vertex; and
- a deterministic subtriangle or ownership ID.

At a ray hit, combine the hardware barycentrics of the clipped triangle with
the stored source barycentrics to recover barycentrics on the original
triangle. Fetch UVs, rest normals, tangents, colors, and other attributes from
the original mesh. This reduces duplicated attribute storage and makes the
fast and watertight variants shade the same logical primitive.

Overlapping epsilon geometry and fragments on shared cage faces can report the
same logical source primitive more than once. Closest-hit selection needs a
deterministic tie-break, and any-hit behavior needs a defined edge ownership
rule. The paper identifies unique shared-edge ownership as future mitigation
but does not specify it.

## Degenerate and inverted tetrahedra

Rest tetrahedra must be nondegenerate because `inverse(M)` is precomputed.
The fast runtime transform must also remain invertible. Monitor:

- signed determinant;
- determinant sign changes;
- a condition-number estimate;
- minimum altitude or volume;
- NaN and infinity generation; and
- transformed bounds.

A production policy must choose among:

- reject the animation during asset validation;
- locally refine the cage;
- clamp or regularize cage motion;
- disable culling and flip winding for a valid mirrored transform;
- route an affected tetrahedron to a software/watertight fallback; or
- use the dense ground-truth path for the affected object.

Silently submitting a singular instance transform is not acceptable.

## Reported results

All numbers below are claims from the supplied paper, measured on an AMD
Radeon RX 9070 XT with 16 GB on Windows 11 using DirectX 12 and HLSL. Rendering
used one primary and one shadow ray per pixel.

### Base assets and cage sizes

| Asset | Original triangles | Example tetrahedron counts | Average clipped triangles per tetrahedron |
| --- | ---: | ---: | ---: |
| Tree | 1.58 million | 0.64k or 2.32k | 3.3k or 1.1k |
| Grass patch | 120k | 0.66k or 2.42k | 0.3k or 0.1k |
| Frog | 408k | 1.91k or 5.55k | 0.3k or 0.1k |

Clipping increases triangles by 1.3x to 2.3x and vertices by 1.4x to 3.7x in
the tested assets. The distribution is highly uneven: individual
tetrahedra range from one triangle to thousands. The paper does not enforce a
triangle budget per tetrahedron.

### Fast versus watertight

| Scene | Fast memory | Watertight memory | Fast render | Watertight render |
| --- | ---: | ---: | ---: | ---: |
| 25 trees | 209.7 MB | 540.5 MB | 2.96 ms | 203 ms |
| 500 grass patches | 145.6 MB | 459.4 MB | 1.35 ms | 108 ms |
| 81 frogs | 190 MB | 442 MB | 1 ms | 19 ms |

### Combined scene

| Animated triangles | Tetrahedra | Cage animation | TLAS update/rebuild | Render | Total | GPU memory |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 584 million | 2.8 million | 0.35 ms | 9.66 ms | 2.42 ms | 12.43 ms | 770.10 MB |

The figure caption rounds this to 585 million triangles and about 60 frames per
second at 1080p. The ordinary dense-animation comparison did not fit in the
16 GB GPU memory. For 500 grass patches, the paper reports up to 9x lower total
time and 16.1x lower memory than the ordinary per-copy animated BLAS path.

These results establish plausibility, not a portable target. Driver AS layout,
GPU architecture, object distribution, ray coherence, materials, cage error,
and top-level build algorithms can all change the crossover.

## What the paper leaves unspecified

The first implementation milestones must resolve and document these gaps:

1. Exact voxel bounds, padding, occupancy test, and six-tet indexing pattern.
2. Whether clipping uses original or epsilon-expanded face planes at every
   stage and how epsilon scales per asset.
3. Numerical precision and predicates used during clipping.
4. Global vertex deduplication key, tolerance, and deterministic ordering.
5. Coplanar triangle, zero-area fragment, and face/edge ownership rules.
6. Polygon triangulation rule and winding preservation.
7. Exact `muBLAS` vertex coordinate convention.
8. Material segmentation and alpha/any-hit behavior.
9. Stable mapping from clipped fragments back to original attributes.
10. Treatment of mirrored, inverted, near-singular, or collapsed animated
    tetrahedra.
11. Whether the top-level structure is rebuilt or refit for each deformation
    regime, beyond the paper's stated rebuild benchmark.
12. Build batching, `muBLAS` object overhead, and static compaction details.
13. Cage LOD transition policy and temporal stability.
14. Objective deformation-error thresholds; "no visible difference" is the
    paper's camera-path observation, not a reproducible metric.
15. Motion blur behavior.
16. How duplicate hits are resolved for transparency and shadow rays.
17. Asset availability and exact animation inputs used for the published
    benchmark.

## Required reproduction oracles

Before GPU optimization, create deterministic tests for:

- six-tetrahedra voxel conformity and adjacency;
- triangle/tetrahedron overlap;
- clipping containment and area conservation;
- source-barycentric attribute reconstruction;
- shared vertex bit identity in the watertight encoding;
- affine forward/inverse mapping;
- normal transformation;
- negative determinant and singularity policy;
- closed-surface escape rays;
- shared-edge and shared-vertex adversarial rays;
- closest-hit agreement against a dense CPU tracer;
- any-hit agreement with alpha masks;
- shadow/transmission duplicate-hit behavior; and
- deformation error against dense animation over a sequence.

Property-based generation should cover scales from very small to very large,
nearly coplanar triangles, thin slivers, rays at grazing angles, and transforms
with poor condition numbers.

## Applicability decision

Use the cage path only when a measured cost function favors it. A first useful
model is:

```text
ordinary_cost =
    dense_vertex_animation
  + unique_dense_geometry_memory
  + BLAS_update_or_rebuild
  + ordinary_trace

cage_cost =
    cage_animation
  + instance_descriptor_generation
  + tetLAS_build_or_update
  + extra_trace
  + approximation_penalty
```

The approximation penalty is not just image error. It includes authoring,
preprocessing, cage storage, failure fallbacks, and the operational cost of
supporting two geometry paths.

## Primary references

- Gruen et al.,
  ["Ray Tracing Massive Amounts of Animated Geometry"](https://doi.org/10.1145/3820014),
  July 2026.
- Khronos,
  [Vulkan acceleration structures specification](https://docs.vulkan.org/spec/latest/chapters/accelstructures.html).
- Apple,
  [Ray tracing with acceleration structures](https://developer.apple.com/documentation/metal/ray-tracing-with-acceleration-structures).

