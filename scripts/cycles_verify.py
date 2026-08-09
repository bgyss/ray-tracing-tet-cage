#!/usr/bin/env python3
"""Verify the pinned standalone Cycles and Blender development environments."""

from __future__ import annotations

import json
import os
import pathlib
import re
import shlex
import subprocess
import sys
import tempfile
from typing import Any


DEFAULT_CYCLES_REVISION = "97dbe6f57cdf4ede2d2b75ebdda507c8712edb7a"
DEFAULT_CYCLES_DEP_REVISION = "5a140a8ccc8c070221b1b06e2c6f89f136c5758d"
DEFAULT_BLENDER_REVISION = "4a09c19bea7bd2800d85f018280b2dc62e654e51"
DEFAULT_BLENDER_DEP_REVISION = "a76ef917b4849ba2b1b1deb1a643e131a884a63b"
DEFAULT_LFS_FILE_COUNT = 622


class CommandResult:
    def __init__(
        self,
        args: list[str],
        cwd: pathlib.Path | None,
        env: dict[str, str] | None = None,
    ) -> None:
        self.args = args
        self.cwd = cwd
        try:
            process = subprocess.run(
                args,
                cwd=cwd,
                check=False,
                text=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
            )
        except OSError as error:
            self.returncode = 127
            self.stdout = ""
            self.stderr = str(error)
        else:
            self.returncode = process.returncode
            self.stdout = process.stdout
            self.stderr = process.stderr

    @property
    def command(self) -> str:
        return shlex.join(self.args)

    @property
    def output(self) -> str:
        return self.stdout.strip() or self.stderr.strip()


def run(
    args: list[str],
    cwd: pathlib.Path | None = None,
    env: dict[str, str] | None = None,
) -> CommandResult:
    return CommandResult(args, cwd, env)


def env_path(name: str, default: str) -> pathlib.Path:
    return pathlib.Path(os.environ.get(name, default)).expanduser()


def env_revision(name: str, default: str) -> str:
    return os.environ.get(name, default)


def env_count(name: str) -> int:
    try:
        return int(os.environ.get(name, str(DEFAULT_LFS_FILE_COUNT)))
    except ValueError as error:
        raise SystemExit(f"{name} must be an integer") from error


def tail(text: str, lines: int = 8) -> str:
    values = [line for line in text.splitlines() if line.strip()]
    return "\n".join(values[-lines:])


def record(
    status: str,
    detail: str,
    *,
    command: str = "",
    actual: str | None = None,
    expected: str | None = None,
    **extra: Any,
) -> dict[str, Any]:
    result: dict[str, Any] = {
        "status": status,
        "passed": status == "passed",
        "detail": detail,
    }
    if command:
        result["command"] = command
    if actual is not None:
        result["actual"] = actual
    if expected is not None:
        result["expected"] = expected
    result.update(extra)
    return result


def git_value(root: pathlib.Path, *args: str) -> tuple[CommandResult, str]:
    result = run(["git", *args], root)
    return result, result.stdout.strip()


def check_layout(root: pathlib.Path, required: list[str]) -> dict[str, Any]:
    missing = [relative for relative in required if not (root / relative).exists()]
    if not root.exists() or not root.is_dir():
        return record("blocked", f"source root is missing: {root}")
    if missing:
        return record("blocked", f"required paths are missing: {', '.join(missing)}")
    return record("passed", "required source layout is present")


def check_revision(root: pathlib.Path, expected: str) -> dict[str, Any]:
    result, actual = git_value(root, "rev-parse", "HEAD")
    if result.returncode != 0:
        return record("blocked", result.output or "unable to read Git revision", command=result.command)
    if actual != expected:
        return record(
            "failed",
            "Git revision does not match the pinned integration revision",
            command=result.command,
            actual=actual,
            expected=expected,
        )
    return record("passed", "Git revision matches the pinned integration revision", command=result.command, actual=actual, expected=expected)


def check_clean(root: pathlib.Path) -> dict[str, Any]:
    result = run(["git", "status", "--porcelain=v1", "--untracked-files=all"], root)
    if result.returncode != 0:
        return record("blocked", result.output or "unable to inspect Git status", command=result.command)
    if result.stdout.strip():
        return record("failed", f"working tree is dirty:\n{tail(result.stdout, 12)}", command=result.command)
    return record("passed", "working tree is clean", command=result.command)


