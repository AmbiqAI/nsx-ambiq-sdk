"""Startup sentinels and narrowly scoped, artifact-only negative controls."""
from pathlib import Path
import re
from typing import Protocol

SENTINELS = ('nsx_probe_data', 'nsx_probe_bss', 'nsx_probe_constructor')
POISON = (0xA5A5A5A5, 0x5A5A5A5A, 0xC3C3C3C3)
OMISSIONS = ('omit-data', 'omit-bss', 'omit-constructors')


class Memory(Protocol):
    def memory_write32(self, address: int, values: list[int]) -> int: ...
    def memory_read32(self, address: int, count: int) -> list[int]: ...


def poison_sentinels(probe: Memory, addresses: tuple[int, ...]) -> dict[str, int]:
    if len(addresses) != 3 or len(set(addresses)) != 3 or any(a % 4 for a in addresses):
        raise ValueError('Expected three distinct aligned sentinel addresses')
    observed = {}
    for name, address, value in zip(SENTINELS, addresses, POISON):
        probe.memory_write32(address, [value])
        actual = probe.memory_read32(address, 1)
        if actual != [value]:
            raise RuntimeError(f'Could not seed startup sentinel {name}')
        observed[name] = value
    return observed


def mutant(source: str, omission: str) -> str:
    if omission == 'omit-constructors':
        pattern = r'    __libc_init_array\(\);'
    elif omission in ('omit-data', 'omit-bss'):
        symbol = '_init_data' if omission == 'omit-data' else '_sbss'
        pattern = r'    __asm\("    ldr     r0, =' + symbol + r'\\n".*?\);'
    else:
        raise ValueError(f'Unknown omission: {omission}')
    changed, count = re.subn(pattern, f'    /* Negative control: {omission}. */', source, flags=re.DOTALL)
    if count != 1:
        raise ValueError(f'Refusing mutation: expected exactly one {omission} operation, found {count}')
    return changed


def make_mutant(repo: Path, output: Path, board: str, omission: str) -> Path:
    # These audited mutation points belong to AP3P's startup, not every family.
    if board != 'apollo3p_evb':
        raise ValueError('Startup omission controls currently require apollo3p_evb')
    original = repo / 'modules/nsx-core/src/apollo3p/gcc/startup_gcc.c'
    path = output / 'startup_mutant.c'
    path.write_text(mutant(original.read_text(), omission))
    return path
