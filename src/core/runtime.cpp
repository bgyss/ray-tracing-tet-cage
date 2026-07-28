#include "tetcage/runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace tetcage {
namespace {

Tetrahedron asset_tet(const CompiledAsset &asset, std::uint32_t tet_id,
                      const std::vector<Vec3> &positions) {
  Tetrahedron tet{};
  const auto &indices = asset.cage.tetrahedra[tet_id].vertex_indices;
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    tet.positions[corner] = positions[indices[corner]];
    tet.vertex_ids[corner] = asset.cage.vertex_ids[indices[corner]];
  }
  return tet;
}

std::optional<double> median(std::vector<double> values) {
  if (values.empty()) {
    return std::nullopt;
  }
  std::sort(values.begin(), values.end());
  const std::size_t middle = values.size() / 2U;
  if ((values.size() & 1U) != 0U) {
    return values[middle];
  }
  return (values[middle - 1U] + values[middle]) / 2.0;
}

BenchmarkSummary summarize(const std::vector<double> &samples) {
  BenchmarkSummary result{};
  if (samples.empty()) {
    return result;
  }
  std::vector<double> sorted = samples;
  std::sort(sorted.begin(), sorted.end());
  result.median_ms = *median(sorted);
  const auto p95_index =
      static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(sorted.size())) - 1.0);
  result.p95_ms = sorted[std::min(p95_index, sorted.size() - 1U)];
  const double mean =
      std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
  for (const double sample : samples) {
    const double delta = sample - mean;
    result.variance_ms2 += delta * delta;
  }
  result.variance_ms2 /= static_cast<double>(samples.size());
  return result;
}

std::string hex_u64(std::uint64_t value) {
  std::ostringstream output;
  output << std::hex << std::setfill('0') << std::setw(16) << value;
  return output.str();
}

std::string utc_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return output.str();
}

std::string json_escape(const std::string &value) {
  std::ostringstream output;
  for (const char character : value) {
    switch (character) {
    case '"':
      output << "\\\"";
      break;
    case '\\':
      output << "\\\\";
      break;
    case '\n':
      output << "\\n";
      break;
    case '\r':
      output << "\\r";
      break;
    case '\t':
      output << "\\t";
      break;
    default:
      output << character;
    }
  }
  return output.str();
}

template <typename T> void json_optional(std::ostringstream &output, std::optional<T> value) {
  if (value) {
    output << *value;
  } else {
    output << "null";
  }
}

struct IterationTimings {
  double cage{};
  double transforms{};
  double instances{};
  double traversal{};
  double total{};
  double dense_deformation{};
  double dense_traversal{};
  double dense_total{};
};

double elapsed_ms(std::chrono::steady_clock::time_point start,
                  std::chrono::steady_clock::time_point stop) {
  return std::chrono::duration<double, std::milli>(stop - start).count();
}

std::vector<Vec3> animated_pose(const CompiledAsset &asset, const BenchmarkScene &scene,
                                std::uint32_t frame) {
  std::vector<Vec3> pose = asset.cage.vertices;
  const double phase = static_cast<double>(frame) * 0.17;
  for (std::size_t index = 0; index < pose.size(); ++index) {
    const double weight =
        0.25 + static_cast<double>(index + 1U) / static_cast<double>(pose.size() + 1U);
    pose[index].z += scene.motion_amplitude * std::sin(phase + weight);
    pose[index].x += scene.motion_amplitude * 0.25 * std::cos(phase * 0.5 + weight);
  }
  return pose;
}

} // namespace

const char *representation_choice_name(RepresentationChoice choice) {
  switch (choice) {
  case RepresentationChoice::rigid_instancing:
    return "rigid_instancing";
  case RepresentationChoice::conventional_dynamic:
    return "conventional_dynamic";
  case RepresentationChoice::tet_cage:
    return "tet_cage";
  case RepresentationChoice::hybrid:
    return "hybrid";
  }
  return "conventional_dynamic";
}

const char *frame_build_status_name(FrameBuildStatus status) {
  switch (status) {
  case FrameBuildStatus::built:
    return "built";
  case FrameBuildStatus::invalid_input:
    return "invalid_input";
  case FrameBuildStatus::unsupported:
    return "unsupported";
  case FrameBuildStatus::resource_limit:
    return "resource_limit";
  case FrameBuildStatus::cancelled:
    return "cancelled";
  case FrameBuildStatus::device_lost:
    return "device_lost";
  case FrameBuildStatus::reset_required:
    return "reset_required";
  }
  return "invalid_input";
}

