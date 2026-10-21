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


def append_load(data: bytearray, file_offset: int, address: int, physical: int, file_size: int, memory_size: int, flags: int) -> None:
    phoff = struct.unpack_from("<I", data, 28)[0]
    phsize, phnum = struct.unpack_from("<HH", data, 42)
    headers = bytes(data[phoff:phoff + phsize * phnum])
    new_phoff = (len(data) + 3) & ~3
    data.extend(bytes(new_phoff - len(data)))
    data.extend(headers)
    data.extend(struct.pack("<8I", 1, file_offset, address, physical, file_size, memory_size, flags, 4))
    struct.pack_into("<I", data, 28, new_phoff)
    struct.pack_into("<H", data, 44, phnum + 1)


@pytest.mark.parametrize("fault", ("entry", "entry-even", "missing-system", "missing-hal", "reset-vector", "stack", "load-overrun", "vector-load-shift", "load-overlap", "code-load-shift", "reset-in-ram", "runtime-address", "runtime-overrun", "section-overrun"))
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
        append_load(data, vector_offset, 0x20000000, evidence.nvm_origin, 4, 4, 6)
    elif fault == "code-load-shift":
        assert code_header is not None
        physical = struct.unpack_from("<I", data, code_header + 12)[0]
        struct.pack_into("<I", data, code_header + 12, physical + 0x100)
    elif fault == "runtime-address":
        append_load(data, vector_offset, 0x30000000, (evidence.load_end + 3) & ~3, 4, 4, 6)
    elif fault == "runtime-overrun":
        append_load(data, 0, 0x20000000, 0, 0, 0x80000000, 6)
    else:
        shoff = struct.unpack_from("<I", data, 32)[0]
        shsize, shnum = struct.unpack_from("<HH", data, 46)
        sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
        if fault == "section-overrun":
            index = next(i for i, section in enumerate(sections) if section[1] == 8 and section[2] & 2 and section[5])
            struct.pack_into("<I", data, shoff + index * shsize + 20, 0x80000000)
        else:
            assert fault == "reset-in-ram"
            ram_entry = 0x20002001
            changed = False
            for section in sections:
                if section[1] != 2:
                    continue
                strings = sections[section[6]]
                names = data[strings[4]:strings[4] + strings[5]]
                for offset in range(section[4], section[4] + section[5], 16):
                    name, value, size, _, _, index = struct.unpack_from("<IIIBBH", data, offset)
                    if names[name:names.find(0, name)] == b"Reset_Handler":
                        code = sections[index]
                        start = code[4] + (value & ~1) - code[3]
                        payload = bytes(data[start:start + size])
                        assert size and len(payload) == size
                        struct.pack_into("<I", data, offset + 4, ram_entry)
                        struct.pack_into("<H", data, offset + 14, len(sections))
                        changed = True
            assert changed
            struct.pack_into("<I", data, 24, ram_entry)
            struct.pack_into("<I", data, vector_offset + 4, ram_entry)
            data.extend(b"\0" * (-len(data) % 4))
            payload_offset = len(data)
            data.extend(payload)
            append_load(data, payload_offset, ram_entry & ~1,
                        (evidence.load_end + 3) & ~3, len(payload), len(payload), 5)
            sections.append((0, 1, 6, ram_entry & ~1, payload_offset, len(payload), 0, 0, 4, 0))
            struct.pack_into("<I", data, 32, len(data))
            struct.pack_into("<H", data, 48, len(sections))
            for section in sections:
                data.extend(struct.pack("<10I", *section))
    broken = tmp_path / "broken.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError, match="must execute from application NVM" if fault == "reset-in-ram" else None):
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