def check_lfs(root: pathlib.Path, expected_count: int) -> dict[str, Any]:
    result = run(["git", "lfs", "ls-files", "--long"], root)
    if result.returncode != 0:
        return record("blocked", result.output or "Git LFS is unavailable", command=result.command)
    entries = [line.split(maxsplit=2) for line in result.stdout.splitlines() if line.strip()]
    missing: list[str] = []
    for entry in entries:
        if len(entry) >= 3:
            hydrated = entry[1] == "*"
            path = entry[2]
        else:
            hydrated = False
            path = "<malformed>"
        if not hydrated:
            missing.append(path)
    count = len(entries)
    if count != expected_count:
        return record(
            "failed",
            f"Git LFS file count is {count}, expected {expected_count}",
            command=result.command,
            actual=str(count),
            expected=str(expected_count),
            hydrated_count=count - len(missing),
            missing_files=missing[:12],
        )
    if missing:
        return record(
            "failed",
            f"{len(missing)} Git LFS paths are not hydrated",
            command=result.command,
            lfs_file_count=count,
            hydrated_count=count - len(missing),
            missing_files=missing[:12],
        )
    return record(
        "passed",
        f"all {count} Git LFS paths are hydrated",
        command=result.command,
        lfs_file_count=count,
        hydrated_count=count,
    )


def check_cycles_tests(build_root: pathlib.Path, binary: pathlib.Path) -> tuple[dict[str, Any], dict[str, Any]]:
    if not build_root.is_dir():
        blocked = record("blocked", f"Cycles build directory is missing: {build_root}")
        return blocked, blocked
    ctest = run(["ctest", "--test-dir", str(build_root), "--output-on-failure"])
    if ctest.returncode != 0:
        ctest_diagnostics = f"{ctest.stdout}\n{ctest.stderr}"
        if "Operation not permitted" in ctest_diagnostics or "Cannot create log file" in ctest_diagnostics:
            tests = check_cycles_tests_in_writable_mirror(build_root, ctest)
        else:
            tests = record(
                "failed",
                tail(ctest.output) or "Cycles CTest failed",
                command=ctest.command,
                returncode=ctest.returncode,
                stdout=tail(ctest.stdout),
                stderr=tail(ctest.stderr),
            )
    else:
        tests = record("passed", tail(ctest.output) or "Cycles CTest passed", command=ctest.command)
    if not binary.is_file() or not os.access(binary, os.X_OK):
        runtime = record("blocked", f"Cycles executable is missing or not executable: {binary}")
        return tests, runtime
    runtime_result = run([str(binary), "--list-devices"])
    if runtime_result.returncode != 0:
        runtime = record("failed", runtime_result.output or "Cycles device listing failed", command=runtime_result.command)
    elif "CPU" not in runtime_result.stdout:
        runtime = record("failed", "Cycles device listing did not report CPU", command=runtime_result.command, output=tail(runtime_result.stdout))
    else:
        runtime = record("passed", tail(runtime_result.stdout), command=runtime_result.command)
    return tests, runtime


def check_cycles_tests_in_writable_mirror(
    build_root: pathlib.Path, original: CommandResult
) -> dict[str, Any]:
    """Run the configured upstream CTest test without mutating an external build tree.

    CTest always writes ``Testing/Temporary``.  The host verifier is often run
    in a sandbox where the pinned checkout is readable but not writable, so
    mirror the generated root/src/app test metadata into a temporary directory
    and keep the actual test command (and executable) pinned to the original
    build.  This preserves the upstream test definition while making the
    verification lane read-only with respect to the source/build checkout.
    """

    app_testfile = build_root / "src/app/CTestTestfile.cmake"
    try:
        app_contents = app_testfile.read_text()
    except OSError as error:
        return record(
            "failed",
            f"CTest metadata mirror is unavailable: {error}",
            command=original.command,
            returncode=original.returncode,
        )
    if not re.search(r"add_test\(cycles_version\s+", app_contents):
        return record(
            "failed",
            "CTest metadata mirror could not find the cycles_version test",
            command=original.command,
            returncode=original.returncode,
        )

    with tempfile.TemporaryDirectory(prefix="tetcage-cycles-ctest-") as raw:
        mirror = pathlib.Path(raw)
        (mirror / "src/app").mkdir(parents=True)
        (mirror / "CTestTestfile.cmake").write_text('subdirs("src")\n')
        (mirror / "src/CTestTestfile.cmake").write_text('subdirs("app")\n')
        (mirror / "src/app/CTestTestfile.cmake").write_text(app_contents)
        mirrored = run(["ctest", "--test-dir", str(mirror), "--output-on-failure"])
        if mirrored.returncode != 0:
            return record(
                "failed",
                tail(mirrored.output) or "Cycles CTest failed in writable metadata mirror",
                command=mirrored.command,
                returncode=mirrored.returncode,
                original_returncode=original.returncode,
            )
        detail = "Cycles CTest passed in a writable metadata mirror; the original build tree was read-only"
        normalized_output = mirrored.output.replace(str(mirror), "<writable metadata mirror>")
        return record(
            "passed",
            f"{detail}\n{tail(normalized_output)}",
            command="ctest --test-dir <writable metadata mirror> --output-on-failure",
            fallback=True,
            original_command=original.command,
            original_returncode=original.returncode,
        )


