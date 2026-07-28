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

Vec3 procedural_clip_delta(Vec3 position, std::uint32_t frame, double amplitude) {
  const double scale = std::max(1.0, length(position));
  const Vec3 normalized_position = position / scale;
  const double phase = static_cast<double>(frame) * 0.37;
  return {
      amplitude *
          (0.10 * std::sin(phase + 2.7 * normalized_position.x + 0.9 * normalized_position.y) +
           0.025 * std::cos(phase * 1.7 + normalized_position.z * 3.1)),
      amplitude * (0.08 * std::cos(phase * 0.8 + normalized_position.y * 2.1 +
                                   normalized_position.z * 1.3) +
                   0.02 * std::sin(phase + normalized_position.x * 4.0)),
      amplitude * (0.07 * std::sin(phase * 1.2 + normalized_position.z * 2.5 +
                                   normalized_position.x * 1.1) +
                   0.02 * std::cos(phase * 0.6 + normalized_position.y * 3.7)),
  };
}

Vec3 procedural_clip_position(Vec3 position, std::uint32_t frame, double amplitude) {
  return position + procedural_clip_delta(position, frame, amplitude);
}

std::vector<Vec3> procedural_clip_pose(const CompiledAsset &asset, std::uint32_t frame,
                                       double amplitude) {
  std::vector<Vec3> pose;
  pose.reserve(asset.cage.vertices.size());
  for (const auto &vertex : asset.cage.vertices) {
    pose.push_back(procedural_clip_position(vertex, frame, amplitude));
  }
  return pose;
}

struct SurfaceSample {
  Vec3 position{};
  Vec3 normal{};
  GeneratedVertex generated{};
};

std::vector<SurfaceSample> collect_surface_samples(const CompiledAsset &asset,
                                                   std::uint64_t &uncovered) {
  std::vector<SurfaceSample> samples;
  samples.reserve(asset.generated_vertices.size());
  uncovered = 0U;
  for (const auto &generated : asset.generated_vertices) {
    const auto triangle_iterator =
        std::find_if(asset.source.triangles.begin(), asset.source.triangles.end(),
                     [&](const SourceTriangle &triangle) {
                       return triangle.primitive_id == generated.source_primitive;
                     });
    if (triangle_iterator == asset.source.triangles.end() ||
        generated.tet_id >= asset.cage.tetrahedra.size()) {
      ++uncovered;
      continue;
    }
    const auto &triangle = *triangle_iterator;
    if (triangle.vertex_indices[0] >= asset.source.vertices.size() ||
        triangle.vertex_indices[1] >= asset.source.vertices.size() ||
        triangle.vertex_indices[2] >= asset.source.vertices.size()) {
      ++uncovered;
      continue;
    }
    const auto &a = asset.source.vertices[triangle.vertex_indices[0]];
    const auto &b = asset.source.vertices[triangle.vertex_indices[1]];
    const auto &c = asset.source.vertices[triangle.vertex_indices[2]];
    const auto source_bary = generated.source_barycentric;
    const auto position =
        a.position * source_bary.x + b.position * source_bary.y + c.position * source_bary.z;
    const auto normal =
        normalized(a.normal * source_bary.x + b.normal * source_bary.y + c.normal * source_bary.z);
    samples.push_back({position, normal.value_or(Vec3{0.0, 0.0, 1.0}), generated});
  }
  return samples;
}

std::array<double, 4> project_simplex(std::array<double, 4> values) {
  std::array<double, 4> sorted = values;
  std::sort(sorted.begin(), sorted.end(), std::greater<double>());
  double cumulative = 0.0;
  int rho = -1;
  double theta = 0.0;
  for (int index = 0; index < 4; ++index) {
    cumulative += sorted[static_cast<std::size_t>(index)];
    const double candidate = (cumulative - 1.0) / static_cast<double>(index + 1);
    if (sorted[static_cast<std::size_t>(index)] - candidate > 0.0) {
      rho = index;
      theta = candidate;
    }
  }
  if (rho < 0) {
    return {0.25, 0.25, 0.25, 0.25};
  }
  std::array<double, 4> result{};
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = std::max(0.0, values[index] - theta);
  }
  double sum = 0.0;
  for (const double value : result) {
    sum += value;
  }
  if (!(sum > 0.0) || !std::isfinite(sum)) {
    return {0.25, 0.25, 0.25, 0.25};
  }
  for (double &value : result) {
    value /= sum;
  }
  return result;
}

