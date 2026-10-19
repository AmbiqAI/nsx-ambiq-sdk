"""Reject layouts that exceed the documented Apollo330P/510L app apertures."""

from __future__ import annotations

import os
from pathlib import Path
import re
import subprocess

import pytest

from tools.nsx_npu_compile_check import find_gcc
from test_cmake_contract import configure_contract_project


SOCS = ("apollo330P", "apollo510L")
PROFILES = ("sbl", "nbl")
# DS-A330PS-0p9p0 / DS-A510LS-0p9p0, Table 4, p33. This is the
# application boundary for the AP330MPA/AP510DLA parts our boards select,
# rather than the 2 MiB physical NVM end (radio firmware occupies the rest).
APP_END = 0x00594000


def compiler_for_test(family: str) -> Path:
    if family == "gcc":
        compiler = find_gcc(None)
        if compiler is not None:
            return compiler
        if os.environ.get("NSX_REQUIRE_ARM_GCC") == "1":
            pytest.fail("Arm GNU compiler is required")
        pytest.skip("Arm GNU compiler is unavailable")
    root = Path(os.environ.get("ATFE_ROOT", "/opt/toolchains/atfe"))
    compiler = root / "bin/clang"
    if compiler.is_file():
        return compiler
    if "ATFE_ROOT" in os.environ:
        pytest.fail(f"ATFE_ROOT does not contain bin/clang: {root}")
    pytest.skip("ATfE is unavailable")


@pytest.mark.parametrize("board", ("apollo330mP_evb", "apollo510dL_evb"))
@pytest.mark.parametrize("family", ("gcc", "atfe", "armclang"))
def test_small_m55_rejects_itcm_profile(repo_root: Path, tmp_path: Path, board: str, family: str) -> None:
    result = configure_contract_project(repo_root, tmp_path, board, family, prelude=("set(NSX_LINKER_PROFILE itcm)",))
    assert result.returncode != 0, "A nonexistent ITCM must not be selectable"
    assert "not supported" in result.stdout


@pytest.mark.parametrize("soc", SOCS)
@pytest.mark.parametrize("profile", PROFILES)
@pytest.mark.parametrize("family,extension", (("gcc", "ld"), ("armclang", "sct")))
def test_small_m55_linker_regions(repo_root: Path, soc: str, profile: str, family: str, extension: str) -> None:
    path = repo_root / f"modules/nsx-core/src/{soc}/{family}/linker_script_{profile}.{extension}"
    source = path.read_text()
    if family == "gcc":
        regions = {name: (int(origin, 0), int(length, 0)) for name, origin, length in re.findall(
            r"(\w+)\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(\w+),\s*LENGTH\s*=\s*(\w+)", source
        )}
    else:
        regions = {name: (int(origin, 0), int(length, 0)) for name, origin, length in re.findall(
            r"^\s*(MCU_\w+|SHARED_SRAM)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)", source, re.M
        )}
    assert "MCU_ITCM" not in regions, "These parts have no ITCM at address zero"
    origin, size = regions["MCU_MRAM"]
    assert origin == (0x00410000 if profile == "sbl" else 0x00400000)
    assert origin + size == APP_END, "Do not overwrite reserved radio firmware"
    for name, start, end in (("MCU_TCM", 0x20000000, 0x20040000), ("SHARED_SRAM", 0x20080000, 0x20240000)):
        origin, size = regions[name]
        assert origin == start and 0 < size <= end - start, (path, name, origin, size)


@pytest.mark.parametrize("soc", SOCS)
@pytest.mark.parametrize("family,extension", (("gcc", "ld"), ("armclang", "sct")))
def test_small_m55_does_not_ship_an_itcm_script(repo_root: Path, soc: str, family: str, extension: str) -> None:
    assert not (repo_root / f"modules/nsx-core/src/{soc}/{family}/linker_script_itcm_sbl.{extension}").exists()


