"""Exercise the physical-control observer with real processes, no SEGGER/device."""
import json
import os
from pathlib import Path
import sys

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
@pytest.mark.parametrize('action,broken', [('term', False), ('interrupt', False),
                                         ('timeout', False), ('term', True)])
def test_observer_distinguishes_native_cleanup_from_its_emergency_cleanup(
        tmp_path, repo_root, monkeypatch, action, broken):
    pytest.importorskip('psutil')
    request = BoardRequest(str(800000000+os.getpid()), 'fake-device', 1)
    def fake_command(repo, board, gcc_root, request, output, sdk_root, fault, timeout):
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
import json, time
from pathlib import Path
directory=Path({str(directory)!r})
(directory/'seeded.json').write_text({json.dumps({s.symbol:s.value for s in STARTUP.seeds})!r})
time.sleep({1 if action == 'timeout' else 60})
(directory/'failure.json').write_text({json.dumps(dict(status='qualification_failed', error='Timed out waiting for complete firmware evidence'))!r})
raise SystemExit(1)
'''
            command = [sys.executable, '-c', worker, 'tools.hardware.jlink_worker', '--output', str(directory)]
            if broken:
                substitute = f'''
    child=subprocess.Popen({command!r}, pass_fds=(kwargs['lease_fd'],), start_new_session=True)
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
    output = tmp_path / 'control'
    if broken:
        with pytest.raises(RuntimeError, match='Native transport cleanup'):
            lifecycle.run_control(repo_root, 'fake-board', Path('/unused'), request, output, action)
        receipt = json.loads((output/'control.json').read_text())
        assert receipt['native_outcome']['worker_alive']
        assert not receipt['native_outcome']['lease_released']
        assert receipt['status'] == 'failed' and receipt['board_state'] == 'unknown'
        assert receipt['emergency_cleanup']
        assert not (output/'recovery').exists()
    else:
        receipt = lifecycle.run_control(repo_root, 'fake-board', Path('/unused'), request, output, action)
        assert receipt['status'] == 'passed'
        assert not receipt['emergency_cleanup']
        assert receipt['board_state'] == 'startup-recovered'
        assert receipt['build_id'] != receipt['recovery_build_id']
    with probe_lock(request.probe):
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