std::array<double, 4> fit_clip_weights(const SurfaceSample &sample, const CompiledAsset &asset,
                                       const std::vector<std::vector<Vec3>> &poses,
                                       double amplitude) {
  std::array<double, 4> weights{
      sample.generated.cage_barycentric.x, sample.generated.cage_barycentric.y,
      sample.generated.cage_barycentric.z, sample.generated.cage_barycentric.w};
  weights = project_simplex(weights);
  double matrix[5][6]{};
  const auto &cage_tet = asset.cage.tetrahedra[sample.generated.tet_id];
  for (std::size_t frame = 0; frame < poses.size(); ++frame) {
    const auto &pose = poses[frame];
    const auto target =
        procedural_clip_position(sample.position, static_cast<std::uint32_t>(frame), amplitude);
    std::array<Vec3, 4> corners{};
    for (std::size_t corner = 0; corner < corners.size(); ++corner) {
      corners[corner] = pose[cage_tet.vertex_indices[corner]];
    }
    for (std::size_t row = 0; row < 4U; ++row) {
      for (std::size_t column = 0; column < 4U; ++column) {
        matrix[row][column] += dot(corners[row], corners[column]);
      }
      matrix[row][4] = 1.0;
      matrix[row][5] += dot(corners[row], target);
    }
  }
  for (std::size_t index = 0; index < 4U; ++index) {
    matrix[4][index] = 1.0;
    matrix[index][4] = 1.0;
  }
  matrix[4][5] = 1.0;
  for (std::size_t pivot = 0; pivot < 5U; ++pivot) {
    std::size_t best = pivot;
    for (std::size_t row = pivot + 1U; row < 5U; ++row) {
      if (std::abs(matrix[row][pivot]) > std::abs(matrix[best][pivot])) {
        best = row;
      }
    }
    if (std::abs(matrix[best][pivot]) < 1.0e-14) {
      return weights;
    }
    if (best != pivot) {
      for (std::size_t column = pivot; column < 6U; ++column) {
        std::swap(matrix[pivot][column], matrix[best][column]);
      }
    }
    const double divisor = matrix[pivot][pivot];
    for (std::size_t column = pivot; column < 6U; ++column) {
      matrix[pivot][column] /= divisor;
    }
    for (std::size_t row = 0; row < 5U; ++row) {
      if (row == pivot) {
        continue;
      }
      const double factor = matrix[row][pivot];
      for (std::size_t column = pivot; column < 6U; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
      }
    }
  }
  for (std::size_t index = 0; index < 4U; ++index) {
    weights[index] = matrix[index][5];
  }
  return project_simplex(weights);
}

Vec3 weighted_cage_position(const std::vector<Vec3> &pose, const CageTet &tet,
                            const std::array<double, 4> &weights) {
  Vec3 result{};
  for (std::size_t corner = 0; corner < weights.size(); ++corner) {
    result = result + pose[tet.vertex_indices[corner]] * weights[corner];
  }
  return result;
}

Tetrahedron posed_tet(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                      std::uint32_t tet_id);

