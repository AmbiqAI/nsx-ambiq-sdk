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
def test_required_evidence(phases, hardware, session_ok, expected):
    q = Qualification(PROFILES['apollo3p_evb-gcc-startup'])
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
    ('def test_startup():\n    pass\n', 'passed'),
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


def test_every_required_suite_must_complete():
    second = Suite('second', 'tests/hardware/test_second.py::test_second', ('check',))
    q = Qualification(Profile('two-suites', 'apollo3p_evb', 'gcc', (STARTUP, second)))
    for phase in ('setup', 'call', 'teardown'):
        q.record(STARTUP.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == 'incomplete'
    for phase in ('setup', 'call', 'teardown'):
        q.record(second.nodeid, phase, 'passed')
    assert q.report(True, True)['status'] == 'passed'
