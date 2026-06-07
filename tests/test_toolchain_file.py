from __future__ import annotations

from pathlib import Path

from tools.nsx_r5_toolchain_file import toolchain_text, write_toolchain_file


def test_gcc_toolchain_file_uses_arm_gnu_paths(tmp_path: Path) -> None:
    output = write_toolchain_file(
        tmp_path / "gcc.cmake",
        "gcc",
        gcc_root=Path("/toolchains/gcc"),
    )

    text = output.read_text(encoding="utf-8")
    assert "set(CMAKE_C_COMPILER /toolchains/gcc/bin/arm-none-eabi-gcc)" in text
    assert "set(CMAKE_EXE_LINKER_FLAGS_INIT \"-nostartfiles\")" in text


def test_atfe_toolchain_file_uses_llvm_paths(tmp_path: Path) -> None:
    output = write_toolchain_file(
        tmp_path / "atfe.cmake",
        "atfe",
        atfe_root=Path("/toolchains/atfe"),
    )

    text = output.read_text(encoding="utf-8")
    assert "set(CMAKE_C_COMPILER /toolchains/atfe/bin/clang)" in text
    assert "set(CMAKE_AR /toolchains/atfe/bin/llvm-ar)" in text
    assert "thumbv8.1m.main-unknown-none-eabihf" in text


def test_armclang_toolchain_file_uses_acfe_paths(tmp_path: Path) -> None:
    output = write_toolchain_file(
        tmp_path / "armclang.cmake",
        "armclang",
        acfe_root=Path("/toolchains/acfe"),
    )

    text = output.read_text(encoding="utf-8")
    assert "set(CMAKE_C_COMPILER /toolchains/acfe/bin/armclang)" in text
    assert "set(CMAKE_LINKER /toolchains/acfe/bin/armlink)" in text
    assert "set(CMAKE_C_FLAGS_INIT \"--target=arm-arm-none-eabi\")" in text


def test_unknown_toolchain_family_fails() -> None:
    try:
        toolchain_text(
            "iar",
            gcc_root=Path("/toolchains/gcc"),
            atfe_root=Path("/toolchains/atfe"),
            acfe_root=Path("/toolchains/acfe"),
        )
    except ValueError as error:
        assert "Unsupported toolchain family" in str(error)
    else:
        raise AssertionError("expected ValueError")