const char *frame_fallback_name(FrameFallback fallback) {
  switch (fallback) {
  case FrameFallback::none:
    return "none";
  case FrameFallback::conventional_dynamic:
    return "conventional_dynamic";
  case FrameFallback::retain_last_valid_frame:
    return "retain_last_valid_frame";
  }
  return "conventional_dynamic";
}

MethodSelectionResult select_representation(const MethodSelectionInput &input) {
  MethodSelectionResult result{};
  const auto add_reason = [&result](const char *reason) { result.reasons.emplace_back(reason); };
  if (input.source_triangles == 0U || input.occupied_tetrahedra == 0U) {
    add_reason("asset_has_no_renderable_geometry");
    return result;
  }
  if (!input.animated) {
    result.choice = RepresentationChoice::rigid_instancing;
    add_reason("no_deformation_workload");
    return result;
  }
  if (!std::isfinite(input.maximum_condition) || input.maximum_condition >= 1.0e8) {
    add_reason("conditioning_requires_conventional_fallback");
    return result;
  }
  if (!std::isfinite(input.maximum_position_error) || !std::isfinite(input.maximum_normal_error) ||
      input.maximum_position_error > 1.0e-3 || input.maximum_normal_error > 1.0e-2) {
    add_reason("animation_residual_exceeds_authoring_threshold");
    return result;
  }
  if (!input.hardware_tet_backend || !input.gpu_correctness_proven) {
    add_reason("tet_backend_correctness_is_not_proven_on_target_device");
    return result;
  }
  if (!std::isfinite(input.boundary_fallback_fraction) || input.boundary_fallback_fraction < 0.0 ||
      input.boundary_fallback_fraction > 1.0) {
    add_reason("boundary_fallback_fraction_is_invalid");
    return result;
  }
  if (input.boundary_fallback_fraction > 0.25) {
    result.choice = RepresentationChoice::hybrid;
    add_reason("boundary_fallback_fraction_is_high");
    return result;
  }
  if (input.copies < 8U || input.source_triangles < 100U) {
    result.choice = RepresentationChoice::conventional_dynamic;
    add_reason("measured_instancing_workload_is_too_small_for_tet_cage");
    return result;
  }
  result.choice = RepresentationChoice::tet_cage;
  add_reason("animated_geometry_and_repeated_instances_match_tet_cage_domain");
  return result;
}

std::string method_selection_json(const MethodSelectionInput &input,
                                  const MethodSelectionResult &result) {
  std::ostringstream output;
  output << std::setprecision(17) << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"policy_version\": 1,\n"
         << "  \"report_kind\": \"representation_selection\",\n"
         << "  \"evidence_class\": \"policy_evaluation\",\n"
         << "  \"inputs\": {\n"
         << "    \"source_triangles\": " << input.source_triangles << ",\n"
         << "    \"occupied_tetrahedra\": " << input.occupied_tetrahedra << ",\n"
         << "    \"copies\": " << input.copies << ",\n"
         << "    \"maximum_condition\": " << input.maximum_condition << ",\n"
         << "    \"maximum_position_error\": " << input.maximum_position_error << ",\n"
         << "    \"maximum_normal_error\": " << input.maximum_normal_error << ",\n"
         << "    \"boundary_fallback_fraction\": " << input.boundary_fallback_fraction << ",\n"
         << "    \"animated\": " << (input.animated ? "true" : "false") << ",\n"
         << "    \"hardware_tet_backend\": " << (input.hardware_tet_backend ? "true" : "false")
         << ",\n"
         << "    \"gpu_correctness_proven\": " << (input.gpu_correctness_proven ? "true" : "false")
         << "\n  },\n"
         << "  \"selection\": {\n"
         << "    \"representation\": \"" << representation_choice_name(result.choice) << "\",\n"
         << "    \"reasons\": [";
  for (std::size_t index = 0; index < result.reasons.size(); ++index) {
    output << (index == 0U ? "\n" : ",\n") << "      \"" << result.reasons[index] << "\"";
  }
  if (!result.reasons.empty()) {
    output << '\n';
  }
  output << "    ]\n  },\n"
         << "  \"claim_boundary\": \"This deterministic policy consumes measured inputs; it does "
            "not create performance evidence.\"\n"
         << "}\n";
  return output.str();
}

