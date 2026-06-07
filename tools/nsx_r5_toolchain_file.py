#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
from pathlib import Path


TOOLCHAIN_FAMILIES = ("gcc", "atfe", "armclang")


def default_root(env_name: str, fallback: str) -> Path:
    return Path(os.environ.get(env_name, fallback))


def cmake_quote(value: Path | str) -> str:
    return str(value).replace("\\", "/")


def toolchain_text(
    toolchain_family: str,
    *,
    gcc_root: Path,
    atfe_root: Path,
    acfe_root: Path,
) -> str:
    if toolchain_family == "gcc":
        root = cmake_quote(gcc_root)
        return f"""set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m55)
set(CMAKE_C_COMPILER {root}/bin/arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER {root}/bin/arm-none-eabi-gcc)
set(CMAKE_AR {root}/bin/arm-none-eabi-ar)
set(CMAKE_OBJCOPY {root}/bin/arm-none-eabi-objcopy)
set(CMAKE_SIZE {root}/bin/arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles")
"""
    if toolchain_family == "atfe":
        root = cmake_quote(atfe_root)
        return f"""set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m55)
set(CMAKE_C_COMPILER {root}/bin/clang)
set(CMAKE_ASM_COMPILER {root}/bin/clang)
set(CMAKE_AR {root}/bin/llvm-ar)
set(CMAKE_RANLIB {root}/bin/llvm-ranlib)
set(CMAKE_OBJCOPY {root}/bin/llvm-objcopy)
set(CMAKE_SIZE {root}/bin/llvm-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_TARGET thumbv8.1m.main-unknown-none-eabihf)
set(CMAKE_ASM_COMPILER_TARGET thumbv8.1m.main-unknown-none-eabihf)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-nostartfiles")
"""
    if toolchain_family == "armclang":
        root = cmake_quote(acfe_root)
        return f"""set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR cortex-m55)
set(CMAKE_C_COMPILER {root}/bin/armclang)
set(CMAKE_ASM_COMPILER {root}/bin/armclang)
set(CMAKE_AR {root}/bin/armar)
set(CMAKE_LINKER {root}/bin/armlink)
set(CMAKE_OBJCOPY {root}/bin/fromelf)
set(CMAKE_SIZE {root}/bin/fromelf)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_C_COMPILER_TARGET arm-arm-none-eabi)
set(CMAKE_ASM_COMPILER_TARGET arm-arm-none-eabi)
set(CMAKE_C_FLAGS_INIT "--target=arm-arm-none-eabi")
set(CMAKE_ASM_FLAGS_INIT "--target=arm-arm-none-eabi")
"""
    raise ValueError(f"Unsupported toolchain family: {toolchain_family}")


def write_toolchain_file(
    output: Path,
    toolchain_family: str,
    *,
    gcc_root: Path | None = None,
    atfe_root: Path | None = None,
    acfe_root: Path | None = None,
) -> Path:
    text = toolchain_text(
        toolchain_family,
        gcc_root=gcc_root or default_root("GCC_ROOT", "/opt/toolchains/gcc"),
        atfe_root=atfe_root or default_root("ATFE_ROOT", "/opt/toolchains/atfe"),
        acfe_root=acfe_root or default_root("ACFE_ROOT", "/opt/toolchains/acfe"),
    )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(text, encoding="utf-8")
    return output


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Write an NSX R5 CMake toolchain file.")
    parser.add_argument("--toolchain-family", choices=TOOLCHAIN_FAMILIES, required=True)
    parser.add_argument("--output", type=Path, required=True, help="Path to write the CMake toolchain file.")
    parser.add_argument("--gcc-root", type=Path, default=default_root("GCC_ROOT", "/opt/toolchains/gcc"))
    parser.add_argument("--atfe-root", type=Path, default=default_root("ATFE_ROOT", "/opt/toolchains/atfe"))
    parser.add_argument("--acfe-root", type=Path, default=default_root("ACFE_ROOT", "/opt/toolchains/acfe"))
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    output = write_toolchain_file(
        args.output,
        args.toolchain_family,
        gcc_root=args.gcc_root,
        atfe_root=args.atfe_root,
        acfe_root=args.acfe_root,
    )
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
