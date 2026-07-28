#include "tetcage/oracle.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace tetcage {
namespace {

constexpr double intersection_epsilon = 1.0e-12;

double component(Vec3 value, std::size_t axis) {
  if (axis == 0U) {
    return value.x;
  }
  if (axis == 1U) {
    return value.y;
  }
  return value.z;
}

void set_component(Vec3 &value, std::size_t axis, double component_value) {
  if (axis == 0U) {
    value.x = component_value;
  } else if (axis == 1U) {
    value.y = component_value;
  } else {
    value.z = component_value;
  }
}

std::array<std::uint8_t, 4> ordered_to_local(const CompiledAsset &asset, std::uint32_t tet_id) {
  std::array<std::pair<std::uint64_t, std::uint8_t>, 4> order{};
  const auto &tet = asset.cage.tetrahedra[tet_id];
  for (std::uint8_t local = 0; local < 4U; ++local) {
    order[local] = {asset.cage.vertex_ids[tet.vertex_indices[local]], local};
  }
  std::sort(order.begin(), order.end());
  std::array<std::uint8_t, 4> result{};
  for (std::size_t ordered = 0; ordered < 4U; ++ordered) {
    result[ordered] = order[ordered].second;
  }
  return result;
}

Vec4 reorder_barycentric(Vec4 local, const std::array<std::uint8_t, 4> &order) {
  Vec4 ordered{};
  for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
    ordered[coordinate] = local[order[coordinate]];
  }
  return ordered;
}

std::array<Vec3, 4> posed_tet_vertices(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                                       std::uint32_t tet_id) {
  if (pose.size() != asset.cage.vertices.size()) {
    throw std::invalid_argument("posed cage vertex count does not match asset cage");
  }
  std::array<Vec3, 4> result{};
  const auto &tet = asset.cage.tetrahedra[tet_id];
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    result[corner] = pose[tet.vertex_indices[corner]];
  }
  return result;
}

Vec3 reconstruct_local(Vec4 barycentric, const std::array<Vec3, 4> &vertices) {
  return vertices[0] * barycentric.x + vertices[1] * barycentric.y + vertices[2] * barycentric.z +
         vertices[3] * barycentric.w;
}

struct TriangleIntersection {
  double t{};
  Vec3 barycentric{};
};

std::optional<TriangleIntersection> intersect_moller(const Ray &ray, Vec3 a, Vec3 b, Vec3 c) {
  const Vec3 edge_ab = b - a;
  const Vec3 edge_ac = c - a;
  const Vec3 p = cross(ray.direction, edge_ac);
  const double determinant = dot(edge_ab, p);
  const double determinant_scale =
      std::max(std::numeric_limits<double>::min(), length(edge_ab) * length(p));
  if (std::abs(determinant) <= intersection_epsilon * determinant_scale) {
    return std::nullopt;
  }
  const double inverse_determinant = 1.0 / determinant;
  const Vec3 translated = ray.origin - a;
  const double u = dot(translated, p) * inverse_determinant;
  const Vec3 q = cross(translated, edge_ab);
  const double v = dot(ray.direction, q) * inverse_determinant;
  const double t = dot(edge_ac, q) * inverse_determinant;
  const double relative_determinant = std::abs(determinant) / determinant_scale;
  const double tolerance =
      std::min(1.0e-6, std::max(2.0e-11, 64.0 * std::numeric_limits<double>::epsilon() /
                                             std::max(relative_determinant, 1.0e-12)));
  if (u < -tolerance || v < -tolerance || u + v > 1.0 + tolerance || t < ray.minimum_t ||
      t > ray.maximum_t) {
    return std::nullopt;
  }
  return TriangleIntersection{t, {1.0 - u - v, u, v}};
}

