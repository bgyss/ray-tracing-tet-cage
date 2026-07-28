#include "tetcage/asset_format.h"
#include "tetcage/authoring.h"
#include "tetcage/io.h"
#include "tetcage/math.h"
#include "tetcage/oracle.h"
#include "tetcage/runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;
struct FailureRecord {
  std::string test;
  std::string expression;
  int line{};
};
std::vector<FailureRecord> failure_records;

void check(bool condition, const char *expression, const char *test, int line) {
  if (!condition) {
    std::cerr << test << ':' << line << ": CHECK(" << expression << ") failed\n";
    ++failures;
    failure_records.push_back({test, expression, line});
  }
}

#define CHECK_IN(TEST, EXPR) check((EXPR), #EXPR, TEST, __LINE__)

bool near(double a, double b, double tolerance = 1.0e-11) { return std::abs(a - b) <= tolerance; }

bool near(const tetcage::Vec3 &a, const tetcage::Vec3 &b, double tolerance = 1.0e-11) {
  return near(a.x, b.x, tolerance) && near(a.y, b.y, tolerance) && near(a.z, b.z, tolerance);
}

tetcage::Tetrahedron unit_tet() {
  return {{tetcage::Vec3{0.0, 0.0, 0.0}, tetcage::Vec3{1.0, 0.0, 0.0}, tetcage::Vec3{0.0, 1.0, 0.0},
           tetcage::Vec3{0.0, 0.0, 1.0}},
          {10U, 20U, 30U, 40U}};
}

void test_barycentric_round_trip() {
  constexpr const char *test = "barycentric round trip";
  const auto tet = unit_tet();
  const tetcage::Vec4 expected{0.4, 0.1, 0.2, 0.3};
  const auto p = tetcage::from_barycentric(tet, expected);
  const auto got = tetcage::to_barycentric(tet, p);
  CHECK_IN(test, got.has_value());
  if (got) {
    CHECK_IN(test, near(got->x, expected.x));
    CHECK_IN(test, near(got->y, expected.y));
    CHECK_IN(test, near(got->z, expected.z));
    CHECK_IN(test, near(got->w, expected.w));
  }
}

void test_singular_tet_is_explicit() {
  constexpr const char *test = "singular tet is explicit";
  auto tet = unit_tet();
  tet.positions[3] = {0.25, 0.25, 0.0};
  const auto diagnostics = tetcage::diagnose(tet);
  CHECK_IN(test, diagnostics.classification == tetcage::TetClass::near_singular);
  CHECK_IN(test, !tetcage::to_barycentric(tet, {0.2, 0.2, 0.0}).has_value());
  CHECK_IN(test, std::isfinite(diagnostics.determinant));
}

void test_affine_matches_vertex_interpolation() {
  constexpr const char *test = "affine matches vertex interpolation";
  const auto rest = unit_tet();
  auto posed = rest;
  posed.positions = {{{2.0, -1.0, 3.0}, {3.5, -0.5, 2.0}, {1.75, 1.0, 3.25}, {2.5, -1.25, 5.0}}};
  const tetcage::Vec4 bary{0.17, 0.23, 0.31, 0.29};
  const auto transform = tetcage::canonical_to_object(posed);
  CHECK_IN(test, transform.has_value());
  if (transform) {
    const tetcage::Vec3 canonical{bary.y, bary.z, bary.w};
    const auto matrix_result = transform->apply_point(canonical);
    const auto vertex_result = tetcage::from_barycentric(posed, bary);
    CHECK_IN(test, near(matrix_result, vertex_result, 2.0e-12));
  }
}

void test_normal_uses_inverse_transpose() {
  constexpr const char *test = "normal uses inverse transpose";
  const tetcage::Mat3 shear{
      {tetcage::Vec3{2.0, 0.5, 0.0}, tetcage::Vec3{0.0, 1.5, 0.25}, tetcage::Vec3{0.0, 0.0, 0.5}}};
  const auto transformed = tetcage::transform_normal(shear, {0.0, 0.0, 1.0});
  CHECK_IN(test, transformed.has_value());
  if (transformed) {
    const auto tangent_a = shear * tetcage::Vec3{1.0, 0.0, 0.0};
    const auto tangent_b = shear * tetcage::Vec3{0.0, 1.0, 0.0};
    CHECK_IN(test, near(tetcage::dot(*transformed, tangent_a), 0.0, 1.0e-12));
    CHECK_IN(test, near(tetcage::dot(*transformed, tangent_b), 0.0, 1.0e-12));
    CHECK_IN(test, near(tetcage::length(*transformed), 1.0, 1.0e-12));
  }
}

void test_permutations_preserve_points() {
  constexpr const char *test = "tet permutations preserve points";
  const auto original = unit_tet();
  std::array<std::uint32_t, 4> order{0, 1, 2, 3};
  const tetcage::Vec3 point{0.15, 0.25, 0.35};
  do {
    tetcage::Tetrahedron permuted{};
    for (std::size_t i = 0; i < order.size(); ++i) {
      permuted.positions[i] = original.positions[order[i]];
      permuted.vertex_ids[i] = original.vertex_ids[order[i]];
    }
    const auto bary = tetcage::to_barycentric(permuted, point);
    CHECK_IN(test, bary.has_value());
    if (bary) {
      CHECK_IN(test, near(tetcage::from_barycentric(permuted, *bary), point, 2.0e-12));
    }
  } while (std::next_permutation(order.begin(), order.end()));
}

void test_reflection_is_classified() {
  constexpr const char *test = "reflection is classified";
  auto reflected = unit_tet();
  reflected.positions[1].x = -1.0;
  const auto diagnostics = tetcage::diagnose(reflected);
  CHECK_IN(test, diagnostics.classification == tetcage::TetClass::mirrored);
  CHECK_IN(test, diagnostics.determinant < 0.0);
}

void test_triangle_clipping_preserves_source_barycentrics() {
  constexpr const char *test = "clipping preserves source barycentrics";
  const auto tet = unit_tet();
  std::array<tetcage::ClipVertex, 3> triangle{{
      {{-0.5, 0.25, 0.25}, {1.0, 0.0, 0.0}, {}},
      {{0.75, 0.25, 0.25}, {0.0, 1.0, 0.0}, {}},
      {{0.25, 0.75, 0.25}, {0.0, 0.0, 1.0}, {}},
  }};
  const auto clipped = tetcage::clip_triangle_to_tetrahedron(triangle, tet, 0.0);
  CHECK_IN(test, clipped.size() >= 3);
  for (const auto &vertex : clipped) {
    const auto tet_bary = tetcage::to_barycentric(tet, vertex.position);
    CHECK_IN(test, tet_bary.has_value());
    if (tet_bary) {
      CHECK_IN(test, tet_bary->x >= -1.0e-12);
      CHECK_IN(test, tet_bary->y >= -1.0e-12);
      CHECK_IN(test, tet_bary->z >= -1.0e-12);
      CHECK_IN(test, tet_bary->w >= -1.0e-12);
    }
    CHECK_IN(test, near(vertex.source_bary.x + vertex.source_bary.y + vertex.source_bary.z, 1.0,
                        1.0e-12));
  }
}

