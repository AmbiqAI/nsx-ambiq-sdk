"""Local preflight and aggregation contracts; no network or probe access."""
from copy import deepcopy
from dataclasses import asdict
import json
from pathlib import Path
from types import SimpleNamespace
import uuid

import pytest

from tools.sdk_qualification import evidence, runtime
from tools.sdk_qualification.assessment import Collection, assess
from tools.sdk_qualification.io import contained, read_record, sha256, write_record
from tools.sdk_qualification.models import Revisions
from tools.sdk_qualification.plan import Plan
from tools.sdk_qualification.policy import BOARDS
from tools.sdk_qualification.runtime import Binary, Binding, Config, Tools, inventory


@pytest.fixture
def setup(tmp_path, monkeypatch):
    plan = Plan(Revisions('a' * 40, 'b' * 40, 'c' * 40))
    roots = {name: {'root': str(tmp_path / name), 'revision': getattr(plan.revisions, name), 'dirty': False}
             for name in ('baseline', 'candidate', 'harness')}
    tools = Tools('/gcc', '/cmake', tuple(Binary(name, '/gcc/bin/arm-none-eabi-' + name, 'd' * 64,
                                               'gcc proof' if name == 'gcc' else name + ' proof')
                                         for name in ('gcc', 'nm')))
    return SimpleNamespace(plan=plan, roots=roots, tools=tools, root=tmp_path)


def record(setup, directory, mode, boards=(), reference=None):
    directory.mkdir(parents=True)
    log = directory / 'command.log'
    log.write_text('retained command output')
    labels = ['intake-review', 'candidate-host-gcc', 'candidate-hygiene', 'candidate-host-tests'] if mode == 'host' else ['build']
    body = dict(execution_id=uuid.uuid4().hex, plan_sha256=setup.plan.digest,
                origin_output=str(directory), revisions=asdict(setup.plan.revisions), roots=setup.roots,
                tools=setup.tools.to_dict(), scope={'mode': mode, 'boards': list(boards)},
                scope_status='passed', qualification=False, global_status='incomplete', inputs_rechecked=True,
                firmware=[], host={} if mode == 'host' else None, host_reference=reference,
                calls=[{'label': label, 'outcome': 'passed', 'returncode': 0, 'log': log.name} for label in labels],
                started_at='recorded', completed_at='recorded', elapsed_seconds=1,
                cleanup={'process_groups': 'completed', 'board_state': 'tests-completed' if mode == 'hardware' else 'not-accessed'},
                error=None, files={log.name: sha256(log)})
    for row in setup.plan.rows:
        if row.board in boards:
            for side in row.sides:
                output = directory / row.id / side
                output.mkdir(parents=True)
                body['firmware'].append(dict(row=row.id, side=side, directory=str(output.relative_to(directory)),
                                            origin=str(output), hardware=mode == 'hardware',
                                            binding=asdict(Binding(row.board, str(100 + BOARDS.index(row.board)), 'device'))
                                            if mode == 'hardware' else None))
    write_record(directory, body)
    return body


def rewrite(directory, body):
    (directory / 'record.json').unlink()
    write_record(directory, body)


@pytest.fixture
def collection(setup, monkeypatch):
    # Isolate the collection algorithm from compiler/transport validation. Those
    # validators have separate negative controls and a real GCC rehearsal.
    monkeypatch.setattr('tools.sdk_qualification.assessment.verify_host', lambda *args: None)
    monkeypatch.setattr('tools.sdk_qualification.assessment.verify_firmware', lambda *args: None)
    return Collection(setup.plan, setup.roots, setup.tools, None)


def test_build_scope_never_satisfies_hardware_rows(setup, collection):
    directory = setup.root / 'build'
    record(setup, directory, 'build', (BOARDS[0],))
    collection.add(directory)
    assert len(collection.passed) == 3  # Only that board's concrete compiler obligations.
    assert all(key[0].startswith('compiler/') for key in collection.passed)


def test_all_board_and_host_scopes_complete_exact_plan(setup, collection):
    host = setup.root / 'host'
    record(setup, host, 'host')
    collection.add(host)
    digest = read_record(host)['sha256']
    import shutil
    for index, board in enumerate(BOARDS):
        directory = setup.root / f'board{index}'
        record(setup, directory, 'hardware', (board,), {'directory': 'host', 'sha256': digest})
        shutil.copytree(host, directory / 'host')
        collection.add(directory)
    assert len(collection.passed) == 58
    assert len(collection.references) == 6  # One host execution reused by five boards.
    collection.add(host)
    assert len(collection.references) == 6


