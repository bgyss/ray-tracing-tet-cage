#include "tetcage/io.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: tetcage_cycles_xml <asset.tetcage> <scene.xml>\n";
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  const auto error = tetcage::write_cycles_xml(argv[2], *loaded.value);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "wrote Cycles fallback scene: " << argv[2] << '\n';
  return EXIT_SUCCESS;
}
