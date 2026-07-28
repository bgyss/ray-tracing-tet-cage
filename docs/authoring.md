# Cage authoring and animation suitability

The authoring tools deliberately separate three questions:

1. `tetcage_cage_generate` creates a deterministic starter cage.
2. `tetcage_cage_quality` checks posed tetrahedron conditioning and the
   affine/barycentric identity.
3. `tetcage_cage_animation` measures a clip-wide surface residual and reports
   whether the selected thresholds are met.

The animation evaluator consumes the compiled asset, samples a deterministic
non-affine procedural clip, and compares dense source positions and normals
with cage-driven reconstruction over every generated surface vertex. It also
fits non-negative, sum-to-one cage weights with a constrained least-squares
solve and reports the fitted residual as an authoring signal. The procedural
clip is a reproducible diagnostic, not production animation evidence; a real
asset clip must be supplied before an M10 suitability claim is promoted.

Example:

```sh
cmake --build --preset dev --target tetcage_cage_animation
./build/dev/tetcage_cage_animation \
  build/dev/one-tet.tetcage results/authoring/animation-study.json \
  --samples 32 --motion 0.05 --position-threshold 0.001 \
  --normal-threshold 0.01
jq . results/authoring/animation-study.json
```

`fallback_reasons` is actionable: position or normal thresholds identify a
clip that should use a denser cage, ordinary skinning, or a hybrid region.
`optimized_weight_samples` and `maximum_weight_delta` make the fitted-weight
experiment auditable rather than implying that a hidden optimizer changed the
asset.
`optimized_maximum_position_error` and `optimized_rms_position_error` show
whether weight fitting improves the same clip; they do not override a failed
base-cage threshold. Conforming refinement remains available through
`tetcage_cage_refine`. `tetcage_cage_lods` builds a deterministic level set and
emits one parent-tetrahedron map per refined level:

```sh
./build/dev/tetcage_cage_lods tests/assets/one-tet.cage build/dev/lod-cage \
  results/authoring/lods.json --levels 2
```

The manifest uses whole-cage level switching and explicitly does not claim
visual cross-fade, popping, or TLAS transition performance. Those checks still
require representative production clips.