@pytest.mark.parametrize('change,match', [
    (lambda body: body.update(scope_status='failed'), 'complete successfully'),
    (lambda body: body.update(inputs_rechecked=False), 'stable inputs'),
    (lambda body: body.update(qualification=True), 'cannot assert'),
    (lambda body: body['cleanup'].update(process_groups='unknown'), 'lifecycle'),
    (lambda body: body['calls'][0].update(returncode=1), 'command'),
    (lambda body: body['revisions'].update(candidate='f' * 40), 'match its plan'),
    (lambda body: body['tools']['binaries'][0].update(sha256='0' * 64), 'tool identities'),
    (lambda body: body['roots']['candidate'].update(dirty=True), 'dirty state'),
    (lambda body: body.update(host_reference=None), 'prior host'),
])
def test_invalid_record_header_cannot_contribute(setup, collection, change, match):
    directory = setup.root / 'bad'
    body = record(setup, directory, 'hardware', (BOARDS[0],), {'directory': 'host', 'sha256': 'x'})
    change(body)
    rewrite(directory, body)
    with pytest.raises(ValueError, match=match):
        collection.add(directory)
    assert not collection.passed


@pytest.mark.parametrize('change', [
    lambda body: body['firmware'].pop(),
    lambda body: body['firmware'].append(deepcopy(body['firmware'][0])),
    lambda body: body['firmware'][0].update(side='other'),
    lambda body: body['firmware'][0].update(hardware=True),
    lambda body: body['firmware'][0].update(origin='/unrelated/build'),
])
def test_unplanned_missing_or_wrong_execution_kind_rejected(setup, collection, change):
    directory = setup.root / 'bad'
    body = record(setup, directory, 'build', (BOARDS[0],))
    change(body)
    rewrite(directory, body)
    with pytest.raises(ValueError):
        collection.add(directory)


def test_corrupt_duplicate_copy_does_not_reuse_earlier_pass(setup, collection):
    directory = setup.root / 'host'
    record(setup, directory, 'host')
    collection.add(directory)
    (directory / 'command.log').write_text('changed')
    with pytest.raises(ValueError, match='Evidence file changed'):
        collection.add(directory)


def test_different_executions_cannot_win_same_obligation(setup, collection):
    for name in ('host1', 'host2'):
        record(setup, setup.root / name, 'host')
    collection.add(setup.root / 'host1')
    with pytest.raises(ValueError, match='Competing executions'):
        collection.add(setup.root / 'host2')


def test_same_id_different_digest_rejected(setup, collection):
    first, second = setup.root / 'first', setup.root / 'second'
    body = record(setup, first, 'host')
    collection.add(first)
    record(setup, second, 'host')
    other = read_record(second)['body']
    other['execution_id'] = body['execution_id']
    rewrite(second, other)
    with pytest.raises(ValueError, match='share an execution ID'):
        collection.add(second)


def test_record_digest_and_duplicate_keys_rejected(setup):
    directory = setup.root / 'record'
    record(setup, directory, 'host')
    path = directory / 'record.json'
    text = path.read_text()
    path.write_text(text.replace('retained', 'altered').replace('"scope_status": "passed"', '"scope_status": "failed"'))
    with pytest.raises(ValueError, match='Invalid execution record'):
        read_record(directory)
    path.write_text('{"schema_version":1,"schema_version":1}')
    with pytest.raises(ValueError, match='Duplicate'):
        read_record(directory)


def test_evidence_path_rejects_traversal_and_symlink(tmp_path):
    root = tmp_path / 'root'
    root.mkdir()
    (root / 'link').symlink_to(tmp_path)
    for relative in ('../outside', '/absolute', 'link/outside'):
        with pytest.raises(ValueError):
            contained(root, relative)


@pytest.mark.parametrize('boards', [
    [{'board': BOARDS[0], 'probe': '123', 'device': 'D'}, {'board': BOARDS[1], 'probe': '00123', 'device': 'D'}],
    [{'board': BOARDS[0], 'probe': '123', 'device': 'D'}, {'board': BOARDS[0], 'probe': '124', 'device': 'D'}],
    [{'board': BOARDS[0], 'probe': 123, 'device': 'D'}],
    [{'board': BOARDS[0], 'probe': '0', 'device': 'D'}],
    [{'board': BOARDS[0], 'probe': '123', 'device': ''}],
])
def test_inventory_rejects_ambiguous_or_automatic_probe_selection(tmp_path, boards):
    path = tmp_path / 'inventory.json'
    path.write_text(json.dumps({'schema_version': 1, 'boards': boards}))
    with pytest.raises(ValueError):
        inventory(path)


