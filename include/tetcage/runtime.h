#pragma once

#include "tetcage/asset_format.h"
#include "tetcage/oracle.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace tetcage {

struct CagePose {
  std::vector<Vec3> positions;
};

enum class BuildStrategy : std::uint8_t {
  rebuild,
  update,
  periodic_rebuild,
};

struct BuildPolicy {
  BuildStrategy strategy{BuildStrategy::rebuild};
  std::uint32_t rebuild_period{1U};
  bool allow_one_frame_latency{};
  std::optional<std::uint64_t> max_instances;
};

enum class RepresentationChoice : std::uint8_t {
  rigid_instancing,
  conventional_dynamic,
  tet_cage,
  hybrid,
};

struct MethodSelectionInput {
  std::uint64_t source_triangles{};
  std::uint64_t occupied_tetrahedra{};
  std::uint64_t copies{};
  double maximum_condition{};
  double maximum_position_error{};
  double maximum_normal_error{};
  double boundary_fallback_fraction{};
  bool animated{};
  bool hardware_tet_backend{};
  bool gpu_correctness_proven{};
};

struct MethodSelectionResult {
  RepresentationChoice choice{RepresentationChoice::conventional_dynamic};
  std::vector<std::string> reasons;
};

[[nodiscard]] MethodSelectionResult select_representation(const MethodSelectionInput &input);
[[nodiscard]] const char *representation_choice_name(RepresentationChoice choice);
[[nodiscard]] std::string method_selection_json(const MethodSelectionInput &input,
                                                const MethodSelectionResult &result);

enum TetTransformFlags : std::uint32_t {
  tet_transform_none = 0U,
  tet_transform_mirrored = 1U << 0U,
  tet_transform_poorly_conditioned = 1U << 1U,
};

struct TetTransform {
  Affine3 object_from_canonical{};
  Mat3 normal_from_canonical{};
  std::uint32_t object_id{};
  std::uint32_t mesh_id{};
  std::uint32_t tet_id{};
  std::uint32_t flags{};
};

struct TransformBuildResult {
  std::vector<TetTransform> transforms;
  std::string error;
};

[[nodiscard]] TransformBuildResult build_tet_transforms(const CompiledAsset &asset,
                                                        const CagePose &pose,
                                                        std::uint32_t object_id,
                                                        std::uint32_t mesh_id);

struct DenseDeformationResult {
  std::vector<Vec3> positions;
  std::vector<Vec3> normals;
  std::string error;
};

[[nodiscard]] DenseDeformationResult deform_dense_source(const CompiledAsset &asset,
                                                         const CagePose &pose);

struct OpaqueBlasHandle {
  std::uint64_t token{};
  std::uint64_t bytes{};
};

struct CachedAsset {
  std::shared_ptr<const CompiledAsset> asset;
  std::string backend;
  std::vector<OpaqueBlasHandle> immutable_micro_blas;
};

class AssetCache {
public:
  [[nodiscard]] std::uint64_t insert(std::shared_ptr<const CompiledAsset> asset,
                                     std::string backend,
                                     std::vector<OpaqueBlasHandle> immutable_micro_blas);
  [[nodiscard]] const CachedAsset *find(std::uint64_t key) const;
  [[nodiscard]] bool erase(std::uint64_t key);
  [[nodiscard]] std::size_t size() const;

private:
  std::map<std::uint64_t, CachedAsset> entries_;
};

struct VisibleObject {
  std::uint32_t object_id{};
  std::uint32_t mesh_id{};
  std::uint32_t pose_index{};
  bool visible{true};
};

struct FrameBuildInput {
  std::vector<VisibleObject> objects;
  std::vector<CagePose> poses;
  BuildPolicy policy{};
  struct Control {
    bool cancellation_requested{};
    bool device_lost{};
    bool reset_requested{};
    std::optional<std::uint64_t> max_allocation_bytes;
  } control{};
};

struct BackendCapabilities {
  bool hardware_acceleration{};
  bool supports_rebuild{true};
  bool supports_update{};
  bool supports_periodic_rebuild{};
  std::optional<std::uint64_t> max_instances;
};

enum class FrameBuildStatus : std::uint8_t {
  built,
  invalid_input,
  unsupported,
  resource_limit,
  cancelled,
  device_lost,
  reset_required,
};

enum class FrameFallback : std::uint8_t {
  none,
  conventional_dynamic,
  retain_last_valid_frame,
};

[[nodiscard]] const char *frame_build_status_name(FrameBuildStatus status);
[[nodiscard]] const char *frame_fallback_name(FrameFallback fallback);

