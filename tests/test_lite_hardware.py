"""Local Lite smoke requirements; no farm or physical-identity credit from builds."""
import json
import subprocess
import sys

import pytest

from test_hardware_qualification import bind_execution
from test_small_m55_memory import compiler_for_test
from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.qualification import PROFILES, Qualification
from tools.hardware.suites import STARTUP, RUNTIME, OWNERSHIP, SYSTEM_FULL


BOARDS = ('apollo4l_blue_evb', 'apollo510dL_evb')
SUITES = (STARTUP, RUNTIME, OWNERSHIP, SYSTEM_FULL)
NODES = tuple(suite.nodeid for suite in SUITES)


@pytest.mark.parametrize('board', BOARDS)
@pytest.mark.parametrize('omitted', (None, *SUITES), ids=('complete', 'no-crt', 'no-runtime', 'no-ownership', 'no-system'))
def test_lite_profile_requires_each_contract(tmp_path, board, omitted):
    q = Qualification(PROFILES[f'{board}-gcc-lite-smoke'])
    assert q.profile.board == board
    assert q.profile.suites == SUITES
    assert sum(len(suite.cases) for suite in q.profile.suites) == 10
    for suite in SUITES:
        if suite == omitted:
            continue
        bind_execution(q, suite, tmp_path / suite.name)
        for phase in ('setup', 'call', 'teardown'):
            q.record(suite.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == ('passed' if omitted is None else 'incomplete')
    assert q.report(True, False)['status'] == 'incomplete'


@pytest.mark.parametrize('board', BOARDS)
def test_lite_profile_collects_exactly_four_suites(repo_root, tmp_path, board):
    report = tmp_path / 'qualification.json'
    result = subprocess.run([sys.executable, '-m', 'pytest', '--collect-only', '-q',
        *NODES, '--qualification-profile', f'{board}-gcc-lite-smoke',
        '--qualification-report', str(report)], cwd=repo_root,
        capture_output=True, text=True, timeout=30)
    collected = tuple(line for line in result.stdout.splitlines() if line.startswith('tests/hardware/'))
    assert collected == NODES, result.stdout + result.stderr
    assert result.returncode != 0
    data = json.loads(report.read_text())
    assert data['status'] == 'incomplete'
    assert len(data['suites']) == 4


@pytest.mark.parametrize('board', BOARDS)
def test_lite_deselection_cannot_pass(repo_root, tmp_path, board):
    report = tmp_path / 'qualification.json'
    result = subprocess.run([sys.executable, '-m', 'pytest', '-q', *NODES,
        '--qualification-profile', f'{board}-gcc-lite-smoke', '--qualification-report', str(report),
        '-k', 'nonexistent_case'], cwd=repo_root, capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert json.loads(report.read_text())['status'] == 'incomplete'


@pytest.mark.parametrize('board', BOARDS)
@pytest.mark.parametrize('suite', SUITES, ids=lambda suite: suite.name)
def test_lite_real_firmware_builds(repo_root, tmp_path, board, suite):
    compiler = compiler_for_test('gcc')
    firmware = build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, suite))
    manifest = json.loads((tmp_path / 'manifest.json').read_text())
    assert firmware.board == board and firmware.cases == suite.cases
    assert manifest['board'] == board and manifest['expected_cases'] == list(suite.cases)
    assert manifest['device'] == firmware.device
    if board == 'apollo510dL_evb':
        assert firmware.device == 'AP510DLA-CBR'
    commands = json.loads((tmp_path / 'compile_commands.json').read_text())
    cpu = 'cortex-m4' if board == 'apollo4l_blue_evb' else 'cortex-m55'
    assert any(f'-mcpu={cpu}' in entry['command'] for entry in commands)
    attributes = subprocess.check_output([str(compiler.with_name('arm-none-eabi-readelf')),
                                          '-A', str(firmware.elf)], text=True)
    assert 'Tag_ABI_VFP_args: VFP registers' in attributes


def test_lite_local_support_does_not_expand_farm_qualification():
    from tools.sdk_qualification.policy import BOARDS as qualification_boards
    from tools.sdk_qualification.pr_reporting.models import BOARDS as check_boards
    assert not set(BOARDS).intersection(qualification_boards)
    assert not set(BOARDS).intersection(check_boards)
