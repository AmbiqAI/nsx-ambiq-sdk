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
    for key in ('PYTEST_ADDOPTS', 'PYTHONPATH', 'SKIP'):
        monkeypatch.setenv(key, 'undesired')
    env = setup.tools.environment(setup.root / 'scratch')
    assert not any(key in env for key in ('PYTEST_ADDOPTS', 'PYTHONPATH', 'SKIP'))
    assert env['GCC_ROOT'] == '/gcc'
    assert env['NSX_REQUIRE_ARM_GCC'] == '1'
    assert env['PYTEST_DISABLE_PLUGIN_AUTOLOAD'] == '1'


def test_assessment_partial_is_incomplete_and_invalid_is_failed(setup, collection, monkeypatch):
    monkeypatch.setattr(Config, 'prepare', lambda self, plan: (self.output.mkdir(), setup.roots)[1])
    monkeypatch.setattr(Config, 'roots', lambda *args: setup.roots)
    monkeypatch.setattr(Tools, 'capture', lambda *args: setup.tools)
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