def test_clean_planned_roots_and_fresh_external_output_required(setup, monkeypatch):
    monkeypatch.setattr(runtime, 'HARNESS_ROOT', setup.root / 'harness')
    monkeypatch.setattr(runtime, 'checkout', lambda path: deepcopy(setup.roots[path.name]))
    config = Config(setup.root / 'baseline', setup.root / 'candidate', Path('/gcc'), setup.root / 'out')
    assert config.prepare(setup.plan) == setup.roots
    with pytest.raises(ValueError, match='fresh'):
        config.prepare(setup.plan)
    nested = Config(config.baseline, config.candidate, Path('/gcc'), config.candidate / 'out')
    with pytest.raises(ValueError, match='outside'):
        nested.prepare(setup.plan)
    setup.roots['candidate']['dirty'] = True
    with pytest.raises(ValueError, match='clean planned'):
        config.roots(setup.plan)


def test_environment_clears_test_filters_and_pins_compiler(setup, monkeypatch):
    for key in ('PYTEST_ADDOPTS', 'PYTHONPATH', 'SKIP', 'GCC_ROOT'):
        monkeypatch.setenv(key, 'undesired')
    env = setup.tools.environment(setup.root / 'scratch')
    assert not any(key in env for key in ('PYTEST_ADDOPTS', 'PYTHONPATH', 'SKIP', 'GCC_ROOT'))
    assert env['PATH'].split(':')[0] == '/gcc/bin'
    assert env['NSX_REQUIRE_ARM_GCC'] == '1'
    assert env['PYTEST_DISABLE_PLUGIN_AUTOLOAD'] == '1'


def test_assessment_partial_is_incomplete_and_invalid_is_failed(setup, collection, monkeypatch):
    monkeypatch.setattr(Config, 'prepare', lambda self, plan: (self.output.mkdir(), setup.roots)[1])
    monkeypatch.setattr(Config, 'roots', lambda *args: setup.roots)
    monkeypatch.setattr(Tools, 'capture', lambda *args: setup.tools)
    monkeypatch.setattr(Tools, 'recheck', lambda self: None)  # Aggregation-only fixture has synthetic tool paths.
    directory = setup.root / 'build'
    body = record(setup, directory, 'build', (BOARDS[0],))
    config = Config(Path('/baseline'), Path('/candidate'), Path('/gcc'), setup.root / 'assessment')
    report = assess(setup.plan, config, (directory,))
    assert report['status'] == 'incomplete' and report['qualification'] is False
    assert sum(item['status'] == 'passed' for item in report['requirements']) == 3
    body['firmware'].pop()
    rewrite(directory, body)
    from dataclasses import replace
    failed = assess(setup.plan, replace(config, output=setup.root / 'assessment-bad'), (directory,))
    assert failed['status'] == 'failed' and failed['qualification'] is False
    assert failed['errors']


def test_generated_and_source_paths_rebase_without_old_checkouts(setup):
    origin = {name: dict(value, root='/old/' + name) for name, value in setup.roots.items()}
    output = setup.root / 'relocated-build'
    output.mkdir()
    old_tools = {'gcc_root': '/old/gcc', 'cmake_root': '/old/cmake'}
    for path, role, expected in (
        ('/old/candidate/include/header.h', 'sdk', setup.root / 'candidate/include/header.h'),
        ('/old/harness/test.c', 'harness', setup.root / 'harness/test.c'),
        ('/old/build/identity.h', 'generated', output / 'identity.h'),
        ('/old/gcc/include/stddef.h', 'environment', Path('/gcc/include/stddef.h')),
    ):
        assert evidence.input_path(path, role, origin, setup.roots, '/old/build', output,
                                   'candidate', old_tools, setup.tools) == expected


