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

struct CageAnimationFrame {
  std::uint32_t frame{};
  std::uint64_t surface_samples{};
  std::uint64_t uncovered_samples{};
  double maximum_position_error{};
  double rms_position_error{};
  double maximum_normal_error{};
  double optimized_maximum_position_error{};
  double optimized_rms_position_error{};
};

struct CageAnimationReport {
  std::uint64_t asset_hash{};
  std::uint32_t samples{};
  std::uint64_t surface_samples{};
  std::uint64_t uncovered_samples{};
  std::uint64_t optimized_weight_samples{};
  double position_threshold{1.0e-3};
  double normal_threshold{1.0e-2};
  double maximum_position_error{};
  double rms_position_error{};
  double maximum_normal_error{};
  double optimized_maximum_position_error{};
  double optimized_rms_position_error{};
  double maximum_weight_delta{};
  std::vector<CageAnimationFrame> frames;
  std::vector<std::string> fallback_reasons;
  bool suitable{};
};

struct CageRefinementResult {
  std::optional<Cage> cage;
  std::string error;
};

[[nodiscard]] CageRefinementResult refine_cage(const Cage &cage, std::uint32_t levels);

[[nodiscard]] CageQualityReport
analyze_cage_quality(const CompiledAsset &asset, std::uint32_t samples, double motion_amplitude);

[[nodiscard]] std::string cage_quality_json(const CageQualityReport &report);

[[nodiscard]] CageAnimationReport
analyze_cage_animation(const CompiledAsset &asset, std::uint32_t samples, double motion_amplitude,
                       double position_threshold = 1.0e-3, double normal_threshold = 1.0e-2);

[[nodiscard]] std::string cage_animation_json(const CageAnimationReport &report);

} // namespace tetcage
