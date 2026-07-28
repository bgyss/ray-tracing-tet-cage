#include "tetcage/asset_format.h"
#include "tetcage/io.h"

#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: tetcage_inspect <asset.tetcage>\n";
    return EXIT_FAILURE;
  }
  const auto asset = tetcage::load_asset_file(argv[1]);
  if (!asset.value) {
    std::cerr << asset.error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << tetcage::inspect_asset_json(*asset.value);
  return EXIT_SUCCESS;
}
