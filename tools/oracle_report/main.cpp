#include "tetcage/asset_format.h"
#include "tetcage/io.h"
#include "tetcage/oracle.h"

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool parse_u64(const std::string &text, std::uint64_t &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

bool parse_double(const std::string &text, double &value) {
  std::istringstream input(text);
  input >> value;
  return input && input.eof();
}

std::string json_escape(const std::string &value) {
  std::ostringstream output;
  for (const char character : value) {
    if (character == '"' || character == '\\') {
      output << '\\';
    }
    output << character;
  }
  return output.str();
}

std::array<tetcage::Vec3, 4> ordered_pose(const tetcage::CompiledAsset &asset,
                                          const tetcage::Bvh4DTree &tree) {
  std::array<tetcage::Vec3, 4> local{};
  const auto &tet = asset.cage.tetrahedra[tree.tet_id];
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    local[corner] = asset.cage.vertices[tet.vertex_indices[corner]];
  }
  std::array<tetcage::Vec3, 4> ordered{};
  for (std::size_t corner = 0; corner < 4U; ++corner) {
    ordered[corner] = local[tree.ordered_to_local[corner]];
  }
  return ordered;
}

std::vector<tetcage::Vec3> animated_pose(const tetcage::CompiledAsset &asset, std::uint32_t frame,
                                         double amplitude) {
  std::vector<tetcage::Vec3> pose = asset.cage.vertices;
  const double phase = static_cast<double>(frame) * 0.37;
  for (std::size_t index = 0; index < pose.size(); ++index) {
    const double weight = static_cast<double>(index + 1U);
    pose[index].x += amplitude * 0.25 * std::cos(phase + weight * 0.17);
    pose[index].y += amplitude * 0.5 * std::sin(phase + weight * 0.23);
    pose[index].z += amplitude * std::sin(phase + weight * 0.31);
  }
  return pose;
}

struct ProjectionRun {
  double milliseconds{};
  std::uint64_t hits{};
  std::uint64_t visited_nodes{};
  std::uint64_t raw_duplicate_candidates{};
};

struct BoundsRun {
  double milliseconds{};
  double summed_volume{};
  std::uint64_t invalid_bounds{};
};

ProjectionRun trace_projection(const tetcage::CompiledAsset &asset, const tetcage::Bvh4D &bvh,
                               const std::vector<tetcage::Ray> &rays,
                               tetcage::ProjectionMode mode) {
  ProjectionRun run{};
  const auto start = std::chrono::steady_clock::now();
  for (const auto &ray : rays) {
    const auto trace = tetcage::trace_watertight4d(asset, bvh, asset.cage.vertices, ray, mode);
    run.hits += trace.closest.has_value() ? 1U : 0U;
    run.visited_nodes += trace.visited_nodes;
    run.raw_duplicate_candidates += trace.raw_duplicate_candidates;
  }
  const auto stop = std::chrono::steady_clock::now();
  run.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  return run;
}

BoundsRun build_projected_bounds(const tetcage::CompiledAsset &asset, const tetcage::Bvh4D &bvh,
                                 tetcage::ProjectionMode mode) {
  BoundsRun run{};
  const auto start = std::chrono::steady_clock::now();
  for (const auto &tree : bvh.trees) {
    const auto pose = ordered_pose(asset, tree);
    for (const auto &node : tree.nodes) {
      if (mode == tetcage::ProjectionMode::interval_sum) {
        run.summed_volume +=
            tetcage::aabb_volume(tetcage::project_bounds_interval_sum(node.bounds, pose));
      } else {
        const auto bounds = tetcage::project_bounds_bounded_simplex(node.bounds, pose);
        if (bounds) {
          run.summed_volume += tetcage::aabb_volume(*bounds);
        } else {
          ++run.invalid_bounds;
        }
      }
    }
  }
  const auto stop = std::chrono::steady_clock::now();
  run.milliseconds = std::chrono::duration<double, std::milli>(stop - start).count();
  return run;
}

} // namespace

