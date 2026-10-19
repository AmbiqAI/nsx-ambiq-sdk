"""Link real C++ constructor tables with the shipped startup and scripts."""

from __future__ import annotations

import os
import re
import subprocess
from pathlib import Path

import pytest

from tools.nsx_npu_compile_check import find_gcc


def run(*command: str, cwd: Path) -> str:
    result = subprocess.run(command, cwd=cwd, capture_output=True, text=True)
    assert result.returncode == 0, f"{' '.join(command)}\n{result.stdout}\n{result.stderr}"
    return result.stdout


@pytest.mark.parametrize("family", ("gcc", "atfe"))
@pytest.mark.parametrize("soc", ("apollo2", "apollo3", "apollo3p"))
def test_cpp_constructor_image(repo_root: Path, tmp_path: Path, family: str, soc: str) -> None:
    if family == "gcc":
        compiler = find_gcc(None)
        if compiler is None:
            if os.environ.get("NSX_REQUIRE_ARM_GCC") == "1":
                pytest.fail("arm-none-eabi-gcc required by NSX_REQUIRE_ARM_GCC=1")
            pytest.skip("Arm GNU toolchain is not installed")
        tool_bin = compiler.parent
        cc, cxx, objcopy, readelf, objdump = (
            str(tool_bin / f"arm-none-eabi-{tool}") for tool in ("gcc", "g++", "objcopy", "readelf", "objdump")
        )
    else:
        root = Path(os.environ.get("ATFE_ROOT", "/opt/toolchains/atfe"))
        tool_bin = root / "bin"
        if not (tool_bin / "clang++").is_file():
            if "ATFE_ROOT" in os.environ:
                pytest.fail(f"ATFE_ROOT does not contain bin/clang++: {root}")
            pytest.skip("ATfE is not installed; set ATFE_ROOT to enable")
        cc, cxx, objcopy, readelf, objdump = (
            str(tool_bin / tool) for tool in ("clang", "clang++", "llvm-objcopy", "llvm-readelf", "llvm-objdump")
        )

    run(
        "cmake", "-S", str(repo_root / "tests/fixtures/cpp-startup"), "-B", str(tmp_path),
        "-DCMAKE_SYSTEM_NAME=Generic", "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY",
        f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}",
        f"-DNSX_TEST_SDK_ROOT={repo_root}", f"-DNSX_TEST_BOARD={soc}_evb",
        f"-DNSX_TOOLCHAIN_FAMILY={family}", "-DCMAKE_BUILD_TYPE=Release",
        cwd=repo_root,
    )
    run("cmake", "--build", str(tmp_path), cwd=repo_root)
    elf = tmp_path / "constructors.elf"
    binary = tmp_path / "constructors.bin"
    run(objcopy, "-O", "binary", str(elf), str(binary), cwd=repo_root)
    assert 0 < binary.stat().st_size < 2 * 1024 * 1024, "constructor table inflated the flash image"

    sections = run(readelf, "-SW", str(elf), cwd=repo_root)
    table = re.search(r"\]\s+\.init_array\s+\S+\s+([0-9a-fA-F]+)\s+\S+\s+([0-9a-fA-F]+)", sections)
    assert table, sections
    address, size = (int(value, 16) for value in table.groups())
    assert 0 <= address < 0x200000 and size >= 4, "constructor table must be nonempty and in flash"

    symbols = run(readelf, "-sW", str(elf), cwd=repo_root)
    assert not re.search(r"\bUND\s+__(?:bothinit|preinit|init)_array_(?:start|end)\b", symbols), (
        "the selected C runtime's constructor-array bounds must resolve, including weak references\n" + symbols
    )
    table_file = tmp_path / "init-array.bin"
    run(objcopy, "--dump-section", f".init_array={table_file}", str(elf), cwd=repo_root)
    data = table_file.read_bytes()
    entries = [int.from_bytes(data[index:index + 4], "little") | 1 for index in range(0, len(data), 4)]
    priorities = []
    for name in ("priority_first", "priority_second"):
        symbol = re.search(rf"\d+:\s+([0-9a-fA-F]+)\s+\d+\s+FUNC\s+\w+\s+\w+\s+\S+\s+{name}\b", symbols)
        assert symbol, symbols
        priorities.append(entries.index(int(symbol.group(1), 16) | 1))
    assert priorities[0] < priorities[1], "constructor priorities must be sorted numerically (101 before 1000)"
    handler = re.search(r"\d+:\s+([0-9a-fA-F]+)\s+(\d+)\s+FUNC\s+\w+\s+\w+\s+\S+\s+Reset_Handler\b", symbols)
    assert handler, symbols
    start, length = int(handler.group(1), 16) & ~1, int(handler.group(2))
    # Symbol-bounded disassembly includes the inline-assembly loop labels:
    # llvm-objdump --disassemble-symbols stops at the first such label.
    reset = run(objdump, "-d", f"--start-address={start}", f"--stop-address={start + length}", str(elf), cwd=repo_root)
    assert re.search(r"\bbl\S*\s+[^\n]*<__libc_init_array>", reset), reset
    assert reset.index("<__libc_init_array>") < reset.index("<main>"), reset
