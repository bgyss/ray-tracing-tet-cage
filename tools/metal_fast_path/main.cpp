#include "tetcage/io.h"
#include "tetcage/metal_backend.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_metal_fast_path <asset.tetcage> <result.json> "
               "--copies <u32> --rays <u32> --motion <value> "
               "[--frames <u32>] [--rebuild-period <u32>] "
               "[--compact] [--extended-limits] [--boundary-fallback] [--gpu-instances] "
               "[--gpu-only] [--allow-unverified]\n";
}

bool parse_u32(const std::string &text, std::uint32_t &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_double(const std::string &text, double &value) {
  std::istringstream input(text);
  input.imbue(std::locale::classic());
  input >> std::noskipws >> value;
  return input && input.peek() == std::char_traits<char>::eof() && std::isfinite(value);
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 9 || std::string(argv[3]) != "--copies" || std::string(argv[5]) != "--rays" ||
      std::string(argv[7]) != "--motion") {
    usage();
    return EXIT_FAILURE;
  }

  tetcage::MetalFastPathOptions options{};
  if (!parse_u32(argv[4], options.copies) || !parse_u32(argv[6], options.ray_count) ||
      !parse_double(argv[8], options.motion_amplitude) || options.copies == 0U ||
      options.ray_count == 0U) {
    usage();
    return EXIT_FAILURE;
  }
  for (int index = 9; index < argc; ++index) {
    const std::string flag(argv[index]);
    if (flag == "--frames" || flag == "--rebuild-period") {
      if (index + 1 >= argc) {
        usage();
        return EXIT_FAILURE;
      }
      std::uint32_t value{};
      if (!parse_u32(argv[++index], value) || (flag == "--frames" && value == 0U)) {
        usage();
        return EXIT_FAILURE;
      }
      if (flag == "--frames") {
        options.frames = value;
      } else {
        options.rebuild_period = value;
      }
    } else if (flag == "--compact") {
      options.compact_blas = true;
    } else if (flag == "--extended-limits") {
      options.extended_limits = true;
    } else if (flag == "--boundary-fallback") {
      options.boundary_fallback = true;
    } else if (flag == "--gpu-instances") {
      options.gpu_instances = true;
    } else if (flag == "--gpu-only") {
      options.cpu_validation = false;
    } else if (flag == "--allow-unverified") {
      options.allow_unverified = true;
    } else {
      usage();
      return EXIT_FAILURE;
    }
  }
  if (!options.cpu_validation && options.boundary_fallback) {
    std::cerr << "--gpu-only cannot be combined with --boundary-fallback\n";
    return EXIT_FAILURE;
  }

  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }

  std::string command;
  for (int index = 0; index < argc; ++index) {
    if (!command.empty()) {
      command += ' ';
    }
    command += argv[index];
  }
  const auto outcome = tetcage::run_metal_fast_path(*loaded.value, options, command);
  std::ofstream output(argv[2], std::ios::trunc);
  output << outcome.manifest;
  if (!output) {
    std::cerr << "failed to write Metal result manifest\n";
    return EXIT_FAILURE;
  }
  std::cout << outcome.manifest;
  return outcome.exit_code;
}
