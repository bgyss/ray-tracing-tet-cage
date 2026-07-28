#include "tetcage/asset_format.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace tetcage {
namespace {

constexpr std::array<char, 8> asset_magic{'T', 'E', 'T', 'C', 'A', 'G', 'E', '\0'};
constexpr std::uint64_t fnv_offset = 1469598103934665603ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::uint64_t maximum_serialized_elements = 100'000'000ULL;

struct FaceKey {
  std::array<std::uint64_t, 3> vertex_ids{};

  auto operator<=>(const FaceKey &) const = default;
};

struct VertexKey {
  std::uint32_t tet_id{};
  std::uint32_t source_primitive{};
  std::array<std::uint64_t, 3> source_bary_bits{};
  std::uint8_t cage_boundary_mask{};
  std::uint8_t source_boundary_mask{};

  auto operator<=>(const VertexKey &) const = default;
};

Tetrahedron make_tet(const Cage &cage, std::size_t index) {
  Tetrahedron result{};
  for (std::size_t corner = 0; corner < 4; ++corner) {
    const auto vertex_index = cage.tetrahedra[index].vertex_indices[corner];
    result.positions[corner] = cage.vertices[vertex_index];
    result.vertex_ids[corner] = cage.vertex_ids[vertex_index];
  }
  return result;
}

double snap_scalar(double value, double epsilon) {
  if (std::abs(value) <= epsilon) {
    return 0.0;
  }
  if (std::abs(value - 1.0) <= epsilon) {
    return 1.0;
  }
  return value;
}

Vec4 snap_barycentric(Vec4 barycentric, double epsilon) {
  barycentric.x = snap_scalar(barycentric.x, epsilon);
  barycentric.y = snap_scalar(barycentric.y, epsilon);
  barycentric.z = snap_scalar(barycentric.z, epsilon);
  barycentric.w = snap_scalar(barycentric.w, epsilon);
  const double sum = barycentric.x + barycentric.y + barycentric.z + barycentric.w;
  if (std::isfinite(sum) && std::abs(sum) > std::numeric_limits<double>::min() &&
      std::abs(sum - 1.0) <= epsilon * 8.0) {
    barycentric.x /= sum;
    barycentric.y /= sum;
    barycentric.z /= sum;
    barycentric.w /= sum;
  }
  return barycentric;
}

Vec3 snap_source_barycentric(Vec3 barycentric, double epsilon) {
  barycentric.x = snap_scalar(barycentric.x, epsilon);
  barycentric.y = snap_scalar(barycentric.y, epsilon);
  barycentric.z = snap_scalar(barycentric.z, epsilon);
  const double sum = barycentric.x + barycentric.y + barycentric.z;
  if (std::isfinite(sum) && std::abs(sum) > std::numeric_limits<double>::min()) {
    barycentric = barycentric / sum;
  }
  return barycentric;
}

std::uint8_t boundary_mask(Vec4 barycentric, double epsilon) {
  std::uint8_t result = 0;
  for (std::size_t coordinate = 0; coordinate < 4; ++coordinate) {
    if (std::abs(barycentric[coordinate]) <= epsilon) {
      result = static_cast<std::uint8_t>(result | (1U << coordinate));
    }
  }
  return result;
}

std::uint8_t source_mask(Vec3 barycentric, double epsilon) {
  std::uint8_t result = 0;
  if (std::abs(barycentric.x) <= epsilon) {
    result = static_cast<std::uint8_t>(result | 1U);
  }
  if (std::abs(barycentric.y) <= epsilon) {
    result = static_cast<std::uint8_t>(result | 2U);
  }
  if (std::abs(barycentric.z) <= epsilon) {
    result = static_cast<std::uint8_t>(result | 4U);
  }
  return result;
}

void hash_word(std::uint64_t &hash, std::uint64_t word) {
  for (unsigned int byte = 0; byte < 8; ++byte) {
    hash ^= (word >> (byte * 8U)) & 0xffU;
    hash *= fnv_prime;
  }
}

std::uint64_t stable_vertex_id(std::uint32_t primitive, Vec3 source_barycentric,
                               const Tetrahedron &tet, std::uint8_t cage_mask,
                               std::uint8_t source_boundary_mask) {
  std::uint64_t hash = fnv_offset;
  hash_word(hash, primitive);
  hash_word(hash, std::bit_cast<std::uint64_t>(source_barycentric.x));
  hash_word(hash, std::bit_cast<std::uint64_t>(source_barycentric.y));
  hash_word(hash, std::bit_cast<std::uint64_t>(source_barycentric.z));
  std::array<std::uint64_t, 4> feature_ids{};
  std::size_t feature_count = 0;
  for (std::size_t coordinate = 0; coordinate < 4; ++coordinate) {
    if ((cage_mask & (1U << coordinate)) == 0U) {
      feature_ids[feature_count++] = tet.vertex_ids[coordinate];
    }
  }
  std::sort(feature_ids.begin(), feature_ids.begin() + static_cast<std::ptrdiff_t>(feature_count));
  hash_word(hash, feature_count);
  for (std::size_t index = 0; index < feature_count; ++index) {
    hash_word(hash, feature_ids[index]);
  }
  hash_word(hash, source_boundary_mask);
  return hash == 0U ? 1U : hash;
}

FaceKey face_key(const Tetrahedron &tet, std::size_t opposite_corner) {
  FaceKey key{};
  std::size_t output = 0;
  for (std::size_t corner = 0; corner < 4; ++corner) {
    if (corner != opposite_corner) {
      key.vertex_ids[output++] = tet.vertex_ids[corner];
    }
  }
  std::sort(key.vertex_ids.begin(), key.vertex_ids.end());
  return key;
}

std::optional<std::size_t> coplanar_face(const std::vector<Vec4> &barycentrics, double epsilon) {
  for (std::size_t coordinate = 0; coordinate < 4; ++coordinate) {
    bool on_face = true;
    for (const auto &barycentric : barycentrics) {
      on_face = on_face && std::abs(barycentric[coordinate]) <= epsilon;
    }
    if (on_face) {
      return coordinate;
    }
  }
  return std::nullopt;
}

double polygon_scale(const std::vector<ClipVertex> &polygon) {
  double scale = 1.0;
  for (const auto &vertex : polygon) {
    scale = std::max(scale, length(vertex.position));
  }
  return scale;
}

template <typename T> void append_integral(std::vector<std::byte> &output, T value) {
  static_assert(std::is_integral_v<T>);
  using Unsigned = std::make_unsigned_t<T>;
  const auto unsigned_value = static_cast<Unsigned>(value);
  for (std::size_t byte = 0; byte < sizeof(T); ++byte) {
    output.push_back(
        static_cast<std::byte>((unsigned_value >> (byte * 8U)) & static_cast<Unsigned>(0xffU)));
  }
}

void append_double(std::vector<std::byte> &output, double value) {
  append_integral(output, std::bit_cast<std::uint64_t>(value));
}

template <typename T>
std::optional<T> read_integral(const std::vector<std::byte> &input, std::size_t &offset) {
  static_assert(std::is_integral_v<T>);
  if (offset > input.size() || input.size() - offset < sizeof(T)) {
    return std::nullopt;
  }
  using Unsigned = std::make_unsigned_t<T>;
  Unsigned value = 0;
  for (std::size_t byte = 0; byte < sizeof(T); ++byte) {
    value |= static_cast<Unsigned>(std::to_integer<unsigned int>(input[offset + byte]))
             << (byte * 8U);
  }
  offset += sizeof(T);
  return static_cast<T>(value);
}

std::optional<double> read_double(const std::vector<std::byte> &input, std::size_t &offset) {
  const auto bits = read_integral<std::uint64_t>(input, offset);
  if (!bits) {
    return std::nullopt;
  }
  return std::bit_cast<double>(*bits);
}

void append_vec2(std::vector<std::byte> &output, Vec2 value) {
  append_double(output, value.x);
  append_double(output, value.y);
}

void append_vec3(std::vector<std::byte> &output, Vec3 value) {
  append_double(output, value.x);
  append_double(output, value.y);
  append_double(output, value.z);
}

void append_vec4(std::vector<std::byte> &output, Vec4 value) {
  append_double(output, value.x);
  append_double(output, value.y);
  append_double(output, value.z);
  append_double(output, value.w);
}

std::optional<Vec2> read_vec2(const std::vector<std::byte> &input, std::size_t &offset) {
  const auto x = read_double(input, offset);
  const auto y = read_double(input, offset);
  if (!x || !y) {
    return std::nullopt;
  }
  return Vec2{*x, *y};
}

std::optional<Vec3> read_vec3(const std::vector<std::byte> &input, std::size_t &offset) {
  const auto x = read_double(input, offset);
  const auto y = read_double(input, offset);
  const auto z = read_double(input, offset);
  if (!x || !y || !z) {
    return std::nullopt;
  }
  return Vec3{*x, *y, *z};
}

std::optional<Vec4> read_vec4(const std::vector<std::byte> &input, std::size_t &offset) {
  const auto x = read_double(input, offset);
  const auto y = read_double(input, offset);
  const auto z = read_double(input, offset);
  const auto w = read_double(input, offset);
  if (!x || !y || !z || !w) {
    return std::nullopt;
  }
  return Vec4{*x, *y, *z, *w};
}

bool count_is_safe(std::uint64_t count) { return count <= maximum_serialized_elements; }

template <typename T>
bool read_count(const std::vector<std::byte> &bytes, std::size_t &offset, T &destination) {
  const auto value = read_integral<std::uint64_t>(bytes, offset);
  if (!value || !count_is_safe(*value)) {
    return false;
  }
  destination.resize(static_cast<std::size_t>(*value));
  return true;
}

} // namespace

