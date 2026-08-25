cmake_minimum_required(VERSION 3.25)

project(tetcage
  VERSION 0.1.0
  DESCRIPTION "Portable tetrahedral-cage animated ray-tracing research implementation"
  LANGUAGES CXX)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TETCAGE_BUILD_TESTS "Build the portable test suite" ON)
option(TETCAGE_BUILD_TOOLS "Build command-line tools" ON)
option(TETCAGE_ENABLE_METAL "Build the real Metal capability probe on macOS" ON)
option(TETCAGE_ENABLE_VULKAN "Build the real Vulkan capability probe when an SDK is available" ON)

add_library(tetcage_project_options INTERFACE)
target_compile_features(tetcage_project_options INTERFACE cxx_std_20)
if(MSVC)
  target_compile_options(tetcage_project_options INTERFACE /W4 /permissive-)
else()
  target_compile_options(tetcage_project_options INTERFACE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion)
endif()

add_library(tetcage_core
  src/core/math.cpp
  src/compiler/asset_compiler.cpp
  src/compiler/cycles_xml.cpp
  src/compiler/cycles_bridge.cpp
  src/compiler/io.cpp
  src/cpu_reference/oracle.cpp
  src/core/metal_evidence.cpp
  src/core/runtime.cpp
  src/core/authoring.cpp)
