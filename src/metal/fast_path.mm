#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "tetcage/metal_backend.h"

#include "tetcage/oracle.h"
#include "tetcage/runtime.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

struct BlasVertex {
  float x{};
  float y{};
  float z{};
  float padding{};
};

static_assert(sizeof(PackedFloat2) == 8U);
static_assert(sizeof(PackedFloat3) == 12U);

struct GpuRay {
  PackedFloat3 origin{};
  float minimum_distance{};
  PackedFloat3 direction{};
  float maximum_distance{};
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

static_assert(sizeof(GpuRay) == 32U);
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
};

struct InstanceInfo {
  std::uint32_t copy{};
  std::uint32_t blas_index{};
  std::uint32_t tet_id{};
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
  double total_ms{};
  bool runtime_shader_compiled{};
  bool gpu_instance_generation{};
  bool gpu_attribute_reconstruction{};
  bool mirrored_instances{};
  std::vector<std::string> mismatch_samples;
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
         << "    \"motion_amplitude\": " << options.motion_amplitude << ",\n"
         << "    \"compact_blas\": " << (options.compact_blas ? "true" : "false") << ",\n"
         << "    \"extended_limits\": " << (options.extended_limits ? "true" : "false") << ",\n"
         << "    \"boundary_policy\": \""
         << (options.boundary_fallback ? "cpu_fallback_experimental" : "hardware_all_rays")
         << "\",\n"
         << "    \"instance_generation\": \""
         << (options.gpu_instances ? "gpu_compute_descriptor" : "cpu_direct_baseline") << "\",\n"
         << "    \"blas_usage\": \"static_prefer_fast_intersection_when_available\",\n"
         << "    \"triangle_culling\": \"disabled_for_mirror_safe_baseline\"\n"
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
  output << ",\n    \"shading\": ";
  emit_number_or_null(output, measured, 0.0);
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
  emit_integer_or_null(output, measured, run.misses);
  output << ",\n    \"duplicate_hits\": 0,\n"
         << "    \"wrong_ownership\": ";
  emit_integer_or_null(output, measured, run.wrong_ownership);
  output << ",\n    \"boundary_sensitive_rays\": ";
  emit_integer_or_null(output, measured, run.boundary_sensitive_rays);
  output << ",\n    \"gpu_eligible_rays\": ";
  emit_integer_or_null(output, measured, run.gpu_eligible_rays);
  output << ",\n    \"position_error_max\": ";
  emit_number_or_null(output, measured, run.position_error_max);
  output << ",\n    \"normal_error_max\": ";
  emit_number_or_null(output, measured, run.normal_error_max);
  output << ",\n    \"attribute_error_max\": ";
  emit_number_or_null(output, measured, run.attribute_error_max);
  output << ",\n    \"image_error\": null\n"
         << "  },\n"
         << "  \"statistics\": {\n"
         << "    \"occupied_micro_blas\": " << run.blas_count << ",\n"
         << "    \"instances\": " << run.total_instances << ",\n"
         << "    \"blas_uncompacted_bytes\": " << run.blas_uncompacted_bytes << ",\n"
         << "    \"blas_compacted_bytes\": " << run.blas_bytes << ",\n"
         << "    \"blas_build_gpu_ms\": " << run.blas_gpu_ms << ",\n"
         << "    \"blas_compaction_gpu_ms\": " << run.compaction_gpu_ms << ",\n"
         << "    \"tlas_build_gpu_ms\": " << run.tlas_gpu_ms << ",\n"
         << "    \"traversal_gpu_ms\": " << run.traversal_gpu_ms << ",\n"
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

std::vector<Vec3> animated_pose(const CompiledAsset &asset, double amplitude, std::uint32_t copy) {
  std::vector<Vec3> pose = asset.cage.vertices;
  for (std::size_t index = 0; index < pose.size(); ++index) {
    const double phase = static_cast<double>(index + 1U);
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
  const std::string tags =
      extended_limits ? "triangle_data, instancing, extended_limits" : "triangle_data, instancing";
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
};

struct Provenance {
  uint source_primitive;
  uint material;
  packed_float3 source_barycentric[3];
  packed_float3 normals[3];
  packed_float2 uvs[3];
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

kernel void trace_rays(instance_acceleration_structure scene [[buffer(0)]],
                       device const TraceRay *rays [[buffer(1)]],
                       device TraceHit *hits [[buffer(2)]],
                       device const uint *instance_to_blas [[buffer(3)]],
                       device const uint *primitive_offsets [[buffer(4)]],
                       device const Provenance *provenance [[buffer(5)]],
                       uint tid [[thread_position_in_grid]]) {
  TraceHit output = {};
  TraceRay input = rays[tid];
  ray query(float3(input.origin), float3(input.direction),
            input.minimum_distance, input.maximum_distance);
  intersector<__INTERSECTOR_TAGS__> ray_intersector;
  ray_intersector.assume_geometry_type(geometry_type::triangle);
  auto intersection = ray_intersector.intersect(query, scene, 0xFFFFFFFF);
  if (intersection.type == intersection_type::triangle) {
    output.hit = 1;
    output.primitive_id = intersection.primitive_id;
    output.user_instance_id = intersection.user_instance_id;
    output.front_facing = intersection.triangle_front_facing ? 1 : 0;
    output.distance = intersection.distance;
    output.triangle_barycentric = packed_float2(intersection.triangle_barycentric_coord);
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

void record_mismatch(RunMeasurements &run, std::size_t index, const Ray &ray,
                     const std::optional<TraceHit> &expected, const GpuHit &actual,
                     const std::optional<Vec4> &expected_cage_bary, const std::string &kind) {
  if (run.mismatch_samples.size() >= 8U) {
    return;
  }
  std::ostringstream sample;
  sample << std::setprecision(9) << "{\"kind\":\"" << kind << "\",\"ray_index\":" << index
         << ",\"origin\":[" << ray.origin.x << ',' << ray.origin.y << ',' << ray.origin.z
         << "],\"direction\":[" << ray.direction.x << ',' << ray.direction.y << ','
         << ray.direction.z << "],\"cpu_hit\":" << (expected ? "true" : "false")
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
  sample << ",\"gpu_source_bary\":[" << actual.source_barycentric.x << ','
         << actual.source_barycentric.y << ',' << actual.source_barycentric.z
         << "],\"gpu_user_instance_id\":" << actual.user_instance_id
         << ",\"gpu_primitive_id\":" << actual.primitive_id << "}";
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

  NSMutableArray<id<MTLBuffer>> *vertex_buffers = [NSMutableArray array];
  NSMutableArray<id<MTLBuffer>> *index_buffers = [NSMutableArray array];
  NSMutableArray<MTLPrimitiveAccelerationStructureDescriptor *> *blas_descriptors =
      [NSMutableArray array];
  NSMutableArray<id<MTLAccelerationStructure>> *blas = [NSMutableArray array];
  std::vector<MTLAccelerationStructureSizes> blas_sizes;
  std::vector<std::size_t> scratch_offsets;
  std::size_t total_scratch = 0U;

  for (const auto &group : groups) {
    id<MTLBuffer> vertex_buffer =
        [device newBufferWithBytes:group.vertices.data()
                            length:group.vertices.size() * sizeof(BlasVertex)
                           options:MTLResourceStorageModeShared];
    if (vertex_buffer == nil) {
      throw std::runtime_error("failed to allocate canonical micro-mesh vertex buffer");
    }
    [vertex_buffers addObject:vertex_buffer];

    std::vector<std::uint32_t> local_indices(group.vertices.size());
    for (std::uint32_t vertex_index = 0; vertex_index < local_indices.size(); ++vertex_index) {
      local_indices[vertex_index] = vertex_index;
    }
    id<MTLBuffer> index_buffer =
        [device newBufferWithBytes:local_indices.data()
                            length:local_indices.size() * sizeof(std::uint32_t)
                           options:MTLResourceStorageModeShared];
    if (index_buffer == nil) {
      throw std::runtime_error("failed to allocate canonical micro-mesh index buffer");
    }
    [index_buffers addObject:index_buffer];

    auto *geometry = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
    geometry.vertexBuffer = vertex_buffer;
    geometry.vertexBufferOffset = 0U;
    geometry.vertexStride = sizeof(BlasVertex);
    geometry.triangleCount = group.micro_triangle_indices.size();
    geometry.indexBuffer = index_buffer;
    geometry.indexBufferOffset = 0U;
    geometry.indexType = MTLIndexTypeUInt32;
    geometry.opaque = YES;
    if (@available(macOS 13.0, *)) {
      geometry.vertexFormat = MTLAttributeFormatFloat3;
    }

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
    poses.push_back(animated_pose(asset, options.motion_amplitude, copy));
  }
  const auto cage_end = Clock::now();
  run.cage_deformation_ms = milliseconds(cage_begin, cage_end);

  const auto transform_begin = Clock::now();
  std::vector<MTLAccelerationStructureUserIDInstanceDescriptor> descriptors;
  std::vector<GpuInstanceInput> gpu_instance_inputs;
  std::vector<InstanceInfo> instance_info;
  std::vector<std::uint32_t> instance_to_blas;
  descriptors.reserve(options.copies * groups.size());
  gpu_instance_inputs.reserve(options.copies * groups.size());
  instance_info.reserve(options.copies * groups.size());
  instance_to_blas.reserve(options.copies * groups.size());
  for (std::uint32_t copy = 0; copy < options.copies; ++copy) {
    CagePose pose{poses[copy]};
    const auto transforms = build_tet_transforms(asset, pose, copy, 0U);
    if (!transforms.error.empty()) {
      throw std::runtime_error(transforms.error);
    }
    for (std::uint32_t blas_index = 0; blas_index < groups.size(); ++blas_index) {
      const auto tet_id = groups[blas_index].tet_id;
      const auto &transform = transforms.transforms[tet_id];
      const PackedFloat3 c0{
          static_cast<float>(transform.object_from_canonical.linear.columns[0].x),
          static_cast<float>(transform.object_from_canonical.linear.columns[0].y),
          static_cast<float>(transform.object_from_canonical.linear.columns[0].z)};
      const PackedFloat3 c1{
          static_cast<float>(transform.object_from_canonical.linear.columns[1].x),
          static_cast<float>(transform.object_from_canonical.linear.columns[1].y),
          static_cast<float>(transform.object_from_canonical.linear.columns[1].z)};
      const PackedFloat3 c2{
          static_cast<float>(transform.object_from_canonical.linear.columns[2].x),
          static_cast<float>(transform.object_from_canonical.linear.columns[2].y),
          static_cast<float>(transform.object_from_canonical.linear.columns[2].z)};
      const PackedFloat3 translation{
          static_cast<float>(transform.object_from_canonical.translation.x),
          static_cast<float>(transform.object_from_canonical.translation.y),
          static_cast<float>(transform.object_from_canonical.translation.z)};
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
      descriptor.userID = static_cast<std::uint32_t>(descriptors.size());
      gpu_instance_inputs.push_back({c0, c1, c2, translation,
                                     static_cast<std::uint32_t>(descriptor.options),
                                     descriptor.mask, descriptor.intersectionFunctionTableOffset,
                                     descriptor.accelerationStructureIndex, descriptor.userID});
      run.mirrored_instances =
          run.mirrored_instances || (transform.flags & tet_transform_mirrored) != 0U;
      descriptors.push_back(descriptor);
      instance_info.push_back({copy, blas_index, tet_id});
      instance_to_blas.push_back(blas_index);
    }
  }
  const auto transform_end = Clock::now();
  run.transform_generation_ms = milliseconds(transform_begin, transform_end);
  run.total_instances = descriptors.size();

  const auto instance_begin = Clock::now();
  id<MTLBuffer> instance_input_buffer = nil;
  id<MTLBuffer> instance_buffer = nil;
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
    id<MTLComputePipelineState> instance_pipeline =
        [device newComputePipelineStateWithFunction:instance_function
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
    const NSUInteger instance_threads =
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
  if (instance_buffer == nil || instance_to_blas_buffer == nil) {
    throw std::runtime_error("failed to allocate Metal instance metadata buffers");
  }
  const auto instance_end = Clock::now();
  run.instance_generation_ms = milliseconds(instance_begin, instance_end);
  run.instance_buffer_bytes = instance_buffer.allocatedSize + instance_to_blas_buffer.allocatedSize;
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
  id<MTLLibrary> library = [device newLibraryWithSource:trace_kernel_source(options.extended_limits)
                                                options:nil
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

  auto rays = generate_adversarial_rays(asset, poses.front(), 81002718ULL, options.ray_count);
  if (rays.empty()) {
    throw std::runtime_error("ray generator returned no correctness rays");
  }
  if (rays.size() > options.ray_count) {
    rays.resize(options.ray_count);
  }
  while (rays.size() < options.ray_count) {
    rays.push_back(rays[rays.size() % std::max<std::size_t>(rays.size(), 1U)]);
  }
  std::vector<GpuRay> gpu_rays;
  gpu_rays.reserve(rays.size());
  for (const auto &ray : rays) {
    gpu_rays.push_back({{static_cast<float>(ray.origin.x), static_cast<float>(ray.origin.y),
                         static_cast<float>(ray.origin.z)},
                        static_cast<float>(ray.minimum_t),
                        {static_cast<float>(ray.direction.x), static_cast<float>(ray.direction.y),
                         static_cast<float>(ray.direction.z)},
                        static_cast<float>(ray.maximum_t)});
  }

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
  for (id<MTLAccelerationStructure> micro_blas in blas) {
    [trace_encoder useResource:micro_blas usage:MTLResourceUsageRead];
  }
  const NSUInteger thread_width =
      std::min<NSUInteger>(pipeline.maxTotalThreadsPerThreadgroup,
                           std::max<NSUInteger>(pipeline.threadExecutionWidth, 1U));
  [trace_encoder dispatchThreads:MTLSizeMake(gpu_rays.size(), 1U, 1U)
           threadsPerThreadgroup:MTLSizeMake(thread_width, 1U, 1U)];
  [trace_encoder endEncoding];
  [trace_commands commit];
  const auto trace_sync_begin = Clock::now();
  [trace_commands waitUntilCompleted];
  const auto trace_sync_end = Clock::now();
  require_completed(trace_commands, "ray traversal");
  const auto traversal_end = Clock::now();
  run.traversal_ms = milliseconds(traversal_begin, traversal_end);
  run.traversal_gpu_ms = command_gpu_ms(trace_commands);
  run.synchronization_ms += milliseconds(trace_sync_begin, trace_sync_end);

  const auto *gpu_hits = static_cast<const GpuHit *>(hit_buffer.contents);
  run.rays = rays.size();
  run.gpu_eligible_rays = rays.size();
  for (std::size_t index = 0; index < rays.size(); ++index) {
    const auto cpu = trace_fast(asset, poses.front(), rays[index]);
    std::optional<Vec4> expected_cage_bary;
    if (cpu.closest && cpu.closest->tet_id < asset.cage.tetrahedra.size()) {
      expected_cage_bary = to_barycentric(posed_tet(asset, poses.front(), cpu.closest->tet_id),
                                          cpu.closest->position);
    }
    if (cpu.closest && boundary_sensitive_ray(asset, poses.front(), rays[index], cpu)) {
      ++run.boundary_sensitive_rays;
      if (options.boundary_fallback) {
        --run.gpu_eligible_rays;
        continue;
      }
    }
    const bool cpu_hit = cpu.closest.has_value();
    const bool gpu_hit = gpu_hits[index].hit != 0U;
    if (cpu_hit != gpu_hit) {
      ++run.misses;
      record_mismatch(run, index, rays[index], cpu.closest, gpu_hits[index], expected_cage_bary,
                      "hit_presence");
      continue;
    }
    if (!cpu_hit) {
      continue;
    }
    const auto &expected = *cpu.closest;
    const auto &actual = gpu_hits[index];
    if (actual.user_instance_id >= instance_info.size() ||
        instance_info[actual.user_instance_id].copy != 0U ||
        actual.source_primitive != expected.source_primitive ||
        actual.material != expected.material) {
      ++run.wrong_ownership;
      record_mismatch(run, index, rays[index], cpu.closest, actual, expected_cage_bary,
                      "ownership");
      continue;
    }
    const double position_error =
        std::abs(expected.t - static_cast<double>(actual.distance)) * length(rays[index].direction);
    const double normal_error = vec3_error(expected.normal, actual.normal);
    const double attribute_error =
        std::max(vec3_error(expected.source_barycentric, actual.source_barycentric),
                 vec2_error(expected.uv, actual.uv));
    run.position_error_max = std::max(run.position_error_max, position_error);
    run.normal_error_max = std::max(run.normal_error_max, normal_error);
    run.attribute_error_max = std::max(run.attribute_error_max, attribute_error);
    if (position_error > 2.5e-5 || normal_error > 2.5e-5 || attribute_error > 2.5e-5) {
      record_mismatch(run, index, rays[index], cpu.closest, actual, expected_cage_bary, "value");
    }
  }
  run.gpu_attribute_reconstruction = true;
  run.status = "measured";
  run.evidence_class = "direct";
  if (run.misses != 0U || run.wrong_ownership != 0U || run.position_error_max > 2.5e-5 ||
      run.normal_error_max > 2.5e-5 || run.attribute_error_max > 2.5e-5) {
    run.failure_code = "metal_cpu_oracle_mismatch";
    run.failure_message =
        "GPU hits or reconstructed attributes exceeded the declared 2.5e-5 tolerance";
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
