"""Compile real vendor-facing types using standalone SDK consumer flags."""
from __future__ import annotations

import json
from pathlib import Path
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize("family", ("gcc", "atfe", "armclang"))
@pytest.mark.parametrize("language", ("C", "CXX"))
@pytest.mark.parametrize("selection", ("default", "atfe", "conflicting-global"))
def test_vendor_layout_from_sdk_target(repo_root: Path, tmp_path: Path, family: str, language: str, selection: str) -> None:
    if family == "armclang":
        executable = shutil.which("armclang")
        if not executable:
            pytest.skip("ArmClang unavailable")
        compiler = Path(executable)
    else:
        compiler = compiler_for_test(family)
    artifact = "atfe" if selection == "atfe" else "acfe" if family == "armclang" else "gcc"
    short = artifact != "atfe"
    expected_enum, expected_wchar = (1 if short else 4), (2 if artifact == "acfe" else 4)
    source = tmp_path / ("abi.cpp" if language == "CXX" else "abi.c")
    assertion = "static_assert" if language == "CXX" else "_Static_assert"
    source.write_text(f'''#include "am_mcu_apollo.h"
#include <stddef.h>
{assertion}(sizeof(am_hal_pdm_mclkdiv_e) == {expected_enum}, "selected vendor enum ABI");
{assertion}(sizeof(wchar_t) == {expected_wchar}, "selected vendor wchar ABI");
{assertion}(sizeof(am_hal_pdm_config_t) == {28 if short else 56}, "real PDM layout");
{assertion}(offsetof(am_hal_pdm_config_t, ui32DecimationRate) == {8 if short else 20}, "real PDM offset");
uint32_t vendor_call(void *handle) {{
    static am_hal_pdm_config_t config;
    return am_hal_pdm_configure(handle, &config);
}}
''')
    prelude = ["set(CMAKE_EXPORT_COMPILE_COMMANDS ON)"]
    if language == "CXX":
        prelude.append("enable_language(CXX)")
    if selection == "atfe":
        prelude.append("set(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN atfe)")
    if selection == "conflicting-global":
        prelude.append(f'set(CMAKE_{language}_FLAGS "-fno-short-enums -fshort-wchar")')
    result = configure_contract_project(repo_root, tmp_path, "apollo510_evb", family,
        prelude=tuple(prelude), post_board_include=(
            f'add_library(abi OBJECT "{source.as_posix()}")',
            "target_link_libraries(abi PRIVATE nsx::ambiq_hal)",))
    assert result.returncode == 0, result.stdout
    build = tmp_path / family / "apollo510_evb/build"
    entries = json.loads((build / "compile_commands.json").read_text())
    selected = [entry for entry in entries if entry["file"] == str(source)]
    assert len(selected) == 1
    command = shlex.split(selected[0]["command"])
    command[0] = str(compiler.parent / "arm-none-eabi-g++") if family == "gcc" and language == "CXX" else str(compiler)
    obj = tmp_path / "abi.o"
    command[command.index("-o") + 1] = str(obj)
    result = subprocess.run(command, cwd=selected[0]["directory"], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    # Use the selected real archive, force its PDM implementation into a
    # relocatable ELF and require ABI warnings to be fatal. No MMIO executes.
    gcc = compiler_for_test("gcc")
    archive = repo_root / f"modules/nsx-ambiqsuite/sdk/lib/{artifact}/apollo510/libam_hal.a"
    linked = tmp_path / "vendor-linked.o"
    result = subprocess.run([str(gcc.parent / "arm-none-eabi-ld"), "-r", "--fatal-warnings", "-z", "noexecstack",
        "-u", "am_hal_pdm_configure", str(obj), str(archive), "-o", str(linked)], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    symbols = subprocess.check_output([str(gcc.parent / "arm-none-eabi-nm"), str(linked)], text=True)
    assert " T am_hal_pdm_configure" in symbols


def test_late_artifact_change_is_rejected(repo_root: Path, tmp_path: Path) -> None:
    result = configure_contract_project(repo_root, tmp_path, "apollo510_evb", "atfe",
        post_board_include=("set(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN atfe)",))
    assert result.returncode != 0
    assert "selection changed after SoC ABI flags" in result.stdout
