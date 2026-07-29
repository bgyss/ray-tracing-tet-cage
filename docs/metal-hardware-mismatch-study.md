# Metal hardware mismatch and CPU-oracle elimination study

## Outcome

The retained Metal implementation no longer needs the CPU oracle to produce a
final hit stream. A CPU-audited run of the same kernel reports zero hardware
mismatches over all 1,408 declared correctness rays, and `--gpu-only` runs the
same four fixtures with:

- no CPU BVH construction;
- no `trace_fast` or exact-oracle invocation;
- no boundary or mismatch-based CPU selection;
- zero CPU fallback hits; and
- 1,408 complete final records sourced from Metal.

The paired modes are intentional. The audited mode proves the kernel against
the portable contract. GPU-only mode proves that production execution does not
depend on that oracle; oracle-derived error fields are `null` in its manifest
rather than being represented as measured.

The earlier direct worktree result is retained in
`results/metal/2026-07-29-procedural-gpu-only-study.json`. It must be rerun from
a clean commit before it can replace the checked-in release evidence. That
clean rerun is now recorded in
`results/metal/2026-07-29-m5-clean-rerun.json`.

## Why the original path had 672 mismatches

The original fixed-function triangle path returned one closest intersection.
The minimized 2026-07-28 corpus showed that the 672 differences were four
related boundary and precision problems:

| Classification | Count | Root cause |
| --- | ---: | --- |
| Shared-edge ownership disagreement | 408 | Metal selected one of several equal-distance triangle hits before the application could apply the stable source/owner rule. |
| Floating-point tolerance policy error | 133 | The reference geometry and rays are binary64, while the uploaded Metal triangle geometry and ray were binary32. Near-coplanar and near-edge predicates therefore changed sign or ordering. |
| Metal intersection acceptance rule | 127 | Some exact edge and vertex hits accepted by the portable projected-edge test were not returned by fixed-function triangle traversal. |
| Generated-triangle overlap or gap | 4 | A small residual set crossed generated micro-triangle boundaries differently. |

The old fallback count was 1,099 rather than 672 because it classified every
CPU-observed boundary as uncertain and routed it to the CPU even when Metal had
already returned the expected hit. The CPU therefore both validated every ray
and supplied most final hits.

## What the intersection-query experiment established

An intermediate implementation changed to an `intersection_query`, enumerated
all triangle candidates that Metal reported, and applied stable ownership in
the shader. That reduced the result from 672 to 544 mismatches and the CPU
final-hit count from 1,099 to 544.