void test_plane_classification_and_polygon_clipping() {
  constexpr const char *test = "plane classification and polygon clipping";
  const tetcage::Plane plane{{1.0, 0.0, 0.0}, -0.25};
  CHECK_IN(test, tetcage::classify(plane, {0.0, 0.0, 0.0}, 1.0e-12) == tetcage::PlaneSide::outside);
  CHECK_IN(test, tetcage::classify(plane, {0.25, 9.0, -3.0}, 1.0e-12) == tetcage::PlaneSide::on);
  CHECK_IN(test, tetcage::classify(plane, {0.5, 0.0, 0.0}, 1.0e-12) == tetcage::PlaneSide::inside);
  std::vector<tetcage::ClipVertex> polygon{
      {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {}},
      {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {}},
      {{1.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {}},
  };
  const auto clipped = tetcage::clip_polygon_against_plane(polygon, plane, 1.0e-12, 4U);
  CHECK_IN(test, clipped.size() == 4U);
  for (const auto &vertex : clipped) {
    CHECK_IN(test, vertex.position.x >= 0.25 - 1.0e-12);
  }
}

void test_face_identity_is_independent_of_local_order() {
  constexpr const char *test = "face identity independent of local order";
  const auto tet = unit_tet();
  const auto expected = tetcage::canonical_face_identity(tet, 3U);
  tetcage::Tetrahedron reordered{};
  reordered.positions = {tet.positions[2], tet.positions[0], tet.positions[1], tet.positions[3]};
  reordered.vertex_ids = {tet.vertex_ids[2], tet.vertex_ids[0], tet.vertex_ids[1],
                          tet.vertex_ids[3]};
  const auto actual = tetcage::canonical_face_identity(reordered, 3U);
  CHECK_IN(test, actual == expected);
  CHECK_IN(test, (expected == std::array<std::uint64_t, 3>{10U, 20U, 30U}));
}

void test_scale_range_and_near_collapse_policy() {
  constexpr const char *test = "scale range and near collapse policy";
  const tetcage::Vec4 expected{0.1, 0.2, 0.3, 0.4};
  for (const double scale : {0.01, 1.0, 1.0e6}) {
    auto tet = unit_tet();
    for (auto &position : tet.positions) {
      position = position * scale + tetcage::Vec3{3.0 * scale, -2.0 * scale, 5.0 * scale};
    }
    const auto point = tetcage::from_barycentric(tet, expected);
    const auto bary = tetcage::to_barycentric(tet, point);
    CHECK_IN(test, bary.has_value());
    if (bary) {
      const double tolerance = scale >= 1.0e6 ? 2.0e-9 : 2.0e-11;
      CHECK_IN(test, near(bary->x, expected.x, tolerance));
      CHECK_IN(test, near(bary->y, expected.y, tolerance));
      CHECK_IN(test, near(bary->z, expected.z, tolerance));
      CHECK_IN(test, near(bary->w, expected.w, tolerance));
    }
  }
  auto collapsed = unit_tet();
  collapsed.positions[3] = {0.25, 0.25, 1.0e-16};
  CHECK_IN(test, tetcage::diagnose(collapsed).classification == tetcage::TetClass::near_singular);
  CHECK_IN(test, !tetcage::canonical_to_object(collapsed).has_value());
}

void test_randomized_affine_differential() {
  constexpr const char *test = "seeded affine differential";
  constexpr std::uint64_t seed = 0x5eedc0deULL;
  std::mt19937_64 random(seed);
  std::uniform_real_distribution<double> coefficient(0.01, 1.0);
  std::uniform_real_distribution<double> coordinate(-10000.0, 10000.0);
  for (int sample = 0; sample < 2000; ++sample) {
    std::array<double, 4> raw{};
    double sum = 0.0;
    for (double &value : raw) {
      value = coefficient(random);
      sum += value;
    }
    tetcage::Vec4 bary{raw[0] / sum, raw[1] / sum, raw[2] / sum, raw[3] / sum};
    auto posed = unit_tet();
    for (auto &vertex : posed.positions) {
      vertex = {coordinate(random), coordinate(random), coordinate(random)};
    }
    if (tetcage::diagnose(posed).classification == tetcage::TetClass::near_singular) {
      --sample;
      continue;
    }
    const auto transform = tetcage::canonical_to_object(posed);
    CHECK_IN(test, transform.has_value());
    if (!transform) {
      continue;
    }
    const auto by_matrix = transform->apply_point({bary.y, bary.z, bary.w});
    const auto by_vertices = tetcage::from_barycentric(posed, bary);
    const double scale = std::max(1.0, tetcage::length(by_vertices));
    CHECK_IN(test, near(by_matrix, by_vertices, scale * 2.0e-12));
  }
}

tetcage::SourceMesh single_triangle_mesh(tetcage::Vec3 a, tetcage::Vec3 b, tetcage::Vec3 c) {
  tetcage::SourceMesh mesh{};
  mesh.vertices = {
      {a, {0.0, 0.0, 1.0}, {0.0, 0.0}},
      {b, {0.0, 0.0, 1.0}, {1.0, 0.0}},
      {c, {0.0, 0.0, 1.0}, {0.0, 1.0}},
  };
  mesh.triangles.push_back({{0U, 1U, 2U}, 17U, 3U});
  return mesh;
}

tetcage::Cage single_tet_cage() {
  tetcage::Cage cage{};
  const auto tet = unit_tet();
  cage.vertices.assign(tet.positions.begin(), tet.positions.end());
  cage.vertex_ids.assign(tet.vertex_ids.begin(), tet.vertex_ids.end());
  cage.tetrahedra.push_back({{0U, 1U, 2U, 3U}});
  return cage;
}

tetcage::Cage freudenthal_cube_cage() {
  tetcage::Cage cage{};
  cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {1.0, 1.0, 0.0},
      {0.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, {0.0, 1.0, 1.0}, {1.0, 1.0, 1.0},
  };
  cage.vertex_ids = {100U, 101U, 102U, 103U, 104U, 105U, 106U, 107U};
  cage.tetrahedra = {
      {{0U, 1U, 3U, 7U}}, {{0U, 1U, 7U, 5U}}, {{0U, 3U, 2U, 7U}},
      {{0U, 2U, 6U, 7U}}, {{0U, 4U, 5U, 7U}}, {{0U, 6U, 4U, 7U}},
  };
  return cage;
}

tetcage::SourceMesh dense_smooth_patch(std::uint32_t divisions) {
  tetcage::SourceMesh mesh{};
  for (std::uint32_t row = 0; row <= divisions; ++row) {
    for (std::uint32_t column = 0; column <= divisions; ++column) {
      const double u = static_cast<double>(column) / static_cast<double>(divisions);
      const double v = static_cast<double>(row) / static_cast<double>(divisions);
      const double x = 0.1 + 0.3 * u;
      const double y = 0.1 + 0.3 * v;
      const double z =
          0.1 + 0.01 * std::sin(u * 3.141592653589793) * std::sin(v * 3.141592653589793);
      mesh.vertices.push_back({{x, y, z}, {0.0, 0.0, 1.0}, {u, v}});
    }
  }
  for (std::uint32_t row = 0; row < divisions; ++row) {
    for (std::uint32_t column = 0; column < divisions; ++column) {
      const std::uint32_t stride = divisions + 1U;
      const std::uint32_t a = row * stride + column;
      const std::uint32_t b = a + 1U;
      const std::uint32_t c = a + stride;
      const std::uint32_t d = c + 1U;
      const std::uint32_t primitive = static_cast<std::uint32_t>(mesh.triangles.size());
      mesh.triangles.push_back({{a, b, d}, primitive, 11U});
      mesh.triangles.push_back({{a, d, c}, primitive + 1U, 11U});
    }
  }
  return mesh;
}

void test_compiler_preserves_provenance_and_reconstructs_rest_pose() {
  constexpr const char *test = "compiler provenance reconstructs rest pose";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto result = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, result.asset.has_value());
  CHECK_IN(test, result.diagnostics.empty());
  if (!result.asset) {
    return;
  }
  CHECK_IN(test, result.asset->micro_triangles.size() == 1);
  CHECK_IN(test, result.asset->generated_vertices.size() == 3);
  const auto &fragment = result.asset->micro_triangles.front();
  CHECK_IN(test, fragment.source_primitive == 17U);
  CHECK_IN(test, fragment.material == 3U);
  for (std::uint32_t index : fragment.vertex_indices) {
    const auto &generated = result.asset->generated_vertices[index];
    const auto reconstructed = tetcage::from_barycentric(unit_tet(), generated.cage_barycentric);
    const auto source = mesh.vertices[0].position * generated.source_barycentric.x +
                        mesh.vertices[1].position * generated.source_barycentric.y +
                        mesh.vertices[2].position * generated.source_barycentric.z;
    const auto source_uv = mesh.vertices[0].uv.x * generated.source_barycentric.x +
                           mesh.vertices[1].uv.x * generated.source_barycentric.y +
                           mesh.vertices[2].uv.x * generated.source_barycentric.z;
    CHECK_IN(test, near(reconstructed, source, 2.0e-12));
    CHECK_IN(test, source_uv >= -1.0e-12 && source_uv <= 1.0 + 1.0e-12);
    CHECK_IN(test, generated.stable_id != 0U);
  }
}

