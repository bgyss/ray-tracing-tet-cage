#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "tetcage/metal_backend.h"

#include "tetcage/cycles_bridge.h"
#include "tetcage/metal_evidence.h"
#include "tetcage/oracle.h"
#include "tetcage/runtime.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tetcage {
namespace {

using Clock = std::chrono::steady_clock;

struct PackedFloat2 {
  float x{};
  float y{};
};

struct PackedFloat3 {
  float x{};
  float y{};
  float z{};
};

struct PackedTripleFloat3 {
  PackedFloat3 high{};
  float padding0{};
  PackedFloat3 middle{};
  float padding1{};
  PackedFloat3 low{};
  float padding2{};
};

struct BlasVertex {
  float x{};
  float y{};
  float z{};
  float padding{};
};

struct GpuAabb {
  PackedFloat3 minimum{};
  PackedFloat3 maximum{};
};

static_assert(sizeof(PackedFloat2) == 8U);
static_assert(sizeof(PackedFloat3) == 12U);
static_assert(sizeof(GpuAabb) == 24U);

struct GpuRay {
  PackedFloat3 origin{};
  float minimum_distance{};
  PackedFloat3 direction{};
  float maximum_distance{};
  PackedFloat3 origin_low{};
  float minimum_distance_low{};
  PackedFloat3 direction_low{};
  float maximum_distance_low{};
  PackedFloat3 origin_middle{};
  float minimum_distance_middle{};
  PackedFloat3 direction_middle{};
  float maximum_distance_middle{};
};

struct GpuProvenance {
  std::uint32_t source_primitive{};
  std::uint32_t material{};
  std::array<PackedFloat3, 3> source_barycentric{};
  std::array<PackedFloat3, 3> normals{};
  std::array<PackedFloat2, 3> uvs{};
};

struct GpuHit {
  std::uint32_t hit{};
  std::uint32_t primitive_id{};
  std::uint32_t user_instance_id{};
  std::uint32_t source_primitive{};
  std::uint32_t material{};
  std::uint32_t front_facing{};
  std::uint32_t reserved0{};
  std::uint32_t reserved1{};
  float distance{};
  PackedFloat2 triangle_barycentric{};
  float reserved2{};
  PackedFloat3 source_barycentric{};
  float reserved3{};
  PackedFloat3 normal{};
  float reserved4{};
  PackedFloat2 uv{};
  std::array<float, 2> reserved5{};
};

static_assert(sizeof(GpuRay) == 96U);
static_assert(sizeof(PackedTripleFloat3) == 48U);
static_assert(sizeof(GpuProvenance) == 104U);
static_assert(sizeof(GpuHit) == 96U);

struct GpuInstanceInput {
  PackedFloat3 c0{};
  PackedFloat3 c1{};
  PackedFloat3 c2{};
  PackedFloat3 translation{};
  std::uint32_t options{};
  std::uint32_t mask{};
  std::uint32_t intersection_function_table_offset{};
  std::uint32_t acceleration_structure_index{};
  std::uint32_t user_id{};
};

static_assert(sizeof(GpuInstanceInput) == sizeof(MTLAccelerationStructureUserIDInstanceDescriptor));

struct BlasGroup {
  std::uint32_t tet_id{};
  std::vector<std::uint32_t> micro_triangle_indices;
  std::vector<BlasVertex> vertices;
  std::vector<PackedTripleFloat3> precise_vertices;
};

struct InstanceInfo {
  std::uint32_t copy{};
  std::uint32_t blas_index{};
  std::uint32_t tet_id{};
};

struct InstanceRecords {
  std::vector<MTLAccelerationStructureUserIDInstanceDescriptor> descriptors;
  std::vector<GpuInstanceInput> gpu_inputs;
  std::vector<InstanceInfo> info;
  std::vector<std::uint32_t> instance_to_blas;
  std::vector<std::array<PackedTripleFloat3, 4>> precise_transforms;
  bool mirrored{};
};

struct RunMeasurements {
  std::string status{"error"};
  std::string evidence_class{"partial"};
  std::string device;
  std::string failure_code;
  std::string failure_message;
  std::uint64_t asset_hash{};
  std::uint64_t blas_count{};
  std::uint64_t total_instances{};
  std::uint64_t rays{};
  std::uint64_t misses{};
  std::uint64_t wrong_ownership{};
  std::uint64_t boundary_sensitive_rays{};
  std::uint64_t gpu_eligible_rays{};
  std::uint64_t cpu_fallback_rays{};
  std::uint64_t cpu_fallback_hits{};
  std::uint64_t hardware_mismatches{};
  std::uint64_t minimization_replays{};
  std::uint64_t minimized_regressions{};
  std::uint64_t minimization_failures{};
  std::uint64_t final_stream_errors{};
  std::uint64_t completed_frames{};
  std::uint64_t tlas_refit_frames{};
  std::uint64_t tlas_rebuild_frames{};
  double position_error_max{};
  double normal_error_max{};
  double attribute_error_max{};
  std::uint64_t source_geometry_bytes{};
  std::uint64_t canonical_geometry_bytes{};
  std::uint64_t provenance_bytes{};
  std::uint64_t cage_bytes{};
  std::uint64_t blas_uncompacted_bytes{};
  std::uint64_t blas_bytes{};
  std::uint64_t tlas_bytes{};
  std::uint64_t scratch_bytes{};
  std::uint64_t instance_buffer_bytes{};
  std::uint64_t ray_buffer_bytes{};
  std::uint64_t hit_buffer_bytes{};
  double cage_deformation_ms{};
  double transform_generation_ms{};
  double instance_generation_ms{};
  double blas_ms{};
  double blas_gpu_ms{};
  double compaction_ms{};
  double compaction_gpu_ms{};
  double tlas_ms{};
  double tlas_gpu_ms{};
  double synchronization_ms{};
  double traversal_ms{};
  double traversal_gpu_ms{};
  double tlas_refit_ms{};
  double tlas_rebuild_ms{};
  double fallback_ms{};
  double fallback_decision_oracle_ms{};
  double cpu_validation_ms{};
  std::uint64_t cpu_oracle_rays{};
  double final_merge_ms{};
  double minimization_replay_ms{};
  double minimization_total_ms{};
  double transfer_ms{};
  double total_ms{};
  bool runtime_shader_compiled{};
  bool gpu_instance_generation{};
  bool gpu_attribute_reconstruction{};
  bool mirrored_instances{};
  std::vector<std::string> mismatch_samples;
  std::vector<std::string> fallback_samples;
  std::vector<std::string> final_hit_records;
};

double milliseconds(Clock::time_point begin, Clock::time_point end) {
  return std::chrono::duration<double, std::milli>(end - begin).count();
}

std::string utc_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
  gmtime_r(&now, &utc);
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
      if (static_cast<unsigned char>(character) < 0x20U) {
        output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
               << static_cast<unsigned int>(static_cast<unsigned char>(character)) << std::dec;
      } else {
        output << character;
      }
    }
  }
  return output.str();
}

std::string string_from_ns(NSString *value) {
  if (value == nil) {
    return {};
  }
  const char *utf8 = value.UTF8String;
  return utf8 == nullptr ? std::string{} : std::string(utf8);
}

std::string hex_u64(std::uint64_t value) {
  std::ostringstream output;
  output << std::hex << std::setw(16) << std::setfill('0') << value;
  return output.str();
}

void emit_number_or_null(std::ostringstream &output, bool measured, double value) {
  if (measured) {
    output << value;
  } else {
    output << "null";
  }
}

void emit_integer_or_null(std::ostringstream &output, bool measured, std::uint64_t value) {
  if (measured) {
    output << value;
  } else {
    output << "null";
  }
}

