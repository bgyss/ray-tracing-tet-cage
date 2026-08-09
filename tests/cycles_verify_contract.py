#!/usr/bin/env python3
"""Exercise the Cycles/Blender verification harness with real local fixtures."""

from __future__ import annotations

import json
import os
import pathlib
import subprocess
import tempfile


REPO_ROOT = pathlib.Path(__file__).resolve().parents[1]
VERIFY = REPO_ROOT / "scripts" / "cycles_verify.py"


def run(*args: str, cwd: pathlib.Path) -> str:
    result = subprocess.run(
        list(args),
        cwd=cwd,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return result.stdout.strip()


def init_repo(path: pathlib.Path, files: dict[str, str]) -> str:
    path.mkdir(parents=True)
    run("git", "init", "--quiet", cwd=path)
    for relative, content in files.items():
        target = path / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    run("git", "add", ".", cwd=path)
    run(
        "git",
        "-c",
        "user.name=Cycles verifier test",
        "-c",
        "user.email=cycles-verifier@example.invalid",
        "commit",
        "--quiet",
        "-m",
        "fixture",
        cwd=path,
    )
    return run("git", "rev-parse", "HEAD", cwd=path)


def add_dependency(parent: pathlib.Path, dependency: pathlib.Path, target: str) -> str:
    run(
        "git",
        "-c",
        "protocol.file.allow=always",
        "submodule",
        "add",
        "--quiet",
        str(dependency),
        target,
        cwd=parent,
    )
    run(
        "git",
        "-c",
        "user.name=Cycles verifier test",
        "-c",
        "user.email=cycles-verifier@example.invalid",
        "commit",
        "--quiet",
        "-m",
        "pin dependency",
        cwd=parent,
    )
    return run("git", "-C", str(parent / target), "rev-parse", "HEAD", cwd=parent)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="tetcage-cycles-verify-") as raw:
        root = pathlib.Path(raw)
        cycles_dependency = root / "cycles-dependency"
        blender_dependency = root / "blender-dependency"
        init_repo(cycles_dependency, {"README": "cycles"})
        init_repo(blender_dependency, {"README": "blender"})

        cycles = root / "cycles"
        cycles_revision = init_repo(
            cycles,
            {
                "CMakeLists.txt": "cmake_minimum_required(VERSION 3.25)\n",
                "src/device/metal/.keep": "",
                "src/device/optix/.keep": "",
            },
        )
        cycles_dep_revision = add_dependency(cycles, cycles_dependency, "lib/macos_arm64")
        cycles_revision = run("git", "rev-parse", "HEAD", cwd=cycles)

        blender = root / "blender"
        blender_revision = init_repo(
            blender,
            {
                "CMakeLists.txt": "cmake_minimum_required(VERSION 3.25)\n",
                "source/blender/.keep": "",
                "intern/cycles/.keep": "",
            },
        )
        blender_dep_revision = add_dependency(blender, blender_dependency, "lib/macos_arm64")
        blender_revision = run("git", "rev-parse", "HEAD", cwd=blender)

        cycles_build_source = root / "cycles-build-source"
        cycles_build_source.mkdir()
        (cycles_build_source / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.25)\n"
            "project(cycles_verifier_fixture NONE)\n"
            "enable_testing()\n"
            f"add_test(NAME cycles_version COMMAND {root / 'cycles-build' / 'bin' / 'cycles'} --list-devices)\n"
        )
        cycles_build = root / "cycles-build"
        run("cmake", "-S", str(cycles_build_source), "-B", str(cycles_build), cwd=root)
        binary = cycles_build / "bin" / "cycles"
        binary.parent.mkdir()
        binary.write_text("#!/bin/sh\nprintf 'Devices:\\n    CPU       Fixture\\n'\n")
        binary.chmod(0o755)

        blender_build = root / "blender-build"
        (blender_build).mkdir()
        (blender_build / "CMakeCache.txt").write_text(
            "CMAKE_BUILD_TYPE:STRING=Debug\n"
            "CMAKE_C_COMPILER:FILEPATH=/usr/bin/clang\n"
            "CMAKE_CXX_COMPILER:FILEPATH=/usr/bin/clang++\n"
            "CMAKE_GENERATOR:INTERNAL=Unix Makefiles\n"
            "WITH_CYCLES:BOOL=ON\n"
            "WITH_CYCLES_DEVICE_METAL:BOOL=ON\n"
            "WITH_CYCLES_DEVICE_CUDA:UNINITIALIZED=OFF\n"
            "WITH_CYCLES_DEVICE_OPTIX:UNINITIALIZED=OFF\n"
        )
        blender_cycles_library = blender_build / "lib" / "libbf_intern_cycles.a"
        blender_cycles_library.parent.mkdir()
        blender_cycles_library.write_bytes(b"fixture Cycles library")

        environment = os.environ.copy()
        environment.update(
            {
                "CYCLES_SOURCE_ROOT": str(cycles),
                "CYCLES_REVISION": cycles_revision,
                "CYCLES_DEP_REVISION": cycles_dep_revision,
                "CYCLES_DEP_LFS_FILES": "0",
                "CYCLES_BUILD_DIR": str(cycles_build),
                "CYCLES_BINARY": str(binary),
                "BLENDER_SOURCE_ROOT": str(blender),
                "BLENDER_REVISION": blender_revision,
                "BLENDER_DEP_REVISION": blender_dep_revision,
                "BLENDER_DEP_LFS_FILES": "0",
                "BLENDER_BUILD_DIR": str(blender_build),
                "BLENDER_CYCLES_LIBRARY": str(blender_cycles_library),
            }
        )
        result = subprocess.run(
            ["python3", str(VERIFY)],
            cwd=REPO_ROOT,
            check=False,
            env=environment,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if result.returncode != 0:
            raise AssertionError(
                f"verification harness failed ({result.returncode}):\n"
                f"stdout={result.stdout}\nstderr={result.stderr}"
            )
        payload = json.loads(result.stdout)
        if payload["checks"]["blender_cycles_library"]["status"] != "passed":
            raise AssertionError(f"Cycles Blender library was not verified:\n{result.stdout}")
        if '"status": "passed"' not in result.stdout:
            raise AssertionError(f"verification did not pass:\n{result.stdout}")
        print("cycles verification contract: pass")


if __name__ == "__main__":
    main()