double orient2d(double ax, double ay, double bx, double by, double cx, double cy) {
  return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

std::optional<TriangleIntersection> intersect_projected_edges(const Ray &ray, Vec3 a, Vec3 b,
                                                              Vec3 c) {
  const Vec3 normal = cross(b - a, c - a);
  const double denominator = dot(normal, ray.direction);
  const double scale =
      std::max(std::numeric_limits<double>::min(), length(normal) * length(ray.direction));
  if (std::abs(denominator) <= intersection_epsilon * scale) {
    return std::nullopt;
  }
  const double t = dot(normal, a - ray.origin) / denominator;
  if (t < ray.minimum_t || t > ray.maximum_t) {
    return std::nullopt;
  }
  const Vec3 point = ray.origin + ray.direction * t;
  const Vec3 absolute_normal{std::abs(normal.x), std::abs(normal.y), std::abs(normal.z)};
  std::size_t dropped_axis = 0;
  if (absolute_normal.y > absolute_normal.x) {
    dropped_axis = 1;
  }
  if (absolute_normal.z > component(absolute_normal, dropped_axis)) {
    dropped_axis = 2;
  }
  const std::size_t x_axis = (dropped_axis + 1U) % 3U;
  const std::size_t y_axis = (dropped_axis + 2U) % 3U;
  const double area = orient2d(component(a, x_axis), component(a, y_axis), component(b, x_axis),
                               component(b, y_axis), component(c, x_axis), component(c, y_axis));
  if (std::abs(area) <= std::numeric_limits<double>::min()) {
    return std::nullopt;
  }
  const double w0 =
      orient2d(component(b, x_axis), component(b, y_axis), component(c, x_axis),
               component(c, y_axis), component(point, x_axis), component(point, y_axis)) /
      area;
  const double w1 =
      orient2d(component(c, x_axis), component(c, y_axis), component(a, x_axis),
               component(a, y_axis), component(point, x_axis), component(point, y_axis)) /
      area;
  const double w2 = 1.0 - w0 - w1;
  const double relative_denominator = std::abs(denominator) / scale;
  const double maximum_edge_squared =
      std::max({dot(b - a, b - a), dot(c - b, c - b), dot(a - c, a - c)});
  const double triangle_condition =
      maximum_edge_squared / std::max(std::numeric_limits<double>::min(), length(normal));
  const double tolerance = std::min(
      1.0e-6, std::max(2.0e-11, 128.0 * std::numeric_limits<double>::epsilon() *
                                    triangle_condition / std::max(relative_denominator, 1.0e-12)));
  if (w0 < -tolerance || w1 < -tolerance || w2 < -tolerance) {
    return std::nullopt;
  }
  return TriangleIntersection{t, {w0, w1, w2}};
}

const SourceTriangle *find_source_triangle(const CompiledAsset &asset, std::uint32_t primitive) {
  const auto found = std::find_if(
      asset.source.triangles.begin(), asset.source.triangles.end(),
      [primitive](const auto &triangle) { return triangle.primitive_id == primitive; });
  return found == asset.source.triangles.end() ? nullptr : &*found;
}

TraceHit make_hit(const CompiledAsset &asset, std::uint32_t micro_triangle_index,
                  const TriangleIntersection &intersection, const Ray &ray) {
  const auto &micro = asset.micro_triangles[micro_triangle_index];
  const auto &generated_a = asset.generated_vertices[micro.vertex_indices[0]];
  const auto &generated_b = asset.generated_vertices[micro.vertex_indices[1]];
  const auto &generated_c = asset.generated_vertices[micro.vertex_indices[2]];
  const Vec3 source_bary = generated_a.source_barycentric * intersection.barycentric.x +
                           generated_b.source_barycentric * intersection.barycentric.y +
                           generated_c.source_barycentric * intersection.barycentric.z;
  TraceHit hit{};
  hit.t = intersection.t;
  hit.position = ray.origin + ray.direction * hit.t;
  hit.source_barycentric = source_bary;
  hit.source_primitive = micro.source_primitive;
  hit.material = micro.material;
  hit.tet_id = micro.tet_id;
  hit.micro_triangle = micro_triangle_index;
  const SourceTriangle *source = find_source_triangle(asset, micro.source_primitive);
  if (source != nullptr) {
    const auto &a = asset.source.vertices[source->vertex_indices[0]];
    const auto &b = asset.source.vertices[source->vertex_indices[1]];
    const auto &c = asset.source.vertices[source->vertex_indices[2]];
    hit.normal = a.normal * source_bary.x + b.normal * source_bary.y + c.normal * source_bary.z;
    if (const auto unit = normalized(hit.normal)) {
      hit.normal = *unit;
    }
    hit.uv = {a.uv.x * source_bary.x + b.uv.x * source_bary.y + c.uv.x * source_bary.z,
              a.uv.y * source_bary.x + b.uv.y * source_bary.y + c.uv.y * source_bary.z};
  }
  return hit;
}

TraceHit make_dense_hit(const SourceMesh &mesh, std::uint32_t triangle_index,
                        const TriangleIntersection &intersection, const Ray &ray) {
  const auto &triangle = mesh.triangles[triangle_index];
  const auto &a = mesh.vertices[triangle.vertex_indices[0]];
  const auto &b = mesh.vertices[triangle.vertex_indices[1]];
  const auto &c = mesh.vertices[triangle.vertex_indices[2]];
  TraceHit hit{};
  hit.t = intersection.t;
  hit.position = ray.origin + ray.direction * hit.t;
  hit.source_barycentric = intersection.barycentric;
  hit.normal = a.normal * intersection.barycentric.x + b.normal * intersection.barycentric.y +
               c.normal * intersection.barycentric.z;
  if (const auto unit = normalized(hit.normal)) {
    hit.normal = *unit;
  }
  hit.uv = {a.uv.x * intersection.barycentric.x + b.uv.x * intersection.barycentric.y +
                c.uv.x * intersection.barycentric.z,
            a.uv.y * intersection.barycentric.x + b.uv.y * intersection.barycentric.y +
                c.uv.y * intersection.barycentric.z};
  hit.source_primitive = triangle.primitive_id;
  hit.material = triangle.material_id;
  hit.micro_triangle = triangle_index;
  return hit;
}

TraceResult resolve_hits(std::vector<TraceHit> hits, std::uint64_t visited_nodes) {
  TraceResult result{};
  result.raw_hits = hits.size();
  result.visited_nodes = visited_nodes;
  std::sort(hits.begin(), hits.end(), [](const TraceHit &a, const TraceHit &b) {
    return std::tie(a.t, a.source_primitive, a.tet_id, a.micro_triangle) <
           std::tie(b.t, b.source_primitive, b.tet_id, b.micro_triangle);
  });
  std::vector<TraceHit> resolved;
  for (const auto &hit : hits) {
    const auto duplicate = std::find_if(resolved.begin(), resolved.end(), [&hit](const auto &kept) {
      const double tolerance = 2.0e-10 * std::max({1.0, std::abs(hit.t), std::abs(kept.t)});
      return hit.source_primitive == kept.source_primitive && std::abs(hit.t - kept.t) <= tolerance;
    });
    if (duplicate == resolved.end()) {
      resolved.push_back(hit);
    } else {
      ++result.raw_duplicate_candidates;
      if (std::tie(hit.tet_id, hit.micro_triangle) <
          std::tie(duplicate->tet_id, duplicate->micro_triangle)) {
        *duplicate = hit;
      }
    }
  }
  std::sort(resolved.begin(), resolved.end(), [](const TraceHit &a, const TraceHit &b) {
    return std::tie(a.t, a.source_primitive, a.tet_id, a.micro_triangle) <
           std::tie(b.t, b.source_primitive, b.tet_id, b.micro_triangle);
  });
  result.resolved_hits = resolved.size();
  for (std::size_t index = 1; index < resolved.size(); ++index) {
    const double tolerance =
        2.0e-10 * std::max({1.0, std::abs(resolved[index - 1].t), std::abs(resolved[index].t)});
    if (resolved[index - 1].source_primitive == resolved[index].source_primitive &&
        std::abs(resolved[index - 1].t - resolved[index].t) <= tolerance) {
      ++result.duplicate_ownership;
    }
  }
  if (!resolved.empty()) {
    result.closest = resolved.front();
  }
  return result;
}

bool intersects(const Aabb &bounds, const Ray &ray) {
  double minimum_t = ray.minimum_t;
  double maximum_t = ray.maximum_t;
  for (std::size_t axis = 0; axis < 3U; ++axis) {
    const double origin = component(ray.origin, axis);
    const double direction = component(ray.direction, axis);
    const double minimum = component(bounds.minimum, axis);
    const double maximum = component(bounds.maximum, axis);
    if (std::abs(direction) <= std::numeric_limits<double>::min()) {
      if (origin < minimum || origin > maximum) {
        return false;
      }
      continue;
    }
    double first = (minimum - origin) / direction;
    double second = (maximum - origin) / direction;
    if (first > second) {
      std::swap(first, second);
    }
    minimum_t = std::max(minimum_t, first);
    maximum_t = std::min(maximum_t, second);
    if (minimum_t > maximum_t) {
      return false;
    }
  }
  return true;
}

BarycentricBounds triangle_bounds(const CompiledAsset &asset, std::uint32_t triangle_index,
                                  const std::array<std::uint8_t, 4> &order) {
  BarycentricBounds bounds{};
  bounds.lower = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  bounds.upper = {
      -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
  const auto &triangle = asset.micro_triangles[triangle_index];
  for (const auto vertex_index : triangle.vertex_indices) {
    const auto bary =
        reorder_barycentric(asset.generated_vertices[vertex_index].cage_barycentric, order);
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      bounds.lower[coordinate] = std::min(bounds.lower[coordinate], bary[coordinate]);
      bounds.upper[coordinate] = std::max(bounds.upper[coordinate], bary[coordinate]);
    }
  }
  return bounds;
}

BarycentricBounds range_bounds(const CompiledAsset &asset, const Bvh4DTree &tree, std::size_t begin,
                               std::size_t end) {
  BarycentricBounds result{};
  result.lower = {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
                  std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()};
  result.upper = {
      -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()};
  for (std::size_t index = begin; index < end; ++index) {
    const auto bounds = triangle_bounds(asset, tree.triangle_indices[index], tree.ordered_to_local);
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      result.lower[coordinate] = std::min(result.lower[coordinate], bounds.lower[coordinate]);
      result.upper[coordinate] = std::max(result.upper[coordinate], bounds.upper[coordinate]);
    }
  }
  return result;
}

