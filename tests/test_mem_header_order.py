"""Compile real memory/HAL headers and inspect placement, without hardware."""
from __future__ import annotations

import json
from pathlib import Path
import re
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from test_small_m55_memory import compiler_for_test
from tools.nsx_firmware_elf import memory_regions


BOARDS = (
    "apollo2_evb", "apollo3_evb", "apollo3p_evb", "apollo4p_evb", "apollo4l_evb",
    "apollo510_evb", "apollo510b_evb", "apollo510dL_evb", "apollo330mP_evb",
    "atomiq110_fpga_turbo",
)
LEGACY_BOARDS = {"apollo2_evb", "apollo3_evb", "apollo3p_evb"}


def symbol_sections(obj: Path, gcc: Path) -> dict[str, str]:
    table = subprocess.check_output([str(gcc.parent / "arm-none-eabi-objdump"), "-t", str(obj)], text=True)
    return {name: section for section, name in re.findall(
        r"^\w+\s+g\s+O\s+(\S+)\s+\w+\s+(?:\.hidden\s+)?(\w+)$", table, re.M)}


@pytest.mark.parametrize("board", BOARDS)
@pytest.mark.parametrize("family", ("gcc", "atfe", "armclang"))
def test_memory_header_order_and_sections(repo_root: Path, tmp_path: Path, board: str, family: str) -> None:
    if family == "armclang":
        if board == "apollo2_evb":
            pytest.skip("Apollo2 does not advertise ArmClang support")
        executable = shutil.which("armclang")
        if not executable:
            pytest.skip("ArmClang unavailable")
        compiler = Path(executable)
    else:
        compiler = compiler_for_test(family)
    gcc = compiler_for_test("gcc")
    sources = []
    setup = ["set(CMAKE_EXPORT_COMPILE_COMMANDS ON)", "enable_language(CXX)"]
    targets = ['file(WRITE "${CMAKE_BINARY_DIR}/linker-path.txt" "${NSX_LINKER_SCRIPT}")']
    for language, extension in (("C", "c"), ("CXX", "cpp")):
        for first in ("nsx", "hal"):
            source = tmp_path / f"{language}_{first}.{extension}"
            first_header = "nsx_mem.h" if first == "nsx" else "am_mcu_apollo.h"
            second_header = "am_mcu_apollo.h" if first == "nsx" else "nsx_mem.h"
            # Older HALs do not define AM_SHARED_RW; load NSX before declaring
            # the compatibility symbol in that control case.
            before = first == "nsx" or board not in LEGACY_BOARDS
            declaration = 'AM_SHARED_RW volatile uint32_t legacy_before = 0x11223344;\n'
            source.write_text(f'#include "{first_header}"\n'
                + (declaration if before else "")
                + f'#include "{second_header}"\n'
                + ("" if before else declaration)
                + 'AM_SHARED_RW volatile uint32_t legacy_after = 0x55667788;\n'
                + 'NSX_MEM_SRAM volatile uint32_t native_value = 0xAABBCCDD;\n'
                + 'void Reset_Handler(void) { for (;;) {} }\n')
            name = source.stem
            targets.extend((f'add_library({name} OBJECT "{source.as_posix()}")',
                            f'target_link_libraries({name} PRIVATE nsx::mem)',
                            f'target_compile_options({name} PRIVATE -Wall -Werror)'))
            sources.append((language, first, source))
    result = configure_contract_project(repo_root, tmp_path, board, family,
        modules=("nsx-core",), prelude=tuple(setup), post_board_include=tuple(targets))
    assert result.returncode == 0, result.stdout
    build = tmp_path / family / board / "build"
    commands = json.loads((build / "compile_commands.json").read_text())
    script = Path((build / "linker-path.txt").read_text())
    for language, first, source in sources:
        entry, = [entry for entry in commands if entry["file"] == str(source)]
        command = shlex.split(entry["command"])
        command[0] = str(compiler.parent / "arm-none-eabi-g++") if family == "gcc" and language == "CXX" else str(compiler)
        obj = source.with_suffix(".o")
        command[command.index("-o") + 1] = str(obj)
        if board == "atomiq110_fpga_turbo":
            # Existing vendor FPGA/TODO #warnings; macro diagnostics stay fatal.
            command.append("-Wno-error=cpp")
        if family == "gcc" and language == "CXX" and board in {"apollo4p_evb", "apollo4l_evb", "atomiq110_fpga_turbo"}:
            # Existing am_hal_card.h signed/unsigned comparison, unrelated to placement.
            command.append("-Wno-error=sign-compare")
        result = subprocess.run(command, cwd=entry["directory"], text=True, capture_output=True)
        assert result.returncode == 0, result.stdout + result.stderr
        sections = symbol_sections(obj, gcc)
        expected = "SHARED_RW" if family == "armclang" else ".shared"
        for name in ("legacy_before", "legacy_after", "native_value"):
            if board in LEGACY_BOARDS:
                assert sections[name].startswith(".data"), (board, family, sections)
            else:
                assert sections[name] == (".shared" if name == "native_value" else expected), sections
        # Link a representative real Cortex-M55 image in both orders, checking
        # the SRAM address and the initialized bytes using the shipped script.
        if board == "apollo510_evb" and family in ("gcc", "atfe") and language == "C":
            elf = source.with_suffix(".elf")
            flags = [flag for flag in command if flag.startswith(("-mcpu=", "-mfpu=", "-mfloat-abi=", "--target=")) or flag == "-mthumb"]
            result = subprocess.run([str(compiler), *flags, "-nostdlib", f"-T{script}", str(obj), "-o", str(elf)], text=True, capture_output=True)
            assert result.returncode == 0, result.stdout + result.stderr
            symbols = subprocess.check_output([str(gcc.parent / "arm-none-eabi-nm"), "-n", str(elf)], text=True)
            addresses = {name: int(address, 16) for address, name in re.findall(r"^([0-9a-fA-F]+)\s+\S\s+(\S+)$", symbols, re.M)}
            start, size = memory_regions(script)["SHARED_SRAM"]
            for name in ("legacy_before", "legacy_after", "native_value"):
                assert start <= addresses[name] < start + size
            contents = source.with_suffix(".bin")
            subprocess.run([str(gcc.parent / "arm-none-eabi-objcopy"), "--dump-section", f".shared={contents}", str(elf)], check=True, capture_output=True)
            data = contents.read_bytes()
            for name, value in (("legacy_before", 0x11223344), ("legacy_after", 0x55667788), ("native_value", 0xAABBCCDD)):
                offset = addresses[name] - addresses["_ssdata"]
                assert int.from_bytes(data[offset:offset + 4], "little") == value