TransformBuildResult build_tet_transforms(const CompiledAsset &asset, const CagePose &pose,
                                          std::uint32_t object_id, std::uint32_t mesh_id) {
  TransformBuildResult result{};
  if (pose.positions.size() != asset.cage.vertices.size()) {
    result.error = "pose vertex count does not match the asset cage";
    return result;
  }
  result.transforms.reserve(asset.cage.tetrahedra.size());
  for (std::uint32_t tet_id = 0; tet_id < asset.cage.tetrahedra.size(); ++tet_id) {
    const auto tet = asset_tet(asset, tet_id, pose.positions);
    const auto diagnostics = diagnose(tet);
    const auto transform = canonical_to_object(tet);
    if (diagnostics.classification == TetClass::near_singular || !transform) {
      result.transforms.clear();
      result.error = "posed tetrahedron " + std::to_string(tet_id) +
                     " is near singular and cannot become an instance transform";
      return result;
    }
    const auto inverse = transform->linear.inverse();
    if (!inverse) {
      result.transforms.clear();
      result.error = "posed tetrahedron normal transform is singular";
      return result;
    }
    std::uint32_t flags = tet_transform_none;
    if (diagnostics.classification == TetClass::mirrored) {
      flags |= tet_transform_mirrored;
    }
    if (diagnostics.condition_estimate > 1.0e8) {
      flags |= tet_transform_poorly_conditioned;
    }
    result.transforms.push_back(
        {*transform, inverse->transposed(), object_id, mesh_id, tet_id, flags});
  }
  return result;
}

DenseDeformationResult deform_dense_source(const CompiledAsset &asset, const CagePose &pose) {
  DenseDeformationResult result{};
  if (pose.positions.size() != asset.cage.vertices.size()) {
    result.error = "pose vertex count does not match the asset cage";
    return result;
  }
  result.positions.reserve(asset.source.vertices.size());
  result.normals.reserve(asset.source.vertices.size());
  for (std::size_t vertex_index = 0; vertex_index < asset.source.vertices.size(); ++vertex_index) {
    const auto &source_vertex = asset.source.vertices[vertex_index];
    bool found_owner = false;
    for (std::uint32_t tet_id = 0; tet_id < asset.cage.tetrahedra.size(); ++tet_id) {
      const auto rest = asset_tet(asset, tet_id, asset.cage.vertices);
      const auto barycentric = to_barycentric(rest, source_vertex.position);
      if (!barycentric) {
        continue;
      }
      const double minimum_barycentric =
          std::min({barycentric->x, barycentric->y, barycentric->z, barycentric->w});
      if (minimum_barycentric < -asset.tolerance.feature_snap_epsilon * 8.0) {
        continue;
      }
      const auto posed = asset_tet(asset, tet_id, pose.positions);
      result.positions.push_back(from_barycentric(posed, *barycentric));
      const auto rest_transform = canonical_to_object(rest);
      const auto posed_transform = canonical_to_object(posed);
      if (!rest_transform || !posed_transform) {
        result.error = "dense baseline encountered a singular tetrahedron";
        result.positions.clear();
        result.normals.clear();
        return result;
      }
      const auto rest_inverse = rest_transform->linear.inverse();
      if (!rest_inverse) {
        result.error = "dense baseline encountered a singular rest tetrahedron";
        result.positions.clear();
        result.normals.clear();
        return result;
      }
      Mat3 deformation{};
      for (std::size_t column = 0; column < 3U; ++column) {
        deformation.columns[column] = posed_transform->linear * rest_inverse->columns[column];
      }
      const auto normal = transform_normal(deformation, source_vertex.normal);
      result.normals.push_back(normal.value_or(Vec3{}));
      found_owner = true;
      break;
    }
    if (!found_owner) {
      result.error = "source vertex " + std::to_string(vertex_index) +
                     " is not covered by any cage tetrahedron";
      result.positions.clear();
      result.normals.clear();
      return result;
    }
  }
  return result;
}

std::uint64_t AssetCache::insert(std::shared_ptr<const CompiledAsset> asset, std::string backend,
                                 std::vector<OpaqueBlasHandle> immutable_micro_blas) {
  if (!asset) {
    throw std::invalid_argument("asset cache cannot store a null asset");
  }
  const auto bytes = serialize_asset(*asset);
  std::uint64_t key = asset_checksum(bytes);
  for (const char character : backend) {
    key ^= static_cast<unsigned char>(character);
    key *= 1099511628211ULL;
  }
  entries_.insert_or_assign(
      key, CachedAsset{std::move(asset), std::move(backend), std::move(immutable_micro_blas)});
  return key;
}

