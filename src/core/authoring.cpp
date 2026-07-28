#include "tetcage/authoring.h"

#include "tetcage/io.h"
#include "tetcage/runtime.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace tetcage {
namespace {

using EdgeKey = std::array<std::uint64_t, 2>;

EdgeKey edge_key(const Cage &cage, std::uint32_t first, std::uint32_t second) {
  EdgeKey result{cage.vertex_ids[first], cage.vertex_ids[second]};
  if (result[1] < result[0]) {
    std::swap(result[0], result[1]);
  }
  return result;
}

std::uint64_t midpoint_id(EdgeKey edge, std::set<std::uint64_t> &used) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (const auto word : edge) {
    hash ^= word;
    hash *= 1099511628211ULL;
  }
  hash ^= 0x6d6964706f696e74ULL;
  hash *= 1099511628211ULL;
  if (hash == 0U) {
    hash = 1U;
  }
  while (used.contains(hash)) {
    hash = hash * 1099511628211ULL + 1U;
    if (hash == 0U) {
      hash = 1U;
    }
  }
  used.insert(hash);
  return hash;
}

Tetrahedron cage_tet(const Cage &cage, CageTet tet) {
  Tetrahedron result{};
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    result.positions[corner] = cage.vertices[tet.vertex_indices[corner]];
    result.vertex_ids[corner] = cage.vertex_ids[tet.vertex_indices[corner]];
  }
  return result;
}

CageTet orient_like(const Cage &cage, CageTet candidate, double reference_determinant) {
  const double candidate_determinant = diagnose(cage_tet(cage, candidate)).determinant;
  if (std::isfinite(candidate_determinant) && reference_determinant * candidate_determinant < 0.0) {
    std::swap(candidate.vertex_indices[2], candidate.vertex_indices[3]);
  }
  return candidate;
}

std::vector<Vec3> sample_pose(const CompiledAsset &asset, std::uint32_t frame, double amplitude) {
  std::vector<Vec3> pose = asset.cage.vertices;
  const double phase = static_cast<double>(frame) * 0.37;
  for (std::size_t index = 0; index < pose.size(); ++index) {
    const double weight = static_cast<double>(index + 1U);
    pose[index].x += amplitude * 0.25 * std::cos(phase + weight * 0.17);
    pose[index].y += amplitude * 0.5 * std::sin(phase + weight * 0.23);
    pose[index].z += amplitude * std::sin(phase + weight * 0.31);
  }
  return pose;
}

Tetrahedron posed_tet(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                      std::uint32_t tet_id) {
  Tetrahedron result{};
  const auto &source = asset.cage.tetrahedra[tet_id];
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    result.positions[corner] = pose[source.vertex_indices[corner]];
    result.vertex_ids[corner] = asset.cage.vertex_ids[source.vertex_indices[corner]];
  }
  return result;
}

std::string json_escape(const std::string &value) {
  std::ostringstream output;
  for (const char character : value) {
    if (character == '\\' || character == '"') {
      output << '\\';
    }
    output << character;
  }
  return output.str();
}

} // namespace

