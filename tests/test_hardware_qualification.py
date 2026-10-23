"""Qualification must not turn absent hardware evidence into a pass."""
import json
from pathlib import Path
import subprocess
import sys

import pytest

from tools.hardware.qualification import PROFILES, Profile, Qualification, STARTUP, Suite


@pytest.mark.parametrize('phases,hardware,session_ok,expected', [
    ({}, True, True, 'incomplete'),
    ({'setup': 'skipped'}, True, True, 'incomplete'),
    ({'setup': 'passed', 'call': 'passed'}, True, True, 'incomplete'),
    ({'setup': 'passed', 'call': 'passed', 'teardown': 'failed'}, True, False, 'failed'),
    ({'setup': 'passed', 'call': 'passed', 'teardown': 'passed'}, False, True, 'incomplete'),
    ({'setup': 'passed', 'call': 'passed', 'teardown': 'passed'}, True, False, 'incomplete'),
    ({'setup': 'passed', 'call': 'passed', 'teardown': 'passed'}, True, True, 'passed'),
])
def test_required_evidence(tmp_path, phases, hardware, session_ok, expected):
    q = Qualification(PROFILES['apollo3p_evb-gcc-startup'])
    bind_execution(q, STARTUP, tmp_path)
    for phase, outcome in phases.items():
        q.record(STARTUP.nodeid, phase, outcome)
    q.record('unrelated::test', 'call', 'passed')
    assert q.report(session_ok, hardware)['status'] == expected


@pytest.mark.parametrize('selection', [[], ['-k', 'nonexistent_case']])
def test_real_pytest_missing_execution_fails(repo_root: Path, tmp_path: Path, selection):
    report = tmp_path / 'qualification.json'
    report.write_text('{"status":"passed"}')
    result = subprocess.run([sys.executable, '-m', 'pytest', '-q',
        'tests/hardware', '--qualification-profile', 'apollo3p_evb-gcc-startup',
        '--qualification-report', str(report), *selection], cwd=repo_root,
        capture_output=True, text=True, timeout=30)
    assert result.returncode != 0, result.stdout + result.stderr
    data = json.loads(report.read_text())
    assert data['status'] == 'incomplete'
    assert data['suites'][0]['status'] == 'incomplete'


@pytest.mark.parametrize('extra', [ ['--board', 'apollo510_evb'], ['--fault-injection', 'assertion'] ])
def test_profile_rejects_conflicts(repo_root: Path, tmp_path: Path, extra):
    result = subprocess.run([sys.executable, '-m', 'pytest', '-q', 'tests/hardware',
        '--qualification-profile', 'apollo3p_evb-gcc-startup',
        '--qualification-report', str(tmp_path / 'report.json'), *extra],
        cwd=repo_root, capture_output=True, text=True, timeout=30)
    assert result.returncode == 4


@pytest.mark.parametrize('body,expected', [
    ('def test_startup():\n    pass\n', 'incomplete'),
    ('def test_startup():\n    assert False\n', 'failed'),
    ('import pytest\n@pytest.mark.skip(reason="fixture unavailable")\ndef test_startup():\n    pass\n', 'incomplete'),
    ('import pytest\n@pytest.fixture(autouse=True)\ndef resource():\n    yield\n    raise RuntimeError("cleanup failed")\ndef test_startup():\n    pass\n', 'failed'),
])
def test_pytest_report_lifecycle(repo_root: Path, tmp_path: Path, body, expected):
    # Synthetic test bodies exercise orchestration only; no probe is opened.
    import os
    suite = tmp_path / 'tests/hardware'
    suite.mkdir(parents=True)
    (suite / 'test_startup.py').write_text(body)
    (tmp_path / 'conftest.py').write_text('pytest_plugins = ["tools.hardware.pytest_plugin"]\n')
    (tmp_path / 'pytest.ini').write_text('[pytest]\n')
    report = tmp_path / 'report.json'
    env = os.environ.copy()
    env['PYTHONPATH'] = str(repo_root)
    result = subprocess.run([sys.executable, '-m', 'pytest', '-q', 'tests/hardware',
        '--qualification-profile', 'apollo3p_evb-gcc-startup', '--hardware',
        '--qualification-report', str(report)], cwd=tmp_path, env=env,
        capture_output=True, text=True, timeout=30)
    assert json.loads(report.read_text())['status'] == expected, result.stdout + result.stderr
    assert (result.returncode == 0) == (expected == 'passed')


