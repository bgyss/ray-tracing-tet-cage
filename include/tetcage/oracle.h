#pragma once

#include "tetcage/asset_format.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace tetcage {

struct BarycentricBounds {
  Vec4 lower{};
  Vec4 upper{};
};

struct Aabb {
  Vec3 minimum{};
  Vec3 maximum{};
};

[[nodiscard]] std::optional<std::pair<double, double>>
bounded_simplex_extrema(const BarycentricBounds &bounds, const std::array<double, 4> &coefficients);
[[nodiscard]] Aabb project_bounds_interval_sum(const BarycentricBounds &bounds,
                                               const std::array<Vec3, 4> &posed_vertices);
[[nodiscard]] std::optional<Aabb>
project_bounds_bounded_simplex(const BarycentricBounds &bounds,
                               const std::array<Vec3, 4> &posed_vertices);
[[nodiscard]] double aabb_volume(const Aabb &bounds);

struct Ray {
  Vec3 origin{};
  Vec3 direction{};
  double minimum_t{0.0};
  double maximum_t{1.0e30};
};

struct TraceHit {
  double t{};
  Vec3 position{};
  Vec3 source_barycentric{};
  Vec3 normal{};
  Vec2 uv{};
  std::uint32_t source_primitive{};
  std::uint32_t material{};
  std::uint32_t tet_id{};
  std::uint32_t micro_triangle{};
};

struct TraceResult {
  std::optional<TraceHit> closest;
  std::uint64_t raw_hits{};
  std::uint64_t resolved_hits{};
  std::uint64_t raw_duplicate_candidates{};
  std::uint64_t duplicate_ownership{};
  std::uint64_t visited_nodes{};
  std::uint64_t triangle_tests{};
};

struct Bvh4DNode {
  BarycentricBounds bounds{};
  std::uint32_t left{};
  std::uint32_t right{};
  std::uint32_t first{};
  std::uint32_t count{};
  bool leaf{};
};

struct Bvh4DTree {
  std::uint32_t tet_id{};
  std::array<std::uint8_t, 4> ordered_to_local{};
  std::vector<Bvh4DNode> nodes;
  std::vector<std::uint32_t> triangle_indices;
};

struct Bvh4D {
  std::vector<Bvh4DTree> trees;
  std::uint32_t leaf_size{};
};

enum class ProjectionMode : std::uint8_t {
  interval_sum,
  bounded_simplex,
};

[[nodiscard]] Bvh4D build_bvh4d(const CompiledAsset &asset, std::uint32_t leaf_size = 4U);
[[nodiscard]] TraceResult trace_fast(const CompiledAsset &asset,
                                     const std::vector<Vec3> &posed_cage_vertices, const Ray &ray);
[[nodiscard]] TraceResult trace_dense(const SourceMesh &mesh, const Ray &ray);
[[nodiscard]] TraceResult trace_watertight4d(const CompiledAsset &asset, const Bvh4D &bvh,
                                             const std::vector<Vec3> &posed_cage_vertices,
                                             const Ray &ray, ProjectionMode projection);

[[nodiscard]] std::vector<Ray>
generate_adversarial_rays(const CompiledAsset &asset, const std::vector<Vec3> &posed_cage_vertices,
                          std::uint64_t seed, std::uint32_t random_count);

struct OracleComparison {
  std::uint64_t rays{};
  std::uint64_t fast_misses{};
  std::uint64_t exact_misses{};
  std::uint64_t primitive_mismatches{};
  std::uint64_t fast_duplicate_ownership{};
  std::uint64_t exact_duplicate_ownership{};
  std::uint64_t fast_raw_duplicate_candidates{};
  std::uint64_t exact_raw_duplicate_candidates{};
  std::uint64_t fast_visited_nodes{};
  std::uint64_t exact_visited_nodes{};
  double max_position_error{};
  double max_attribute_error{};
  struct Failure {
    std::uint64_t ray_index{};
    Ray ray{};
    bool fast_hit{};
    bool exact_hit{};
    std::optional<std::uint32_t> fast_primitive;
    std::optional<std::uint32_t> exact_primitive;
  };
  std::vector<Failure> failures;
};

[[nodiscard]] OracleComparison compare_oracles(const CompiledAsset &asset, const Bvh4D &bvh,
                                               const std::vector<Vec3> &posed_cage_vertices,
                                               const std::vector<Ray> &rays);

struct ImageDifferential {
  std::uint64_t pixels{};
  std::uint64_t dense_hits{};
  std::uint64_t fast_hits{};
  std::uint64_t exact_hits{};
  std::uint64_t hit_mismatches{};
  std::uint64_t primitive_mismatches{};
  std::uint64_t material_mismatches{};
  double max_position_error{};
  double max_normal_error{};
  double max_uv_error{};
};

[[nodiscard]] ImageDifferential compare_rest_pose_image(const CompiledAsset &asset,
                                                        const Bvh4D &bvh, std::uint32_t width,
                                                        std::uint32_t height);

} // namespace tetcage