CageRefinementResult refine_cage(const Cage &input, std::uint32_t levels) {
  CageRefinementResult result{};
  if (input.vertices.size() != input.vertex_ids.size() || input.vertices.empty() ||
      input.tetrahedra.empty()) {
    result.error = "cage refinement requires vertices, stable IDs, and tetrahedra";
    return result;
  }
  std::set<std::uint64_t> stable_ids(input.vertex_ids.begin(), input.vertex_ids.end());
  if (stable_ids.size() != input.vertex_ids.size()) {
    result.error = "cage refinement requires globally unique stable IDs";
    return result;
  }
  for (const auto &vertex : input.vertices) {
    if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y) || !std::isfinite(vertex.z)) {
      result.error = "cage refinement rejects non-finite vertex positions";
      return result;
    }
  }
  Cage current = input;
  for (std::uint32_t level = 0; level < levels; ++level) {
    if (current.tetrahedra.size() > 125'000U) {
      result.error = "cage refinement would exceed the one-million-tetrahedron safety limit";
      return result;
    }
    std::map<EdgeKey, std::array<std::uint32_t, 2>> edge_vertices;
    for (const auto &tet : current.tetrahedra) {
      for (std::size_t first = 0; first < 4U; ++first) {
        for (std::size_t second = first + 1U; second < 4U; ++second) {
          const auto key = edge_key(current, tet.vertex_indices[first], tet.vertex_indices[second]);
          edge_vertices.emplace(key, std::array<std::uint32_t, 2>{tet.vertex_indices[first],
                                                                  tet.vertex_indices[second]});
        }
      }
    }
    Cage next = current;
    std::map<EdgeKey, std::uint32_t> midpoint_indices;
    for (const auto &[key, endpoints] : edge_vertices) {
      const auto index = static_cast<std::uint32_t>(next.vertices.size());
      next.vertices.push_back((current.vertices[endpoints[0]] + current.vertices[endpoints[1]]) *
                              0.5);
      next.vertex_ids.push_back(midpoint_id(key, stable_ids));
      midpoint_indices.emplace(key, index);
    }
    next.tetrahedra.clear();
    next.tetrahedra.reserve(current.tetrahedra.size() * 8U);
    for (const auto &tet : current.tetrahedra) {
      const auto original_determinant = diagnose(cage_tet(current, tet)).determinant;
      const auto midpoint = [&](std::size_t first, std::size_t second) {
        return midpoint_indices.at(
            edge_key(current, tet.vertex_indices[first], tet.vertex_indices[second]));
      };
      const auto a = tet.vertex_indices[0];
      const auto b = tet.vertex_indices[1];
      const auto c = tet.vertex_indices[2];
      const auto d = tet.vertex_indices[3];
      const auto ab = midpoint(0U, 1U);
      const auto ac = midpoint(0U, 2U);
      const auto ad = midpoint(0U, 3U);
      const auto bc = midpoint(1U, 2U);
      const auto bd = midpoint(1U, 3U);
      const auto cd = midpoint(2U, 3U);
      const std::array<CageTet, 8> candidates{{
          {{a, ab, ac, ad}},
          {{b, ab, bc, bd}},
          {{c, ac, bc, cd}},
          {{d, ad, bd, cd}},
          {{ab, ac, ad, cd}},
          {{ab, ac, bc, cd}},
          {{ab, ad, bd, cd}},
          {{ab, bc, bd, cd}},
      }};
      for (const auto candidate : candidates) {
        next.tetrahedra.push_back(orient_like(next, candidate, original_determinant));
      }
    }
    current = std::move(next);
  }
  result.cage = std::move(current);
  return result;
}