@pytest.fixture
def firmware_evidence(setup, monkeypatch):
    """Synthetic files exercise the real verifier; only binutils/ABI are stubbed."""
    from tools.hardware.protocol import HEADER, MAGIC, TEXT_SIZE, VERSION, RESULT_SIZE
    from tools.hardware.suites import RUNTIME
    row = next(row for row in setup.plan.rows if row.board == BOARDS[0] and row.suite == 'runtime')
    output = setup.root / 'firmware'
    output.mkdir()
    sdk = Path(setup.roots['candidate']['root'])
    for name in evidence.SDK_IDENTITIES:
        path = sdk / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(name)
    inputs = [sdk / name for name in ('startup.c', 'system.c', 'app.ld', 'hal.a', 'bsp.a')]
    for path in inputs:
        path.write_text(path.name)
    build_id = '1' * 32
    descriptor = dict(schema_version=1, elf=RUNTIME.elf_name, address=0x1000, build_id=build_id,
                      board=row.board, device='device', speed=4000, cases=list(row.cases), seeds=[])
    (output / RUNTIME.elf_name).write_bytes(b'synthetic ELF; binutils stubbed')
    (output / 'inputs.txt').write_text(''.join(str(path) + '\n' for path in inputs))
    (output / 'probe.txt').write_text('device\n4000\n')
    (output / 'compile_commands.json').write_text('[]')
    (output / 'abi.json').write_text('{}')
    manifest = dict(schema_version=3, harness=setup.roots['harness'], sdk=setup.roots['candidate'],
                    board=row.board, suite=row.suite, toolchain='gcc', fault_injection='none',
                    expected_cases=list(row.cases), build_id=build_id, compiler='gcc proof',
                    sdk_identity_sha256={name: sha256(sdk / name) for name in evidence.SDK_IDENTITIES},
                    input_roles={'harness': [], 'sdk': [str(path) for path in inputs], 'generated': [], 'environment': []},
                    inputs_sha256={str(path): sha256(path) for path in inputs},
                    elf_sha256=sha256(output / RUNTIME.elf_name), device='device', swd_speed_khz=4000, seeds=[])
    text = ''.join(f'test.c:1:{case}:PASS\n' for case in row.cases) + f'{len(row.cases)} Tests 0 Failures 0 Ignored\nOK\n'
    raw = HEADER.pack(MAGIC, VERSION, 2, len(row.cases), 0, 0, len(text), 0, build_id.encode()) + text.encode().ljust(TEXT_SIZE, b'\0')
    (output / 'last-result.bin').write_bytes(raw)
    (output / 'execution.json').write_text(json.dumps(dict(status='passed', board=row.board, device='device',
        probe='123', swd_speed_khz=4000, debugger_connected=True, cold_boot=False,
        reset='jlink-reset-before-flash-reset-halt-then-run')))
    (output / 'seeded.json').write_text('{}')
    (output / 'jlink.json').write_text('{"device":"device","version":"9.88"}')
    (output / 'result.json').write_text(json.dumps(asdict(evidence.decode(raw, build_id, row.cases))))
    (output / 'manifest.json').write_text(json.dumps(manifest))
    (output / 'firmware.json').write_text(json.dumps(descriptor))
    entry = dict(elf=RUNTIME.elf_name, build_id=build_id, origin=str(output), hardware=True,
                 binding=dict(board=row.board, probe='123', device='device'),
                 files={path.name: sha256(path) for path in output.iterdir()})
    monkeypatch.setattr(evidence.abi, 'verify', lambda *args: None)
    command = lambda *args: f'00001000 {RESULT_SIZE:08x} B nsx_test_result\n'
    return SimpleNamespace(output=output, row=row, entry=entry, command=command, manifest=manifest)


def verify_example(setup, sample):
    return evidence.verify_firmware(sample.output, sample.entry, sample.row, 'candidate', setup.roots,
                                    setup.roots, setup.tools.to_dict(), setup.tools, sample.command)


def test_firmware_verifier_accepts_consistent_raw_evidence(setup, firmware_evidence):
    assert verify_example(setup, firmware_evidence)['build_id'] == '1' * 32


@pytest.mark.parametrize('fault', ['stale-id', 'truncated', 'wrong-cases', 'input-changed', 'elf-changed',
                                   'descriptor-address', 'probe-descriptor', 'raw-json-disagree', 'dirty-build'])