std::string manifest_json(const RunMeasurements &run, const MetalFastPathOptions &options,
                          const std::string &command) {
  const bool measured = run.status == "measured";
  const bool validated = measured && options.cpu_validation;
  const std::uint64_t peak_bytes = run.source_geometry_bytes + run.canonical_geometry_bytes +
                                   run.provenance_bytes + run.cage_bytes + run.blas_bytes +
                                   run.tlas_bytes + run.scratch_bytes + run.instance_buffer_bytes +
                                   run.ray_buffer_bytes + run.hit_buffer_bytes;
  std::ostringstream output;
  output << std::setprecision(17);
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"run_id\": \"metal-fast-" << hex_u64(run.asset_hash) << "\",\n"
         << "  \"timestamp_utc\": \"" << utc_timestamp() << "\",\n"
         << "  \"source\": {\"commit\": \"" << TETCAGE_GIT_COMMIT
         << "\", \"dirty\": " << (TETCAGE_GIT_DIRTY != 0 ? "true" : "false") << "},\n"
         << "  \"build\": {\"compiler\": \"" << TETCAGE_COMPILER_ID
         << "\", \"compiler_version\": \"" << TETCAGE_COMPILER_VERSION << "\", \"build_type\": \""
         << TETCAGE_BUILD_TYPE << "\"},\n"
         << "  \"host\": {\"os\": \"macOS\", \"os_version\": \"" << TETCAGE_SYSTEM_VERSION
         << "\", \"architecture\": \"arm64\"},\n"
         << "  \"backend\": {\"api\": \"metal\", \"status\": \"" << run.status
         << "\", \"device\": ";
  if (run.device.empty()) {
    output << "null";
  } else {
    output << '"' << json_escape(run.device) << '"';
  }
  output << ", \"driver\": \"Metal framework\", "
            "\"capability_manifest\": \"results/capabilities/2026-07-27-metal.json\"},\n"
         << "  \"scene\": {\"id\": \"metal-fast-path\", \"hash\": \"" << hex_u64(run.asset_hash)
         << "\", \"seed\": 81002718},\n"
         << "  \"configuration\": {\n"
         << "    \"copies\": " << options.copies << ",\n"
         << "    \"requested_rays\": " << options.ray_count << ",\n"
         << "    \"frames\": " << options.frames << ",\n"
         << "    \"rebuild_period\": " << options.rebuild_period << ",\n"
         << "    \"motion_amplitude\": " << options.motion_amplitude << ",\n"
         << "    \"compact_blas\": " << (options.compact_blas ? "true" : "false") << ",\n"
         << "    \"extended_limits\": " << (options.extended_limits ? "true" : "false") << ",\n"
         << "    \"boundary_policy\": \""
         << (!options.cpu_validation
                 ? "gpu_only_no_cpu_oracle"
                 : (options.boundary_fallback ? "cpu_fallback_experimental" : "hardware_all_rays"))
         << "\",\n"
         << "    \"cpu_validation\": " << (options.cpu_validation ? "true" : "false") << ",\n"
         << "    \"instance_generation\": \""
         << (options.gpu_instances ? "gpu_compute_descriptor" : "cpu_direct_baseline") << "\",\n"
         << "    \"blas_usage\": \"static_prefer_fast_intersection_when_available\",\n"
         << "    \"triangle_culling\": \"custom_double_sided_projected_edges\"\n"
         << "  },\n"
         << "  \"timings_ms\": {\n"
         << "    \"cage_deformation\": ";
  emit_number_or_null(output, measured, run.cage_deformation_ms);
  output << ",\n    \"transform_generation\": ";
  emit_number_or_null(output, measured, run.transform_generation_ms);
  output << ",\n    \"instance_generation\": ";
  emit_number_or_null(output, measured, run.instance_generation_ms);
  output << ",\n    \"blas\": ";
  emit_number_or_null(output, measured, run.blas_ms + run.compaction_ms);
  output << ",\n    \"tlas\": ";
  emit_number_or_null(output, measured, run.tlas_ms);
  output << ",\n    \"synchronization\": ";
  emit_number_or_null(output, measured, run.synchronization_ms);
  output << ",\n    \"traversal\": ";
  emit_number_or_null(output, measured, run.traversal_ms);
  output << ",\n    \"shading\": null";
  output << ",\n    \"total\": ";
  emit_number_or_null(output, measured, run.total_ms);
  output << ",\n    \"warmup_iterations\": 0,\n"
         << "    \"measured_iterations\": " << (measured ? 1 : 0) << "\n"
         << "  },\n"
         << "  \"memory_bytes\": {\n"
         << "    \"source_geometry\": ";
  emit_integer_or_null(output, measured, run.source_geometry_bytes);
  output << ",\n    \"canonical_geometry\": ";
  emit_integer_or_null(output, measured, run.canonical_geometry_bytes);
  output << ",\n    \"provenance\": ";
  emit_integer_or_null(output, measured, run.provenance_bytes);
  output << ",\n    \"cage\": ";
  emit_integer_or_null(output, measured, run.cage_bytes);
  output << ",\n    \"blas\": ";
  emit_integer_or_null(output, measured, run.blas_bytes);
  output << ",\n    \"tlas\": ";
  emit_integer_or_null(output, measured, run.tlas_bytes);
  output << ",\n    \"scratch\": ";
  emit_integer_or_null(output, measured, run.scratch_bytes);
  output << ",\n    \"instance_buffers\": ";
  emit_integer_or_null(output, measured, run.instance_buffer_bytes);
  output << ",\n    \"api_objects\": null,\n"
         << "    \"renderer_state\": null,\n"
         << "    \"peak_total\": ";
  emit_integer_or_null(output, measured, peak_bytes);
  output << "\n  },\n"
         << "  \"correctness\": {\n"
         << "    \"rays\": ";
  emit_integer_or_null(output, measured, run.rays);
  output << ",\n    \"misses\": ";
  emit_integer_or_null(output, validated, run.misses);
  output << ",\n    \"duplicate_hits\": ";
  emit_integer_or_null(output, validated, 0U);
  output << ",\n    \"wrong_ownership\": ";
  emit_integer_or_null(output, validated, run.wrong_ownership);
  output << ",\n    \"boundary_sensitive_rays\": ";
  emit_integer_or_null(output, validated, run.boundary_sensitive_rays);
  output << ",\n    \"gpu_eligible_rays\": ";
  emit_integer_or_null(output, measured, run.gpu_eligible_rays);
  output << ",\n    \"cpu_fallback_rays\": ";
  emit_integer_or_null(output, measured, run.cpu_fallback_rays);
  output << ",\n    \"cpu_fallback_hits\": ";
  emit_integer_or_null(output, measured, run.cpu_fallback_hits);
  output << ",\n    \"hardware_mismatches\": ";
  emit_integer_or_null(output, validated, run.hardware_mismatches);
  output << ",\n    \"position_error_max\": ";
  emit_number_or_null(output, validated, run.position_error_max);
  output << ",\n    \"normal_error_max\": ";
  emit_number_or_null(output, validated, run.normal_error_max);
  output << ",\n    \"attribute_error_max\": ";
  emit_number_or_null(output, validated, run.attribute_error_max);
  output << ",\n    \"image_error\": null\n"
         << "  },\n"
         << "  \"statistics\": {\n"
         << "    \"traversal_mode\": "
            "\"procedural_aabb_expansion3_projected_edges\",\n"
         << "    \"occupied_micro_blas\": " << run.blas_count << ",\n"
         << "    \"instances\": " << run.total_instances << ",\n"
         << "    \"blas_uncompacted_bytes\": " << run.blas_uncompacted_bytes << ",\n"
         << "    \"blas_compacted_bytes\": " << run.blas_bytes << ",\n"
         << "    \"blas_build_gpu_ms\": " << run.blas_gpu_ms << ",\n"
         << "    \"blas_compaction_gpu_ms\": " << run.compaction_gpu_ms << ",\n"
         << "    \"tlas_build_gpu_ms\": " << run.tlas_gpu_ms << ",\n"
         << "    \"traversal_gpu_ms\": " << run.traversal_gpu_ms << ",\n"
         << "    \"frames_completed\": " << run.completed_frames << ",\n"
         << "    \"static_blas_update_frames\": 0,\n"
         << "    \"static_blas_update_policy\": "
            "\"not_applicable_canonical_geometry_is_immutable\",\n"
         << "    \"tlas_refit_frames\": " << run.tlas_refit_frames << ",\n"
         << "    \"tlas_rebuild_frames\": " << run.tlas_rebuild_frames << ",\n"
         << "    \"tlas_refit_ms\": " << run.tlas_refit_ms << ",\n"
         << "    \"periodic_tlas_rebuild_ms\": " << run.tlas_rebuild_ms << ",\n"
         << "    \"fallback_ms\": " << run.fallback_ms << ",\n"
         << "    \"fallback_decision_oracle_ms\": " << run.fallback_decision_oracle_ms << ",\n"
         << "    \"fallback_selection_requires_cpu_oracle_for_all_rays\": "
         << (options.cpu_validation ? "true" : "false") << ",\n"
         << "    \"cpu_oracle_constructed\": "
         << (measured && options.cpu_validation ? "true" : "false") << ",\n"
         << "    \"cpu_oracle_rays\": " << run.cpu_oracle_rays << ",\n"
         << "    \"cpu_validation_ms\": " << run.cpu_validation_ms << ",\n"
         << "    \"final_merge_ms\": " << run.final_merge_ms << ",\n"
         << "    \"same_work_comparator\": {\"rays\": " << run.rays
         << ", \"hardware_only_trace_ms\": " << run.traversal_ms
         << ", \"selection_oracle_ms\": " << run.fallback_decision_oracle_ms
         << ", \"selection_oracle_scope\": "
         << (options.cpu_validation
                 ? "\"cpu_trace_boundary_test_hardware_comparison_and_path_choice\""
                 : "\"disabled_gpu_only\"")
         << ", \"selected_fallback_ms\": " << run.fallback_ms
         << ", \"selected_fallback_cost_accounting\": "
            "\"subset_of_selection_oracle_ms_reused_without_retrace\""
         << ", \"merge_ms\": " << run.final_merge_ms
         << ", \"end_to_end_trace_selection_merge_ms\": "
         << (run.traversal_ms + run.fallback_decision_oracle_ms + run.final_merge_ms)
         << ", \"end_to_end_formula\": "
            "\"hardware_only_trace_ms + selection_oracle_ms + merge_ms\""
         << ", \"full_run_total_ms\": " << run.total_ms << "},\n"
         << "    \"minimization_replays\": " << run.minimization_replays << ",\n"
         << "    \"minimization_replay_ms\": " << run.minimization_replay_ms << ",\n"
         << "    \"minimization_total_ms\": " << run.minimization_total_ms << ",\n"
         << "    \"minimized_regressions\": " << run.minimized_regressions << ",\n"
         << "    \"minimization_failures\": " << run.minimization_failures << ",\n"
         << "    \"final_stream_records\": " << run.final_hit_records.size() << ",\n"
         << "    \"final_stream_errors\": " << run.final_stream_errors << ",\n"
         << "    \"final_stream_validated\": "
         << (validated && run.final_hit_records.size() == run.rays && run.final_stream_errors == 0U
                 ? "true"
                 : "false")
         << ",\n"
         << "    \"final_stream_complete\": "
         << (measured && run.final_hit_records.size() == run.rays && run.final_stream_errors == 0U
                 ? "true"
                 : "false")
         << ",\n"
         << "    \"transfer_ms\": " << run.transfer_ms << ",\n"
         << "    \"transfer_model\": \"shared_unified_memory_no_explicit_dense_mesh_copy\",\n"
         << "    \"dense_cpu_mesh_regenerations\": 0,\n"
         << "    \"gpu_attribute_recovery_ms\": null,\n"
         << "    \"gpu_attribute_recovery_timing\": "
            "\"unavailable_fused_with_traversal_kernel\",\n"
         << "    \"runtime_shader_compiled\": " << (run.runtime_shader_compiled ? "true" : "false")
         << ",\n"
         << "    \"gpu_attribute_reconstruction\": "
         << (run.gpu_attribute_reconstruction ? "true" : "false") << ",\n"
         << "    \"gpu_instance_generation\": " << (run.gpu_instance_generation ? "true" : "false")
         << ",\n"
         << "    \"mirrored_instances\": " << (run.mirrored_instances ? "true" : "false") << ",\n"
         << "    \"mismatch_samples\": [";
  for (std::size_t index = 0; index < run.mismatch_samples.size(); ++index) {
    output << run.mismatch_samples[index];
    if (index + 1U != run.mismatch_samples.size()) {
      output << ", ";
    }
  }
  output << "],\n"
         << "    \"fallback_samples\": [";
  for (std::size_t index = 0; index < run.fallback_samples.size(); ++index) {
    output << run.fallback_samples[index];
    if (index + 1U != run.fallback_samples.size()) {
      output << ", ";
    }
  }
  output << "],\n"
         << "    \"final_hit_records\": [";
  for (std::size_t index = 0; index < run.final_hit_records.size(); ++index) {
    output << run.final_hit_records[index];
    if (index + 1U != run.final_hit_records.size()) {
      output << ", ";
    }
  }
  output << "]\n"
         << "  },\n"
         << "  \"failures\": [";
  if (!run.failure_code.empty()) {
    output << "{\"code\": \"" << json_escape(run.failure_code) << "\", \"message\": \""
           << json_escape(run.failure_message) << "\"}";
  }
  output << "],\n"
         << "  \"command\": \"" << json_escape(command) << "\",\n"
         << "  \"evidence_class\": \"" << run.evidence_class << "\"\n"
         << "}\n";
  return output.str();
}

std::array<float, 3> split_double(double value) {
  const float high = static_cast<float>(value);
  const double high_remainder = value - static_cast<double>(high);
  const float middle = static_cast<float>(high_remainder);
  const float low = static_cast<float>(high_remainder - static_cast<double>(middle));
  return {high, middle, low};
}

PackedTripleFloat3 split_vec3(Vec3 value) {
  const auto [high_x, middle_x, low_x] = split_double(value.x);
  const auto [high_y, middle_y, low_y] = split_double(value.y);
  const auto [high_z, middle_z, low_z] = split_double(value.z);
  return {{high_x, high_y, high_z}, 0.0F, {middle_x, middle_y, middle_z}, 0.0F,
          {low_x, low_y, low_z},    0.0F};
}

std::vector<BlasGroup> make_blas_groups(const CompiledAsset &asset) {
  std::map<std::uint32_t, BlasGroup> by_tet;
  for (std::uint32_t index = 0; index < asset.micro_triangles.size(); ++index) {
    const auto &triangle = asset.micro_triangles[index];
    auto &group = by_tet[triangle.tet_id];
    group.tet_id = triangle.tet_id;
    group.micro_triangle_indices.push_back(index);
    for (const auto vertex_index : triangle.vertex_indices) {
      const auto &barycentric = asset.generated_vertices[vertex_index].cage_barycentric;
      group.vertices.push_back({static_cast<float>(barycentric.y),
                                static_cast<float>(barycentric.z),
                                static_cast<float>(barycentric.w), 0.0F});
      group.precise_vertices.push_back(
          split_vec3(Vec3{barycentric.y, barycentric.z, barycentric.w}));
    }
  }
  std::vector<BlasGroup> groups;
  groups.reserve(by_tet.size());
  for (auto &[tet_id, group] : by_tet) {
    (void)tet_id;
    groups.push_back(std::move(group));
  }
  return groups;
}

std::vector<GpuAabb> make_conservative_aabbs(const BlasGroup &group) {
  std::vector<GpuAabb> bounds;
  bounds.reserve(group.micro_triangle_indices.size());
  for (std::size_t triangle = 0; triangle < group.micro_triangle_indices.size(); ++triangle) {
    const auto first = triangle * 3U;
    const std::array<BlasVertex, 3> vertices{group.vertices[first], group.vertices[first + 1U],
                                             group.vertices[first + 2U]};
    GpuAabb bound{};
    auto expand_axis = [&](auto component, float &minimum, float &maximum) {
      minimum = std::min({component(vertices[0]), component(vertices[1]), component(vertices[2])});
      maximum = std::max({component(vertices[0]), component(vertices[1]), component(vertices[2])});
      const float scale = std::max({1.0F, std::abs(minimum), std::abs(maximum)});
      const float margin = 32.0F * std::numeric_limits<float>::epsilon() * scale;
      minimum -= margin;
      maximum += margin;
    };
    expand_axis([](const BlasVertex &vertex) { return vertex.x; }, bound.minimum.x,
                bound.maximum.x);
    expand_axis([](const BlasVertex &vertex) { return vertex.y; }, bound.minimum.y,
                bound.maximum.y);
    expand_axis([](const BlasVertex &vertex) { return vertex.z; }, bound.minimum.z,
                bound.maximum.z);
    bounds.push_back(bound);
  }
  return bounds;
}

const SourceTriangle *find_source_triangle(const CompiledAsset &asset, std::uint32_t primitive_id) {
  const auto found = std::find_if(asset.source.triangles.begin(), asset.source.triangles.end(),
                                  [primitive_id](const SourceTriangle &triangle) {
                                    return triangle.primitive_id == primitive_id;
                                  });
  return found == asset.source.triangles.end() ? nullptr : &*found;
}

std::vector<GpuProvenance> make_provenance(const CompiledAsset &asset,
                                           const std::vector<BlasGroup> &groups,
                                           std::vector<std::uint32_t> &primitive_offsets) {
  std::vector<GpuProvenance> provenance;
  primitive_offsets.reserve(groups.size() + 1U);
  for (const auto &group : groups) {
    primitive_offsets.push_back(static_cast<std::uint32_t>(provenance.size()));
    for (const std::uint32_t micro_index : group.micro_triangle_indices) {
      const auto &micro = asset.micro_triangles[micro_index];
      const SourceTriangle *source = find_source_triangle(asset, micro.source_primitive);
      if (source == nullptr) {
        throw std::runtime_error("micro triangle references an unknown source primitive");
      }
      GpuProvenance record{};
      record.source_primitive = micro.source_primitive;
      record.material = micro.material;
      for (std::size_t corner = 0; corner < 3U; ++corner) {
        const auto &generated = asset.generated_vertices[micro.vertex_indices[corner]];
        record.source_barycentric[corner] = {static_cast<float>(generated.source_barycentric.x),
                                             static_cast<float>(generated.source_barycentric.y),
                                             static_cast<float>(generated.source_barycentric.z)};
        const auto &source_vertex = asset.source.vertices[source->vertex_indices[corner]];
        record.normals[corner] = {static_cast<float>(source_vertex.normal.x),
                                  static_cast<float>(source_vertex.normal.y),
                                  static_cast<float>(source_vertex.normal.z)};
        record.uvs[corner] = {static_cast<float>(source_vertex.uv.x),
                              static_cast<float>(source_vertex.uv.y)};
      }
      provenance.push_back(record);
    }
  }
  primitive_offsets.push_back(static_cast<std::uint32_t>(provenance.size()));
  return provenance;
}

