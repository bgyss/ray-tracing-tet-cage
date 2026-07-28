#include "tetcage/math.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tetcage {
namespace {

Mat3 edge_matrix(const Tetrahedron &tet) {
  return {{{tet.positions[1] - tet.positions[0], tet.positions[2] - tet.positions[0],
            tet.positions[3] - tet.positions[0]}}};
}

double matrix_scale(const Mat3 &matrix) { return std::max(1.0, matrix.infinity_norm()); }

ClipVertex interpolate(const ClipVertex &a, const ClipVertex &b, double t,
                       std::uint8_t clipped_boundary) {
  ClipVertex result{};
  result.position = a.position + (b.position - a.position) * t;
  result.source_bary = a.source_bary + (b.source_bary - a.source_bary) * t;
  result.feature.cage_boundary_mask =
      static_cast<std::uint8_t>(a.feature.cage_boundary_mask & b.feature.cage_boundary_mask);
  result.feature.cage_boundary_mask =
      static_cast<std::uint8_t>(result.feature.cage_boundary_mask | clipped_boundary);
  result.feature.source_boundary_mask =
      static_cast<std::uint8_t>(a.feature.source_boundary_mask & b.feature.source_boundary_mask);
  return result;
}

std::uint8_t source_boundary_mask(Vec3 barycentric, double tolerance) {
  std::uint8_t mask = 0;
  if (std::abs(barycentric.x) <= tolerance) {
    mask = static_cast<std::uint8_t>(mask | 1U);
  }
  if (std::abs(barycentric.y) <= tolerance) {
    mask = static_cast<std::uint8_t>(mask | 2U);
  }
  if (std::abs(barycentric.z) <= tolerance) {
    mask = static_cast<std::uint8_t>(mask | 4U);
  }
  return mask;
}

} // namespace

double Vec4::operator[](std::size_t index) const {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    throw std::out_of_range("Vec4 index");
  }
}

double &Vec4::operator[](std::size_t index) {
  switch (index) {
  case 0:
    return x;
  case 1:
    return y;
  case 2:
    return z;
  case 3:
    return w;
  default:
    throw std::out_of_range("Vec4 index");
  }
}

Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 value, double scalar) {
  return {value.x * scalar, value.y * scalar, value.z * scalar};
}
Vec3 operator*(double scalar, Vec3 value) { return value * scalar; }
Vec3 operator/(Vec3 value, double scalar) { return value * (1.0 / scalar); }
Vec4 operator+(Vec4 a, Vec4 b) { return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w}; }
Vec4 operator-(Vec4 a, Vec4 b) { return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w}; }
Vec4 operator*(Vec4 value, double scalar) {
  return {value.x * scalar, value.y * scalar, value.z * scalar, value.w * scalar};
}
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(Vec3 value) { return std::sqrt(dot(value, value)); }

std::optional<Vec3> normalized(Vec3 value) {
  const double magnitude = length(value);
  if (!std::isfinite(magnitude) || magnitude <= std::numeric_limits<double>::min()) {
    return std::nullopt;
  }
  return value / magnitude;
}

Vec3 Mat3::operator*(Vec3 value) const {
  return columns[0] * value.x + columns[1] * value.y + columns[2] * value.z;
}

Mat3 Mat3::transposed() const {
  return {{{{columns[0].x, columns[1].x, columns[2].x},
            {columns[0].y, columns[1].y, columns[2].y},
            {columns[0].z, columns[1].z, columns[2].z}}}};
}

double Mat3::determinant() const { return dot(columns[0], cross(columns[1], columns[2])); }

std::optional<Mat3> Mat3::inverse(double relative_epsilon) const {
  const double det = determinant();
  const double threshold =
      relative_epsilon * matrix_scale(*this) * matrix_scale(*this) * matrix_scale(*this);
  if (!std::isfinite(det) || std::abs(det) <= threshold) {
    return std::nullopt;
  }
  const Vec3 row0 = cross(columns[1], columns[2]) / det;
  const Vec3 row1 = cross(columns[2], columns[0]) / det;
  const Vec3 row2 = cross(columns[0], columns[1]) / det;
  return Mat3{{{{row0.x, row1.x, row2.x}, {row0.y, row1.y, row2.y}, {row0.z, row1.z, row2.z}}}};
}

