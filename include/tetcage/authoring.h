#pragma once

#include "tetcage/asset_format.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tetcage {

struct CageQualityFrame {
  std::uint32_t frame{};
  std::uint64_t near_singular_tetrahedra{};
  std::uint64_t mirrored_tetrahedra{};
  double minimum_edge{};
  double maximum_condition{};
  double maximum_affine_residual{};
};

struct CageQualityReport {
  std::uint64_t asset_hash{};
  std::uint32_t samples{};
  std::uint64_t tetrahedra{};
  std::uint64_t occupied_tetrahedra{};
  std::uint64_t boundary_fragments{};
  double worst_condition{};
  double minimum_edge{};
  double maximum_affine_residual{};
  std::vector<CageQualityFrame> frames;
  std::vector<std::string> fallback_reasons;
  bool suitable{};
};

[[nodiscard]] CageQualityReport
analyze_cage_quality(const CompiledAsset &asset, std::uint32_t samples, double motion_amplitude);

[[nodiscard]] std::string cage_quality_json(const CageQualityReport &report);

} // namespace tetcage
