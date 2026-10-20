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
class Section:
    name: str
    kind: int
    flags: int
    address: int
    offset: int
    size: int


@dataclass(frozen=True)
class Symbol:
    address: int
    size: int
    section: int


def check_section_loads(data: bytes, sections: list[Section], segments: list[Segment],
                        regions: dict[str, tuple[int, int]], scatter: bool, definitions: dict[str, Symbol]) -> None:
    copy_sources = {".data": "_init_data", ".shared": "_init_data_sram",
                    ".itcm_text": "_init_itcm_text", ".dtcm_text": "_init_dtcm_text",
                    ".tcm": "_init_tcm"}
    for section in sections:
        if not section.flags & 2 or not section.size or section.kind == 8:  # SHF_ALLOC, SHT_NOBITS
            continue
        take(data, section.offset, section.size)
        candidates = [seg for seg in segments if seg.offset <= section.offset
                      and section.offset + section.size <= seg.offset + seg.file_size]
        # GNU copy-down segments retain the execution VMA. ArmClang's single
        # NVM load segment also contains RAM/ITCM scatter-region initializers;
        # their section VMA deliberately differs from the segment mapping.
        scatter_copy = scatter and section.name in regions and section.name != "MCU_MRAM"
        if scatter_copy:
            start, size = regions[section.name]
            candidates = [seg for seg in candidates if start <= section.address
                          and section.address + section.size <= start + size]
        else:
            candidates = [seg for seg in candidates
                          if section.address == seg.address + section.offset - seg.offset]
        if not scatter and section.name in copy_sources:
            source = definitions.get(copy_sources[section.name])
            if source is None or not candidates or any(
                seg.physical + section.offset - seg.offset != source.address for seg in candidates
            ):
                raise ValueError(f"Firmware load disagrees with startup copy source: {section.name}")
        if not candidates or section.flags & 4 and not any(seg.flags & 1 for seg in candidates):
            raise ValueError(f"Firmware section lacks complete consistent load coverage: {section.name}")


def check_stack(stack: int, sections: list[Section], definitions: dict[str, Symbol],
                regions: dict[str, tuple[int, int]], scatter: bool) -> None:
    if scatter:
        if "ARM_LIB_STACK" not in regions:
            raise ValueError("Firmware has no declared stack allocation")
        bottom, size = regions["ARM_LIB_STACK"]
        limits = ("Image$$ARM_LIB_STACK$$ZI$$Base", "Image$$ARM_LIB_STACK$$ZI$$Limit")
        if any(name not in definitions for name in limits) or (
            definitions[limits[0]].address, definitions[limits[1]].address
        ) != (bottom, bottom + size):
            raise ValueError("Firmware scatter stack symbols disagree with allocation")
        allocated = [sec for sec in sections if sec.name == "ARM_LIB_STACK"
                     and sec.address == bottom and sec.size == size]
    else:
        symbol = definitions.get("g_pui32Stack")
        if symbol is None or not 0 < symbol.section < len(sections):
            raise ValueError("Firmware has no startup stack object")
        bottom, size = symbol.address, symbol.size
        section = sections[symbol.section]
        allocated = [section] if (section.name == ".stack" and section.address <= bottom
                                  and bottom + size <= section.address + section.size) else []
    if size < 8 or stack != bottom + size or stack % 8 or not any(
        sec.kind == 8 and sec.flags & 3 == 3 for sec in allocated
    ):
        raise ValueError("Firmware initial stack does not select the allocated startup stack top")


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


