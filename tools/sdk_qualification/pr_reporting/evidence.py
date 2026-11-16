"""Data-only publication checks over a completed, trusted farm execution."""
from dataclasses import asdict
from pathlib import Path

from tools.hardware.protocol import decode
from ..evidence import verify_files
from ..farm_artifacts import fetch, read_receipt
from ..farm_evidence import canonical_host, scoped_record, verify_selected
from ..farm_inputs import load_dispatch, passed_state
from ..io import contained, read_json, read_record
from .models import BOARDS, Binding


def validate(api, binding: Binding, output: Path) -> dict:
    """Recheck data identities, files and target buffers; execute no artifact code."""
    run = binding.run
    receipts = {}
    for stage in ('host', 'assessment'):
        receipts[stage] = fetch(run, stage, output / stage, api).to_dict()
    host_download = output / 'host'
    host_payload = host_download / 'payload'
    dispatch, plan = load_dispatch(host_payload / 'inputs', run)
    binding.verify_inputs(dispatch, plan)
    host = canonical_host(host_download, dispatch, plan)
    executions = {read_record(host.directory)['body']['execution_id']: host.sha256}
    payload = output / 'assessment/payload'
    other, other_plan = load_dispatch(payload / 'inputs', run)
    binding.verify_inputs(other, other_plan)
    state = passed_state(payload / 'evidence', dispatch, 'assessment')
    if state['host_artifact'] != host.receipt.to_dict() or state['host_record_sha256'] != host.sha256:
        raise ValueError('Assessment references another canonical host')
    images = cases = 0
    for board in BOARDS:
        download = payload / 'evidence/downloaded' / board
        receipt = read_receipt(download / 'receipt.json', run, board)
        if receipt != api.artifact(board):
            raise ValueError('Nested board receipt differs from the published artifact')
        receipts[board] = receipt.to_dict()
        board_payload = download / 'payload'
        prior, prior_plan = load_dispatch(board_payload / 'inputs', run)
        binding.verify_inputs(prior, prior_plan)
        released = passed_state(board_payload / 'evidence', dispatch, 'board', board)
        if released['host_artifact'] != host.receipt.to_dict() or released['host_record_sha256'] != host.sha256:
            raise ValueError('Board used another canonical host')
        directory = board_payload / 'evidence/board'
        record = scoped_record(directory, plan, 'hardware', (board,))
        body = record['body']
        if (body['host_reference']['sha256'] != host.sha256
                or read_record(directory / 'host')['sha256'] != host.sha256):
            raise ValueError('Board record references another host execution')
        executions[body['execution_id']] = record['sha256']
        rows = {row.id: row for row in plan.rows if row.board == board}
        expected = {(row.id, side) for row in rows.values() for side in row.sides}
        actual = [(entry['row'], entry['side']) for entry in body['firmware']]
        if len(actual) != len(expected) or set(actual) != expected:
            raise ValueError('Selected firmware scope differs from the plan')
        for entry in body['firmware']:
            row = rows[entry['row']]
            directory = contained(board_payload / 'evidence/board', entry['directory'])
            verify_files(directory, entry['files'])
            manifest = read_json(directory / 'manifest.json')
            if (manifest['sdk']['revision'] != getattr(plan.revisions, entry['side'])
                    or manifest['harness']['revision'] != plan.revisions.harness
                    or manifest['sdk']['dirty'] is not False or manifest['harness']['dirty'] is not False
                    or manifest['board'] != board or manifest['build_id'] != entry['build_id']
                    or tuple(manifest['expected_cases']) != row.cases):
                raise ValueError('Firmware manifest differs from the declared PR scope')
            if entry['hardware'] is not (row.execution == 'hardware'):
                raise ValueError('Hardware/build evidence kind changed')
            images += 1
            if entry['hardware']:
                result = decode((directory / 'last-result.bin').read_bytes(), entry['build_id'], row.cases)
                if result is None or asdict(result) != read_json(directory / 'result.json'):
                    raise ValueError('Target result is incomplete or differs from retained JSON')
                if (entry['binding']['board'] != board
                        or entry['binding']['probe'] != released['binding']['probe']):
                    raise ValueError('Target result has another board/probe binding')
                cases += result.total
    report = read_json(payload / 'evidence/assessment/assessment.json')
    verify_selected(report, plan, BOARDS)
    if (len(executions) != len(BOARDS) + 1 or len(report['executions']) != len(executions)
            or {e['execution_id']: e['sha256'] for e in report['executions']} != executions):
        raise ValueError('Assessment does not contain exactly the canonical host and selected boards')
    return {'images': images, 'target_case_executions': cases,
            'requirements': {status: sum(item['status'] == status for item in report['requirements'])
                             for status in ('passed', 'incomplete')},
            'full_qualification': report['qualification'], 'receipts': receipts}
