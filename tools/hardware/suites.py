"""SDK-owned suite definitions; hardware facts remain in SDK descriptors."""
from dataclasses import dataclass
from pathlib import Path
from typing import Callable

from .protocol import validate_cases
from .startup import OMISSIONS, POISON, SENTINELS, make_mutant


@dataclass(frozen=True)
class Seed:
    symbol: str
    value: int


def no_options(repo: Path, output: Path, board: str, fault: str) -> list[str]:
    if fault != 'none':
        raise ValueError('This suite has no fault injection modes')
    return []


@dataclass(frozen=True)
class Suite:
    name: str
    nodeid: str
    cases: tuple[str, ...]
    source_dir: str = ''
    elf_name: str = 'test.elf'
    seeds: tuple[Seed, ...] = ()
    configure: Callable[[Path, Path, str, str], list[str]] = no_options

    def __post_init__(self):
        validate_cases(self.cases)
        if len({seed.symbol for seed in self.seeds}) != len(self.seeds):
            raise ValueError('Suite seed symbols must be unique')


def startup_options(repo: Path, output: Path, board: str, fault: str) -> list[str]:
    if fault in OMISSIONS:
        return [f'-DNSX_TEST_STARTUP_OVERRIDE={make_mutant(repo, output, board, fault)}']
    if fault not in ('none', 'assertion', 'timeout'):
        raise ValueError(f'Unknown startup fault mode: {fault}')
    return []


STARTUP = Suite('startup', 'tests/hardware/test_startup.py::test_startup',
                ('initialized_data', 'zeroed_bss', 'cpp_constructor'),
                'tests/hardware/firmware', 'startup_test.elf',
                tuple(Seed(n, v) for n, v in zip(SENTINELS, POISON)), startup_options)
SUITES = (STARTUP,)
