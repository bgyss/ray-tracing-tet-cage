#include "tetcage/cycles_bridge.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace tetcage {
namespace {

bool finite(Vec3 value) {
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool same_topology(const CompiledAsset &asset, const Cage &posed_cage) {
  if (asset.format_version != asset_format_version ||
      asset.cage.vertices.size() != posed_cage.vertices.size() ||
      asset.cage.vertex_ids != posed_cage.vertex_ids ||
      asset.cage.tetrahedra.size() != posed_cage.tetrahedra.size() ||
      asset.tet_metadata.size() != posed_cage.tetrahedra.size()) {
    return false;
  }
  for (std::size_t index = 0; index < posed_cage.tetrahedra.size(); ++index) {
    if (asset.cage.tetrahedra[index].vertex_indices !=
        posed_cage.tetrahedra[index].vertex_indices) {
      return false;
    }
  }
  return std::all_of(posed_cage.vertices.begin(), posed_cage.vertices.end(), finite);
}

Tetrahedron posed_tet(const Cage &cage, std::size_t index) {
  Tetrahedron tet{};
  for (std::size_t corner = 0; corner < tet.positions.size(); ++corner) {
    const auto vertex_index = cage.tetrahedra[index].vertex_indices[corner];
    tet.positions[corner] = cage.vertices[vertex_index];
    tet.vertex_ids[corner] = cage.vertex_ids[vertex_index];
  }
  return tet;
}

void set_fallback(CyclesTetCageFrame &frame, CyclesFallbackReason reason) {
  frame.mode = CyclesGeometryMode::conventional_mesh;
  frame.fallback_reason = reason;
  frame.tet_transforms.clear();
  frame.primitives.clear();
  frame.micro_triangle_indices.clear();
}

bool source_primitive_exists(const CompiledAsset &asset, std::uint32_t primitive) {
  return std::any_of(
      asset.source.triangles.begin(), asset.source.triangles.end(),
      [primitive](const SourceTriangle &triangle) { return triangle.primitive_id == primitive; });
}

bool valid_micro_triangle(const CompiledAsset &asset, const MicroTriangle &triangle) {
  if (triangle.tet_id >= asset.cage.tetrahedra.size() ||
      triangle.owner_tet >= asset.cage.tetrahedra.size() ||
      !source_primitive_exists(asset, triangle.source_primitive)) {
    return false;
  }
  return std::all_of(
      triangle.vertex_indices.begin(), triangle.vertex_indices.end(),
      [&asset](std::uint32_t index) { return index < asset.generated_vertices.size(); });
}

} // namespace

const char *cycles_geometry_mode_name(CyclesGeometryMode mode) {
  switch (mode) {
  case CyclesGeometryMode::conventional_mesh:
    return "conventional_mesh";
  case CyclesGeometryMode::procedural_metalrt:
    return "procedural_metalrt";
  }
  return "conventional_mesh";
}

const char *cycles_fallback_reason_name(CyclesFallbackReason reason) {
  switch (reason) {
  case CyclesFallbackReason::none:
    return "none";
  case CyclesFallbackReason::invalid_asset:
    return "invalid_asset";
  case CyclesFallbackReason::invalid_pose:
    return "invalid_pose";
  case CyclesFallbackReason::unsupported_device:
    return "unsupported_device";
  case CyclesFallbackReason::capacity_rejected:
    return "capacity_rejected";
  case CyclesFallbackReason::update_failed:
    return "update_failed";
  }
  return "invalid_asset";
}

CyclesTetCageFrame build_cycles_tet_cage_frame(const CompiledAsset &asset, const Cage &posed_cage,
                                               std::uint64_t pose_generation) {
  return build_cycles_tet_cage_frame(asset, posed_cage, pose_generation,
                                     asset_checksum(serialize_asset(asset)));
}

CyclesTetCageFrame build_cycles_tet_cage_frame(const CompiledAsset &asset, const Cage &posed_cage,
                                               std::uint64_t pose_generation,
                                               std::uint64_t checksum) {
  CyclesTetCageFrame frame{};
  frame.pose_generation = pose_generation;
  frame.asset_checksum = checksum;
  if (!same_topology(asset, posed_cage)) {
    set_fallback(frame, CyclesFallbackReason::invalid_asset);
    return frame;
  }

  frame.tet_transforms.reserve(posed_cage.tetrahedra.size());
  for (std::size_t tet_index = 0; tet_index < posed_cage.tetrahedra.size(); ++tet_index) {
    const auto &metadata = asset.tet_metadata[tet_index];
    const auto pose_tet = posed_tet(posed_cage, tet_index);
    const auto diagnostics = diagnose(pose_tet);
    if (metadata.mirrored || metadata.near_singular ||
        diagnostics.classification != TetClass::healthy) {
      set_fallback(frame, CyclesFallbackReason::invalid_pose);
      return frame;
    }
    const auto transform = canonical_to_object(pose_tet);
    if (!transform) {
      set_fallback(frame, CyclesFallbackReason::invalid_pose);
      return frame;
    }
    frame.tet_transforms.push_back(*transform);
  }

  std::vector<std::vector<std::uint32_t>> by_tet(posed_cage.tetrahedra.size());
  for (std::uint32_t micro_index = 0;
       micro_index < static_cast<std::uint32_t>(asset.micro_triangles.size()); ++micro_index) {
    const auto &triangle = asset.micro_triangles[micro_index];
    if (!valid_micro_triangle(asset, triangle)) {
      set_fallback(frame, CyclesFallbackReason::invalid_asset);
      return frame;
    }
    by_tet[triangle.tet_id].push_back(micro_index);
  }

  for (std::uint32_t tet_id = 0; tet_id < by_tet.size(); ++tet_id) {
    const auto &indices = by_tet[tet_id];
    if (indices.empty()) {
      continue;
    }
    CyclesTetPrimitive primitive{};
    primitive.tet_id = tet_id;
    primitive.micro_triangle_begin =
        static_cast<std::uint32_t>(frame.micro_triangle_indices.size());
    primitive.micro_triangle_count = static_cast<std::uint32_t>(indices.size());
    primitive.bounds_min = {std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity(),
                            std::numeric_limits<double>::infinity()};
    primitive.bounds_max = {-std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity(),
                            -std::numeric_limits<double>::infinity()};
    const auto pose = posed_tet(posed_cage, tet_id);
    for (const auto micro_index : indices) {
      frame.micro_triangle_indices.push_back(micro_index);
      const auto &triangle = asset.micro_triangles[micro_index];
      for (const auto vertex_index : triangle.vertex_indices) {
        const auto point =
            from_barycentric(pose, asset.generated_vertices[vertex_index].cage_barycentric);
        primitive.bounds_min.x = std::min(primitive.bounds_min.x, point.x);
        primitive.bounds_min.y = std::min(primitive.bounds_min.y, point.y);
        primitive.bounds_min.z = std::min(primitive.bounds_min.z, point.z);
        primitive.bounds_max.x = std::max(primitive.bounds_max.x, point.x);
        primitive.bounds_max.y = std::max(primitive.bounds_max.y, point.y);
        primitive.bounds_max.z = std::max(primitive.bounds_max.z, point.z);
      }
    }
    const double scale =
        std::max({1.0, std::abs(primitive.bounds_min.x), std::abs(primitive.bounds_min.y),
                  std::abs(primitive.bounds_min.z), std::abs(primitive.bounds_max.x),
                  std::abs(primitive.bounds_max.y), std::abs(primitive.bounds_max.z)});
    const double margin = 32.0 * std::numeric_limits<double>::epsilon() * scale;
    primitive.bounds_min = primitive.bounds_min - Vec3{margin, margin, margin};
    primitive.bounds_max = primitive.bounds_max + Vec3{margin, margin, margin};
    frame.primitives.push_back(primitive);
  }

  if (frame.primitives.empty()) {
    set_fallback(frame, CyclesFallbackReason::invalid_asset);
    return frame;
  }
  frame.mode = CyclesGeometryMode::procedural_metalrt;
  frame.fallback_reason = CyclesFallbackReason::none;
  return frame;
}

std::optional<CyclesNormalizedHit> normalize_cycles_metal_hit(const CompiledAsset &asset,
                                                              const CyclesMetalHit &hit) {
  constexpr double barycentric_tolerance = 1.0e-12;
  if (hit.tet_id >= asset.cage.tetrahedra.size() ||
      hit.primitive_id >= asset.micro_triangles.size() || !std::isfinite(hit.distance) ||
      !std::isfinite(hit.u) || !std::isfinite(hit.v) || hit.u < -barycentric_tolerance ||
      hit.v < -barycentric_tolerance || hit.u + hit.v > 1.0 + barycentric_tolerance) {
    return std::nullopt;
  }
  const auto &triangle = asset.micro_triangles[hit.primitive_id];
  if (!valid_micro_triangle(asset, triangle) || triangle.tet_id != hit.tet_id) {
    return std::nullopt;
  }
  const auto &a = asset.generated_vertices[triangle.vertex_indices[0]];
  const auto &b = asset.generated_vertices[triangle.vertex_indices[1]];
  const auto &c = asset.generated_vertices[triangle.vertex_indices[2]];
  const auto source_barycentric = a.source_barycentric * (1.0 - hit.u - hit.v) +
                                  b.source_barycentric * hit.u + c.source_barycentric * hit.v;
  if (!finite(source_barycentric)) {
    return std::nullopt;
  }
  return CyclesNormalizedHit{
      hit.object_id, triangle.source_primitive, triangle.owner_tet,  source_barycentric,
      hit.distance,  source_barycentric.y,      source_barycentric.z};
}

} // namespace tetcage
