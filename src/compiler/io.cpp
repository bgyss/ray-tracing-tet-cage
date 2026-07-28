#include "tetcage/io.h"

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace tetcage {
namespace {

struct ObjIndex {
  int position{};
  int uv{};
  int normal{};

  auto operator<=>(const ObjIndex &) const = default;
};

bool parse_integer(const std::string &text, int &value) {
  const auto *begin = text.data();
  const auto *end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_unsigned(const std::string &text, std::uint64_t &value) {
  const auto *begin = text.data();
  const auto *end = begin + text.size();
  const auto parsed = std::from_chars(begin, end, value);
  return parsed.ec == std::errc{} && parsed.ptr == end;
}

bool parse_obj_index(const std::string &token, ObjIndex &index) {
  std::array<std::string, 3> parts{};
  std::size_t part = 0;
  std::size_t start = 0;
  while (part < parts.size()) {
    const auto slash = token.find('/', start);
    parts[part++] = token.substr(start, slash == std::string::npos ? slash : slash - start);
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1U;
  }
  if (parts[0].empty() || !parse_integer(parts[0], index.position)) {
    return false;
  }
  if (!parts[1].empty() && !parse_integer(parts[1], index.uv)) {
    return false;
  }
  if (!parts[2].empty() && !parse_integer(parts[2], index.normal)) {
    return false;
  }
  return index.position > 0 && index.uv >= 0 && index.normal >= 0;
}

std::uint32_t material_id(const std::string &name) {
  std::uint64_t numeric = 0;
  if (parse_unsigned(name, numeric) && numeric <= UINT32_MAX) {
    return static_cast<std::uint32_t>(numeric);
  }
  std::uint32_t hash = 2166136261U;
  for (const auto character : name) {
    hash ^= static_cast<unsigned char>(character);
    hash *= 16777619U;
  }
  return hash;
}

template <typename T> LoadResult<T> file_error(const std::string &path, const std::string &reason) {
  return {std::nullopt, path + ": " + reason};
}

std::string line_error(const std::string &path, std::size_t line, const std::string &reason) {
  return path + ": line " + std::to_string(line) + ": " + reason;
}

} // namespace

LoadResult<SourceMesh> load_obj(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    return file_error<SourceMesh>(path, "cannot open OBJ file");
  }
  std::vector<Vec3> positions;
  std::vector<Vec2> uvs;
  std::vector<Vec3> normals;
  SourceMesh mesh{};
  std::map<ObjIndex, std::uint32_t> generated_indices;
  std::uint32_t current_material = 0;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    std::istringstream fields(line);
    std::string kind;
    fields >> kind;
    if (kind.empty() || kind[0] == '#') {
      continue;
    }
    if (kind == "v") {
      Vec3 value{};
      if (!(fields >> value.x >> value.y >> value.z)) {
        return {std::nullopt, line_error(path, line_number, "invalid vertex position")};
      }
      positions.push_back(value);
    } else if (kind == "vt") {
      Vec2 value{};
      if (!(fields >> value.x >> value.y)) {
        return {std::nullopt, line_error(path, line_number, "invalid texture coordinate")};
      }
      uvs.push_back(value);
    } else if (kind == "vn") {
      Vec3 value{};
      if (!(fields >> value.x >> value.y >> value.z)) {
        return {std::nullopt, line_error(path, line_number, "invalid normal")};
      }
      normals.push_back(value);
    } else if (kind == "usemtl") {
      std::string name;
      if (!(fields >> name)) {
        return {std::nullopt, line_error(path, line_number, "missing material name")};
      }
      current_material = material_id(name);
    } else if (kind == "f") {
      std::array<ObjIndex, 3> indices{};
      std::string token;
      for (auto &index : indices) {
        if (!(fields >> token) || !parse_obj_index(token, index)) {
          return {std::nullopt,
                  line_error(path, line_number,
                             "faces must be positive-index triangles in v/vt/vn form")};
        }
      }
      if (fields >> token) {
        return {std::nullopt,
                line_error(path, line_number, "non-triangle OBJ faces are not accepted")};
      }
      SourceTriangle triangle{};
      triangle.primitive_id = static_cast<std::uint32_t>(mesh.triangles.size());
      triangle.material_id = current_material;
      for (std::size_t corner = 0; corner < indices.size(); ++corner) {
        const auto &index = indices[corner];
        const auto position_index = static_cast<std::size_t>(index.position - 1);
        const auto uv_index = index.uv > 0 ? static_cast<std::size_t>(index.uv - 1) : 0U;
        const auto normal_index =
            index.normal > 0 ? static_cast<std::size_t>(index.normal - 1) : 0U;
        if (position_index >= positions.size() || (index.uv > 0 && uv_index >= uvs.size()) ||
            (index.normal > 0 && normal_index >= normals.size())) {
          return {std::nullopt, line_error(path, line_number, "OBJ index is out of range")};
        }
        auto found = generated_indices.find(index);
        if (found == generated_indices.end()) {
          SourceVertex vertex{};
          vertex.position = positions[position_index];
          vertex.uv = index.uv > 0 ? uvs[uv_index] : Vec2{};
          vertex.normal = index.normal > 0 ? normals[normal_index] : Vec3{};
          const auto generated_index = static_cast<std::uint32_t>(mesh.vertices.size());
          mesh.vertices.push_back(vertex);
          found = generated_indices.emplace(index, generated_index).first;
        }
        triangle.vertex_indices[corner] = found->second;
      }
      mesh.triangles.push_back(triangle);
    }
  }
  if (mesh.vertices.empty() || mesh.triangles.empty()) {
    return file_error<SourceMesh>(path, "OBJ contains no triangle geometry");
  }
  return {std::move(mesh), {}};
}