def test_firmware_verifier_rejects_wrong_or_incomplete_evidence(setup, firmware_evidence, fault):
    sample = firmware_evidence
    if fault in ('stale-id', 'truncated'):
        path = sample.output / 'last-result.bin'
        raw = path.read_bytes()
        path.write_bytes(raw.replace(b'1' * 32, b'2' * 32) if fault == 'stale-id' else raw[:-1])
    elif fault == 'input-changed':
        Path(next(iter(sample.manifest['inputs_sha256']))).write_text('changed input')
    elif fault == 'elf-changed':
        (sample.output / sample.entry['elf']).write_bytes(b'changed ELF')
    elif fault == 'descriptor-address':
        path = sample.output / 'firmware.json'
        value = json.loads(path.read_text())
        value['address'] += 4
        path.write_text(json.dumps(value))
    elif fault == 'probe-descriptor':
        (sample.output / 'probe.txt').write_text('wrong-device\n4000\n')
    elif fault == 'raw-json-disagree':
        (sample.output / 'result.json').write_text('{}')
    else:
        if fault == 'wrong-cases':
            sample.manifest['expected_cases'] = ['other']
        else:
            sample.manifest['sdk']['dirty'] = True
        (sample.output / 'manifest.json').write_text(json.dumps(sample.manifest))
    # Re-seal file hashes: these tests check semantic evidence, not only checksums.
    sample.entry['files'] = {path.name: sha256(path) for path in sample.output.iterdir()}
    with pytest.raises((ValueError, RuntimeError)):
        verify_example(setup, sample)


def test_tools_capture_and_build_environment_select_same_cmake(tmp_path):
    import shutil
    gcc = tmp_path / 'combined-toolchain'
    (gcc / 'bin').mkdir(parents=True)
    for name in ('gcc', 'g++', 'ld', 'nm', 'readelf'):
        path = gcc / 'bin' / ('arm-none-eabi-' + name)
        path.write_text('#!/bin/sh\necho fake-tool-version\n')
        path.chmod(0o755)
    cmake = gcc / 'bin/cmake'
    cmake.write_text(f'#!/bin/sh\nif [ "$1" = "-P" ]; then echo "{tmp_path}"; else echo selected-cmake-version; fi\n')
    cmake.chmod(0o755)
    tools = Tools.capture(gcc, tmp_path / 'tools')
    env = tools.environment(tmp_path / 'scratch')
    assert tools.path('cmake') == cmake
    assert shutil.which('cmake', path=env['PATH']) == str(cmake)
    assert next(item.version for item in tools.binaries if item.name == 'cmake') == 'selected-cmake-version'


def test_selected_cross_toolchain_does_not_break_host_c_headers(tmp_path, monkeypatch):
    import shutil
    import subprocess
    gcc = tmp_path / 'cross-toolchain'
    # This host compile does not need an ARM installation, only the explicit
    # lookup path. The fake value proves ambient GCC_ROOT cannot leak through.
    monkeypatch.setenv('GCC_ROOT', '/invalid-cross-headers')
    tools = Tools(str(gcc), '/cmake', ())
    env = tools.environment(tmp_path / 'scratch')
    compiler = shutil.which('cc', path=env['PATH'])
    assert compiler, 'Host C compiler is a qualification prerequisite'
    source = '#include <stdio.h>\n#include <stddef.h>\nint main(void){puts("host-ok");return 0;}\n'
    output = tmp_path / 'host'
    result = subprocess.run([compiler, '-x', 'c', '-', '-o', str(output)], input=source, text=True,
                            capture_output=True, env=env, timeout=15)
    assert result.returncode == 0, result.stderr
    assert subprocess.check_output([str(output)], env=env, text=True, timeout=5).strip() == 'host-ok'


@pytest.mark.parametrize('component', ('.', ''))
def test_tools_freeze_relative_and_empty_path_before_changing_directory(tmp_path, monkeypatch, component):
    import os
    import shutil
    original = tmp_path / 'launch'
    original.mkdir()
    other = tmp_path / 'execution'
    other.mkdir()
    gcc = tmp_path / 'cross'
    (gcc / 'bin').mkdir(parents=True)
    for name in ('gcc', 'g++', 'ld', 'nm', 'readelf'):
        path = gcc / 'bin' / ('arm-none-eabi-' + name)
        path.write_text('#!/bin/sh\necho selected-tool\n')
        path.chmod(0o755)
    cmake = original / 'cmake'
    cmake.write_text(f'#!/bin/sh\nif [ "$1" = "-P" ]; then echo "{tmp_path}"; else echo launch-cmake; fi\n')
    cmake.chmod(0o755)
    monkeypatch.chdir(original)
    monkeypatch.setenv('PATH', component + os.pathsep + os.environ['PATH'])
    tools = Tools.capture(gcc, tmp_path / 'tools')
    assert tools.path('cmake') == cmake
    monkeypatch.chdir(other)
    monkeypatch.setenv('PATH', '/changed-after-capture')
    env = tools.environment(tmp_path / 'scratch')
    assert all(Path(part).is_absolute() for part in env['PATH'].split(os.pathsep))
    assert shutil.which('cmake', path=env['PATH']) == str(cmake)
    import subprocess
    assert subprocess.check_output(['cmake', '--version'], env=env, text=True, timeout=5).strip() == 'launch-cmake'


