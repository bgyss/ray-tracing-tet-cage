# Repository Guidelines

## Project Structure & Module Organization

Public C++20 interfaces live in `include/tetcage/`; implementations are grouped under `src/core`, `src/compiler`, `src/cpu_reference`, `src/metal`, and `src/vulkan`. Each executable has a small entry point in `tools/<tool>/main.cpp`. Tests are in `tests/`, with compact OBJ and cage fixtures under `tests/assets/`. Keep generated build output in `build/`. Documentation belongs in `docs/`, schemas in `schemas/`, and reproducible evidence in the appropriate `results/<category>/` directory.

## Build, Test, and Development Commands

Nix defines the pinned toolchain and mise provides the normal task interface:

```sh
MISE_DISABLE_VERSION_CHECK=1 mise run doctor  # inspect required and optional tools
MISE_DISABLE_VERSION_CHECK=1 mise run check   # format, lint, build, test, and diff checks
MISE_DISABLE_VERSION_CHECK=1 mise run report  # regenerate result summaries
```

On macOS, a direct build uses `cmake --preset dev`, `cmake --build --preset dev`, and `ctest --preset dev`. Use the `portable` preset elsewhere. Prefer `nix develop path:.` when invoking individual tools so untracked sources are included.

## Coding Style & Naming Conventions

Use C++20, two-space indentation, and the checked-in `.clang-format`. Namespaces, functions, variables, and files use `snake_case`; types use `PascalCase`; CMake targets use the `tetcage_` prefix. Keep public declarations in `include/tetcage` and implementation details in the matching `src` module. Run `mise run format` before submitting; `scripts/check.sh` also runs `nixfmt`, ShellCheck, and warning-enabled compilation.

## Testing & Evidence Guidelines

CTest is the primary test runner. Add focused cases to `tests/test_main.cpp`, CLI coverage through `add_test()` in `CMakeLists.txt`, and minimal deterministic fixtures to `tests/assets/`. Test names follow `tetcage.<feature>`. Run `mise run test` for the suite and `mise run check` for the full gate. Hardware probes and synthetic benchmarks must remain explicitly labeled; do not present them as production-scale or cross-platform proof. Record stable, reproducible outputs under `results/`.

## Commit & Pull Request Guidelines

Recent commits use short, imperative subjects such as `Add portable runtime safety policy`. Keep each commit scoped to one coherent change. Pull requests should explain the behavior and proof impact, list commands run, link relevant roadmap or issue context, and call out unverified hardware gates. Include updated result manifests or documentation when claims change; screenshots are only useful for visual integrations.