int main(int argc, char **argv) {
  if (argc != 7 && argc != 11) {
    std::cerr << "usage: tetcage_oracle_report <asset.tetcage> <output.json> "
                 "--seed <u64> --random-rays <u32> [--frames <u32> --motion <value>]\n";
    return EXIT_FAILURE;
  }
  std::uint64_t seed = 0;
  std::uint64_t random_ray_count = 0;
  if (std::string(argv[3]) != "--seed" || std::string(argv[5]) != "--random-rays" ||
      !parse_u64(argv[4], seed) || !parse_u64(argv[6], random_ray_count) ||
      random_ray_count == 0U || random_ray_count > UINT32_MAX) {
    std::cerr << "seed or random ray count is invalid\n";
    return EXIT_FAILURE;
  }
  std::uint64_t frame_count = 1U;
  double motion = 0.0;
  if (argc == 11 &&
      (std::string(argv[7]) != "--frames" || std::string(argv[9]) != "--motion" ||
       !parse_u64(argv[8], frame_count) || frame_count == 0U || frame_count > UINT32_MAX ||
       !parse_double(argv[10], motion) || !std::isfinite(motion))) {
    std::cerr << "frame count or motion is invalid\n";
    return EXIT_FAILURE;
  }
  const auto loaded = tetcage::load_asset_file(argv[1]);
  if (!loaded.value) {
    std::cerr << loaded.error << '\n';
    return EXIT_FAILURE;
  }
  const auto &asset = *loaded.value;
  const auto build_start = std::chrono::steady_clock::now();
  const auto bvh = tetcage::build_bvh4d(asset, 4U);
  const auto build_stop = std::chrono::steady_clock::now();
  const double build_ms =
      std::chrono::duration<double, std::milli>(build_stop - build_start).count();
  tetcage::OracleComparison comparison{};
  ProjectionRun interval{};
  ProjectionRun exact{};
  std::uint64_t ray_count = 0U;
  struct FrameResult {
    std::uint64_t frame{};
    std::uint64_t rays{};
    std::uint64_t fast_misses{};
    std::uint64_t exact_misses{};
    std::uint64_t ownership_changes{};
  };
  std::vector<FrameResult> frames;
  for (std::uint32_t frame = 0; frame < frame_count; ++frame) {
    const auto pose = animated_pose(asset, frame, motion);
    const auto rays = tetcage::generate_adversarial_rays(
        asset, pose, seed + frame, static_cast<std::uint32_t>(random_ray_count));
    const auto current = tetcage::compare_oracles(asset, bvh, pose, rays);
    const auto current_interval =
        trace_projection(asset, bvh, rays, tetcage::ProjectionMode::interval_sum);
    const auto current_exact =
        trace_projection(asset, bvh, rays, tetcage::ProjectionMode::bounded_simplex);
    frames.push_back({frame, current.rays, current.fast_misses, current.exact_misses,
                      current.primitive_mismatches + current.exact_duplicate_ownership});
    ray_count += current.rays;
    comparison.rays += current.rays;
    comparison.fast_misses += current.fast_misses;
    comparison.exact_misses += current.exact_misses;
    comparison.primitive_mismatches += current.primitive_mismatches;
    comparison.fast_duplicate_ownership += current.fast_duplicate_ownership;
    comparison.exact_duplicate_ownership += current.exact_duplicate_ownership;
    comparison.fast_raw_duplicate_candidates += current.fast_raw_duplicate_candidates;
    comparison.exact_raw_duplicate_candidates += current.exact_raw_duplicate_candidates;
    comparison.fast_visited_nodes += current.fast_visited_nodes;
    comparison.exact_visited_nodes += current.exact_visited_nodes;
    comparison.max_position_error =
        std::max(comparison.max_position_error, current.max_position_error);
    comparison.max_attribute_error =
        std::max(comparison.max_attribute_error, current.max_attribute_error);
    for (const auto &failure : current.failures) {
      if (comparison.failures.size() < 64U) {
        comparison.failures.push_back(failure);
      }
    }
    interval.milliseconds += current_interval.milliseconds;
    interval.hits += current_interval.hits;
    interval.visited_nodes += current_interval.visited_nodes;
    interval.raw_duplicate_candidates += current_interval.raw_duplicate_candidates;
    exact.milliseconds += current_exact.milliseconds;
    exact.hits += current_exact.hits;
    exact.visited_nodes += current_exact.visited_nodes;
    exact.raw_duplicate_candidates += current_exact.raw_duplicate_candidates;
  }

  const auto interval_bounds =
      build_projected_bounds(asset, bvh, tetcage::ProjectionMode::interval_sum);
  const auto exact_bounds =
      build_projected_bounds(asset, bvh, tetcage::ProjectionMode::bounded_simplex);
  std::uint64_t node_count = 0;
  for (const auto &tree : bvh.trees) {
    node_count += tree.nodes.size();
  }

  std::ostringstream command;
  for (int index = 0; index < argc; ++index) {
    if (index != 0) {
      command << ' ';
    }
    command << argv[index];
  }
  const auto bytes = tetcage::serialize_asset(asset);
  std::ostringstream hash;
  hash << std::hex << std::setfill('0') << std::setw(16) << tetcage::asset_checksum(bytes);

  std::ostringstream json;
  json << std::setprecision(17);
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"report_kind\": \"cpu_4d_oracle\",\n"
       << "  \"evidence_class\": \"synthetic\",\n"
       << "  \"asset_hash_fnv1a64\": \"" << hash.str() << "\",\n"
       << "  \"seed\": " << seed << ",\n"
       << "  \"frames\": " << frame_count << ",\n"
       << "  \"motion\": " << motion << ",\n"
       << "  \"command\": \"" << json_escape(command.str()) << "\",\n"
       << "  \"ray_count\": " << ray_count << ",\n"
       << "  \"bvh\": {\n"
       << "    \"tree_count\": " << bvh.trees.size() << ",\n"
       << "    \"node_count\": " << node_count << ",\n"
       << "    \"leaf_size\": " << bvh.leaf_size << ",\n"
       << "    \"build_ms\": " << build_ms << "\n"
       << "  },\n"
       << "  \"correctness\": {\n"
       << "    \"fast_misses\": " << comparison.fast_misses << ",\n"
       << "    \"exact_misses\": " << comparison.exact_misses << ",\n"
       << "    \"primitive_mismatches\": " << comparison.primitive_mismatches << ",\n"
       << "    \"fast_duplicate_ownership\": " << comparison.fast_duplicate_ownership << ",\n"
       << "    \"exact_duplicate_ownership\": " << comparison.exact_duplicate_ownership << ",\n"
       << "    \"fast_raw_duplicate_candidates\": " << comparison.fast_raw_duplicate_candidates
       << ",\n"
       << "    \"exact_raw_duplicate_candidates\": " << comparison.exact_raw_duplicate_candidates
       << ",\n"
       << "    \"max_position_error\": " << comparison.max_position_error << ",\n"
       << "    \"max_attribute_error\": " << comparison.max_attribute_error << ",\n"
       << "    \"failures\": [\n";
  for (std::size_t index = 0; index < comparison.failures.size(); ++index) {
    const auto &failure = comparison.failures[index];
    json << "      {\"ray_index\": " << failure.ray_index << ", \"origin\": ["
         << failure.ray.origin.x << ", " << failure.ray.origin.y << ", " << failure.ray.origin.z
         << "], \"direction\": [" << failure.ray.direction.x << ", " << failure.ray.direction.y
         << ", " << failure.ray.direction.z
         << "], \"fast_hit\": " << (failure.fast_hit ? "true" : "false")
         << ", \"exact_hit\": " << (failure.exact_hit ? "true" : "false") << "}"
         << (index + 1U == comparison.failures.size() ? "\n" : ",\n");
  }
  json << "    ]\n"
       << "  },\n"
       << "  \"frame_correctness\": [\n";
  for (std::size_t index = 0; index < frames.size(); ++index) {
    const auto &frame = frames[index];
    json << "    {\"frame\": " << frame.frame << ", \"rays\": " << frame.rays
         << ", \"fast_misses\": " << frame.fast_misses
         << ", \"exact_misses\": " << frame.exact_misses
         << ", \"ownership_changes\": " << frame.ownership_changes << "}"
         << (index + 1U == frames.size() ? "\n" : ",\n");
  }
  json << "  ],\n"
       << "  \"projection_comparison\": {\n"
       << "    \"paper_interval_sum\": {\n"
       << "      \"summed_node_volume\": " << interval_bounds.summed_volume << ",\n"
       << "      \"bounds_build_ms\": " << interval_bounds.milliseconds << ",\n"
       << "      \"invalid_bounds\": " << interval_bounds.invalid_bounds << ",\n"
       << "      \"trace_ms\": " << interval.milliseconds << ",\n"
       << "      \"visited_nodes\": " << interval.visited_nodes << ",\n"
       << "      \"hits\": " << interval.hits << ",\n"
       << "      \"raw_duplicate_candidates\": " << interval.raw_duplicate_candidates << "\n"
       << "    },\n"
       << "    \"bounded_simplex\": {\n"
       << "      \"summed_node_volume\": " << exact_bounds.summed_volume << ",\n"
       << "      \"bounds_build_ms\": " << exact_bounds.milliseconds << ",\n"
       << "      \"invalid_bounds\": " << exact_bounds.invalid_bounds << ",\n"
       << "      \"trace_ms\": " << exact.milliseconds << ",\n"
       << "      \"visited_nodes\": " << exact.visited_nodes << ",\n"
       << "      \"hits\": " << exact.hits << ",\n"
       << "      \"raw_duplicate_candidates\": " << exact.raw_duplicate_candidates << "\n"
       << "    }\n"
       << "  }\n"
       << "}\n";

  std::ofstream output(argv[2], std::ios::trunc);
  if (!output) {
    std::cerr << argv[2] << ": cannot create report\n";
    return EXIT_FAILURE;
  }
  output << json.str();
  if (!output) {
    std::cerr << argv[2] << ": cannot write report\n";
    return EXIT_FAILURE;
  }
  std::cout << json.str();
  return comparison.exact_misses == 0U && comparison.exact_duplicate_ownership == 0U ? EXIT_SUCCESS
                                                                                     : EXIT_FAILURE;
}
