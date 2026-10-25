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


def no_options(repo: Path, output: Path, board: str, fault: str, sdk_root: Path | None = None) -> list[str]:
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
    configure: Callable[[Path, Path, str, str, Path], list[str]] = no_options

    def __post_init__(self):
        validate_cases(self.cases)
        if len({seed.symbol for seed in self.seeds}) != len(self.seeds):
            raise ValueError('Suite seed symbols must be unique')


def startup_options(repo: Path, output: Path, board: str, fault: str, sdk_root: Path | None = None) -> list[str]:
    if fault in OMISSIONS:
        return [f'-DNSX_TEST_STARTUP_OVERRIDE={make_mutant(sdk_root or repo, output, board, fault)}']
    if fault not in ('none', 'assertion', 'timeout'):
        raise ValueError(f'Unknown startup fault mode: {fault}')
    return []


STARTUP = Suite('startup', 'tests/hardware/test_startup.py::test_startup',
                ('initialized_data', 'zeroed_bss', 'cpp_constructor'),
                'tests/hardware/firmware', 'startup_test.elf',
                tuple(Seed(n, v) for n, v in zip(SENTINELS, POISON)), startup_options)

def runtime_options(repo: Path, output: Path, board: str, fault: str, sdk_root: Path | None = None) -> list[str]:
    if fault not in ('none', 'runtime-no-svc'):
        raise ValueError(f'Unknown runtime fault mode: {fault}')
    return []


RUNTIME = Suite('runtime', 'tests/hardware/test_runtime.py::test_runtime',
                ('stack_bounds_alignment', 'heap_allocation', 'heap_limit_rejection', 'svc_dispatch'),
                'tests/hardware/runtime', 'runtime_test.elf', configure=runtime_options)
CLOCK_BOARDS = ('apollo510b_evb', 'apollo330mP_evb')
CLOCK_FAULTS = ('none', 'clock-stale-publication', 'clock-no-reload', 'clock-wrong-scaling')


def clock_options(repo: Path, output: Path, board: str, fault: str, sdk_root: Path | None = None) -> list[str]:
    if board not in CLOCK_BOARDS:
        raise ValueError('Clock diagnostic supports AP510B and AP330 Plus only')
    if fault not in CLOCK_FAULTS:
        raise ValueError(f'Unknown clock fault mode: {fault}')
    return []


CLOCK = Suite('clock', 'tests/hardware/test_clock.py::test_clock',
              ('reference_ready', 'clock_transitions', 'restore_low'),
              'tests/hardware/clock', 'clock_test.elf', configure=clock_options)
TEMPCO = Suite('tempco-lifecycle', 'tests/hardware/test_tempco.py::test_tempco',
               ('setup_release_retry',), 'tests/fixtures/tempco-smoke', 'tempco_smoke.elf')
OWNERSHIP = Suite('startup-ownership', 'tests/hardware/test_ownership.py::test_ownership',
                  ('initialize_once', 'reject_reapplication'),
                  'tests/fixtures/startup-ownership', 'startup_ownership.elf')
SUITES = (STARTUP, RUNTIME, CLOCK, TEMPCO, OWNERSHIP)
