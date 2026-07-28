#include "tetcage/authoring.h"
#include "tetcage/io.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_cage_refine <input.cage> <output.cage> [--levels <u32>]\n";
}

bool parse_u32(const std::string &text, std::uint32_t &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 3 || argc > 5 || (argc == 5 && std::string(argv[3]) != "--levels")) {
    usage();
    return EXIT_FAILURE;
  }
  std::uint32_t levels = 1U;
  if (argc == 5 && (!parse_u32(argv[4], levels) || levels == 0U)) {
    usage();
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_tet_cage(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  const auto refined = tetcage::refine_cage(*loaded.value, levels);
  if (!refined.cage) {
    std::cerr << refined.error << '\n';
    return EXIT_FAILURE;
  }
  const auto error = tetcage::write_tet_cage_file(argv[2], *refined.cage);
  if (!error.empty()) {
    std::cerr << error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "{\"schema_version\":1,\"levels\":" << levels
            << ",\"vertices\":" << refined.cage->vertices.size()
            << ",\"tetrahedra\":" << refined.cage->tetrahedra.size() << "}\n";
  return EXIT_SUCCESS;
}