double Mat3::infinity_norm() const {
  const double row0 = std::abs(columns[0].x) + std::abs(columns[1].x) + std::abs(columns[2].x);
  const double row1 = std::abs(columns[0].y) + std::abs(columns[1].y) + std::abs(columns[2].y);
  const double row2 = std::abs(columns[0].z) + std::abs(columns[1].z) + std::abs(columns[2].z);
  return std::max({row0, row1, row2});
}

Vec3 Affine3::apply_point(Vec3 point) const { return linear * point + translation; }
Vec3 Affine3::apply_vector(Vec3 vector) const { return linear * vector; }

TetDiagnostics diagnose(const Tetrahedron &tet, double relative_epsilon) {
  const Mat3 matrix = edge_matrix(tet);
  TetDiagnostics result{};
  result.determinant = matrix.determinant();
  result.minimum_edge = std::numeric_limits<double>::infinity();
  for (std::size_t a = 0; a < tet.positions.size(); ++a) {
    for (std::size_t b = a + 1; b < tet.positions.size(); ++b) {
      result.minimum_edge =
          std::min(result.minimum_edge, length(tet.positions[a] - tet.positions[b]));
    }
  }
  const auto inverse = matrix.inverse(relative_epsilon);
  if (!inverse) {
    result.condition_estimate = std::numeric_limits<double>::infinity();
    result.classification = TetClass::near_singular;
    return result;
  }
  result.condition_estimate = matrix.infinity_norm() * inverse->infinity_norm();
  if (!std::isfinite(result.condition_estimate) ||
      result.condition_estimate >= 1.0 / relative_epsilon) {
    result.classification = TetClass::near_singular;
  } else if (result.determinant < 0.0) {
    result.classification = TetClass::mirrored;
  } else {
    result.classification = TetClass::healthy;
  }
  return result;
}

std::optional<Vec4> to_barycentric(const Tetrahedron &tet, Vec3 point, double relative_epsilon) {
  const auto inverse = edge_matrix(tet).inverse(relative_epsilon);
  if (!inverse) {
    return std::nullopt;
  }
  const Vec3 independent = *inverse * (point - tet.positions[0]);
  return Vec4{1.0 - independent.x - independent.y - independent.z, independent.x, independent.y,
              independent.z};
}

Vec3 from_barycentric(const Tetrahedron &tet, Vec4 barycentric) {
  return tet.positions[0] * barycentric.x + tet.positions[1] * barycentric.y +
         tet.positions[2] * barycentric.z + tet.positions[3] * barycentric.w;
}

std::optional<Affine3> canonical_to_object(const Tetrahedron &posed_tet, double relative_epsilon) {
  const Mat3 linear = edge_matrix(posed_tet);
  if (!linear.inverse(relative_epsilon)) {
    return std::nullopt;
  }
  return Affine3{linear, posed_tet.positions[0]};
}

std::optional<Vec3> transform_normal(const Mat3 &object_from_source, Vec3 normal,
                                     double relative_epsilon) {
  const auto inverse = object_from_source.inverse(relative_epsilon);
  if (!inverse) {
    return std::nullopt;
  }
  return normalized(inverse->transposed() * normal);
}

PlaneSide classify(const Plane &plane, Vec3 point, double tolerance) {
  const double signed_distance = dot(plane.normal, point) + plane.offset;
  if (signed_distance > tolerance) {
    return PlaneSide::inside;
  }
  if (signed_distance < -tolerance) {
    return PlaneSide::outside;
  }
  return PlaneSide::on;
}

