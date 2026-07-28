#include "tetcage/io.h"
#include "tetcage/runtime.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>

namespace {

void usage() {
  std::cerr << "usage: tetcage_benchmark <asset.tetcage> <result.json> "
               "--scene <id> --seed <u64> --copies <u32> --rays <u32> "
               "--warmup <u32> --iterations <u32> --motion <value> --visible <0..1> "
               "[--inject-corruption]\n";
}

bool parse_u64(const std::string &text, std::uint64_t &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_double(const std::string &text, double &value) {
  std::istringstream input(text);
  input.imbue(std::locale::classic());
  input >> std::noskipws >> value;
  return input && input.peek() == std::char_traits<char>::eof() && std::isfinite(value);
}

bool write_text(const std::filesystem::path &path, const std::string &contents) {
  std::ofstream output(path, std::ios::trunc);
  output << contents;
  return static_cast<bool>(output);
}

} // namespace

int main(int argc, char **argv) {
  if ((argc != 19 && argc != 20) || std::string(argv[3]) != "--scene" ||
      std::string(argv[5]) != "--seed" || std::string(argv[7]) != "--copies" ||
      std::string(argv[9]) != "--rays" || std::string(argv[11]) != "--warmup" ||
      std::string(argv[13]) != "--iterations" || std::string(argv[15]) != "--motion" ||
      std::string(argv[17]) != "--visible" ||
      (argc == 20 && std::string(argv[19]) != "--inject-corruption")) {
    usage();
    return EXIT_FAILURE;
  }
  std::uint64_t seed = 0;
  std::uint64_t copies = 0;
  std::uint64_t rays = 0;
  std::uint64_t warmup = 0;
  std::uint64_t iterations = 0;
  double motion = 0.0;
  double visible = 0.0;
  if (!parse_u64(argv[6], seed) || !parse_u64(argv[8], copies) || !parse_u64(argv[10], rays) ||
      !parse_u64(argv[12], warmup) || !parse_u64(argv[14], iterations) ||
      !parse_double(argv[16], motion) || !parse_double(argv[18], visible) || copies > UINT32_MAX ||
      rays > UINT32_MAX || warmup > UINT32_MAX || iterations > UINT32_MAX) {
    std::cerr << "benchmark numeric argument is invalid\n";
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  tetcage::BenchmarkScene scene{};
  scene.id = argv[4];
  scene.seed = seed;
  scene.copies = static_cast<std::uint32_t>(copies);
  scene.ray_count = static_cast<std::uint32_t>(rays);
  scene.motion_amplitude = motion;
  scene.visible_fraction = visible;
  tetcage::BenchmarkOptions options{};
  options.warmup_iterations = static_cast<std::uint32_t>(warmup);
  options.measured_iterations = static_cast<std::uint32_t>(iterations);
  options.inject_stub_corruption = argc == 20;
  tetcage::BenchmarkResult result{};
  try {
    result = tetcage::run_cpu_stub_benchmark(*loaded.value, scene, options);
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return EXIT_FAILURE;
  }
  std::ostringstream command;
  for (int index = 0; index < argc; ++index) {
    if (index != 0) {
      command << ' ';
    }
    command << argv[index];
  }
  result.command = command.str();
  result.source_commit = TETCAGE_GIT_COMMIT;
  result.source_dirty = TETCAGE_GIT_DIRTY != 0;
  result.host_os_version = TETCAGE_SYSTEM_VERSION;
  const auto manifest = tetcage::benchmark_manifest_json(result);
  const std::filesystem::path json_path(argv[2]);
  auto csv_path = json_path;
  csv_path.replace_extension(".csv");
  if (!write_text(json_path, manifest) ||
      !write_text(csv_path, tetcage::benchmark_samples_csv(result))) {
    std::cerr << "cannot write benchmark JSON/CSV output\n";
    return EXIT_FAILURE;
  }
  std::cout << manifest;
  return result.correctness_errors == 0U ? EXIT_SUCCESS : 3;
}