@pytest.mark.parametrize("soc", SOCS)
@pytest.mark.parametrize("profile", PROFILES)
@pytest.mark.parametrize("allocation", ("small", "mram-overflow", "tcm-overflow", "sram-overflow"))
@pytest.mark.parametrize("family", ("gcc", "atfe"))
def test_small_m55_link_enforces_memory_bounds(repo_root: Path, tmp_path: Path, soc: str, profile: str, allocation: str, family: str) -> None:
    gcc = compiler_for_test(family)
    origin = 0x00410000 if profile == "sbl" else 0x00400000
    extra = {
        "small": '.section .dtcm_text,"ax"\n.thumb_func\nhot_function: bx lr\n',
        "mram-overflow": f'.section .rodata,"a"\n.space {APP_END - origin + 1}\n',
        "tcm-overflow": '.section .bss,"aw",%nobits\n.space 262145\n',
        "sram-overflow": '.section .sram_bss,"aw",%nobits\n.space 1835009\n',
    }[allocation]
    asm = tmp_path / "probe.s"
    asm.write_text('.syntax unified\n.thumb\n.section .text,"ax"\n.global Reset_Handler\n.thumb_func\nReset_Handler: b .\n' + extra)
    obj = tmp_path / "probe.o"
    target = ["--target=arm-none-eabi"] if family == "atfe" else []
    subprocess.run([str(gcc), *target, "-mcpu=cortex-m55", "-mthumb", "-mfloat-abi=hard", "-c", str(asm), "-o", str(obj)], check=True, capture_output=True)
    script = repo_root / f"modules/nsx-core/src/{soc}/gcc/linker_script_{profile}.ld"
    result = subprocess.run([str(gcc), *target, "-mcpu=cortex-m55", "-mthumb", "-mfloat-abi=hard", "-nostdlib", f"-T{script}", str(obj), "-o", str(tmp_path / "probe.elf")], text=True, capture_output=True)
    if allocation == "small":
        assert result.returncode == 0, result.stderr
    else:
        assert result.returncode != 0, f"{script} accepted {allocation}"
        assert any(word in result.stderr.lower() for word in ("overflow", "will not fit", "outside", "backwards")), result.stderr


@pytest.mark.parametrize("soc", SOCS)
@pytest.mark.parametrize("profile", PROFILES)
@pytest.mark.parametrize("family", ("gcc", "atfe"))
def test_small_m55_startup_links(repo_root: Path, tmp_path: Path, soc: str, profile: str, family: str) -> None:
    """Use the actual startup TU so NBL cannot retain incompatible copy symbols."""
    compiler = compiler_for_test(family)
    stub = tmp_path / "main.c"
    stub.write_text('''
volatile unsigned initialized_data = 0x12345678;
volatile unsigned zeroed_bss;
__attribute__((section(".dtcm_text"), noinline)) unsigned hot_function(void) {
    return 0x600DC0DE;
}
volatile unsigned memory_result;
void SystemInit(void) {}
void __libc_init_array(void) {}
int main(void) {
    memory_result = initialized_data == 0x12345678 && zeroed_bss == 0
        ? hot_function() : 0xBAD;
    for (;;) { __asm volatile("nop"); }
}
''')
    flags = ["-mcpu=cortex-m55", "-mthumb", "-mfloat-abi=hard", "-O1", "-DSTACK_SIZE=1024", "-DARMCM55"]
    if family == "atfe":
        flags.append("--target=arm-none-eabi")
    includes = [repo_root / "modules/nsx-cmsis-core/Include", repo_root / "modules/nsx-ambiqsuite/sdk/CMSIS/AmbiqMicro/Include"]
    flags.extend(f"-I{path}" for path in includes)
    startup = repo_root / f"modules/nsx-core/src/{soc}/gcc/startup_gcc.c"
    for source, name in ((startup, "startup"), (stub, "main")):
        result = subprocess.run([str(compiler), *flags, "-c", str(source), "-o", str(tmp_path / f"{name}.o")], capture_output=True, text=True)
        assert result.returncode == 0, result.stderr
    script = repo_root / f"modules/nsx-core/src/{soc}/gcc/linker_script_{profile}.ld"
    result = subprocess.run([str(compiler), *flags, "-nostdlib", f"-T{script}", str(tmp_path / "startup.o"), str(tmp_path / "main.o"), "-o", str(tmp_path / "memory.elf")], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
