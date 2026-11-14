"""Assess explicit finalized executions without rerunning firmware or choosing newest results."""
from datetime import datetime, timezone
import json
from pathlib import Path
import re
import uuid

from tools.hardware.process import run_worker
from .evidence import verify_files, verify_firmware, verify_header, verify_host
from .io import contained, read_record
from .plan import Plan
from .report import initial_report
from .runtime import Binding, Config, HARNESS_ROOT, Tools


class Collection:
    """Collect verified obligations; repeated copies are idempotent, competing runs are errors."""

    def __init__(self, plan: Plan, roots: dict, tools: Tools, command):
        self.plan, self.roots, self.tools, self.command = plan, roots, tools, command
        self.executions = {}
        self.references = []
        self.passed = {}
        self.probes = {}
        self.active = set()

    def accept(self, requirement: str, side: str, execution: str):
        key = (requirement, side)
        previous = self.passed.get(key)
        if previous is not None and previous != execution:
            raise ValueError(f'Competing executions for {requirement}/{side}; select explicit evidence')
        self.passed[key] = execution

    def add(self, directory: Path):
        envelope = read_record(directory)
        body, digest = envelope['body'], envelope['sha256']
        verify_header(self.plan, body, self.roots, self.tools)
        # Recheck even an identical copy: its referenced artifacts may be damaged.
        verify_files(directory, body['files'])
        execution = body['execution_id']
        if execution in self.active:
            raise ValueError('Cyclic execution references')
        if execution in self.executions and self.executions[execution] != digest:
            raise ValueError('Conflicting records share an execution ID')
        self.active.add(execution)
        try:
            self._verify(directory, body)
        finally:
            self.active.remove(execution)
        if execution not in self.executions:
            self.executions[execution] = digest
            self.references.append({'execution_id': execution, 'sha256': digest, 'directory': str(directory)})

    def _verify(self, directory, body):
        scope, execution = body['scope'], body['execution_id']
        if scope['mode'] == 'host':
            if any(name not in body['files'] for name in body['host'].values()):
                raise ValueError('Host reports are absent from the sealed file inventory')
            verify_host(directory, body['host'], self.roots, body['tools'], self.tools)
            for gate in self.plan.gates:
                self.accept(gate.id, 'candidate' if gate.kind == 'host' else 'plan', execution)
            return
        if scope['mode'] == 'hardware':
            reference = body['host_reference']
            if set(reference) != {'directory', 'sha256'}:
                raise ValueError('Invalid prior host reference')
            source = contained(directory, reference['directory'])
            host = read_record(source)
            if host['sha256'] != reference['sha256'] or host['body']['scope']['mode'] != 'host':
                raise ValueError('Prior host reference differs from its copied record')
            self.add(source)
        rows = {row.id: row for row in self.plan.rows if row.board in scope['boards']}
        expected = {(row.id, side) for row in rows.values() for side in row.sides}
        entries = body['firmware']
        actual = [(entry['row'], entry['side']) for entry in entries]
        if len(actual) != len(set(actual)) or set(actual) != expected:
            raise ValueError('Firmware scope is incomplete, duplicated or contains unplanned rows')
        for entry in entries:
            row = rows[entry['row']]
            is_hardware = scope['mode'] == 'hardware' and row.execution == 'hardware'
            if entry['hardware'] is not is_hardware:
                raise ValueError('Firmware execution kind differs from its scope')
            output = contained(directory, entry['directory'])
            if not Path(entry['origin']).is_relative_to(body['origin_output']):
                raise ValueError('Build provenance is outside its original execution directory')
            if is_hardware:
                binding = Binding(**entry['binding'])
                key = int(binding.probe)
                previous = self.probes.get(key)
                if previous is not None and previous != row.board:
                    raise ValueError('One physical probe cannot qualify different boards')
                self.probes[key] = row.board
            elif entry['binding'] is not None:
                raise ValueError('Build-only entry cannot claim a probe binding')
            verify_firmware(output, entry, row, entry['side'], body['roots'], self.roots,
                            body['tools'], self.tools, self.command)
            if is_hardware or row.execution == 'build':
                self.accept(row.id, entry['side'], body['execution_id'])
        # Compiler claims require the complete candidate build/link/ABI scope for this board.
        for compiler in self.plan.compilers:
            for board in scope['boards']:
                for check in compiler.checks:
                    if board in compiler.boards:
                        self.accept(f'compiler/{compiler.toolchain}/{board}/{check}', 'candidate', execution)


def assess(plan: Plan, config: Config, records: tuple[Path, ...]) -> dict:
    roots = config.prepare(plan)
    report = dict(initial_report(plan), kind='sdk-qualification-assessment',
                  assessment_id=uuid.uuid4().hex, assessed_at=datetime.now(timezone.utc).isoformat(),
                  executed=False, executions=[], errors=[])
    collection = None
    try:
        tools = Tools.capture(config.gcc_root, config.output / 'tools')
        report['tools'] = tools.to_dict()
        calls = config.output / 'commands'
        calls.mkdir()
        count = 0
        def command(args, label):
            nonlocal count
            config.roots(plan)
            output = calls / f'{count:04}-{re.sub("[^A-Za-z0-9_-]", "_", label)}.log'
            count += 1
            with output.open('w') as log:
                run_worker(args, log=log, cwd=HARNESS_ROOT, timeout=60,
                           env=tools.environment(config.output / 'scratch'))
            config.roots(plan)
            return output.read_text()
        collection = Collection(plan, roots, tools, command)
        for directory in records:
            collection.add(directory.resolve())
        config.roots(plan)
        for item in report['requirements']:
            execution = collection.passed.get((item['requirement'], item['side']))
            if execution is not None:
                item.update(status='passed', execution_id=execution)
        if all(item['status'] == 'passed' for item in report['requirements']):
            report.update(status='passed', qualification=True)
    except Exception as error:
        report.update(status='failed', qualification=False)
        report['errors'].append(f'{type(error).__name__}: {error}')
    finally:
        if collection is not None:
            report['executions'] = collection.references
        with (config.output / 'assessment.json').open('x') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
    return report
