#include "tetcage/asset_format.h"
#include "tetcage/io.h"
#include "tetcage/oracle.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

using Pixel = std::array<std::uint8_t, 3>;

bool parse_u32(const std::string &text, std::uint32_t &value) {
  std::istringstream input(text);
  input >> value;
  return input && input.eof() && value > 0U;
}

std::uint8_t byte(double value) { return static_cast<std::uint8_t>(std::clamp(value, 0.0, 255.0)); }

bool write_ppm(const std::filesystem::path &path, std::uint32_t width, std::uint32_t height,
               const std::vector<Pixel> &pixels) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    return false;
  }
  output << "P6\n" << width << ' ' << height << "\n255\n";
  for (const auto &pixel : pixels) {
    output.write(reinterpret_cast<const char *>(pixel.data()), 3);
  }
  return static_cast<bool>(output);
}

struct ImageSet {
  std::vector<Pixel> position;
  std::vector<Pixel> normal;
  std::vector<Pixel> uv;
  std::vector<Pixel> material;
  std::vector<Pixel> ownership;
  std::vector<Pixel> miss_classification;
  std::uint64_t hit_mismatches{};
  std::uint64_t primitive_mismatches{};
  std::uint64_t material_mismatches{};
  std::uint64_t ownership_changes{};
  double max_position_error{};
  double max_normal_error{};
  double max_uv_error{};
};

ImageSet render(const tetcage::CompiledAsset &asset, const tetcage::Bvh4D &bvh, std::uint32_t width,
                std::uint32_t height) {
  tetcage::Vec3 minimum{std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity()};
  tetcage::Vec3 maximum{-std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
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
  const double span = std::max({x_span, y_span, std::max(1.0e-9, maximum.z - minimum.z)});
  const double x_min = minimum.x - x_span * 0.05;
  const double x_max = maximum.x + x_span * 0.05;
  const double y_min = minimum.y - y_span * 0.05;
  const double y_max = maximum.y + y_span * 0.05;
  const double origin_z = maximum.z + span * 2.0;

  ImageSet result{};
  const auto reserve = static_cast<std::size_t>(width) * height;
  for (auto *view : {&result.position, &result.normal, &result.uv, &result.material,
                     &result.ownership, &result.miss_classification}) {
    view->reserve(reserve);
  }
  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      const double u = (static_cast<double>(x) + 0.5) / static_cast<double>(width);
      const double v = (static_cast<double>(y) + 0.5) / static_cast<double>(height);
      const tetcage::Ray ray{{x_min + (x_max - x_min) * u, y_min + (y_max - y_min) * v, origin_z},
                             {0.0, 0.0, -1.0},
                             0.0,
                             span * 8.0};
      const auto dense = tetcage::trace_dense(asset.source, ray);
      const auto exact = tetcage::trace_watertight4d(asset, bvh, asset.cage.vertices, ray,
                                                     tetcage::ProjectionMode::bounded_simplex);
      if (!dense.closest && !exact.closest) {
        result.position.push_back({0, 0, 0});
        result.normal.push_back({0, 0, 0});
        result.uv.push_back({0, 0, 0});
        result.material.push_back({0, 0, 0});
        result.ownership.push_back({0, 0, 0});
        result.miss_classification.push_back({0, 0, 96});
        continue;
      }
      if (!dense.closest || !exact.closest) {
        ++result.hit_mismatches;
        result.position.push_back({255, 0, 0});
        result.normal.push_back({255, 0, 0});
        result.uv.push_back({255, 0, 0});
        result.material.push_back({255, 0, 0});
        result.ownership.push_back({255, 0, 0});
        result.miss_classification.push_back(dense.closest ? Pixel{255, 196, 0} : Pixel{255, 0, 0});
        continue;
      }
      const auto &expected = *dense.closest;
      const auto &actual = *exact.closest;
      const double position_error = tetcage::length(expected.position - actual.position);
      const double normal_error = tetcage::length(expected.normal - actual.normal);
      const double uv_error =
          std::max(std::abs(expected.uv.x - actual.uv.x), std::abs(expected.uv.y - actual.uv.y));
      result.max_position_error = std::max(result.max_position_error, position_error);
      result.max_normal_error = std::max(result.max_normal_error, normal_error);
      result.max_uv_error = std::max(result.max_uv_error, uv_error);
      result.position.push_back({byte(position_error * 1.0e9), 0, 0});
      result.normal.push_back({byte(normal_error * 1.0e9), 0, 0});
      result.uv.push_back({byte(uv_error * 1.0e9), 0, 0});
      if (expected.material != actual.material) {
        ++result.material_mismatches;
        result.material.push_back({255, 0, 0});
      } else {
        result.material.push_back({0, 180, 0});
      }
      if (expected.source_primitive != actual.source_primitive) {
        ++result.primitive_mismatches;
        result.ownership.push_back({255, 0, 0});
      } else {
        result.ownership.push_back({0, byte(48.0 + 31.0 * (actual.tet_id % 7U)),
                                    byte(96.0 + 23.0 * (actual.tet_id % 7U))});
      }
      result.miss_classification.push_back({0, 180, 0});
    }
  }
  return result;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 7 || std::string(argv[3]) != "--width" || std::string(argv[5]) != "--height") {
    std::cerr << "usage: tetcage_image_differential <asset.tetcage> <output-dir> "
                 "--width <u32> --height <u32>\n";
    return EXIT_FAILURE;
  }
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  if (!parse_u32(argv[4], width) || !parse_u32(argv[6], height)) {
    std::cerr << "image dimensions must be positive integers\n";
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  const auto output_dir = std::filesystem::path(argv[2]);
  std::error_code error;
  std::filesystem::create_directories(output_dir, error);
  if (error) {
    std::cerr << output_dir << ": cannot create image directory: " << error.message() << '\n';
    return EXIT_FAILURE;
  }
  const auto bvh = tetcage::build_bvh4d(*loaded.value, 4U);
  const auto images = render(*loaded.value, bvh, width, height);
  const std::array<std::pair<const char *, const std::vector<Pixel> *>, 6> views{{
      {"position", &images.position},
      {"normal", &images.normal},
      {"uv", &images.uv},
      {"material", &images.material},
      {"ownership", &images.ownership},
      {"miss_classification", &images.miss_classification},
  }};
  for (const auto &[name, pixels] : views) {
    if (!write_ppm(output_dir / (std::string(name) + ".ppm"), width, height, *pixels)) {
      std::cerr << "cannot write " << name << " image\n";
      return EXIT_FAILURE;
    }
  }
  std::cout
      << "{\n"
      << "  \"schema_version\": 1,\n"
      << "  \"report_kind\": \"image_space_differential\",\n"
      << "  \"width\": " << width << ",\n"
      << "  \"height\": " << height << ",\n"
      << "  \"channels\": [\"position\", \"normal\", \"uv\", \"material\", "
         "\"ownership\", \"miss_classification\"],\n"
      << "  \"comparison_policy\": \"Per-pixel edge disagreements are retained as a "
         "visualization classification; the adversarial ray oracle is the ownership gate.\",\n"
      << "  \"correctness\": {\"hit_mismatches\": " << images.hit_mismatches
      << ", \"primitive_mismatches\": " << images.primitive_mismatches
      << ", \"material_mismatches\": " << images.material_mismatches
      << ", \"ownership_changes\": " << images.ownership_changes
      << ", \"max_position_error\": " << images.max_position_error
      << ", \"max_normal_error\": " << images.max_normal_error
      << ", \"max_uv_error\": " << images.max_uv_error << "}\n"
      << "}\n";
  return EXIT_SUCCESS;
}