std::vector<Vec3> animated_pose(const CompiledAsset &asset, double amplitude, std::uint32_t copy,
                                std::uint32_t frame) {
  std::vector<Vec3> pose = asset.cage.vertices;
  for (std::size_t index = 0; index < pose.size(); ++index) {
    const double phase = static_cast<double>(index + 1U) + static_cast<double>(frame) * 0.375;
    pose[index].x += amplitude * 0.25 * std::cos(phase * 0.5);
    pose[index].y += amplitude * 0.5 * std::sin(phase * 0.7);
    pose[index].z += amplitude * std::sin(phase);
    pose[index].x += static_cast<double>(copy) * 1000.0;
  }
  return pose;
}

std::size_t align_up(std::size_t value, std::size_t alignment) {
  return (value + alignment - 1U) & ~(alignment - 1U);
}

double command_gpu_ms(id<MTLCommandBuffer> command_buffer) {
  if (command_buffer.GPUEndTime <= command_buffer.GPUStartTime) {
    return 0.0;
  }
  return (command_buffer.GPUEndTime - command_buffer.GPUStartTime) * 1000.0;
}

void require_completed(id<MTLCommandBuffer> command_buffer, const char *stage) {
  if (command_buffer.status == MTLCommandBufferStatusCompleted) {
    return;
  }
  std::string message(stage);
  message += " command buffer failed";
  if (command_buffer.error != nil) {
    message += ": ";
    message += string_from_ns(command_buffer.error.localizedDescription);
  }
  throw std::runtime_error(message);
}

MTLAccelerationStructureUsage static_blas_usage() {
  MTLAccelerationStructureUsage usage = MTLAccelerationStructureUsageNone;
#if defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 260000
  if (@available(macOS 26.0, *)) {
    usage |= MTLAccelerationStructureUsagePreferFastIntersection;
  }
#endif
  return usage;
}

NSString *trace_kernel_source(bool extended_limits) {
  // Metal's intersection_query tag sequence accepts instancing/triangle_data,
  // but not extended_limits. Extended limits are selected on the acceleration
  // structure descriptor and do not need a query template tag.
  (void)extended_limits;
  const std::string tags = "instancing";
  std::string source = R"METAL(
#include <metal_stdlib>
#include <metal_raytracing>
using namespace metal;
using namespace metal::raytracing;

struct TraceRay {
  packed_float3 origin;
  float minimum_distance;
  packed_float3 direction;
  float maximum_distance;
  packed_float3 origin_low;
  float minimum_distance_low;
  packed_float3 direction_low;
  float maximum_distance_low;
  packed_float3 origin_middle;
  float minimum_distance_middle;
  packed_float3 direction_middle;
  float maximum_distance_middle;
};

struct Provenance {
  uint source_primitive;
  uint material;
  packed_float3 source_barycentric[3];
  packed_float3 normals[3];
  packed_float2 uvs[3];
};

struct DoubleFloat3 {
  packed_float3 high;
  float padding0;
  packed_float3 middle;
  float padding1;
  packed_float3 low;
  float padding2;
};

struct PreciseTransform {
  DoubleFloat3 columns[4];
};

struct TraceHit {
  uint hit;
  uint primitive_id;
  uint user_instance_id;
  uint source_primitive;
  uint material;
  uint front_facing;
  uint reserved0;
  uint reserved1;
  float distance;
  packed_float2 triangle_barycentric;
  float reserved2;
  packed_float3 source_barycentric;
  float reserved3;
  packed_float3 normal;
  float reserved4;
  packed_float2 uv;
  float reserved5[2];
};

struct DoubleFloat {
  float high;
  float middle;
  float low;
};

struct DoubleFloatVector3 {
  DoubleFloat x;
  DoubleFloat y;
  DoubleFloat z;
};

DoubleFloat make_double_float(float high, float middle = 0.0f, float low = 0.0f) {
  return {high, middle, low};
}

DoubleFloat renormalize_double_float(float high, float middle, float low) {
  float sum0 = high + middle;
  float error0 = middle - (sum0 - high);
  float sum1 = sum0 + low;
  float error1 = low - (sum1 - sum0);
  float combined_error = error0 + error1;
  float virtual_error1 = combined_error - error0;
  float combined_error_tail =
      (error0 - (combined_error - virtual_error1)) + (error1 - virtual_error1);
  float normalized_high = sum1 + combined_error;
  float normalized_middle = combined_error - (normalized_high - sum1);
  return {normalized_high, normalized_middle, combined_error_tail};
}

DoubleFloat add_double_float(DoubleFloat a, DoubleFloat b) {
  float high_sum = a.high + b.high;
  float virtual_b = high_sum - a.high;
  float high_error = (a.high - (high_sum - virtual_b)) + (b.high - virtual_b);
  float middle_sum = a.middle + b.middle;
  float middle_virtual_b = middle_sum - a.middle;
  float middle_error =
      (a.middle - (middle_sum - middle_virtual_b)) + (b.middle - middle_virtual_b);
  return renormalize_double_float(
      high_sum, high_error + middle_sum,
      middle_error + a.low + b.low);
}

DoubleFloat negate_double_float(DoubleFloat value) {
  return {-value.high, -value.middle, -value.low};
}

DoubleFloat subtract_double_float(DoubleFloat a, DoubleFloat b) {
  return add_double_float(a, negate_double_float(b));
}

DoubleFloat multiply_double_float(DoubleFloat a, DoubleFloat b) {
  float product = a.high * b.high;
  float product_error = fma(a.high, b.high, -product);
  float middle =
      product_error + a.high * b.middle + a.middle * b.high;
  float low = a.high * b.low + a.middle * b.middle + a.low * b.high;
  return renormalize_double_float(product, middle, low);
}

DoubleFloat divide_double_float(DoubleFloat numerator, DoubleFloat denominator) {
  float quotient_high = numerator.high / denominator.high;
  DoubleFloat remainder =
      subtract_double_float(numerator, multiply_double_float(denominator,
                                                             make_double_float(quotient_high)));
  float quotient_middle = remainder.high / denominator.high;
  remainder = subtract_double_float(
      remainder, multiply_double_float(denominator, make_double_float(quotient_middle)));
  float quotient_low =
      (remainder.high + remainder.middle + remainder.low) / denominator.high;
  return renormalize_double_float(quotient_high, quotient_middle, quotient_low);
}

float double_float_value(DoubleFloat value) {
  return value.high + value.middle + value.low;
}

bool double_float_less(DoubleFloat a, DoubleFloat b) {
  return a.high < b.high ||
         (a.high == b.high &&
          (a.middle < b.middle || (a.middle == b.middle && a.low < b.low)));
}

DoubleFloatVector3 load_double_float3(DoubleFloat3 value) {
  return {{value.high.x, value.middle.x, value.low.x},
          {value.high.y, value.middle.y, value.low.y},
          {value.high.z, value.middle.z, value.low.z}};
}

DoubleFloat df_component(DoubleFloatVector3 value, uint axis) {
  return axis == 0U ? value.x : (axis == 1U ? value.y : value.z);
}

DoubleFloatVector3 add_double_float3(DoubleFloatVector3 a, DoubleFloatVector3 b) {
  return {add_double_float(a.x, b.x), add_double_float(a.y, b.y),
          add_double_float(a.z, b.z)};
}

DoubleFloatVector3 subtract_double_float3(DoubleFloatVector3 a, DoubleFloatVector3 b) {
  return {subtract_double_float(a.x, b.x), subtract_double_float(a.y, b.y),
          subtract_double_float(a.z, b.z)};
}

DoubleFloatVector3 multiply_double_float3(DoubleFloatVector3 value, DoubleFloat scalar) {
  return {multiply_double_float(value.x, scalar), multiply_double_float(value.y, scalar),
          multiply_double_float(value.z, scalar)};
}

DoubleFloat dot_double_float3(DoubleFloatVector3 a, DoubleFloatVector3 b) {
  return add_double_float(
      add_double_float(multiply_double_float(a.x, b.x), multiply_double_float(a.y, b.y)),
      multiply_double_float(a.z, b.z));
}

DoubleFloatVector3 cross_double_float3(DoubleFloatVector3 a, DoubleFloatVector3 b) {
  return {
      subtract_double_float(multiply_double_float(a.y, b.z), multiply_double_float(a.z, b.y)),
      subtract_double_float(multiply_double_float(a.z, b.x), multiply_double_float(a.x, b.z)),
      subtract_double_float(multiply_double_float(a.x, b.y), multiply_double_float(a.y, b.x))};
}

float3 double_float3_value(DoubleFloatVector3 value) {
  return float3(double_float_value(value.x), double_float_value(value.y),
                double_float_value(value.z));
}

DoubleFloatVector3 transform_precise_point(PreciseTransform transform,
                                           DoubleFloatVector3 point) {
  DoubleFloatVector3 result = load_double_float3(transform.columns[3]);
  result = add_double_float3(
      result, multiply_double_float3(load_double_float3(transform.columns[0]), point.x));
  result = add_double_float3(
      result, multiply_double_float3(load_double_float3(transform.columns[1]), point.y));
  return add_double_float3(
      result, multiply_double_float3(load_double_float3(transform.columns[2]), point.z));
}

DoubleFloat orient2d_precise(DoubleFloat ax, DoubleFloat ay, DoubleFloat bx, DoubleFloat by,
                             DoubleFloat cx, DoubleFloat cy) {
  return subtract_double_float(
      multiply_double_float(subtract_double_float(bx, ax), subtract_double_float(cy, ay)),
      multiply_double_float(subtract_double_float(by, ay), subtract_double_float(cx, ax)));
}

bool intersect_projected_edges_precise(
    DoubleFloatVector3 origin, DoubleFloatVector3 direction, DoubleFloat minimum_distance,
    DoubleFloat maximum_distance, DoubleFloatVector3 vertex0, DoubleFloatVector3 vertex1,
    DoubleFloatVector3 vertex2, thread DoubleFloat &distance,
    thread float2 &triangle_barycentric, thread bool &front_facing) {
  DoubleFloatVector3 edge01 = subtract_double_float3(vertex1, vertex0);
  DoubleFloatVector3 edge02 = subtract_double_float3(vertex2, vertex0);
  DoubleFloatVector3 normal = cross_double_float3(edge01, edge02);
  DoubleFloat denominator = dot_double_float3(normal, direction);
  float3 normal_value = double_float3_value(normal);
  float3 direction_value = double_float3_value(direction);
  float scale = max(FLT_MIN, length(normal_value) * length(direction_value));
  if (abs(double_float_value(denominator)) <= 1.0e-12f * scale) {
    return false;
  }
  distance = divide_double_float(
      dot_double_float3(normal, subtract_double_float3(vertex0, origin)), denominator);
  if (double_float_less(distance, minimum_distance) ||
      double_float_less(maximum_distance, distance)) {
    return false;
  }

  DoubleFloatVector3 point =
      add_double_float3(origin, multiply_double_float3(direction, distance));
  float3 absolute_normal = abs(normal_value);
  uint dropped_axis = 0U;
  if (absolute_normal.y > absolute_normal.x) {
    dropped_axis = 1U;
  }
  if (absolute_normal.z > absolute_normal[dropped_axis]) {
    dropped_axis = 2U;
  }
  uint x_axis = (dropped_axis + 1U) % 3U;
  uint y_axis = (dropped_axis + 2U) % 3U;
  DoubleFloat area = orient2d_precise(
      df_component(vertex0, x_axis), df_component(vertex0, y_axis), df_component(vertex1, x_axis),
      df_component(vertex1, y_axis), df_component(vertex2, x_axis), df_component(vertex2, y_axis));
  if (area.high == 0.0f && area.middle == 0.0f && area.low == 0.0f) {
    return false;
  }
  DoubleFloat weight0 = divide_double_float(
      orient2d_precise(df_component(vertex1, x_axis), df_component(vertex1, y_axis),
                       df_component(vertex2, x_axis), df_component(vertex2, y_axis),
                       df_component(point, x_axis), df_component(point, y_axis)),
      area);
  DoubleFloat weight1 = divide_double_float(
      orient2d_precise(df_component(vertex2, x_axis), df_component(vertex2, y_axis),
                       df_component(vertex0, x_axis), df_component(vertex0, y_axis),
                       df_component(point, x_axis), df_component(point, y_axis)),
      area);
  DoubleFloat weight2 =
      subtract_double_float(make_double_float(1.0f), add_double_float(weight0, weight1));

  float3 edge12 = double_float3_value(subtract_double_float3(vertex2, vertex1));
  float maximum_edge_squared =
      max(dot(double_float3_value(edge01), double_float3_value(edge01)),
          max(dot(edge12, edge12),
              dot(double_float3_value(edge02), double_float3_value(edge02))));
  float triangle_condition = maximum_edge_squared / max(FLT_MIN, length(normal_value));
  float relative_denominator = abs(double_float_value(denominator)) / scale;
  float tolerance =
      min(1.0e-6f,
          max(2.0e-11f, 128.0f * 2.220446049250313e-16f * triangle_condition /
                              max(relative_denominator, 1.0e-12f)));
  if (double_float_value(weight0) < -tolerance ||
      double_float_value(weight1) < -tolerance ||
      double_float_value(weight2) < -tolerance) {
    return false;
  }

  triangle_barycentric =
      float2(double_float_value(weight1), double_float_value(weight2));
  front_facing = double_float_value(denominator) < 0.0f;
  return isfinite(double_float_value(distance)) && all(isfinite(triangle_barycentric));
}

kernel void trace_rays(instance_acceleration_structure scene [[buffer(0)]],
                       device const TraceRay *rays [[buffer(1)]],
                       device TraceHit *hits [[buffer(2)]],
                       device const uint *instance_to_blas [[buffer(3)]],
                       device const uint *primitive_offsets [[buffer(4)]],
                       device const Provenance *provenance [[buffer(5)]],
                       device const DoubleFloat3 *canonical_vertices [[buffer(6)]],
                       device const PreciseTransform *precise_transforms [[buffer(7)]],
                       uint tid [[thread_position_in_grid]]) {
  TraceHit output = {};
  TraceRay input = rays[tid];
  ray query(float3(input.origin), float3(input.direction),
            input.minimum_distance, input.maximum_distance);
  DoubleFloatVector3 precise_origin = {
      {input.origin.x, input.origin_middle.x, input.origin_low.x},
      {input.origin.y, input.origin_middle.y, input.origin_low.y},
      {input.origin.z, input.origin_middle.z, input.origin_low.z}};
  DoubleFloatVector3 precise_direction = {
      {input.direction.x, input.direction_middle.x, input.direction_low.x},
      {input.direction.y, input.direction_middle.y, input.direction_low.y},
      {input.direction.z, input.direction_middle.z, input.direction_low.z}};
  DoubleFloat precise_minimum = {input.minimum_distance, input.minimum_distance_middle,
                                 input.minimum_distance_low};
  DoubleFloat precise_maximum = {input.maximum_distance, input.maximum_distance_middle,
                                 input.maximum_distance_low};
  intersection_params params;
  params.force_opacity(forced_opacity::non_opaque);
  params.assume_geometry_type(geometry_type::bounding_box);
  intersection_query<__INTERSECTOR_TAGS__> intersection(query, scene, params);
  bool found = false;
  DoubleFloat closest_distance = {INFINITY, 0.0f, 0.0f};
  DoubleFloat best_distance = {INFINITY, 0.0f, 0.0f};
  uint best_primitive = 0;
  uint best_user_instance = 0;
  uint best_source_primitive = 0;
  uint best_provenance = 0;
  bool best_front_facing = false;
  float2 best_barycentric = float2(0.0);
  while (intersection.next()) {
    if (intersection.get_candidate_intersection_type() != intersection_type::bounding_box) {
      continue;
    }
    uint candidate_primitive = intersection.get_candidate_primitive_id();
    uint candidate_user_instance = intersection.get_candidate_user_instance_id();
    uint candidate_blas = instance_to_blas[candidate_user_instance];
    uint candidate_provenance =
        primitive_offsets[candidate_blas] + candidate_primitive;
    uint candidate_source_primitive = provenance[candidate_provenance].source_primitive;
    uint vertex_offset = candidate_provenance * 3U;
    PreciseTransform transform = precise_transforms[candidate_user_instance];
    DoubleFloatVector3 vertex0 =
        transform_precise_point(transform, load_double_float3(canonical_vertices[vertex_offset]));
    DoubleFloatVector3 vertex1 = transform_precise_point(
        transform, load_double_float3(canonical_vertices[vertex_offset + 1U]));
    DoubleFloatVector3 vertex2 = transform_precise_point(
        transform, load_double_float3(canonical_vertices[vertex_offset + 2U]));
    DoubleFloat candidate_distance = {0.0f, 0.0f, 0.0f};
    float2 candidate_barycentric = float2(0.0f);
    bool candidate_front_facing = false;
    if (!intersect_projected_edges_precise(
            precise_origin, precise_direction, precise_minimum, precise_maximum, vertex0, vertex1,
            vertex2, candidate_distance, candidate_barycentric, candidate_front_facing)) {
      continue;
    }
    bool closer = !found || double_float_less(candidate_distance, closest_distance);
    DoubleFloat next_closest = closer ? candidate_distance : closest_distance;
    float ownership_tolerance =
        2.0e-10f * max(1.0f, abs(double_float_value(next_closest)));
    bool candidate_in_bucket =
        abs(double_float_value(subtract_double_float(candidate_distance, next_closest))) <=
        ownership_tolerance;
    bool previous_owner_in_bucket =
        found &&
        abs(double_float_value(subtract_double_float(best_distance, next_closest))) <=
            ownership_tolerance;
    bool lower_owner =
        candidate_in_bucket && previous_owner_in_bucket &&
        (candidate_source_primitive < best_source_primitive ||
         (candidate_source_primitive == best_source_primitive &&
          (candidate_blas < instance_to_blas[best_user_instance] ||
           (candidate_blas == instance_to_blas[best_user_instance] &&
            (candidate_provenance < best_provenance ||
             (candidate_provenance == best_provenance &&
              candidate_user_instance < best_user_instance))))));
    if (!found || !previous_owner_in_bucket || lower_owner) {
      found = true;
      best_distance = candidate_distance;
      best_primitive = candidate_primitive;
      best_user_instance = candidate_user_instance;
      best_source_primitive = candidate_source_primitive;
      best_provenance = candidate_provenance;
      best_front_facing = candidate_front_facing;
      best_barycentric = candidate_barycentric;
    }
    closest_distance = next_closest;
  }
  if (found) {
    output.hit = 1;
    output.primitive_id = best_primitive;
    output.user_instance_id = best_user_instance;
    output.front_facing = best_front_facing ? 1 : 0;
    output.distance = double_float_value(best_distance);
    output.triangle_barycentric = packed_float2(best_barycentric);
    uint blas_index = instance_to_blas[output.user_instance_id];
    uint provenance_index = primitive_offsets[blas_index] + output.primitive_id;
    Provenance record = provenance[provenance_index];
    float3 weights = float3(1.0 - output.triangle_barycentric.x -
                                output.triangle_barycentric.y,
                            output.triangle_barycentric.x,
                            output.triangle_barycentric.y);
    float3 source_barycentric =
        float3(record.source_barycentric[0]) * weights.x +
        float3(record.source_barycentric[1]) * weights.y +
        float3(record.source_barycentric[2]) * weights.z;
    float3 normal = float3(record.normals[0]) * source_barycentric.x +
                    float3(record.normals[1]) * source_barycentric.y +
                    float3(record.normals[2]) * source_barycentric.z;
    float2 uv = float2(record.uvs[0]) * source_barycentric.x +
                float2(record.uvs[1]) * source_barycentric.y +
                float2(record.uvs[2]) * source_barycentric.z;
    output.source_primitive = record.source_primitive;
    output.material = record.material;
    output.source_barycentric = packed_float3(source_barycentric);
    output.normal = packed_float3(normalize(normal));
    output.uv = packed_float2(uv);
  }
  hits[tid] = output;
}
)METAL";
  const std::string marker = "__INTERSECTOR_TAGS__";
  source.replace(source.find(marker), marker.size(), tags);
  return [NSString stringWithUTF8String:source.c_str()];
}

