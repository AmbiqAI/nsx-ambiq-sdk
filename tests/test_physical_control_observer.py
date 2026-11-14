"""Exercise the physical-control observer with real processes, no SEGGER/device."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from contextlib import contextmanager

import pytest

from tools.hardware.api import BoardRequest
from tools.hardware import lifecycle
from tools.hardware.protocol import HEADER, MAGIC, VERSION, RESULT_SIZE
from tools.hardware.runner import probe_lock
from tools.hardware.suites import STARTUP


def complete_packet(build_id):
    text = (''.join(f'main.cpp:1:{case}:PASS\n' for case in STARTUP.cases) +
            '3 Tests 0 Failures 0 Ignored\nOK\n').encode()
    header = HEADER.pack(MAGIC, VERSION, 2, 3, 0, 0, len(text), 0, build_id.encode())
    return header + text + bytes(RESULT_SIZE-len(header)-len(text))


@pytest.mark.skipif(sys.platform != 'linux', reason='Linux observer/process lease')
@pytest.mark.parametrize('action,broken,late_child', [('term', False, False), ('interrupt', False, False),
                                                     ('timeout', False, False), ('term', True, False),
                                                     ('term', True, True)])
def test_observer_distinguishes_native_cleanup_from_its_emergency_cleanup(
        tmp_path, repo_root, monkeypatch, action, broken, late_child):
    pytest.importorskip('psutil')
    candidate = tmp_path/'candidate'
    candidate.mkdir()
    monkeypatch.chdir(tmp_path)
    marker = tmp_path/'late-child.json'
    request = BoardRequest(str(800000000+os.getpid()), 'fake-device', 1)
    def fake_command(repo, board, gcc_root, request, output, sdk_root, fault, timeout):
        assert sdk_root == candidate, 'Relative SDK root must resolve before child cwd changes'
        directory = output / board / 'only-owned-build'
        build_id = 'negative' if fault == 'timeout' else 'recovery'
        manifest = dict(board=board, suite='startup', fault_injection=fault,
                        expected_cases=list(STARTUP.cases), device=request.device, build_id=build_id)
        common = f'''
import json, signal, subprocess, sys, time
from pathlib import Path
directory=Path({str(directory)!r})
directory.mkdir(parents=True)
(directory/'manifest.json').write_text({json.dumps(manifest)!r})
(directory/'jlink.json').write_text({json.dumps(dict(device=request.device))!r})
'''
        if fault == 'none':
            code = common + f'''
(directory/'last-result.bin').write_bytes({complete_packet(build_id)!r})
(directory/'execution.json').write_text({json.dumps(dict(status='passed',probe=request.probe))!r})
'''
        else:
            worker = f'''
import json, os, subprocess, sys, time
from pathlib import Path
directory=Path({str(directory)!r})
parent=os.getppid()
(directory/'seeded.json').write_text({json.dumps({s.symbol:s.value for s in STARTUP.seeds})!r})
if {late_child!r}:
    while os.getppid() == parent:
        time.sleep(0.01)
    import psutil
    leaf=subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(60)'], pass_fds=(int(sys.argv[-1]),))
    marker=Path({str(marker)!r})
    pending=marker.with_suffix('.pending')
    pending.write_text(json.dumps(dict(pid=leaf.pid,created=psutil.Process(leaf.pid).create_time())))
    pending.replace(marker)
time.sleep({1 if action == 'timeout' else 60})
(directory/'failure.json').write_text({json.dumps(dict(status='qualification_failed', error='Timed out waiting for complete firmware evidence'))!r})
raise SystemExit(1)
'''
            command = [sys.executable, '-c', worker, 'tools.hardware.jlink_worker', '--output', str(directory)]
            if broken:
                substitute = f'''
    child=subprocess.Popen([*{command!r}, str(kwargs['lease_fd'])], pass_fds=(kwargs['lease_fd'],), start_new_session=True)
    def cancel(signum, frame):
        raise RunCancelled('deliberate defective cleanup')
    signal.signal(signal.SIGTERM, cancel)
    child.wait(timeout=60)
'''
            else:
                substitute = f'''
    return original({command!r}, **kwargs)
'''
            code = common + f'''
from tools.hardware import runner
from tools.hardware.build import Firmware
from tools.hardware.process import RunCancelled
original=runner.run_worker
def substitute(command, **kwargs):
{substitute}
runner.run_worker=substitute
runner.execute(Firmware(Path('unused'),0,{build_id!r}, {board!r}, {request.device!r},1000, {STARTUP.cases!r}),
               {request.probe!r}, {request.device!r}, 5, directory)
'''
        return [sys.executable, '-c', code]
    monkeypatch.setattr(lifecycle, '_startup_command', fake_command)
    if late_child:
        original_outcome = lifecycle._native_outcome
        def after_late_child(*args, **kwargs):
            deadline = time.monotonic()+5
            while not marker.exists() and time.monotonic() < deadline:
                time.sleep(0.01)
            assert marker.exists(), 'Owned worker did not spawn its late child'
            return original_outcome(*args, **kwargs)
        monkeypatch.setattr(lifecycle, '_native_outcome', after_late_child)
    output = tmp_path / 'control'
    with dispose_leaf(marker):
        if broken:
            with pytest.raises(RuntimeError, match='Native transport cleanup'):
                lifecycle.run_control(repo_root, 'fake-board', Path('/unused'), request,
                                       output, action, Path('candidate'))
            receipt = json.loads((output/'control.json').read_text())
            assert receipt['native_outcome']['worker_alive']
            assert not receipt['native_outcome']['lease_released']
            assert receipt['status'] == 'failed' and receipt['board_state'] == 'unknown'
            assert receipt['emergency_cleanup']
            assert not (output/'recovery').exists()
        else:
            receipt = lifecycle.run_control(repo_root, 'fake-board', Path('/unused'), request,
                                            output, action, Path('candidate'))
            assert receipt['status'] == 'passed'
            assert not receipt['emergency_cleanup']
            assert receipt['board_state'] == 'startup-recovered'
            assert receipt['build_id'] != receipt['recovery_build_id']
        with probe_lock(request.probe):
            pass


@contextmanager
def dispose_leaf(marker):
    """A failing regression must not leak the exact fake leaf it created."""
    import psutil
    try:
        yield
    finally:
        if marker.is_file():
            identity = json.loads(marker.read_text())
            try:
                child = psutil.Process(identity['pid'])
                if child.create_time() == identity['created']:
                    child.kill()
            except psutil.NoSuchProcess:
                pass


@pytest.mark.parametrize('change', ['success', 'live-worker', 'held-lease', 'wrong-status', 'wrong-timeout'])
def test_wrong_control_failure_cannot_pass(tmp_path, change):
    (tmp_path/'execution.json').write_text(json.dumps(dict(status='qualification_failed')))
    (tmp_path/'failure.json').write_text(json.dumps(dict(status='qualification_failed',
        error='disconnected' if change == 'wrong-timeout' else 'Timed out waiting for complete firmware evidence')))
    if change == 'wrong-status':
        (tmp_path/'execution.json').write_text(json.dumps(dict(status='infrastructure_error')))
    with pytest.raises(RuntimeError):
        lifecycle._native_outcome(tmp_path, 'timeout', 0 if change == 'success' else 1,
                                   change == 'live-worker', change != 'held-lease')


@pytest.mark.skipif(sys.platform != 'linux', reason='Linux observer/process lease')
@pytest.mark.parametrize('cancel_signal', (signal.SIGTERM, signal.SIGINT))
@pytest.mark.parametrize('repeat_during_cleanup', (False, True))
def test_cancelling_observer_stops_owned_child_and_records_unknown_state(
        tmp_path, repo_root, cancel_signal, repeat_during_cleanup):
    psutil = pytest.importorskip('psutil')
    serial = str(810000000+os.getpid())
    ready = tmp_path/'child-ready'
    child = f'''
import os, time
from pathlib import Path
from tools.hardware.runner import probe_lock
with probe_lock({serial!r}):
    Path({str(ready)!r}).write_text(str(os.getpid()))
    time.sleep(60)
'''
    observer = f'''
from pathlib import Path
from tools.hardware import lifecycle
from tools.hardware.api import BoardRequest
import os, signal, sys
original_signal=signal.signal
injected=False
def intercept_signal(signum, handler):
    global injected
    if {repeat_during_cleanup!r} and handler == signal.SIG_IGN and not injected:
        injected=True
        # Arrive exactly before the observer installs its cleanup ignore mask.
        os.kill(os.getpid(), {int(cancel_signal)!r})
    return original_signal(signum, handler)
signal.signal=intercept_signal
lifecycle._startup_command=lambda *args: [sys.executable, '-c', {child!r}]
lifecycle.run_control(Path({str(repo_root)!r}), 'fake-board', Path('/unused'),
    BoardRequest({serial!r}), Path({str(tmp_path/'control')!r}), 'term')
'''
    owned = None
    with (tmp_path/'observer.log').open('w') as log:
        process = subprocess.Popen([sys.executable, '-c', observer], cwd=repo_root,
                                   stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+10
            while time.monotonic() < deadline:
                if ready.is_file() and ready.read_text():
                    owned = psutil.Process(int(ready.read_text()))
                    break
                assert process.poll() is None, (tmp_path/'observer.log').read_text()
                time.sleep(0.02)
            assert owned is not None, 'Fake child did not acquire its lease'
            process.send_signal(cancel_signal)
            process.wait(timeout=10)
            assert process.returncode != 0
            receipt = json.loads((tmp_path/'control/control.json').read_text())
            assert receipt['status'] == 'failed' and receipt['board_state'] == 'unknown'
            assert receipt['emergency_cleanup']
            assert 'observer cancelled' in receipt['error']
            assert not owned.is_running() or owned.status() == psutil.STATUS_ZOMBIE
            with probe_lock(serial):
                pass
        finally:
            if owned is not None and owned.is_running():
                owned.kill()
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)