def test_frozen_path_preserves_symlink_parent_traversal(tmp_path, monkeypatch):
    import os
    import shutil
    from tools.sdk_qualification.runtime import execution_path
    launch = tmp_path / 'launch'
    installed = tmp_path / 'installed'
    (launch / 'bin').mkdir(parents=True)
    (installed / 'bin').mkdir(parents=True)
    (installed / 'release').mkdir()
    (launch / 'alias').symlink_to(installed / 'release', target_is_directory=True)
    for parent, label in ((launch, 'wrong-cmake'), (installed, 'selected-cmake')):
        tool = parent / 'bin/cmake'
        tool.write_text('#!/bin/sh\necho ' + label + '\n')
        tool.chmod(0o755)
    monkeypatch.chdir(launch)
    monkeypatch.setenv('PATH', 'alias/../bin' + os.pathsep + os.environ['PATH'])
    expected = Path(shutil.which('cmake')).resolve()
    assert expected == installed / 'bin/cmake'
    frozen = execution_path(tmp_path / 'cross')
    monkeypatch.chdir(tmp_path)
    assert Path(shutil.which('cmake', path=frozen)).resolve() == expected


@pytest.mark.parametrize('index', (0, 1, 2), ids=('startup', 'system', 'linker'))
@pytest.mark.parametrize('role', ('harness', 'generated', 'environment'))
def test_production_startup_system_linker_cannot_be_replaced_outside_sdk(setup, firmware_evidence, index, role):
    sample = firmware_evidence
    manifest = sample.manifest
    inputs = (sample.output / 'inputs.txt').read_text().splitlines()
    old = inputs[index]
    bases = {'harness': Path(setup.roots['harness']['root']), 'generated': sample.output,
             'environment': setup.root / 'external'}
    replacement = bases[role] / ('replacement-' + Path(old).name)
    replacement.parent.mkdir(parents=True, exist_ok=True)
    replacement.write_text('validly inventoried replacement, but not SDK production input')
    inputs[index] = str(replacement)
    manifest['input_roles']['sdk'].remove(old)
    manifest['input_roles'][role].append(str(replacement))
    del manifest['inputs_sha256'][old]
    manifest['inputs_sha256'][str(replacement)] = sha256(replacement)
    (sample.output / 'inputs.txt').write_text('\n'.join(inputs) + '\n')
    (sample.output / 'manifest.json').write_text(json.dumps(manifest))
    sample.entry['files'] = {path.name: sha256(path) for path in sample.output.iterdir() if path.is_file()}
    with pytest.raises(ValueError, match='outside the SDK'):
        verify_example(setup, sample)


@pytest.fixture
def mutable_tools(tmp_path):
    executable = tmp_path / 'recorded-python'
    executable.write_text('recorded executable bytes')
    executable.chmod(0o755)
    return Tools(str(tmp_path / 'gcc'), '/cmake',
                 (Binary('python', str(executable), sha256(executable), 'recorded-version'),))


@pytest.mark.parametrize('action', ('replace', 'remove', 'unexecutable'))
def test_tool_recheck_rejects_replacement_or_disappearance(mutable_tools, action):
    mutable_tools.recheck()
    executable = mutable_tools.path('python')
    if action == 'replace':
        executable.write_text('replacement executable bytes')
    elif action == 'remove':
        executable.unlink()
    else:
        executable.chmod(0o644)
    with pytest.raises(ValueError, match='identity changed or disappeared'):
        mutable_tools.recheck()


@pytest.mark.parametrize('symlink', (False, True))
def test_tool_recheck_rejects_new_path_shadow(tmp_path, symlink):
    early, late = tmp_path / 'early', tmp_path / 'late'
    early.mkdir(); late.mkdir()
    selected = late / 'cmake'
    selected.write_text('#!/bin/sh\necho selected\n'); selected.chmod(0o755)
    tools = Tools('/gcc', '/cmake', (Binary('cmake', str(selected), sha256(selected), 'version'),),
                  str(early) + ':' + str(late))
    tools.recheck()
    shadow = early / 'cmake'
    if symlink:
        shadow.symlink_to(selected)
    else:
        shadow.write_bytes(selected.read_bytes()); shadow.chmod(0o755)
    with pytest.raises(ValueError, match='selection changed'):
        tools.recheck()


