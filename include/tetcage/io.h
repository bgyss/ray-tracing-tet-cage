#pragma once

#include "tetcage/asset_format.h"

#include <optional>
#include <string>

namespace tetcage {

template <typename T> struct LoadResult {
  std::optional<T> value;
  std::string error;
};

[[nodiscard]] LoadResult<SourceMesh> load_obj(const std::string &path);
[[nodiscard]] LoadResult<Cage> load_tet_cage(const std::string &path);
[[nodiscard]] LoadResult<CompiledAsset> load_asset_file(const std::string &path);
[[nodiscard]] std::string write_asset_file(const std::string &path, const CompiledAsset &asset);

} // namespace tetcage