def memory_regions(linker_script: Path) -> dict[str, tuple[int, int]]:
    """Read the literal memory declarations in the supported board scripts."""
    script = re.sub(r"/\*.*?\*/|//[^\n]*", "", linker_script.read_text(), flags=re.S)
    number = r"(?:0[xX][\da-fA-F]+|\d+)"
    if linker_script.suffix == ".sct":
        script = re.sub(r";[^\n]*", "", script)
        names = r"MCU_MRAM|MCU_ITCM|MCU_TCM|SHARED_SRAM|ARM_LIB_HEAP|ARM_LIB_STACK"
        matches = re.findall(rf"^\s*({names})\s+({number})\s+(?:EMPTY\s+)?(-\s*)?({number})\b", script, re.M)
        return {name: (int(a, 0) - int(n, 0) if negative else int(a, 0), int(n, 0))
                for name, a, negative, n in matches}
    matches = re.findall(rf"^\s*(\w+)\s*\([^)]*\)\s*:\s*ORIGIN\s*=\s*({number}),\s*LENGTH\s*=\s*({number})\b", script, re.M)
    return {name: (int(a, 0), int(n, 0)) for name, a, n in matches}


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
    names_index = header[13]
    if not 0 < names_index < len(sections):
        raise ValueError("Invalid firmware ELF section-name table")
    names_section = sections[names_index]
    names = take(data, names_section[4], names_section[5])
    allocated_sections = [Section(name_at(names, sec[0]), sec[1], sec[2], sec[3], sec[4], sec[5])
                          for sec in sections]
    symbols: dict[str, int] = {}
    definitions: dict[str, Symbol] = {}
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        if section[9] != 16 or section[5] % 16 or section[6] >= len(sections):
            raise ValueError("Invalid firmware ELF symbol table")
        strings_section = sections[section[6]]
        strings = take(data, strings_section[4], strings_section[5])
        table = take(data, section[4], section[5])
        for offset in range(0, len(table), 16):
            name, value, size, _, _, defined = unpack(table, offset, "<IIIBBH")
            if defined:
                symbol_name = name_at(strings, name)
                symbols[symbol_name] = value
                definitions[symbol_name] = Symbol(value, size, defined)
    required = ("Reset_Handler", "SystemInit", "main", "am_hal_pdm_configure", "am_bsp_low_power_init")
    for symbol in required:
        if symbol not in symbols:
            raise ValueError(f"Firmware is missing required definition: {symbol}")
        definition = definitions[symbol]
        address = definition.address & ~1
        if not 0 < definition.section < len(sections):
            raise ValueError(f"Firmware symbol has no code section: {symbol}")
        section = allocated_sections[definition.section]
        if section.flags & 6 != 6 or not (section.address <= address
            and address + max(1, definition.size) <= section.address + section.size):
            raise ValueError(f"Firmware symbol extends outside executable section: {symbol}")
        if not any(seg.flags & 1 and seg.address <= address < seg.address + seg.file_size for seg in segments):
            raise ValueError(f"Firmware symbol is outside loaded executable code: {symbol}")
    if not entry & 1 or entry & ~1 != symbols["Reset_Handler"] & ~1:
        raise ValueError("Firmware entry does not select Thumb Reset_Handler")
    regions = memory_regions(linker_script)
    if "MCU_MRAM" not in regions:
        raise ValueError("Cannot identify the selected application NVM aperture")
    origin, size = regions["MCU_MRAM"]
    ram = [span for name, span in regions.items() if name not in ("MCU_MRAM", "MCU_ITCM")]
    if not origin <= (entry & ~1) < origin + size:
        raise ValueError("Firmware Reset_Handler must execute from application NVM before RAM initialization")
    # A load segment may cover adjacent scatter data/heap/stack regions.
    # Merge only touching/overlapping declarations, preserving any holes.
    extents: list[tuple[int, int]] = []
    for address, length in sorted(regions.values()):
        end = address + length
        if address < 0 or not length or end > 2**32:
            raise ValueError("Invalid declared firmware memory region")
        if extents and address <= extents[-1][1]:
            extents[-1] = (extents[-1][0], max(extents[-1][1], end))
        else:
            extents.append((address, end))
    runtime_spans = [(seg.address, seg.memory_size) for seg in segments]
    runtime_spans += [(section[3], section[5]) for section in sections if section[2] & 2]
    if any(length and not any(start <= address and address + length <= end for start, end in extents)
           for address, length in runtime_spans):
        raise ValueError("Firmware runtime span extends outside declared memory regions")
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
    if stack % 8 or not any(a < stack <= a + n for a, n in ram):
        raise ValueError("Firmware initial stack is outside declared RAM or unaligned")
    check_stack(stack, allocated_sections, definitions, regions, linker_script.suffix == ".sct")
    check_section_loads(data, allocated_sections, segments, regions, linker_script.suffix == ".sct", definitions)
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