void test_asset_build_is_byte_deterministic() {
  constexpr const char *test = "asset build is byte deterministic";
  const auto mesh = single_triangle_mesh({-0.2, 0.2, 0.2}, {0.8, 0.2, 0.2}, {0.2, 0.8, 0.2});
  tetcage::TolerancePolicy tolerance{};
  tolerance.expanded_barycentric_epsilon = 2.5e-6;
  const auto first = tetcage::compile_asset(mesh, single_tet_cage(), tolerance);
  const auto second = tetcage::compile_asset(mesh, single_tet_cage(), tolerance);
  CHECK_IN(test, first.asset.has_value());
  CHECK_IN(test, second.asset.has_value());
  if (first.asset && second.asset) {
    const auto first_bytes = tetcage::serialize_asset(*first.asset);
    const auto second_bytes = tetcage::serialize_asset(*second.asset);
    CHECK_IN(test, first_bytes == second_bytes);
    CHECK_IN(test, tetcage::asset_checksum(first_bytes) != 0U);
  }
}

void test_serialized_asset_round_trips() {
  constexpr const char *test = "serialized asset round trips";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.6, 0.1, 0.1}, {0.1, 0.6, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bytes = tetcage::serialize_asset(*compiled.asset);
  const auto decoded = tetcage::deserialize_asset(bytes);
  CHECK_IN(test, decoded.asset.has_value());
  CHECK_IN(test, decoded.error.empty());
  if (decoded.asset) {
    CHECK_IN(test, tetcage::serialize_asset(*decoded.asset) == bytes);
    CHECK_IN(test, decoded.asset->format_version == tetcage::asset_format_version);
    CHECK_IN(test, decoded.asset->statistics.occupied_tetrahedra ==
                       compiled.asset->statistics.occupied_tetrahedra);
    CHECK_IN(test, near(decoded.asset->statistics.triangle_expansion,
                        compiled.asset->statistics.triangle_expansion));
    CHECK_IN(test, near(decoded.asset->statistics.worst_condition,
                        compiled.asset->statistics.worst_condition));
  }
}

void write_u64_le(std::vector<std::byte> &bytes, std::size_t offset, std::uint64_t value) {
  CHECK_IN("serialized mutation helper", offset + sizeof(value) <= bytes.size());
  if (offset + sizeof(value) > bytes.size()) {
    return;
  }
  for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
    bytes[offset + byte] = static_cast<std::byte>((value >> (byte * 8U)) & 0xffU);
  }
}

void test_deserializer_rejects_unsafe_and_nonfinite_assets() {
  constexpr const char *test = "deserializer rejects unsafe and nonfinite assets";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.6, 0.1, 0.1}, {0.1, 0.6, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bytes = tetcage::serialize_asset(*compiled.asset);
  // Header is magic, two u32 values, and three little-endian doubles. The
  // first stream count therefore begins at byte 40.
  constexpr std::size_t source_vertex_count_offset = 40U;
  constexpr std::size_t first_source_position_offset = 48U;
  constexpr std::size_t source_triangle_index_offset = 248U;

  auto oversized = bytes;
  write_u64_le(oversized, source_vertex_count_offset,
               static_cast<std::uint64_t>(tetcage::maximum_asset_bytes) + 1U);
  const auto oversized_result = tetcage::deserialize_asset(oversized);
  CHECK_IN(test, !oversized_result.asset.has_value());

  auto impossible_count = bytes;
  write_u64_le(impossible_count, source_vertex_count_offset, 10'000U);
  const auto impossible_count_result = tetcage::deserialize_asset(impossible_count);
  CHECK_IN(test, !impossible_count_result.asset.has_value());

  auto nonfinite = bytes;
  write_u64_le(nonfinite, first_source_position_offset, 0x7ff8000000000001ULL);
  const auto nonfinite_result = tetcage::deserialize_asset(nonfinite);
  CHECK_IN(test, !nonfinite_result.asset.has_value());

  auto bad_index = bytes;
  write_u64_le(bad_index, source_triangle_index_offset, 99U);
  const auto bad_index_result = tetcage::deserialize_asset(bad_index);
  CHECK_IN(test, !bad_index_result.asset.has_value());

  auto unsupported_version = bytes;
  write_u64_le(unsupported_version, 8U, 2U);
  const auto unsupported_result = tetcage::deserialize_asset(unsupported_version);
  CHECK_IN(test, !unsupported_result.asset.has_value());
}

void test_shared_face_has_one_deterministic_owner() {
  constexpr const char *test = "shared face has one deterministic owner";
  tetcage::Cage cage{};
  cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
  cage.vertex_ids = {10U, 20U, 30U, 40U, 50U};
  cage.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}};
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.0}, {0.7, 0.1, 0.0}, {0.1, 0.7, 0.0});
  const auto compiled = tetcage::compile_asset(mesh, cage, {});
  CHECK_IN(test, compiled.asset.has_value());
  if (compiled.asset) {
    CHECK_IN(test, compiled.asset->micro_triangles.size() == 1);
    CHECK_IN(test, compiled.asset->statistics.boundary_fragments == 1);
    CHECK_IN(test, compiled.asset->micro_triangles.front().owner_tet == 0U);
  }
}

