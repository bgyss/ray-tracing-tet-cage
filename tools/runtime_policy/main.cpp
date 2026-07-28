#include "tetcage/io.h"
#include "tetcage/runtime.h"

#include <fstream>
#include <iostream>
#include <string>

namespace {

std::string json_escape(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char character : value) {
    if (character == '\\' || character == '"') {
      escaped.push_back('\\');
    }
    if (character == '\n') {
      escaped += "\\n";
    } else if (character == '\r') {
      escaped += "\\r";
    } else if (character == '\t') {
      escaped += "\\t";
    } else {
      escaped.push_back(character);
    }
  }
  return escaped;
}

struct CaseResult {
  std::string name;
  tetcage::BackendFrameResult result;
};

tetcage::FrameBuildInput valid_input(const tetcage::CompiledAsset &asset) {
  tetcage::FrameBuildInput input{};
  input.objects.push_back({1U, 0U, 0U, true});
  input.poses.push_back({asset.cage.vertices});
  return input;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 3) {
    std::cerr << "usage: tetcage_runtime_policy <asset.tetcage> <output.json>\n";
    return 2;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << "failed to load asset: " << loaded.error << '\n';
    return 1;
  }
  auto asset = std::make_shared<const tetcage::CompiledAsset>(*loaded.value);
  tetcage::CpuStubBackend backend(asset);
  std::vector<CaseResult> cases;

  auto normal = valid_input(*asset);
  cases.push_back({"normal_build", backend.build_frame(normal)});

  auto unsupported = valid_input(*asset);
  unsupported.policy.strategy = tetcage::BuildStrategy::update;
  cases.push_back({"unsupported_update", backend.build_frame(unsupported)});

  auto allocation = valid_input(*asset);
  allocation.control.max_allocation_bytes = 1U;
  cases.push_back({"allocation_limit", backend.build_frame(allocation)});

  auto cancelled = valid_input(*asset);
  cancelled.control.cancellation_requested = true;
  cases.push_back({"cancelled_after_valid_frame", backend.build_frame(cancelled)});

  auto lost = valid_input(*asset);
  lost.control.device_lost = true;
  cases.push_back({"device_lost", backend.build_frame(lost)});

  tetcage::CpuStubBackend reset_backend(asset);
  auto reset = valid_input(*asset);
  reset.control.reset_requested = true;
  cases.push_back({"reset_required", reset_backend.build_frame(reset)});

  std::ofstream output(argv[2], std::ios::trunc);
  if (!output) {
    std::cerr << "failed to create output report\n";
    return 1;
  }
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"report_kind\": \"runtime_safety_policy\",\n"
         << "  \"evidence_class\": \"portable_contract\",\n"
         << "  \"asset_format_version\": " << asset->format_version << ",\n"
         << "  \"cases\": [\n";
  for (std::size_t index = 0; index < cases.size(); ++index) {
    const auto &entry = cases[index];
    const auto &result = entry.result;
    output << "    {\"name\": \"" << entry.name << "\", \"status\": \""
           << tetcage::frame_build_status_name(result.status) << "\", \"fallback\": \""
           << tetcage::frame_fallback_name(result.fallback)
           << "\", \"allocation_bytes\": " << result.allocation_bytes << ", \"error\": \""
           << json_escape(result.error) << "\"}" << (index + 1U == cases.size() ? "\n" : ",\n");
  }
  output << "  ],\n"
         << "  \"claim_boundary\": \"This report proves portable policy behavior only; GPU "
            "device-loss recovery and renderer integration remain hardware/source gates.\"\n"
         << "}\n";
  if (!output) {
    std::cerr << "failed to write output report\n";
    return 1;
  }
  std::cout << "{\"cases\":" << cases.size() << "}\n";
  return 0;
}
