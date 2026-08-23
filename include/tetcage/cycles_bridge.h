#pragma once

#include "tetcage/asset_format.h"

#include <cstdint>
#include <vector>

namespace tetcage {

enum class CyclesGeometryMode : std::uint8_t {
  conventional_mesh,
  procedural_metalrt,
};

enum class CyclesFallbackReason : std::uint8_t {
  none,
  invalid_asset,
  invalid_pose,
  unsupported_device,
  capacity_rejected,
  update_failed,
};

[[nodiscard]] const char *cycles_geometry_mode_name(CyclesGeometryMode mode);
[[nodiscard]] const char *cycles_fallback_reason_name(CyclesFallbackReason reason);

struct CyclesTetPrimitive {
  std::uint32_t tet_id{};
  std::uint32_t micro_triangle_begin{};
  std::uint32_t micro_triangle_count{};
  Vec3 bounds_min{};
  Vec3 bounds_max{};
};

struct CyclesTetCageFrame {
  std::uint64_t asset_checksum{};
  std::uint64_t pose_generation{};
  CyclesGeometryMode mode{CyclesGeometryMode::conventional_mesh};
  CyclesFallbackReason fallback_reason{CyclesFallbackReason::none};
  std::vector<Affine3> tet_transforms;
  std::vector<CyclesTetPrimitive> primitives;
  std::vector<std::uint32_t> micro_triangle_indices;
};

/* Values returned by a MetalRT custom intersection function before Cycles hit normalization. */
struct CyclesMetalHit {
  std::uint32_t object_id{};
  std::uint32_t tet_id{};
  std::uint32_t primitive_id{};
  double distance{};
  double u{};
  double v{};
};

/* A normalized hit suitable for Cycles' ordinary source-triangle shading path. */
struct CyclesNormalizedHit {
  std::uint32_t object_id{};
  std::uint32_t source_primitive{};
  std::uint32_t owner_tet{};
  Vec3 source_barycentric{};
  double distance{};
  double u{};
  double v{};
};

[[nodiscard]] CyclesTetCageFrame build_cycles_tet_cage_frame(const CompiledAsset &asset,
                                                              const Cage &posed_cage,
                                                              std::uint64_t pose_generation);

[[nodiscard]] std::optional<CyclesNormalizedHit>
normalize_cycles_metal_hit(const CompiledAsset &asset, const CyclesMetalHit &hit);

} // namespace tetcage