def cmake_cache_values(cache_path: pathlib.Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in cache_path.read_text().splitlines():
        if not line or line.startswith("//") or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.split(":", 1)[0]] = value
    return values


def check_blender_debug_cache(build_root: pathlib.Path) -> dict[str, Any]:
    cache_path = build_root / "CMakeCache.txt"
    if not cache_path.is_file():
        return record("blocked", f"Blender CMake cache is missing: {cache_path}")
    values = cmake_cache_values(cache_path)
    expected = {
        "CMAKE_BUILD_TYPE": "Debug",
        "CMAKE_C_COMPILER": "/usr/bin/clang",
        "CMAKE_CXX_COMPILER": "/usr/bin/clang++",
        "CMAKE_CXX_FLAGS": "-Wno-error=unguarded-availability-new",
        "CMAKE_C_FLAGS": "-Wno-error=unguarded-availability-new",
        "CMAKE_GENERATOR": "Unix Makefiles",
        "CMAKE_OSX_ARCHITECTURES": "arm64",
        "CMAKE_OSX_DEPLOYMENT_TARGET": "11.2",
        "WITH_BLENDER": "ON",
        "WITH_CYCLES": "ON",
        "WITH_CYCLES_OSL": "ON",
        "WITH_CYCLES_DEVICE_METAL": "ON",
        "WITH_CYCLES_DEVICE_CUDA": "OFF",
        "WITH_CYCLES_DEVICE_OPTIX": "OFF",
    }
    mismatches = {
        key: {"actual": values.get(key), "expected": value}
        for key, value in expected.items()
        if values.get(key) != value
    }
    if mismatches:
        return record("failed", "Blender debug cache does not match the verification profile", mismatches=mismatches)
    return record("passed", "Blender developer/debug cache has UI/Cycles profile", profile=expected)


def check_blender_runtime(binary: pathlib.Path, smoke_script: pathlib.Path) -> dict[str, Any]:
    if not binary.is_file() or not os.access(binary, os.X_OK):
        return record("blocked", f"Blender executable is missing or not executable: {binary}")
    if not smoke_script.is_file():
        return record("blocked", f"Blender smoke script is missing: {smoke_script}")

    version = run([str(binary), "--version"])
    if version.returncode != 0:
        return record(
            "failed",
            version.output or "Blender executable did not start",
            command=version.command,
        )

    with tempfile.TemporaryDirectory(prefix="tetcage-blender-verify-") as raw:
        root = pathlib.Path(raw)
        user_config = root / "config"
        user_scripts = root / "scripts"
        user_config.mkdir()
        user_scripts.mkdir()
        smoke_output = root / "smoke.json"
        environment = os.environ.copy()
        environment.update(
            {
                "BLENDER_USER_CONFIG": str(user_config),
                "BLENDER_USER_SCRIPTS": str(user_scripts),
                "TETCAGE_BLENDER_SMOKE_OUTPUT": str(smoke_output),
            }
        )
        smoke = run(
            [
                str(binary),
                "--background",
                "--factory-startup",
                "--python",
                str(smoke_script),
                "--python-exit-code",
                "7",
            ],
            env=environment,
        )
        if smoke.returncode != 0:
            return record(
                "failed",
                tail(smoke.output) or "Blender background UI/Cycles smoke failed",
                command=smoke.command,
                version=tail(version.output, 1),
            )
        try:
            payload = json.loads(smoke_output.read_text())
        except (OSError, json.JSONDecodeError) as error:
            return record(
                "failed",
                f"Blender smoke did not produce valid JSON: {error}",
                command=smoke.command,
                version=tail(version.output, 1),
                output=tail(smoke.output),
            )
    if payload.get("status") != "passed":
        return record(
            "failed",
            "Blender smoke reported a failed UI/Cycles profile",
            command=smoke.command,
            version=tail(version.output, 1),
            smoke=payload,
        )
    return record(
        "passed",
        "Blender executable started and the isolated UI/Cycles Python smoke passed",
        command=smoke.command,
        version=tail(version.output, 1),
        smoke=payload,
    )


def check_file(path: pathlib.Path, detail: str) -> dict[str, Any]:
    if not path.is_file():
        return record("blocked", f"{detail} is missing: {path}")
    return record("passed", f"{detail} is present: {path}", path=str(path))


def tool_version(args: list[str]) -> str:
    result = run(args)
    if result.returncode != 0:
        return "unavailable"
    return (result.stdout or result.stderr).splitlines()[0] if (result.stdout or result.stderr) else "unknown"