CompileResult compile_asset(const SourceMesh &mesh, const Cage &cage,
                            const TolerancePolicy &tolerance) {
  CompileResult result{};
  if (tolerance.version != 1U || !std::isfinite(tolerance.expanded_barycentric_epsilon) ||
      tolerance.expanded_barycentric_epsilon < 0.0 ||
      !std::isfinite(tolerance.feature_snap_epsilon) || tolerance.feature_snap_epsilon <= 0.0) {
    result.diagnostics.push_back(
        {"invalid_tolerance_policy", "Tolerance policy values are invalid.", 0U, 0U});
    return result;
  }
  if (cage.vertex_ids.size() != cage.vertices.size()) {
    result.diagnostics.push_back({"invalid_cage_vertex_ids",
                                  "Cage vertex positions and stable IDs differ in count.", 0U, 0U});
    return result;
  }
  {
    std::set<std::uint64_t> stable_ids(cage.vertex_ids.begin(), cage.vertex_ids.end());
    if (stable_ids.size() != cage.vertex_ids.size()) {
      result.diagnostics.push_back(
          {"duplicate_cage_vertex_id", "Cage stable vertex IDs must be globally unique.", 0U, 0U});
      return result;
    }
  }
  if (mesh.vertices.empty() || mesh.triangles.empty() || cage.tetrahedra.empty()) {
    result.diagnostics.push_back(
        {"empty_input", "Mesh and cage must contain vertices and primitives.", 0U, 0U});
    return result;
  }

  std::vector<Tetrahedron> tets;
  tets.reserve(cage.tetrahedra.size());
  std::map<FaceKey, std::vector<std::uint32_t>> face_owners;
  std::set<std::array<std::uint64_t, 4>> unique_tets;
  std::vector<CompiledTetMetadata> tet_metadata;
  tet_metadata.reserve(cage.tetrahedra.size());
  double worst_condition = 0.0;
  for (std::size_t tet_index = 0; tet_index < cage.tetrahedra.size(); ++tet_index) {
    std::set<std::uint32_t> unique_indices;
    for (const auto vertex_index : cage.tetrahedra[tet_index].vertex_indices) {
      if (vertex_index >= cage.vertices.size()) {
        result.diagnostics.push_back({"invalid_cage_index", "A tetrahedron index is out of range.",
                                      0U, static_cast<std::uint32_t>(tet_index)});
        return result;
      }
      unique_indices.insert(vertex_index);
    }
    if (unique_indices.size() != 4U) {
      result.diagnostics.push_back({"duplicate_cage_corner",
                                    "A tetrahedron must reference four distinct vertices.", 0U,
                                    static_cast<std::uint32_t>(tet_index)});
      return result;
    }
    tets.push_back(make_tet(cage, tet_index));
    auto canonical_tet_ids = tets.back().vertex_ids;
    std::sort(canonical_tet_ids.begin(), canonical_tet_ids.end());
    if (!unique_tets.insert(canonical_tet_ids).second) {
      result.diagnostics.push_back({"duplicate_cage_tet",
                                    "Two cage tetrahedra reference the same four stable vertices.",
                                    0U, static_cast<std::uint32_t>(tet_index)});
      return result;
    }
    const auto diagnostics = diagnose(tets.back());
    if (diagnostics.classification == TetClass::near_singular) {
      result.diagnostics.push_back({"degenerate_cage_tet",
                                    "A cage tetrahedron is singular or too poorly conditioned.", 0U,
                                    static_cast<std::uint32_t>(tet_index)});
      return result;
    }
    worst_condition = std::max(worst_condition, diagnostics.condition_estimate);
    CompiledTetMetadata metadata{};
    metadata.determinant = diagnostics.determinant;
    metadata.condition_estimate = diagnostics.condition_estimate;
    metadata.minimum_edge = diagnostics.minimum_edge;
    metadata.mirrored = diagnostics.classification == TetClass::mirrored;
    metadata.near_singular = diagnostics.classification == TetClass::near_singular;
    tet_metadata.push_back(metadata);
    for (std::size_t face = 0; face < 4; ++face) {
      face_owners[face_key(tets.back(), face)].push_back(static_cast<std::uint32_t>(tet_index));
    }
  }
  for (auto &[key, owners] : face_owners) {
    static_cast<void>(key);
    std::sort(owners.begin(), owners.end());
    if (owners.size() > 2U) {
      result.diagnostics.push_back({"nonmanifold_cage_face",
                                    "More than two tetrahedra share one canonical cage face.", 0U,
                                    owners.front()});
    }
  }
  if (!result.diagnostics.empty()) {
    return result;
  }
  for (std::size_t tet_index = 0; tet_index < tets.size(); ++tet_index) {
    for (std::size_t face = 0; face < 4U; ++face) {
      const auto &owners = face_owners[face_key(tets[tet_index], face)];
      tet_metadata[tet_index].face_owner[face] = owners.front();
      if (owners.size() == 2U) {
        tet_metadata[tet_index].adjacent_tet[face] =
            static_cast<std::int32_t>(owners[0] == tet_index ? owners[1] : owners[0]);
      }
    }
  }

  CompiledAsset asset{};
  asset.tolerance = tolerance;
  asset.source = mesh;
  asset.cage = cage;
  asset.tet_metadata = std::move(tet_metadata);
  asset.statistics.source_triangles = mesh.triangles.size();
  asset.statistics.worst_condition = worst_condition;
  std::set<std::uint32_t> occupied_tets;
  std::map<VertexKey, std::uint32_t> generated_vertex_indices;

  for (std::size_t source_triangle_index = 0; source_triangle_index < mesh.triangles.size();
       ++source_triangle_index) {
    const auto &source_triangle = mesh.triangles[source_triangle_index];
    bool covered = false;
    for (const auto vertex_index : source_triangle.vertex_indices) {
      if (vertex_index >= mesh.vertices.size()) {
        result.diagnostics.push_back({"invalid_source_index",
                                      "A source triangle index is out of range.",
                                      source_triangle.primitive_id, 0U});
        return result;
      }
    }
    std::array<ClipVertex, 3> triangle{{
        {mesh.vertices[source_triangle.vertex_indices[0]].position, {1.0, 0.0, 0.0}, {}},
        {mesh.vertices[source_triangle.vertex_indices[1]].position, {0.0, 1.0, 0.0}, {}},
        {mesh.vertices[source_triangle.vertex_indices[2]].position, {0.0, 0.0, 1.0}, {}},
    }};

    for (std::size_t tet_index = 0; tet_index < tets.size(); ++tet_index) {
      const auto polygon = clip_triangle_to_tetrahedron(triangle, tets[tet_index],
                                                        tolerance.expanded_barycentric_epsilon);
      if (polygon.size() < 3U) {
        continue;
      }
      std::vector<Vec4> cage_barycentrics;
      cage_barycentrics.reserve(polygon.size());
      bool valid_polygon = true;
      for (const auto &vertex : polygon) {
        const auto barycentric =
            to_barycentric(tets[tet_index], vertex.position, tolerance.feature_snap_epsilon);
        if (!barycentric) {
          valid_polygon = false;
          break;
        }
        cage_barycentrics.push_back(snap_barycentric(*barycentric, tolerance.feature_snap_epsilon));
      }
      if (!valid_polygon) {
        continue;
      }

      const auto face = coplanar_face(cage_barycentrics, tolerance.feature_snap_epsilon * 8.0);
      std::uint32_t owner_tet = static_cast<std::uint32_t>(tet_index);
      if (face) {
        const auto &owners = face_owners[face_key(tets[tet_index], *face)];
        owner_tet = owners.front();
        if (owner_tet != tet_index) {
          covered = true;
          continue;
        }
      }

      std::uint32_t emitted_for_polygon = 0;
      for (std::size_t fan_index = 1; fan_index + 1 < polygon.size(); ++fan_index) {
        const std::array<std::size_t, 3> polygon_indices{0U, fan_index, fan_index + 1U};
        const Vec3 a = polygon[polygon_indices[0]].position;
        const Vec3 b = polygon[polygon_indices[1]].position;
        const Vec3 c = polygon[polygon_indices[2]].position;
        const double twice_area = length(cross(b - a, c - a));
        const double area_threshold =
            tolerance.minimum_area_relative * polygon_scale(polygon) * polygon_scale(polygon);
        if (!std::isfinite(twice_area) || twice_area <= area_threshold) {
          continue;
        }

        MicroTriangle fragment{};
        fragment.tet_id = static_cast<std::uint32_t>(tet_index);
        fragment.owner_tet = owner_tet;
        fragment.source_primitive = source_triangle.primitive_id;
        fragment.material = source_triangle.material_id;
        fragment.deterministic_subtriangle = emitted_for_polygon++;
        for (std::size_t corner = 0; corner < 3; ++corner) {
          const auto polygon_index = polygon_indices[corner];
          const auto source_barycentric = snap_source_barycentric(
              polygon[polygon_index].source_bary, tolerance.feature_snap_epsilon);
          const auto cage_barycentric = cage_barycentrics[polygon_index];
          const auto cage_boundary =
              boundary_mask(cage_barycentric, tolerance.feature_snap_epsilon * 8.0);
          const auto source_boundary =
              source_mask(source_barycentric, tolerance.feature_snap_epsilon * 8.0);
          const VertexKey key{
              static_cast<std::uint32_t>(tet_index),
              source_triangle.primitive_id,
              {std::bit_cast<std::uint64_t>(source_barycentric.x),
               std::bit_cast<std::uint64_t>(source_barycentric.y),
               std::bit_cast<std::uint64_t>(source_barycentric.z)},
              cage_boundary,
              source_boundary,
          };
          auto found = generated_vertex_indices.find(key);
          if (found == generated_vertex_indices.end()) {
            const auto generated_index =
                static_cast<std::uint32_t>(asset.generated_vertices.size());
            GeneratedVertex generated{};
            generated.cage_barycentric = cage_barycentric;
            generated.source_barycentric = source_barycentric;
            generated.feature = {cage_boundary, source_boundary};
            generated.stable_id = stable_vertex_id(source_triangle.primitive_id, source_barycentric,
                                                   tets[tet_index], cage_boundary, source_boundary);
            generated.tet_id = static_cast<std::uint32_t>(tet_index);
            generated.source_primitive = source_triangle.primitive_id;
            asset.generated_vertices.push_back(generated);
            found = generated_vertex_indices.emplace(key, generated_index).first;
          }
          fragment.vertex_indices[corner] = found->second;
        }
        asset.micro_triangles.push_back(fragment);
        covered = true;
        occupied_tets.insert(static_cast<std::uint32_t>(tet_index));
        if (face) {
          ++asset.statistics.boundary_fragments;
        }
      }
    }
    if (!covered) {
      result.diagnostics.push_back({"uncovered_source_triangle",
                                    "Source primitive " +
                                        std::to_string(source_triangle.primitive_id) +
                                        " has no nondegenerate fragment inside the cage.",
                                    source_triangle.primitive_id, 0U});
    }
  }

  if (!result.diagnostics.empty()) {
    return result;
  }
  asset.statistics.generated_triangles = asset.micro_triangles.size();
  asset.statistics.generated_vertices = asset.generated_vertices.size();
  asset.statistics.occupied_tetrahedra = occupied_tets.size();
  asset.statistics.canonical_bytes =
      asset.generated_vertices.size() * (sizeof(Vec4) + sizeof(std::uint32_t));
  asset.statistics.provenance_bytes =
      asset.generated_vertices.size() * (sizeof(Vec3) + sizeof(std::uint64_t)) +
      asset.micro_triangles.size() * (4U * sizeof(std::uint32_t));
  asset.statistics.triangle_expansion = static_cast<double>(asset.micro_triangles.size()) /
                                        static_cast<double>(mesh.triangles.size());
  asset.statistics.vertex_expansion = static_cast<double>(asset.generated_vertices.size()) /
                                      static_cast<double>(mesh.vertices.size());
  result.asset = std::move(asset);
  return result;
}

