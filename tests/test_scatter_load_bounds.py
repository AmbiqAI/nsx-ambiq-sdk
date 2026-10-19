"""Bound aggregate load bytes, independently of individual execution sizes."""
from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess

import pytest

from test_small_m55_memory import compiler_for_test


ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = tuple(path.relative_to(ROOT).as_posix() for path in sorted((ROOT / "modules/nsx-core/src").glob("*/armclang/linker*.sct"))
    if "MCU_MRAM" in path.read_text())


@pytest.mark.parametrize("script", SCRIPTS)
@pytest.mark.parametrize("case", ("small", "exact", "ro-overflow", "load-overflow"))
def test_scatter_aggregate_load_bound(repo_root: Path, tmp_path: Path, script: str, case: str) -> None:
    linker = shutil.which("armlink")
    if linker is None:
        pytest.skip("Real armlink required for scatter load qualification")
    compiler = compiler_for_test("gcc")
    path = repo_root / script
    text = path.read_text()
    memory = re.search(r"^\s*MCU_MRAM\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)", text, re.M)
    assert memory
    origin, size = int(memory[1], 16), int(memory[2], 16)
    # RESET is exactly four bytes. No scatter runtime is linked into this
    # layout-only probe; --no_remove retains every allocation under test.
    extra = {"small": "", "exact": f'.section .rodata,"a"\n.space {size - 4}\n',
        "ro-overflow": f'.section .rodata,"a"\n.space {size}\n',
        "load-overflow": f'.section .rodata,"a"\n.space {size - 2048}\n.section .data,"aw"\n.space 8196\n'}[case]
    source = tmp_path / "probe.s"
    source.write_text('.syntax unified\n.thumb\n.section RESET,"ax"\n.global Reset_Handler\n.thumb_func\nReset_Handler: b.w Reset_Handler\n' + extra)
    obj, elf = tmp_path / "probe.o", tmp_path / "probe.elf"
    subprocess.run([str(compiler), "-mcpu=cortex-m55", "-mthumb", "-c", str(source), "-o", str(obj)], check=True, capture_output=True)
    result = subprocess.run([linker, "--no_remove", "--scatter", str(path), "--entry", "Reset_Handler", str(obj), "-o", str(elf)], capture_output=True, text=True)
    if case.endswith("overflow"):
        assert result.returncode != 0, f"{script} accepted {case}"
        assert "exceeds limit" in result.stdout + result.stderr, result.stdout + result.stderr
    else:
        assert result.returncode == 0, result.stdout + result.stderr
        # Check physical load span in the produced ELF, not just LR_1 text.
        readelf = compiler.parent / "arm-none-eabi-readelf"
        output = subprocess.check_output([str(readelf), "-lW", str(elf)], text=True)
        ends = [int(addr, 16) + int(length, 16) for addr, length in re.findall(r"^\s*LOAD\s+\S+\s+\S+\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)", output, re.M) if int(length, 16)]
        assert ends and max(ends) <= origin + size
        if case == "exact":
            assert max(ends) == origin + size
