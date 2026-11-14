"""Local operations over the existing intake, pytest and board-session APIs."""
from contextlib import contextmanager
from dataclasses import asdict, replace
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import shutil
import sys
import time
import uuid

from tools.hardware.api import BoardRequest, open_board
from tools.hardware.build import Firmware
from tools.hardware.preparation import MemorySeed
from tools.hardware.process import run_worker
from . import abi
from .evidence import verify_firmware, verify_host, verify_files, verify_header
from .io import contained, read_json, read_record, sha256, write_record
from .models import Revisions
from .plan import Plan, write_plan
from .policy import BOARDS
from .runtime import Binding, Config, HARNESS_ROOT, Tools


def create_plan(baseline: Path, candidate: Path, path: Path) -> Plan:
    from tools.hardware.provenance import checkout
    values = {name: checkout(root.resolve()) for name, root in (
        ('baseline', baseline), ('candidate', candidate), ('harness', HARNESS_ROOT))}
    if any(value['dirty'] for value in values.values()):
        raise ValueError('Plan requires clean committed input checkouts')
    plan = Plan(Revisions(**{name: value['revision'] for name, value in values.items()}))
    write_plan(plan, path)
    return plan


class Session:
    def __init__(self, plan: Plan, config: Config, mode: str, boards: tuple[str, ...]):
        self.plan, self.config, self.mode = plan, config, mode
        if len(set(boards)) != len(boards) or any(board not in BOARDS for board in boards):
            raise ValueError('Scope requires unique declared boards')
        if mode not in ('host', 'build', 'hardware') or ((mode == 'host') != (not boards)):
            raise ValueError('Host scopes have no boards; build/hardware scopes require boards')
        if mode == 'hardware' and len(boards) != 1:
            raise ValueError('Hardware scope requires exactly one board')
        self.boards = tuple(sorted(boards))
        self.roots = config.prepare(plan)
        self.tools = None
        self.entries = []
        self.calls = []
        self.tracked = set()
        self.host = None
        self.host_reference = None
        self.touched = False
        self.started = datetime.now(timezone.utc).isoformat()
        self.clock = time.monotonic()
        self.execution_id = uuid.uuid4().hex
        self.progress('preflight')

    def progress(self, phase):
        (self.config.output / 'progress.json').write_text(json.dumps({
            'execution_id': self.execution_id, 'plan_sha256': self.plan.digest,
            'status': 'incomplete', 'phase': phase}, indent=2) + '\n')

    def start(self):
        try:
            self.tools = Tools.capture(self.config.gcc_root, self.config.output / 'tools')
        finally:
            if (self.config.output / 'tools').is_dir():
                self.tracked.update(path for path in (self.config.output / 'tools').iterdir() if path.is_file())

    def track(self, *paths):
        self.tracked.update(path for path in paths if path.is_file())

    def call(self, command, label, *, cwd=None, env=None, timeout=None):
        self.config.roots(self.plan)
        folder = self.config.output / 'commands'
        folder.mkdir(exist_ok=True)
        path = folder / f'{len(self.calls):04}-{re.sub("[^A-Za-z0-9_-]", "_", label)}.log'
        step = {'label': label, 'command': command, 'cwd': str(cwd or HARNESS_ROOT),
                'log': str(path.relative_to(self.config.output)),
                'outcome': 'incomplete', 'returncode': None}
        self.calls.append(step)
        self.progress(label)
        try:
            with path.open('w') as log:
                run_worker(command, log=log, cwd=cwd or HARNESS_ROOT,
                           timeout=timeout or self.config.host_timeout,
                           env=env if env is not None else self.tools.environment(self.config.output / 'scratch'))
            self.config.roots(self.plan)
            step.update(outcome='passed', returncode=0)
            return path.read_text()
        except BaseException as error:
            step.update(outcome='failed', error=f'{type(error).__name__}: {error}')
            raise
        finally:
            self.track(path)

    def finish(self, error):
        checked = False
        try:
            checked = self.config.roots(self.plan) == self.roots
        except Exception as changed:
            error = error or changed
        if not checked:
            error = error or RuntimeError('Inputs changed during execution')
        body = {'execution_id': self.execution_id, 'plan_sha256': self.plan.digest,
                'origin_output': str(self.config.output),
                'revisions': asdict(self.plan.revisions), 'roots': self.roots,
                'tools': None if self.tools is None else self.tools.to_dict(),
                'scope': {'mode': self.mode, 'boards': self.boards},
                'scope_status': 'passed' if error is None else 'failed',
                'qualification': False, 'global_status': 'incomplete' if error is None else 'failed',
                'inputs_rechecked': checked, 'firmware': self.entries,
                'host': self.host, 'host_reference': self.host_reference,
                'calls': self.calls, 'started_at': self.started,
                'completed_at': datetime.now(timezone.utc).isoformat(),
                'elapsed_seconds': time.monotonic() - self.clock,
                'cleanup': {'process_groups': 'completed' if error is None else 'failed-or-cancelled',
                            'board_state': ('unknown' if error is not None else 'tests-completed')
                            if self.touched else 'not-accessed'},
                'error': None if error is None else f'{type(error).__name__}: {error}',
                'files': {str(path.relative_to(self.config.output)): sha256(path)
                          for path in sorted(self.tracked)}}
        return write_record(self.config.output, body)


