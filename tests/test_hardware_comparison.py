"""Separate-root builds and fail-closed comparison orchestration."""
from contextlib import contextmanager
import json
from pathlib import Path
import subprocess
from types import SimpleNamespace

import pytest

from tools.hardware.api import BuildRequest, BoardRequest, build_firmware
from tools.hardware import compare as comparison
from tools.hardware.compare import ComparisonRequest
from tools.hardware.suites import CLOCK, STARTUP
from tools.hardware.provenance import dependency_inputs
from test_small_m55_memory import compiler_for_test


def test_candidate_tests_are_not_loaded_but_production_headers_are(repo_root, tmp_path):
    candidate = tmp_path / 'candidate'
    subprocess.run(['git', 'clone', '--local', '--shared', '--quiet', str(repo_root), str(candidate)],
                   check=True, capture_output=True, timeout=60)
    # This must never be configured: test code belongs to the running harness.
    (candidate / 'tests/hardware/common/firmware.cmake').write_text('message(FATAL_ERROR "candidate test loaded")\n')
    compiler = compiler_for_test('gcc')
    output = tmp_path / 'normal'
    build_firmware(BuildRequest(repo_root, output, 'apollo510b_evb', compiler.parent.parent,
                                CLOCK, sdk_root=candidate))
    record = json.loads((output / 'manifest.json').read_text())
    assert record['schema_version'] == 3
    assert record['harness']['root'] == str(repo_root)
    assert record['sdk']['root'] == str(candidate)
    assert record['sdk']['dirty']
    assert str(repo_root / 'tests/hardware/clock/main.c') in record['input_roles']['harness']
    header = candidate / 'modules/nsx-power/includes-api/nsx_power.h'
    assert str(header) in record['input_roles']['sdk']
    assert str(candidate / 'boards/apollo510b_evb/board.cmake') in record['input_roles']['sdk']
    assert not any('/candidate/tests/' in p for p in record['inputs_sha256'])
    header.write_text('#error candidate_production_header_used\n' + header.read_text())
    with pytest.raises(RuntimeError, match='Command failed'):
        build_firmware(BuildRequest(repo_root, tmp_path / 'broken', 'apollo510b_evb',
                                    compiler.parent.parent, CLOCK, sdk_root=candidate))
    assert 'candidate_production_header_used' in (tmp_path / 'broken/build.log').read_text()


def test_dependency_paths_with_spaces(tmp_path):
    header = tmp_path / 'a header.h'
    header.write_text('')
    assert dependency_inputs('file.o: #deps 1\n    a header.h\n', tmp_path) == [header]


@pytest.fixture
def comparison_fakes(tmp_path, monkeypatch):
    events = []
    monkeypatch.setattr(comparison, 'checkout', lambda root: {'root': str(root), 'revision': 'a'*40, 'dirty': False})
    def build(request):
        events.append(('build', request.sdk_root))
        return SimpleNamespace(build_id=request.sdk_root.name, device='AP510BFA-CBR')
    monkeypatch.setattr(comparison, 'build_firmware', build)
    @contextmanager
    def session(request):
        events.append(('open', request.probe))
        try:
            yield SimpleNamespace(run=lambda image, output: events.append(('run', image.build_id)))
        finally:
            events.append(('close', request.probe))
    monkeypatch.setattr(comparison, 'open_board', session)
    return events


def request(tmp_path, hardware=False):
    return ComparisonRequest(tmp_path/'baseline', tmp_path/'candidate', tmp_path/'output',
                             'apollo510b_evb', tmp_path/'gcc', STARTUP,
                             BoardRequest('999999995') if hardware else None)


@pytest.mark.parametrize('hardware', [False, True])
def test_comparison_builds_both_before_single_board_lease(tmp_path, comparison_fakes, hardware):
    report = comparison.compare(request(tmp_path, hardware))
    assert report['status'] == ('passed' if hardware else 'built')
    assert report['qualification'] is False
    assert [e[0] for e in comparison_fakes] == (['build','build','open','run','run','close'] if hardware else ['build','build'])


def test_dirty_checkout_refused_before_build(tmp_path, comparison_fakes, monkeypatch):
    monkeypatch.setattr(comparison, 'checkout', lambda root: {'dirty': True})
    with pytest.raises(ValueError, match='clean committed'):
        comparison.compare(request(tmp_path))
    assert not comparison_fakes
    assert json.loads((tmp_path/'output/comparison.json').read_text())['status'] == 'failed'


def test_failure_never_leaves_success_report(tmp_path, comparison_fakes, monkeypatch):
    def fail(request):
        raise RuntimeError('candidate failure')
    monkeypatch.setattr(comparison, 'build_firmware', fail)
    with pytest.raises(RuntimeError, match='candidate failure'):
        comparison.compare(request(tmp_path))
    assert json.loads((tmp_path/'output/comparison.json').read_text())['status'] == 'failed'


def test_checkout_change_invalidates_completed_builds(tmp_path, comparison_fakes, monkeypatch):
    n = 0
    def checkout(root):
        nonlocal n
        n += 1
        return {'dirty': False, 'revision': 'old' if n<=3 else 'new'}
    monkeypatch.setattr(comparison, 'checkout', checkout)
    with pytest.raises(RuntimeError, match='changed during'):
        comparison.compare(request(tmp_path))
    assert json.loads((tmp_path/'output/comparison.json').read_text())['status'] == 'failed'


def test_existing_evidence_is_not_overwritten(tmp_path, comparison_fakes):
    output=tmp_path/'output'
    output.mkdir()
    (output/'comparison.json').write_text('original')
    with pytest.raises(ValueError, match='new or empty'):
        comparison.compare(request(tmp_path))
    assert (output/'comparison.json').read_text() == 'original'