LoadResult<Cage> load_tet_cage(const std::string &path) {
  std::ifstream input(path);
  if (!input) {
    return file_error<Cage>(path, "cannot open cage file");
  }
  Cage cage{};
  std::map<std::uint64_t, std::uint32_t> id_to_index;
  std::string line;
  std::size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    std::istringstream fields(line);
    std::string kind;
    fields >> kind;
    if (kind.empty() || kind[0] == '#') {
      continue;
    }
    if (kind == "v") {
      std::uint64_t id = 0;
      Vec3 position{};
      if (!(fields >> id >> position.x >> position.y >> position.z)) {
        return {std::nullopt, line_error(path, line_number, "invalid cage vertex")};
      }
      if (id_to_index.contains(id)) {
        return {std::nullopt, line_error(path, line_number, "duplicate stable vertex ID")};
      }
      const auto index = static_cast<std::uint32_t>(cage.vertices.size());
      cage.vertices.push_back(position);
      cage.vertex_ids.push_back(id);
      id_to_index.emplace(id, index);
    } else if (kind == "t") {
      CageTet tet{};
      for (auto &index : tet.vertex_indices) {
        std::uint64_t id = 0;
        if (!(fields >> id)) {
          return {std::nullopt,
                  line_error(path, line_number, "tet requires four stable vertex IDs")};
        }
        const auto found = id_to_index.find(id);
        if (found == id_to_index.end()) {
          return {std::nullopt,
                  line_error(path, line_number, "tet references an unknown stable vertex ID")};
        }
        index = found->second;
      }
      cage.tetrahedra.push_back(tet);
    } else {
      return {std::nullopt, line_error(path, line_number, "unknown cage record '" + kind + "'")};
    }
  }
  if (cage.vertices.empty() || cage.tetrahedra.empty()) {
    return file_error<Cage>(path, "cage contains no tetrahedra");
  }
  return {std::move(cage), {}};
}

LoadResult<CompiledAsset> load_asset_file(const std::string &path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return file_error<CompiledAsset>(path, "cannot open compiled asset");
  }
  const auto size = input.tellg();
  if (size < 0) {
    return file_error<CompiledAsset>(path, "cannot determine compiled asset size");
  }
  if (static_cast<std::uintmax_t>(size) > static_cast<std::uintmax_t>(maximum_asset_bytes)) {
    return file_error<CompiledAsset>(path,
                                     "compiled asset exceeds the maximum supported byte size");
  }
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), size);
  if (!input) {
    return file_error<CompiledAsset>(path, "cannot read compiled asset");
  }
  auto decoded = deserialize_asset(bytes);
  if (!decoded.asset) {
    return file_error<CompiledAsset>(path, decoded.error);
  }
  return {std::move(decoded.asset), {}};
}

std::string write_asset_file(const std::string &path, const CompiledAsset &asset) {
  const auto bytes = serialize_asset(asset);
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return path + ": cannot create compiled asset";
  }
  output.write(reinterpret_cast<const char *>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  if (!output) {
    return path + ": cannot write compiled asset";
  }
  return {};
}

} // namespace tetcage
