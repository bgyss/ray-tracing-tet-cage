#pragma once

#include "tetcage/oracle.h"

#include <cstdint>
#include <functional>

namespace tetcage {

enum class MetalMismatchClass : std::uint8_t {
  cpu_oracle_defect,
  floating_point_tolerance_policy_error,
  generated_triangle_overlap_or_gap,
  shared_edge_ownership_disagreement,
  metal_intersection_acceptance_rule,
  stale_acceleration_structure_or_synchronization_defect,
  provenance_attribute_reconstruction_defect,
};

enum class MetalFinalPath : std::uint8_t {
  hardware,
  cpu_fallback,
};

struct MetalMismatchSignals {
  bool cpu_disagrees_with_exact_oracle{};
  bool distance_within_policy{};
  bool generated_boundary{};
  bool shared_edge{};
  bool hit_presence_differs{};
  bool ownership_differs{};
  bool primitive_matches{};
  bool attributes_differ{};
  bool stale_or_unsynchronized{};
};

[[nodiscard]] MetalMismatchClass classify_metal_mismatch(const MetalMismatchSignals &signals);
[[nodiscard]] const char *metal_mismatch_class_name(MetalMismatchClass value);
[[nodiscard]] MetalFinalPath choose_metal_final_path(bool fallback_enabled, bool boundary_sensitive,
                                                     bool hardware_mismatch);
[[nodiscard]] const char *metal_final_path_name(MetalFinalPath value);

[[nodiscard]] Ray
minimize_metal_mismatch_ray(const Ray &ray,
                            const std::function<bool(const Ray &)> &preserves_mismatch);

} // namespace tetcage