const CachedAsset *AssetCache::find(std::uint64_t key) const {
  const auto found = entries_.find(key);
  return found == entries_.end() ? nullptr : &found->second;
}

bool AssetCache::erase(std::uint64_t key) { return entries_.erase(key) != 0U; }
std::size_t AssetCache::size() const { return entries_.size(); }

CpuStubBackend::CpuStubBackend(std::shared_ptr<const CompiledAsset> asset)
    : asset_(std::move(asset)) {
  if (!asset_) {
    throw std::invalid_argument("CPU stub backend requires an asset");
  }
  active_pose_ = asset_->cage.vertices;
}

std::string CpuStubBackend::api_name() const { return "stub"; }

BackendCapabilities CpuStubBackend::capabilities() const {
  return {false, true, false, true, std::nullopt};
}

BackendFrameResult CpuStubBackend::build_frame(const FrameBuildInput &input) {
  BackendFrameResult result{};
  const auto reject = [&result](FrameBuildStatus status, FrameFallback fallback,
                                const char *message) {
    result.status = status;
    result.fallback = fallback;
    result.error = message;
    return result;
  };
  if (input.control.device_lost) {
    has_valid_frame_ = false;
    active_pose_ = asset_->cage.vertices;
    return reject(
        FrameBuildStatus::device_lost, FrameFallback::conventional_dynamic,
        "backend device was lost; discard frame and rebuild through the conventional path");
  }
  if (input.control.reset_requested) {
    has_valid_frame_ = false;
    active_pose_ = asset_->cage.vertices;
    return reject(FrameBuildStatus::reset_required, FrameFallback::conventional_dynamic,
                  "backend reset is required before another tet-cage frame can be built");
  }
  if (input.control.cancellation_requested) {
    return reject(FrameBuildStatus::cancelled,
                  has_valid_frame_ ? FrameFallback::retain_last_valid_frame
                                   : FrameFallback::conventional_dynamic,
                  "frame build was cancelled before submission");
  }
  if (input.policy.strategy == BuildStrategy::periodic_rebuild &&
      input.policy.rebuild_period == 0U) {
    return reject(FrameBuildStatus::invalid_input, FrameFallback::conventional_dynamic,
                  "periodic rebuild policy requires a nonzero period");
  }
  if (input.policy.strategy == BuildStrategy::update) {
    return reject(FrameBuildStatus::unsupported, FrameFallback::conventional_dynamic,
                  "CPU stub backend does not support in-place acceleration-structure updates");
  }
  std::uint64_t planned_instances = 0U;
  for (const auto &object : input.objects) {
    if (!object.visible) {
      continue;
    }
    if (object.pose_index >= input.poses.size()) {
      return reject(FrameBuildStatus::invalid_input, FrameFallback::conventional_dynamic,
                    "visible object references an unavailable cage pose");
    }
    if (input.control.cancellation_requested) {
      return reject(FrameBuildStatus::cancelled,
                    has_valid_frame_ ? FrameFallback::retain_last_valid_frame
                                     : FrameFallback::conventional_dynamic,
                    "frame build was cancelled before transform generation");
    }
    const auto tet_count = static_cast<std::uint64_t>(asset_->cage.tetrahedra.size());
    if (planned_instances > std::numeric_limits<std::uint64_t>::max() - tet_count) {
      return reject(FrameBuildStatus::resource_limit, FrameFallback::conventional_dynamic,
                    "frame transform count overflows the allocation budget");
    }
    planned_instances += tet_count;
  }
  if (input.policy.max_instances && planned_instances > *input.policy.max_instances) {
    return reject(FrameBuildStatus::resource_limit, FrameFallback::conventional_dynamic,
                  "frame exceeds the build policy instance limit");
  }
  if (planned_instances > std::numeric_limits<std::uint64_t>::max() / sizeof(TetTransform)) {
    return reject(FrameBuildStatus::resource_limit, FrameFallback::conventional_dynamic,
                  "frame transform allocation exceeds representable size");
  }
  result.allocation_bytes = planned_instances * sizeof(TetTransform);
  if (input.control.max_allocation_bytes &&
      result.allocation_bytes > *input.control.max_allocation_bytes) {
    return reject(FrameBuildStatus::resource_limit, FrameFallback::conventional_dynamic,
                  "frame exceeds the configured allocation byte limit");
  }
  bool selected_pose = false;
  for (const auto &object : input.objects) {
    if (!object.visible) {
      continue;
    }
    if (object.pose_index >= input.poses.size()) {
      return reject(FrameBuildStatus::invalid_input, FrameFallback::conventional_dynamic,
                    "visible object references an unavailable cage pose");
    }
    if (input.control.cancellation_requested) {
      return reject(FrameBuildStatus::cancelled,
                    has_valid_frame_ ? FrameFallback::retain_last_valid_frame
                                     : FrameFallback::conventional_dynamic,
                    "frame build was cancelled during transform generation");
    }
    const auto transforms = build_tet_transforms(*asset_, input.poses[object.pose_index],
                                                 object.object_id, object.mesh_id);
    if (!transforms.error.empty()) {
      result.status = FrameBuildStatus::invalid_input;
      result.fallback = FrameFallback::conventional_dynamic;
      result.error = transforms.error;
      return result;
    }
    ++result.visible_instances;
    result.transforms += transforms.transforms.size();
    if (!selected_pose) {
      active_pose_ = input.poses[object.pose_index].positions;
      selected_pose = true;
    }
  }
  has_valid_frame_ = true;
  result.status = FrameBuildStatus::built;
  return result;
}