NSString *instance_kernel_source() {
  const char *source = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct InstanceDescriptor {
  packed_float3 c0;
  packed_float3 c1;
  packed_float3 c2;
  packed_float3 translation;
  uint options;
  uint mask;
  uint intersection_function_table_offset;
  uint acceleration_structure_index;
  uint user_id;
};

kernel void write_instance_descriptors(
    device const InstanceDescriptor *input [[buffer(0)]],
    device InstanceDescriptor *output [[buffer(1)]],
    uint tid [[thread_position_in_grid]]) {
  output[tid] = input[tid];
}
)METAL";
  return [NSString stringWithUTF8String:source];
}

std::uint64_t source_geometry_bytes(const CompiledAsset &asset) {
  return asset.source.vertices.size() * sizeof(SourceVertex) +
         asset.source.triangles.size() * sizeof(SourceTriangle);
}

std::uint64_t canonical_geometry_bytes(const CompiledAsset &asset) {
  return asset.generated_vertices.size() * sizeof(GeneratedVertex) +
         asset.micro_triangles.size() * sizeof(MicroTriangle);
}

std::uint64_t cage_bytes(const CompiledAsset &asset) {
  return asset.cage.vertices.size() * sizeof(Vec3) +
         asset.cage.vertex_ids.size() * sizeof(std::uint64_t) +
         asset.cage.tetrahedra.size() * sizeof(CageTet);
}

double vec3_error(Vec3 a, PackedFloat3 b) {
  return std::max({std::abs(a.x - static_cast<double>(b.x)),
                   std::abs(a.y - static_cast<double>(b.y)),
                   std::abs(a.z - static_cast<double>(b.z))});
}

double vec2_error(Vec2 a, PackedFloat2 b) {
  return std::max(std::abs(a.x - static_cast<double>(b.x)),
                  std::abs(a.y - static_cast<double>(b.y)));
}

struct HardwareComparison {
  std::string kind;
  double position_error{};
  double normal_error{};
  double attribute_error{};
};

HardwareComparison compare_hardware_hit(const TraceResult &expected_result, const GpuHit &actual,
                                        const std::vector<InstanceInfo> &instance_info,
                                        const Ray &ray) {
  HardwareComparison comparison;
  const bool cpu_hit = expected_result.closest.has_value();
  const bool gpu_hit = actual.hit != 0U;
  if (cpu_hit != gpu_hit) {
    comparison.kind = "hit_presence";
    return comparison;
  }
  if (!cpu_hit) {
    return comparison;
  }
  const auto &expected = *expected_result.closest;
  if (actual.user_instance_id >= instance_info.size() ||
      instance_info[actual.user_instance_id].copy != 0U ||
      actual.source_primitive != expected.source_primitive ||
      actual.material != expected.material) {
    comparison.kind = "ownership";
    return comparison;
  }
  comparison.position_error =
      std::abs(expected.t - static_cast<double>(actual.distance)) * length(ray.direction);
  comparison.normal_error = vec3_error(expected.normal, actual.normal);
  comparison.attribute_error =
      std::max(vec3_error(expected.source_barycentric, actual.source_barycentric),
               vec2_error(expected.uv, actual.uv));
  if (comparison.position_error > 2.5e-5 || comparison.normal_error > 2.5e-5 ||
      comparison.attribute_error > 2.5e-5) {
    comparison.kind = "value";
  }
  return comparison;
}

MetalMismatchClass classify_recorded_mismatch(const Ray &ray, const TraceResult &expected_result,
                                              const TraceResult &exact_result, const GpuHit &actual,
                                              const std::optional<Vec4> &expected_cage_bary,
                                              const std::string &kind) {
  const auto &expected = expected_result.closest;
  MetalMismatchSignals signals{};
  signals.cpu_disagrees_with_exact_oracle =
      expected.has_value() != exact_result.closest.has_value() ||
      (expected && exact_result.closest &&
       expected->source_primitive != exact_result.closest->source_primitive);
  signals.hit_presence_differs = expected.has_value() != (actual.hit != 0U);
  signals.ownership_differs = kind == "ownership";
  signals.primitive_matches =
      expected && actual.hit != 0U && expected->source_primitive == actual.source_primitive;
  signals.attributes_differ = kind == "value";
  signals.distance_within_policy =
      expected && actual.hit != 0U &&
      std::abs(expected->t - static_cast<double>(actual.distance)) * length(ray.direction) <=
          2.5e-5;
  signals.shared_edge =
      expected && std::min({expected->source_barycentric.x, expected->source_barycentric.y,
                            expected->source_barycentric.z}) <= 1.0e-6;
  signals.generated_boundary =
      expected_result.raw_hits > 1U || expected_result.raw_duplicate_candidates != 0U ||
      signals.shared_edge ||
      (expected_cage_bary && std::min({expected_cage_bary->x, expected_cage_bary->y,
                                       expected_cage_bary->z, expected_cage_bary->w}) <= 1.0e-5);
  return classify_metal_mismatch(signals);
}