std::optional<Mat3> cage_surface_linear(const CompiledAsset &asset, const SurfaceSample &sample,
                                        const std::vector<Vec3> &pose) {
  const auto rest =
      canonical_to_object(cage_tet(asset.cage, asset.cage.tetrahedra[sample.generated.tet_id]));
  const auto posed = canonical_to_object(posed_tet(asset, pose, sample.generated.tet_id));
  if (!rest || !posed) {
    return std::nullopt;
  }
  const auto inverse = rest->linear.inverse();
  if (!inverse) {
    return std::nullopt;
  }
  Mat3 result{};
  for (std::size_t column = 0; column < 3U; ++column) {
    result.columns[column] = posed->linear * inverse->columns[column];
  }
  return result;
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

CageAnimationReport analyze_cage_animation(const CompiledAsset &asset, std::uint32_t samples,
                                           double motion_amplitude, double position_threshold,
                                           double normal_threshold) {
  CageAnimationReport report{};
  report.asset_hash = asset_checksum(serialize_asset(asset));
  report.samples = std::max(samples, 1U);
  report.position_threshold = std::max(0.0, position_threshold);
  report.normal_threshold = std::max(0.0, normal_threshold);
  report.frames.reserve(report.samples);

  std::uint64_t initially_uncovered = 0U;
  const auto surface_samples = collect_surface_samples(asset, initially_uncovered);
  report.surface_samples = surface_samples.size();
  report.uncovered_samples = initially_uncovered * report.samples;
  std::vector<std::vector<Vec3>> poses;
  poses.reserve(report.samples);
  for (std::uint32_t frame = 0; frame < report.samples; ++frame) {
    poses.push_back(procedural_clip_pose(asset, frame, motion_amplitude));
  }

  std::vector<std::array<double, 4>> fitted_weights;
  fitted_weights.reserve(surface_samples.size());
  for (const auto &sample : surface_samples) {
    const auto fitted = fit_clip_weights(sample, asset, poses, motion_amplitude);
    const std::array<double, 4> base{
        sample.generated.cage_barycentric.x, sample.generated.cage_barycentric.y,
        sample.generated.cage_barycentric.z, sample.generated.cage_barycentric.w};
    for (std::size_t corner = 0; corner < base.size(); ++corner) {
      report.maximum_weight_delta =
          std::max(report.maximum_weight_delta, std::abs(fitted[corner] - base[corner]));
    }
    fitted_weights.push_back(fitted);
  }
  report.optimized_weight_samples = fitted_weights.size();

  double position_squared_sum = 0.0;
  double optimized_position_squared_sum = 0.0;
  std::uint64_t position_count = 0U;
  for (std::uint32_t frame = 0; frame < report.samples; ++frame) {
    CageAnimationFrame frame_report{};
    frame_report.frame = frame;
    frame_report.surface_samples = surface_samples.size();
    frame_report.uncovered_samples = initially_uncovered;
    for (std::size_t sample_index = 0; sample_index < surface_samples.size(); ++sample_index) {
      const auto &sample = surface_samples[sample_index];
      const auto posed_tet_value = posed_tet(asset, poses[frame], sample.generated.tet_id);
      const auto cage_position =
          from_barycentric(posed_tet_value, sample.generated.cage_barycentric);
      const auto dense_position =
          procedural_clip_position(sample.position, frame, motion_amplitude);
      const double position_error = length(cage_position - dense_position);
      frame_report.maximum_position_error =
          std::max(frame_report.maximum_position_error, position_error);
      position_squared_sum += position_error * position_error;
      frame_report.rms_position_error += position_error * position_error;

      const auto optimized_position =
          weighted_cage_position(poses[frame], asset.cage.tetrahedra[sample.generated.tet_id],
                                 fitted_weights[sample_index]);
      const double optimized_error = length(optimized_position - dense_position);
      frame_report.optimized_maximum_position_error =
          std::max(frame_report.optimized_maximum_position_error, optimized_error);
      optimized_position_squared_sum += optimized_error * optimized_error;
      frame_report.optimized_rms_position_error += optimized_error * optimized_error;
      ++position_count;

      const auto cage_linear = cage_surface_linear(asset, sample, poses[frame]);
      if (cage_linear) {
        const auto cage_normal = transform_normal(*cage_linear, sample.normal);
        const double normal_epsilon =
            std::max(1.0e-7, 1.0e-6 * std::max(1.0, length(sample.position)));
        const Vec3 dense_x = procedural_clip_position(
            sample.position + Vec3{normal_epsilon, 0.0, 0.0}, frame, motion_amplitude);
        const Vec3 dense_y = procedural_clip_position(
            sample.position + Vec3{0.0, normal_epsilon, 0.0}, frame, motion_amplitude);
        const Vec3 dense_z = procedural_clip_position(
            sample.position + Vec3{0.0, 0.0, normal_epsilon}, frame, motion_amplitude);
        const Vec3 dense_origin = dense_position;
        const Mat3 dense_linear{{(dense_x - dense_origin) / normal_epsilon,
                                 (dense_y - dense_origin) / normal_epsilon,
                                 (dense_z - dense_origin) / normal_epsilon}};
        const auto dense_normal = transform_normal(dense_linear, sample.normal);
        if (cage_normal && dense_normal) {
          frame_report.maximum_normal_error =
              std::max(frame_report.maximum_normal_error, length(*cage_normal - *dense_normal));
        }
      }
    }
    if (frame_report.surface_samples != 0U) {
      frame_report.rms_position_error = std::sqrt(
          frame_report.rms_position_error / static_cast<double>(frame_report.surface_samples));
      frame_report.optimized_rms_position_error =
          std::sqrt(frame_report.optimized_rms_position_error /
                    static_cast<double>(frame_report.surface_samples));
    }
    report.maximum_position_error =
        std::max(report.maximum_position_error, frame_report.maximum_position_error);
    report.maximum_normal_error =
        std::max(report.maximum_normal_error, frame_report.maximum_normal_error);
    report.optimized_maximum_position_error = std::max(
        report.optimized_maximum_position_error, frame_report.optimized_maximum_position_error);
    report.frames.push_back(frame_report);
  }

  if (position_count != 0U) {
    report.rms_position_error =
        std::sqrt(position_squared_sum / static_cast<double>(position_count));
    report.optimized_rms_position_error =
        std::sqrt(optimized_position_squared_sum / static_cast<double>(position_count));
  }
  if (report.uncovered_samples != 0U) {
    report.fallback_reasons.emplace_back("generated_surface_sample_has_invalid_provenance");
  }
  if (report.maximum_position_error > report.position_threshold) {
    report.fallback_reasons.emplace_back("clip_position_error_exceeds_threshold");
  }
  if (report.maximum_normal_error > report.normal_threshold) {
    report.fallback_reasons.emplace_back("clip_normal_error_exceeds_threshold");
  }
  report.suitable = report.fallback_reasons.empty();
  return report;
}

std::string cage_animation_json(const CageAnimationReport &report) {
  std::ostringstream output;
  output << std::setprecision(17) << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"clip_kind\": \"procedural_non_affine\",\n"
         << "  \"weight_fit\": \"projected_simplex_least_squares\",\n"
         << "  \"evidence_class\": \"synthetic\",\n"
         << "  \"command\": \"tetcage_cage_animation\",\n"
         << "  \"asset_hash\": \"" << std::hex << report.asset_hash << std::dec << "\",\n"
         << "  \"samples\": " << report.samples << ",\n"
         << "  \"surface_samples\": " << report.surface_samples << ",\n"
         << "  \"uncovered_samples\": " << report.uncovered_samples << ",\n"
         << "  \"optimized_weight_samples\": " << report.optimized_weight_samples << ",\n"
         << "  \"maximum_weight_delta\": " << report.maximum_weight_delta << ",\n"
         << "  \"position_threshold\": " << report.position_threshold << ",\n"
         << "  \"normal_threshold\": " << report.normal_threshold << ",\n"
         << "  \"maximum_position_error\": " << report.maximum_position_error << ",\n"
         << "  \"rms_position_error\": " << report.rms_position_error << ",\n"
         << "  \"maximum_normal_error\": " << report.maximum_normal_error << ",\n"
         << "  \"optimized_maximum_position_error\": " << report.optimized_maximum_position_error
         << ",\n"
         << "  \"optimized_rms_position_error\": " << report.optimized_rms_position_error << ",\n"
         << "  \"suitable\": " << (report.suitable ? "true" : "false") << ",\n"
         << "  \"frames\": [";
  for (std::size_t index = 0; index < report.frames.size(); ++index) {
    const auto &frame = report.frames[index];
    output << "{\"frame\": " << frame.frame << ", \"surface_samples\": " << frame.surface_samples
           << ", \"uncovered_samples\": " << frame.uncovered_samples
           << ", \"maximum_position_error\": " << frame.maximum_position_error
           << ", \"rms_position_error\": " << frame.rms_position_error
           << ", \"maximum_normal_error\": " << frame.maximum_normal_error
           << ", \"optimized_maximum_position_error\": " << frame.optimized_maximum_position_error
           << ", \"optimized_rms_position_error\": " << frame.optimized_rms_position_error << "}";
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
