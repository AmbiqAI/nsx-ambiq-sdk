"""Read-only farm evidence checks shared by execution and PR reporting."""
from dataclasses import asdict, dataclass
from pathlib import Path

from .farm_artifacts import Artifact, read_receipt
from .farm_inputs import Dispatch, load_dispatch, passed_state
from .io import read_record
from .plan import Plan
from .report import initial_report


def scoped_record(directory: Path, plan: Plan, mode: str, boards: tuple[str, ...] = ()) -> dict:
    from .evidence import verify_files
    envelope = read_record(directory)
    body = envelope['body']
    if (body['plan_sha256'] != plan.digest or body['revisions'] != asdict(plan.revisions)
            or body['scope'] != {'mode': mode, 'boards': list(boards)}
            or body['scope_status'] != 'passed' or body['error'] is not None
            or body['inputs_rechecked'] is not True
            or body['cleanup'] != {'process_groups': 'completed',
                                   'board_state': 'tests-completed' if mode == 'hardware' else 'not-accessed'}):
        raise ValueError('Qualification scope/cleanup does not match the farm job')
    verify_files(directory, body['files'])
    return envelope


def verify_selected(report: dict, plan: Plan, boards: tuple[str, ...]):
    expected = initial_report(plan)
    if (any(report[name] != expected[name] for name in (
            'schema_version', 'profile', 'plan_sha256', 'revisions', 'exclusions')) or report['errors']):
        raise ValueError('Selected scope requires valid evidence for the unchanged reviewed plan')
    wanted = {(entry['requirement'], entry['side']): entry for entry in expected['requirements']}
    actual = {(entry['requirement'], entry['side']): entry for entry in report['requirements']}
    if len(actual) != len(report['requirements']) or actual.keys() != wanted.keys():
        raise ValueError('Assessment requirements differ from the reviewed plan')
    gates = {gate.id for gate in plan.gates}
    for key, pending in wanted.items():
        item = actual[key]
        if any(item[name] != pending[name] for name in ('kind', 'revision', 'cases')):
            raise ValueError('Assessment changed a declared requirement')
        parts = item['requirement'].split('/')
        selected = (parts[0] == 'compiler' and parts[2] in boards) or parts[0] in boards or key[0] in gates
        if item['status'] != ('passed' if selected else 'incomplete'):
            raise ValueError(f'Selected/missing requirement has an invalid result: {key}')
    complete = all(item['status'] == 'passed' for item in actual.values())
    if report['status'] != ('passed' if complete else 'incomplete') or report['qualification'] is not complete:
        raise ValueError('Assessment qualification does not match its complete requirement set')


@dataclass(frozen=True)
class CanonicalHost:
    receipt: Artifact
    directory: Path
    sha256: str


def canonical_host(download: Path, dispatch: Dispatch, plan: Plan) -> CanonicalHost:
    receipt = read_receipt(download / 'receipt.json', dispatch.run, 'host')
    origin = download / 'payload'
    original, _ = load_dispatch(origin / 'inputs', dispatch.run)
    if original != dispatch:
        raise ValueError('Inputs differ from canonical host dispatch')
    state = passed_state(origin / 'evidence', dispatch, 'host')
    source = origin / 'evidence/host'
    envelope = scoped_record(source, plan, 'host')
    if state['host_record_sha256'] != envelope['sha256']:
        raise ValueError('Canonical host outcome/record identity mismatch')
    return CanonicalHost(receipt, source, envelope['sha256'])