double centroid_coordinate(const CompiledAsset &asset, std::uint32_t triangle_index,
                           const std::array<std::uint8_t, 4> &order, std::size_t coordinate) {
  const auto &triangle = asset.micro_triangles[triangle_index];
  double sum = 0.0;
  for (const auto vertex_index : triangle.vertex_indices) {
    sum += reorder_barycentric(asset.generated_vertices[vertex_index].cage_barycentric,
                               order)[coordinate];
  }
  return sum / 3.0;
}

std::array<Vec3, 4> ordered_pose(const CompiledAsset &asset, const std::vector<Vec3> &pose,
                                 const Bvh4DTree &tree) {
  const auto local = posed_tet_vertices(asset, pose, tree.tet_id);
  std::array<Vec3, 4> result{};
  for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
    result[coordinate] = local[tree.ordered_to_local[coordinate]];
  }
  return result;
}

std::optional<Aabb> project_node(const BarycentricBounds &bounds, const std::array<Vec3, 4> &pose,
                                 ProjectionMode mode) {
  std::optional<Aabb> projected;
  if (mode == ProjectionMode::interval_sum) {
    projected = project_bounds_interval_sum(bounds, pose);
  } else {
    projected = project_bounds_bounded_simplex(bounds, pose);
  }
  if (!projected) {
    return std::nullopt;
  }
  for (std::size_t axis = 0; axis < 3U; ++axis) {
    double minimum = component(projected->minimum, axis);
    double maximum = component(projected->maximum, axis);
    for (int step = 0; step < 16; ++step) {
      minimum = std::nextafter(minimum, -std::numeric_limits<double>::infinity());
      maximum = std::nextafter(maximum, std::numeric_limits<double>::infinity());
    }
    set_component(projected->minimum, axis, minimum);
    set_component(projected->maximum, axis, maximum);
  }
  return projected;
}

} // namespace