def test_every_required_suite_must_complete(tmp_path):
    second = Suite('second', 'tests/hardware/test_second.py::test_second', ('check',))
    q = Qualification(Profile('two-suites', 'apollo3p_evb', 'gcc', (STARTUP, second)))
    bind_execution(q, STARTUP, tmp_path / 'startup')
    for phase in ('setup', 'call', 'teardown'):
        q.record(STARTUP.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == 'incomplete'
    bind_execution(q, second, tmp_path / 'second')
    for phase in ('setup', 'call', 'teardown'):
        q.record(second.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == 'passed'


def evidence_packet(cases, identity='test-build'):
    from tools.hardware.protocol import HEADER, MAGIC, VERSION, RESULT_SIZE
    text = (''.join(f'suite.c:1:{name}:PASS\n' for name in cases) +
            f'{len(cases)} Tests 0 Failures 0 Ignored\nOK\n').encode()
    head = HEADER.pack(MAGIC, VERSION, 2, len(cases), 0, 0, len(text), 0, identity.encode())
    return head + text + bytes(RESULT_SIZE - len(head) - len(text))


def bind_execution(qualification, suite, output):
    from tools.hardware.build import Firmware
    from tools.hardware.protocol import decode
    output.mkdir(parents=True, exist_ok=True)
    image = Firmware(output / 'test.elf', 0, 'test-build', qualification.profile.board, 'device', 4000, suite.cases)
    qualification.built(suite.nodeid, suite, image, output)
    (output / 'execution.json').write_text(json.dumps({'status': 'passed', 'board': image.board}))
    raw = evidence_packet(suite.cases)
    (output / 'last-result.bin').write_bytes(raw)
    qualification.executed(suite.nodeid, decode(raw, image.build_id, suite.cases))


def test_successful_phases_without_execution_are_incomplete():
    q = Qualification(PROFILES['apollo3p_evb-gcc-startup'])
    for phase in ('setup', 'call', 'teardown'):
        q.record(STARTUP.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == 'incomplete'


@pytest.mark.parametrize('mode', ['correct', 'wrong_suite', 'wrong_cases', 'missing_evidence', 'stale', 'failed_execution'])
def test_real_fixture_contract_binding(repo_root, tmp_path, mode):
    import os
    suite_dir = tmp_path / 'tests/hardware'
    suite_dir.mkdir(parents=True)
    (suite_dir / 'test_startup.py').write_text((repo_root / 'tests/hardware/test_startup.py').read_text())
    (tmp_path / 'pytest.ini').write_text('[pytest]\n')
    conftest = r"""
import json
from contextlib import contextmanager
from dataclasses import replace
from pathlib import Path
import pytest
from tools.hardware import pytest_plugin
from tools.hardware.build import Firmware
from tools.hardware.suites import STARTUP
from tools.hardware.protocol import Result
from test_hardware_qualification import evidence_packet
pytest_plugins = ['tools.hardware.pytest_plugin']
@pytest.fixture
def repo_root(): return Path.cwd()
@pytest.fixture
def suite_spec():
    if MODE == 'wrong_suite': return replace(STARTUP, name='other', cases=('one', 'two'))
    if MODE == 'wrong_cases': return replace(STARTUP, cases=('one', 'two'))
    return STARTUP
@pytest.fixture(autouse=True)
def simulate(monkeypatch):
    def build(request):
        Path('build-called').touch()
        request.output.mkdir(parents=True)
        return Firmware(request.output / 'test.elf', 0, 'test-build', request.board, 'device', 4000, request.suite.cases)
    class Session:
        def run(self, image, output):
            raw = evidence_packet(image.cases, 'stale-build' if MODE == 'stale' else image.build_id)
            if MODE != 'missing_evidence':
                (output / 'execution.json').write_text(json.dumps({
                    'status': 'failed' if MODE == 'failed_execution' else 'passed', 'board': image.board}))
                (output / 'last-result.bin').write_bytes(raw)
            from tools.hardware.protocol import HEADER
            text = raw[HEADER.size:].rstrip(bytes([0])).decode()
            return Result(image.build_id, len(image.cases), 0, 0, text)
    @contextmanager
    def open_board(request): yield Session()
    monkeypatch.setattr(pytest_plugin, 'build_firmware', build)
    monkeypatch.setattr(pytest_plugin, 'open_board', open_board)
"""
    (tmp_path / 'conftest.py').write_text(f'MODE = {mode!r}\n' + conftest)
    env = os.environ.copy()
    env['PYTHONPATH'] = os.pathsep.join((str(repo_root), str(repo_root / 'tests')))
    report = tmp_path / 'report.json'
    result = subprocess.run([sys.executable, '-m', 'pytest', '-q', 'tests/hardware',
        '--hardware', '--probe', '999999990', '--gcc-root', str(tmp_path),
        '--qualification-profile', 'apollo3p_evb-gcc-startup', '--qualification-report', str(report)],
        cwd=tmp_path, env=env, capture_output=True, text=True, timeout=30)
    data = json.loads(report.read_text())
    assert (result.returncode == 0) == (mode == 'correct'), result.stdout + result.stderr
    assert (data['status'] == 'passed') == (mode == 'correct')
    if mode.startswith('wrong_'):
        assert not (tmp_path / 'build-called').exists()
    if mode == 'correct':
        assert data['suites'][0]['build_id'] == 'test-build'
        assert data['suites'][0]['evidence_directory'] is not None
