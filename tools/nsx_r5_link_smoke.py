#!/usr/bin/env python3
from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

from nsx_r5_toolchain_file import TOOLCHAIN_FAMILIES, write_toolchain_file


DEFAULT_BOARDS = (
    "apollo330mP_evb",
    "apollo510_evb",
    "apollo510b_evb",
    "apollo510dL_evb",
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def write_smoke_source(source_dir: Path, repo: Path, board: str, toolchain_family: str) -> None:
    source_dir.mkdir(parents=True, exist_ok=True)
    (source_dir / "main.c").write_text(
        "int main(void) {\n"
        "    return 0;\n"
        "}\n",
        encoding="utf-8",
    )
    (source_dir / "CMakeLists.txt").write_text(
        f"""cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0123)
    cmake_policy(SET CMP0123 NEW)
endif()

project(nsx_r5_link_smoke C)

include(GNUInstallDirs)
set(CMAKE_EXECUTABLE_SUFFIX ".elf")

set(NSX_ROOT "{repo.as_posix()}")
set(NSX_CMAKE_DIR "${{NSX_ROOT}}/cmake")
set(NSX_SDK_PROVIDER "ambiqsuite-r5")
set(NSX_TOOLCHAIN_FAMILY "{toolchain_family}")
set(NSX_AMBIQSUITE_VERSION "R5.2.0")
set(NSX_AMBIQSUITE_ROOT "${{NSX_ROOT}}/modules/nsx-ambiqsuite-r5/sdk")

include("${{NSX_ROOT}}/boards/{board}/board.cmake")

add_subdirectory("${{NSX_ROOT}}/modules/nsx-cmsis-core" nsx-cmsis-core)
add_subdirectory("${{NSX_ROOT}}/modules/nsx-ambiq-hal-r5" nsx-ambiq-hal-r5)
add_subdirectory("${{NSX_ROOT}}/modules/nsx-ambiq-bsp-r5" nsx-ambiq-bsp-r5)
add_subdirectory("${{NSX_ROOT}}/modules/nsx-soc-hal" nsx-soc-hal)

add_executable(smoke main.c)
target_link_libraries(smoke PRIVATE
    nsx::board
    nsx::ambiq_bsp
    nsx::soc_hal
)
""",
        encoding="utf-8",
    )


def run(command: list[str], *, cwd: Path | None = None) -> None:
    print("+ " + " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, check=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build minimal NSX R5 link smokes for staged boards.")
    parser.add_argument("--repo-root", type=Path, default=repo_root(), help="Path to nsx-ambiq-sdk checkout.")
    parser.add_argument("--build-root", type=Path, default=Path("build/link-smoke"), help="Build/output root.")
    parser.add_argument("--toolchain-family", choices=TOOLCHAIN_FAMILIES, default="armclang")
    parser.add_argument(
        "--toolchain-file",
        type=Path,
        help="CMake toolchain file to use for configure. Defaults to a generated file under build-root.",
    )
    parser.add_argument("--board", action="append", choices=DEFAULT_BOARDS, help="Board to build. May be repeated. Default: all staged boards.")
    parser.add_argument("--generator", help="Optional CMake generator, for example Ninja or Unix Makefiles.")
    parser.add_argument("--verbose", action="store_true", help="Enable verbose CMake build output.")
    parser.add_argument("--keep", action="store_true", help="Keep existing build-root contents instead of deleting them first.")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo = args.repo_root.expanduser().resolve()
    build_root = args.build_root.expanduser().resolve()
    toolchain_file = (
        args.toolchain_file.expanduser().resolve()
        if args.toolchain_file is not None
        else build_root / "toolchains" / f"{args.toolchain_family}.cmake"
    )
    boards = tuple(args.board or DEFAULT_BOARDS)

    if args.toolchain_file is not None and not toolchain_file.is_file():
        raise FileNotFoundError(toolchain_file)
    if not (repo / "modules" / "nsx-ambiqsuite-r5" / "sdk").is_dir():
        raise FileNotFoundError(repo / "modules" / "nsx-ambiqsuite-r5" / "sdk")

    if build_root.exists() and not args.keep:
        shutil.rmtree(build_root)
    build_root.mkdir(parents=True, exist_ok=True)
    if args.toolchain_file is None:
        write_toolchain_file(toolchain_file, args.toolchain_family)

    for board in boards:
        source_dir = build_root / "src" / board
        binary_dir = build_root / args.toolchain_family / board
        write_smoke_source(source_dir, repo, board, args.toolchain_family)

        configure = [
            "cmake",
            "-S",
            str(source_dir),
            "-B",
            str(binary_dir),
            f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
        ]
        if args.generator:
            configure.extend(["-G", args.generator])
        if args.verbose:
            configure.append("-DCMAKE_VERBOSE_MAKEFILE=ON")
        run(configure)

        build = ["cmake", "--build", str(binary_dir)]
        if args.verbose:
            build.append("--verbose")
        run(build)

        output = binary_dir / "smoke.elf"
        if not output.is_file():
            raise FileNotFoundError(output)
        print(f"built {output}", flush=True)

    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except subprocess.CalledProcessError as error:
        print(f"error: command failed with exit code {error.returncode}: {' '.join(error.cmd)}", file=sys.stderr)
        raise SystemExit(error.returncode)