void record_mismatch(RunMeasurements &run, const CompiledAsset &asset,
                     const std::vector<Vec3> &pose, std::size_t index, const Ray &ray,
                     const Ray &minimized_ray, MetalMismatchClass minimized_class,
                     bool minimization_verified, const TraceResult &expected_result,
                     const TraceResult &exact_result, const GpuHit &actual,
                     const std::optional<Vec4> &expected_cage_bary, const std::string &kind) {
  const auto &expected = expected_result.closest;
  const auto classification = classify_recorded_mismatch(ray, expected_result, exact_result, actual,
                                                         expected_cage_bary, kind);

  std::ostringstream sample;
  sample << std::setprecision(17) << "{\"kind\":\"" << kind << "\",\"classification\":\""
         << metal_mismatch_class_name(classification) << "\",\"ray_index\":" << index
         << ",\"ray\":{\"origin\":[" << ray.origin.x << ',' << ray.origin.y << ',' << ray.origin.z
         << "],\"direction\":[" << ray.direction.x << ',' << ray.direction.y << ','
         << ray.direction.z << "],\"minimum_t\":" << ray.minimum_t
         << ",\"maximum_t\":" << ray.maximum_t << "},\"cage\":{\"posed_vertices\":[";
  for (std::size_t vertex = 0; vertex < pose.size(); ++vertex) {
    sample << '[' << pose[vertex].x << ',' << pose[vertex].y << ',' << pose[vertex].z << ']';
    if (vertex + 1U != pose.size()) {
      sample << ',';
    }
  }
  sample << "],\"tetrahedra\":[";
  for (std::size_t tet = 0; tet < asset.cage.tetrahedra.size(); ++tet) {
    const auto &indices = asset.cage.tetrahedra[tet].vertex_indices;
    sample << '[' << indices[0] << ',' << indices[1] << ',' << indices[2] << ',' << indices[3]
           << ']';
    if (tet + 1U != asset.cage.tetrahedra.size()) {
      sample << ',';
    }
  }
  sample << "]},\"embedded_triangle\":";
  const std::uint32_t source_primitive =
      expected ? expected->source_primitive : actual.source_primitive;
  const SourceTriangle *triangle = find_source_triangle(asset, source_primitive);
  if (triangle == nullptr) {
    sample << "null";
  } else {
    sample << "{\"primitive\":" << triangle->primitive_id
           << ",\"material\":" << triangle->material_id << ",\"positions\":[";
    for (std::size_t corner = 0; corner < 3U; ++corner) {
      const auto &position = asset.source.vertices[triangle->vertex_indices[corner]].position;
      sample << '[' << position.x << ',' << position.y << ',' << position.z << ']';
      if (corner != 2U) {
        sample << ',';
      }
    }
    sample << "]}";
  }
  sample << ",\"cpu_hit\":" << (expected ? "true" : "false")
         << ",\"gpu_hit\":" << (actual.hit != 0U ? "true" : "false") << ",\"cpu_t\":";
  if (expected) {
    sample << expected->t;
  } else {
    sample << "null";
  }
  sample << ",\"gpu_t\":";
  if (actual.hit != 0U) {
    sample << actual.distance;
  } else {
    sample << "null";
  }
  sample << ",\"gpu_triangle_bary\":[" << actual.triangle_barycentric.x << ','
         << actual.triangle_barycentric.y << "],\"cpu_source_bary\":";
  if (expected) {
    sample << '[' << expected->source_barycentric.x << ',' << expected->source_barycentric.y << ','
           << expected->source_barycentric.z << "],\"cpu_tet_id\":" << expected->tet_id;
  } else {
    sample << "null";
  }
  sample << ",\"cpu_cage_bary\": ";
  if (expected_cage_bary) {
    sample << '[' << expected_cage_bary->x << ',' << expected_cage_bary->y << ','
           << expected_cage_bary->z << ',' << expected_cage_bary->w << ']';
  } else {
    sample << "null";
  }
  sample << ",\"expected_primitive\":";
  if (expected) {
    sample << expected->source_primitive;
  } else {
    sample << "null";
  }
  sample << ",\"expected_owner_tet\":";
  if (expected) {
    sample << expected->tet_id;
  } else {
    sample << "null";
  }
  sample << ",\"expected_micro_triangle\":";
  if (expected) {
    sample << expected->micro_triangle;
  } else {
    sample << "null";
  }
  sample << ",\"gpu_source_bary\":[" << actual.source_barycentric.x << ','
         << actual.source_barycentric.y << ',' << actual.source_barycentric.z
         << "],\"gpu_user_instance_id\":" << actual.user_instance_id
         << ",\"gpu_primitive_id\":" << actual.primitive_id
         << ",\"gpu_source_primitive\":" << actual.source_primitive
         << ",\"gpu_material\":" << actual.material
         << ",\"synchronization_proof\":\"completed_command_buffer_before_shared_read\""
         << ",\"minimization\":{\"algorithm\":\"deterministic_coordinate_ddmin\","
            "\"status\":\""
         << (minimization_verified && minimized_class == classification
                 ? "hardware_replay_preserved_classification"
                 : "hardware_replay_failed")
         << "\",\"classification\":\"" << metal_mismatch_class_name(minimized_class)
         << "\",\"ray\":{\"origin\":[" << minimized_ray.origin.x << ',' << minimized_ray.origin.y
         << ',' << minimized_ray.origin.z << "],\"direction\":[" << minimized_ray.direction.x << ','
         << minimized_ray.direction.y << ',' << minimized_ray.direction.z
         << "],\"minimum_t\":" << minimized_ray.minimum_t
         << ",\"maximum_t\":" << minimized_ray.maximum_t << "}}}";
  run.mismatch_samples.push_back(sample.str());
}

Tetrahedron posed_tet(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                      std::uint32_t tet_id) {
  Tetrahedron tet{};
  const auto &source = asset.cage.tetrahedra[tet_id];
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    tet.positions[corner] = pose[source.vertex_indices[corner]];
    tet.vertex_ids[corner] = asset.cage.vertex_ids[source.vertex_indices[corner]];
  }
  return tet;
}

InstanceRecords make_instance_records(const CompiledAsset &asset,
                                      const std::vector<BlasGroup> &groups,
                                      const std::vector<std::vector<Vec3>> &poses) {
  InstanceRecords records;
  const std::size_t count = poses.size() * groups.size();
  records.descriptors.reserve(count);
  records.gpu_inputs.reserve(count);
  records.info.reserve(count);
  records.instance_to_blas.reserve(count);
  records.precise_transforms.reserve(count);
  for (std::uint32_t copy = 0; copy < poses.size(); ++copy) {
    Cage posed_cage = asset.cage;
    posed_cage.vertices = poses[copy];
    const auto frame = build_cycles_tet_cage_frame(asset, posed_cage, copy);
    if (frame.mode != CyclesGeometryMode::procedural_metalrt) {
      throw std::runtime_error("Cycles tet-cage frame selected conventional fallback: " +
                               std::string(cycles_fallback_reason_name(frame.fallback_reason)));
    }
    for (std::uint32_t blas_index = 0; blas_index < groups.size(); ++blas_index) {
      const auto tet_id = groups[blas_index].tet_id;
      if (tet_id >= frame.tet_transforms.size()) {
        throw std::runtime_error("Cycles tet-cage frame is missing a tetrahedron transform");
      }
      const auto &transform = frame.tet_transforms[tet_id];
      const PackedFloat3 c0{
          static_cast<float>(transform.linear.columns[0].x),
          static_cast<float>(transform.linear.columns[0].y),
          static_cast<float>(transform.linear.columns[0].z)};
      const PackedFloat3 c1{
          static_cast<float>(transform.linear.columns[1].x),
          static_cast<float>(transform.linear.columns[1].y),
          static_cast<float>(transform.linear.columns[1].z)};
      const PackedFloat3 c2{
          static_cast<float>(transform.linear.columns[2].x),
          static_cast<float>(transform.linear.columns[2].y),
          static_cast<float>(transform.linear.columns[2].z)};
      const PackedFloat3 translation{
          static_cast<float>(transform.translation.x),
          static_cast<float>(transform.translation.y),
          static_cast<float>(transform.translation.z)};
      MTLAccelerationStructureUserIDInstanceDescriptor descriptor{};
      descriptor.transformationMatrix =
          MTLPackedFloat4x3(MTLPackedFloat3(c0.x, c0.y, c0.z), MTLPackedFloat3(c1.x, c1.y, c1.z),
                            MTLPackedFloat3(c2.x, c2.y, c2.z),
                            MTLPackedFloat3(translation.x, translation.y, translation.z));
      descriptor.options = MTLAccelerationStructureInstanceOptionOpaque |
                           MTLAccelerationStructureInstanceOptionDisableTriangleCulling;
      descriptor.mask = std::numeric_limits<std::uint32_t>::max();
      descriptor.intersectionFunctionTableOffset = 0U;
      descriptor.accelerationStructureIndex = blas_index;
      descriptor.userID = static_cast<std::uint32_t>(records.descriptors.size());
      records.gpu_inputs.push_back({c0, c1, c2, translation,
                                    static_cast<std::uint32_t>(descriptor.options), descriptor.mask,
                                    descriptor.intersectionFunctionTableOffset,
                                    descriptor.accelerationStructureIndex, descriptor.userID});
      records.descriptors.push_back(descriptor);
      records.info.push_back({copy, blas_index, tet_id});
      records.instance_to_blas.push_back(blas_index);
      records.precise_transforms.push_back(
          {split_vec3(transform.linear.columns[0]),
           split_vec3(transform.linear.columns[1]),
           split_vec3(transform.linear.columns[2]),
           split_vec3(transform.translation)});
    }
  }
  return records;
}

bool boundary_sensitive_ray(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                            const Ray &ray, const TraceResult &expected_result) {
  if (!expected_result.closest) {
    return false;
  }
  const auto &expected = *expected_result.closest;
  const double source_edge = std::min({expected.source_barycentric.x, expected.source_barycentric.y,
                                       expected.source_barycentric.z});
  const auto direction = normalized(ray.direction);
  const double normal_alignment = direction ? std::abs(dot(*direction, expected.normal)) : 0.0;
  const auto cage_barycentric =
      expected.tet_id < asset.cage.tetrahedra.size()
          ? to_barycentric(posed_tet(asset, pose, expected.tet_id), expected.position)
          : std::nullopt;
  const double cage_edge = cage_barycentric ? std::min({cage_barycentric->x, cage_barycentric->y,
                                                        cage_barycentric->z, cage_barycentric->w})
                                            : 1.0;
  return expected_result.raw_hits > 1U || expected_result.raw_duplicate_candidates != 0U ||
         source_edge <= 1.0e-6 || cage_edge <= 1.0e-5 || normal_alignment <= 1.0e-5;
}

void record_fallback_sample(RunMeasurements &run, std::size_t index, const Ray &ray,
                            const TraceResult &result) {
  if (run.fallback_samples.size() >= 8U) {
    return;
  }
  std::ostringstream sample;
  sample << std::setprecision(17) << "{\"ray_index\":" << index
         << ",\"result_source\":\"cpu_oracle_final\",\"origin\":[" << ray.origin.x << ','
         << ray.origin.y << ',' << ray.origin.z << "],\"direction\":[" << ray.direction.x << ','
         << ray.direction.y << ',' << ray.direction.z
         << "],\"hit\":" << (result.closest ? "true" : "false");
  if (result.closest) {
    const auto &hit = *result.closest;
    sample << ",\"t\":" << hit.t << ",\"position\":[" << hit.position.x << ',' << hit.position.y
           << ',' << hit.position.z << "],\"source_barycentric\":[" << hit.source_barycentric.x
           << ',' << hit.source_barycentric.y << ',' << hit.source_barycentric.z << "],\"normal\":["
           << hit.normal.x << ',' << hit.normal.y << ',' << hit.normal.z << "],\"uv\":[" << hit.uv.x
           << ',' << hit.uv.y << "],\"source_primitive\":" << hit.source_primitive
           << ",\"material\":" << hit.material << ",\"owner_tet\":" << hit.tet_id;
  }
  sample << '}';
  run.fallback_samples.push_back(sample.str());
}