TraceResult CpuStubBackend::trace(const Ray &ray) const {
  return trace_fast(*asset_, active_pose_, ray);
}

BenchmarkResult run_cpu_stub_benchmark(const CompiledAsset &asset, const BenchmarkScene &scene,
                                       const BenchmarkOptions &options) {
  if (scene.copies == 0U || scene.ray_count == 0U || scene.visible_fraction < 0.0 ||
      scene.visible_fraction > 1.0 || options.measured_iterations == 0U) {
    throw std::invalid_argument("benchmark scene/options are invalid");
  }
  BenchmarkResult result{};
  result.scene = scene;
  result.options = options;
  const auto asset_bytes = serialize_asset(asset);
  result.asset_hash = hex_u64(asset_checksum(asset_bytes));
  result.memory.source_geometry_bytes = asset.source.vertices.size() * sizeof(SourceVertex) +
                                        asset.source.triangles.size() * sizeof(SourceTriangle);
  result.memory.canonical_geometry_bytes = asset.statistics.canonical_bytes;
  result.memory.provenance_bytes = asset.statistics.provenance_bytes;
  result.memory.cage_bytes =
      asset.cage.vertices.size() * sizeof(Vec3) + asset.cage.tetrahedra.size() * sizeof(CageTet);
  const auto visible_copies = static_cast<std::uint64_t>(
      std::ceil(static_cast<double>(scene.copies) * scene.visible_fraction));
  result.memory.instance_buffer_bytes =
      visible_copies * asset.cage.tetrahedra.size() * sizeof(TetTransform);

  const auto bvh = build_bvh4d(asset, 4U);
  const std::uint32_t total_iterations = options.warmup_iterations + options.measured_iterations;
  for (std::uint32_t frame = 0; frame < total_iterations; ++frame) {
    IterationTimings iteration{};
    const auto cage_start = std::chrono::steady_clock::now();
    CagePose pose{animated_pose(asset, scene, frame)};
    const auto cage_stop = std::chrono::steady_clock::now();
    iteration.cage = elapsed_ms(cage_start, cage_stop);

    const auto transform_start = std::chrono::steady_clock::now();
    std::vector<TetTransform> all_transforms;
    for (std::uint32_t copy = 0; copy < scene.copies; ++copy) {
      const auto transforms = build_tet_transforms(asset, pose, copy, 0U);
      if (!transforms.error.empty()) {
        result.failures.push_back(transforms.error);
        ++result.correctness_errors;
        continue;
      }
      all_transforms.insert(all_transforms.end(), transforms.transforms.begin(),
                            transforms.transforms.end());
    }
    const auto transform_stop = std::chrono::steady_clock::now();
    iteration.transforms = elapsed_ms(transform_start, transform_stop);

    const auto instance_start = std::chrono::steady_clock::now();
    const std::size_t visible_instances = static_cast<std::size_t>(
        std::ceil(static_cast<double>(all_transforms.size()) * scene.visible_fraction));
    std::vector<std::uint32_t> instance_metadata(visible_instances);
    std::iota(instance_metadata.begin(), instance_metadata.end(), 0U);
    const auto instance_stop = std::chrono::steady_clock::now();
    iteration.instances = elapsed_ms(instance_start, instance_stop);

    const auto dense_start = std::chrono::steady_clock::now();
    const auto dense = deform_dense_source(asset, pose);
    const auto dense_stop = std::chrono::steady_clock::now();
    iteration.dense_deformation = elapsed_ms(dense_start, dense_stop);
    if (!dense.error.empty()) {
      result.failures.push_back(dense.error);
      ++result.correctness_errors;
    }
    auto rays =
        generate_adversarial_rays(asset, pose.positions, scene.seed + frame, scene.ray_count);
    if (rays.size() > scene.ray_count) {
      rays.resize(scene.ray_count);
    }
    SourceMesh dense_mesh = asset.source;
    if (dense.error.empty()) {
      for (std::size_t index = 0; index < dense_mesh.vertices.size(); ++index) {
        dense_mesh.vertices[index].position = dense.positions[index];
        dense_mesh.vertices[index].normal = dense.normals[index];
      }
    }
    const auto dense_traversal_start = std::chrono::steady_clock::now();
    std::vector<TraceResult> dense_hits;
    dense_hits.reserve(rays.size());
    for (const auto &ray : rays) {
      dense_hits.push_back(trace_dense(dense_mesh, ray));
    }
    const auto dense_traversal_stop = std::chrono::steady_clock::now();
    iteration.dense_traversal = elapsed_ms(dense_traversal_start, dense_traversal_stop);
    iteration.dense_total = iteration.dense_deformation + iteration.dense_traversal;

    const auto traversal_start = std::chrono::steady_clock::now();
    bool injected_this_frame = false;
    for (std::size_t ray_index = 0; ray_index < rays.size(); ++ray_index) {
      const auto &ray = rays[ray_index];
      const auto oracle =
          trace_watertight4d(asset, bvh, pose.positions, ray, ProjectionMode::bounded_simplex);
      auto stub = trace_fast(asset, pose.positions, ray);
      ++result.rays_traced;
      if (options.inject_stub_corruption && !injected_this_frame && stub.closest) {
        ++stub.closest->source_primitive;
        injected_this_frame = true;
      }
      if (!oracle.closest || !stub.closest) {
        ++result.misses;
        ++result.correctness_errors;
        if (result.failures.size() < 16U) {
          const auto interval_oracle =
              trace_watertight4d(asset, bvh, pose.positions, ray, ProjectionMode::interval_sum);
          std::ostringstream failure;
          failure << "frame " << frame << " ray " << ray_index
                  << " missing hit: oracle=" << (oracle.closest ? "hit" : "miss")
                  << " interval_oracle=" << (interval_oracle.closest ? "hit" : "miss")
                  << " stub=" << (stub.closest ? "hit" : "miss")
                  << " exact_tests=" << oracle.triangle_tests
                  << " interval_tests=" << interval_oracle.triangle_tests << " origin=["
                  << ray.origin.x << ',' << ray.origin.y << ',' << ray.origin.z << "] direction=["
                  << ray.direction.x << ',' << ray.direction.y << ',' << ray.direction.z << ']';
          result.failures.push_back(failure.str());
        }
        continue;
      }
      if (ray_index >= dense_hits.size() || !dense_hits[ray_index].closest) {
        ++result.dense_baseline_misses;
      } else if (dense_hits[ray_index].closest->source_primitive !=
                     oracle.closest->source_primitive ||
                 dense_hits[ray_index].closest->material != oracle.closest->material) {
        ++result.dense_baseline_provenance_mismatches;
      }
      if (oracle.closest->source_primitive != stub.closest->source_primitive ||
          oracle.closest->material != stub.closest->material) {
        ++result.correctness_errors;
        if (result.failures.size() < 16U) {
          result.failures.push_back("stub hit provenance differs from the CPU 4D oracle");
        }
      }
      result.max_position_error = std::max(
          result.max_position_error, length(oracle.closest->position - stub.closest->position));
      result.max_attribute_error =
          std::max({result.max_attribute_error, std::abs(oracle.closest->uv.x - stub.closest->uv.x),
                    std::abs(oracle.closest->uv.y - stub.closest->uv.y)});
      result.duplicate_hits += stub.duplicate_ownership;
    }
    const auto traversal_stop = std::chrono::steady_clock::now();
    iteration.traversal = elapsed_ms(traversal_start, traversal_stop);
    iteration.total =
        iteration.cage + iteration.transforms + iteration.instances + iteration.traversal;
    if (frame >= options.warmup_iterations) {
      result.cage_samples_ms.push_back(iteration.cage);
      result.transform_samples_ms.push_back(iteration.transforms);
      result.instance_samples_ms.push_back(iteration.instances);
      result.traversal_samples_ms.push_back(iteration.traversal);
      result.dense_deformation_samples_ms.push_back(iteration.dense_deformation);
      result.dense_traversal_samples_ms.push_back(iteration.dense_traversal);
      result.dense_total_samples_ms.push_back(iteration.dense_total);
      result.total_samples_ms.push_back(iteration.total);
    }
  }
  result.stages.cage_deformation_ms = median(result.cage_samples_ms);
  result.stages.transform_generation_ms = median(result.transform_samples_ms);
  result.stages.instance_generation_ms = median(result.instance_samples_ms);
  result.stages.traversal_ms = median(result.traversal_samples_ms);
  result.stages.total_ms = median(result.total_samples_ms);
  result.dense_deformation_median_ms = median(result.dense_deformation_samples_ms);
  result.dense_traversal_median_ms = median(result.dense_traversal_samples_ms);
  result.summary = summarize(result.total_samples_ms);
  result.dense_summary = summarize(result.dense_total_samples_ms);
  result.corruption_detected = options.inject_stub_corruption && result.correctness_errors > 0U;
  return result;
}

