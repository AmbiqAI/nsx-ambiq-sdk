"""Physical transport controls, separate from positive qualification records.

The observer deliberately does not use run_worker: enclosing worker cleanup
could conceal the cleanup defect being tested. Emergency intervention fails
the control even if it succeeds in releasing the probe.
"""
import json
from pathlib import Path
import signal
import subprocess
import sys
import time

from .api import BoardRequest
from .protocol import decode
from .runner import probe_lock
from .suites import STARTUP


def _startup_command(repo, board, gcc_root, request, output, sdk_root, fault, timeout):
    return [sys.executable, '-m', 'pytest', '-q', STARTUP.nodeid, '--hardware',
            '--board', board, '--probe', request.probe, '--gcc-root', str(gcc_root),
            '--hardware-artifacts', str(output), '--fault-injection', fault,
            '--hardware-timeout', str(timeout),
            *(['--device', request.device] if request.device else []),
            *(['--candidate-sdk-root', str(sdk_root)] if sdk_root else [])]


def _read(path):
    try:
        return json.loads(path.read_text())
    except (FileNotFoundError, json.JSONDecodeError):
        return None  # The child may still be publishing the file.


def _prepared(output, board, device):
    manifests = list(output.glob(f'{board}/*/manifest.json'))
    if len(manifests) > 1:
        raise RuntimeError('Control requires exactly one child firmware build')
    if not manifests:
        return None
    directory = manifests[0].parent
    manifest, seeded, jlink = (_read(directory / name) for name in
                               ('manifest.json', 'seeded.json', 'jlink.json'))
    if manifest is None or seeded is None or jlink is None:
        return None
    expected = {s.symbol: s.value for s in STARTUP.seeds}
    if (manifest.get('board') != board or manifest.get('suite') != 'startup' or
            manifest.get('fault_injection') != 'timeout' or
            manifest.get('expected_cases') != list(STARTUP.cases) or
            seeded != expected or jlink.get('device') != manifest.get('device') or
            (device is not None and device != manifest.get('device'))):
        raise RuntimeError('Control preparation identity/readback mismatch')
    return directory, manifest


def _released(probe):
    try:
        with probe_lock(probe):
            return True
    except RuntimeError:
        return False


def _native_outcome(directory, action, returncode, worker_alive, lease_released):
    execution = _read(directory / 'execution.json') or {}
    expected = 'qualification_failed' if action == 'timeout' else 'cancelled'
    if returncode == 0 or worker_alive or not lease_released or execution.get('status') != expected:
        raise RuntimeError('Native transport cleanup/status did not satisfy the control')
    if action == 'timeout':
        failure = _read(directory / 'failure.json') or {}
        if failure.get('status') != expected or failure.get('error') != 'Timed out waiting for complete firmware evidence':
            raise RuntimeError('Expected the target-result timeout, not another failure')