std::vector<std::byte> serialize_asset(const CompiledAsset &asset) {
  std::vector<std::byte> output;
  output.reserve(256U + asset.source.vertices.size() * 64U + asset.generated_vertices.size() * 96U);
  for (const char character : asset_magic) {
    output.push_back(static_cast<std::byte>(character));
  }
  append_integral(output, asset.format_version);
  append_integral(output, asset.tolerance.version);
  append_double(output, asset.tolerance.expanded_barycentric_epsilon);
  append_double(output, asset.tolerance.feature_snap_epsilon);
  append_double(output, asset.tolerance.minimum_area_relative);

  append_integral(output, static_cast<std::uint64_t>(asset.source.vertices.size()));
  for (const auto &vertex : asset.source.vertices) {
    append_vec3(output, vertex.position);
    append_vec3(output, vertex.normal);
    append_vec2(output, vertex.uv);
  }
  append_integral(output, static_cast<std::uint64_t>(asset.source.triangles.size()));
  for (const auto &triangle : asset.source.triangles) {
    for (const auto index : triangle.vertex_indices) {
      append_integral(output, index);
    }
    append_integral(output, triangle.primitive_id);
    append_integral(output, triangle.material_id);
  }

  append_integral(output, static_cast<std::uint64_t>(asset.cage.vertices.size()));
  for (std::size_t index = 0; index < asset.cage.vertices.size(); ++index) {
    append_vec3(output, asset.cage.vertices[index]);
    append_integral(output, asset.cage.vertex_ids[index]);
  }
  append_integral(output, static_cast<std::uint64_t>(asset.cage.tetrahedra.size()));
  for (const auto &tet : asset.cage.tetrahedra) {
    for (const auto index : tet.vertex_indices) {
      append_integral(output, index);
    }
  }
  append_integral(output, static_cast<std::uint64_t>(asset.tet_metadata.size()));
  for (const auto &metadata : asset.tet_metadata) {
    append_double(output, metadata.determinant);
    append_double(output, metadata.condition_estimate);
    append_double(output, metadata.minimum_edge);
    append_integral(output, static_cast<std::uint8_t>(metadata.mirrored ? 1U : 0U));
    append_integral(output, static_cast<std::uint8_t>(metadata.near_singular ? 1U : 0U));
    for (const auto adjacent : metadata.adjacent_tet) {
      append_integral(output, adjacent);
    }
    for (const auto owner : metadata.face_owner) {
      append_integral(output, owner);
    }
  }

  append_integral(output, static_cast<std::uint64_t>(asset.generated_vertices.size()));
  for (const auto &vertex : asset.generated_vertices) {
    append_vec4(output, vertex.cage_barycentric);
    append_vec3(output, vertex.source_barycentric);
    append_integral(output, vertex.feature.cage_boundary_mask);
    append_integral(output, vertex.feature.source_boundary_mask);
    append_integral(output, vertex.stable_id);
    append_integral(output, vertex.tet_id);
    append_integral(output, vertex.source_primitive);
  }
  append_integral(output, static_cast<std::uint64_t>(asset.micro_triangles.size()));
  for (const auto &triangle : asset.micro_triangles) {
    for (const auto index : triangle.vertex_indices) {
      append_integral(output, index);
    }
    append_integral(output, triangle.tet_id);
    append_integral(output, triangle.owner_tet);
    append_integral(output, triangle.source_primitive);
    append_integral(output, triangle.material);
    append_integral(output, triangle.deterministic_subtriangle);
  }
  append_integral(output, asset.statistics.source_triangles);
  append_integral(output, asset.statistics.generated_triangles);
  append_integral(output, asset.statistics.generated_vertices);
  append_integral(output, asset.statistics.occupied_tetrahedra);
  append_integral(output, asset.statistics.boundary_fragments);
  append_integral(output, asset.statistics.canonical_bytes);
  append_integral(output, asset.statistics.provenance_bytes);
  append_double(output, asset.statistics.triangle_expansion);
  append_double(output, asset.statistics.vertex_expansion);
  append_double(output, asset.statistics.worst_condition);
  return output;
}

