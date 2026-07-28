#pragma once

#include "tetcage/math.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tetcage {

inline constexpr std::uint32_t asset_format_version = 1;

struct SourceVertex {
  Vec3 position{};
  Vec3 normal{};
  Vec2 uv{};
};

struct SourceTriangle {
  std::array<std::uint32_t, 3> vertex_indices{};
  std::uint32_t primitive_id{};
  std::uint32_t material_id{};
};

struct SourceMesh {
  std::vector<SourceVertex> vertices;
  std::vector<SourceTriangle> triangles;
};

struct CageTet {
  std::array<std::uint32_t, 4> vertex_indices{};
};

struct Cage {
  std::vector<Vec3> vertices;
  std::vector<std::uint64_t> vertex_ids;
  std::vector<CageTet> tetrahedra;
};

struct CompiledTetMetadata {
  double determinant{};
  double condition_estimate{};
  double minimum_edge{};
  bool mirrored{};
  bool near_singular{};
  std::array<std::int32_t, 4> adjacent_tet{{-1, -1, -1, -1}};
  std::array<std::uint32_t, 4> face_owner{};
};

struct TolerancePolicy {
  std::uint32_t version{1};
  double expanded_barycentric_epsilon{0.0};
  double feature_snap_epsilon{1.0e-12};
  double minimum_area_relative{1.0e-14};
};

struct GeneratedVertex {
  Vec4 cage_barycentric{};
  Vec3 source_barycentric{};
  FeatureIdentity feature{};
  std::uint64_t stable_id{};
  std::uint32_t tet_id{};
  std::uint32_t source_primitive{};
};

struct MicroTriangle {
  std::array<std::uint32_t, 3> vertex_indices{};
  std::uint32_t tet_id{};
  std::uint32_t owner_tet{};
  std::uint32_t source_primitive{};
  std::uint32_t material{};
  std::uint32_t deterministic_subtriangle{};
};

struct AssetStatistics {
  std::uint64_t source_triangles{};
  std::uint64_t generated_triangles{};
  std::uint64_t generated_vertices{};
  std::uint64_t occupied_tetrahedra{};
  std::uint64_t boundary_fragments{};
  std::uint64_t canonical_bytes{};
  std::uint64_t provenance_bytes{};
  double triangle_expansion{};
  double vertex_expansion{};
  double worst_condition{};
};

struct CompiledAsset {
  std::uint32_t format_version{asset_format_version};
  TolerancePolicy tolerance{};
  SourceMesh source{};
  Cage cage{};
  std::vector<CompiledTetMetadata> tet_metadata;
  std::vector<GeneratedVertex> generated_vertices;
  std::vector<MicroTriangle> micro_triangles;
  AssetStatistics statistics{};
};

struct CompileDiagnostic {
  std::string code;
  std::string message;
  std::uint32_t source_primitive{};
  std::uint32_t tet_id{};
};

struct CompileResult {
  std::optional<CompiledAsset> asset;
  std::vector<CompileDiagnostic> diagnostics;
};

struct DeserializeResult {
  std::optional<CompiledAsset> asset;
  std::string error;
};

[[nodiscard]] CompileResult compile_asset(const SourceMesh &mesh, const Cage &cage,
                                          const TolerancePolicy &tolerance);
[[nodiscard]] std::vector<std::byte> serialize_asset(const CompiledAsset &asset);
[[nodiscard]] DeserializeResult deserialize_asset(const std::vector<std::byte> &bytes);
[[nodiscard]] std::uint64_t asset_checksum(const std::vector<std::byte> &bytes);
[[nodiscard]] std::string inspect_asset_json(const CompiledAsset &asset);

} // namespace tetcage