std::optional<std::pair<double, double>>
bounded_simplex_extrema(const BarycentricBounds &bounds,
                        const std::array<double, 4> &coefficients) {
  double lower_sum = 0.0;
  double upper_sum = 0.0;
  for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
    if (!std::isfinite(bounds.lower[coordinate]) || !std::isfinite(bounds.upper[coordinate]) ||
        bounds.lower[coordinate] > bounds.upper[coordinate]) {
      return std::nullopt;
    }
    lower_sum += bounds.lower[coordinate];
    upper_sum += bounds.upper[coordinate];
  }
  if (lower_sum > 1.0 + 1.0e-12 || upper_sum < 1.0 - 1.0e-12) {
    return std::nullopt;
  }
  const auto extreme = [&](bool maximum) -> std::optional<double> {
    std::array<std::size_t, 4> order{0U, 1U, 2U, 3U};
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
      return maximum ? coefficients[a] > coefficients[b] : coefficients[a] < coefficients[b];
    });
    std::array<double, 4> values{};
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      values[coordinate] = bounds.lower[coordinate];
    }
    double remaining = std::max(0.0, 1.0 - lower_sum);
    for (const auto coordinate : order) {
      const double capacity = bounds.upper[coordinate] - bounds.lower[coordinate];
      const double assigned = std::min(capacity, remaining);
      values[coordinate] += assigned;
      remaining -= assigned;
    }
    if (remaining > 1.0e-10) {
      return std::nullopt;
    }
    double value = 0.0;
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      value += coefficients[coordinate] * values[coordinate];
    }
    return value;
  };
  const auto minimum = extreme(false);
  const auto maximum = extreme(true);
  if (!minimum || !maximum) {
    return std::nullopt;
  }
  return std::pair<double, double>{*minimum, *maximum};
}