def main() -> int:
    cycles_root = env_path("CYCLES_SOURCE_ROOT", "/Users/briangyss/src/cycles")
    blender_root = env_path("BLENDER_SOURCE_ROOT", "/Users/briangyss/src/blender")
    cycles_dependency = cycles_root / "lib/macos_arm64"
    blender_dependency = blender_root / "lib/macos_arm64"
    cycles_build = env_path("CYCLES_BUILD_DIR", str(cycles_root / "build-tetcage-cpu-clang"))
    cycles_binary = env_path("CYCLES_BINARY", str(cycles_build / "bin/cycles"))
    blender_build = env_path("BLENDER_BUILD_DIR", "/Users/briangyss/src/build_blender_tetcage_debug_make")
    blender_binary = env_path(
        "BLENDER_BINARY",
        str(blender_build / "bin/Blender.app/Contents/MacOS/Blender"),
    )
    blender_smoke_script = env_path(
        "BLENDER_SMOKE_SCRIPT",
        str(pathlib.Path(__file__).resolve().with_name("blender_ui_smoke.py")),
    )
    blender_cycles_library = env_path(
        "BLENDER_CYCLES_LIBRARY", str(blender_build / "lib/libbf_intern_cycles.a")
    )

    checks: dict[str, dict[str, Any]] = {}
    checks["cycles_layout"] = check_layout(cycles_root, ["CMakeLists.txt", "src/device/metal", "src/device/optix"])
    checks["cycles_revision"] = check_revision(cycles_root, env_revision("CYCLES_REVISION", DEFAULT_CYCLES_REVISION))
    checks["cycles_clean"] = check_clean(cycles_root)
    checks["cycles_dependency_revision"] = check_revision(
        cycles_dependency, env_revision("CYCLES_DEP_REVISION", DEFAULT_CYCLES_DEP_REVISION)
    )
    checks["cycles_dependency_clean"] = check_clean(cycles_dependency)
    checks["cycles_lfs"] = check_lfs(cycles_dependency, env_count("CYCLES_DEP_LFS_FILES"))
    cycles_tests, cycles_runtime = check_cycles_tests(cycles_build, cycles_binary)
    checks["cycles_ctest"] = cycles_tests
    checks["cycles_runtime"] = cycles_runtime

    checks["blender_layout"] = check_layout(blender_root, ["CMakeLists.txt", "source/blender", "intern/cycles"])
    checks["blender_revision"] = check_revision(blender_root, env_revision("BLENDER_REVISION", DEFAULT_BLENDER_REVISION))
    checks["blender_clean"] = check_clean(blender_root)
    checks["blender_dependency_revision"] = check_revision(
        blender_dependency, env_revision("BLENDER_DEP_REVISION", DEFAULT_BLENDER_DEP_REVISION)
    )
    checks["blender_dependency_clean"] = check_clean(blender_dependency)
    checks["blender_lfs"] = check_lfs(blender_dependency, env_count("BLENDER_DEP_LFS_FILES"))
    checks["blender_debug_cache"] = check_blender_debug_cache(blender_build)
    checks["blender_cycles_library"] = check_file(
        blender_cycles_library, "Blender Cycles integration library"
    )
    checks["blender_runtime"] = check_blender_runtime(blender_binary, blender_smoke_script)

    statuses = {check["status"] for check in checks.values()}
    if statuses == {"passed"}:
        status = "passed"
        exit_code = 0
    elif "failed" in statuses:
        status = "failed"
        exit_code = 1
    else:
        status = "blocked"
        exit_code = 2

    payload = {
        "schema_version": 1,
        "kind": "cycles_verification",
        "status": status,
        "roots": {
            "cycles": str(cycles_root),
            "blender": str(blender_root),
            "cycles_build": str(cycles_build),
            "blender_build": str(blender_build),
            "blender_binary": str(blender_binary),
        },
        "toolchain": {
            "git": tool_version(["git", "--version"]),
            "git_lfs": tool_version(["git", "lfs", "version"]),
            "cmake": tool_version(["cmake", "--version"]),
            "ctest": tool_version(["ctest", "--version"]),
            "python": tool_version(["python3", "--version"]),
        },
        "checks": checks,
        "claim_boundary": "A passed result proves the pinned source/dependency trees, hydrated LFS payloads, standalone Cycles CPU test/runtime, a full Blender developer/debug executable with installed resources, and an isolated Blender Python/UI/Cycles smoke. It does not prove a rendered tet-cage scene, MetalRT, OptiX, or device performance integration.",
    }
    encoded = json.dumps(payload, indent=2, sort_keys=True) + "\n"
    output_path = os.environ.get("CYCLES_VERIFY_OUTPUT")
    if output_path:
        pathlib.Path(output_path).expanduser().write_text(encoded)
    sys.stdout.write(encoded)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