@contextmanager
def session(plan, config, mode, boards=()):
    current = Session(plan, config, mode, boards)
    error = None
    try:
        current.start()
        yield current
    except BaseException as caught:
        error = caught
        raise
    finally:
        envelope = current.finish(error)
        if error is None and envelope['body']['scope_status'] != 'passed':
            raise RuntimeError(envelope['body']['error'])


def host(plan: Plan, config: Config, decisions: Path) -> dict:
    with session(plan, config, 'host') as current:
        destination = config.output / 'patch-decisions.json'
        destination.write_bytes(decisions.read_bytes())
        current.track(destination)
        python = str(current.tools.path('python'))
        review = config.output / 'intake-reviewed.json'
        current.call([python, '-m', 'tools.sdk_intake_review', '--baseline', str(config.baseline),
                      '--candidate', str(config.candidate), '--decisions', str(destination),
                      '--output', str(review)], 'intake-review')
        current.track(review)
        compatibility = config.output / 'compatibility'
        current.call([python, '-m', 'tools.sdk_compatibility', '--profile', 'gcc',
                      '--output', str(compatibility)], 'candidate-host-gcc', cwd=config.candidate)
        current.track(compatibility / 'compatibility.json', compatibility / 'junit.xml')
        env = current.tools.environment(config.output / 'scratch')
        # The same full pytest run is performed once, below, with retained JUnit.
        env['SKIP'] = 'pytest-contracts'
        current.call([python, '-m', 'pre_commit', 'run', '--all-files'], 'candidate-hygiene',
                     cwd=config.candidate, env=env)
        junit = config.output / 'host-tests.xml'
        current.call([python, '-m', 'pytest', '-q', '-o', 'addopts=', f'--junitxml={junit}',
                      f'--basetemp={config.output / "host-scratch"}'], 'candidate-host-tests', cwd=config.candidate)
        current.track(junit)
        current.host = {'review': review.name, 'compatibility': 'compatibility/compatibility.json', 'junit': junit.name}
        verify_host(config.output, current.host, current.roots, current.tools.to_dict(), current.tools)
        # Preserve reports/logs, not successful temporary compiler/test trees.
        for path in (compatibility / 'pytest-work', config.output / 'host-scratch', config.output / 'scratch'):
            if path.is_dir():
                shutil.rmtree(path)
    return read_record(config.output)


def _build(current: Session, row, side):
    output = current.config.output / 'firmware' / row.id / side
    python = str(current.tools.path('python'))
    try:
        current.call([python, '-m', 'tools.sdk_qualification.build_worker', '--sdk-root',
                      current.roots[side]['root'], '--gcc-root', str(current.config.gcc_root),
                      '--output', str(output), '--board', row.board, '--suite', row.suite],
                     row.id + '-' + side + '-build')
        descriptor = read_json(output / 'firmware.json')
        if descriptor['schema_version'] != 1:
            raise ValueError('Unsupported firmware descriptor')
        command = lambda args, label: current.call(args, row.id + '-' + side + '-' + label, cwd=output, timeout=60)
        abi.capture(output, HARNESS_ROOT, current.tools, command)
        files = [output / name for name in ('firmware.json', 'manifest.json', 'abi.json',
                 'compile_commands.json', 'inputs.txt', 'probe.txt', descriptor['elf'])]
        entry = {'row': row.id, 'side': side, 'directory': str(output.relative_to(current.config.output)),
                 'origin': str(output), 'elf': descriptor['elf'], 'build_id': descriptor['build_id'],
                 'hardware': False, 'binding': None,
                 'files': {str(path.relative_to(output)): sha256(path) for path in files}}
        verify_firmware(output, entry, row, side, current.roots, current.roots,
                        current.tools.to_dict(), current.tools, command)
        image = Firmware(output / descriptor['elf'], descriptor['address'], descriptor['build_id'],
                         descriptor['board'], descriptor['device'], descriptor['speed'], tuple(descriptor['cases']),
                         tuple(MemorySeed(**seed) for seed in descriptor['seeds']))
        current.entries.append(entry)
        current.track(*files)
        return image, entry
    finally:
        current.track(output / 'build.log')