CageQualityReport analyze_cage_quality(const CompiledAsset &asset, std::uint32_t samples,
                                       double motion_amplitude) {
  CageQualityReport report{};
  report.asset_hash = asset_checksum(serialize_asset(asset));
  report.samples = std::max(samples, 1U);
  report.tetrahedra = asset.cage.tetrahedra.size();
  report.occupied_tetrahedra = asset.statistics.occupied_tetrahedra;
  report.boundary_fragments = asset.statistics.boundary_fragments;
  report.minimum_edge = std::numeric_limits<double>::infinity();
  report.frames.reserve(report.samples);

  for (std::uint32_t frame = 0; frame < report.samples; ++frame) {
    const auto pose = sample_pose(asset, frame, motion_amplitude);
    CageQualityFrame frame_report{};
    frame_report.frame = frame;
    frame_report.minimum_edge = std::numeric_limits<double>::infinity();
    for (std::uint32_t tet_id = 0; tet_id < asset.cage.tetrahedra.size(); ++tet_id) {
      const auto tet = posed_tet(asset, pose, tet_id);
      const auto diagnostics = diagnose(tet);
      frame_report.minimum_edge = std::min(frame_report.minimum_edge, diagnostics.minimum_edge);
      frame_report.maximum_condition =
          std::max(frame_report.maximum_condition, diagnostics.condition_estimate);
      report.minimum_edge = std::min(report.minimum_edge, diagnostics.minimum_edge);
      report.worst_condition = std::max(report.worst_condition, diagnostics.condition_estimate);
      if (diagnostics.classification == TetClass::near_singular) {
        ++frame_report.near_singular_tetrahedra;
      } else if (diagnostics.classification == TetClass::mirrored) {
        ++frame_report.mirrored_tetrahedra;
      }
    }

    const auto transforms = build_tet_transforms(asset, {pose}, 0U, 0U);
    if (!transforms.error.empty()) {
      ++frame_report.near_singular_tetrahedra;
    } else {
      for (const auto &vertex : asset.generated_vertices) {
        if (vertex.tet_id >= transforms.transforms.size()) {
          frame_report.maximum_affine_residual = std::numeric_limits<double>::infinity();
          continue;
        }
        const auto &tet = posed_tet(asset, pose, vertex.tet_id);
        const auto direct = from_barycentric(tet, vertex.cage_barycentric);
        const Vec3 canonical{vertex.cage_barycentric.y, vertex.cage_barycentric.z,
                             vertex.cage_barycentric.w};
        const auto matrix = transforms.transforms[vertex.tet_id].object_from_canonical;
        const auto transformed = matrix.apply_point(canonical);
        frame_report.maximum_affine_residual =
            std::max(frame_report.maximum_affine_residual, length(direct - transformed));
      }
    }
    report.maximum_affine_residual =
        std::max(report.maximum_affine_residual, frame_report.maximum_affine_residual);
    report.frames.push_back(frame_report);
  }

  if (report.minimum_edge == std::numeric_limits<double>::infinity()) {
    report.minimum_edge = 0.0;
  }
  if (report.worst_condition > 1.0e8) {
    report.fallback_reasons.emplace_back("tetrahedron_condition_exceeds_1e8");
  }
  for (const auto &frame : report.frames) {
    if (frame.near_singular_tetrahedra != 0U) {
      report.fallback_reasons.emplace_back("near_singular_pose_sample");
      break;
    }
  }
  for (const auto &frame : report.frames) {
    if (frame.mirrored_tetrahedra != 0U) {
      report.fallback_reasons.emplace_back("mirrored_pose_requires_explicit_policy");
      break;
    }
  }
  if (report.maximum_affine_residual > 1.0e-8) {
    report.fallback_reasons.emplace_back("affine_barycentric_residual_exceeds_1e-8");
  }
  report.suitable = report.fallback_reasons.empty();
  return report;
}

std::string cage_quality_json(const CageQualityReport &report) {
  std::ostringstream output;
  output << std::setprecision(17) << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"asset_hash\": \"" << std::hex << report.asset_hash << std::dec << "\",\n"
         << "  \"samples\": " << report.samples << ",\n"
         << "  \"tetrahedra\": " << report.tetrahedra << ",\n"
         << "  \"occupied_tetrahedra\": " << report.occupied_tetrahedra << ",\n"
         << "  \"boundary_fragments\": " << report.boundary_fragments << ",\n"
         << "  \"worst_condition\": " << report.worst_condition << ",\n"
         << "  \"minimum_edge\": " << report.minimum_edge << ",\n"
         << "  \"maximum_affine_residual\": " << report.maximum_affine_residual << ",\n"
         << "  \"suitable\": " << (report.suitable ? "true" : "false") << ",\n"
         << "  \"frames\": [";
  for (std::size_t index = 0; index < report.frames.size(); ++index) {
    const auto &frame = report.frames[index];
    output << "{\"frame\": " << frame.frame
           << ", \"near_singular_tetrahedra\": " << frame.near_singular_tetrahedra
           << ", \"mirrored_tetrahedra\": " << frame.mirrored_tetrahedra
           << ", \"minimum_edge\": " << frame.minimum_edge
           << ", \"maximum_condition\": " << frame.maximum_condition
           << ", \"maximum_affine_residual\": " << frame.maximum_affine_residual << "}";
    if (index + 1U != report.frames.size()) {
      output << ", ";
    }
  }
  output << "],\n  \"fallback_reasons\": [";
  for (std::size_t index = 0; index < report.fallback_reasons.size(); ++index) {
    output << "\"" << json_escape(report.fallback_reasons[index]) << "\"";
    if (index + 1U != report.fallback_reasons.size()) {
      output << ", ";
    }
  }
  output << "]\n}\n";
  return output.str();
}

} // namespace tetcage
