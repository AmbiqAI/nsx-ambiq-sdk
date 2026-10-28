"""SDK-owned suite definitions; hardware facts remain in SDK descriptors."""
from dataclasses import dataclass
from pathlib import Path
from functools import partial
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
TEMPCO_CAPTURE = Suite('tempco-capture', 'tests/hardware/test_tempco_capture.py::test_tempco_capture',
                        ('capture_and_resume',), 'tests/fixtures/tempco-capture-smoke', 'tempco_capture.elf')
OWNERSHIP = Suite('startup-ownership', 'tests/hardware/test_ownership.py::test_ownership',
                  ('initialize_once', 'reject_reapplication'),
                  'tests/fixtures/startup-ownership', 'startup_ownership.elf')
def usb_options(repo: Path, output: Path, board: str, fault: str,
                sdk_root: Path | None = None, *, keep: bool) -> list[str]:
    if board != 'apollo4p_blue_kxr_evb' or fault != 'none':
        raise ValueError('USB policy smoke requires AP4 Blue KXR without fault injection')
    return [f'-DNSX_TEST_USB_KEEP={"ON" if keep else "OFF"}']


USB_KEEP, USB_OFF = tuple(
    Suite(f'usb-{mode}', f'tests/hardware/test_usb_policy.py::test_usb_policy[usb-{mode}]',
          ('usb_policy',), 'tests/fixtures/usb-policy', 'usb_policy.elf',
          configure=partial(usb_options, keep=keep))
    for mode, keep in (('keep', True), ('off', False)))
def memory_options(repo: Path, output: Path, board: str, fault: str,
                   sdk_root: Path | None = None) -> list[str]:
    if board != 'apollo4p_blue_kxr_evb' or fault != 'none':
        raise ValueError('Memory preflight smoke requires AP4 Blue KXR without fault injection')
    return []


MEMORY_PREFLIGHT = Suite('memory-preflight', 'tests/hardware/test_memory_preflight.py::test_memory_preflight',
                         ('reject_reductions_then_start',), 'tests/fixtures/memory-preflight',
                         'memory_preflight.elf', configure=memory_options)
def system_status_options(repo: Path, output: Path, board: str, fault: str,
                          sdk_root: Path | None = None, *, minimal: bool) -> list[str]:
    if board not in ('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510b_evb', 'apollo330mP_evb') or fault != 'none':
        raise ValueError('System status smoke supports AP3P, AP4 KXR, AP510B and AP330P without faults')
    return [f'-DNSX_TEST_MINIMAL={"ON" if minimal else "OFF"}']


SYSTEM_FULL, SYSTEM_MINIMAL = tuple(
    Suite(f'system-{mode}', f'tests/hardware/test_system_status_smoke.py::test_system_status[system-{mode}]',
          ('reject_then_start',), 'tests/fixtures/system-status-smoke', 'system_status.elf',
          configure=partial(system_status_options, minimal=minimal))
    for mode, minimal in (('full', False), ('minimal', True)))
DEBUG_STATE = Suite('debug-state', 'tests/hardware/test_debug_output.py::test_debug_output',
                    ('output_lifecycle',), 'tests/fixtures/debug-state-smoke', 'debug_state.elf')


def debug_failure_options(repo: Path, output: Path, board: str, fault: str,
                          sdk_root: Path | None = None) -> list[str]:
    if board not in ('apollo330mP_evb', 'apollo510dL_evb') or fault != 'none':
        raise ValueError('SWO preparation failure smoke requires AP330P or AP510L')
    return ['-DNSX_TEST_SWO_FAILURE=ON']


DEBUG_FAILURE = Suite('debug-failure', 'tests/hardware/test_debug_failure.py::test_debug_failure',
                      ('preparation_failure',), 'tests/fixtures/debug-state-smoke',
                      'debug_state.elf', configure=debug_failure_options)

TIMER_BOARDS = ('apollo4p_blue_kxr_evb', 'apollo330mP_evb',
                'apollo510_evb', 'apollo510b_evb')


def timer_options(repo: Path, output: Path, board: str, fault: str,
                  sdk_root: Path | None = None) -> list[str]:
    if board not in TIMER_BOARDS or fault != 'none':
        raise ValueError('Timer-period smoke requires an Apollo4/510/330 board without faults')
    return []


TIMER_PERIOD = Suite('timer-period', 'tests/hardware/test_timer.py::test_timer_period',
                     ('setup_clock', 'period_1ms', 'period_10ms', 'period_50ms'),
                     'tests/hardware/timer', 'timer_period.elf', configure=timer_options)


PERFORMANCE = Suite('performance', 'tests/hardware/test_performance.py::test_performance',
                    ('prepare_performance', 'mixed_public_transitions', 'invalid_requests', 'restore_low'),
                    'tests/hardware/performance', 'performance_contract.elf', configure=timer_options)


def ap3_performance_options(repo: Path, output: Path, board: str, fault: str,
                            sdk_root: Path | None = None) -> list[str]:
    if board not in ('apollo3_evb', 'apollo3p_evb') or fault != 'none':
        raise ValueError('AP3 performance smoke requires AP3/AP3P without faults')
    return []


AP3_PERFORMANCE = Suite('ap3-performance', 'tests/hardware/test_ap3_performance.py::test_ap3_performance',
                        ('prepare', 'mixed_transitions', 'divided_normal', 'restore_normal'),
                        'tests/hardware/ap3-performance', 'ap3_performance_contract.elf',
                        configure=ap3_performance_options)


POWER_STARTUP = Suite('power-startup', 'tests/hardware/test_power_startup.py::test_power_startup',
                      ('configure_then_reject',), 'tests/fixtures/power-startup-smoke',
                      'power_startup.elf')


SUITES = (POWER_STARTUP, STARTUP, RUNTIME, CLOCK, TEMPCO, TEMPCO_CAPTURE, OWNERSHIP, USB_KEEP, USB_OFF, MEMORY_PREFLIGHT, SYSTEM_FULL, SYSTEM_MINIMAL, DEBUG_STATE, DEBUG_FAILURE, TIMER_PERIOD, PERFORMANCE, AP3_PERFORMANCE)
