#include "tetcage/runtime.h"

#include <charconv>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {

template <typename T> bool parse_integer(const char *text, T &value) {
  const auto parsed = std::from_chars(text, text + std::char_traits<char>::length(text), value);
  return parsed.ec == std::errc{} && *parsed.ptr == '\0';
}

bool parse_double(const char *text, double &value) {
  char *end = nullptr;
  value = std::strtod(text, &end);
  return end != text && *end == '\0';
}

bool parse_bool(const char *text, bool &value) {
  if (std::string(text) == "0" || std::string(text) == "false") {
    value = false;
    return true;
  }
  if (std::string(text) == "1" || std::string(text) == "true") {
    value = true;
    return true;
  }
  return false;
}

void usage() {
  std::cerr << "usage: tetcage_method_select <output.json> <source_triangles> "
               "<occupied_tetrahedra> <copies> <maximum_condition> "
               "<maximum_position_error> <maximum_normal_error> "
               "<boundary_fallback_fraction> <animated 0|1> "
               "<hardware_tet_backend 0|1> <gpu_correctness_proven 0|1>\n";
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 12) {
    usage();
    return EXIT_FAILURE;
  }
  tetcage::MethodSelectionInput input{};
  if (!parse_integer(argv[2], input.source_triangles) ||
      !parse_integer(argv[3], input.occupied_tetrahedra) || !parse_integer(argv[4], input.copies) ||
      !parse_double(argv[5], input.maximum_condition) ||
      !parse_double(argv[6], input.maximum_position_error) ||
      !parse_double(argv[7], input.maximum_normal_error) ||
      !parse_double(argv[8], input.boundary_fallback_fraction) ||
      !parse_bool(argv[9], input.animated) || !parse_bool(argv[10], input.hardware_tet_backend) ||
      !parse_bool(argv[11], input.gpu_correctness_proven)) {
    std::cerr << "invalid method-selection input\n";
    return EXIT_FAILURE;
  }
  const auto result = tetcage::select_representation(input);
  std::ofstream output(argv[1], std::ios::trunc);
  if (!output) {
    std::cerr << "failed to create output report\n";
    return EXIT_FAILURE;
  }
  output << tetcage::method_selection_json(input, result);
  if (!output) {
    std::cerr << "failed to write output report\n";
    return EXIT_FAILURE;
  }
  std::cout << "{\"representation\":\"" << tetcage::representation_choice_name(result.choice)
            << "\"}\n";
  return EXIT_SUCCESS;
}