@pytest.mark.parametrize("fault", ("stack-limit", "stack-below-top", "missing-data-load", "truncated-function", "section-file-mismatch"))
def test_smoke_rejects_incomplete_load_or_stack(firmware: tuple[Path, Path], tmp_path: Path, fault: str) -> None:
    binary, script = firmware
    evidence = inspect_firmware(binary, script)
    data = bytearray(binary.read_bytes())
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phnum, shsize, shnum, names_index = struct.unpack_from("<5H", data, 42)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_section = sections[names_index]
    names = data[names_section[4]:names_section[4] + names_section[5]]
    loads = [(phoff + i * phsize, struct.unpack_from("<8I", data, phoff + i * phsize)) for i in range(phnum)]
    loads = [(offset, seg) for offset, seg in loads if seg[0] == 1 and seg[4]]
    if fault.startswith("stack-"):
        stack_section = next(s for s in sections if names[s[0]:names.find(0, s[0])] in (b".stack", b"ARM_LIB_STACK"))
        new_stack = stack_section[3] if fault == "stack-limit" else evidence.stack - 8
        _, vector_seg = next((offset, seg) for offset, seg in loads if seg[2] <= evidence.vector < seg[2] + seg[4])
        struct.pack_into("<I", data, vector_seg[1] + evidence.vector - vector_seg[2], new_stack)
    elif fault == "missing-data-load":
        section = next(s for s in sections if s[1] == 1 and s[2] & 2 and s[3] >= 0x20000000 and s[5])
        offset, seg = next((offset, seg) for offset, seg in loads if seg[1] <= section[4] and section[4] + section[5] <= seg[1] + seg[4])
        if script.suffix == ".sct":
            # ArmClang groups scatter load bytes into one NVM segment.
            struct.pack_into("<I", data, offset + 16, section[4] - seg[1])
        else:
            struct.pack_into("<I", data, offset, 0)  # Remove initialized RAM PT_LOAD.
    elif fault == "truncated-function":
        last_function = max(value & ~1 for value in evidence.symbols.values())
        offset, seg = next((offset, seg) for offset, seg in loads if seg[2] <= last_function < seg[2] + seg[4])
        struct.pack_into("<I", data, offset + 16, last_function - seg[2] + 2)
    else:
        index = next(i for i, s in enumerate(sections) if s[2] & 2 and s[3] == evidence.vector)
        struct.pack_into("<I", data, shoff + index * shsize + 16, sections[index][4] + 4)
    broken = tmp_path / "broken.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError):
        inspect_firmware(broken, script)


@pytest.mark.parametrize("section_name", (".data", ".shared", ".itcm_text", ".dtcm_text"))
def test_smoke_rejects_shifted_ram_load(firmware: tuple[Path, Path], tmp_path: Path, section_name: str) -> None:
    binary, script = firmware
    if script.suffix == ".sct":
        pytest.skip("ArmClang uses a single NVM scatter load")
    data = bytearray(binary.read_bytes())
    evidence = inspect_firmware(binary, script)
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phnum, shsize, shnum, names_index = struct.unpack_from("<5H", data, 42)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_section = sections[names_index]
    names = data[names_section[4]:names_section[4] + names_section[5]]
    section = next((s for s in sections if names[s[0]:names.find(0, s[0])] == section_name.encode() and s[5]), None)
    if section is None:
        pytest.skip(f"{section_name} not present in this board image")
    loads = [(phoff + i * phsize, struct.unpack_from("<8I", data, phoff + i * phsize)) for i in range(phnum)]
    offset, seg = next((offset, seg) for offset, seg in loads if seg[0] == 1 and seg[1] <= section[4] and section[4] + section[5] <= seg[1] + seg[4])
    shifted = (evidence.load_end + 0x103) & ~3
    assert shifted + seg[4] < evidence.nvm_end
    struct.pack_into("<I", data, offset + 12, shifted)
    broken = tmp_path / "shifted.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError, match="startup copy source"):
        inspect_firmware(broken, script)


@pytest.mark.parametrize("section_name,start_name,end_name", (
    (".data", "_sdata", "_edata"), (".shared", "_ssdata", "_sedata"),
    (".itcm_text", "_s_itcm_text", "_e_itcm_text"),
    (".dtcm_text", "_s_dtcm_text", "_e_dtcm_text"),
))
@pytest.mark.parametrize("fault", ("empty", "short", "shift-start"))
def test_smoke_rejects_wrong_copy_bounds(firmware: tuple[Path, Path], tmp_path: Path,
        section_name: str, start_name: str, end_name: str, fault: str) -> None:
    binary, script = firmware
    if script.suffix == ".sct":
        pytest.skip("ArmClang scatter copies do not use GNU destination symbols")
    data = bytearray(binary.read_bytes())
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shnum, names_index = struct.unpack_from("<3H", data, 46)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_sec = sections[names_index]
    names = data[names_sec[4]:names_sec[4] + names_sec[5]]
    section = next((s for s in sections if names[s[0]:names.find(0, s[0])] == section_name.encode() and s[5]), None)
    if section is None:
        pytest.skip(f"{section_name} not present in this board image")
    target = start_name if fault == "shift-start" else end_name
    value = section[3] if fault == "empty" else section[3] + (4 if fault == "shift-start" else section[5] - 2)
    changed = False
    for symtab in sections:
        if symtab[1] != 2:
            continue
        strings = sections[symtab[6]]
        strings = data[strings[4]:strings[4] + strings[5]]
        for offset in range(symtab[4], symtab[4] + symtab[5], 16):
            name = struct.unpack_from("<I", data, offset)[0]
            if strings[name:strings.find(0, name)] == target.encode():
                struct.pack_into("<I", data, offset + 4, value)
                changed = True
    assert changed
    broken = tmp_path / "bounds.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError, match="startup copy destination"):
        inspect_firmware(broken, script)