void test_uncovered_geometry_is_actionable() {
  constexpr const char *test = "uncovered geometry is actionable";
  const auto mesh = single_triangle_mesh({3.0, 3.0, 3.0}, {4.0, 3.0, 3.0}, {3.0, 4.0, 3.0});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, !compiled.asset.has_value());
  CHECK_IN(test, !compiled.diagnostics.empty());
  if (!compiled.diagnostics.empty()) {
    CHECK_IN(test, compiled.diagnostics.front().code == "uncovered_source_triangle");
    CHECK_IN(test, compiled.diagnostics.front().source_primitive == 17U);
  }
}

void test_dense_smooth_fixture_compiles_without_losing_provenance() {
  constexpr const char *test = "dense smooth fixture preserves provenance";
  const auto mesh = dense_smooth_patch(12U);
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  CHECK_IN(test, compiled.diagnostics.empty());
  if (compiled.asset) {
    CHECK_IN(test, compiled.asset->micro_triangles.size() == mesh.triangles.size());
    CHECK_IN(test, compiled.asset->statistics.occupied_tetrahedra == 1U);
    CHECK_IN(test, near(compiled.asset->statistics.triangle_expansion, 1.0));
    for (const auto &fragment : compiled.asset->micro_triangles) {
      CHECK_IN(test, fragment.source_primitive < mesh.triangles.size());
      CHECK_IN(test, fragment.material == 11U);
    }
  }
}

void test_regular_cube_six_tet_fixture_is_covered() {
  constexpr const char *test = "regular cube six tet fixture is covered";
  const auto mesh = single_triangle_mesh({0.15, 0.2, 0.3}, {0.85, 0.2, 0.3}, {0.2, 0.8, 0.7});
  const auto compiled = tetcage::compile_asset(mesh, freudenthal_cube_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  CHECK_IN(test, compiled.diagnostics.empty());
  if (compiled.asset) {
    CHECK_IN(test, compiled.asset->statistics.occupied_tetrahedra >= 2U);
    CHECK_IN(test, compiled.asset->statistics.generated_triangles >= 2U);
    CHECK_IN(test, compiled.asset->tet_metadata.size() == 6U);
    for (const auto &metadata : compiled.asset->tet_metadata) {
      CHECK_IN(test, std::isfinite(metadata.determinant));
      CHECK_IN(test, std::isfinite(metadata.condition_estimate));
      CHECK_IN(test, !metadata.near_singular);
    }
  }
}

void test_cross_face_vertices_share_stable_feature_identity() {
  constexpr const char *test = "cross face vertices share stable identity";
  tetcage::Cage cage{};
  cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
  cage.vertex_ids = {10U, 20U, 30U, 40U, 50U};
  cage.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}};
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.2}, {0.6, 0.1, -0.2}, {0.1, 0.6, 0.0});
  const auto compiled = tetcage::compile_asset(mesh, cage, {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  std::map<std::uint64_t, std::set<std::uint32_t>> boundary_ids;
  for (const auto &vertex : compiled.asset->generated_vertices) {
    if (vertex.feature.cage_boundary_mask != 0U) {
      boundary_ids[vertex.stable_id].insert(vertex.tet_id);
    }
  }
  bool found_shared_identity = false;
  for (const auto &[stable_id, tet_ids] : boundary_ids) {
    CHECK_IN(test, stable_id != 0U);
    found_shared_identity = found_shared_identity || tet_ids.size() == 2U;
  }
  CHECK_IN(test, found_shared_identity);
}

void test_cage_validation_rejects_duplicate_ids_and_nonmanifold_faces() {
  constexpr const char *test = "cage validation rejects topology errors";
  auto duplicate_ids = single_tet_cage();
  duplicate_ids.vertex_ids[3] = duplicate_ids.vertex_ids[2];
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.5, 0.1, 0.1}, {0.1, 0.5, 0.1});
  const auto duplicate_result = tetcage::compile_asset(mesh, duplicate_ids, {});
  CHECK_IN(test, !duplicate_result.asset.has_value());
  CHECK_IN(test, !duplicate_result.diagnostics.empty());
  if (!duplicate_result.diagnostics.empty()) {
    CHECK_IN(test, duplicate_result.diagnostics.front().code == "duplicate_cage_vertex_id");
  }

  tetcage::Cage nonmanifold{};
  nonmanifold.vertices = {{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0},  {0.0, 1.0, 0.0},
                          {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}, {0.25, 0.25, 2.0}};
  nonmanifold.vertex_ids = {10U, 20U, 30U, 40U, 50U, 60U};
  nonmanifold.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}, {{0U, 1U, 2U, 5U}}};
  const auto nonmanifold_result = tetcage::compile_asset(mesh, nonmanifold, {});
  CHECK_IN(test, !nonmanifold_result.asset.has_value());
  bool found_nonmanifold = false;
  for (const auto &diagnostic : nonmanifold_result.diagnostics) {
    found_nonmanifold = found_nonmanifold || diagnostic.code == "nonmanifold_cage_face";
  }
  CHECK_IN(test, found_nonmanifold);
}

void test_obj_and_cage_files_compile() {
  constexpr const char *test = "obj and cage files compile";
  const std::string root = TETCAGE_SOURCE_DIR;
  const auto mesh = tetcage::load_obj(root + "/tests/assets/one-tet.obj");
  const auto cage = tetcage::load_tet_cage(root + "/tests/assets/one-tet.cage");
  CHECK_IN(test, mesh.value.has_value());
  CHECK_IN(test, cage.value.has_value());
  CHECK_IN(test, mesh.error.empty());
  CHECK_IN(test, cage.error.empty());
  if (mesh.value && cage.value) {
    const auto compiled = tetcage::compile_asset(*mesh.value, *cage.value, {});
    CHECK_IN(test, compiled.asset.has_value());
    if (compiled.asset) {
      CHECK_IN(test, compiled.asset->source.triangles.front().material_id == 7U);
      // Golden checksum for the checked-in one-tet source/cage pair. This
      // catches accidental format drift while repeated-build tests catch
      // traversal-order nondeterminism.
      CHECK_IN(test, tetcage::asset_checksum(tetcage::serialize_asset(*compiled.asset)) ==
                         0x325c89d5964ef6a7ULL);
    }
  }
}

void test_invalid_cage_file_has_line_diagnostic() {
  constexpr const char *test = "invalid cage file has line diagnostic";
  const std::string root = TETCAGE_SOURCE_DIR;
  const auto cage = tetcage::load_tet_cage(root + "/tests/assets/invalid.cage");
  CHECK_IN(test, !cage.value.has_value());
  CHECK_IN(test, cage.error.find("line 3") != std::string::npos);
}

