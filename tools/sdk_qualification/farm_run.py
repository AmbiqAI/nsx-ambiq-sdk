"""Compose existing qualification scopes and a single-board farm reservation."""
from contextlib import contextmanager
from dataclasses import asdict, dataclass
import os
from pathlib import Path
import re
import signal
import time

from tools.hardware.process import run_worker
from .farm_artifacts import Artifact, GitHub, fetch, read_receipt
from .farm_bench import bench, capability, debugger_inputs, holder, runner_binding
from .farm_inputs import Dispatch, Run, State, load_dispatch, passed_state, write
from .io import read_json, read_record
from .plan import Plan
from .report import initial_report


class FarmCancelled(RuntimeError):
    """Do not inherit InterruptedError: subprocess pipe selectors retry EINTR."""


@dataclass(frozen=True)
class Paths:
    inputs: Path
    baseline: Path
    candidate: Path
    harness: Path
    python: Path
    gcc: Path
    output: Path

    def common(self):
        return ['--plan', str(self.inputs / 'plan.json'), '--baseline', str(self.baseline),
                '--candidate', str(self.candidate), '--gcc-root', str(self.gcc)]

    def cli(self):
        return [str(self.python), '-m', 'tools.sdk_qualification']


@contextmanager
def journal(output: Path, state: State):
    output.mkdir(parents=True, exist_ok=False)
    started = time.monotonic()
    previous = {sig: signal.getsignal(sig) for sig in (signal.SIGTERM, signal.SIGINT)}
    def cancel(signum, frame):
        raise FarmCancelled(f'Farm stage cancelled by signal {signum}')
    try:
        for sig in previous:
            signal.signal(sig, cancel)
        state.save(output)
        yield state
        state.status = 'passed'
    except BaseException as error:
        state.status = 'failed'
        state.global_status = 'failed'
        state.error = f'{type(error).__name__}: {error}'
        raise
    finally:
        state.elapsed_seconds = time.monotonic() - started
        try:
            state.save(output)
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)


def command(paths: Paths, state: State, args: list[str], label: str, timeout: float):
    print(f'{state.stage}/{state.board or "host"}: START {label}', flush=True)
    started = time.monotonic()
    env = dict(os.environ)
    env.pop('PYTHONPATH', None)
    try:
        with (paths.output / f'{label}.log').open('w') as stream:
            run_worker(args, log=stream, cwd=paths.harness, timeout=timeout, env=env)
    finally:
        state.timings[label] = time.monotonic() - started
        state.save(paths.output)
        print(f'{state.stage}/{state.board or "host"}: END {label} ({state.timings[label]:.1f}s)', flush=True)


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
    if (report['plan_sha256'] != plan.digest or report['revisions'] != expected['revisions']
            or report['errors'] or report['status'] != 'incomplete' or report['qualification'] is not False):
        raise ValueError('Selected pilot requires valid evidence and incomplete five-board qualification')
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


def assess_scope(paths: Paths, state: State, plan: Plan, boards: tuple[str, ...], records: list[Path]):
    import subprocess
    args = [*paths.cli(), 'assess', *paths.common(), '--output', str(paths.output / 'assessment')]
    for record in records:
        args.extend(('--record', str(record)))
    try:
        command(paths, state, args, 'assessment', 300)
    except subprocess.CalledProcessError as error:
        if error.returncode != 1:
            raise
    report = read_json(paths.output / 'assessment/assessment.json')
    verify_selected(report, plan, boards)


def host(paths: Paths):
    dispatch, plan = load_dispatch(paths.inputs, Run.current())
    state = State(dispatch.run, 'host', dispatch.digest)
    with journal(paths.output, state):
        command(paths, state, [*paths.cli(), 'host', *paths.common(), '--output', str(paths.output / 'host'),
                              '--decisions', str(paths.inputs / 'decisions.json')], 'host', 2700)
        state.host_record_sha256 = scoped_record(paths.output / 'host', plan, 'host')['sha256']
    return state


