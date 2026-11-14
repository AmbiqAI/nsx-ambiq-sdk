"""Linux worker groups: bounded cancellation, cleanup and inherited probe leases."""
import math
import os
import signal
import subprocess
import sys
import time
from pathlib import Path
from typing import IO


class RunCancelled(InterruptedError):
    """The caller requested termination; worker execution did not pass."""


def _group_alive(group: int) -> bool:
    try:
        os.killpg(group, 0)
    except ProcessLookupError:
        return False
    # Adopted zombies have no running code or open lease. Linux is the supported
    # execution host; an unobservable group is conservatively still active.
    found = False
    for path in Path('/proc').glob('[0-9]*/stat'):
        try:
            fields = path.read_text().rsplit(')', 1)[1].split()
            if int(fields[2]) == group:
                found = True
                if fields[0] not in ('Z', 'X'):
                    return True
        except (OSError, ValueError, IndexError):
            continue
    return not found


def run_worker(command: list[str], *, log: IO[str], cwd: Path,
               lease_fd: int | None = None, timeout: float,
               env: dict[str, str] | None = None) -> None:
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError('Worker timeout must be finite and positive')
    signals = {signal.SIGTERM, signal.SIGINT}
    previous = {sig: signal.getsignal(sig) for sig in signals}
    worker = None
    succeeded = False

    def cancel(signum, frame):
        raise RunCancelled(f'Worker run cancelled by signal {signum}')

    def stop():
        if worker is None:
            return False
        worker.poll()
        alive = _group_alive(worker.pid)
        if not alive:
            return False
        try:
            os.killpg(worker.pid, signal.SIGTERM)
        except ProcessLookupError:
            return False
        deadline = time.monotonic() + 3
        while _group_alive(worker.pid) and time.monotonic() < deadline:
            worker.poll()
            time.sleep(0.02)
        if _group_alive(worker.pid):
            try:
                os.killpg(worker.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
        worker.wait(timeout=5)
        deadline = time.monotonic() + 5
        while _group_alive(worker.pid) and time.monotonic() < deadline:
            time.sleep(0.02)
        if _group_alive(worker.pid):
            raise RuntimeError('Worker process-group cleanup failed')
        return alive

    try:
        for sig in signals:
            signal.signal(sig, cancel)
        mask = signal.pthread_sigmask(signal.SIG_BLOCK, signals)
        try:
            launcher = [sys.executable, str(Path(__file__).with_name('child_exec.py')), *command]
            worker = subprocess.Popen(launcher, stdout=log, stderr=subprocess.STDOUT,
                                      cwd=cwd, pass_fds=() if lease_fd is None else (lease_fd,),
                                      start_new_session=True, env=env)
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, mask)
        code = worker.wait(timeout=timeout)
        if code:
            raise subprocess.CalledProcessError(code, command)
        succeeded = True
    finally:
        for sig in signals:
            signal.signal(sig, signal.SIG_IGN)
        try:
            leftover = stop()
            if succeeded and leftover:
                raise RuntimeError('Worker exited with live descendants; process group was terminated')
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
