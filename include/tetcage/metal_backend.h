#pragma once

#include "tetcage/asset_format.h"

#include <cstdint>
#include <string>

namespace tetcage {

struct MetalFastPathOptions {
  std::uint32_t copies{1U};
  std::uint32_t ray_count{128U};
  double motion_amplitude{0.05};
  bool compact_blas{true};
  bool extended_limits{};
  bool boundary_fallback{};
  bool gpu_instances{};
  bool allow_unverified{};
};

struct MetalFastPathOutcome {
  std::string manifest;
  int exit_code{};
};

[[nodiscard]] MetalFastPathOutcome run_metal_fast_path(const CompiledAsset &asset,
                                                       const MetalFastPathOptions &options,
                                                       const std::string &command);

} // namespace tetcage
