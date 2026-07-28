#include "tetcage/authoring.h"
#include "tetcage/io.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_cage_lods <input.cage> <output-prefix> <manifest.json> "
               "--levels <u32>\n";
}

bool parse_u32(const std::string &text, std::uint32_t &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 6 || std::string(argv[4]) != "--levels") {
    usage();
    return EXIT_FAILURE;
  }
  std::uint32_t levels = 0U;
  if (!parse_u32(argv[5], levels)) {
    usage();
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_tet_cage(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  const auto lods = tetcage::build_cage_lods(*loaded.value, levels);
  if (!lods.error.empty()) {
    std::cerr << lods.error << '\n';
    return EXIT_FAILURE;
  }
  for (const auto &level : lods.levels) {
    const std::string path = std::string(argv[2]) + "-lod" + std::to_string(level.level) + ".cage";
    const auto error = tetcage::write_tet_cage_file(path, level.cage);
    if (!error.empty()) {
      std::cerr << error << '\n';
      return EXIT_FAILURE;
    }
  }
  const auto json = tetcage::cage_lod_json(lods);
  std::cout << json;
  std::ofstream output(argv[3]);
  if (!output) {
    std::cerr << "failed to open output: " << argv[3] << '\n';
    return EXIT_FAILURE;
  }
  output << json;
  return EXIT_SUCCESS;
}
