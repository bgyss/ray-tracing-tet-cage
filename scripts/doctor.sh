#!/usr/bin/env bash
set -euo pipefail

failures=0

required() {
  local command_name="$1"
  local purpose="$2"
  if command -v "${command_name}" >/dev/null 2>&1; then
    printf 'required  %-18s ok  %s\n' "${command_name}" "${purpose}"
  else
    printf 'required  %-18s MISSING  %s\n' "${command_name}" "${purpose}" >&2
    failures=$((failures + 1))
  fi
}

optional() {
  local command_name="$1"
  local purpose="$2"
  if command -v "${command_name}" >/dev/null 2>&1; then
    printf 'optional  %-18s available  %s\n' "${command_name}" "${purpose}"
  else
    printf 'optional  %-18s unavailable  %s\n' "${command_name}" "${purpose}"
  fi
}

optional_host() {
  local command_name="$1"
  local host_path="$2"
  local purpose="$3"
  if command -v "${command_name}" >/dev/null 2>&1 || [[ -x "${host_path}" ]]; then
    printf 'optional  %-18s available  %s\n' "${command_name}" "${purpose}"
  else
    printf 'optional  %-18s unavailable  %s\n' "${command_name}" "${purpose}"
  fi
}

printf 'tetcage developer environment\n'
printf 'host: %s %s\n' "$(uname -s)" "$(uname -m)"
printf 'nix shell: %s\n\n' "${TETCAGE_NIX_SHELL:-0}"

required cmake "CMake project configuration"
required ctest "repo-native test execution"
required ninja "Nix build generator"
required clang++ "C++ and Objective-C++ compilation"
required clang-format "C++, Objective-C++, and Metal formatting"
required nixfmt "Nix expression formatting"
required python3 "schema, fuzz, and release utilities"
required jq "JSON evidence inspection"
required shellcheck "Bash static analysis"
required pkg-config "native dependency discovery"
required glslangValidator "GLSL to SPIR-V compilation"
required glslc "shaderc GLSL to SPIR-V compilation"
required spirv-as "SPIR-V assembly and validation tools"

printf '\n'
optional lldb "native debugger"
optional vulkaninfo "Vulkan loader/device report"
optional nvcc "CUDA compilation for the NVIDIA backend"
optional nvidia-smi "NVIDIA driver/device report"
optional_host blender "/Applications/Blender.app/Contents/MacOS/Blender" \
  "Cycles integration and background smoke tests"
optional_host prman "/Applications/Pixar/RenderManProServer-26.2/bin/prman" \
  "RenderMan integration smoke tests"

if [[ "$(uname -s)" == "Darwin" ]]; then
  if [[ "${CC:-}" == "/usr/bin/clang" && "${CXX:-}" == "/usr/bin/clang++" ]]; then
    printf 'required  %-18s ok  %s\n' "Xcode clang" "Apple frameworks and Objective-C++"
  else
    printf 'required  %-18s MISSING  %s\n' \
      "Xcode clang" "enter the Nix shell so CC/CXX select /usr/bin/clang" >&2
    failures=$((failures + 1))
  fi

  if command -v xcrun >/dev/null 2>&1 && xcrun metal --version >/dev/null 2>&1; then
    printf 'optional  %-18s available  %s\n' "Metal compiler" "offline MSL compilation"
  else
    printf 'optional  %-18s unavailable  %s\n' \
      "Metal compiler" "install the Xcode Metal Toolchain component for offline MSL"
  fi
fi

if ((failures != 0)); then
  printf '\ndoctor: %d required tool(s) missing\n' "${failures}" >&2
  exit 1
fi

printf '\ndoctor: required environment is ready\n'