void record_final_hit(RunMeasurements &run, std::size_t index, MetalFinalPath path, const Ray &ray,
                      const TraceResult &cpu, const GpuHit &hardware,
                      const std::vector<InstanceInfo> &instance_info) {
  std::ostringstream record;
  record << std::setprecision(17) << "{\"ray_index\":" << index << ",\"selected_path\":\""
         << metal_final_path_name(path) << "\",\"hit\":";
  if (path == MetalFinalPath::cpu_fallback) {
    record << (cpu.closest ? "true" : "false");
    if (cpu.closest) {
      const auto &hit = *cpu.closest;
      record << ",\"t\":" << hit.t << ",\"position\":[" << hit.position.x << ',' << hit.position.y
             << ',' << hit.position.z << "],\"source_barycentric\":[" << hit.source_barycentric.x
             << ',' << hit.source_barycentric.y << ',' << hit.source_barycentric.z
             << "],\"normal\":[" << hit.normal.x << ',' << hit.normal.y << ',' << hit.normal.z
             << "],\"uv\":[" << hit.uv.x << ',' << hit.uv.y
             << "],\"source_primitive\":" << hit.source_primitive
             << ",\"material\":" << hit.material << ",\"owner_tet\":" << hit.tet_id
             << ",\"micro_triangle\":" << hit.micro_triangle;
    }
  } else {
    record << (hardware.hit != 0U ? "true" : "false");
    if (hardware.hit != 0U) {
      const Vec3 position = ray.origin + ray.direction * static_cast<double>(hardware.distance);
      record << ",\"t\":" << hardware.distance << ",\"position\":[" << position.x << ','
             << position.y << ',' << position.z << "],\"source_barycentric\":["
             << hardware.source_barycentric.x << ',' << hardware.source_barycentric.y << ','
             << hardware.source_barycentric.z << "],\"normal\":[" << hardware.normal.x << ','
             << hardware.normal.y << ',' << hardware.normal.z << "],\"uv\":[" << hardware.uv.x
             << ',' << hardware.uv.y << "],\"source_primitive\":" << hardware.source_primitive
             << ",\"material\":" << hardware.material << ",\"owner_tet\":";
      if (hardware.user_instance_id < instance_info.size()) {
        record << instance_info[hardware.user_instance_id].tet_id;
      } else {
        record << "null";
      }
      record << ",\"micro_triangle\":" << hardware.primitive_id;
    }
  }
  record << '}';
  run.final_hit_records.push_back(record.str());
}