def test_worker_tool_change_and_finalization_cannot_pass(setup, mutable_tools, monkeypatch):
    from tools.sdk_qualification.operations import Session
    monkeypatch.setattr(Config, 'prepare', lambda self, plan: (self.output.mkdir(), setup.roots)[1])
    monkeypatch.setattr(Config, 'roots', lambda *args: setup.roots)
    config = Config(Path('/baseline'), Path('/candidate'), Path('/gcc'), setup.root / 'changed-tools')
    current = Session(setup.plan, config, 'host', ())
    current.tools = mutable_tools
    def replace_during_command(*args, **kwargs):
        mutable_tools.path('python').write_text('changed during worker execution')
    monkeypatch.setattr('tools.sdk_qualification.operations.run_worker', replace_during_command)
    with pytest.raises(ValueError, match='identity changed'):
        current.call(['owned-worker'], 'replaced-tool')
    assert current.calls[0]['outcome'] == 'failed'
    final = current.finish(None)['body']
    assert final['scope_status'] == 'failed' and final['qualification'] is False
    assert 'Tool identity changed' in final['error']


def test_assessment_rechecks_tools_before_finalizing(setup, mutable_tools, monkeypatch):
    monkeypatch.setattr(Config, 'prepare', lambda self, plan: (self.output.mkdir(), setup.roots)[1])
    monkeypatch.setattr(Config, 'roots', lambda *args: setup.roots)
    monkeypatch.setattr(Tools, 'capture', lambda *args: mutable_tools)
    monkeypatch.setattr(Collection, 'add', lambda *args: mutable_tools.path('python').unlink())
    config = Config(Path('/baseline'), Path('/candidate'), Path('/gcc'), setup.root / 'assessment-tools-changed')
    report = assess(setup.plan, config, (setup.root,))
    assert report['status'] == 'failed' and report['qualification'] is False
    assert 'Tool identity changed or disappeared' in report['errors'][0]


@pytest.fixture
def board_orchestration(setup, mutable_tools, monkeypatch):
    """Exercise run_board with real session/host-copy/finalization and fake build/transport."""
    from contextlib import contextmanager
    from tools.hardware.build import Firmware
    from tools.sdk_qualification import operations
    monkeypatch.setattr(Config, 'prepare', lambda self, plan: (self.output.mkdir(), setup.roots)[1])
    monkeypatch.setattr(Config, 'roots', lambda *args: setup.roots)
    monkeypatch.setattr(Tools, 'capture', lambda *args: mutable_tools)
    monkeypatch.setattr(operations, 'run_worker', lambda command, log, **kwargs: log.write('fake build completed\n'))
    events = []
    monkeypatch.setattr(operations, 'verify_host', lambda *args: events.append(('host-verified',)))
    monkeypatch.setattr(operations, 'verify_firmware', lambda *args: events.append(('firmware-verified',)))
    board = BOARDS[0]
    host = setup.root / 'prior-host'
    body = record(setup, host, 'host')
    body['tools'] = mutable_tools.to_dict()
    rewrite(host, body)
    state = SimpleNamespace(outcome='success', fail_side='baseline', opens=0, closes=0, runs=0)
    def fake_build(current, row, side):
        current.call(['fake-build'], row.id + '-' + side)
        output = current.config.output / 'firmware' / row.id / side
        output.mkdir(parents=True)
        elf = output / 'test.elf'
        elf.write_bytes(b'fake ELF for orchestration only')
        device = 'different-device' if state.outcome == 'device-mismatch' else 'device'
        image = Firmware(elf, 0x1000, uuid.uuid4().hex, row.board, device, 4000, row.cases)
        entry = dict(row=row.id, side=side, directory=str(output.relative_to(current.config.output)),
                     origin=str(output), elf=elf.name, build_id=image.build_id, hardware=False, binding=None,
                     files={elf.name: sha256(elf)})
        current.entries.append(entry)
        current.track(elf)
        events.append(('build', row.suite, side))
        return image, entry
    monkeypatch.setattr(operations, '_build', fake_build)
    class FakeTarget:
        def run(self, image, output):
            state.runs += 1
            side = output.name
            events.append(('run', side))
            for name in ('execution.json', 'seeded.json', 'jlink.json', 'result.json'):
                (output / name).write_text('{}')
            (output / 'last-result.bin').write_bytes(b'fake transport; protocol verifier tested separately')
            (output / 'transport.log').write_text('owned fake board session')
            if side == state.fail_side and state.outcome in ('failure', 'interrupt'):
                (output / 'failure.json').write_text('{"status":"fake-failure"}')
                if state.outcome == 'interrupt':
                    raise KeyboardInterrupt('fake board interruption')
                raise RuntimeError('fake board execution failure')
    @contextmanager
    def fake_open(request):
        assert request.probe == '123' and request.device == 'device'
        assert events[0] == ('host-verified',)
        assert events[-2][0] == events[-1][0] == 'build'
        assert [events[-2][-1], events[-1][-1]] == ['baseline', 'candidate']
        state.opens += 1
        events.append(('open',))
        try:
            yield FakeTarget()
        finally:
            state.closes += 1
            events.append(('close',))
    monkeypatch.setattr(operations, 'open_board', fake_open)
    config = Config(Path('/baseline'), Path('/candidate'), Path('/gcc'), setup.root / 'board-output')
    return SimpleNamespace(operations=operations, config=config, board=board, host=host, state=state, events=events,
                           binding=Binding(board, '123', 'device'))