def check_host(plan: Plan, baseline: Path, candidate: Path, gcc: Path, output: Path, source: Path):
    from .operations import checked_host
    from .runtime import Config, Tools
    config = Config(baseline, candidate, gcc, output)
    roots = config.prepare(plan)
    tools = Tools.capture(gcc, output / 'tools')
    envelope = checked_host(plan, roots, tools, source)
    config.roots(plan)
    tools.recheck()
    write(output / 'checked.json', {'host_record_sha256': envelope['sha256']})


def pack_for_device(dispatch: Dispatch, device: str):
    from tools.hardware import device_pack
    manifest = read_json(device_pack.MANIFEST)
    if device not in manifest['devices']:
        return None
    archive = Path(dispatch.device_pack_archive)
    root = device_pack.install(archive, device_pack.cache_root())
    return {'manifest': manifest, 'directory': str(root)}


def board(paths: Paths, board: str, downloaded_host: Path, inventory: Path = Path('/etc/bench/boards.json')):
    dispatch, plan = load_dispatch(paths.inputs, Run.current())
    if board not in dispatch.boards:
        raise ValueError('Board is not in the explicitly dispatched scope')
    state = State(dispatch.run, 'board', dispatch.digest, board=board)
    with journal(paths.output, state):
        receipt = read_receipt(downloaded_host / 'receipt.json', dispatch.run, 'host')
        origin = downloaded_host / 'payload'
        original, _ = load_dispatch(origin / 'inputs', dispatch.run)
        if original != dispatch:
            raise ValueError('Board inputs differ from canonical host dispatch')
        host_state = passed_state(origin / 'evidence', dispatch, 'host')
        source = origin / 'evidence/host'
        host_record = scoped_record(source, plan, 'host')
        if host_state['host_record_sha256'] != host_record['sha256']:
            raise ValueError('Canonical host outcome/record identity mismatch')
        state.host_artifact = receipt.to_dict()
        state.host_record_sha256 = host_record['sha256']
        binding = runner_binding(inventory, board)
        state.binding = binding
        capability(inventory)
        library = Path(os.environ['HPX_JLINK_DLL']).resolve(strict=True)
        overlay = Path.home() / '.config/SEGGER/JLinkDevices'
        debug = debugger_inputs(library, overlay)
        state.debugger = debug
        command(paths, state, [str(paths.python), '-m', 'tools.sdk_qualification.farm', 'check-host',
                              *paths.common(), '--output', str(paths.output / 'preflight'),
                              '--host-record', str(source)], 'host-preflight', 300)
        if read_json(paths.output / 'preflight/checked.json')['host_record_sha256'] != host_record['sha256']:
            raise ValueError('Runner preflight used a different host record')
        command(paths, state, [str(paths.python), '-m', 'tools.sdk_qualification.build_worker',
                              '--sdk-root', str(paths.candidate), '--gcc-root', str(paths.gcc),
                              '--board', board, '--suite', 'startup', '--output', str(paths.output / 'descriptor')],
                'descriptor', 300)
        device = read_json(paths.output / 'descriptor/firmware.json')['device']
        pack = pack_for_device(dispatch, device)
        if pack:
            state.debugger = dict(debug, sdk_device_pack=pack)
        write(paths.output / 'inventory.json', {'schema_version': 1,
              'boards': [{'board': board, 'probe': binding['probe'], 'device': device}]})
        if debugger_inputs(library, overlay) != debug:
            raise RuntimeError('Debugger inputs changed before reservation')
        state.holder = f'sdk-{dispatch.run.run_id}-{dispatch.run.attempt}-{board}'
        state.reason = f'UNKNOWN/recovery-required if interrupted: {dispatch.run.url} board={board}'
        state.lease, state.board_state = 'acquiring', 'unknown'
        state.save(paths.output)
        response = bench(['lock', binding['id'], '--holder', state.holder, '--reason', state.reason,
                          '--timeout', '120', '--ttl', '4200'], inventory)
        state.lease = 'retained-until-ttl-or-operator-recovery'
        state.save(paths.output)
        match = re.search(r'\(pid ([0-9]+)\)', response)
        acquired = holder(binding, inventory)
        if match is None or acquired['pid'] != int(match[1]) or acquired['holder'] != state.holder or acquired['reason'] != state.reason:
            raise RuntimeError('Bench holder does not match acquisition receipt')
        state.acquired = acquired
        state.save(paths.output)
        command(paths, state, [*paths.cli(), 'run-board', *paths.common(), '--output', str(paths.output / 'board'),
                              '--board', board, '--inventory', str(paths.output / 'inventory.json'),
                              '--host-record', str(source), '--jlink-library', str(library)], 'board', 900)
        scoped_record(paths.output / 'board', plan, 'hardware', (board,))
        assess_scope(paths, state, plan, (board,), [source, paths.output / 'board'])
        if debugger_inputs(library, overlay) != debug:
            raise RuntimeError('Debugger inputs changed during execution')
        if pack:
            from tools.hardware.device_pack import verify
            verify(Path(pack['directory']), pack['manifest'])
        if holder(binding, inventory) != acquired:
            raise RuntimeError('Lease ownership changed; refusing unlock')
        bench(['unlock', binding['id'], '--expected-pid', str(acquired['pid']),
               '--expected-holder', acquired['holder'], '--expected-reason', acquired['reason']], inventory)
        state.lease, state.board_state = 'released', 'tests-completed'
    return state


