#include "tetcage/asset_format.h"
#include "tetcage/io.h"
#include "tetcage/oracle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>
#include <string>

namespace {

struct ModeResult {
  bool passed{};
  std::uint64_t qualifying_fragments{};
  bool ray_hit{};
  std::uint32_t ray_primitive{};
};

using Edge = std::array<std::uint64_t, 2>;

bool shares_face(const tetcage::CompiledAsset &asset, std::uint32_t first, std::uint32_t second) {
  if (first >= asset.tet_metadata.size() || second >= asset.tet_metadata.size()) {
    return false;
  }
  for (std::uint32_t face = 0U; face < 4U; ++face) {
    if (asset.tet_metadata[first].adjacent_tet[face] == static_cast<std::int32_t>(second)) {
      return true;
    }
  }
  return false;
}

bool is_source_corner(const tetcage::GeneratedVertex &vertex, std::uint32_t corner) {
  constexpr double tolerance = 1.0e-12;
  const std::array<double, 3> source{vertex.source_barycentric.x, vertex.source_barycentric.y,
                                     vertex.source_barycentric.z};
  for (std::uint32_t index = 0U; index < source.size(); ++index) {
    if (std::abs(source[index] - (index == corner ? 1.0 : 0.0)) > tolerance) {
      return false;
    }
  }
  return true;
}

std::set<Edge> edges_at_vertex(const tetcage::CompiledAsset &asset,
                               const tetcage::GeneratedVertex &vertex) {
  std::set<Edge> edges;
  if (vertex.tet_id >= asset.cage.tetrahedra.size()) {
    return edges;
  }
  const auto &tet = asset.cage.tetrahedra[vertex.tet_id];
  for (std::uint32_t first_face = 0U; first_face < 4U; ++first_face) {
    if ((vertex.feature.cage_boundary_mask & (1U << first_face)) == 0U) {
      continue;
    }
    for (std::uint32_t second_face = first_face + 1U; second_face < 4U; ++second_face) {
      if ((vertex.feature.cage_boundary_mask & (1U << second_face)) == 0U) {
        continue;
      }
      Edge edge{};
      std::size_t output = 0U;
      for (std::uint32_t corner = 0U; corner < 4U; ++corner) {
        if (corner != first_face && corner != second_face) {
          edge[output++] = asset.cage.vertex_ids[tet.vertex_indices[corner]];
        }
      }
      std::sort(edge.begin(), edge.end());
      edges.insert(edge);
    }
  }
  return edges;
}

std::set<std::uint64_t> cage_vertices_at_source_corner(const tetcage::CompiledAsset &asset,
                                                       std::uint32_t primitive,
                                                       std::uint32_t corner) {
  std::set<std::uint64_t> result;
  for (const auto &vertex : asset.generated_vertices) {
    if (vertex.source_primitive != primitive || !is_source_corner(vertex, corner) ||
        vertex.tet_id >= asset.cage.tetrahedra.size()) {
      continue;
    }
    const auto &tet = asset.cage.tetrahedra[vertex.tet_id];
    for (std::uint32_t tet_corner = 0U; tet_corner < 4U; ++tet_corner) {
      if ((vertex.feature.cage_boundary_mask & (1U << tet_corner)) == 0U) {
        result.insert(asset.cage.vertex_ids[tet.vertex_indices[tet_corner]]);
      }
    }
  }
  return result;
}

std::set<Edge> shared_edges_at_source_corner(const tetcage::CompiledAsset &asset,
                                             std::uint32_t primitive, std::uint32_t corner) {
  std::set<Edge> result;
  for (const auto &vertex : asset.generated_vertices) {
    if (vertex.source_primitive == primitive && is_source_corner(vertex, corner)) {
      for (const auto &edge : edges_at_vertex(asset, vertex)) {
        std::uint32_t containing_tets = 0U;
        for (const auto &tet : asset.cage.tetrahedra) {
          bool has_first = false;
          bool has_second = false;
          for (const auto tet_corner : tet.vertex_indices) {
            const auto id = asset.cage.vertex_ids[tet_corner];
            has_first = has_first || id == edge[0];
            has_second = has_second || id == edge[1];
          }
          containing_tets += has_first && has_second ? 1U : 0U;
        }
        if (containing_tets >= 2U) {
          result.insert(edge);
        }
      }
    }
  }
  return result;
}

bool is_shared_cage_vertex(const tetcage::CompiledAsset &asset, std::uint64_t vertex_id) {
  std::uint32_t containing_tets = 0U;
  for (const auto &tet : asset.cage.tetrahedra) {
    for (const auto corner : tet.vertex_indices) {
      if (asset.cage.vertex_ids[corner] == vertex_id) {
        ++containing_tets;
        break;
      }
    }
  }
  return containing_tets >= 2U;
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
  for (const auto &fragment : asset.micro_triangles) {
    if (fragment.source_primitive == 0U) {
      crossing_tets.insert(fragment.tet_id);
    }
    if (fragment.source_primitive == 1U && fragment.tet_id < asset.tet_metadata.size()) {
      for (std::uint32_t face = 0U; face < 4U; ++face) {
        if (asset.tet_metadata[fragment.tet_id].adjacent_tet[face] < 0) {
          continue;
        }
        bool all_on_face = true;
        for (const auto vertex_index : fragment.vertex_indices) {
          if (vertex_index >= asset.generated_vertices.size() ||
              asset.generated_vertices[vertex_index].tet_id != fragment.tet_id ||
              (asset.generated_vertices[vertex_index].feature.cage_boundary_mask & (1U << face)) ==
                  0U) {
            all_on_face = false;
            break;
          }
        }
        if (all_on_face) {
          ++on_face_fragments;
          break;
        }
      }
    }
  }
  std::uint64_t shared_cross_faces = 0U;
  for (auto first = crossing_tets.begin(); first != crossing_tets.end(); ++first) {
    for (auto second = std::next(first); second != crossing_tets.end(); ++second) {
      shared_cross_faces += shares_face(asset, *first, *second) ? 1U : 0U;
    }
  }
  const auto first_edge = shared_edges_at_source_corner(asset, 2U, 0U);
  const auto second_edge = shared_edges_at_source_corner(asset, 2U, 1U);
  std::set<Edge> common_edges;
  std::set_intersection(first_edge.begin(), first_edge.end(), second_edge.begin(),
                        second_edge.end(), std::inserter(common_edges, common_edges.begin()));
  const auto source_vertex_ids = cage_vertices_at_source_corner(asset, 2U, 0U);
  const auto shared_vertex_count = std::count_if(
      source_vertex_ids.begin(), source_vertex_ids.end(),
      [&](std::uint64_t vertex_id) { return is_shared_cage_vertex(asset, vertex_id); });
  const auto cross_a = source_position(asset, 0U, 0U);
  const auto cross_b = source_position(asset, 0U, 1U);
  const auto cross_c = source_position(asset, 0U, 2U);
  const auto face_a = source_position(asset, 1U, 0U);
  const auto face_b = source_position(asset, 1U, 1U);
  const auto face_c = source_position(asset, 1U, 2U);
  const auto edge_a = source_position(asset, 2U, 0U);
  const auto edge_b = source_position(asset, 2U, 1U);
  const auto vertex = source_position(asset, 2U, 0U);
  const auto cross =
      measure_ray(asset, bvh, (cross_a + cross_b + cross_c) / 3.0, 0U, shared_cross_faces);
  const auto face =
      measure_ray(asset, bvh, (face_a + face_b + face_c) / 3.0, 1U, on_face_fragments);
  const auto edge = measure_ray(asset, bvh, (edge_a + edge_b) / 2.0, 2U, common_edges.size());
  const auto vertex_result =
      measure_ray(asset, bvh, vertex, 2U, static_cast<std::uint64_t>(shared_vertex_count));
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
