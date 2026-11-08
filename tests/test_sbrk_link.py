"""Link real Arm/newlib C++ constructors against HAL archives (no hardware)."""
from __future__ import annotations

import os
from pathlib import Path
import re
import shlex
import struct
import subprocess

import pytest

from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize("family", ("gcc", "atfe"))
@pytest.mark.parametrize("soc", ("apollo330P", "apollo510"))
def test_newlib_constructor_uses_bounded_sbrk(
    repo_root: Path, tmp_path: Path, family: str, soc: str,
) -> None:
    """Production order plus -u selects NSX; released no--u baseline is unsafe.

    Uses GCC HAL also for ATfE, matching NSX's current artifact policy. Tests
    archive selection, not reset/hardware or C++ exception-pool policy. ATfE
    must use newlib, not its default LLVM libc. The existing CMake contract
    test guards production flag propagation. Pin the negative control to the
    old release so a future provider fix does not invalidate this regression.
    """
    compiler = compiler_for_test(family)
    gcc = compiler_for_test("gcc")
    cxx = compiler.parent / ("arm-none-eabi-g++" if family == "gcc" else "clang++")
    fixture = repo_root / "tests/fixtures/sbrk-link"
    relative_hal = f"modules/nsx-ambiqsuite/sdk/lib/gcc/{soc}/libam_hal.a"
    baseline = subprocess.run(["git", "show", f"v5.2.29:{relative_hal}"],
                              cwd=repo_root, capture_output=True)
    if baseline.returncode:
        reason = "sbrk negative control requires v5.2.29 history (git fetch --tags)"
        if os.environ.get("CI", "").lower() in {"1", "true", "yes"}:
            pytest.fail(reason)
        pytest.skip(reason)
    (tmp_path / "baseline-hal.a").write_bytes(baseline.stdout)
    common = ["-mcpu=cortex-m55", "-mthumb", "-mfpu=fpv5-d16", "-mfloat-abi=hard",
              "-Os", "-ffunction-sections", "-fdata-sections"]
    if family == "atfe":
        assert (compiler.parent / "newlib.cfg").is_file(), "ATfE requires its matching newlib overlay"
        common += ["--target=arm-none-eabi", "--config=newlib.cfg"]

    def run(args: list[str | Path]) -> str:
        command = list(map(str, args))
        result = subprocess.run(command, cwd=tmp_path, capture_output=True, text=True)
        output = result.stdout + result.stderr
        with (tmp_path / "commands.log").open("a") as log:
            log.write(shlex.join(command) + "\n" + output + f"\nexit={result.returncode}\n")
        assert result.returncode == 0, output
        return output

    def tool(name: str) -> Path:
        return gcc.parent / f"arm-none-eabi-{name}"

    run([compiler, *common, "-c", repo_root / "modules/nsx-core/src/sbrk.c", "-o", "sbrk.o"])
    run([tool("ar"), "rcs", "system.a", "sbrk.o"])
    run([cxx, *common, "-fno-exceptions", "-fno-rtti", "-c", fixture / "probe.cpp", "-o", "probe.o"])
    for forced in (False, True):
        name = "bounded" if forced else "negative-control"
        elf = tmp_path / f"{name}.elf"
        options = ["-Wl,-u,_sbrk"] if forced else []
        hal = repo_root / relative_hal if forced else tmp_path / "baseline-hal.a"
        run([cxx, *common, "-nostartfiles", "-Wl,--gc-sections",
             f"-Wl,-T,{fixture / 'link.ld'}", f"-Wl,-Map,{name}.map",
             *options, "probe.o", "system.a", hal, "-o", elf])
        symbols = run([tool("nm"), "-P", elf])
        (tmp_path / f"{name}.nm").write_text(symbols)
        parsed = {parts[0]: (parts[1], int(parts[2], 16))
                  for line in symbols.splitlines() if len(parts := line.split()) >= 3}
        assert parsed["_sbrk"][0] == ("T" if forced else "W"), symbols
        assembly = run([tool("objdump"), "-d", elf])
        (tmp_path / f"{name}.disassembly").write_text(assembly)
        sbrk = re.search(r"<_sbrk>:\n(.*?)(?=\n\n)", assembly, re.S)
        assert sbrk is not None
        if forced:
            assert "sbrk.o" in (tmp_path / f"{name}.map").read_text()
            assert parsed["__HeapBase"][1] < parsed["__HeapLimit"][1]
            assert "<__errno>" in sbrk[1], "bounded overflow path must set errno"
        else:
            assert re.search(r"movs\s+r0, #88", sbrk[1]), sbrk[1]
            assert "bx\tlr" in sbrk[1], sbrk[1]
        # Retained initializer -> real malloc -> _sbrk_r -> selected _sbrk.
        # The application deliberately has no direct _sbrk reference.
        init = tmp_path / f"{name}.init"
        run([tool("objcopy"), "--dump-section", f".init_array={init}", elf])
        initializers = struct.unpack("<" + "I" * (init.stat().st_size // 4), init.read_bytes())
        constructors = [address for symbol, (_, address) in parsed.items()
                        if symbol.startswith("_GLOBAL__sub_I")]
        assert constructors and any((address | 1) in initializers for address in constructors)
        for symbol in ("malloc", "_malloc_r", "_sbrk_r", "__libc_init_array", "main"):
            assert symbol in parsed
        entry = re.search(r"<_start>:\n(.*?)(?=\n\n)", assembly, re.S)
        assert entry is not None
        assert entry[1].index("<__libc_init_array>") < entry[1].index("<main>")


def test_shipped_hal_archives_do_not_define_sbrk(repo_root: Path) -> None:
    """The retained force-link mitigation must not hide a returned vendor stub."""
    gcc = compiler_for_test("gcc")
    root = repo_root / "modules/nsx-ambiqsuite/sdk/lib"
    archives = sorted(root.glob("*/*/libam_hal.a"))
    assert archives, "no shipped HAL archives found"
    for archive in archives:
        symbols = subprocess.check_output(
            [str(gcc.parent / "arm-none-eabi-nm"), "--defined-only", "-P", str(archive)],
            text=True,
        )
        assert not any(line.split() and line.split()[0] == "_sbrk"
                       for line in symbols.splitlines()), str(archive)