def collect_board(paths: Paths, dispatch: Dispatch, plan: Plan, host_receipt: Artifact,
                  host_sha256: str, board_name: str, client: GitHub) -> Path:
    download = paths.output / 'downloaded' / board_name
    fetch(dispatch.run, board_name, download, client)
    payload = download / 'payload'
    prior, _ = load_dispatch(payload / 'inputs', dispatch.run)
    if prior != dispatch:
        raise ValueError('Board artifact uses different reviewed inputs')
    result = passed_state(payload / 'evidence', dispatch, 'board', board_name)
    if result['host_artifact'] != host_receipt.to_dict() or result['host_record_sha256'] != host_sha256:
        raise ValueError('Board used a different canonical host artifact')
    board_record = scoped_record(payload / 'evidence/board', plan, 'hardware', (board_name,))
    if board_record['body']['host_reference']['sha256'] != host_sha256:
        raise ValueError('Board record references another host execution')
    return payload / 'evidence/board'


def aggregate(paths: Paths, downloaded_host: Path, client: GitHub | None = None):
    dispatch, plan = load_dispatch(paths.inputs, Run.current())
    client = client or GitHub(dispatch.run)
    state = State(dispatch.run, 'assessment', dispatch.digest)
    with journal(paths.output, state):
        receipt = read_receipt(downloaded_host / 'receipt.json', dispatch.run, 'host')
        origin = downloaded_host / 'payload'
        original, _ = load_dispatch(origin / 'inputs', dispatch.run)
        if original != dispatch:
            raise ValueError('Assessment inputs differ from canonical host dispatch')
        host_state = passed_state(origin / 'evidence', dispatch, 'host')
        source = origin / 'evidence/host'
        if scoped_record(source, plan, 'host')['sha256'] != host_state['host_record_sha256']:
            raise ValueError('Canonical host outcome/record identity mismatch')
        state.host_artifact = receipt.to_dict()
        state.host_record_sha256 = host_state['host_record_sha256']
        records = [source]
        problems = []
        for board_name in dispatch.boards:
            try:
                records.append(collect_board(paths, dispatch, plan, receipt, state.host_record_sha256, board_name, client))
            except (ValueError, OSError, KeyError, TypeError) as error:
                problems.append(f'{board_name}: {error}')
        try:
            client.require_successful_jobs(dispatch.boards)
        except (ValueError, OSError, KeyError, TypeError) as error:
            problems.append(str(error))
        if problems:
            raise ValueError('Selected farm evidence incomplete: ' + '; '.join(problems))
        assess_scope(paths, state, plan, dispatch.boards, records)
    return state
