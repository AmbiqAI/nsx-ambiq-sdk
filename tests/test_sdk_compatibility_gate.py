"""Prove missing, skipped and failing compatibility evidence cannot pass."""
import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

from tools.sdk_compatibility import Gate, Requirement, belongs, GCC


@pytest.mark.parametrize('node,expected', [
    ('test.py::test_a', True), ('test.py::test_a[x]', True),
    ('test.py::test_another', False), ('other.py::test_a', False),
])
def test_selector_boundary(node, expected):
    assert belongs(node, 'test.py::test_a') == expected


def test_minimum_matrix_is_independent_of_collection():
    gate = Gate((Requirement('test.py::test_a', 2),))
    gate.selected = ['test.py::test_a[one]']
    gate.phases = {gate.selected[0]: dict.fromkeys(('setup', 'call', 'teardown'), 'passed')}
    assert gate.report(0)['status'] == 'incomplete'
    assert gate.report(0)['missing_requirements'] == ['test.py::test_a']


def test_filter_uses_compiler_parameters_not_substrings():
    selector = GCC[0].selector
    def item(family):
        return SimpleNamespace(nodeid=selector + f'[{family}-atfe-artifact]',
                               callspec=SimpleNamespace(params={'family': family, 'selection': 'atfe'}))
    items = [item('gcc'), item('atfe'), item('armclang')]
    config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=lambda **kw: None))
    gate = Gate(GCC)
    gate.pytest_collection_modifyitems(config, items)
    assert len(items) == 1
    assert items[0].callspec.params['family'] == 'gcc'


@pytest.mark.parametrize('body,status', [
    ('def test_case():\n    pass\n', 'passed'),
    ('def test_case():\n    assert False\n', 'failed'),
    ('import pytest\ndef test_case():\n    pytest.skip("compiler unavailable")\n', 'incomplete'),
    ('import pytest\n@pytest.fixture(autouse=True)\ndef resource():\n    yield\n    raise RuntimeError("cleanup")\ndef test_case():\n    pass\n', 'failed'),
    ('def test_other():\n    pass\n', 'incomplete'),
    ('this is not valid python!', 'incomplete'),
])
def test_real_pytest_gate(repo_root: Path, tmp_path: Path, body, status):
    (tmp_path / 'test_sample.py').write_text(body)
    (tmp_path / 'pytest.ini').write_text('[pytest]\n')
    script = '''
import json
from pathlib import Path
import pytest
from tools.sdk_compatibility import Gate, Requirement
gate = Gate((Requirement('test_sample.py::test_case'),))
code = pytest.main(['-q', 'test_sample.py'], plugins=[gate])
Path('report.json').write_text(json.dumps(gate.report(code)))
'''
    env = os.environ.copy()
    env['PYTHONPATH'] = str(repo_root)
    result = subprocess.run([sys.executable, '-c', script], cwd=tmp_path, env=env,
                            capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    assert json.loads((tmp_path / 'report.json').read_text())['status'] == status


def test_same_count_cannot_hide_missing_board():
    gate = Gate((Requirement('test.py::test_a', 2, ('board_a', 'board_b')),))
    gate.selected = ['test.py::test_a[one]', 'test.py::test_a[two]']
    gate.boards = dict.fromkeys(gate.selected, 'board_a')
    gate.phases = {n: dict.fromkeys(('setup', 'call', 'teardown'), 'passed') for n in gate.selected}
    report = gate.report(0)
    assert report['status'] == 'incomplete'
    assert report['missing_boards'] == {'test.py::test_a': ['board_b']}


@pytest.mark.parametrize('extra_options', [False, True])
def test_cli_refuses_reused_evidence_and_selection_overrides(repo_root: Path, tmp_path: Path, extra_options):
    output = tmp_path / 'evidence'
    output.mkdir()
    prior = output / 'compatibility.json'
    prior.write_text('preserved evidence')
    env = os.environ.copy()
    env.pop('PYTEST_ADDOPTS', None)
    if extra_options:
        env['PYTEST_ADDOPTS'] = '-k nonexistent'
    result = subprocess.run([sys.executable, '-m', 'tools.sdk_compatibility',
                             '--profile', 'gcc', '--output', str(output)],
                            cwd=repo_root, env=env, capture_output=True, text=True, timeout=30)
    assert result.returncode == 2
    assert ('PYTEST_ADDOPTS' if extra_options else 'new directory') in result.stderr
    assert prior.read_text() == 'preserved evidence'


def test_same_abi_count_cannot_hide_missing_combinations():
    requirement = GCC[0]
    gate = Gate((requirement,))
    items = [SimpleNamespace(nodeid=requirement.selector + f'[repeat{i}]',
             callspec=SimpleNamespace(params={'family': 'gcc', 'language': 'C', 'selection': 'default'}))
             for i in range(6)]
    config = SimpleNamespace(hook=SimpleNamespace(pytest_deselected=lambda **kw: None))
    gate.pytest_collection_modifyitems(config, items)
    gate.phases = {n: dict.fromkeys(('setup', 'call', 'teardown'), 'passed') for n in gate.selected}
    report = gate.report(0)
    assert report['status'] == 'incomplete'
    assert len(report['missing_variants'][requirement.selector]) == 5
    for node, variant in zip(gate.selected, requirement.variants):
        gate.parameters[node] = dict(zip(requirement.parameters, variant))
    assert gate.report(0)['status'] == 'passed'