def test_memory_header_without_target_keeps_portable_fallback(repo_root: Path, tmp_path: Path) -> None:
    compiler = shutil.which("cc")
    if compiler is None:
        pytest.skip("host C compiler unavailable")
    source = tmp_path / "portable.c"
    source.write_text('#include "nsx_mem.h"\nAM_SHARED_RW int value = 1;\nNSX_MEM_SRAM int native_value = 2;\n')
    subprocess.run([compiler, "-Wall", "-Werror", "-I" + str(repo_root / "modules/nsx-core/includes-api"),
                    "-c", str(source), "-o", str(tmp_path / "portable.o")], check=True, capture_output=True)


@pytest.mark.parametrize("soc,part,api", (
    ("apollo4p", "AM_PART_APOLLO4P", "AM_PART_APOLLO4_API"),
    ("apollo4l", "AM_PART_APOLLO4L", "AM_PART_APOLLO4_API"),
    ("apollo510", "AM_PART_APOLLO510", "AM_PART_APOLLO5_API"),
    ("apollo510", "AM_PART_APOLLO510B", "AM_PART_APOLLO5_API"),
    ("apollo510L", "AM_PART_APOLLO510L", "AM_PART_APOLLO5_API"),
    ("apollo330P", "AM_PART_APOLLO330P", "AM_PART_APOLLO5_API"),
    ("atomiq110", "PART_atomiq110", "AM_PART_ATOMIQ11X_API"),
))
def test_iar_selector_redefinition_tokens(repo_root: Path, tmp_path: Path, soc: str, part: str, api: str) -> None:
    """Preprocessor compatibility only; this does not qualify an IAR compiler."""
    compiler = compiler_for_test("gcc")
    source = tmp_path / "iar_tokens.c"
    for headers in (("nsx_mem.h", "am_hal_global.h"), ("am_hal_global.h", "nsx_mem.h")):
        source.write_text('#include <stdint.h>\n' + ''.join(f'#include "{header}"\n' for header in headers))
        result = subprocess.run([str(compiler), "-E", "-Wall", "-Werror", "-Diar", f"-D{part}", f"-D{api}",
            "-I" + str(repo_root / "modules/nsx-core/includes-api"),
            "-I" + str(repo_root / f"modules/nsx-ambiqsuite/sdk/mcu/{soc}/hal"), str(source)], text=True, capture_output=True)
        assert result.returncode == 0, result.stderr
