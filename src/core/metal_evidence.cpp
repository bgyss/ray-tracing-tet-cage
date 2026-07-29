#include "tetcage/metal_evidence.h"

#include <array>
#include <cmath>
#include <limits>

namespace tetcage {
namespace {

template <typename Setter>
void simplify_scalar(double original, Setter set_candidate,
                     const std::function<bool(const Ray &)> &preserves_mismatch, Ray &current) {
  const auto try_value = [&](double value) {
    Ray candidate = current;
    set_candidate(candidate, value);
    if (std::isfinite(value) && preserves_mismatch(candidate)) {
      current = candidate;
      return true;
    }
    return false;
  };

  if (try_value(0.0)) {
    return;
  }

  const double sign = std::signbit(original) ? -1.0 : 1.0;
  if (std::abs(original) >= 1.0) {
    constexpr std::array<double, 10> integers{1.0,  2.0,  4.0,   8.0,   16.0,
                                              32.0, 64.0, 128.0, 256.0, 512.0};
    for (const double integer : integers) {
      if (try_value(sign * integer)) {
        return;
      }
    }
  }

  double scale = 10.0;
  for (unsigned digits = 1U; digits <= 12U; ++digits) {
    const double rounded = std::round(original * scale) / scale;
    if (try_value(rounded)) {
      return;
    }
    scale *= 10.0;
  }
}

} // namespace

MetalMismatchClass classify_metal_mismatch(const MetalMismatchSignals &signals) {
  if (signals.cpu_disagrees_with_exact_oracle) {
    return MetalMismatchClass::cpu_oracle_defect;
  }
  if (signals.stale_or_unsynchronized) {
    return MetalMismatchClass::stale_acceleration_structure_or_synchronization_defect;
  }
  if (signals.shared_edge && signals.ownership_differs) {
    return MetalMismatchClass::shared_edge_ownership_disagreement;
  }
  if (signals.hit_presence_differs && signals.generated_boundary) {
    return MetalMismatchClass::metal_intersection_acceptance_rule;
  }
  if (signals.primitive_matches && signals.distance_within_policy && signals.attributes_differ) {
    return MetalMismatchClass::provenance_attribute_reconstruction_defect;
  }
  if (signals.generated_boundary) {
    return MetalMismatchClass::generated_triangle_overlap_or_gap;
  }
  return MetalMismatchClass::floating_point_tolerance_policy_error;
}

const char *metal_mismatch_class_name(MetalMismatchClass value) {
  switch (value) {
  case MetalMismatchClass::cpu_oracle_defect:
    return "cpu_oracle_defect";
  case MetalMismatchClass::floating_point_tolerance_policy_error:
    return "floating_point_tolerance_policy_error";
  case MetalMismatchClass::generated_triangle_overlap_or_gap:
    return "generated_triangle_overlap_or_gap";
  case MetalMismatchClass::shared_edge_ownership_disagreement:
    return "shared_edge_ownership_disagreement";
  case MetalMismatchClass::metal_intersection_acceptance_rule:
    return "metal_intersection_acceptance_rule";
  case MetalMismatchClass::stale_acceleration_structure_or_synchronization_defect:
    return "stale_blas_tlas_or_synchronization_defect";
  case MetalMismatchClass::provenance_attribute_reconstruction_defect:
    return "provenance_attribute_reconstruction_defect";
  }
  return "floating_point_tolerance_policy_error";
}

MetalFinalPath choose_metal_final_path(bool fallback_enabled, bool boundary_sensitive,
                                       bool hardware_mismatch, bool hardware_resolves_boundaries) {
  const bool unresolved_boundary = boundary_sensitive && !hardware_resolves_boundaries;
  return fallback_enabled && (unresolved_boundary || hardware_mismatch)
             ? MetalFinalPath::cpu_fallback
             : MetalFinalPath::hardware;
}

const char *metal_final_path_name(MetalFinalPath value) {
  switch (value) {
  case MetalFinalPath::hardware:
    return "hardware";
  case MetalFinalPath::cpu_fallback:
    return "cpu_fallback";
  }
  return "hardware";
}

Ray minimize_metal_mismatch_ray(const Ray &ray,
                                const std::function<bool(const Ray &)> &preserves_mismatch) {
  if (!preserves_mismatch(ray)) {
    return ray;
  }
  Ray reduced = ray;
  simplify_scalar(
      ray.origin.x, [](Ray &value, double candidate) { value.origin.x = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.origin.y, [](Ray &value, double candidate) { value.origin.y = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.origin.z, [](Ray &value, double candidate) { value.origin.z = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.direction.x, [](Ray &value, double candidate) { value.direction.x = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.direction.y, [](Ray &value, double candidate) { value.direction.y = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.direction.z, [](Ray &value, double candidate) { value.direction.z = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.minimum_t, [](Ray &value, double candidate) { value.minimum_t = candidate; },
      preserves_mismatch, reduced);
  simplify_scalar(
      ray.maximum_t, [](Ray &value, double candidate) { value.maximum_t = candidate; },
      preserves_mismatch, reduced);
  return reduced;
}

} // namespace tetcage
