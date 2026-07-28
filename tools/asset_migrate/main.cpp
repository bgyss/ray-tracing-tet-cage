#include "tetcage/asset_format.h"
#include "tetcage/io.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void usage() {
  std::cerr << "usage: tetcage_asset_migrate <input.tetcage> <output.tetcage> <report.json>\n";
}

bool read_bytes(const std::string &path, std::vector<std::byte> &bytes) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input) {
    return false;
  }
  const auto size = input.tellg();
  if (size < 0) {
    return false;
  }
  bytes.resize(static_cast<std::size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), size);
  return static_cast<bool>(input);
}

std::string checksum_hex(const std::vector<std::byte> &bytes) {
  std::ostringstream output;
  output << std::hex << tetcage::asset_checksum(bytes);
  return output.str();
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 4) {
    usage();
    return EXIT_FAILURE;
  }
  std::vector<std::byte> input_bytes;
  if (!read_bytes(argv[1], input_bytes)) {
    std::cerr << "failed to read input asset\n";
    return EXIT_FAILURE;
  }
  const auto decoded = tetcage::deserialize_asset(input_bytes);
  if (!decoded.asset) {
    std::cerr << "migration refused: " << decoded.error << '\n';
    return EXIT_FAILURE;
  }
  // There is no historical pre-v1 stream in this repository. Keep the
  // identity adapter explicit so a future format change must add a real table
  // and a new golden fixture rather than silently rewriting unknown bytes.
  const auto output_bytes = tetcage::serialize_asset(*decoded.asset);
  std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
  if (!output) {
    std::cerr << "failed to create output asset\n";
    return EXIT_FAILURE;
  }
  output.write(reinterpret_cast<const char *>(output_bytes.data()),
               static_cast<std::streamsize>(output_bytes.size()));
  if (!output) {
    std::cerr << "failed to write output asset\n";
    return EXIT_FAILURE;
  }
  std::ofstream report(argv[3]);
  if (!report) {
    std::cerr << "failed to create migration report\n";
    return EXIT_FAILURE;
  }
  report << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"status\": \"identity_current_format\",\n"
         << "  \"source_format_version\": " << decoded.asset->format_version << ",\n"
         << "  \"target_format_version\": " << tetcage::asset_format_version << ",\n"
         << "  \"migration\": \"v1_identity_round_trip\",\n"
         << "  \"source_bytes\": " << input_bytes.size() << ",\n"
         << "  \"target_bytes\": " << output_bytes.size() << ",\n"
         << "  \"source_checksum\": \"" << checksum_hex(input_bytes) << "\",\n"
         << "  \"target_checksum\": \"" << checksum_hex(output_bytes) << "\",\n"
         << "  \"byte_identical\": " << (input_bytes == output_bytes ? "true" : "false") << ",\n"
         << "  \"evidence_class\": \"portable_test\"\n"
         << "}\n";
  std::cout << "{\"migration\":\"v1_identity_round_trip\",\"byte_identical\":"
            << (input_bytes == output_bytes ? "true" : "false") << "}\n";
  return EXIT_SUCCESS;
}
