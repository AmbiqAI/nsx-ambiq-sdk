"""Apply explicit pre-execution memory seeds, independent of any suite."""
from dataclasses import dataclass
from typing import Protocol


@dataclass(frozen=True)
class MemorySeed:
    symbol: str
    address: int
    value: int


class Memory(Protocol):
    def memory_write32(self, address: int, values: list[int]) -> int: ...
    def memory_read32(self, address: int, count: int) -> list[int]: ...


def seed_memory(probe: Memory, seeds: tuple[MemorySeed, ...]) -> dict[str, int]:
    addresses = [s.address for s in seeds]
    if len(set(addresses)) != len(addresses) or any(a < 0 or a % 4 for a in addresses):
        raise ValueError('Expected distinct aligned seed addresses')
    if len({s.symbol for s in seeds}) != len(seeds) or any(not 0 <= s.value <= 0xFFFFFFFF for s in seeds):
        raise ValueError('Expected unique seed symbols and uint32 values')
    observed = {}
    for seed in seeds:
        probe.memory_write32(seed.address, [seed.value])
        if probe.memory_read32(seed.address, 1) != [seed.value]:
            raise RuntimeError(f'Could not seed {seed.symbol}')
        observed[seed.symbol] = seed.value
    return observed
