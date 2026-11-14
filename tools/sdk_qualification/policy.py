"""Reviewed minimum contracts; neither pytest collection nor inventory defines them."""
from .models import CompilerCoverage, Gate, Row


PROFILE = 'local-five-board-gcc-v1'
BOARDS = ('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo330mP_evb',
          'apollo510_evb', 'apollo510b_evb')

# Deliberately independent of the executable suite registry. A case removed
# there must require a policy review rather than silently reducing this floor.
CORE_CASES = {
    'startup': ('initialized_data', 'zeroed_bss', 'cpp_constructor'),
    'runtime': ('stack_bounds_alignment', 'heap_allocation', 'heap_limit_rejection', 'svc_dispatch'),
    'startup-ownership': ('initialize_once', 'reject_reapplication'),
    'system-full': ('reject_then_start',),
}

CORE_GATES = (
    Gate('intake-review', 'intake', 'Revision-bound source, patch and artifact review'),
    Gate('candidate-host-gcc', 'host', 'Candidate GCC compatibility gate, including payload/ABI checks'),
    Gate('candidate-hygiene', 'host', 'Candidate pre-commit and full host checks'),
)
CORE_COMPILER = CompilerCoverage('gcc', BOARDS, ('build', 'link', 'abi'))

NON_AP3 = BOARDS[1:]
PSRAM_BOARDS = BOARDS[-2:]
SUPPORTED = {name: BOARDS for name in (*CORE_CASES, 'system-minimal', 'debug-state')}
SUPPORTED.update({name: NON_AP3 for name in ('timer-period', 'performance')})
SUPPORTED['ap3-performance'] = BOARDS[:1]
SUPPORTED['power-startup'] = BOARDS[2:]
SUPPORTED['debug-failure'] = ('apollo330mP_evb',)
SUPPORTED.update({name: ('apollo4p_blue_kxr_evb',) for name in (
    'tempco-lifecycle', 'tempco-capture', 'usb-keep', 'usb-off', 'memory-preflight')})
SUPPORTED.update({name: PSRAM_BOARDS for name in (
    'psram-blocking', 'psram-async', 'psram-async-lp-96',
    'psram-async-hp-48', 'psram-async-hp-96')})
# The clock diagnostic is experimental, not a qualification row.


def core_rows() -> tuple[Row, ...]:
    return tuple(Row(board, name, cases) for board in BOARDS for name, cases in CORE_CASES.items())


def validate_rows(rows: tuple[Row, ...]) -> None:
    from tools.hardware.suites import SUITES
    registry = {suite.name: suite for suite in SUITES}
    by_id = {row.id: row for row in rows}
    if len(by_id) != len(rows):
        raise ValueError('Duplicate/conflicting board-suite requirements')
    for expected in core_rows():
        actual = by_id.get(expected.id)
        if actual is None or actual.cases != expected.cases or actual.execution != 'hardware':
            raise ValueError(f'Required hardware contract missing or weakened: {expected.id}')
        if expected.suite in ('startup', 'runtime') and actual.comparison != 'paired':
            raise ValueError(f'Core CRT/runtime comparison must be paired: {expected.id}')
    for row in rows:
        if row.suite not in SUPPORTED or row.board not in SUPPORTED[row.suite]:
            raise ValueError(f'Unqualified board/suite combination: {row.id}')
        suite = registry.get(row.suite)
        if suite is None or suite.cases != row.cases:
            raise ValueError(f'Harness suite/case contract drift: {row.id}')
