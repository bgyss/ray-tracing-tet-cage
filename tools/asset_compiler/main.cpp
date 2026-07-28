#include "tetcage/asset_format.h"
#include "tetcage/io.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_asset_compiler <mesh.obj> <cage.tet> <output.tetcage> "
               "[--epsilon <barycentric-value>]\n";
}

bool parse_double(const std::string &text, double &value) {
  std::istringstream input(text);
  input.imbue(std::locale::classic());
  input >> std::noskipws >> value;
  return input && input.peek() == std::char_traits<char>::eof() && std::isfinite(value);
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4 && argc != 6) {
    usage();
    return EXIT_FAILURE;
  }
  tetcage::TolerancePolicy tolerance{};
  if (argc == 6) {
    if (std::string(argv[4]) != "--epsilon" ||
        !parse_double(argv[5], tolerance.expanded_barycentric_epsilon)) {
      usage();
      return EXIT_FAILURE;
    }
  }
  const auto mesh = tetcage::load_obj(argv[1]);
  if (!mesh.value) {
    std::cerr << mesh.error << '\n';
    return EXIT_FAILURE;
  }
  const auto cage = tetcage::load_tet_cage(argv[2]);
  if (!cage.value) {
    std::cerr << cage.error << '\n';
    return EXIT_FAILURE;
  }
  const auto compiled = tetcage::compile_asset(*mesh.value, *cage.value, tolerance);
  if (!compiled.asset) {
    for (const auto &diagnostic : compiled.diagnostics) {
      std::cerr << diagnostic.code << ": " << diagnostic.message << '\n';
    }
    return EXIT_FAILURE;
  }
  const auto write_error = tetcage::write_asset_file(argv[3], *compiled.asset);
  if (!write_error.empty()) {
    std::cerr << write_error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << tetcage::inspect_asset_json(*compiled.asset);
  return EXIT_SUCCESS;
}
