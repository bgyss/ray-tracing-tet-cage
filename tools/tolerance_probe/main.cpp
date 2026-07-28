#include "tetcage/math.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

bool parse_double(const char *text, double &value) {
  char *end = nullptr;
  value = std::strtod(text, &end);
  return end != text && *end == '\0';
}

bool parse_u32(const char *text, std::uint32_t &value) {
  const auto parsed = std::from_chars(text, text + std::char_traits<char>::length(text), value);
  return parsed.ec == std::errc{} && *parsed.ptr == '\0';
}

void usage() {
  std::cerr << "usage: tetcage_tolerance_probe <output.json> <coordinate_scale> "
               "<minimum_edge> <condition_estimate> <ulp_multiplier>\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 6) {
    usage();
    return EXIT_FAILURE;
  }
  tetcage::RobustToleranceInput input{};
  if (!parse_double(argv[2], input.coordinate_scale) ||
      !parse_double(argv[3], input.minimum_edge) ||
      !parse_double(argv[4], input.condition_estimate) ||
      !parse_u32(argv[5], input.ulp_multiplier)) {
    std::cerr << "invalid tolerance input\n";
    return EXIT_FAILURE;
  }
  const auto result = tetcage::derive_robust_tolerance(input);
  std::ofstream output(argv[1], std::ios::trunc);
  if (!output) {
    std::cerr << "failed to create output report\n";
    return EXIT_FAILURE;
  }
  output << std::setprecision(17) << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"report_kind\": \"robust_tolerance_policy\",\n"
         << "  \"evidence_class\": \"portable_policy\",\n"
         << "  \"inputs\": {\n"
         << "    \"coordinate_scale\": " << input.coordinate_scale << ",\n"
         << "    \"minimum_edge\": " << input.minimum_edge << ",\n"
         << "    \"condition_estimate\": " << input.condition_estimate << ",\n"
         << "    \"ulp_multiplier\": " << input.ulp_multiplier << "\n"
         << "  },\n"
         << "  \"outputs\": {\n"
         << "    \"position_epsilon\": " << result.position_epsilon << ",\n"
         << "    \"barycentric_epsilon\": " << result.barycentric_epsilon << ",\n"
         << "    \"condition_factor\": " << result.condition_factor << ",\n"
         << "    \"edge_factor\": " << result.edge_factor << ",\n"
         << "    \"decision\": \"" << tetcage::robust_policy_decision_name(result.decision)
         << "\",\n"
         << "    \"reason\": \"" << result.reason << "\"\n"
         << "  },\n"
         << "  \"claim_boundary\": \"This derives a deterministic tolerance; it does not establish "
            "GPU watertightness.\"\n"
         << "}\n";
  if (!output) {
    std::cerr << "failed to write output report\n";
    return EXIT_FAILURE;
  }
  std::cout << "{\"decision\":\"" << tetcage::robust_policy_decision_name(result.decision)
            << "\"}\n";
  return EXIT_SUCCESS;
}
