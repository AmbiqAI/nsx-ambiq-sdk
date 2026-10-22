"""Linux worker lifetime: inherited probe lease and bounded cancellation cleanup."""
import os
import signal
import subprocess
from pathlib import Path
from typing import IO


class RunCancelled(InterruptedError):
    """The caller requested termination; hardware execution did not pass."""


def run_worker(command: list[str], *, log: IO[str], cwd: Path,
               lease_fd: int, timeout: float) -> None:
    signals = {signal.SIGTERM, signal.SIGINT}
    previous = {sig: signal.getsignal(sig) for sig in signals}
    worker = None

    def cancel(signum, frame):
        raise RunCancelled(f'Hardware run cancelled by signal {signum}')

    def stop():
        if worker is None or worker.poll() is not None:
            return
        try:
            os.killpg(worker.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            worker.wait(timeout=3)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(worker.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            worker.wait(timeout=5)

    try:
        for sig in signals:
            signal.signal(sig, cancel)
        # Prevent cancellation between fork and acquiring the process handle.
        mask = signal.pthread_sigmask(signal.SIG_BLOCK, signals)
        try:
            worker = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT,
                                      cwd=cwd, pass_fds=(lease_fd,), start_new_session=True)
        finally:
            signal.pthread_sigmask(signal.SIG_SETMASK, mask)
        code = worker.wait(timeout=timeout)
        if code:
            raise subprocess.CalledProcessError(code, command)
    finally:
        # Repeated cancellation must not interrupt termination/reaping.
        for sig in signals:
            signal.signal(sig, signal.SIG_IGN)
        try:
            stop()
        finally:
            for sig, handler in previous.items():
                signal.signal(sig, handler)