Aabb project_bounds_interval_sum(const BarycentricBounds &bounds,
                                 const std::array<Vec3, 4> &posed_vertices) {
  Aabb result{};
  for (std::size_t axis = 0; axis < 3U; ++axis) {
    double minimum = 0.0;
    double maximum = 0.0;
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      const double a = bounds.lower[coordinate] * component(posed_vertices[coordinate], axis);
      const double b = bounds.upper[coordinate] * component(posed_vertices[coordinate], axis);
      minimum += std::min(a, b);
      maximum += std::max(a, b);
    }
    set_component(result.minimum, axis, minimum);
    set_component(result.maximum, axis, maximum);
  }
  return result;
}

std::optional<Aabb> project_bounds_bounded_simplex(const BarycentricBounds &bounds,
                                                   const std::array<Vec3, 4> &posed_vertices) {
  Aabb result{};
  for (std::size_t axis = 0; axis < 3U; ++axis) {
    std::array<double, 4> coefficients{};
    for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
      coefficients[coordinate] = component(posed_vertices[coordinate], axis);
    }
    const auto extrema = bounded_simplex_extrema(bounds, coefficients);
    if (!extrema) {
      return std::nullopt;
    }
    set_component(result.minimum, axis, extrema->first);
    set_component(result.maximum, axis, extrema->second);
  }
  return result;
}

double aabb_volume(const Aabb &bounds) {
  return std::max(0.0, bounds.maximum.x - bounds.minimum.x) *
         std::max(0.0, bounds.maximum.y - bounds.minimum.y) *
         std::max(0.0, bounds.maximum.z - bounds.minimum.z);
}

Bvh4D build_bvh4d(const CompiledAsset &asset, std::uint32_t leaf_size) {
  if (leaf_size == 0U) {
    throw std::invalid_argument("4D BVH leaf size must be positive");
  }
  Bvh4D bvh{};
  bvh.leaf_size = leaf_size;
  std::map<std::uint32_t, std::vector<std::uint32_t>> by_tet;
  for (std::uint32_t index = 0; index < asset.micro_triangles.size(); ++index) {
    by_tet[asset.micro_triangles[index].tet_id].push_back(index);
  }
  for (auto &[tet_id, indices] : by_tet) {
    Bvh4DTree tree{};
    tree.tet_id = tet_id;
    tree.ordered_to_local = ordered_to_local(asset, tet_id);
    tree.triangle_indices = std::move(indices);
    std::function<std::uint32_t(std::size_t, std::size_t)> build =
        [&](std::size_t begin, std::size_t end) -> std::uint32_t {
      const auto node_index = static_cast<std::uint32_t>(tree.nodes.size());
      tree.nodes.push_back({});
      tree.nodes[node_index].bounds = range_bounds(asset, tree, begin, end);
      tree.nodes[node_index].first = static_cast<std::uint32_t>(begin);
      tree.nodes[node_index].count = static_cast<std::uint32_t>(end - begin);
      if (end - begin <= leaf_size) {
        tree.nodes[node_index].leaf = true;
        return node_index;
      }
      std::size_t split_coordinate = 0;
      double largest_extent = -1.0;
      for (std::size_t coordinate = 0; coordinate < 4U; ++coordinate) {
        const double extent = tree.nodes[node_index].bounds.upper[coordinate] -
                              tree.nodes[node_index].bounds.lower[coordinate];
        if (extent > largest_extent) {
          largest_extent = extent;
          split_coordinate = coordinate;
        }
      }
      std::stable_sort(tree.triangle_indices.begin() + static_cast<std::ptrdiff_t>(begin),
                       tree.triangle_indices.begin() + static_cast<std::ptrdiff_t>(end),
                       [&](std::uint32_t a, std::uint32_t b) {
                         const double ca =
                             centroid_coordinate(asset, a, tree.ordered_to_local, split_coordinate);
                         const double cb =
                             centroid_coordinate(asset, b, tree.ordered_to_local, split_coordinate);
                         return std::tie(ca, a) < std::tie(cb, b);
                       });
      const std::size_t middle = begin + (end - begin) / 2U;
      tree.nodes[node_index].left = build(begin, middle);
      tree.nodes[node_index].right = build(middle, end);
      return node_index;
    };
    if (!tree.triangle_indices.empty()) {
      static_cast<void>(build(0U, tree.triangle_indices.size()));
    }
    bvh.trees.push_back(std::move(tree));
  }
  return bvh;
}

