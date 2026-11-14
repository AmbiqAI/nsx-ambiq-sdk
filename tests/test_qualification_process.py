"""A completed leader must not leave a live group holding lab resources."""
import math
import os
from pathlib import Path
import signal
import subprocess
import sys

import pytest

from tools.hardware.process import _group_alive, run_worker


pytestmark = pytest.mark.skipif(sys.platform != 'linux', reason='Linux worker groups')


@pytest.mark.parametrize('early_exit', [True, False])
def test_leader_exit_and_timeout_both_clean_descendants(tmp_path, early_exit):
    ready = tmp_path / 'child'
    child = ('import os,signal,time; from pathlib import Path; '
             'signal.signal(signal.SIGTERM, signal.SIG_IGN); '
             f'Path({str(ready)!r}).write_text(str(os.getpid())+":"+str(os.getpgrp())); time.sleep(60)')
    leader = (f'import subprocess,sys,time; from pathlib import Path; '
              f'p=subprocess.Popen([sys.executable,"-c",{child!r}]); '
              f'path=Path({str(ready)!r});\n'
              'while not path.exists(): time.sleep(0.01)\n' +
              ('sys.exit(0)' if early_exit else 'time.sleep(60)'))
    group = None
    try:
        with (tmp_path / 'worker.log').open('w') as log:
            error = RuntimeError if early_exit else subprocess.TimeoutExpired
            with pytest.raises(error):
                run_worker([sys.executable, '-c', leader], log=log, cwd=tmp_path, timeout=2)
        _, group = map(int, ready.read_text().split(':'))
        assert not _group_alive(group)
    finally:
        if group is None and ready.exists():
            _, group = map(int, ready.read_text().split(':'))
        if group is not None and _group_alive(group):
            try:
                os.killpg(group, signal.SIGKILL)
            except ProcessLookupError:
                pass


def test_host_worker_uses_explicit_environment(tmp_path):
    with (tmp_path / 'worker.log').open('w') as log:
        run_worker([sys.executable, '-c', 'import os; print(os.environ["QUALIFIER_TEST"])'],
                   log=log, cwd=tmp_path, timeout=5, env={'QUALIFIER_TEST': 'pinned'})
    assert (tmp_path / 'worker.log').read_text().strip() == 'pinned'


@pytest.mark.parametrize('timeout', [0, -1, math.inf, math.nan])
def test_invalid_deadline_never_launches_a_worker(tmp_path, timeout):
    marker = tmp_path / 'launched'
    with (tmp_path / 'worker.log').open('w') as log:
        with pytest.raises(ValueError, match='finite and positive'):
            run_worker([sys.executable, '-c', f'open({str(marker)!r},"w").close()'],
                       log=log, cwd=tmp_path, timeout=timeout)
    assert not marker.exists()
