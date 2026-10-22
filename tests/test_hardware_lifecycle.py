"""Real OS-process tests; no SEGGER library or physical board is used."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

import pytest

from tools.hardware.runner import probe_lock
from tools.hardware.startup import OMISSIONS, POISON, SENTINELS, mutant, poison_sentinels


def wait_until(predicate, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(0.02)
    pytest.fail('Timed out waiting for process state')


@pytest.mark.skipif(sys.platform != 'linux', reason='Linux hardware lifecycle')
@pytest.mark.parametrize('action', ['term', 'interrupt', 'timeout', 'kill-parent'])
def test_real_worker_lifecycle(tmp_path, repo_root, action):
    serial = str(900000000 + os.getpid())
    ready = tmp_path / 'ready'
    child = f'''
import os, signal, time
from pathlib import Path
signal.pthread_sigmask(signal.SIG_UNBLOCK, {{signal.SIGTERM, signal.SIGINT}})
# Exercise forced cleanup, not just a cooperative child.
signal.signal(signal.SIGTERM, signal.SIG_IGN)
Path({str(ready)!r}).write_text(str(os.getpid()))
time.sleep(60)
'''
    parent = f'''
import sys
from pathlib import Path
from tools.hardware import runner
from tools.hardware.build import Firmware
original = runner.run_worker
def substitute(command, **kwargs):
    kwargs['timeout'] = {1 if action == 'timeout' else 30}
    return original([sys.executable, '-c', {child!r}], **kwargs)
runner.run_worker = substitute
runner.execute(Firmware(Path('unused'), 0, 'id', 'board', 'device', 4000),
               {serial!r}, 'device', 1, Path({str(tmp_path)!r}))
'''
    worker_pid = None
    with (tmp_path / 'parent.log').open('w') as log:
        process = subprocess.Popen([sys.executable, '-c', parent], cwd=repo_root,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            wait_until(lambda: ready.is_file() and bool(ready.read_text()))
            worker_pid = int(ready.read_text())
            with pytest.raises(RuntimeError, match='already held'):
                with probe_lock(serial):
                    pass
            if action != 'timeout':
                process.send_signal({'term': signal.SIGTERM, 'interrupt': signal.SIGINT,
                                     'kill-parent': signal.SIGKILL}[action])
            process.wait(timeout=10)
            assert process.returncode != 0
            if action == 'kill-parent':
                # A dead parent cannot clean up, but must not expose a live probe.
                os.kill(worker_pid, 0)
                with pytest.raises(RuntimeError, match='already held'):
                    with probe_lock(serial):
                        pass
                os.killpg(worker_pid, signal.SIGKILL)
            else:
                with pytest.raises(ProcessLookupError):
                    os.kill(worker_pid, 0)  # Parent reaped the worker.
                record = json.loads((tmp_path / 'execution.json').read_text())
                assert record['status'] == ('infrastructure_error' if action == 'timeout' else 'cancelled')
            def released():
                try:
                    with probe_lock(serial):
                        return True
                except RuntimeError:
                    return False
            wait_until(released)
        finally:
            if worker_pid is not None:
                try:
                    os.killpg(worker_pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)


class FakeMemory:
    def __init__(self, drop_write=False):
        self.words = {}
        self.drop_write = drop_write

    def memory_write32(self, address, values):
        if not self.drop_write:
            self.words[address] = values[0]
        return len(values)

    def memory_read32(self, address, count):
        return [self.words.get(address, 0)]


def test_sentinels_require_verified_poison():
    memory = FakeMemory()
    assert poison_sentinels(memory, (4, 8, 12)) == dict(zip(SENTINELS, POISON))
    with pytest.raises(RuntimeError, match='Could not seed'):
        poison_sentinels(FakeMemory(drop_write=True), (4, 8, 12))
    with pytest.raises(ValueError):
        poison_sentinels(memory, (4, 4, 8))


@pytest.mark.parametrize('omission', OMISSIONS)
def test_startup_negative_control_omits_real_operation(repo_root, omission):
    source = (repo_root / 'modules/nsx-core/src/apollo3p/gcc/startup_gcc.c').read_text()
    changed = mutant(source, omission)
    assert source != changed
    assert f'Negative control: {omission}' in changed
    with pytest.raises(ValueError, match='exactly one'):
        mutant(changed, omission)
    with pytest.raises(ValueError, match='exactly one'):
        mutant(source + source, omission)
