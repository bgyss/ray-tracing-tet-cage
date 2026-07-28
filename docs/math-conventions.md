# Mathematical and numeric conventions

The shared implementation stores every `Mat3` by mathematical columns. For a
rest tetrahedron `(v0, v1, v2, v3)`, the edge matrix is exactly:

```text
M = [v1 - v0 | v2 - v0 | v3 - v0].
```

Given the independent canonical coordinate `q = (b1, b2, b3)`, the fourth
barycentric coordinate is `b0 = 1 - b1 - b2 - b3`. Reconstruction is:

```text
p = v0 + M q
  = b0 v0 + b1 v1 + b2 v2 + b3 v3.
```

For a posed tetrahedron `(a0, a1, a2, a3)`, canonical micro-vertices are
deformed by:

```text
A = affine([a1-a0 | a2-a0 | a3-a0], a0).
```

This is algebraically identical to direct per-vertex cage interpolation. Tests
compare the two independently across all 24 vertex orders and 2,000 random
poses using seed `0x5eedc0de`.

Normals are never multiplied by the position matrix directly. They use the
normalized inverse transpose of the full linear transform. Singular matrices
return no transform; mirrored matrices are classified explicitly so a backend
can flip winding or disable culling.

## Tolerances

- Matrix inversion rejects `|det(M)| <= 1e-12 * max(1, ||M||∞)^3` in public
  tetrahedron operations. This scale-aware threshold prevents an absolute
  determinant test from accepting large ill-conditioned matrices or rejecting
  small well-shaped ones.
- Analytical affine and barycentric tests use `1e-11` absolute error; seeded
  large-world differential tests use `2e-12 * max(1, |p|)`.
- Feature snapping defaults to `1e-12` in barycentric space. It changes exact
  zero/one feature coordinates, not arbitrary positions.
- The fast clipping expansion is versioned separately and defaults to zero.
  `2.5e-6` is supported as the paper-derived regression seed, not a universal
  production value.
- Generated fragments with twice-area below
  `1e-14 * max(1, coordinate_scale)^2` are diagnosed as degenerate and excluded.

Future GPU conformance tests must use the same mathematical convention but may
declare looser FP32 tolerances with evidence. Copying raw matrix memory between
this column convention and Metal/Vulkan descriptors is prohibited without a
conformance vector.

