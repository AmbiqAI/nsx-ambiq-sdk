"""Supervised AP510 farm adapter; qualification policy stays in the local CLI."""
import argparse
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys

from tools.hardware.process import RunCancelled, run_worker
from .io import read_json, read_record, sha256

BOARD = 'apollo510_evb'


def write(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + '\n')


def prepare(output: Path) -> None:
    output.mkdir(parents=True, exist_ok=False)
    for name, variable in (('plan', 'SDK_PLAN_JSON'), ('decisions', 'SDK_DECISIONS_JSON')):
        path = output / f'{name}.json'
        path.write_text(os.environ[variable])
        read_json(path)
    revisions = read_json(output / 'plan.json')['revisions']
    if set(revisions) != {'baseline', 'candidate', 'harness'} or any(
            not isinstance(sha, str) or re.fullmatch('[0-9a-f]{40}', sha) is None
            for sha in revisions.values()):
        raise ValueError('Plan requires full immutable baseline/candidate/harness SHAs')
    with Path(os.environ['GITHUB_OUTPUT']).open('a') as stream:
        for role, sha in revisions.items():
            stream.write(f'{role}={sha}\n')


def runner_binding(inventory: Path) -> dict:
    values = {name: os.environ[name] for name in ('BENCH_BOARD', 'HPX_BOARD', 'HPX_JLINK_SERIAL')}
    serial = values['HPX_JLINK_SERIAL']
    if values['HPX_BOARD'] != BOARD or not serial.isdecimal() or int(serial) <= 0:
        raise ValueError('Pilot requires the AP510 runner and an explicit positive probe serial')
    entries = [b for b in read_json(inventory)['boards'] if b['id'] == values['BENCH_BOARD']]
    if (len(entries) != 1 or entries[0]['name'] != BOARD or
            str(entries[0]['serial']).lstrip('0') != serial.lstrip('0') or entries[0]['runner'] is not True):
        raise ValueError('Runner identity differs from authoritative bench inventory')
    return {'id': values['BENCH_BOARD'], 'board': BOARD, 'probe': serial}


def debugger_inputs(library: Path, overlay: Path) -> dict:
    if not library.is_file() or not overlay.is_dir():
        raise ValueError('Farm requires its explicit J-Link library and deployed JLinkDevices tree')
    files = {str(path.relative_to(overlay)): {'path': str(path.resolve()), 'sha256': sha256(path)}
             for path in sorted(overlay.rglob('*')) if path.is_file()}
    if not files:
        raise ValueError('Deployed J-Link device tree is empty')
    return {'library': {'path': str(library.resolve()), 'sha256': sha256(library)},
            'overlay': {'path': str(overlay), 'resolved': str(overlay.resolve()), 'files': files}}


def bench(command: list[str], inventory: Path) -> str:
    env = dict(os.environ)
    env.pop('RUNNER_TRACKING_ID', None)
    result = subprocess.run(['bench-agent', *command, '--boards', str(inventory),
                             '--locks', '/run/lock/bench'], env=env, capture_output=True,
                            text=True, timeout=150, check=True)
    return result.stdout


def holder(binding: dict, inventory: Path) -> dict:
    status = json.loads(bench(['status', '--json'], inventory))
    matches = [b for b in status['boards'] if b['id'] == binding['id']]
    if len(matches) != 1:
        raise RuntimeError('Cannot identify allocated board in bench status')
    entry = matches[0]
    if entry['name'] != binding['board'] or str(entry['serial']).lstrip('0') != binding['probe'].lstrip('0'):
        raise RuntimeError('Bench allocation identity changed')
    return {key: entry[key] for key in ('pid', 'holder', 'reason', 'user')}


def command(args: list[str], cwd: Path, log: Path, timeout: float) -> None:
    env = dict(os.environ)
    env.pop('PYTHONPATH', None)
    with log.open('w') as stream:
        run_worker(args, log=stream, cwd=cwd, timeout=timeout, env=env)