This isolated the remaining limitation: query enumeration can resolve among
reported candidates, but it cannot recover a triangle that fixed-function
triangle acceptance rejected. Apple's Metal material describes intersection
queries as a way to inspect and accept candidates, while its custom-primitive
path uses bounding boxes plus application-defined intersection logic. See
[Control the ray tracing process using intersection queries](https://developer.apple.com/documentation/Metal/control-the-ray-tracing-process-using-intersection-queries),
[Ray tracing with acceleration structures](https://developer.apple.com/documentation/metal/ray-tracing-with-acceleration-structures?language=objc),
and [Your guide to Metal ray tracing](https://developer.apple.com/videos/play/wwdc2023/10128/).

A float-only procedural prototype was rejected: it returned to 672 total
mismatches because manual intersection alone did not restore the input
precision lost by binary32 geometry and rays.

## Retained Metal design

The retained path uses procedural AABB primitives for broad-phase traversal and
implements the complete narrow phase inside the Metal intersection-query
kernel:

1. Each generated micro-triangle contributes a conservative AABB to its
   immutable per-tet BLAS.
2. Canonical vertices, per-instance affine transforms, ray origins,
   directions, and distance bounds are uploaded as three-component float
   expansions (high, middle, and low components).
3. The shader reconstructs world-space vertices with compensated expansion
   arithmetic.
4. It evaluates a projected-edge ray/triangle predicate, including the same
   scale/conditioning tolerance policy as the portable reference.
5. It considers every accepted procedural candidate in a near-equal distance
   bucket and selects the stable tuple of source primitive, owner BLAS,
   provenance index, and instance.
6. It reconstructs source barycentrics, material, normal, and UV in the same
   kernel.

Metal remains responsible for AABB acceleration, candidate enumeration,
intersection acceptance, ownership, distance, and attributes. No
intersection-function table is required: the query enumerates bounding-box
candidates and the shader retains the best candidate accepted by its manual
predicate.

The projected-edge construction follows the same family of watertight
techniques described by Woop, Benthin, and Wald in
[Watertight Ray/Triangle Intersection](https://jcgt.org/published/0002/01/05/).
The compensated representation is a bounded GPU technique, not a claim of
arbitrary exact arithmetic; Shewchuk's
[Adaptive Precision Floating-Point Arithmetic and Fast Robust Predicates](https://people.eecs.berkeley.edu/~jrs/papers/robustr.pdf)
is the relevant reference for why predicate sign and representation precision
must be treated together.

## CPU ownership-contract correction

The three-component prototype exposed a defect in the portable owner resolver.
The documented contract says near-equal distances form one boundary bucket and
the lowest stable source owner wins. The CPU implementation instead sorted by
raw binary64 `t` first and only used the tolerance while deduplicating hits from
the same source.

For two smooth-corpus rays, the higher-precision Metal expansion found that a
lower source primitive was closer by roughly `1.6e-17`, while binary64 rounded
the candidates differently and returned source primitive 3. Both distances
were far inside the declared `2e-10 * max(1, |t|)` ownership bucket.

`resolve_hits` now builds that closest-distance bucket first and selects the
stable `(source_primitive, tet_id, micro_triangle, t)` tuple. A focused
portable regression fixes the literal smooth-corpus ray that previously
returned primitive 3 instead of primitive 0. The Metal kernel applies the same
bucket before its stable owner comparison.

## M1 Max evidence

The final CPU-audited matrix used GPU-generated instance descriptors and the
procedural expansion kernel:

| Corpus | Rays | Hardware mismatches | Misses | Wrong ownership | CPU final hits | Metal final hits |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `smooth_closed` | 128 | 0 | 0 | 0 | 0 | 128 |
| `sharp_material_uv_seam` | 128 | 0 | 0 | 0 | 0 | 128 |
| `face_edge_vertex_sharing` | 128 | 0 | 0 | 0 | 0 | 128 |
| `dense_animated_embedded_surface` | 1,024 | 0 | 0 | 0 | 0 | 1,024 |
| **Total** | **1,408** | **0** | **0** | **0** | **0** | **1,408** |

Maximum observed errors remain below the declared `2.5e-5` policy:

- position: `1.1710072689297135e-8`;
- normal: `3.12721421869e-7`; and
- reconstructed attributes: `9.404971856863398e-8`.

The paired GPU-only matrix also completed 1,408/1,408 final records. Each
manifest reports `cpu_oracle_constructed: false`, `cpu_oracle_rays: 0`,
`cpu_validation_ms: 0`, `fallback_decision_oracle_ms: 0`, and
`cpu_fallback_rays: 0`. Its mismatch and error fields are `null` because those
measurements require an oracle.

## Remaining limits

This closes the declared corpus defect, not every Metal acceptance question:

- the evidence is from an Apple M1 Max and an uncommitted worktree;
- the four fixtures are deterministic project assets, not a production-scale
  content library;
- the procedural compensated kernel trades fixed-function triangle throughput
  for a software narrow phase, so repeated scale/performance comparison remains
  part of method selection;
- GPU attribute recovery is fused with traversal, so it has no separate timing;
  and
- the CPU still prepares cage poses, precise instance transforms, and the test
  rays. `--gpu-only` means no CPU intersection oracle or final-hit fallback; it
  does not claim an entirely CPU-free renderer.

M5 is closed for the declared Apple M1 Max/corpus gate at clean commit
`32af558`. Broader production content and cross-platform performance acceptance
remain outside this milestone.