def test_smoke_rejects_nobits_code(firmware: tuple[Path, Path], tmp_path: Path) -> None:
    binary, script = firmware
    evidence = inspect_firmware(binary, script)
    data = bytearray(binary.read_bytes())
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phnum, shsize, shnum = struct.unpack_from("<4H", data, 42)
    last = max(value & ~1 for value in evidence.symbols.values())
    changed = False
    for i in range(shnum):
        offset = shoff + i * shsize
        section = struct.unpack_from("<10I", data, offset)
        if section[2] & 6 == 6 and section[3] <= last < section[3] + section[5]:
            struct.pack_into("<I", data, offset + 4, 8)
            changed = True
    assert changed
    for i in range(phnum):
        offset = phoff + i * phsize
        seg = struct.unpack_from("<8I", data, offset)
        if seg[0] == 1 and seg[2] <= last < seg[2] + seg[4]:
            struct.pack_into("<I", data, offset + 16, last - seg[2] + 2)
    broken = tmp_path / "nobits-code.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError):
        inspect_firmware(broken, script)


@pytest.mark.parametrize("section_name", (".data", ".shared", ".itcm_text", ".dtcm_text"))
@pytest.mark.parametrize("missing_section", (False, True))
def test_smoke_rejects_nobits_copy(firmware: tuple[Path, Path], tmp_path: Path, section_name: str, missing_section: bool) -> None:
    binary, script = firmware
    if script.suffix == ".sct":
        pytest.skip("ArmClang does not use GNU startup copy sections")
    data = bytearray(binary.read_bytes())
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phnum, shsize, shnum, names_index = struct.unpack_from("<5H", data, 42)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_sec = sections[names_index]
    names = data[names_sec[4]:names_sec[4] + names_sec[5]]
    found = next(((i, s) for i, s in enumerate(sections)
                  if names[s[0]:names.find(0, s[0])] == section_name.encode() and s[5]), None)
    if found is None:
        pytest.skip(f"{section_name} not present in this board image")
    index, section = found
    if missing_section:
        struct.pack_into("<10I", data, shoff + index * shsize, *([0] * 10))
    else:
        struct.pack_into("<I", data, shoff + index * shsize + 4, 8)  # SHT_NOBITS
    for i in range(phnum):
        offset = phoff + i * phsize
        seg = struct.unpack_from("<8I", data, offset)
        if seg[0] == 1 and seg[1] <= section[4] and section[4] + section[5] <= seg[1] + seg[4]:
            struct.pack_into("<I", data, offset, 0)  # Remove the initializer load.
    broken = tmp_path / "nobits-copy.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError):
        inspect_firmware(broken, script)


def test_smoke_rejects_scatter_nobits_code(firmware: tuple[Path, Path], tmp_path: Path) -> None:
    binary, script = firmware
    if script.suffix != ".sct":
        pytest.skip("ArmClang scatter executable-section regression")
    data = bytearray(binary.read_bytes())
    shoff = struct.unpack_from("<I", data, 32)[0]
    shsize, shnum, names_index = struct.unpack_from("<3H", data, 46)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_sec = sections[names_index]
    names = data[names_sec[4]:names_sec[4] + names_sec[5]]
    found = next(((i, s) for i, s in enumerate(sections)
                  if names[s[0]:names.find(0, s[0])] == b"MCU_ITCM" and s[5]), None)
    if found is None:
        pytest.skip("This scatter image does not place executable code in MCU_ITCM")
    index, section = found
    assert section[2] & 6 == 6
    struct.pack_into("<I", data, shoff + index * shsize + 4, 8)
    data[section[4]:section[4] + section[5]] = bytes(section[5])
    broken = tmp_path / "scatter-nobits-code.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError, match="executable.*NOBITS"):
        inspect_firmware(broken, script)