std::string benchmark_manifest_json(const BenchmarkResult &result) {
  std::ostringstream output;
  output << std::setprecision(17);
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"run_id\": \"cpu-stub-" << result.asset_hash << '-' << result.scene.seed << "\",\n"
         << "  \"timestamp_utc\": \"" << utc_timestamp() << "\",\n"
         << "  \"source\": {\"commit\": \"" << result.source_commit
         << "\", \"dirty\": " << (result.source_dirty ? "true" : "false") << "},\n"
         << "  \"build\": {\"compiler\": \""
#if defined(__apple_build_version__)
         << "AppleClang"
#elif defined(__clang__)
         << "Clang"
#elif defined(__GNUC__)
         << "GCC"
#elif defined(_MSC_VER)
         << "MSVC"
#else
         << "unknown"
#endif
         << "\", \"compiler_version\": \""
#if defined(__clang__)
         << __clang_version__
#elif defined(__GNUC__)
         << __VERSION__
#else
         << "unknown"
#endif
         << "\", \"build_type\": \"local\"},\n"
         << "  \"host\": {\"os\": \""
#if defined(__APPLE__)
         << "macOS"
#elif defined(_WIN32)
         << "Windows"
#elif defined(__linux__)
         << "Linux"
#else
         << "unknown"
#endif
         << "\", \"os_version\": \"" << json_escape(result.host_os_version)
         << "\", \"architecture\": \""
#if defined(__aarch64__) || defined(_M_ARM64)
         << "arm64"
#elif defined(__x86_64__) || defined(_M_X64)
         << "x86_64"
#else
         << "unknown"
#endif
         << "\"},\n"
         << "  \"backend\": {\"api\": \"stub\", \"status\": \"measured\", "
            "\"device\": \"CPU reference\", \"driver\": null, "
            "\"capability_manifest\": null},\n"
         << "  \"scene\": {\"id\": \"" << json_escape(result.scene.id) << "\", \"hash\": \""
         << result.asset_hash << "\", \"seed\": " << result.scene.seed << "},\n"
         << "  \"configuration\": {\"copies\": " << result.scene.copies
         << ", \"ray_count\": " << result.scene.ray_count
         << ", \"motion_amplitude\": " << result.scene.motion_amplitude
         << ", \"visible_fraction\": " << result.scene.visible_fraction
         << ", \"inject_stub_corruption\": "
         << (result.options.inject_stub_corruption ? "true" : "false") << "},\n"
         << "  \"timings_ms\": {\n"
         << "    \"cage_deformation\": ";
  json_optional(output, result.stages.cage_deformation_ms);
  output << ",\n    \"transform_generation\": ";
  json_optional(output, result.stages.transform_generation_ms);
  output << ",\n    \"instance_generation\": ";
  json_optional(output, result.stages.instance_generation_ms);
  output << ",\n    \"blas\": ";
  json_optional(output, result.stages.blas_ms);
  output << ",\n    \"tlas\": ";
  json_optional(output, result.stages.tlas_ms);
  output << ",\n    \"synchronization\": ";
  json_optional(output, result.stages.synchronization_ms);
  output << ",\n    \"traversal\": ";
  json_optional(output, result.stages.traversal_ms);
  output << ",\n    \"shading\": ";
  json_optional(output, result.stages.shading_ms);
  output << ",\n    \"total\": ";
  json_optional(output, result.stages.total_ms);
  output << ",\n    \"median\": " << result.summary.median_ms
         << ",\n    \"p95\": " << result.summary.p95_ms
         << ",\n    \"variance\": " << result.summary.variance_ms2
         << ",\n    \"warmup_iterations\": " << result.options.warmup_iterations
         << ",\n    \"measured_iterations\": " << result.options.measured_iterations << "\n  },\n"
         << "  \"memory_bytes\": {\n"
         << "    \"source_geometry\": ";
  json_optional(output, result.memory.source_geometry_bytes);
  output << ",\n    \"canonical_geometry\": ";
  json_optional(output, result.memory.canonical_geometry_bytes);
  output << ",\n    \"provenance\": ";
  json_optional(output, result.memory.provenance_bytes);
  output << ",\n    \"cage\": ";
  json_optional(output, result.memory.cage_bytes);
  output << ",\n    \"blas\": ";
  json_optional(output, result.memory.blas_bytes);
  output << ",\n    \"tlas\": ";
  json_optional(output, result.memory.tlas_bytes);
  output << ",\n    \"scratch\": ";
  json_optional(output, result.memory.scratch_bytes);
  output << ",\n    \"instance_buffers\": ";
  json_optional(output, result.memory.instance_buffer_bytes);
  output << ",\n    \"api_objects\": ";
  json_optional(output, result.memory.api_object_bytes);
  output << ",\n    \"renderer_state\": ";
  json_optional(output, result.memory.renderer_state_bytes);
  output << ",\n    \"peak_total\": ";
  json_optional(output, result.memory.peak_total_bytes);
  output << "\n  },\n"
         << "  \"correctness\": {\"rays\": " << result.rays_traced
         << ", \"misses\": " << result.misses << ", \"duplicate_hits\": " << result.duplicate_hits
         << ", \"wrong_ownership\": " << result.wrong_ownership
         << ", \"position_error_max\": " << result.max_position_error
         << ", \"normal_error_max\": null, \"attribute_error_max\": " << result.max_attribute_error
         << ", \"image_error\": null},\n"
         << "  \"statistics\": {\n"
         << "    \"correctness_errors\": " << result.correctness_errors
         << ",\n    \"corruption_detected\": " << (result.corruption_detected ? "true" : "false")
         << ",\n"
         << "    \"dense_cpu_baseline\": {\n"
         << "      \"deformation_median_ms\": ";
  json_optional(output, result.dense_deformation_median_ms);
  output << ",\n      \"traversal_median_ms\": ";
  json_optional(output, result.dense_traversal_median_ms);
  output << ",\n      \"total_median_ms\": " << result.dense_summary.median_ms
         << ",\n      \"total_p95_ms\": " << result.dense_summary.p95_ms
         << ",\n      \"oracle_ray_misses\": " << result.dense_baseline_misses
         << ",\n      \"provenance_mismatches\": " << result.dense_baseline_provenance_mismatches
         << ",\n      \"blas_update_ms\": null,\n"
         << "      \"blas_rebuild_ms\": null,\n"
         << "      \"note\": \"CPU geometry/traversal baseline only; no GPU performance "
            "conclusion\"\n"
         << "    }\n"
         << "  },\n"
         << "  \"failures\": [\n";
  for (std::size_t index = 0; index < result.failures.size(); ++index) {
    output << "    {\"code\": \"benchmark_failure\", \"message\": \""
           << json_escape(result.failures[index]) << "\"}"
           << (index + 1U == result.failures.size() ? "\n" : ",\n");
  }
  output << "  ],\n"
         << "  \"command\": \"" << json_escape(result.command) << "\",\n"
         << "  \"evidence_class\": \"synthetic\"\n"
         << "}\n";
  return output.str();
}

std::string benchmark_samples_csv(const BenchmarkResult &result) {
  std::ostringstream output;
  output << "iteration,cage_deformation_ms,transform_generation_ms,"
            "instance_generation_ms,traversal_ms,tet_stub_total_ms,"
            "dense_deformation_ms,dense_traversal_ms,dense_total_ms\n";
  for (std::size_t index = 0; index < result.total_samples_ms.size(); ++index) {
    output << index << ',' << std::setprecision(17) << result.cage_samples_ms[index] << ','
           << result.transform_samples_ms[index] << ',' << result.instance_samples_ms[index] << ','
           << result.traversal_samples_ms[index] << ',' << result.total_samples_ms[index] << ','
           << result.dense_deformation_samples_ms[index] << ','
           << result.dense_traversal_samples_ms[index] << ','
           << result.dense_total_samples_ms[index] << '\n';
  }
  return output.str();
}

} // namespace tetcage