TraceResult trace_fast(const CompiledAsset &asset, const std::vector<Vec3> &posed_cage_vertices,
                       const Ray &ray) {
  std::vector<TraceHit> hits;
  hits.reserve(8U);
  for (std::uint32_t triangle_index = 0; triangle_index < asset.micro_triangles.size();
       ++triangle_index) {
    const auto &triangle = asset.micro_triangles[triangle_index];
    const auto pose = posed_tet_vertices(asset, posed_cage_vertices, triangle.tet_id);
    const Vec3 a = reconstruct_local(
        asset.generated_vertices[triangle.vertex_indices[0]].cage_barycentric, pose);
    const Vec3 b = reconstruct_local(
        asset.generated_vertices[triangle.vertex_indices[1]].cage_barycentric, pose);
    const Vec3 c = reconstruct_local(
        asset.generated_vertices[triangle.vertex_indices[2]].cage_barycentric, pose);
    const auto intersection = intersect_moller(ray, a, b, c);
    if (intersection) {
      hits.push_back(make_hit(asset, triangle_index, *intersection, ray));
    }
  }
  auto result = resolve_hits(std::move(hits), 0U);
  result.triangle_tests = asset.micro_triangles.size();
  return result;
}

TraceResult trace_dense(const SourceMesh &mesh, const Ray &ray) {
  std::vector<TraceHit> hits;
  for (std::uint32_t triangle_index = 0; triangle_index < mesh.triangles.size(); ++triangle_index) {
    const auto &triangle = mesh.triangles[triangle_index];
    const auto intersection =
        intersect_moller(ray, mesh.vertices[triangle.vertex_indices[0]].position,
                         mesh.vertices[triangle.vertex_indices[1]].position,
                         mesh.vertices[triangle.vertex_indices[2]].position);
    if (intersection) {
      hits.push_back(make_dense_hit(mesh, triangle_index, *intersection, ray));
    }
  }
  auto result = resolve_hits(std::move(hits), 0U);
  result.triangle_tests = mesh.triangles.size();
  return result;
}

TraceResult trace_watertight4d(const CompiledAsset &asset, const Bvh4D &bvh,
                               const std::vector<Vec3> &posed_cage_vertices, const Ray &ray,
                               ProjectionMode projection) {
  std::vector<TraceHit> hits;
  hits.reserve(8U);
  std::uint64_t visited_nodes = 0;
  std::uint64_t triangle_tests = 0;
  for (const auto &tree : bvh.trees) {
    if (tree.nodes.empty()) {
      continue;
    }
    const auto pose = ordered_pose(asset, posed_cage_vertices, tree);
    std::vector<std::uint32_t> stack{0U};
    while (!stack.empty()) {
      const auto node_index = stack.back();
      stack.pop_back();
      ++visited_nodes;
      const auto &node = tree.nodes[node_index];
      const auto projected = project_node(node.bounds, pose, projection);
      if (!projected || !intersects(*projected, ray)) {
        continue;
      }
      if (!node.leaf) {
        stack.push_back(node.right);
        stack.push_back(node.left);
        continue;
      }
      for (std::uint32_t offset = 0; offset < node.count; ++offset) {
        ++triangle_tests;
        const auto triangle_index = tree.triangle_indices[node.first + offset];
        const auto &triangle = asset.micro_triangles[triangle_index];
        std::array<Vec3, 3> vertices{};
        for (std::size_t corner = 0; corner < 3U; ++corner) {
          const auto local_bary =
              asset.generated_vertices[triangle.vertex_indices[corner]].cage_barycentric;
          vertices[corner] =
              reconstruct_local(reorder_barycentric(local_bary, tree.ordered_to_local), pose);
        }
        const auto intersection =
            intersect_projected_edges(ray, vertices[0], vertices[1], vertices[2]);
        if (intersection) {
          hits.push_back(make_hit(asset, triangle_index, *intersection, ray));
        }
      }
    }
  }
  auto result = resolve_hits(std::move(hits), visited_nodes);
  result.triangle_tests = triangle_tests;
  return result;
}