std::pair<double, double> brute_bounded_simplex_extrema(const tetcage::BarycentricBounds &bounds,
                                                        const std::array<double, 4> &coefficients) {
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = -std::numeric_limits<double>::infinity();
  for (std::size_t free_coordinate = 0; free_coordinate < 4U; ++free_coordinate) {
    for (std::uint32_t choices = 0; choices < 8U; ++choices) {
      std::array<double, 4> values{};
      std::size_t choice = 0;
      double assigned = 0.0;
      for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
        if (coordinate == free_coordinate) {
          continue;
        }
        values[coordinate] =
            ((choices >> choice) & 1U) != 0U ? bounds.upper[coordinate] : bounds.lower[coordinate];
        assigned += values[coordinate];
        ++choice;
      }
      values[free_coordinate] = 1.0 - assigned;
      if (values[free_coordinate] < bounds.lower[free_coordinate] - 1.0e-12 ||
          values[free_coordinate] > bounds.upper[free_coordinate] + 1.0e-12) {
        continue;
      }
      double objective = 0.0;
      for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
        objective += coefficients[coordinate] * values[coordinate];
      }
      minimum = std::min(minimum, objective);
      maximum = std::max(maximum, objective);
    }
  }
  return {minimum, maximum};
}

void test_bounded_simplex_projection_matches_vertex_enumeration() {
  constexpr const char *test = "bounded simplex projection is exact";
  const tetcage::BarycentricBounds bounds{{0.1, 0.1, 0.1, 0.1}, {0.7, 0.7, 0.7, 0.7}};
  const std::array<double, 4> coefficients{0.0, 1.0, 2.0, 3.0};
  const auto exact = tetcage::bounded_simplex_extrema(bounds, coefficients);
  CHECK_IN(test, exact.has_value());
  if (exact) {
    CHECK_IN(test, near(exact->first, 0.6, 1.0e-12));
    CHECK_IN(test, near(exact->second, 2.4, 1.0e-12));
  }

  std::mt19937_64 random(0x4db0'0d5ULL);
  std::uniform_real_distribution<double> coefficient(-10.0, 10.0);
  std::uniform_real_distribution<double> lower_distribution(0.0, 0.15);
  for (int sample = 0; sample < 500; ++sample) {
    tetcage::BarycentricBounds random_bounds{};
    double lower_sum = 0.0;
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      random_bounds.lower[coordinate] = lower_distribution(random);
      lower_sum += random_bounds.lower[coordinate];
    }
    const double remaining = 1.0 - lower_sum;
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      random_bounds.upper[coordinate] = random_bounds.lower[coordinate] + remaining * 0.75;
    }
    std::array<double, 4> random_coefficients{};
    for (double &value : random_coefficients) {
      value = coefficient(random);
    }
    const auto greedy = tetcage::bounded_simplex_extrema(random_bounds, random_coefficients);
    const auto brute = brute_bounded_simplex_extrema(random_bounds, random_coefficients);
    CHECK_IN(test, greedy.has_value());
    if (greedy) {
      CHECK_IN(test, near(greedy->first, brute.first, 5.0e-12));
      CHECK_IN(test, near(greedy->second, brute.second, 5.0e-12));
    }
  }
}

void test_cpu_fast_and_4d_oracles_reconstruct_same_hit() {
  constexpr const char *test = "cpu fast and 4d oracles reconstruct same hit";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bvh = tetcage::build_bvh4d(*compiled.asset, 1U);
  const tetcage::Ray ray{{0.25, 0.25, 2.0}, {0.0, 0.0, -1.0}, 0.0, 10.0};
  const auto fast = tetcage::trace_fast(*compiled.asset, compiled.asset->cage.vertices, ray);
  const auto exact =
      tetcage::trace_watertight4d(*compiled.asset, bvh, compiled.asset->cage.vertices, ray,
                                  tetcage::ProjectionMode::bounded_simplex);
  CHECK_IN(test, fast.closest.has_value());
  CHECK_IN(test, exact.closest.has_value());
  if (fast.closest && exact.closest) {
    CHECK_IN(test, fast.closest->source_primitive == 17U);
    CHECK_IN(test, exact.closest->source_primitive == 17U);
    CHECK_IN(test, near(fast.closest->position, exact.closest->position, 1.0e-12));
    CHECK_IN(test, near(fast.closest->source_barycentric.x, exact.closest->source_barycentric.x,
                        1.0e-12));
    CHECK_IN(test, near(fast.closest->uv.x, exact.closest->uv.x, 1.0e-12));
    CHECK_IN(test, near(fast.closest->uv.y, exact.closest->uv.y, 1.0e-12));
  }
}

void test_adversarial_ray_corpus_matches_4d_oracle() {
  constexpr const char *test = "adversarial ray corpus matches 4d oracle";
  tetcage::Cage cage{};
  cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
  cage.vertex_ids = {10U, 20U, 30U, 40U, 50U};
  cage.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}};
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.3}, {0.7, 0.1, -0.2}, {0.1, 0.7, 0.0});
  tetcage::TolerancePolicy tolerance{};
  tolerance.expanded_barycentric_epsilon = 2.5e-6;
  const auto compiled = tetcage::compile_asset(mesh, cage, tolerance);
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bvh = tetcage::build_bvh4d(*compiled.asset, 2U);
  const auto rays =
      tetcage::generate_adversarial_rays(*compiled.asset, cage.vertices, 0x4d3c0deULL, 256U);
  CHECK_IN(test, rays.size() >= 256U);
  const auto comparison = tetcage::compare_oracles(*compiled.asset, bvh, cage.vertices, rays);
  CHECK_IN(test, comparison.rays == rays.size());
  CHECK_IN(test, comparison.fast_misses == 0U);
  CHECK_IN(test, comparison.exact_misses == 0U);
  CHECK_IN(test, comparison.primitive_mismatches == 0U);
  CHECK_IN(test, comparison.exact_duplicate_ownership == 0U);
  // The corpus includes rays only 1e-7 radians off the triangle plane. For
  // those deliberately ill-conditioned intersections, two independent FP64
  // algorithms agree to 2e-9 absolute while normal-incidence cases are much
  // tighter.
  constexpr double grazing_tolerance = 2.0e-9;
  if (comparison.max_position_error > grazing_tolerance ||
      comparison.max_attribute_error > grazing_tolerance) {
    std::cerr << "oracle maxima: position=" << comparison.max_position_error
              << " attribute=" << comparison.max_attribute_error << '\n';
  }
  CHECK_IN(test, comparison.max_position_error <= grazing_tolerance);
  CHECK_IN(test, comparison.max_attribute_error <= grazing_tolerance);
  CHECK_IN(test, comparison.exact_visited_nodes > 0U);
}