target_include_directories(tetcage_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(tetcage_core PUBLIC tetcage_project_options)

if(TETCAGE_BUILD_TESTS)
  add_executable(tetcage_tests tests/test_main.cpp)
  target_link_libraries(tetcage_tests PRIVATE tetcage_core)
  target_compile_definitions(tetcage_tests PRIVATE
    TETCAGE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME tetcage.portable COMMAND tetcage_tests)
endif()

if(TETCAGE_BUILD_TOOLS)
  add_executable(tetcage_asset_compiler tools/asset_compiler/main.cpp)
  target_link_libraries(tetcage_asset_compiler PRIVATE tetcage_core)
  add_executable(tetcage_cycles_xml tools/cycles_xml/main.cpp)
  target_link_libraries(tetcage_cycles_xml PRIVATE tetcage_core)
  add_executable(tetcage_inspect tools/cage_inspector/main.cpp)
  target_link_libraries(tetcage_inspect PRIVATE tetcage_core)
  add_executable(tetcage_oracle_report tools/oracle_report/main.cpp)
  target_link_libraries(tetcage_oracle_report PRIVATE tetcage_core)
  add_executable(tetcage_image_differential tools/image_differential/main.cpp)
  target_link_libraries(tetcage_image_differential PRIVATE tetcage_core)
  add_executable(tetcage_boundary_probe tools/boundary_probe/main.cpp)
  target_link_libraries(tetcage_boundary_probe PRIVATE tetcage_core)
  add_executable(tetcage_benchmark tools/benchmark/main.cpp)
  target_link_libraries(tetcage_benchmark PRIVATE tetcage_core)
  add_executable(tetcage_cage_quality tools/cage_quality/main.cpp)
  target_link_libraries(tetcage_cage_quality PRIVATE tetcage_core)
  add_executable(tetcage_cage_animation tools/cage_animation/main.cpp)
  target_link_libraries(tetcage_cage_animation PRIVATE tetcage_core)
  add_executable(tetcage_cage_lods tools/cage_lods/main.cpp)
  target_link_libraries(tetcage_cage_lods PRIVATE tetcage_core)
  add_executable(tetcage_asset_migrate tools/asset_migrate/main.cpp)
  target_link_libraries(tetcage_asset_migrate PRIVATE tetcage_core)
  add_executable(tetcage_method_select tools/method_select/main.cpp)
  target_link_libraries(tetcage_method_select PRIVATE tetcage_core)
  add_executable(tetcage_tolerance_probe tools/tolerance_probe/main.cpp)
  target_link_libraries(tetcage_tolerance_probe PRIVATE tetcage_core)
  add_executable(tetcage_runtime_policy tools/runtime_policy/main.cpp)
  target_link_libraries(tetcage_runtime_policy PRIVATE tetcage_core)
  add_executable(tetcage_cage_generate tools/cage_generate/main.cpp)
  target_link_libraries(tetcage_cage_generate PRIVATE tetcage_core)
  add_executable(tetcage_cage_refine tools/cage_refine/main.cpp)
  target_link_libraries(tetcage_cage_refine PRIVATE tetcage_core)
  execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE TETCAGE_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  string(LENGTH "${TETCAGE_GIT_COMMIT}" TETCAGE_GIT_COMMIT_LENGTH)
  if(NOT TETCAGE_GIT_COMMIT_LENGTH EQUAL 40 OR
     NOT TETCAGE_GIT_COMMIT MATCHES "^[0-9a-f]+$")
    set(TETCAGE_GIT_COMMIT "0000000000000000000000000000000000000000")
  endif()
  execute_process(
    COMMAND git status --porcelain
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE TETCAGE_GIT_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
  if(TETCAGE_GIT_STATUS STREQUAL "")
    set(TETCAGE_GIT_DIRTY 0)
  else()
    set(TETCAGE_GIT_DIRTY 1)
  endif()
  target_compile_definitions(tetcage_benchmark PRIVATE
    TETCAGE_GIT_COMMIT="${TETCAGE_GIT_COMMIT}"
    TETCAGE_GIT_DIRTY=${TETCAGE_GIT_DIRTY}
    TETCAGE_SYSTEM_VERSION="${CMAKE_SYSTEM_VERSION}")
  if(TETCAGE_BUILD_TESTS)
    add_test(NAME tetcage.asset_compiler_cli
      COMMAND tetcage_asset_compiler
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage")
    set_tests_properties(tetcage.asset_compiler_cli PROPERTIES
      FIXTURES_SETUP compiled_asset
      PASS_REGULAR_EXPRESSION "\"generated_triangles\": 1")
    add_test(NAME tetcage.cycles_xml_fallback
      COMMAND tetcage_cycles_xml
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/cycles-fallback.xml")
    set_tests_properties(tetcage.cycles_xml_fallback PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "wrote Cycles fallback scene")
    add_test(NAME tetcage.asset_parser_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/tetcage_asset_parser.py"
        "$<TARGET_FILE:tetcage_asset_compiler>")
    set_tests_properties(tetcage.asset_parser_contract PROPERTIES
      PASS_REGULAR_EXPRESSION "tetcage asset parser contract: pass")
    add_test(NAME tetcage.asset_cache_smoke
      COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/asset_cache_smoke.sh"
        "$<TARGET_FILE:tetcage_asset_compiler>"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/cache-first.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/cache-second.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/asset-cache"
        "${CMAKE_CURRENT_BINARY_DIR}/asset-cache-reports")
    set_tests_properties(tetcage.asset_cache_smoke PROPERTIES
      PASS_REGULAR_EXPRESSION "\"byte_identical\":true")
    add_test(NAME tetcage.integration_probe_contract
      COMMAND bash "${CMAKE_CURRENT_SOURCE_DIR}/tests/integration_probe_contract.sh")
    add_test(NAME tetcage.cycles_verify_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/cycles_verify_contract.py")
    add_test(NAME tetcage.cycles_native_kernel_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/cycles_native_kernel_contract.py")
    set_tests_properties(tetcage.cycles_native_kernel_contract PROPERTIES
      SKIP_RETURN_CODE 77
      PASS_REGULAR_EXPRESSION "Cycles native local-kernel contract: (pass|skipped)")
    add_test(NAME tetcage.blender_native_deformation_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/blender_tetcage_deformation_contract.py")
    set_tests_properties(tetcage.blender_native_deformation_contract PROPERTIES
      SKIP_RETURN_CODE 77
      PASS_REGULAR_EXPRESSION "Blender native deformation contract: (pass|skipped)")
    add_test(NAME tetcage.blender_mixed_transparent_motion_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/blender_mixed_transparent_motion_contract.py")
    set_tests_properties(tetcage.blender_mixed_transparent_motion_contract PROPERTIES
      SKIP_RETURN_CODE 77
      PASS_REGULAR_EXPRESSION "Blender mixed-transparent motion contract: (pass|skipped)")
    add_test(NAME tetcage.blender_native_differential_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/blender_native_differential_contract.py")
    set_tests_properties(tetcage.blender_native_differential_contract PROPERTIES
      PASS_REGULAR_EXPRESSION "Blender native differential contract: pass")
    add_test(NAME tetcage.blender_area_precision_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/blender_area_precision_contract.py")
    set_tests_properties(tetcage.blender_area_precision_contract PROPERTIES
      PASS_REGULAR_EXPRESSION "Blender area-light precision contract: pass")
    add_test(NAME tetcage.cycles_native_volume_contract
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/cycles_native_volume_contract.py")
    set_tests_properties(tetcage.cycles_native_volume_contract PROPERTIES
      SKIP_RETURN_CODE 77
      PASS_REGULAR_EXPRESSION "Cycles native volume contract: (pass|skipped)")
    add_test(NAME tetcage.asset_inspector_cli
      COMMAND tetcage_inspect "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage")
    set_tests_properties(tetcage.asset_inspector_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"asset_format_version\": 1")
    add_test(NAME tetcage.oracle_report_cli
      COMMAND tetcage_oracle_report
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/oracle-report.json"
        --seed 81002718
        --random-rays 128)
    set_tests_properties(tetcage.oracle_report_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"exact_misses\": 0")
    add_test(NAME tetcage.image_differential_cli
      COMMAND tetcage_image_differential
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/image-differential"
        --width 24 --height 24)
    set_tests_properties(tetcage.image_differential_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"miss_classification\"")
    add_test(NAME tetcage.asset_corpus
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/run_asset_corpus.py"
        "$<TARGET_FILE:tetcage_asset_compiler>"
        "$<TARGET_FILE:tetcage_oracle_report>"
        "$<TARGET_FILE:tetcage_image_differential>"
        "$<TARGET_FILE:tetcage_boundary_probe>"
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/asset-corpus")
    set_tests_properties(tetcage.asset_corpus PROPERTIES
      PASS_REGULAR_EXPRESSION "\"accepted_assets\": 4")
    add_test(NAME tetcage.asset_corpus_manifest
      COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus_smoke.py"
        "${CMAKE_CURRENT_SOURCE_DIR}")
    set_tests_properties(tetcage.asset_corpus_manifest PROPERTIES
      PASS_REGULAR_EXPRESSION "\"cases\": 7")
    add_test(NAME tetcage.shared_boundary_compiler
      COMMAND tetcage_asset_compiler
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/corpus/shared-boundary.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/corpus/topology-sharing.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/shared-boundary.tetcage")
    set_tests_properties(tetcage.shared_boundary_compiler PROPERTIES
      FIXTURES_SETUP shared_boundary)
    add_test(NAME tetcage.shared_boundary_probe
      COMMAND tetcage_boundary_probe
        "${CMAKE_CURRENT_BINARY_DIR}/shared-boundary.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/shared-boundary.json")
    set_tests_properties(tetcage.shared_boundary_probe PROPERTIES
      FIXTURES_REQUIRED shared_boundary
      PASS_REGULAR_EXPRESSION "\"at_shared_vertex\": {\"passed\": true")
    add_test(NAME tetcage.benchmark_cli
      COMMAND tetcage_benchmark
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/benchmark.json"
        --scene unit-cli --seed 12345 --copies 4 --rays 128
        --warmup 1 --iterations 5 --motion 0.05 --visible 0.75)
    set_tests_properties(tetcage.benchmark_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"correctness_errors\": 0")
    add_test(NAME tetcage.benchmark_corruption_cli
      COMMAND tetcage_benchmark
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/benchmark-corrupt.json"
        --scene unit-corrupt --seed 12345 --copies 2 --rays 64
        --warmup 0 --iterations 2 --motion 0.02 --visible 1
        --inject-corruption)
    set_tests_properties(tetcage.benchmark_corruption_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      WILL_FAIL TRUE)
    add_test(NAME tetcage.cage_quality_cli
      COMMAND tetcage_cage_quality
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/cage-quality.json"
        --samples 4 --motion 0.05)
    set_tests_properties(tetcage.cage_quality_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"suitable\": true")
    add_test(NAME tetcage.cage_animation_cli
      COMMAND tetcage_cage_animation
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/cage-animation.json"
        --samples 6 --motion 0.05 --position-threshold 0.05 --normal-threshold 2.0)
    set_tests_properties(tetcage.cage_animation_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"clip_kind\": \"procedural_non_affine\"")
    add_test(NAME tetcage.cage_lods_cli
      COMMAND tetcage_cage_lods
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/lod-cage"
        "${CMAKE_CURRENT_BINARY_DIR}/cage-lods.json"
        --levels 2)
    set_tests_properties(tetcage.cage_lods_cli PROPERTIES
      PASS_REGULAR_EXPRESSION "\"parent_map_valid\": true")
    add_test(NAME tetcage.cage_generate_cli
      COMMAND tetcage_cage_generate
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/generated-baseline.cage")
    set_tests_properties(tetcage.cage_generate_cli PROPERTIES
      FIXTURES_SETUP generated_cage)
    add_test(NAME tetcage.cage_generate_compiles
      COMMAND tetcage_asset_compiler
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/generated-baseline.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/generated-baseline.tetcage")
    set_tests_properties(tetcage.cage_generate_compiles PROPERTIES
      FIXTURES_REQUIRED generated_cage
      FIXTURES_SETUP generated_asset
      PASS_REGULAR_EXPRESSION "\"cage_tetrahedra\": 12")
    add_test(NAME tetcage.cage_refine_cli
      COMMAND tetcage_cage_refine
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/refined.cage" --levels 1)
    set_tests_properties(tetcage.cage_refine_cli PROPERTIES
      PASS_REGULAR_EXPRESSION "\"tetrahedra\":8"
      FIXTURES_SETUP refined_cage)
    add_test(NAME tetcage.cage_refine_compiles
      COMMAND tetcage_asset_compiler
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/assets/one-tet.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/refined.cage"
        "${CMAKE_CURRENT_BINARY_DIR}/refined.tetcage")
    set_tests_properties(tetcage.cage_refine_compiles PROPERTIES
      FIXTURES_REQUIRED refined_cage
      PASS_REGULAR_EXPRESSION "\"cage_tetrahedra\": 8")
    add_test(NAME tetcage.asset_fuzz_smoke
      COMMAND python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz_asset.py"
        "$<TARGET_FILE:tetcage_inspect>"
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/asset-fuzz-report.json")
    set_tests_properties(tetcage.asset_fuzz_smoke PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "")
    add_test(NAME tetcage.result_manifest_smoke
      COMMAND python3
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/result_manifest_smoke.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/results")
    set_tests_properties(tetcage.result_manifest_smoke PROPERTIES
      PASS_REGULAR_EXPRESSION "\"checked_manifests\":")
    add_test(NAME tetcage.result_report_smoke
      COMMAND python3
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/result_report.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/results"
        --json-out "${CMAKE_CURRENT_BINARY_DIR}/reproducibility-summary.json"
        --markdown-out "${CMAKE_CURRENT_BINARY_DIR}/reproducibility-summary.md")
    set_tests_properties(tetcage.result_report_smoke PROPERTIES
      PASS_REGULAR_EXPRESSION "")
    add_test(NAME tetcage.asset_migration_cli
      COMMAND tetcage_asset_migrate
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet-migrated.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/migration.json")
    set_tests_properties(tetcage.asset_migration_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"byte_identical\":true")
    add_test(NAME tetcage.method_selection_cli
      COMMAND tetcage_method_select
        "${CMAKE_CURRENT_BINARY_DIR}/method-selection.json"
        1000 16 64 1000.0 0.0001 0.001 0.05 1 1 1)
    set_tests_properties(tetcage.method_selection_cli PROPERTIES
      PASS_REGULAR_EXPRESSION "\"representation\":\"tet_cage\"")
    add_test(NAME tetcage.tolerance_policy_cli
      COMMAND tetcage_tolerance_probe
        "${CMAKE_CURRENT_BINARY_DIR}/tolerance-policy.json" 1000000 0.25 1000 8)
    set_tests_properties(tetcage.tolerance_policy_cli PROPERTIES
      PASS_REGULAR_EXPRESSION "\"decision\":\"conservative_boundary\"")
    add_test(NAME tetcage.runtime_policy_cli
      COMMAND tetcage_runtime_policy
        "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
        "${CMAKE_CURRENT_BINARY_DIR}/runtime-policy.json")
    set_tests_properties(tetcage.runtime_policy_cli PROPERTIES
      FIXTURES_REQUIRED compiled_asset
      PASS_REGULAR_EXPRESSION "\"cases\":6")
  endif()

  add_executable(tetcage_vulkan_probe src/vulkan/capability_probe.cpp)
  target_link_libraries(tetcage_vulkan_probe PRIVATE tetcage_project_options)
  if(TETCAGE_ENABLE_VULKAN)
    find_package(Vulkan QUIET)
    if(Vulkan_FOUND)
      target_compile_definitions(tetcage_vulkan_probe PRIVATE TETCAGE_HAS_VULKAN=1)
      target_link_libraries(tetcage_vulkan_probe PRIVATE Vulkan::Vulkan)
    endif()
  endif()
  if(TETCAGE_BUILD_TESTS)
    add_test(NAME tetcage.vulkan_probe_json COMMAND tetcage_vulkan_probe)
    set_tests_properties(tetcage.vulkan_probe_json PROPERTIES
      PASS_REGULAR_EXPRESSION "\"schema_version\": 1")
  endif()

  if(APPLE AND TETCAGE_ENABLE_METAL)
    enable_language(OBJCXX)
    add_executable(tetcage_metal_probe src/metal/capability_probe.mm)
    target_link_libraries(tetcage_metal_probe PRIVATE tetcage_project_options
      "-framework Foundation" "-framework Metal")
    target_compile_options(tetcage_metal_probe PRIVATE -fobjc-arc)
    add_library(tetcage_metal_backend src/metal/fast_path.mm)
    target_link_libraries(tetcage_metal_backend PUBLIC tetcage_core
      PRIVATE "-framework Foundation" "-framework Metal")
    target_compile_options(tetcage_metal_backend PRIVATE -fobjc-arc)
    target_compile_definitions(tetcage_metal_backend PRIVATE
      TETCAGE_GIT_COMMIT="${TETCAGE_GIT_COMMIT}"
      TETCAGE_GIT_DIRTY=${TETCAGE_GIT_DIRTY}
      TETCAGE_SYSTEM_VERSION="${CMAKE_SYSTEM_VERSION}"
      TETCAGE_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}"
      TETCAGE_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}"
      TETCAGE_BUILD_TYPE="${CMAKE_BUILD_TYPE}")
    add_executable(tetcage_metal_fast_path tools/metal_fast_path/main.cpp)
    target_link_libraries(tetcage_metal_fast_path PRIVATE tetcage_metal_backend)
    if(TETCAGE_BUILD_TESTS)
      add_test(NAME tetcage.metal_probe_json COMMAND tetcage_metal_probe)
      set_tests_properties(tetcage.metal_probe_json PROPERTIES
        PASS_REGULAR_EXPRESSION "\"probe_kind\": \"metal\"")
      add_test(NAME tetcage.metal_fast_path
        COMMAND tetcage_metal_fast_path
          "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
          "${CMAKE_CURRENT_BINARY_DIR}/metal-fast-path.json"
          --copies 1 --rays 128 --motion 0.05 --compact --allow-unverified)
      set_tests_properties(tetcage.metal_fast_path PROPERTIES
        FIXTURES_REQUIRED compiled_asset
        PASS_REGULAR_EXPRESSION "\"api\": \"metal\"")
      add_test(NAME tetcage.metal_gpu_only
        COMMAND tetcage_metal_fast_path
          "${CMAKE_CURRENT_BINARY_DIR}/one-tet.tetcage"
          "${CMAKE_CURRENT_BINARY_DIR}/metal-gpu-only.json"
          --copies 1 --rays 128 --motion 0.05 --gpu-only --allow-unverified)
      set_tests_properties(tetcage.metal_gpu_only PROPERTIES
        FIXTURES_REQUIRED compiled_asset
        PASS_REGULAR_EXPRESSION "\"cpu_validation\": false")
      add_test(NAME tetcage.metal_wp1_evidence_smoke
        COMMAND "${CMAKE_COMMAND}" -E env
          "PYTHONDONTWRITEBYTECODE=1"
          python3 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/metal_wp1_evidence.py"
          --contract-smoke)
    endif()
  endif()
endif()