MetalFastPathOutcome run_impl(const CompiledAsset &asset, const MetalFastPathOptions &options,
                              const std::string &command) {
  RunMeasurements run{};
  const auto total_begin = Clock::now();
  const auto serialized = serialize_asset(asset);
  run.asset_hash = asset_checksum(serialized);
  run.source_geometry_bytes = source_geometry_bytes(asset);
  run.canonical_geometry_bytes = canonical_geometry_bytes(asset);
  run.provenance_bytes = asset.statistics.provenance_bytes;
  run.cage_bytes = cage_bytes(asset);

  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  if (device == nil) {
    run.status = "unverified";
    run.evidence_class = "blocked";
    run.failure_code = "metal_device_unavailable";
    run.failure_message = "MTLCreateSystemDefaultDevice returned nil in this execution context";
    run.total_ms = milliseconds(total_begin, Clock::now());
    return {manifest_json(run, options, command), options.allow_unverified ? 0 : 2};
  }
  run.device = string_from_ns(device.name);
  if (!device.supportsRaytracing) {
    run.status = "unsupported";
    run.evidence_class = "direct";
    run.failure_code = "ray_tracing_unsupported";
    run.failure_message = "the selected MTLDevice reports supportsRaytracing=false";
    run.total_ms = milliseconds(total_begin, Clock::now());
    return {manifest_json(run, options, command), 2};
  }

  id<MTLCommandQueue> queue = [device newCommandQueue];
  if (queue == nil) {
    throw std::runtime_error("failed to create Metal command queue");
  }

  const auto groups = make_blas_groups(asset);
  if (groups.empty()) {
    throw std::runtime_error("asset has no occupied tetrahedra");
  }
  run.blas_count = groups.size();

  std::vector<std::uint32_t> primitive_offsets;
  const auto provenance = make_provenance(asset, groups, primitive_offsets);
  run.provenance_bytes +=
      provenance.size() * sizeof(GpuProvenance) + primitive_offsets.size() * sizeof(std::uint32_t);

  std::vector<PackedTripleFloat3> canonical_vertices;
  for (const auto &group : groups) {
    canonical_vertices.insert(canonical_vertices.end(), group.precise_vertices.begin(),
                              group.precise_vertices.end());
  }
  id<MTLBuffer> canonical_vertex_buffer =
      [device newBufferWithBytes:canonical_vertices.data()
                          length:canonical_vertices.size() * sizeof(PackedTripleFloat3)
                         options:MTLResourceStorageModeShared];
  if (canonical_vertex_buffer == nil) {
    throw std::runtime_error("failed to allocate canonical micro-mesh vertex buffer");
  }
  run.canonical_geometry_bytes += canonical_vertex_buffer.allocatedSize;

  NSMutableArray<id<MTLBuffer>> *bounding_box_buffers = [NSMutableArray array];
  NSMutableArray<MTLPrimitiveAccelerationStructureDescriptor *> *blas_descriptors =
      [NSMutableArray array];
  NSMutableArray<id<MTLAccelerationStructure>> *blas = [NSMutableArray array];
  std::vector<MTLAccelerationStructureSizes> blas_sizes;
  std::vector<std::size_t> scratch_offsets;
  std::size_t total_scratch = 0U;

  for (const auto &group : groups) {
    const auto bounds = make_conservative_aabbs(group);
    id<MTLBuffer> bounding_box_buffer = [device newBufferWithBytes:bounds.data()
                                                            length:bounds.size() * sizeof(GpuAabb)
                                                           options:MTLResourceStorageModeShared];
    if (bounding_box_buffer == nil) {
      throw std::runtime_error("failed to allocate conservative micro-mesh bounds buffer");
    }
    [bounding_box_buffers addObject:bounding_box_buffer];
    run.canonical_geometry_bytes += bounding_box_buffer.allocatedSize;

    auto *geometry = [MTLAccelerationStructureBoundingBoxGeometryDescriptor descriptor];
    geometry.boundingBoxBuffer = bounding_box_buffer;
    geometry.boundingBoxBufferOffset = 0U;
    geometry.boundingBoxStride = sizeof(GpuAabb);
    geometry.boundingBoxCount = bounds.size();
    geometry.opaque = NO;

    auto *descriptor = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
    descriptor.geometryDescriptors = @[ geometry ];
    descriptor.usage = static_blas_usage();
    const auto sizes = [device accelerationStructureSizesWithDescriptor:descriptor];
    if (sizes.accelerationStructureSize == 0U || sizes.buildScratchBufferSize == 0U) {
      throw std::runtime_error("Metal returned zero size for a micro-BLAS");
    }
    id<MTLAccelerationStructure> acceleration_structure =
        [device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
    if (acceleration_structure == nil) {
      throw std::runtime_error("failed to allocate micro-BLAS");
    }
    scratch_offsets.push_back(align_up(total_scratch, 256U));
    total_scratch = scratch_offsets.back() + sizes.buildScratchBufferSize;
    blas_sizes.push_back(sizes);
    [blas_descriptors addObject:descriptor];
    [blas addObject:acceleration_structure];
    run.blas_uncompacted_bytes += sizes.accelerationStructureSize;
  }

  id<MTLBuffer> blas_scratch = [device newBufferWithLength:std::max<std::size_t>(total_scratch, 1U)
                                                   options:MTLResourceStorageModePrivate];
  if (blas_scratch == nil) {
    throw std::runtime_error("failed to allocate pooled micro-BLAS scratch buffer");
  }
  run.scratch_bytes = total_scratch;

  id<MTLBuffer> compacted_size_buffer = nil;
  if (options.compact_blas) {
    compacted_size_buffer = [device newBufferWithLength:groups.size() * sizeof(std::uint64_t)
                                                options:MTLResourceStorageModeShared];
    if (compacted_size_buffer == nil) {
      throw std::runtime_error("failed to allocate BLAS compaction size buffer");
    }
  }

  const auto blas_begin = Clock::now();
  id<MTLCommandBuffer> blas_commands = [queue commandBuffer];
  id<MTLAccelerationStructureCommandEncoder> blas_encoder =
      [blas_commands accelerationStructureCommandEncoder];
  for (NSUInteger index = 0; index < blas.count; ++index) {
    [blas_encoder buildAccelerationStructure:blas[index]
                                  descriptor:blas_descriptors[index]
                               scratchBuffer:blas_scratch
                         scratchBufferOffset:scratch_offsets[index]];
    if (options.compact_blas) {
      [blas_encoder writeCompactedAccelerationStructureSize:blas[index]
                                                   toBuffer:compacted_size_buffer
                                                     offset:index * sizeof(std::uint64_t)
                                               sizeDataType:MTLDataTypeULong];
    }
  }
  [blas_encoder endEncoding];
  [blas_commands commit];
  const auto sync_begin = Clock::now();
  [blas_commands waitUntilCompleted];
  const auto sync_end = Clock::now();
  require_completed(blas_commands, "micro-BLAS build");
  const auto blas_end = Clock::now();
  run.blas_ms = milliseconds(blas_begin, blas_end);
  run.blas_gpu_ms = command_gpu_ms(blas_commands);
  run.synchronization_ms += milliseconds(sync_begin, sync_end);

  run.blas_bytes = run.blas_uncompacted_bytes;
  if (options.compact_blas) {
    auto *compacted_sizes = static_cast<const std::uint64_t *>(compacted_size_buffer.contents);
    NSMutableArray<id<MTLAccelerationStructure>> *compacted = [blas mutableCopy];
    std::vector<bool> should_compact(groups.size());
    bool any_compaction = false;
    run.blas_bytes = 0U;
    for (std::size_t index = 0; index < groups.size(); ++index) {
      const std::uint64_t compacted_size = compacted_sizes[index];
      if (compacted_size == 0U) {
        throw std::runtime_error("Metal returned zero compacted micro-BLAS size");
      }
      if (compacted_size < blas_sizes[index].accelerationStructureSize) {
        id<MTLAccelerationStructure> destination =
            [device newAccelerationStructureWithSize:compacted_size];
        if (destination == nil) {
          throw std::runtime_error("failed to allocate compacted micro-BLAS");
        }
        compacted[index] = destination;
        should_compact[index] = true;
        any_compaction = true;
        run.blas_bytes += compacted_size;
      } else {
        run.blas_bytes += blas_sizes[index].accelerationStructureSize;
      }
    }
    if (any_compaction) {
      const auto compaction_begin = Clock::now();
      id<MTLCommandBuffer> compaction_commands = [queue commandBuffer];
      id<MTLAccelerationStructureCommandEncoder> compaction_encoder =
          [compaction_commands accelerationStructureCommandEncoder];
      for (NSUInteger index = 0; index < blas.count; ++index) {
        if (should_compact[index]) {
          [compaction_encoder copyAndCompactAccelerationStructure:blas[index]
                                          toAccelerationStructure:compacted[index]];
        }
      }
      [compaction_encoder endEncoding];
      [compaction_commands commit];
      const auto compact_sync_begin = Clock::now();
      [compaction_commands waitUntilCompleted];
      const auto compact_sync_end = Clock::now();
      require_completed(compaction_commands, "micro-BLAS compaction");
      const auto compaction_end = Clock::now();
      run.compaction_ms = milliseconds(compaction_begin, compaction_end);
      run.compaction_gpu_ms = command_gpu_ms(compaction_commands);
      run.synchronization_ms += milliseconds(compact_sync_begin, compact_sync_end);
      blas = compacted;
    }
  }

  const auto cage_begin = Clock::now();
  std::vector<std::vector<Vec3>> poses;
  poses.reserve(options.copies);
  for (std::uint32_t copy = 0; copy < options.copies; ++copy) {
    poses.push_back(animated_pose(asset, options.motion_amplitude, copy, 0U));
  }
  const auto cage_end = Clock::now();
  run.cage_deformation_ms = milliseconds(cage_begin, cage_end);

  const auto transform_begin = Clock::now();
  auto instance_records = make_instance_records(asset, groups, poses);
  auto &descriptors = instance_records.descriptors;
  auto &gpu_instance_inputs = instance_records.gpu_inputs;
  auto &instance_info = instance_records.info;
  auto &instance_to_blas = instance_records.instance_to_blas;
  auto &precise_transforms = instance_records.precise_transforms;
  run.mirrored_instances = instance_records.mirrored;
  const auto transform_end = Clock::now();
  run.transform_generation_ms = milliseconds(transform_begin, transform_end);
  run.total_instances = descriptors.size();

  const auto instance_begin = Clock::now();
  id<MTLBuffer> instance_input_buffer = nil;
  id<MTLBuffer> instance_buffer = nil;
  id<MTLComputePipelineState> instance_pipeline = nil;
  NSUInteger instance_threads = 0U;
  if (options.gpu_instances) {
    instance_input_buffer =
        [device newBufferWithBytes:gpu_instance_inputs.data()
                            length:gpu_instance_inputs.size() * sizeof(GpuInstanceInput)
                           options:MTLResourceStorageModeShared];
    instance_buffer = [device newBufferWithLength:descriptors.size() * sizeof(GpuInstanceInput)
                                          options:MTLResourceStorageModeShared];
    if (instance_input_buffer == nil || instance_buffer == nil) {
      throw std::runtime_error("failed to allocate GPU instance descriptor buffers");
    }
    NSError *instance_library_error = nil;
    id<MTLLibrary> instance_library = [device newLibraryWithSource:instance_kernel_source()
                                                           options:nil
                                                             error:&instance_library_error];
    if (instance_library == nil) {
      throw std::runtime_error("Metal instance descriptor shader compilation failed: " +
                               string_from_ns(instance_library_error.localizedDescription));
    }
    id<MTLFunction> instance_function =
        [instance_library newFunctionWithName:@"write_instance_descriptors"];
    NSError *instance_pipeline_error = nil;
    instance_pipeline = [device newComputePipelineStateWithFunction:instance_function
                                                              error:&instance_pipeline_error];
    if (instance_pipeline == nil) {
      throw std::runtime_error("Metal instance descriptor pipeline creation failed: " +
                               string_from_ns(instance_pipeline_error.localizedDescription));
    }
    id<MTLCommandBuffer> instance_commands = [queue commandBuffer];
    id<MTLComputeCommandEncoder> instance_encoder = [instance_commands computeCommandEncoder];
    [instance_encoder setComputePipelineState:instance_pipeline];
    [instance_encoder setBuffer:instance_input_buffer offset:0U atIndex:0U];
    [instance_encoder setBuffer:instance_buffer offset:0U atIndex:1U];
    instance_threads =
        std::min<NSUInteger>(instance_pipeline.maxTotalThreadsPerThreadgroup,
                             std::max<NSUInteger>(instance_pipeline.threadExecutionWidth, 1U));
    [instance_encoder dispatchThreads:MTLSizeMake(descriptors.size(), 1U, 1U)
                threadsPerThreadgroup:MTLSizeMake(instance_threads, 1U, 1U)];
    [instance_encoder endEncoding];
    [instance_commands commit];
    const auto instance_sync_begin = Clock::now();
    [instance_commands waitUntilCompleted];
    const auto instance_sync_end = Clock::now();
    require_completed(instance_commands, "GPU instance descriptor generation");
    run.synchronization_ms += milliseconds(instance_sync_begin, instance_sync_end);
    run.gpu_instance_generation = true;
  } else {
    instance_buffer =
        [device newBufferWithBytes:descriptors.data()
                            length:descriptors.size() *
                                   sizeof(MTLAccelerationStructureUserIDInstanceDescriptor)
                           options:MTLResourceStorageModeShared];
  }
  id<MTLBuffer> instance_to_blas_buffer =
      [device newBufferWithBytes:instance_to_blas.data()
                          length:instance_to_blas.size() * sizeof(std::uint32_t)
                         options:MTLResourceStorageModeShared];
  id<MTLBuffer> precise_transform_buffer = [device
      newBufferWithBytes:precise_transforms.data()
                  length:precise_transforms.size() * sizeof(std::array<PackedTripleFloat3, 4>)
                 options:MTLResourceStorageModeShared];
  if (instance_buffer == nil || instance_to_blas_buffer == nil || precise_transform_buffer == nil) {
    throw std::runtime_error("failed to allocate Metal instance metadata buffers");
  }
  const auto instance_end = Clock::now();
  run.instance_generation_ms = milliseconds(instance_begin, instance_end);
  run.instance_buffer_bytes = instance_buffer.allocatedSize + instance_to_blas_buffer.allocatedSize;
  run.instance_buffer_bytes += precise_transform_buffer.allocatedSize;
  if (instance_input_buffer != nil) {
    run.instance_buffer_bytes += instance_input_buffer.allocatedSize;
  }

  auto *tlas_descriptor = [MTLInstanceAccelerationStructureDescriptor descriptor];
  tlas_descriptor.instanceDescriptorBuffer = instance_buffer;
  tlas_descriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeUserID;
  tlas_descriptor.instanceCount = descriptors.size();
  tlas_descriptor.instancedAccelerationStructures = blas;
  MTLAccelerationStructureUsage tlas_usage = MTLAccelerationStructureUsageRefit;
  if (options.extended_limits) {
    tlas_usage |= MTLAccelerationStructureUsageExtendedLimits;
  }
  tlas_descriptor.usage = tlas_usage;
  const auto tlas_sizes = [device accelerationStructureSizesWithDescriptor:tlas_descriptor];
  if (tlas_sizes.accelerationStructureSize == 0U || tlas_sizes.buildScratchBufferSize == 0U) {
    throw std::runtime_error("Metal returned zero TLAS allocation size");
  }
  id<MTLAccelerationStructure> tlas =
      [device newAccelerationStructureWithSize:tlas_sizes.accelerationStructureSize];
  id<MTLBuffer> tlas_scratch = [device newBufferWithLength:tlas_sizes.buildScratchBufferSize
                                                   options:MTLResourceStorageModePrivate];
  if (tlas == nil || tlas_scratch == nil) {
    throw std::runtime_error("failed to allocate TLAS or TLAS scratch");
  }
  run.tlas_bytes = tlas.allocatedSize;
  run.scratch_bytes += tlas_scratch.allocatedSize;

  const auto tlas_begin = Clock::now();
  id<MTLCommandBuffer> tlas_commands = [queue commandBuffer];
  id<MTLAccelerationStructureCommandEncoder> tlas_encoder =
      [tlas_commands accelerationStructureCommandEncoder];
  for (id<MTLAccelerationStructure> micro_blas in blas) {
    [tlas_encoder useResource:micro_blas usage:MTLResourceUsageRead];
  }
  [tlas_encoder buildAccelerationStructure:tlas
                                descriptor:tlas_descriptor
                             scratchBuffer:tlas_scratch
                       scratchBufferOffset:0U];
  [tlas_encoder endEncoding];
  [tlas_commands commit];
  const auto tlas_sync_begin = Clock::now();
  [tlas_commands waitUntilCompleted];
  const auto tlas_sync_end = Clock::now();
  require_completed(tlas_commands, "TLAS build");
  const auto tlas_end = Clock::now();
  run.tlas_ms = milliseconds(tlas_begin, tlas_end);
  run.tlas_gpu_ms = command_gpu_ms(tlas_commands);
  run.synchronization_ms += milliseconds(tlas_sync_begin, tlas_sync_end);

  NSError *library_error = nil;
  MTLCompileOptions *trace_compile_options = [MTLCompileOptions new];
  if (@available(macOS 15.0, *)) {
    trace_compile_options.mathMode = MTLMathModeSafe;
  } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    trace_compile_options.fastMathEnabled = NO;
#pragma clang diagnostic pop
  }
  id<MTLLibrary> library = [device newLibraryWithSource:trace_kernel_source(options.extended_limits)
                                                options:trace_compile_options
                                                  error:&library_error];
  if (library == nil) {
    throw std::runtime_error("Metal runtime shader compilation failed: " +
                             string_from_ns(library_error.localizedDescription));
  }
  id<MTLFunction> function = [library newFunctionWithName:@"trace_rays"];
  NSError *pipeline_error = nil;
  id<MTLComputePipelineState> pipeline =
      [device newComputePipelineStateWithFunction:function error:&pipeline_error];
  if (pipeline == nil) {
    throw std::runtime_error("Metal trace pipeline creation failed: " +
                             string_from_ns(pipeline_error.localizedDescription));
  }
  run.runtime_shader_compiled = true;

  std::optional<Bvh4D> exact_bvh;
  if (options.cpu_validation) {
    exact_bvh = build_bvh4d(asset, 4U);
  }
  auto make_rays = [&](const std::vector<Vec3> &pose) {
    auto frame_rays = generate_adversarial_rays(asset, pose, 81002718ULL, options.ray_count);
    if (frame_rays.empty()) {
      throw std::runtime_error("ray generator returned no correctness rays");
    }
    if (frame_rays.size() > options.ray_count) {
      frame_rays.resize(options.ray_count);
    }
    while (frame_rays.size() < options.ray_count) {
      frame_rays.push_back(
          frame_rays[frame_rays.size() % std::max<std::size_t>(frame_rays.size(), 1U)]);
    }
    return frame_rays;
  };
  auto pack_rays = [](const std::vector<Ray> &frame_rays) {
    std::vector<GpuRay> packed;
    packed.reserve(frame_rays.size());
    for (const auto &ray : frame_rays) {
      const auto origin = split_vec3(ray.origin);
      const auto direction = split_vec3(ray.direction);
      const auto [minimum_high, minimum_middle, minimum_low] = split_double(ray.minimum_t);
      const auto [maximum_high, maximum_middle, maximum_low] = split_double(ray.maximum_t);
      packed.push_back({origin.high, minimum_high, direction.high, maximum_high, origin.low,
                        minimum_low, direction.low, maximum_low, origin.middle, minimum_middle,
                        direction.middle, maximum_middle});
    }
    return packed;
  };
  auto rays = make_rays(poses.front());
  auto gpu_rays = pack_rays(rays);

  const auto transfer_begin = Clock::now();
  id<MTLBuffer> ray_buffer = [device newBufferWithBytes:gpu_rays.data()
                                                 length:gpu_rays.size() * sizeof(GpuRay)
                                                options:MTLResourceStorageModeShared];
  id<MTLBuffer> hit_buffer = [device newBufferWithLength:gpu_rays.size() * sizeof(GpuHit)
                                                 options:MTLResourceStorageModeShared];
  id<MTLBuffer> primitive_offset_buffer =
      [device newBufferWithBytes:primitive_offsets.data()
                          length:primitive_offsets.size() * sizeof(std::uint32_t)
                         options:MTLResourceStorageModeShared];
  id<MTLBuffer> provenance_buffer =
      [device newBufferWithBytes:provenance.data()
                          length:provenance.size() * sizeof(GpuProvenance)
                         options:MTLResourceStorageModeShared];
  if (ray_buffer == nil || hit_buffer == nil || primitive_offset_buffer == nil ||
      provenance_buffer == nil) {
    throw std::runtime_error("failed to allocate Metal trace/provenance buffers");
  }
  run.ray_buffer_bytes = ray_buffer.allocatedSize;
  run.hit_buffer_bytes = hit_buffer.allocatedSize;
  run.provenance_bytes += primitive_offset_buffer.allocatedSize + provenance_buffer.allocatedSize;
  run.transfer_ms += milliseconds(transfer_begin, Clock::now());

  const NSUInteger thread_width =
      std::min<NSUInteger>(pipeline.maxTotalThreadsPerThreadgroup,
                           std::max<NSUInteger>(pipeline.threadExecutionWidth, 1U));
  auto trace_frame = [&]() {
    const auto traversal_begin = Clock::now();
    id<MTLCommandBuffer> trace_commands = [queue commandBuffer];
    id<MTLComputeCommandEncoder> trace_encoder = [trace_commands computeCommandEncoder];
    [trace_encoder setComputePipelineState:pipeline];
    [trace_encoder setAccelerationStructure:tlas atBufferIndex:0U];
    [trace_encoder setBuffer:ray_buffer offset:0U atIndex:1U];
    [trace_encoder setBuffer:hit_buffer offset:0U atIndex:2U];
    [trace_encoder setBuffer:instance_to_blas_buffer offset:0U atIndex:3U];
    [trace_encoder setBuffer:primitive_offset_buffer offset:0U atIndex:4U];
    [trace_encoder setBuffer:provenance_buffer offset:0U atIndex:5U];
    [trace_encoder setBuffer:canonical_vertex_buffer offset:0U atIndex:6U];
    [trace_encoder setBuffer:precise_transform_buffer offset:0U atIndex:7U];
    for (id<MTLAccelerationStructure> micro_blas in blas) {
      [trace_encoder useResource:micro_blas usage:MTLResourceUsageRead];
    }
    [trace_encoder dispatchThreads:MTLSizeMake(gpu_rays.size(), 1U, 1U)
             threadsPerThreadgroup:MTLSizeMake(thread_width, 1U, 1U)];
    [trace_encoder endEncoding];
    [trace_commands commit];
    const auto trace_sync_begin = Clock::now();
    [trace_commands waitUntilCompleted];
    const auto trace_sync_end = Clock::now();
    require_completed(trace_commands, "ray traversal");
    run.traversal_ms += milliseconds(traversal_begin, Clock::now());
    run.traversal_gpu_ms += command_gpu_ms(trace_commands);
    run.synchronization_ms += milliseconds(trace_sync_begin, trace_sync_end);
  };

  auto replay_hardware_ray = [&](const Ray &candidate) {
    const auto origin = split_vec3(candidate.origin);
    const auto direction = split_vec3(candidate.direction);
    const auto [minimum_high, minimum_middle, minimum_low] = split_double(candidate.minimum_t);
    const auto [maximum_high, maximum_middle, maximum_low] = split_double(candidate.maximum_t);
    const GpuRay packed{origin.high,   minimum_high,   direction.high,   maximum_high,
                        origin.low,    minimum_low,    direction.low,    maximum_low,
                        origin.middle, minimum_middle, direction.middle, maximum_middle};
    std::memcpy(ray_buffer.contents, &packed, sizeof(packed));
    const auto replay_begin = Clock::now();
    id<MTLCommandBuffer> replay_commands = [queue commandBuffer];
    id<MTLComputeCommandEncoder> replay_encoder = [replay_commands computeCommandEncoder];
    [replay_encoder setComputePipelineState:pipeline];
    [replay_encoder setAccelerationStructure:tlas atBufferIndex:0U];
    [replay_encoder setBuffer:ray_buffer offset:0U atIndex:1U];
    [replay_encoder setBuffer:hit_buffer offset:0U atIndex:2U];
    [replay_encoder setBuffer:instance_to_blas_buffer offset:0U atIndex:3U];
    [replay_encoder setBuffer:primitive_offset_buffer offset:0U atIndex:4U];
    [replay_encoder setBuffer:provenance_buffer offset:0U atIndex:5U];
    [replay_encoder setBuffer:canonical_vertex_buffer offset:0U atIndex:6U];
    [replay_encoder setBuffer:precise_transform_buffer offset:0U atIndex:7U];
    for (id<MTLAccelerationStructure> micro_blas in blas) {
      [replay_encoder useResource:micro_blas usage:MTLResourceUsageRead];
    }
    [replay_encoder dispatchThreads:MTLSizeMake(1U, 1U, 1U)
              threadsPerThreadgroup:MTLSizeMake(1U, 1U, 1U)];
    [replay_encoder endEncoding];
    [replay_commands commit];
    [replay_commands waitUntilCompleted];
    require_completed(replay_commands, "mismatch minimization replay");
    run.minimization_replay_ms += milliseconds(replay_begin, Clock::now());
    ++run.minimization_replays;
    return *static_cast<const GpuHit *>(hit_buffer.contents);
  };

  auto validate_frame = [&](std::uint32_t frame) {
    const auto *gpu_hits = static_cast<const GpuHit *>(hit_buffer.contents);
    const std::vector<GpuHit> frame_hardware_hits(gpu_hits, gpu_hits + rays.size());
    run.rays += rays.size();
    run.gpu_eligible_rays += rays.size();
    if (!options.cpu_validation) {
      const TraceResult unused_cpu{};
      for (std::size_t index = 0; index < rays.size(); ++index) {
        const std::size_t corpus_index = static_cast<std::size_t>(frame) * rays.size() + index;
        const auto merge_begin = Clock::now();
        record_final_hit(run, corpus_index, MetalFinalPath::hardware, rays[index], unused_cpu,
                         frame_hardware_hits[index], instance_info);
        run.final_merge_ms += milliseconds(merge_begin, Clock::now());
      }
      ++run.completed_frames;
      return;
    }
    for (std::size_t index = 0; index < rays.size(); ++index) {
      const std::size_t corpus_index = static_cast<std::size_t>(frame) * rays.size() + index;
      const auto validation_begin = Clock::now();
      const auto selection_begin = Clock::now();
      const auto cpu_begin = selection_begin;
      const auto cpu = trace_fast(asset, poses.front(), rays[index]);
      const auto cpu_end = Clock::now();
      const bool boundary =
          cpu.closest && boundary_sensitive_ray(asset, poses.front(), rays[index], cpu);
      const GpuHit actual = frame_hardware_hits[index];
      const auto comparison = compare_hardware_hit(cpu, actual, instance_info, rays[index]);
      const MetalFinalPath final_path = choose_metal_final_path(options.boundary_fallback, boundary,
                                                                !comparison.kind.empty(), true);
      run.fallback_decision_oracle_ms += milliseconds(selection_begin, Clock::now());

      if (boundary) {
        ++run.boundary_sensitive_rays;
      }
      const auto exact = trace_watertight4d(asset, *exact_bvh, poses.front(), rays[index],
                                            ProjectionMode::bounded_simplex);
      ++run.cpu_oracle_rays;
      std::optional<Vec4> expected_cage_bary;
      if (cpu.closest && cpu.closest->tet_id < asset.cage.tetrahedra.size()) {
        expected_cage_bary = to_barycentric(posed_tet(asset, poses.front(), cpu.closest->tet_id),
                                            cpu.closest->position);
      }
      run.cpu_validation_ms += milliseconds(validation_begin, Clock::now());
      if (!comparison.kind.empty()) {
        ++run.hardware_mismatches;
        const auto classification = classify_recorded_mismatch(rays[index], cpu, exact, actual,
                                                               expected_cage_bary, comparison.kind);
        const auto minimization_begin = Clock::now();
        const auto preserves_classification = [&](const Ray &candidate) {
          if (!std::isfinite(length(candidate.direction)) ||
              length(candidate.direction) <= 1.0e-12 || candidate.minimum_t > candidate.maximum_t) {
            return false;
          }
          const auto candidate_cpu = trace_fast(asset, poses.front(), candidate);
          const auto candidate_exact = trace_watertight4d(
              asset, *exact_bvh, poses.front(), candidate, ProjectionMode::bounded_simplex);
          const GpuHit candidate_hardware = replay_hardware_ray(candidate);
          const auto candidate_comparison =
              compare_hardware_hit(candidate_cpu, candidate_hardware, instance_info, candidate);
          if (candidate_comparison.kind.empty()) {
            return false;
          }
          std::optional<Vec4> candidate_cage_bary;
          if (candidate_cpu.closest &&
              candidate_cpu.closest->tet_id < asset.cage.tetrahedra.size()) {
            candidate_cage_bary =
                to_barycentric(posed_tet(asset, poses.front(), candidate_cpu.closest->tet_id),
                               candidate_cpu.closest->position);
          }
          return classify_recorded_mismatch(candidate, candidate_cpu, candidate_exact,
                                            candidate_hardware, candidate_cage_bary,
                                            candidate_comparison.kind) == classification;
        };
        const Ray minimized = minimize_metal_mismatch_ray(rays[index], preserves_classification);
        const bool minimization_verified = preserves_classification(minimized);
        run.minimization_total_ms += milliseconds(minimization_begin, Clock::now());
        if (minimization_verified) {
          ++run.minimized_regressions;
        } else {
          ++run.minimization_failures;
        }
        record_mismatch(run, asset, poses.front(), corpus_index, rays[index], minimized,
                        classification, minimization_verified, cpu, exact, actual,
                        expected_cage_bary, comparison.kind);
      }
      if (final_path == MetalFinalPath::cpu_fallback) {
        --run.gpu_eligible_rays;
        ++run.cpu_fallback_rays;
        if (cpu.closest) {
          ++run.cpu_fallback_hits;
        }
        run.fallback_ms += milliseconds(cpu_begin, cpu_end);
        record_fallback_sample(run, corpus_index, rays[index], cpu);
      }
      const auto merge_begin = Clock::now();
      record_final_hit(run, corpus_index, final_path, rays[index], cpu, actual, instance_info);
      run.final_merge_ms += milliseconds(merge_begin, Clock::now());
      if (final_path == MetalFinalPath::hardware && !comparison.kind.empty()) {
        ++run.final_stream_errors;
        if (comparison.kind == "hit_presence") {
          ++run.misses;
        } else if (comparison.kind == "ownership") {
          ++run.wrong_ownership;
        }
      }
      if (final_path == MetalFinalPath::hardware) {
        run.position_error_max = std::max(run.position_error_max, comparison.position_error);
        run.normal_error_max = std::max(run.normal_error_max, comparison.normal_error);
        run.attribute_error_max = std::max(run.attribute_error_max, comparison.attribute_error);
      }
    }
    ++run.completed_frames;
  };

  trace_frame();
  validate_frame(0U);

  for (std::uint32_t frame = 1U; frame < options.frames; ++frame) {
    const auto frame_pose_begin = Clock::now();
    poses.clear();
    for (std::uint32_t copy = 0; copy < options.copies; ++copy) {
      poses.push_back(animated_pose(asset, options.motion_amplitude, copy, frame));
    }
    run.cage_deformation_ms += milliseconds(frame_pose_begin, Clock::now());

    const auto frame_transform_begin = Clock::now();
    instance_records = make_instance_records(asset, groups, poses);
    run.transform_generation_ms += milliseconds(frame_transform_begin, Clock::now());
    run.mirrored_instances = run.mirrored_instances || instance_records.mirrored;
    if (instance_records.descriptors.size() != descriptors.size()) {
      throw std::runtime_error("animation changed the Metal TLAS instance count");
    }

    const auto frame_instance_begin = Clock::now();
    std::memcpy(precise_transform_buffer.contents, instance_records.precise_transforms.data(),
                instance_records.precise_transforms.size() *
                    sizeof(std::array<PackedTripleFloat3, 4>));
    if (options.gpu_instances) {
      std::memcpy(instance_input_buffer.contents, instance_records.gpu_inputs.data(),
                  instance_records.gpu_inputs.size() * sizeof(GpuInstanceInput));
      id<MTLCommandBuffer> instance_commands = [queue commandBuffer];
      id<MTLComputeCommandEncoder> instance_encoder = [instance_commands computeCommandEncoder];
      [instance_encoder setComputePipelineState:instance_pipeline];
      [instance_encoder setBuffer:instance_input_buffer offset:0U atIndex:0U];
      [instance_encoder setBuffer:instance_buffer offset:0U atIndex:1U];
      [instance_encoder dispatchThreads:MTLSizeMake(instance_records.descriptors.size(), 1U, 1U)
                  threadsPerThreadgroup:MTLSizeMake(instance_threads, 1U, 1U)];
      [instance_encoder endEncoding];
      [instance_commands commit];
      [instance_commands waitUntilCompleted];
      require_completed(instance_commands, "animated GPU instance descriptor generation");
    } else {
      std::memcpy(instance_buffer.contents, instance_records.descriptors.data(),
                  instance_records.descriptors.size() *
                      sizeof(MTLAccelerationStructureUserIDInstanceDescriptor));
    }
    run.instance_generation_ms += milliseconds(frame_instance_begin, Clock::now());

    const bool rebuild = options.rebuild_period != 0U && frame % options.rebuild_period == 0U;
    const auto tlas_update_begin = Clock::now();
    id<MTLCommandBuffer> update_commands = [queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> update_encoder =
        [update_commands accelerationStructureCommandEncoder];
    for (id<MTLAccelerationStructure> micro_blas in blas) {
      [update_encoder useResource:micro_blas usage:MTLResourceUsageRead];
    }
    if (rebuild) {
      [update_encoder buildAccelerationStructure:tlas
                                      descriptor:tlas_descriptor
                                   scratchBuffer:tlas_scratch
                             scratchBufferOffset:0U];
    } else {
      [update_encoder refitAccelerationStructure:tlas
                                      descriptor:tlas_descriptor
                                     destination:tlas
                                   scratchBuffer:tlas_scratch
                             scratchBufferOffset:0U];
    }
    [update_encoder endEncoding];
    [update_commands commit];
    const auto update_sync_begin = Clock::now();
    [update_commands waitUntilCompleted];
    const auto update_sync_end = Clock::now();
    require_completed(update_commands, rebuild ? "periodic TLAS rebuild" : "TLAS refit");
    const double update_ms = milliseconds(tlas_update_begin, Clock::now());
    run.synchronization_ms += milliseconds(update_sync_begin, update_sync_end);
    if (rebuild) {
      ++run.tlas_rebuild_frames;
      run.tlas_rebuild_ms += update_ms;
    } else {
      ++run.tlas_refit_frames;
      run.tlas_refit_ms += update_ms;
    }

    rays = make_rays(poses.front());
    gpu_rays = pack_rays(rays);
    const auto ray_transfer_begin = Clock::now();
    std::memcpy(ray_buffer.contents, gpu_rays.data(), gpu_rays.size() * sizeof(GpuRay));
    run.transfer_ms += milliseconds(ray_transfer_begin, Clock::now());
    trace_frame();
    validate_frame(frame);
  }
  run.gpu_attribute_reconstruction = true;
  run.status = "measured";
  run.evidence_class = "direct";
  if (options.cpu_validation &&
      (run.misses != 0U || run.wrong_ownership != 0U || run.position_error_max > 2.5e-5 ||
       run.normal_error_max > 2.5e-5 || run.attribute_error_max > 2.5e-5)) {
    run.failure_code = "metal_cpu_oracle_mismatch";
    run.failure_message =
        "GPU hits or reconstructed attributes exceeded the declared 2.5e-5 tolerance";
  } else if (run.final_hit_records.size() != run.rays || run.final_stream_errors != 0U) {
    run.failure_code = "metal_final_stream_invalid";
    run.failure_message = "the merged CPU/GPU final-hit stream did not validate for every ray";
  } else if (options.cpu_validation && (run.minimization_failures != 0U ||
                                        run.minimized_regressions != run.hardware_mismatches)) {
    run.failure_code = "metal_minimization_replay_failed";
    run.failure_message =
        "one or more minimized mismatch rays did not preserve classification on hardware replay";
  }
  run.total_ms = milliseconds(total_begin, Clock::now());
  const int exit_code = run.failure_code.empty() ? 0 : 3;
  return {manifest_json(run, options, command), exit_code};
}

} // namespace

MetalFastPathOutcome run_metal_fast_path(const CompiledAsset &asset,
                                         const MetalFastPathOptions &options,
                                         const std::string &command) {
  @autoreleasepool {
    try {
      @try {
        return run_impl(asset, options, command);
      } @catch (NSException *exception) {
        throw std::runtime_error("Metal exception: " + string_from_ns(exception.reason));
      }
    } catch (const std::exception &error) {
      RunMeasurements run{};
      run.status = "error";
      run.evidence_class = "direct";
      run.asset_hash = asset_checksum(serialize_asset(asset));
      run.failure_code = "metal_fast_path_error";
      run.failure_message = error.what();
      return {manifest_json(run, options, command), 3};
    }
  }
}

} // namespace tetcage