void test_exact_projection_is_no_looser_than_interval_sum() {
  constexpr const char *test = "exact projection is no looser than interval sum";
  const tetcage::BarycentricBounds bounds{{0.05, 0.1, 0.0, 0.0}, {0.8, 0.7, 0.6, 0.5}};
  const std::array<tetcage::Vec3, 4> posed{
      tetcage::Vec3{-2.0, 0.0, 1.0}, tetcage::Vec3{3.0, -1.0, 0.5}, tetcage::Vec3{0.0, 4.0, -3.0},
      tetcage::Vec3{1.0, 2.0, 5.0}};
  const auto interval = tetcage::project_bounds_interval_sum(bounds, posed);
  const auto exact = tetcage::project_bounds_bounded_simplex(bounds, posed);
  CHECK_IN(test, exact.has_value());
  if (exact) {
    CHECK_IN(test, exact->minimum.x >= interval.minimum.x - 1.0e-12);
    CHECK_IN(test, exact->minimum.y >= interval.minimum.y - 1.0e-12);
    CHECK_IN(test, exact->minimum.z >= interval.minimum.z - 1.0e-12);
    CHECK_IN(test, exact->maximum.x <= interval.maximum.x + 1.0e-12);
    CHECK_IN(test, exact->maximum.y <= interval.maximum.y + 1.0e-12);
    CHECK_IN(test, exact->maximum.z <= interval.maximum.z + 1.0e-12);
    CHECK_IN(test, tetcage::aabb_volume(*exact) <= tetcage::aabb_volume(interval) + 1.0e-12);
  }
}

void test_oracles_cover_mirrored_near_degenerate_and_scale_poses() {
  constexpr const char *test = "oracles cover mirrored degenerate and scale poses";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bvh = tetcage::build_bvh4d(*compiled.asset, 1U);
  std::vector<std::vector<tetcage::Vec3>> poses;
  auto mirrored = compiled.asset->cage.vertices;
  for (auto &vertex : mirrored) {
    vertex.x = -vertex.x;
  }
  poses.push_back(mirrored);
  auto near_degenerate = compiled.asset->cage.vertices;
  near_degenerate[3].z = 1.0e-5;
  poses.push_back(near_degenerate);
  for (const double scale : {1.0e-3, 1.0e6}) {
    auto scaled = compiled.asset->cage.vertices;
    for (auto &vertex : scaled) {
      vertex = vertex * scale;
    }
    poses.push_back(scaled);
  }
  std::uint64_t seed = 100U;
  for (const auto &pose : poses) {
    const auto rays = tetcage::generate_adversarial_rays(*compiled.asset, pose, seed++, 64U);
    const auto comparison = tetcage::compare_oracles(*compiled.asset, bvh, pose, rays);
    CHECK_IN(test, comparison.fast_misses == 0U);
    CHECK_IN(test, comparison.exact_misses == 0U);
    CHECK_IN(test, comparison.primitive_mismatches == 0U);
    CHECK_IN(test, comparison.exact_duplicate_ownership == 0U);
  }
}

