"""Require complete firmware and reject corrupted entry/vector/load evidence."""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys

import pytest

from test_small_m55_memory import compiler_for_test
from tools.nsx_firmware_elf import inspect_firmware
from tools.nsx_link_smoke import DEFAULT_BOARDS, write_smoke_source
from tools.nsx_toolchain_file import write_toolchain_file


def armclang_for_test() -> Path:
    root = os.environ.get("ACFE_ROOT")
    if root is not None:
        compiler = Path(root) / "bin" / "armclang"
        if not root or not compiler.is_file() or not os.access(compiler, os.X_OK):
            pytest.fail(f"ACFE_ROOT does not contain an executable bin/armclang: {root!r}")
        return compiler.resolve()
    compiler = shutil.which("armclang")
    if not compiler:
        pytest.skip("ArmClang unavailable")
    return Path(compiler).resolve()


def test_armclang_explicit_root(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    compiler = tmp_path / "bin/armclang"
    compiler.parent.mkdir()
    compiler.touch(mode=0o755)
    monkeypatch.setenv("ACFE_ROOT", str(tmp_path))
    monkeypatch.setenv("PATH", "/nonexistent")
    assert armclang_for_test() == compiler
    monkeypatch.setenv("ACFE_ROOT", str(tmp_path / "missing"))
    with pytest.raises(pytest.fail.Exception, match="ACFE_ROOT"):
        armclang_for_test()


@pytest.fixture(scope="session", params=[(family, board) for family in ("gcc", "atfe", "armclang") for board in DEFAULT_BOARDS])
def firmware(request: pytest.FixtureRequest, repo_root: Path, tmp_path_factory: pytest.TempPathFactory) -> tuple[Path, Path]:
    family, board = request.param
    env = os.environ.copy()
    if family == "armclang":
        compiler = armclang_for_test()
        env["ACFE_ROOT"] = str(compiler.parent.parent)
    else:
        compiler = compiler_for_test(family)
        env["GCC_ROOT" if family == "gcc" else "ATFE_ROOT"] = str(compiler.parent.parent)
    dest = tmp_path_factory.mktemp(f"smoke-{family}-{board}")
    result = subprocess.run([sys.executable, str(repo_root / "tools/nsx_link_smoke.py"),
        "--toolchain-family", family, "--board", board, "--build-root", str(dest)],
        env=env, capture_output=True, text=True)
    (dest / "build.log").write_text(result.stdout + result.stderr)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "validated " in result.stdout
    binary = dest / family / board / "smoke.elf"
    evidence = json.loads((binary.parent / "smoke-evidence.json").read_text())
    assert evidence["elf_sha256"] == hashlib.sha256(binary.read_bytes()).hexdigest()
    assert set(evidence["inputs"]) == {"startup", "system", "linker", "hal", "bsp"}
    for record in evidence["inputs"].values():
        assert record["sha256"] == hashlib.sha256(Path(record["path"]).read_bytes()).hexdigest()
    return binary, Path(evidence["inputs"]["linker"]["path"])


def test_smoke_is_linked_firmware(firmware: tuple[Path, Path]) -> None:
    binary, script = firmware
    evidence = inspect_firmware(binary, script)
    assert evidence.vector == evidence.nvm_origin
    assert evidence.load_end <= evidence.nvm_end
    assert evidence.symbols["SystemInit"]


@pytest.mark.parametrize("fault", ("entry", "entry-even", "missing-system", "missing-hal", "reset-vector", "stack", "load-overrun", "vector-load-shift", "load-overlap", "code-load-shift"))
def test_smoke_rejects_invalid_firmware(firmware: tuple[Path, Path], tmp_path: Path, fault: str) -> None:
    binary, script = firmware
    evidence = inspect_firmware(binary, script)
    data = bytearray(binary.read_bytes())
    phoff = struct.unpack_from("<I", data, 28)[0]
    phsize, phnum = struct.unpack_from("<HH", data, 42)
    vector_offset = None
    vector_header = None
    code_header = None
    first_load = None
    for index in range(phnum):
        offset = phoff + index * phsize
        kind, file_offset, address, _, size, _, _, _ = struct.unpack_from("<8I", data, offset)
        if kind == 1 and size:
            first_load = offset if first_load is None else first_load
            if address <= evidence.vector and evidence.vector + 8 <= address + size:
                vector_offset = file_offset + evidence.vector - address
                vector_header = offset
            if address <= (evidence.entry & ~1) < address + size:
                code_header = offset
    assert vector_offset is not None and first_load is not None and vector_header is not None
    if fault == "entry":
        struct.pack_into("<I", data, 24, 0x8000)
    elif fault == "entry-even":
        struct.pack_into("<I", data, 24, evidence.entry & ~1)
    elif fault == "missing-system":
        data = data.replace(b"SystemInit\0", b"XystemInit\0")
    elif fault == "missing-hal":
        data = data.replace(b"am_hal_pdm_configure\0", b"xm_hal_pdm_configure\0")
    elif fault == "reset-vector":
        struct.pack_into("<I", data, vector_offset + 4, 0x8001)
    elif fault == "stack":
        struct.pack_into("<I", data, vector_offset, 0x1000)
    elif fault == "load-overrun":
        struct.pack_into("<I", data, first_load + 12, evidence.nvm_end)
    elif fault == "vector-load-shift":
        physical = struct.unpack_from("<I", data, vector_header + 12)[0]
        struct.pack_into("<I", data, vector_header + 12, physical + 0x100)
    elif fault == "load-overlap":
        # ArmClang may combine all file-backed bytes into a single segment.
        # Add a RAM-copy segment whose load bytes collide with the vectors,
        # without changing any existing segment or vector VMA.
        headers = bytes(data[phoff:phoff + phsize * phnum])
        new_phoff = (len(data) + 3) & ~3
        data.extend(bytes(new_phoff - len(data)))
        data.extend(headers)
        data.extend(struct.pack("<8I", 1, vector_offset, 0x20000000,
                                evidence.nvm_origin, 4, 4, 6, 4))
        struct.pack_into("<I", data, 28, new_phoff)
        struct.pack_into("<H", data, 44, phnum + 1)
    else:
        assert code_header is not None
        physical = struct.unpack_from("<I", data, code_header + 12)[0]
        struct.pack_into("<I", data, code_header + 12, physical + 0x100)
    broken = tmp_path / "broken.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError):
        inspect_firmware(broken, script)


def test_smoke_without_startup_fails(repo_root: Path, tmp_path: Path) -> None:
    compiler = compiler_for_test("gcc")
    source, build = tmp_path / "src", tmp_path / "build"
    write_smoke_source(source, repo_root, "apollo330mP_evb", "gcc")
    cmake = source / "CMakeLists.txt"
    text = cmake.read_text()
    text = "\n".join(line for line in text.splitlines() if "nsx-cmsis-startup" not in line and "nsx::cmsis_startup" not in line and "target_link_libraries(nsx_startup" not in line)
    cmake.write_text(text + "\n")
    toolchain = write_toolchain_file(tmp_path / "gcc.cmake", "gcc", gcc_root=compiler.parent.parent)
    result = subprocess.run(["cmake", "-S", str(source), "-B", str(build), f"-DCMAKE_TOOLCHAIN_FILE={toolchain}"], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run(["cmake", "--build", str(build)], capture_output=True, text=True)
    if result.returncode == 0:
        script = repo_root / "modules/nsx-core/src/apollo330P/gcc/linker_script_sbl.ld"
        with pytest.raises(ValueError, match="Reset_Handler"):
            inspect_firmware(build / "smoke.elf", script)
    else:
        assert any(name in result.stdout + result.stderr for name in ("Reset_Handler", "SystemInit", "SystemCoreClock")), result.stdout + result.stderr


@pytest.mark.parametrize("contents", (b"", b"ELF", b"\x7fELF" + bytes(60)))
def test_invalid_elf_is_actionable(repo_root: Path, tmp_path: Path, contents: bytes) -> None:
    binary = tmp_path / "invalid.elf"
    binary.write_bytes(contents)
    with pytest.raises(ValueError):
        inspect_firmware(binary, repo_root / "modules/nsx-core/src/apollo330P/gcc/linker_script_sbl.ld")