@pytest.mark.parametrize("destination", ("ram", "nvm"))
def test_smoke_copy_destination_memory(firmware: tuple[Path, Path], tmp_path: Path, destination: str) -> None:
    binary, script = firmware
    if script.suffix == ".sct":
        pytest.skip("GNU/ATfE copy destination regression")
    if destination == "ram":
        inspect_firmware(binary, script)
        return
    data = bytearray(binary.read_bytes())
    phoff, shoff = struct.unpack_from("<II", data, 28)
    phsize, phnum, shsize, shnum, names_index = struct.unpack_from("<5H", data, 42)
    sections = [struct.unpack_from("<10I", data, shoff + i * shsize) for i in range(shnum)]
    names_sec = sections[names_index]
    names = data[names_sec[4]:names_sec[4] + names_sec[5]]
    index, section = next((i, s) for i, s in enumerate(sections)
                          if names[s[0]:names.find(0, s[0])] == b".data" and s[5])
    loads = [(phoff + i * phsize, struct.unpack_from("<8I", data, phoff + i * phsize)) for i in range(phnum)]
    offset, seg = next((o, s) for o, s in loads if s[0] == 1 and s[1] <= section[4]
                       and section[4] + section[5] <= s[1] + s[4])
    target = seg[3] + section[4] - seg[1]
    # Keep the initializer and its physical load unchanged, but consistently
    # move the destination section, segment VMA and copy symbols into NVM.
    struct.pack_into("<I", data, offset + 8, seg[3])
    struct.pack_into("<I", data, shoff + index * shsize + 12, target)
    changed = set()
    for symtab in sections:
        if symtab[1] != 2:
            continue
        strings_sec = sections[symtab[6]]
        strings = data[strings_sec[4]:strings_sec[4] + strings_sec[5]]
        for offset in range(symtab[4], symtab[4] + symtab[5], 16):
            name = struct.unpack_from("<I", data, offset)[0]
            symbol = strings[name:strings.find(0, name)]
            if symbol in (b"_sdata", b"_edata"):
                struct.pack_into("<I", data, offset + 4, target + (section[5] if symbol == b"_edata" else 0))
                changed.add(bytes(symbol))
    assert changed == {b"_sdata", b"_edata"}
    broken = tmp_path / "copy-into-nvm.elf"
    broken.write_bytes(data)
    with pytest.raises(ValueError, match="startup copy destination.*RAM/TCM"):
        inspect_firmware(broken, script)


@pytest.mark.parametrize("section_name,source,start,end", (
    (".data", "_init_data", "_sdata", "_edata"),
    (".shared", "_init_data_sram", "_ssdata", "_sedata"),
    (".itcm_text", "_init_itcm_text", "_s_itcm_text", "_e_itcm_text"),
    (".dtcm_text", "_init_dtcm_text", "_s_dtcm_text", "_e_dtcm_text"),
    (".tcm", "_init_tcm", "_stcm", "_etcm"),
))
@pytest.mark.parametrize("address,valid", ((0, True), (0x2000000C, True), (0x20080000, True),
                                          (0x410000, False), (0x2000000E, False), (0x30000000, False)))
def test_copy_destination_regions(section_name: str, source: str, start: str, end: str,
                                  address: int, valid: bool) -> None:
    from tools.nsx_firmware_elf import Section, Segment, Symbol, check_section_loads

    regions = {"MCU_MRAM": (0x410000, 16), "MCU_ITCM": (0, 16),
               "MCU_TCM": (0x20000000, 16), "SHARED_SRAM": (0x20080000, 16)}
    definitions = {name: Symbol(value, 0, 1) for name, value in
                   ((source, 0x410000), (start, address), (end, address + 4))}
    sections = [Section(section_name, 1, 3, address, 0, 4)]
    segments = [Segment(address, 0x410000, 0, 4, 4, 6)]
    if valid:
        check_section_loads(b"data", sections, segments, regions, False, definitions)
    else:
        with pytest.raises(ValueError, match="startup copy destination.*RAM/TCM"):
            check_section_loads(b"data", sections, segments, regions, False, definitions)