void test_rest_pose_image_and_attributes_match_dense_source() {
  constexpr const char *test = "rest pose image and attributes match dense source";
  tetcage::Cage cage{};
  cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
  cage.vertex_ids = {10U, 20U, 30U, 40U, 50U};
  cage.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}};
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.3}, {0.7, 0.1, -0.2}, {0.1, 0.7, 0.0});
  const auto compiled = tetcage::compile_asset(mesh, cage, {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto bvh = tetcage::build_bvh4d(*compiled.asset, 2U);
  const auto image = tetcage::compare_rest_pose_image(*compiled.asset, bvh, 64U, 64U);
  CHECK_IN(test, image.pixels == 4096U);
  CHECK_IN(test, image.dense_hits > 100U);
  CHECK_IN(test, image.hit_mismatches == 0U);
  CHECK_IN(test, image.primitive_mismatches == 0U);
  CHECK_IN(test, image.material_mismatches == 0U);
  CHECK_IN(test, image.max_position_error <= 1.0e-11);
  CHECK_IN(test, image.max_normal_error <= 1.0e-11);
  CHECK_IN(test, image.max_uv_error <= 1.0e-11);
}

void test_runtime_transform_and_dense_baseline_contracts() {
  constexpr const char *test = "runtime transforms and dense baseline";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  tetcage::CagePose pose{};
  pose.positions = compiled.asset->cage.vertices;
  for (auto &position : pose.positions) {
    position = {2.0 + 1.5 * position.x + 0.2 * position.y,
                -1.0 + 0.5 * position.x + 2.0 * position.y,
                3.0 + 0.25 * position.y + 0.75 * position.z};
  }
  const auto transforms = tetcage::build_tet_transforms(*compiled.asset, pose, 9U, 4U);
  CHECK_IN(test, transforms.error.empty());
  CHECK_IN(test, transforms.transforms.size() == 1U);
  if (!transforms.transforms.empty()) {
    const tetcage::Vec4 bary{0.1, 0.2, 0.3, 0.4};
    const auto by_transform =
        transforms.transforms.front().object_from_canonical.apply_point({bary.y, bary.z, bary.w});
    tetcage::Tetrahedron posed_tet{};
    for (std::size_t corner = 0; corner < 4U; ++corner) {
      posed_tet.positions[corner] = pose.positions[corner];
    }
    CHECK_IN(test, near(by_transform, tetcage::from_barycentric(posed_tet, bary), 2.0e-12));
  }
  const auto dense = tetcage::deform_dense_source(*compiled.asset, pose);
  CHECK_IN(test, dense.error.empty());
  CHECK_IN(test, dense.positions.size() == mesh.vertices.size());
  for (std::size_t index = 0; index < dense.positions.size(); ++index) {
    const auto original_bary = tetcage::to_barycentric(unit_tet(), mesh.vertices[index].position);
    CHECK_IN(test, original_bary.has_value());
    if (original_bary) {
      tetcage::Tetrahedron posed_tet{};
      posed_tet.positions = {pose.positions[0], pose.positions[1], pose.positions[2],
                             pose.positions[3]};
      CHECK_IN(test, near(dense.positions[index],
                          tetcage::from_barycentric(posed_tet, *original_bary), 2.0e-12));
    }
  }
}

void test_asset_cache_owns_opaque_handles_without_serializing_them() {
  constexpr const char *test = "asset cache owns opaque handles";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto before = tetcage::serialize_asset(*compiled.asset);
  tetcage::AssetCache cache{};
  const auto key = cache.insert(std::make_shared<const tetcage::CompiledAsset>(*compiled.asset),
                                "stub", {{101U, 4096U}, {102U, 2048U}});
  CHECK_IN(test, cache.size() == 1U);
  const auto *entry = cache.find(key);
  CHECK_IN(test, entry != nullptr);
  if (entry != nullptr) {
    CHECK_IN(test, entry->immutable_micro_blas.size() == 2U);
    CHECK_IN(test, entry->backend == "stub");
    CHECK_IN(test, tetcage::serialize_asset(*entry->asset) == before);
  }
  CHECK_IN(test, cache.erase(key));
  CHECK_IN(test, cache.size() == 0U);
}

void test_benchmark_detects_corrupt_stub_and_keeps_nullable_metrics() {
  constexpr const char *test = "benchmark detects corrupt stub";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  tetcage::BenchmarkScene scene{};
  scene.id = "unit-runtime";
  scene.seed = 12345U;
  scene.copies = 4U;
  scene.ray_count = 128U;
  scene.motion_amplitude = 0.05;
  scene.visible_fraction = 0.75;
  tetcage::BenchmarkOptions options{};
  options.warmup_iterations = 1U;
  options.measured_iterations = 5U;
  const auto clean = tetcage::run_cpu_stub_benchmark(*compiled.asset, scene, options);
  CHECK_IN(test, clean.correctness_errors == 0U);
  CHECK_IN(test, !clean.corruption_detected);
  CHECK_IN(test, clean.total_samples_ms.size() == 5U);
  CHECK_IN(test, clean.cage_samples_ms.size() == 5U);
  CHECK_IN(test, clean.transform_samples_ms.size() == 5U);
  CHECK_IN(test, clean.instance_samples_ms.size() == 5U);
  CHECK_IN(test, clean.traversal_samples_ms.size() == 5U);
  CHECK_IN(test, clean.dense_deformation_samples_ms.size() == 5U);
  CHECK_IN(test, clean.dense_traversal_samples_ms.size() == 5U);
  CHECK_IN(test, clean.summary.median_ms >= 0.0);
  CHECK_IN(test, clean.summary.p95_ms >= clean.summary.median_ms);
  CHECK_IN(test, clean.memory.blas_bytes == std::nullopt);
  CHECK_IN(test, clean.memory.tlas_bytes == std::nullopt);
  const auto clean_json = tetcage::benchmark_manifest_json(clean);
  CHECK_IN(test, clean_json.find("\"tlas\": null") != std::string::npos);
  CHECK_IN(test, clean_json.find("\"synchronization\": null") != std::string::npos);
  CHECK_IN(test, clean_json.find("\"evidence_class\": \"synthetic\"") != std::string::npos);
  CHECK_IN(test, clean_json.find("\"dense_cpu_baseline\"") != std::string::npos);
  CHECK_IN(test, tetcage::benchmark_samples_csv(clean).find(
                     "cage_deformation_ms,transform_generation_ms") != std::string::npos);

  options.inject_stub_corruption = true;
  const auto corrupt = tetcage::run_cpu_stub_benchmark(*compiled.asset, scene, options);
  CHECK_IN(test, corrupt.corruption_detected);
  CHECK_IN(test, corrupt.correctness_errors > 0U);
  CHECK_IN(test, !corrupt.failures.empty());

  tetcage::Cage crossing_cage{};
  crossing_cage.vertices = {
      {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}, {0.0, 0.0, -1.0}};
  crossing_cage.vertex_ids = {10U, 20U, 30U, 40U, 50U};
  crossing_cage.tetrahedra = {{{0U, 1U, 2U, 3U}}, {{0U, 2U, 1U, 4U}}};
  const auto crossing_mesh =
      single_triangle_mesh({0.1, 0.1, 0.3}, {0.7, 0.1, -0.2}, {0.1, 0.7, 0.0});
  tetcage::TolerancePolicy crossing_tolerance{};
  crossing_tolerance.expanded_barycentric_epsilon = 2.5e-6;
  const auto crossing = tetcage::compile_asset(crossing_mesh, crossing_cage, crossing_tolerance);
  CHECK_IN(test, crossing.asset.has_value());
  if (crossing.asset) {
    scene.id = "crossing-animated";
    scene.copies = 8U;
    scene.ray_count = 512U;
    scene.motion_amplitude = 0.1;
    options.warmup_iterations = 1U;
    options.measured_iterations = 3U;
    options.inject_stub_corruption = false;
    const auto animated = tetcage::run_cpu_stub_benchmark(*crossing.asset, scene, options);
    if (animated.misses != 0U) {
      for (const auto &failure : animated.failures) {
        std::cerr << failure << '\n';
      }
    }
    CHECK_IN(test, animated.misses == 0U);
    CHECK_IN(test, animated.correctness_errors == 0U);
  }
}

void test_backend_adapter_uses_neutral_frame_contract() {
  constexpr const char *test = "backend adapter uses neutral frame contract";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  auto asset = std::make_shared<const tetcage::CompiledAsset>(*compiled.asset);
  tetcage::CpuStubBackend backend(asset);
  tetcage::FrameBuildInput frame{};
  frame.objects.push_back({77U, 4U, 0U, true});
  frame.poses.push_back({asset->cage.vertices});
  frame.policy = {tetcage::BuildStrategy::periodic_rebuild, 8U, false, std::nullopt};
  const auto built = backend.build_frame(frame);
  CHECK_IN(test, built.error.empty());
  CHECK_IN(test, built.visible_instances == 1U);
  CHECK_IN(test, built.transforms == asset->cage.tetrahedra.size());
  frame.policy.max_instances = 0U;
  const auto rejected = backend.build_frame(frame);
  CHECK_IN(test, rejected.error.find("instance limit") != std::string::npos);
  frame.policy.max_instances = std::nullopt;
  const auto traced = backend.trace({{0.25, 0.25, 2.0}, {0.0, 0.0, -1.0}, 0.0, 10.0});
  CHECK_IN(test, traced.closest.has_value());
  CHECK_IN(test, backend.api_name() == "stub");
  CHECK_IN(test, !backend.capabilities().hardware_acceleration);
}

void test_cage_quality_analysis_is_deterministic_and_actionable() {
  constexpr const char *test = "cage quality analysis is deterministic and actionable";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto first = tetcage::analyze_cage_quality(*compiled.asset, 4U, 0.05);
  const auto second = tetcage::analyze_cage_quality(*compiled.asset, 4U, 0.05);
  CHECK_IN(test, first.asset_hash == second.asset_hash);
  CHECK_IN(test, first.frames.size() == 4U);
  CHECK_IN(test, first.suitable);
  CHECK_IN(test, first.maximum_affine_residual < 1.0e-12);
  CHECK_IN(test, tetcage::cage_quality_json(first).find("\"suitable\": true") != std::string::npos);
}

void test_cage_animation_analysis_reports_clip_residuals_and_weight_fit() {
  constexpr const char *test = "cage animation analysis reports clip residuals and weight fit";
  const auto mesh = single_triangle_mesh({0.1, 0.1, 0.1}, {0.7, 0.1, 0.1}, {0.1, 0.7, 0.1});
  const auto compiled = tetcage::compile_asset(mesh, single_tet_cage(), {});
  CHECK_IN(test, compiled.asset.has_value());
  if (!compiled.asset) {
    return;
  }
  const auto first = tetcage::analyze_cage_animation(*compiled.asset, 8U, 0.05);
  const auto second = tetcage::analyze_cage_animation(*compiled.asset, 8U, 0.05);
  CHECK_IN(test, first.asset_hash == second.asset_hash);
  CHECK_IN(test, first.samples == 8U);
  CHECK_IN(test, first.surface_samples == compiled.asset->generated_vertices.size());
  CHECK_IN(test, first.uncovered_samples == 0U);
  CHECK_IN(test, first.frames.size() == 8U);
  CHECK_IN(test, std::isfinite(first.maximum_position_error));
  CHECK_IN(test, std::isfinite(first.maximum_normal_error));
  CHECK_IN(test, first.optimized_rms_position_error <= first.rms_position_error);
  CHECK_IN(test, tetcage::cage_animation_json(first).find(
                     "\"clip_kind\": \"procedural_non_affine\"") != std::string::npos);
}

void test_cage_refinement_is_conforming_and_orientation_preserving() {
  constexpr const char *test = "cage refinement is conforming and orientation preserving";
  const auto refined = tetcage::refine_cage(single_tet_cage(), 1U);
  CHECK_IN(test, refined.cage.has_value());
  CHECK_IN(test, refined.error.empty());
  if (!refined.cage) {
    return;
  }
  CHECK_IN(test, refined.cage->vertices.size() == 10U);
  CHECK_IN(test, refined.cage->tetrahedra.size() == 8U);
  CHECK_IN(test,
           std::set<std::uint64_t>(refined.cage->vertex_ids.begin(), refined.cage->vertex_ids.end())
                   .size() == refined.cage->vertex_ids.size());
  for (const auto &tet : refined.cage->tetrahedra) {
    tetcage::Tetrahedron geometry{};
    for (std::size_t corner = 0; corner < 4U; ++corner) {
      geometry.positions[corner] = refined.cage->vertices[tet.vertex_indices[corner]];
      geometry.vertex_ids[corner] = refined.cage->vertex_ids[tet.vertex_indices[corner]];
    }
    CHECK_IN(test, tetcage::diagnose(geometry).determinant > 0.0);
  }
  const auto refined_twice = tetcage::refine_cage(single_tet_cage(), 2U);
  CHECK_IN(test, refined_twice.cage.has_value());
  if (refined_twice.cage) {
    CHECK_IN(test, refined_twice.cage->tetrahedra.size() == 64U);
  }
}

void test_cage_lod_set_has_deterministic_parent_maps() {
  constexpr const char *test = "cage LOD set has deterministic parent maps";
  const auto first = tetcage::build_cage_lods(single_tet_cage(), 2U);
  const auto second = tetcage::build_cage_lods(single_tet_cage(), 2U);
  CHECK_IN(test, first.error.empty());
  CHECK_IN(test, second.error.empty());
  CHECK_IN(test, first.levels.size() == 3U);
  CHECK_IN(test, second.levels.size() == first.levels.size());
  for (std::size_t level = 0; level < first.levels.size(); ++level) {
    CHECK_IN(test,
             first.levels[level].cage.vertices.size() == second.levels[level].cage.vertices.size());
    if (first.levels[level].cage.vertices.size() == second.levels[level].cage.vertices.size()) {
      for (std::size_t vertex = 0; vertex < first.levels[level].cage.vertices.size(); ++vertex) {
        CHECK_IN(test, near(first.levels[level].cage.vertices[vertex],
                            second.levels[level].cage.vertices[vertex]));
      }
    }
    CHECK_IN(test, first.levels[level].cage.tetrahedra.size() ==
                       second.levels[level].cage.tetrahedra.size());
    if (first.levels[level].cage.tetrahedra.size() == second.levels[level].cage.tetrahedra.size()) {
      for (std::size_t tet = 0; tet < first.levels[level].cage.tetrahedra.size(); ++tet) {
        CHECK_IN(test, first.levels[level].cage.tetrahedra[tet].vertex_indices ==
                           second.levels[level].cage.tetrahedra[tet].vertex_indices);
      }
    }
    CHECK_IN(test, first.levels[level].parent_tetrahedra == second.levels[level].parent_tetrahedra);
    if (level == 0U) {
      CHECK_IN(test, first.levels[level].parent_tetrahedra.empty());
    } else {
      CHECK_IN(test, first.levels[level].parent_tetrahedra.size() ==
                         first.levels[level].cage.tetrahedra.size());
      for (const auto parent : first.levels[level].parent_tetrahedra) {
        CHECK_IN(test, parent < first.levels[level - 1U].cage.tetrahedra.size());
      }
    }
  }
  CHECK_IN(test,
           tetcage::cage_lod_json(first).find("\"parent_map_valid\": true") != std::string::npos);
}

} // namespace

