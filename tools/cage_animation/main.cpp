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
  std::cerr << "usage: tetcage_cage_animation <asset.tetcage> <output.json> "
               "[--samples <u32>] [--motion <value>] [--position-threshold <value>] "
               "[--normal-threshold <value>]\n";
}

template <typename T> bool parse(const std::string &text, T &value) {
  std::istringstream input(text);
  input >> value;
  return input && input.eof();
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3 || (argc - 3) % 2 != 0) {
    usage();
    return EXIT_FAILURE;
  }
  std::uint32_t samples = 8U;
  double motion = 0.05;
  double position_threshold = 1.0e-3;
  double normal_threshold = 1.0e-2;
  for (int index = 3; index < argc; index += 2) {
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
    } else if (option == "--position-threshold") {
      if (!parse(argv[index + 1], position_threshold)) {
        usage();
        return EXIT_FAILURE;
      }
    } else if (option == "--normal-threshold") {
      if (!parse(argv[index + 1], normal_threshold)) {
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
  const auto report = tetcage::analyze_cage_animation(*asset.value, samples, motion,
                                                      position_threshold, normal_threshold);
  const auto json = tetcage::cage_animation_json(report);
  std::cout << json;
  std::ofstream output(argv[2]);
  if (!output) {
    std::cerr << "failed to open output: " << argv[2] << '\n';
    return EXIT_FAILURE;
  }
  output << json;
  // An unsuitable clip is useful authoring evidence, not a command failure.
  return EXIT_SUCCESS;
}