std::vector<Ray> generate_adversarial_rays(const CompiledAsset &asset,
                                           const std::vector<Vec3> &posed_cage_vertices,
                                           std::uint64_t seed, std::uint32_t random_count) {
  std::vector<Ray> rays;
  if (asset.micro_triangles.empty()) {
    return rays;
  }
  std::vector<std::array<Vec3, 3>> world_triangles;
  world_triangles.reserve(asset.micro_triangles.size());
  for (const auto &triangle : asset.micro_triangles) {
    const auto pose = posed_tet_vertices(asset, posed_cage_vertices, triangle.tet_id);
    world_triangles.push_back(
        {reconstruct_local(asset.generated_vertices[triangle.vertex_indices[0]].cage_barycentric,
                           pose),
         reconstruct_local(asset.generated_vertices[triangle.vertex_indices[1]].cage_barycentric,
                           pose),
         reconstruct_local(asset.generated_vertices[triangle.vertex_indices[2]].cage_barycentric,
                           pose)});
  }
  for (const auto &triangle : world_triangles) {
    const auto unit_normal =
        normalized(cross(triangle[1] - triangle[0], triangle[2] - triangle[0]));
    if (!unit_normal) {
      continue;
    }
    const double distance = std::max(
        1.0, std::max({length(triangle[1] - triangle[0]), length(triangle[2] - triangle[1]),
                       length(triangle[0] - triangle[2])}));
    const auto emit = [&](Vec3 target) {
      rays.push_back({target + *unit_normal * distance, *unit_normal * -1.0, 0.0, distance * 4.0});
    };
    emit((triangle[0] + triangle[1] + triangle[2]) / 3.0);
    emit(triangle[0]);
    emit(triangle[1]);
    emit(triangle[2]);
    emit((triangle[0] + triangle[1]) / 2.0);
    emit((triangle[1] + triangle[2]) / 2.0);
    emit((triangle[2] + triangle[0]) / 2.0);
    const auto tangent = normalized(triangle[1] - triangle[0]);
    if (tangent) {
      const Vec3 target = (triangle[0] + triangle[1] + triangle[2]) / 3.0;
      const Vec3 direction = *tangent + *unit_normal * -1.0e-7;
      const auto unit_direction = normalized(direction);
      if (unit_direction) {
        rays.push_back({target - *tangent * distance + *unit_normal * (distance * 1.0e-7),
                        *unit_direction, 0.0, distance * 4.0});
      }
    }
  }
  std::mt19937_64 random(seed);
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  std::uniform_int_distribution<std::size_t> triangle_distribution(0U, world_triangles.size() - 1U);
  for (std::uint32_t sample = 0; sample < random_count; ++sample) {
    const auto &triangle = world_triangles[triangle_distribution(random)];
    const auto unit_normal =
        normalized(cross(triangle[1] - triangle[0], triangle[2] - triangle[0]));
    if (!unit_normal) {
      --sample;
      continue;
    }
    const double root = std::sqrt(unit(random));
    const double first = 1.0 - root;
    const double second = root * (1.0 - unit(random));
    const double third = 1.0 - first - second;
    const Vec3 target = triangle[0] * first + triangle[1] * second + triangle[2] * third;
    const double scale = std::max(
        1.0, std::max({length(triangle[1] - triangle[0]), length(triangle[2] - triangle[1]),
                       length(triangle[0] - triangle[2])}));
    rays.push_back({target + *unit_normal * scale, *unit_normal * -1.0, 0.0, scale * 4.0});
  }
  return rays;
}

OracleComparison compare_oracles(const CompiledAsset &asset, const Bvh4D &bvh,
                                 const std::vector<Vec3> &posed_cage_vertices,
                                 const std::vector<Ray> &rays) {
  OracleComparison result{};
  result.rays = rays.size();
  for (std::size_t ray_index = 0; ray_index < rays.size(); ++ray_index) {
    const auto &ray = rays[ray_index];
    const auto fast = trace_fast(asset, posed_cage_vertices, ray);
    const auto exact =
        trace_watertight4d(asset, bvh, posed_cage_vertices, ray, ProjectionMode::bounded_simplex);
    result.fast_duplicate_ownership += fast.duplicate_ownership;
    result.exact_duplicate_ownership += exact.duplicate_ownership;
    result.fast_raw_duplicate_candidates += fast.raw_duplicate_candidates;
    result.exact_raw_duplicate_candidates += exact.raw_duplicate_candidates;
    result.fast_visited_nodes += fast.visited_nodes;
    result.exact_visited_nodes += exact.visited_nodes;
    if (!fast.closest) {
      ++result.fast_misses;
    }
    if (!exact.closest) {
      ++result.exact_misses;
    }
    if (!fast.closest || !exact.closest) {
      if (result.failures.size() < 64U) {
        result.failures.push_back(
            {ray_index, ray, fast.closest.has_value(), exact.closest.has_value(),
             fast.closest ? std::optional<std::uint32_t>(fast.closest->source_primitive)
                          : std::nullopt,
             exact.closest ? std::optional<std::uint32_t>(exact.closest->source_primitive)
                           : std::nullopt});
      }
      continue;
    }
    if (fast.closest->source_primitive != exact.closest->source_primitive) {
      ++result.primitive_mismatches;
      if (result.failures.size() < 64U) {
        result.failures.push_back({ray_index, ray, true, true, fast.closest->source_primitive,
                                   exact.closest->source_primitive});
      }
    }
    result.max_position_error = std::max(result.max_position_error,
                                         length(fast.closest->position - exact.closest->position));
    result.max_attribute_error = std::max(
        {result.max_attribute_error,
         std::abs(fast.closest->source_barycentric.x - exact.closest->source_barycentric.x),
         std::abs(fast.closest->source_barycentric.y - exact.closest->source_barycentric.y),
         std::abs(fast.closest->source_barycentric.z - exact.closest->source_barycentric.z),
         std::abs(fast.closest->uv.x - exact.closest->uv.x),
         std::abs(fast.closest->uv.y - exact.closest->uv.y)});
  }
  return result;
}