def build(plan: Plan, config: Config, boards: tuple[str, ...] = BOARDS) -> dict:
    with session(plan, config, 'build', boards) as current:
        for row in plan.rows:
            if row.board in boards:
                for side in row.sides:
                    _build(current, row, side)
    return read_record(config.output)


def copy_host(plan: Plan, current: Session, source: Path) -> None:
    # Shared host acceptance is copied before any board access. Final records
    # remain self-contained when moved between local hosts/farm artifact stores.
    envelope = read_record(source)
    body = envelope['body']
    verify_header(plan, body, current.roots, current.tools)
    if body['scope'] != {'mode': 'host', 'boards': []}:
        raise ValueError('Host prerequisite is not a completed run for this plan')
    verify_files(source, body['files'])
    verify_host(source, body['host'], current.roots, body['tools'], current.tools)
    destination = current.config.output / 'host'
    destination.mkdir()
    for name in ('record.json', *body['files']):
        src, dst = contained(source, name), contained(destination, name)
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(src, dst)
        current.track(dst)
    current.host_reference = {'directory': 'host', 'sha256': envelope['sha256']}


def run_board(plan: Plan, config: Config, board: str, binding: Binding, host_record: Path) -> dict:
    if binding.board != board:
        raise ValueError('Inventory does not match requested board')
    with session(plan, config, 'hardware', (board,)) as current:
        copy_host(plan, current, host_record)
        for row in plan.rows:
            if row.board != board:
                continue
            images = [(side, *_build(current, row, side)) for side in row.sides]
            if row.execution == 'build':
                continue
            if any(image.device != binding.device or image.board != board for _, image, _ in images):
                raise ValueError('Inventory device differs from a built SDK descriptor')
            with open_board(BoardRequest(binding.probe, binding.device, config.hardware_timeout)) as target:
                current.touched = True
                for side, image, entry in images:
                    output = contained(config.output, entry['directory'])
                    try:
                        target.run(image, output)
                    finally:
                        current.track(*(output / name for name in (
                            'execution.json', 'failure.json', 'seeded.json', 'jlink.json',
                            'last-result.bin', 'result.json', 'transport.log')))
                    entry['hardware'] = True
                    entry['binding'] = asdict(binding)
                    hardware_files = [output / name for name in (
                        'execution.json', 'seeded.json', 'jlink.json', 'last-result.bin', 'result.json', 'transport.log')]
                    entry['files'].update({path.name: sha256(path) for path in hardware_files})
                    current.track(*hardware_files)
                    command = lambda args, label: current.call(args, row.id + '-' + side + '-' + label,
                                                               cwd=output, timeout=60)
                    verify_firmware(output, entry, row, side, current.roots, current.roots,
                                    current.tools.to_dict(), current.tools, command)
    return read_record(config.output)


def run(plan: Plan, config: Config, bindings: tuple[Binding, ...], decisions: Path) -> dict:
    # The full local operation is just composition of the same reusable scopes.
    if {entry.board for entry in bindings} != set(BOARDS) or len(bindings) != len(BOARDS):
        raise ValueError('Full run requires the exact declared five-board inventory')
    if len({int(entry.probe) for entry in bindings}) != len(bindings):
        raise ValueError('A probe cannot stand in for multiple boards')
    config.roots(plan)
    if config.output.exists() or any(config.output.is_relative_to(root) for root in (
            config.baseline, config.candidate, HARNESS_ROOT)):
        raise ValueError('Full run requires fresh evidence outside input roots')
    config.output.mkdir(parents=True)
    host_output = config.output / 'host'
    host(plan, replace(config, output=host_output), decisions)
    records = [host_output]
    for binding in sorted(bindings, key=lambda entry: entry.board):
        output = config.output / binding.board
        run_board(plan, replace(config, output=output), binding.board, binding, host_output)
        records.append(output)
    from .assessment import assess
    return assess(plan, replace(config, output=config.output / 'assessment'), tuple(records))
