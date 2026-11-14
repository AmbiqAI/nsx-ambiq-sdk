"""New selections preserve the floor and activate the requested built-in BSP."""
from dataclasses import replace
import json

import pytest

from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.suites import BSP_V2_SYSTEM, PSRAM_COMMON, PSRAM_BLOCKING, PSRAM_ASYNC_MATRIX
from tools.sdk_qualification import Plan, Revisions, Row, initial_report
from test_small_m55_memory import compiler_for_test


def test_explicit_extra_rows_preserve_paired_common_contract():
    plan = Plan(Revisions('a'*40, 'b'*40, 'c'*40))
    extra = []
    for board in ('apollo510_evb', 'apollo510b_evb'):
        extra.append(Row(board, PSRAM_COMMON.name, PSRAM_COMMON.cases))
        for suite in (PSRAM_BLOCKING, *PSRAM_ASYNC_MATRIX):
            extra.append(Row(board, suite.name, suite.cases, comparison='candidate-only',
                             reason='Candidate DMA/cache contract or new async API'))
    for board in ('apollo330mP_evb', 'apollo510_evb'):
        extra.append(Row(board, BSP_V2_SYSTEM.name, BSP_V2_SYSTEM.cases,
                         comparison='candidate-only', reason='Opt-in v2 API absent from baseline'))
    revised = replace(plan, rows=(*plan.rows, *extra))
    assert len(initial_report(revised)['requirements']) == 74  # Before 3 representative existing smokes.
    assert all(row.sides == ('baseline', 'candidate') for row in extra if row.suite == 'psram-common')
    for board in ('apollo3p_evb', 'apollo510b_evb'):
        with pytest.raises(ValueError, match='Unqualified board/suite'):
            replace(plan, rows=(*plan.rows, Row(board, BSP_V2_SYSTEM.name, BSP_V2_SYSTEM.cases)))


@pytest.mark.parametrize('board', ('apollo330mP_evb', 'apollo510_evb'))
def test_bsp_v2_firmware_uses_actual_core_private_contract(repo_root, tmp_path, board):
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, BSP_V2_SYSTEM))
    entries = json.loads((tmp_path/'compile_commands.json').read_text())
    core = next(entry['command'] for entry in entries if entry['file'].endswith('/nsx_system.c'))
    debug = next(entry['command'] for entry in entries if entry['file'].endswith('/nsx_debug.c'))
    for command in (core, debug):
        for definition in ('NSX_CORE_BSP_CONTRACT=2', 'NSX_CORE_DEBUG_ITM=1', 'NSX_CORE_DEBUG_UART=1'):
            assert '-D'+definition in command