DeserializeResult deserialize_asset(const std::vector<std::byte> &bytes) {
  DeserializeResult result{};
  if (bytes.size() < asset_magic.size() + sizeof(std::uint32_t)) {
    result.error = "asset is truncated";
    return result;
  }
  for (std::size_t index = 0; index < asset_magic.size(); ++index) {
    if (bytes[index] != static_cast<std::byte>(asset_magic[index])) {
      result.error = "asset magic is invalid";
      return result;
    }
  }
  std::size_t offset = asset_magic.size();
  CompiledAsset asset{};
  const auto version = read_integral<std::uint32_t>(bytes, offset);
  const auto tolerance_version = read_integral<std::uint32_t>(bytes, offset);
  const auto expansion = read_double(bytes, offset);
  const auto snap = read_double(bytes, offset);
  const auto minimum_area = read_double(bytes, offset);
  if (!version || *version != asset_format_version || !tolerance_version || !expansion || !snap ||
      !minimum_area) {
    result.error = "asset header is invalid or uses an unsupported version";
    return result;
  }
  asset.format_version = *version;
  asset.tolerance = {*tolerance_version, *expansion, *snap, *minimum_area};

  if (!read_count(bytes, offset, asset.source.vertices)) {
    result.error = "source vertex count is invalid";
    return result;
  }
  for (auto &vertex : asset.source.vertices) {
    const auto position = read_vec3(bytes, offset);
    const auto normal = read_vec3(bytes, offset);
    const auto uv = read_vec2(bytes, offset);
    if (!position || !normal || !uv) {
      result.error = "source vertex stream is truncated";
      return result;
    }
    vertex = {*position, *normal, *uv};
  }
  if (!read_count(bytes, offset, asset.source.triangles)) {
    result.error = "source triangle count is invalid";
    return result;
  }
  for (auto &triangle : asset.source.triangles) {
    for (auto &index : triangle.vertex_indices) {
      const auto value = read_integral<std::uint32_t>(bytes, offset);
      if (!value) {
        result.error = "source triangle stream is truncated";
        return result;
      }
      index = *value;
    }
    const auto primitive = read_integral<std::uint32_t>(bytes, offset);
    const auto material = read_integral<std::uint32_t>(bytes, offset);
    if (!primitive || !material) {
      result.error = "source triangle metadata is truncated";
      return result;
    }
    triangle.primitive_id = *primitive;
    triangle.material_id = *material;
  }

  if (!read_count(bytes, offset, asset.cage.vertices)) {
    result.error = "cage vertex count is invalid";
    return result;
  }
  asset.cage.vertex_ids.resize(asset.cage.vertices.size());
  for (std::size_t index = 0; index < asset.cage.vertices.size(); ++index) {
    const auto position = read_vec3(bytes, offset);
    const auto id = read_integral<std::uint64_t>(bytes, offset);
    if (!position || !id) {
      result.error = "cage vertex stream is truncated";
      return result;
    }
    asset.cage.vertices[index] = *position;
    asset.cage.vertex_ids[index] = *id;
  }
  if (!read_count(bytes, offset, asset.cage.tetrahedra)) {
    result.error = "cage tetrahedron count is invalid";
    return result;
  }
  for (auto &tet : asset.cage.tetrahedra) {
    for (auto &index : tet.vertex_indices) {
      const auto value = read_integral<std::uint32_t>(bytes, offset);
      if (!value) {
        result.error = "cage tetrahedron stream is truncated";
        return result;
      }
      index = *value;
    }
  }
  if (!read_count(bytes, offset, asset.tet_metadata)) {
    result.error = "tet metadata count is invalid";
    return result;
  }
  for (auto &metadata : asset.tet_metadata) {
    const auto determinant = read_double(bytes, offset);
    const auto condition = read_double(bytes, offset);
    const auto minimum_edge = read_double(bytes, offset);
    const auto mirrored = read_integral<std::uint8_t>(bytes, offset);
    const auto near_singular = read_integral<std::uint8_t>(bytes, offset);
    if (!determinant || !condition || !minimum_edge || !mirrored || !near_singular) {
      result.error = "tet metadata stream is truncated";
      return result;
    }
    metadata.determinant = *determinant;
    metadata.condition_estimate = *condition;
    metadata.minimum_edge = *minimum_edge;
    metadata.mirrored = *mirrored != 0U;
    metadata.near_singular = *near_singular != 0U;
    for (auto &adjacent : metadata.adjacent_tet) {
      const auto value = read_integral<std::int32_t>(bytes, offset);
      if (!value) {
        result.error = "tet adjacency stream is truncated";
        return result;
      }
      adjacent = *value;
    }
    for (auto &owner : metadata.face_owner) {
      const auto value = read_integral<std::uint32_t>(bytes, offset);
      if (!value) {
        result.error = "tet face ownership stream is truncated";
        return result;
      }
      owner = *value;
    }
  }

  if (!read_count(bytes, offset, asset.generated_vertices)) {
    result.error = "generated vertex count is invalid";
    return result;
  }
  for (auto &vertex : asset.generated_vertices) {
    const auto cage_bary = read_vec4(bytes, offset);
    const auto source_bary = read_vec3(bytes, offset);
    const auto cage_mask = read_integral<std::uint8_t>(bytes, offset);
    const auto source_boundary = read_integral<std::uint8_t>(bytes, offset);
    const auto stable_id = read_integral<std::uint64_t>(bytes, offset);
    const auto tet_id = read_integral<std::uint32_t>(bytes, offset);
    const auto primitive = read_integral<std::uint32_t>(bytes, offset);
    if (!cage_bary || !source_bary || !cage_mask || !source_boundary || !stable_id || !tet_id ||
        !primitive) {
      result.error = "generated vertex stream is truncated";
      return result;
    }
    vertex = {*cage_bary, *source_bary, {*cage_mask, *source_boundary},
              *stable_id, *tet_id,      *primitive};
  }
  if (!read_count(bytes, offset, asset.micro_triangles)) {
    result.error = "micro-triangle count is invalid";
    return result;
  }
  for (auto &triangle : asset.micro_triangles) {
    for (auto &index : triangle.vertex_indices) {
      const auto value = read_integral<std::uint32_t>(bytes, offset);
      if (!value) {
        result.error = "micro-triangle stream is truncated";
        return result;
      }
      index = *value;
    }
    const auto tet_id = read_integral<std::uint32_t>(bytes, offset);
    const auto owner = read_integral<std::uint32_t>(bytes, offset);
    const auto primitive = read_integral<std::uint32_t>(bytes, offset);
    const auto material = read_integral<std::uint32_t>(bytes, offset);
    const auto subtriangle = read_integral<std::uint32_t>(bytes, offset);
    if (!tet_id || !owner || !primitive || !material || !subtriangle) {
      result.error = "micro-triangle metadata is truncated";
      return result;
    }
    triangle.tet_id = *tet_id;
    triangle.owner_tet = *owner;
    triangle.source_primitive = *primitive;
    triangle.material = *material;
    triangle.deterministic_subtriangle = *subtriangle;
  }
  const auto source_triangles = read_integral<std::uint64_t>(bytes, offset);
  const auto generated_triangles = read_integral<std::uint64_t>(bytes, offset);
  const auto generated_vertices = read_integral<std::uint64_t>(bytes, offset);
  const auto occupied_tetrahedra = read_integral<std::uint64_t>(bytes, offset);
  const auto boundary_fragments = read_integral<std::uint64_t>(bytes, offset);
  const auto canonical_bytes = read_integral<std::uint64_t>(bytes, offset);
  const auto provenance_bytes = read_integral<std::uint64_t>(bytes, offset);
  const auto triangle_expansion = read_double(bytes, offset);
  const auto vertex_expansion = read_double(bytes, offset);
  const auto worst_condition = read_double(bytes, offset);
  if (!source_triangles || !generated_triangles || !generated_vertices || !occupied_tetrahedra ||
      !boundary_fragments || !canonical_bytes || !provenance_bytes || !triangle_expansion ||
      !vertex_expansion || !worst_condition) {
    result.error = "asset statistics stream is truncated";
    return result;
  }
  asset.statistics = {*source_triangles,    *generated_triangles, *generated_vertices,
                      *occupied_tetrahedra, *boundary_fragments,  *canonical_bytes,
                      *provenance_bytes,    *triangle_expansion,  *vertex_expansion,
                      *worst_condition};
  if (offset != bytes.size()) {
    result.error = "asset has unrecognized trailing bytes";
    return result;
  }
  if (asset.statistics.source_triangles != asset.source.triangles.size() ||
      asset.statistics.generated_triangles != asset.micro_triangles.size() ||
      asset.statistics.generated_vertices != asset.generated_vertices.size() ||
      asset.statistics.occupied_tetrahedra > asset.cage.tetrahedra.size() ||
      asset.tet_metadata.size() != asset.cage.tetrahedra.size()) {
    result.error = "asset statistics contradict serialized stream counts";
    return result;
  }
  result.asset = std::move(asset);
  return result;
}

