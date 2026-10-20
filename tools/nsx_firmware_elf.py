"""Inspect linked ARM ELF32 firmware without a toolchain-specific dumper."""
from __future__ import annotations

from dataclasses import asdict, dataclass
from pathlib import Path
import re
import struct


@dataclass(frozen=True)
class Segment:
    address: int
    physical: int
    offset: int
    file_size: int
    memory_size: int
    flags: int


@dataclass(frozen=True)
class FirmwareEvidence:
    entry: int
    vector: int
    stack: int
    nvm_origin: int
    nvm_end: int
    load_end: int
    symbols: dict[str, int]

    def to_dict(self) -> dict:
        return asdict(self)


def take(data: bytes, offset: int, size: int) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError("Truncated firmware ELF")
    return data[offset:offset + size]


def unpack(data: bytes, offset: int, fmt: str) -> tuple:
    return struct.unpack(fmt, take(data, offset, struct.calcsize(fmt)))


def name_at(strings: bytes, offset: int) -> str:
    if not 0 <= offset < len(strings):
        raise ValueError("Invalid firmware ELF string offset")
    end = strings.find(b"\0", offset)
    if end < 0:
        raise ValueError("Unterminated firmware ELF symbol")
    return strings[offset:end].decode("utf-8", errors="replace")


def inspect_firmware(elf: Path, linker_script: Path) -> FirmwareEvidence:
    data = elf.read_bytes()
    header = unpack(data, 0, "<16sHHIIIIIHHHHHH")
    if header[0][:7] != b"\x7fELF\x01\x01\x01" or header[1:4] != (2, 40, 1):
        raise ValueError("Expected a linked little-endian ARM ELF32 executable")
    entry, phoff, shoff = header[4:7]
    phsize, phnum, shsize, shnum = header[9:13]
    if phsize != 32 or shsize != 40 or not phnum or not shnum:
        raise ValueError("Firmware ELF has no ordinary load/section tables")
    take(data, phoff, phsize * phnum)
    take(data, shoff, shsize * shnum)
    segments = []
    for index in range(phnum):
        kind, offset, address, physical, filesz, memsz, flags, _ = unpack(data, phoff + index * phsize, "<8I")
        if kind == 1:
            take(data, offset, filesz)
            if filesz > memsz or address + memsz > 2**32 or physical + filesz > 2**32:
                raise ValueError("Invalid firmware ELF load span")
            segments.append(Segment(address, physical, offset, filesz, memsz, flags))
    sections = [unpack(data, shoff + index * shsize, "<10I") for index in range(shnum)]
    symbols: dict[str, int] = {}
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        if section[9] != 16 or section[5] % 16 or section[6] >= len(sections):
            raise ValueError("Invalid firmware ELF symbol table")
        strings_section = sections[section[6]]
        strings = take(data, strings_section[4], strings_section[5])
        table = take(data, section[4], section[5])
        for offset in range(0, len(table), 16):
            name, value, _, _, _, defined = unpack(table, offset, "<IIIBBH")
            if defined:
                symbols[name_at(strings, name)] = value
    required = ("Reset_Handler", "SystemInit", "main", "am_hal_pdm_configure", "am_bsp_low_power_init")
    for symbol in required:
        if symbol not in symbols:
            raise ValueError(f"Firmware is missing required definition: {symbol}")
        address = symbols[symbol] & ~1
        if not any(seg.flags & 1 and seg.address <= address < seg.address + seg.file_size for seg in segments):
            raise ValueError(f"Firmware symbol is outside loaded executable code: {symbol}")
    if not entry & 1 or entry & ~1 != symbols["Reset_Handler"] & ~1:
        raise ValueError("Firmware entry does not select Thumb Reset_Handler")
    script = linker_script.read_text()
    if linker_script.suffix == ".sct":
        region = re.search(r"\bMCU_MRAM\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+)", script)
        ram = [(int(a, 0), int(n, 0)) for a, n in re.findall(r"\b(?:MCU_TCM|SHARED_SRAM)\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+)", script)]
        stack_tops = [int(a, 0) for a in re.findall(r"\bARM_LIB_STACK\s+(0x[\da-fA-F]+)\s+EMPTY\s+-", script)]
    else:
        region = re.search(r"MCU_MRAM\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(\w+),\s*LENGTH\s*=\s*(\w+)", script)
        ram = [(int(a, 0), int(n, 0)) for a, n in re.findall(r"(?:MCU_TCM|SHARED_SRAM)\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*(\w+),\s*LENGTH\s*=\s*(\w+)", script)]
        stack_tops = []
    if region is None:
        raise ValueError("Cannot identify the selected application NVM aperture")
    origin, size = int(region[1], 0), int(region[2], 0)
    vector_names = [name for name in ("__Vectors", "__VECTOR_TABLE") if name in symbols]
    if len(vector_names) != 1 or symbols[vector_names[0]] != origin:
        raise ValueError("Firmware vector table is missing or misplaced")
    vector = symbols[vector_names[0]]
    vector_segments = [seg for seg in segments if seg.address <= vector and vector + 8 <= seg.address + seg.file_size]
    if len(vector_segments) != 1:
        raise ValueError("Firmware vector words are not loaded")
    segment = vector_segments[0]
    if segment.physical + vector - segment.address != origin:
        raise ValueError("Firmware vector words are not physically loaded at the application origin")
    stack, reset = unpack(data, segment.offset + vector - segment.address, "<II")
    if not reset & 1 or reset & ~1 != symbols["Reset_Handler"] & ~1:
        raise ValueError("Firmware reset vector does not select Thumb Reset_Handler")
    if stack % 8 or not (stack in stack_tops or any(a < stack <= a + n for a, n in ram)):
        raise ValueError("Firmware initial stack is outside declared RAM or unaligned")
    loads = [seg for seg in segments if seg.file_size]
    if not loads or any(seg.physical < origin or seg.physical + seg.file_size > origin + size for seg in loads):
        raise ValueError("Firmware load bytes extend outside the application NVM aperture")
    ordered_loads = sorted(loads, key=lambda seg: seg.physical)
    if any(left.physical + left.file_size > right.physical
           for left, right in zip(ordered_loads, ordered_loads[1:])):
        raise ValueError("Firmware physical load spans overlap")
    # NVM-resident bytes execute/read in place. RAM sections may have a
    # different load address because startup copies them to their VMA.
    if any(seg.address < origin + size and seg.address + seg.file_size > origin
           and seg.physical != seg.address for seg in loads):
        raise ValueError("Firmware NVM execution and physical load addresses differ")
    return FirmwareEvidence(entry, vector, stack, origin, origin + size,
        max(seg.physical + seg.file_size for seg in loads), {name: symbols[name] for name in required})
