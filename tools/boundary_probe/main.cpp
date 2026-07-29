#include "tetcage/asset_format.h"
#include "tetcage/io.h"
#include "tetcage/oracle.h"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>
#include <string>

namespace {

struct ModeResult {
  bool passed{};
  std::uint64_t qualifying_fragments{};
  bool ray_hit{};
  std::uint32_t ray_primitive{};
};

std::uint32_t popcount(std::uint8_t value) {
  std::uint32_t count = 0U;
  while (value != 0U) {
    count += value & 1U;
    value = static_cast<std::uint8_t>(value >> 1U);
  }
  return count;
}

bool has_shared_face(const tetcage::CompiledAsset &asset, const tetcage::GeneratedVertex &vertex) {
  if (vertex.tet_id >= asset.tet_metadata.size()) {
    return false;
  }
  for (std::uint32_t face = 0U; face < 4U; ++face) {
    if ((vertex.feature.cage_boundary_mask & (1U << face)) != 0U &&
        asset.tet_metadata[vertex.tet_id].adjacent_tet[face] >= 0) {
      return true;
    }
  }
  return false;
}

ModeResult measure_ray(const tetcage::CompiledAsset &asset, const tetcage::Bvh4D &bvh,
                       tetcage::Vec3 target, std::uint32_t expected_primitive,
                       std::uint64_t qualifying_fragments) {
  const tetcage::Ray ray{{target.x, target.y, target.z + 2.0}, {0.0, 0.0, -1.0}, 0.0, 4.0};
  const auto trace = tetcage::trace_watertight4d(asset, bvh, asset.cage.vertices, ray,
                                                 tetcage::ProjectionMode::bounded_simplex);
  const bool hit =
      trace.closest.has_value() && trace.closest->source_primitive == expected_primitive;
  return {qualifying_fragments > 0U && hit, qualifying_fragments, hit,
          trace.closest ? trace.closest->source_primitive : UINT32_MAX};
}

tetcage::Vec3 source_position(const tetcage::CompiledAsset &asset, std::uint32_t primitive,
                              std::uint32_t corner) {
  const auto &triangle = asset.source.triangles[primitive];
  return asset.source.vertices[triangle.vertex_indices[corner]].position;
}

void emit_mode(std::ostream &output, const char *name, const ModeResult &result, bool trailing) {
  output << "    \"" << name << "\": {\"passed\": " << (result.passed ? "true" : "false")
         << ", \"qualifying_fragments\": " << result.qualifying_fragments
         << ", \"exact_ray_hit\": " << (result.ray_hit ? "true" : "false")
         << ", \"exact_ray_primitive\": ";
  if (result.ray_primitive == UINT32_MAX) {
    output << "null";
  } else {
    output << result.ray_primitive;
  }
  output << "}" << (trailing ? ",\n" : "\n");
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: tetcage_boundary_probe <asset.tetcage> <output.json>\n";
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value || loaded.value->source.triangles.size() < 3U) {
    std::cerr << (loaded.value ? "boundary probe requires three source primitives" : loaded.error)
              << '\n';
    return EXIT_FAILURE;
  }
  const auto &asset = *loaded.value;
  const auto bvh = tetcage::build_bvh4d(asset, 4U);
  std::set<std::uint32_t> crossing_tets;
  std::uint64_t on_face_fragments = 0U;
  std::uint64_t on_edge_fragments = 0U;
  std::uint64_t at_vertex_fragments = 0U;
  for (const auto &fragment : asset.micro_triangles) {
    if (fragment.source_primitive == 0U) {
      crossing_tets.insert(fragment.tet_id);
    }
    bool on_shared_face = false;
    bool on_shared_edge = false;
    bool at_shared_vertex = false;
    for (const auto vertex_index : fragment.vertex_indices) {
      if (vertex_index >= asset.generated_vertices.size()) {
        continue;
      }
      const auto &vertex = asset.generated_vertices[vertex_index];
      if (fragment.source_primitive == 1U && has_shared_face(asset, vertex)) {
        on_shared_face = true;
      }
      if (fragment.source_primitive == 2U && popcount(vertex.feature.cage_boundary_mask) >= 2U &&
          has_shared_face(asset, vertex)) {
        on_shared_edge = true;
      }
      if (fragment.source_primitive == 2U && popcount(vertex.feature.cage_boundary_mask) >= 3U &&
          popcount(vertex.feature.source_boundary_mask) >= 2U) {
        at_shared_vertex = true;
      }
    }
    on_face_fragments += on_shared_face ? 1U : 0U;
    on_edge_fragments += on_shared_edge ? 1U : 0U;
    at_vertex_fragments += at_shared_vertex ? 1U : 0U;
  }
  const auto cross_a = source_position(asset, 0U, 0U);
  const auto cross_b = source_position(asset, 0U, 1U);
  const auto cross_c = source_position(asset, 0U, 2U);
  const auto face_a = source_position(asset, 1U, 0U);
  const auto face_b = source_position(asset, 1U, 1U);
  const auto face_c = source_position(asset, 1U, 2U);
  const auto edge_a = source_position(asset, 2U, 0U);
  const auto edge_b = source_position(asset, 2U, 1U);
  const auto vertex = source_position(asset, 2U, 0U);
  const auto cross = measure_ray(asset, bvh, (cross_a + cross_b + cross_c) / 3.0, 0U,
                                 crossing_tets.size() >= 2U ? crossing_tets.size() : 0U);
  const auto face =
      measure_ray(asset, bvh, (face_a + face_b + face_c) / 3.0, 1U, on_face_fragments);
  const auto edge = measure_ray(asset, bvh, (edge_a + edge_b) / 2.0, 2U, on_edge_fragments);
  const auto vertex_result = measure_ray(asset, bvh, vertex, 2U, at_vertex_fragments);
  std::ofstream output(argv[2], std::ios::trunc);
  if (!output) {
    std::cerr << argv[2] << ": cannot create report\n";
    return EXIT_FAILURE;
  }
  output << "{\n  \"schema_version\": 1,\n  \"report_kind\": \"shared_boundary_probe\",\n"
         << "  \"modes\": {\n";
  emit_mode(output, "cross_shared_face", cross, true);
  emit_mode(output, "on_shared_face", face, true);
  emit_mode(output, "on_shared_edge", edge, true);
  emit_mode(output, "at_shared_vertex", vertex_result, false);
  output << "  }\n}\n";
  if (!output) {
    std::cerr << argv[2] << ": cannot write report\n";
    return EXIT_FAILURE;
  }
  output.close();
  std::ifstream result(argv[2]);
  std::cout << result.rdbuf();
  return cross.passed && face.passed && edge.passed && vertex_result.passed ? EXIT_SUCCESS
                                                                            : EXIT_FAILURE;
}