struct BackendFrameResult {
  FrameBuildStatus status{FrameBuildStatus::built};
  FrameFallback fallback{FrameFallback::none};
  std::uint64_t visible_instances{};
  std::uint64_t transforms{};
  std::uint64_t allocation_bytes{};
  std::string error;
};

class RuntimeBackend {
public:
  virtual ~RuntimeBackend() = default;
  [[nodiscard]] virtual std::string api_name() const = 0;
  [[nodiscard]] virtual BackendCapabilities capabilities() const = 0;
  [[nodiscard]] virtual BackendFrameResult build_frame(const FrameBuildInput &input) = 0;
  [[nodiscard]] virtual TraceResult trace(const Ray &ray) const = 0;
};

class CpuStubBackend final : public RuntimeBackend {
public:
  explicit CpuStubBackend(std::shared_ptr<const CompiledAsset> asset);
  [[nodiscard]] std::string api_name() const override;
  [[nodiscard]] BackendCapabilities capabilities() const override;
  [[nodiscard]] BackendFrameResult build_frame(const FrameBuildInput &input) override;
  [[nodiscard]] TraceResult trace(const Ray &ray) const override;

private:
  std::shared_ptr<const CompiledAsset> asset_;
  std::vector<Vec3> active_pose_;
  bool has_valid_frame_{};
};

struct BenchmarkScene {
  std::string id;
  std::uint64_t seed{};
  std::uint32_t copies{1U};
  std::uint32_t ray_count{128U};
  double motion_amplitude{};
  double visible_fraction{1.0};
};

struct BenchmarkOptions {
  std::uint32_t warmup_iterations{2U};
  std::uint32_t measured_iterations{10U};
  bool inject_stub_corruption{};
};

struct BenchmarkSummary {
  double median_ms{};
  double p95_ms{};
  double variance_ms2{};
};

struct BenchmarkMemory {
  std::optional<std::uint64_t> source_geometry_bytes;
  std::optional<std::uint64_t> canonical_geometry_bytes;
  std::optional<std::uint64_t> provenance_bytes;
  std::optional<std::uint64_t> cage_bytes;
  std::optional<std::uint64_t> blas_bytes;
  std::optional<std::uint64_t> tlas_bytes;
  std::optional<std::uint64_t> scratch_bytes;
  std::optional<std::uint64_t> instance_buffer_bytes;
  std::optional<std::uint64_t> api_object_bytes;
  std::optional<std::uint64_t> renderer_state_bytes;
  std::optional<std::uint64_t> peak_total_bytes;
};

struct BenchmarkStageMedians {
  std::optional<double> cage_deformation_ms;
  std::optional<double> transform_generation_ms;
  std::optional<double> instance_generation_ms;
  std::optional<double> blas_ms;
  std::optional<double> tlas_ms;
  std::optional<double> synchronization_ms;
  std::optional<double> traversal_ms;
  std::optional<double> shading_ms;
  std::optional<double> total_ms;
};

struct BenchmarkResult {
  BenchmarkScene scene{};
  BenchmarkOptions options{};
  std::string asset_hash;
  std::vector<double> total_samples_ms;
  std::vector<double> cage_samples_ms;
  std::vector<double> transform_samples_ms;
  std::vector<double> instance_samples_ms;
  std::vector<double> traversal_samples_ms;
  std::vector<double> dense_deformation_samples_ms;
  std::vector<double> dense_traversal_samples_ms;
  std::vector<double> dense_total_samples_ms;
  BenchmarkStageMedians stages{};
  BenchmarkSummary summary{};
  BenchmarkSummary dense_summary{};
  std::optional<double> dense_deformation_median_ms;
  std::optional<double> dense_traversal_median_ms;
  BenchmarkMemory memory{};
  std::uint64_t rays_traced{};
  std::uint64_t correctness_errors{};
  std::uint64_t misses{};
  std::uint64_t dense_baseline_misses{};
  std::uint64_t dense_baseline_provenance_mismatches{};
  std::uint64_t duplicate_hits{};
  std::uint64_t wrong_ownership{};
  double max_position_error{};
  double max_attribute_error{};
  bool corruption_detected{};
  std::vector<std::string> failures;
  std::string command{"library:run_cpu_stub_benchmark"};
  std::string source_commit{"0000000000000000000000000000000000000000"};
  bool source_dirty{true};
  std::string host_os_version{"unavailable"};
};

[[nodiscard]] BenchmarkResult run_cpu_stub_benchmark(const CompiledAsset &asset,
                                                     const BenchmarkScene &scene,
                                                     const BenchmarkOptions &options);
[[nodiscard]] std::string benchmark_manifest_json(const BenchmarkResult &result);
[[nodiscard]] std::string benchmark_samples_csv(const BenchmarkResult &result);

} // namespace tetcage
