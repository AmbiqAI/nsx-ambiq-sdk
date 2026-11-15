"""Farm inventory and allocator boundary; no acceptance policy lives here."""
import json
import os
from pathlib import Path
import subprocess

from .io import read_json, sha256


def runner_binding(inventory: Path, board: str) -> dict:
    values = {name: os.environ[name] for name in ('BENCH_BOARD', 'HPX_BOARD', 'HPX_JLINK_SERIAL')}
    serial = values['HPX_JLINK_SERIAL']
    if values['HPX_BOARD'] != board or not serial.isdecimal() or int(serial) <= 0:
        raise ValueError('Runner must match the explicitly selected board and positive probe serial')
    entries = [b for b in read_json(inventory)['boards'] if b['id'] == values['BENCH_BOARD']]
    if (len(entries) != 1 or entries[0]['name'] != board
            or str(entries[0]['serial']).lstrip('0') != serial.lstrip('0')
            or entries[0]['runner'] is not True):
        raise ValueError('Runner identity differs from authoritative bench inventory')
    return {'id': values['BENCH_BOARD'], 'board': board, 'probe': serial}


def debugger_inputs(library: Path, overlay: Path) -> dict:
    if not library.is_file() or not overlay.is_dir():
        raise ValueError('Farm requires its explicit J-Link library and deployed JLinkDevices tree')
    paths = sorted(overlay.rglob('*'))
    if any(path.is_symlink() and not path.is_file() for path in paths):
        raise ValueError('J-Link overlay directory or broken symlinks are unsupported; use a file tree')
    files = {str(path.relative_to(overlay)): {'path': str(path.resolve()), 'sha256': sha256(path)}
             for path in paths if path.is_file()}
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


def capability(inventory: Path):
    flags = bench(['unlock', '--help'], inventory)
    if any(flag not in flags for flag in ('--expected-pid', '--expected-holder', '--expected-reason')):
        raise RuntimeError('Deployed bench-agent lacks conditional unlock; update the farm before running')


def holder(binding: dict, inventory: Path) -> dict:
    status = json.loads(bench(['status', '--json'], inventory))
    matches = [b for b in status['boards'] if b['id'] == binding['id']]
    if len(matches) != 1:
        raise RuntimeError('Cannot identify allocated board in bench status')
    entry = matches[0]
    if entry['name'] != binding['board'] or str(entry['serial']).lstrip('0') != binding['probe'].lstrip('0'):
        raise RuntimeError('Bench allocation identity changed')
    return {key: entry[key] for key in ('pid', 'holder', 'reason', 'user')}