std::uint64_t asset_checksum(const std::vector<std::byte> &bytes) {
  std::uint64_t hash = fnv_offset;
  for (const auto value : bytes) {
    hash ^= std::to_integer<unsigned int>(value);
    hash *= fnv_prime;
  }
  return hash;
}

std::string inspect_asset_json(const CompiledAsset &asset) {
  std::ostringstream output;
  output << std::setprecision(17);
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"asset_format_version\": " << asset.format_version << ",\n"
         << "  \"source_triangles\": " << asset.source.triangles.size() << ",\n"
         << "  \"generated_triangles\": " << asset.micro_triangles.size() << ",\n"
         << "  \"generated_vertices\": " << asset.generated_vertices.size() << ",\n"
         << "  \"cage_tetrahedra\": " << asset.cage.tetrahedra.size() << ",\n"
         << "  \"occupied_tetrahedra\": " << asset.statistics.occupied_tetrahedra << ",\n"
         << "  \"boundary_fragments\": " << asset.statistics.boundary_fragments << ",\n"
         << "  \"tolerance_policy\": {\n"
         << "    \"version\": " << asset.tolerance.version << ",\n"
         << "    \"expanded_barycentric_epsilon\": " << asset.tolerance.expanded_barycentric_epsilon
         << ",\n"
         << "    \"feature_snap_epsilon\": " << asset.tolerance.feature_snap_epsilon << "\n"
         << "  }\n"
         << "}\n";
  return output.str();
}

} // namespace tetcage
