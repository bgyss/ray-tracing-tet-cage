#include "tetcage/authoring.h"

#include "tetcage/io.h"
#include "tetcage/runtime.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

namespace tetcage {
namespace {

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
