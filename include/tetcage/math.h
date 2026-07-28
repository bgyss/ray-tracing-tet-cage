#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace tetcage {

struct Vec2 {
  double x{};
  double y{};
};

struct Vec3 {
  double x{};
  double y{};
  double z{};
};

struct Vec4 {
  double x{};
  double y{};
  double z{};
  double w{};

  [[nodiscard]] double operator[](std::size_t index) const;
  [[nodiscard]] double &operator[](std::size_t index);
};

[[nodiscard]] Vec3 operator+(Vec3 a, Vec3 b);
[[nodiscard]] Vec3 operator-(Vec3 a, Vec3 b);
[[nodiscard]] Vec3 operator*(Vec3 value, double scalar);
[[nodiscard]] Vec3 operator*(double scalar, Vec3 value);
[[nodiscard]] Vec3 operator/(Vec3 value, double scalar);
[[nodiscard]] Vec4 operator+(Vec4 a, Vec4 b);
[[nodiscard]] Vec4 operator-(Vec4 a, Vec4 b);
[[nodiscard]] Vec4 operator*(Vec4 value, double scalar);
[[nodiscard]] double dot(Vec3 a, Vec3 b);
[[nodiscard]] Vec3 cross(Vec3 a, Vec3 b);
[[nodiscard]] double length(Vec3 value);
[[nodiscard]] std::optional<Vec3> normalized(Vec3 value);

struct Mat3 {
  // Column-major mathematical storage. This matches the paper's
  // [v1-v0 | v2-v0 | v3-v0] notation without relying on API memory layout.
  std::array<Vec3, 3> columns{};

  [[nodiscard]] Vec3 operator*(Vec3 value) const;
  [[nodiscard]] Mat3 transposed() const;
  [[nodiscard]] double determinant() const;
  [[nodiscard]] std::optional<Mat3> inverse(double relative_epsilon = 1.0e-14) const;
  [[nodiscard]] double infinity_norm() const;
};

struct Affine3 {
  Mat3 linear{};
  Vec3 translation{};

  [[nodiscard]] Vec3 apply_point(Vec3 point) const;
  [[nodiscard]] Vec3 apply_vector(Vec3 vector) const;
};

struct Tetrahedron {
  std::array<Vec3, 4> positions{};
  std::array<std::uint64_t, 4> vertex_ids{};
};

enum class TetClass : std::uint8_t {
  healthy,
  mirrored,
  near_singular,
};

struct TetDiagnostics {
  double determinant{};
  double condition_estimate{};
  double minimum_edge{};
  TetClass classification{TetClass::near_singular};
};

enum class RobustPolicyDecision : std::uint8_t {
  fast_path,
  conservative_boundary,
  conventional_fallback,
  invalid_input,
};

struct RobustToleranceInput {
  double coordinate_scale{1.0};
  double minimum_edge{1.0};
  double condition_estimate{1.0};
  std::uint32_t ulp_multiplier{8U};
};

struct RobustToleranceResult {
  double position_epsilon{};
  double barycentric_epsilon{};
  double condition_factor{};
  double edge_factor{};
  RobustPolicyDecision decision{RobustPolicyDecision::invalid_input};
  std::string reason;
};

[[nodiscard]] TetDiagnostics diagnose(const Tetrahedron &tet, double relative_epsilon = 1.0e-12);
[[nodiscard]] RobustToleranceResult derive_robust_tolerance(const RobustToleranceInput &input);
[[nodiscard]] const char *robust_policy_decision_name(RobustPolicyDecision decision);
[[nodiscard]] std::optional<Vec4> to_barycentric(const Tetrahedron &tet, Vec3 point,
                                                 double relative_epsilon = 1.0e-12);
[[nodiscard]] Vec3 from_barycentric(const Tetrahedron &tet, Vec4 barycentric);
[[nodiscard]] std::optional<Affine3> canonical_to_object(const Tetrahedron &posed_tet,
                                                         double relative_epsilon = 1.0e-12);
[[nodiscard]] std::optional<Vec3> transform_normal(const Mat3 &object_from_source, Vec3 normal,
                                                   double relative_epsilon = 1.0e-14);

struct FeatureIdentity {
  std::uint8_t cage_boundary_mask{};
  std::uint8_t source_boundary_mask{};
};

struct ClipVertex {
  Vec3 position{};
  Vec3 source_bary{};
  FeatureIdentity feature{};
};

struct Plane {
  Vec3 normal{};
  double offset{};
};

enum class PlaneSide : std::uint8_t {
  outside,
  on,
  inside,
};

[[nodiscard]] PlaneSide classify(const Plane &plane, Vec3 point, double tolerance);
[[nodiscard]] std::vector<ClipVertex>
clip_polygon_against_plane(const std::vector<ClipVertex> &polygon, const Plane &plane,
                           double tolerance, std::uint8_t boundary_bit);
[[nodiscard]] std::array<std::uint64_t, 3> canonical_face_identity(const Tetrahedron &tet,
                                                                   std::size_t opposite_corner);

[[nodiscard]] std::vector<ClipVertex>
clip_triangle_to_tetrahedron(const std::array<ClipVertex, 3> &triangle, const Tetrahedron &tet,
                             double expanded_barycentric_epsilon);

} // namespace tetcage