int main() {
  test_barycentric_round_trip();
  test_singular_tet_is_explicit();
  test_affine_matches_vertex_interpolation();
  test_normal_uses_inverse_transpose();
  test_permutations_preserve_points();
  test_reflection_is_classified();
  test_triangle_clipping_preserves_source_barycentrics();
  test_plane_classification_and_polygon_clipping();
  test_face_identity_is_independent_of_local_order();
  test_scale_range_and_near_collapse_policy();
  test_randomized_affine_differential();
  test_compiler_preserves_provenance_and_reconstructs_rest_pose();
  test_asset_build_is_byte_deterministic();
  test_serialized_asset_round_trips();
  test_deserializer_rejects_unsafe_and_nonfinite_assets();
  test_shared_face_has_one_deterministic_owner();
  test_uncovered_geometry_is_actionable();
  test_dense_smooth_fixture_compiles_without_losing_provenance();
  test_regular_cube_six_tet_fixture_is_covered();
  test_cross_face_vertices_share_stable_feature_identity();
  test_cage_validation_rejects_duplicate_ids_and_nonmanifold_faces();
  test_obj_and_cage_files_compile();
  test_invalid_cage_file_has_line_diagnostic();
  test_bounded_simplex_projection_matches_vertex_enumeration();
  test_cpu_fast_and_4d_oracles_reconstruct_same_hit();
  test_adversarial_ray_corpus_matches_4d_oracle();
  test_exact_projection_is_no_looser_than_interval_sum();
  test_oracles_cover_mirrored_near_degenerate_and_scale_poses();
  test_rest_pose_image_and_attributes_match_dense_source();
  test_runtime_transform_and_dense_baseline_contracts();
  test_asset_cache_owns_opaque_handles_without_serializing_them();
  test_benchmark_detects_corrupt_stub_and_keeps_nullable_metrics();
  test_backend_adapter_uses_neutral_frame_contract();
  test_cage_quality_analysis_is_deterministic_and_actionable();
  test_cage_animation_analysis_reports_clip_residuals_and_weight_fit();
  test_cage_refinement_is_conforming_and_orientation_preserving();
  test_cage_lod_set_has_deterministic_parent_maps();
  if (failures != 0) {
    const std::filesystem::path corpus =
        std::filesystem::path(TETCAGE_SOURCE_DIR) / "results/generated/cpu-failure-corpus.json";
    std::filesystem::create_directories(corpus.parent_path());
    std::ofstream output(corpus);
    output << "{\n  \"schema_version\": 1,\n  \"seed\": \"0x5eedc0de\",\n"
              "  \"failures\": [\n";
    for (std::size_t index = 0; index < failure_records.size(); ++index) {
      const auto &failure = failure_records[index];
      output << "    {\"test\": \"" << failure.test << "\", \"expression\": \""
             << failure.expression << "\", \"line\": " << failure.line << "}"
             << (index + 1U == failure_records.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    std::cerr << failures << " checks failed\n";
    return EXIT_FAILURE;
  }
  std::filesystem::remove(std::filesystem::path(TETCAGE_SOURCE_DIR) /
                          "results/generated/cpu-failure-corpus.json");
  std::cout << "all portable tests passed\n";
  return EXIT_SUCCESS;
}