ImageDifferential compare_rest_pose_image(const CompiledAsset &asset, const Bvh4D &bvh,
                                          std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U || asset.source.vertices.empty()) {
    throw std::invalid_argument("image differential requires nonzero dimensions and geometry");
  }
  Vec3 minimum{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
               std::numeric_limits<double>::infinity()};
  Vec3 maximum{-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
               -std::numeric_limits<double>::infinity()};
  for (const auto &vertex : asset.source.vertices) {
    minimum.x = std::min(minimum.x, vertex.position.x);
    minimum.y = std::min(minimum.y, vertex.position.y);
    minimum.z = std::min(minimum.z, vertex.position.z);
    maximum.x = std::max(maximum.x, vertex.position.x);
    maximum.y = std::max(maximum.y, vertex.position.y);
    maximum.z = std::max(maximum.z, vertex.position.z);
  }
  const double x_span = std::max(1.0e-9, maximum.x - minimum.x);
  const double y_span = std::max(1.0e-9, maximum.y - minimum.y);
  const double scene_span = std::max({x_span, y_span, std::max(1.0e-9, maximum.z - minimum.z)});
  const double x_min = minimum.x - x_span * 0.05;
  const double x_max = maximum.x + x_span * 0.05;
  const double y_min = minimum.y - y_span * 0.05;
  const double y_max = maximum.y + y_span * 0.05;
  const double origin_z = maximum.z + scene_span * 2.0;

  ImageDifferential result{};
  result.pixels = static_cast<std::uint64_t>(width) * height;
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const double u = (static_cast<double>(x) + 0.5) / static_cast<double>(width);
      const double v = (static_cast<double>(y) + 0.5) / static_cast<double>(height);
      const Ray ray{{x_min + (x_max - x_min) * u, y_min + (y_max - y_min) * v, origin_z},
                    {0.0, 0.0, -1.0},
                    0.0,
                    scene_span * 8.0};
      const auto dense = trace_dense(asset.source, ray);
      const auto fast = trace_fast(asset, asset.cage.vertices, ray);
      const auto exact =
          trace_watertight4d(asset, bvh, asset.cage.vertices, ray, ProjectionMode::bounded_simplex);
      result.dense_hits += dense.closest.has_value() ? 1U : 0U;
      result.fast_hits += fast.closest.has_value() ? 1U : 0U;
      result.exact_hits += exact.closest.has_value() ? 1U : 0U;
      if (dense.closest.has_value() != fast.closest.has_value() ||
          dense.closest.has_value() != exact.closest.has_value()) {
        ++result.hit_mismatches;
        continue;
      }
      if (!dense.closest) {
        continue;
      }
      for (const TraceHit *candidate : {&*fast.closest, &*exact.closest}) {
        if (candidate->source_primitive != dense.closest->source_primitive) {
          ++result.primitive_mismatches;
        }
        if (candidate->material != dense.closest->material) {
          ++result.material_mismatches;
        }
        result.max_position_error = std::max(result.max_position_error,
                                             length(candidate->position - dense.closest->position));
        result.max_normal_error =
            std::max(result.max_normal_error, length(candidate->normal - dense.closest->normal));
        result.max_uv_error =
            std::max({result.max_uv_error, std::abs(candidate->uv.x - dense.closest->uv.x),
                      std::abs(candidate->uv.y - dense.closest->uv.y)});
      }
    }
  }
  return result;
}

} // namespace tetcage
