#include "tetcage/authoring.h"
#include "tetcage/io.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_cage_quality <asset.tetcage> <output.json> "
               "[--samples <u32>] [--motion <value>]\n";
}

template <typename T> bool parse(const std::string &text, T &value) {
  std::istringstream input(text);
  input >> value;
  return input && input.eof();
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3) {
    usage();
    return EXIT_FAILURE;
  }
  std::uint32_t samples = 8U;
  double motion = 0.05;
  for (int index = 3; index < argc; index += 2) {
    if (index + 1 >= argc) {
      usage();
      return EXIT_FAILURE;
    }
    const std::string option = argv[index];
    if (option == "--samples") {
      if (!parse(argv[index + 1], samples) || samples == 0U) {
        usage();
        return EXIT_FAILURE;
      }
    } else if (option == "--motion") {
      if (!parse(argv[index + 1], motion)) {
        usage();
        return EXIT_FAILURE;
      }
    } else {
      usage();
      return EXIT_FAILURE;
    }
  }
  const auto asset = tetcage::load_asset_file(argv[1]);
  if (!asset.value) {
    std::cerr << asset.error << '\n';
    return EXIT_FAILURE;
  }
  const auto report = tetcage::analyze_cage_quality(*asset.value, samples, motion);
  const auto json = tetcage::cage_quality_json(report);
  std::cout << json;
  std::ofstream output(argv[2]);
  if (!output) {
    std::cerr << "failed to open output: " << argv[2] << '\n';
    return EXIT_FAILURE;
  }
  output << json;
  return report.suitable ? EXIT_SUCCESS : 2;
}