std::vector<ClipVertex> clip_polygon_against_plane(const std::vector<ClipVertex> &polygon,
                                                   const Plane &plane, double tolerance,
                                                   std::uint8_t boundary_bit) {
  if (polygon.empty()) {
    return {};
  }
  std::vector<ClipVertex> output;
  output.reserve(polygon.size() + 2U);
  for (std::size_t current_index = 0; current_index < polygon.size(); ++current_index) {
    const auto &current = polygon[current_index];
    const auto &previous = polygon[(current_index + polygon.size() - 1U) % polygon.size()];
    const double current_distance = dot(plane.normal, current.position) + plane.offset;
    const double previous_distance = dot(plane.normal, previous.position) + plane.offset;
    const bool current_inside = current_distance >= -tolerance;
    const bool previous_inside = previous_distance >= -tolerance;
    if (current_inside != previous_inside) {
      const double denominator = previous_distance - current_distance;
      if (std::abs(denominator) > std::numeric_limits<double>::min()) {
        const double t = std::clamp(previous_distance / denominator, 0.0, 1.0);
        output.push_back(interpolate(previous, current, t, boundary_bit));
      }
    }
    if (current_inside) {
      ClipVertex kept = current;
      if (std::abs(current_distance) <= tolerance) {
        kept.feature.cage_boundary_mask =
            static_cast<std::uint8_t>(kept.feature.cage_boundary_mask | boundary_bit);
      }
      output.push_back(kept);
    }
  }
  return output;
}

std::array<std::uint64_t, 3> canonical_face_identity(const Tetrahedron &tet,
                                                     std::size_t opposite_corner) {
  if (opposite_corner >= 4U) {
    throw std::out_of_range("tetrahedron face index");
  }
  std::array<std::uint64_t, 3> result{};
  std::size_t output = 0;
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    if (corner != opposite_corner) {
      result[output++] = tet.vertex_ids[corner];
    }
  }
  std::sort(result.begin(), result.end());
  return result;
}

std::vector<ClipVertex> clip_triangle_to_tetrahedron(const std::array<ClipVertex, 3> &triangle,
                                                     const Tetrahedron &tet,
                                                     double expanded_barycentric_epsilon) {
  std::vector<ClipVertex> polygon(triangle.begin(), triangle.end());
  const double feature_tolerance = std::max(1.0e-13, std::abs(expanded_barycentric_epsilon) * 0.25);
  for (auto &vertex : polygon) {
    vertex.feature.source_boundary_mask = source_boundary_mask(vertex.source_bary, 1.0e-13);
  }

  for (std::size_t coordinate = 0; coordinate < 4; ++coordinate) {
    if (polygon.empty()) {
      break;
    }
    std::vector<ClipVertex> output;
    output.reserve(polygon.size() + 2);
    for (std::size_t current_index = 0; current_index < polygon.size(); ++current_index) {
      const ClipVertex &current = polygon[current_index];
      const ClipVertex &previous = polygon[(current_index + polygon.size() - 1) % polygon.size()];
      const auto current_bary = to_barycentric(tet, current.position);
      const auto previous_bary = to_barycentric(tet, previous.position);
      if (!current_bary || !previous_bary) {
        return {};
      }
      const double current_distance = (*current_bary)[coordinate] + expanded_barycentric_epsilon;
      const double previous_distance = (*previous_bary)[coordinate] + expanded_barycentric_epsilon;
      const bool current_inside = current_distance >= -feature_tolerance;
      const bool previous_inside = previous_distance >= -feature_tolerance;
      const auto boundary_bit = static_cast<std::uint8_t>(1U << coordinate);

      if (current_inside != previous_inside) {
        const double denominator = previous_distance - current_distance;
        if (std::abs(denominator) > std::numeric_limits<double>::min()) {
          const double t = std::clamp(previous_distance / denominator, 0.0, 1.0);
          output.push_back(interpolate(previous, current, t, boundary_bit));
        }
      }
      if (current_inside) {
        ClipVertex kept = current;
        if (std::abs(current_distance) <= feature_tolerance) {
          kept.feature.cage_boundary_mask =
              static_cast<std::uint8_t>(kept.feature.cage_boundary_mask | boundary_bit);
        }
        output.push_back(kept);
      }
    }
    polygon = std::move(output);
  }
  return polygon;
}

} // namespace tetcage