def run(inputs: Path, baseline: Path, candidate: Path, harness: Path, python: Path,
        gcc: Path, output: Path, inventory: Path = Path('/etc/bench/boards.json')) -> dict:
    output.mkdir(parents=True, exist_ok=False)
    state = {'schema_version': 1, 'pilot_status': 'failed', 'qualification': False,
             'global_status': 'incomplete', 'board_state': 'not-accessed',
             'lease': 'not-acquired', 'board': BOARD, 'error': None}
    allocated = False
    previous = {sig: signal.getsignal(sig) for sig in (signal.SIGTERM, signal.SIGINT)}
    def cancel(signum, frame):
        raise RunCancelled(f'Farm pilot cancelled by signal {signum}')
    try:
        for sig in previous:
            signal.signal(sig, cancel)
        binding = runner_binding(inventory)
        capabilities = bench(['unlock', '--help'], inventory)
        if any(flag not in capabilities for flag in ('--expected-pid', '--expected-holder', '--expected-reason')):
            raise RuntimeError('Deployed bench-agent lacks conditional unlock; update the farm before running')
        library = Path(os.environ['HPX_JLINK_DLL']).resolve(strict=True)
        overlay = Path.home() / '.config/SEGGER/JLinkDevices'
        debug = debugger_inputs(library, overlay)
        state.update(binding=binding, debugger=debug,
                     driver_revision=subprocess.check_output(
                         ['git', 'rev-parse', 'HEAD'], text=True, cwd=Path(__file__).resolve().parents[2]).strip())
        write(output / 'pilot.json', state)
        common = ['--plan', str(inputs / 'plan.json'), '--baseline', str(baseline),
                  '--candidate', str(candidate), '--gcc-root', str(gcc)]
        cli = [str(python), '-m', 'tools.sdk_qualification']
        command([*cli, 'host', *common, '--output', str(output / 'host'),
                 '--decisions', str(inputs / 'decisions.json')], harness, output / 'host.log', 2700)
        command([str(python), '-m', 'tools.sdk_qualification.build_worker', '--sdk-root', str(candidate),
                 '--gcc-root', str(gcc), '--board', BOARD, '--suite', 'startup',
                 '--output', str(output / 'descriptor')], harness, output / 'descriptor.log', 300)
        device = read_json(output / 'descriptor/firmware.json')['device']
        write(output / 'inventory.json', {'schema_version': 1,
              'boards': [{'board': BOARD, 'probe': binding['probe'], 'device': device}]})
        if debugger_inputs(library, overlay) != debug:
            raise RuntimeError('Debugger inputs changed before reservation')
        reason = ('UNKNOWN/recovery-required if interrupted: ' + os.environ['GITHUB_SERVER_URL'] + '/' +
                  os.environ['GITHUB_REPOSITORY'] + '/actions/runs/' + os.environ['GITHUB_RUN_ID'] +
                  '/attempts/' + os.environ['GITHUB_RUN_ATTEMPT'])
        owner = 'sdk-' + os.environ['GITHUB_RUN_ID'] + '-' + os.environ['GITHUB_RUN_ATTEMPT']
        state.update(lease='acquiring', board_state='unknown', reason=reason)
        write(output / 'pilot.json', state)
        receipt = bench(['lock', binding['id'], '--holder', owner, '--reason', reason,
                         '--timeout', '120', '--ttl', '4200'], inventory)
        allocated = True
        state['lease'] = 'retained-until-ttl-or-operator-recovery'
        match = re.search(r'\(pid ([0-9]+)\)', receipt)
        acquired = holder(binding, inventory)
        if match is None or acquired['pid'] != int(match[1]) or acquired['holder'] != owner or acquired['reason'] != reason:
            raise RuntimeError('Bench holder does not match acquisition receipt')
        state['holder'] = acquired
        write(output / 'pilot.json', state)
        command([*cli, 'run-board', *common, '--output', str(output / 'board'), '--board', BOARD,
                 '--inventory', str(output / 'inventory.json'), '--host-record', str(output / 'host'),
                 '--jlink-library', str(library)], harness, output / 'board.log', 900)
        board = read_record(output / 'board')['body']
        if board['cleanup'] != {'process_groups': 'completed', 'board_state': 'tests-completed'}:
            raise RuntimeError('Board cleanup is not established')
        # The unchanged five-board plan intentionally makes this CLI return 1.
        try:
            command([*cli, 'assess', *common, '--output', str(output / 'assessment'),
                     '--record', str(output / 'host'), '--record', str(output / 'board')],
                    harness, output / 'assessment.log', 300)
        except subprocess.CalledProcessError as error:
            if error.returncode != 1:
                raise
        assessment = read_json(output / 'assessment/assessment.json')
        if assessment['status'] != 'incomplete' or assessment['errors'] or assessment['qualification'] is not False:
            raise RuntimeError('Pilot requires valid scoped evidence and incomplete full qualification')
        if debugger_inputs(library, overlay) != debug:
            raise RuntimeError('Debugger inputs changed during execution')
        if holder(binding, inventory) != acquired:
            raise RuntimeError('Lease ownership changed; refusing unlock')
        bench(['unlock', binding['id'], '--expected-pid', str(acquired['pid']),
               '--expected-holder', acquired['holder'], '--expected-reason', acquired['reason']], inventory)
        state.update(pilot_status='passed', board_state='tests-completed', lease='released')
        return state
    except BaseException as error:
        state['error'] = f'{type(error).__name__}: {error}'
        if allocated:
            state['lease'] = 'retained-until-ttl-or-operator-recovery'
        raise
    finally:
        write(output / 'pilot.json', state)
        for sig, handler in previous.items():
            signal.signal(sig, handler)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--output', type=Path, required=True)
    pilot = sub.add_parser('run')
    for name in ('inputs', 'baseline', 'candidate', 'harness', 'python', 'gcc-root', 'output'):
        pilot.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'prepare':
            prepare(args.output)
        else:
            run(args.inputs.resolve(), args.baseline.resolve(), args.candidate.resolve(), args.harness.resolve(),
                args.python.absolute(), args.gcc_root.resolve(), args.output.resolve())
        return 0
    except (ValueError, OSError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'Farm pilot stopped: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
