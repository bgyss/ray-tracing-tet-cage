#include "tetcage/io.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

namespace {

double orient(const tetcage::Vec3 &center, const tetcage::Vec3 &a, const tetcage::Vec3 &b,
              const tetcage::Vec3 &c) {
  return tetcage::dot(tetcage::cross(a - center, b - center), c - center);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: tetcage_cage_generate <mesh.obj> <output.cage>\n";
    return EXIT_FAILURE;
  }
  const auto mesh = tetcage::load_obj(argv[1]);
  if (!mesh.value || mesh.value->vertices.empty()) {
    std::cerr << (mesh.error.empty() ? "mesh has no vertices" : mesh.error) << '\n';
    return EXIT_FAILURE;
  }
  tetcage::Vec3 minimum{std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::infinity()};
  tetcage::Vec3 maximum{-std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity(),
                        -std::numeric_limits<double>::infinity()};
  for (const auto &vertex : mesh.value->vertices) {
    minimum.x = std::min(minimum.x, vertex.position.x);
    minimum.y = std::min(minimum.y, vertex.position.y);
    minimum.z = std::min(minimum.z, vertex.position.z);
    maximum.x = std::max(maximum.x, vertex.position.x);
    maximum.y = std::max(maximum.y, vertex.position.y);
    maximum.z = std::max(maximum.z, vertex.position.z);
  }
  const double span =
      std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z, 1.0e-3});
  if (maximum.x - minimum.x < 1.0e-9) {
    minimum.x -= span * 0.5;
    maximum.x += span * 0.5;
  }
  if (maximum.y - minimum.y < 1.0e-9) {
    minimum.y -= span * 0.5;
    maximum.y += span * 0.5;
  }
  if (maximum.z - minimum.z < 1.0e-9) {
    minimum.z -= span * 0.5;
    maximum.z += span * 0.5;
  }
  const tetcage::Vec3 center{(minimum.x + maximum.x) * 0.5, (minimum.y + maximum.y) * 0.5,
                             (minimum.z + maximum.z) * 0.5};
  std::array<tetcage::Vec3, 9> vertices{minimum,
                                        {maximum.x, minimum.y, minimum.z},
                                        {maximum.x, maximum.y, minimum.z},
                                        {minimum.x, maximum.y, minimum.z},
                                        {minimum.x, minimum.y, maximum.z},
                                        {maximum.x, minimum.y, maximum.z},
                                        maximum,
                                        {minimum.x, maximum.y, maximum.z},
                                        center};
  constexpr std::array<std::array<std::uint32_t, 3>, 12> faces{{
      {{0U, 3U, 1U}},
      {{1U, 3U, 2U}},
      {{4U, 5U, 7U}},
      {{5U, 6U, 7U}},
      {{0U, 1U, 5U}},
      {{0U, 5U, 4U}},
      {{3U, 7U, 6U}},
      {{3U, 6U, 2U}},
      {{0U, 4U, 7U}},
      {{0U, 7U, 3U}},
      {{1U, 2U, 6U}},
      {{1U, 6U, 5U}},
  }};
  std::ofstream output(argv[2]);
  if (!output) {
    std::cerr << "failed to open output cage\n";
    return EXIT_FAILURE;
  }
  output << std::setprecision(17)
         << "# Generated AABB-center baseline cage; refine before production use.\n";
  for (std::uint32_t index = 0; index < vertices.size(); ++index) {
    output << "v " << (1000U + index) << ' ' << vertices[index].x << ' ' << vertices[index].y << ' '
           << vertices[index].z << '\n';
  }
  for (const auto &face : faces) {
    std::uint32_t b = face[1];
    std::uint32_t c = face[2];
    if (orient(center, vertices[face[0]], vertices[b], vertices[c]) < 0.0) {
      std::swap(b, c);
    }
    output << "t " << (1000U + 8U) << ' ' << (1000U + face[0]) << ' ' << (1000U + b) << ' '
           << (1000U + c) << '\n';
  }
  return EXIT_SUCCESS;
}
