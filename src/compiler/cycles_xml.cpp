#include "tetcage/io.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace tetcage {
namespace {

Tetrahedron asset_tet(const CompiledAsset &asset, std::uint32_t tet_id) {
  Tetrahedron tet{};
  const auto &indices = asset.cage.tetrahedra[tet_id].vertex_indices;
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    tet.positions[corner] = asset.cage.vertices[indices[corner]];
    tet.vertex_ids[corner] = asset.cage.vertex_ids[indices[corner]];
  }
  return tet;
}

bool valid_micro_triangle(const CompiledAsset &asset, const MicroTriangle &triangle) {
  if (triangle.tet_id >= asset.cage.tetrahedra.size() ||
      triangle.owner_tet >= asset.cage.tetrahedra.size()) {
    return false;
  }
  return std::all_of(triangle.vertex_indices.begin(), triangle.vertex_indices.end(),
                     [&asset](std::uint32_t index) {
                       return index < asset.generated_vertices.size();
                     });
}

} // namespace

std::string write_cycles_xml(const std::string &path, const CompiledAsset &asset) {
  if (asset.cage.vertices.empty() || asset.cage.tetrahedra.empty() ||
      asset.micro_triangles.empty()) {
    return "asset has no compiled microgeometry";
  }
  std::ofstream output(path);
  if (!output) {
    return path + ": cannot create Cycles XML scene";
  }

  std::map<std::uint32_t, std::vector<const MicroTriangle *>> by_tet;
  for (const auto &triangle : asset.micro_triangles) {
    if (!valid_micro_triangle(asset, triangle)) {
      return "asset contains an invalid micro-triangle reference";
    }
    by_tet[triangle.tet_id].push_back(&triangle);
  }

  output << std::setprecision(17) << "<?xml version=\"1.0\" ?>\n<cycles>\n"
         << "<!-- tet_cage_fallback: ordinary Cycles triangles; no procedural primitive claim -->\n"
         << "<camera width=\"64\" height=\"64\" />\n"
         << "<transform translate=\"0 0 0\"><camera type=\"perspective\" fov=\"0.8\" />"
            "</transform>\n"
         << "<shader name=\"tet_cage_fallback\">\n"
         << "  <emission name=\"tet_cage_emission\" color=\"0.7 0.85 1.0\" strength=\"1.0\" />\n"
         << "  <connect from=\"tet_cage_emission emission\" to=\"output surface\" />\n"
         << "</shader>\n";

  for (const auto &[tet_id, triangles] : by_tet) {
    const auto tet = asset_tet(asset, tet_id);
    output << "<!-- tet_id=" << tet_id << " micro_triangles=" << triangles.size() << " -->\n"
           << "<state shader=\"tet_cage_fallback\">\n  <mesh P=\"";
    for (const auto *triangle : triangles) {
      for (const auto vertex_index : triangle->vertex_indices) {
        const auto point = from_barycentric(
            tet, asset.generated_vertices[vertex_index].cage_barycentric);
        output << point.x << ' ' << point.y << ' ' << point.z - 3.0 << ' ';
      }
    }
    output << "\" nverts=\"";
    for (std::size_t triangle = 0; triangle < triangles.size(); ++triangle) {
      output << (triangle == 0U ? "" : " ") << '3';
    }
    output << "\" verts=\"";
    for (std::size_t vertex = 0; vertex < triangles.size() * 3U; ++vertex) {
      output << (vertex == 0U ? "" : " ") << vertex;
    }
    output << "\" />\n</state>\n";
  }

  output << "</cycles>\n";
  if (!output) {
    return path + ": failed while writing Cycles XML scene";
  }
  return {};
}

} // namespace tetcage