def test_run_board_seals_both_sides_and_closes_sessions(setup, board_orchestration):
    sample = board_orchestration
    envelope = sample.operations.run_board(setup.plan, sample.config, sample.board, sample.binding, sample.host)
    body = envelope['body']
    expected = [(row.id, side) for row in setup.plan.rows if row.board == sample.board for side in row.sides]
    assert [(entry['row'], entry['side']) for entry in body['firmware']] == expected
    assert all(entry['hardware'] is True and entry['binding'] == asdict(sample.binding) for entry in body['firmware'])
    assert sample.state.opens == sample.state.closes == 4 and sample.state.runs == 8
    assert body['scope_status'] == 'passed' and body['qualification'] is False and body['global_status'] == 'incomplete'
    assert body['cleanup']['board_state'] == 'tests-completed'
    for entry in body['firmware']:
        evidence.verify_files(contained(sample.config.output, entry['directory']), entry['files'])
        assert {'execution.json', 'last-result.bin', 'result.json', 'transport.log'}.issubset(entry['files'])
    evidence.verify_files(sample.config.output, body['files'])
    assert read_record(sample.config.output / 'host')['sha256'] == read_record(sample.host)['sha256']


@pytest.mark.parametrize('outcome', ('failure', 'interrupt'))
@pytest.mark.parametrize('side', ('baseline', 'candidate'))
def test_run_board_failure_or_interrupt_finalizes_unknown_board(setup, board_orchestration, outcome, side):
    sample = board_orchestration
    sample.state.outcome, sample.state.fail_side = outcome, side
    error = KeyboardInterrupt if outcome == 'interrupt' else RuntimeError
    with pytest.raises(error, match='fake board'):
        sample.operations.run_board(setup.plan, sample.config, sample.board, sample.binding, sample.host)
    body = read_record(sample.config.output)['body']
    assert sample.state.opens == sample.state.closes == 1
    assert body['scope_status'] == 'failed' and body['qualification'] is False and body['global_status'] == 'failed'
    assert body['cleanup']['board_state'] == 'unknown'
    assert body['firmware'][-1]['hardware'] is False
    assert any(name.endswith('failure.json') for name in body['files'])
    evidence.verify_files(sample.config.output, body['files'])


@pytest.mark.parametrize('bad_host', ('failed', 'corrupt'))
def test_run_board_refuses_bad_host_before_build_or_probe(setup, board_orchestration, bad_host):
    sample = board_orchestration
    if bad_host == 'failed':
        body = read_record(sample.host)['body']; body['scope_status'] = 'failed'; rewrite(sample.host, body)
    else:
        (sample.host / 'command.log').write_text('changed after host record finalized')
    with pytest.raises(ValueError):
        sample.operations.run_board(setup.plan, sample.config, sample.board, sample.binding, sample.host)
    body = read_record(sample.config.output)['body']
    assert not sample.events and sample.state.opens == sample.state.runs == sample.state.closes == 0
    assert body['scope_status'] == 'failed' and body['qualification'] is False
    assert body['cleanup']['board_state'] == 'not-accessed' and not body['firmware']


def test_run_board_device_mismatch_stops_before_probe(setup, board_orchestration):
    sample = board_orchestration
    sample.state.outcome = 'device-mismatch'
    with pytest.raises(ValueError, match='Inventory device differs'):
        sample.operations.run_board(setup.plan, sample.config, sample.board, sample.binding, sample.host)
    body = read_record(sample.config.output)['body']
    assert sample.state.opens == sample.state.runs == sample.state.closes == 0
    assert body['scope_status'] == 'failed' and body['qualification'] is False
    assert body['cleanup']['board_state'] == 'not-accessed'