def run_control(repo: Path, board: str, gcc_root: Path, request: BoardRequest,
                output: Path, action: str, sdk_root: Path | None = None) -> dict:
    """Caller holds shared board allocation and has completed the host gates.

    Opens no debugger itself. Executes the normal startup pytest twice: first
    with withheld completion, then a fresh ordinary recovery. The local lease
    namespace/environment is shared with the children, never a private TMPDIR.
    """
    if sys.platform != 'linux' or action not in ('term', 'interrupt', 'timeout'):
        raise ValueError('Linux controls support term, interrupt and timeout only')
    import psutil  # Optional hardware dependency; never needed for normal tests.
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    receipt = dict(schema_version=1, qualification=False, action=action, board=board,
                   probe=request.probe, status='failed', board_state='unknown',
                   emergency_cleanup=False)
    owned = {}
    process = None
    root = None

    def alive(member):
        try:
            return member.is_running() and member.status() != psutil.STATUS_ZOMBIE
        except psutil.NoSuchProcess:
            return False

    def remember():
        if root is None or not alive(root):
            return
        try:
            for member in [root, *root.children(recursive=True)]:
                owned.setdefault((member.pid, member.create_time()), member)
        except psutil.NoSuchProcess:
            pass

    try:
        negative = output / 'negative'
        timeout = 3 if action == 'timeout' else 60
        command = _startup_command(repo, board, gcc_root, request, negative, sdk_root, 'timeout', timeout)
        with (output / 'negative.log').open('w') as log:
            process = subprocess.Popen(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            root = psutil.Process(process.pid)
            receipt['child_pid'] = process.pid
            deadline = time.monotonic() + 420
            prepared = None
            worker = None
            while time.monotonic() < deadline:
                remember()
                if process.poll() is not None:
                    raise RuntimeError('Startup child exited before physical preparation')
                prepared = _prepared(negative, board, request.device)
                if prepared is not None:
                    for member in list(owned.values()):
                        try:
                            args = member.cmdline()
                            if (alive(member) and 'tools.hardware.jlink_worker' in args and
                                    '--output' in args and
                                    Path(args[args.index('--output')+1]).resolve() == prepared[0]):
                                worker = member
                                break
                        except psutil.NoSuchProcess:
                            pass
                    if worker is not None:
                        break
                time.sleep(0.02)
            if prepared is None or worker is None:
                raise RuntimeError('No live prepared J-Link worker before the control deadline')
            receipt.update(directory=str(prepared[0]), build_id=prepared[1]['build_id'],
                           device=prepared[1]['device'], worker_pid=worker.pid,
                           worker_created=worker.create_time(), readiness='verified-preparation-before-restart')
            if _released(request.probe):
                raise RuntimeError('Active prepared worker did not hold the local probe lease')
            if action != 'timeout':
                process.send_signal(signal.SIGTERM if action == 'term' else signal.SIGINT)
            code = process.wait(timeout=timeout+15)
            remember()
            snapshot = dict(returncode=code, worker_alive=alive(worker), lease_released=_released(request.probe))
            receipt['native_outcome'] = snapshot
            _native_outcome(prepared[0], action, **snapshot)

        # This recovery is accepted only after native cleanup passed, and cannot
        # convert failed cleanup into success. It uses the same normal transport.
        recovery = output / 'recovery'
        command = _startup_command(repo, board, gcc_root, request, recovery, sdk_root, 'none', request.timeout)
        with (output / 'recovery.log').open('w') as log:
            process = subprocess.Popen(command, cwd=repo, stdout=log, stderr=subprocess.STDOUT,
                                       start_new_session=True)
            root = psutil.Process(process.pid)
            deadline = time.monotonic()+420
            while process.poll() is None and time.monotonic() < deadline:
                remember()
                time.sleep(0.05)
            code = process.wait(timeout=1)
        manifests = list(recovery.glob(f'{board}/*/manifest.json'))
        if code or len(manifests) != 1:
            raise RuntimeError('Ordinary startup recovery failed')
        directory = manifests[0].parent
        manifest = _read(manifests[0])
        execution = _read(directory / 'execution.json') or {}
        result = decode((directory / 'last-result.bin').read_bytes(), manifest['build_id'], STARTUP.cases)
        if (result is None or manifest['build_id'] == receipt['build_id'] or
                manifest['device'] != receipt['device'] or execution.get('status') != 'passed' or
                execution.get('probe') != request.probe or not _released(request.probe)):
            raise RuntimeError('Recovery evidence/lease did not satisfy the control')
        receipt.update(status='passed', board_state='startup-recovered', recovery=str(directory),
                       recovery_build_id=manifest['build_id'])
        return receipt
    except BaseException as exc:
        receipt['error'] = str(exc)
        raise
    finally:
        try:
            remember()
            remaining = [member for member in owned.values() if alive(member)]
            if remaining:
                receipt.update(status='failed', board_state='unknown', emergency_cleanup=True)
                for member in reversed(remaining):
                    try:
                        member.kill()  # psutil checks process identity, including PID reuse.
                    except psutil.NoSuchProcess:
                        pass
                _, left = psutil.wait_procs(remaining, timeout=5)
                receipt['cleanup_survivors'] = [member.pid for member in left]
            if process is not None:
                process.wait(timeout=5)
        except BaseException as exc:
            receipt.update(status='failed', board_state='unknown', cleanup_error=str(exc))
            raise
        finally:
            (output / 'control.json').write_text(json.dumps(receipt, indent=2)+'\n')
