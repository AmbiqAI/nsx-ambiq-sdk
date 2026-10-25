"""Actual AP4 TempCo and timer adapters with a recording HAL boundary."""
import os
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module')
def tempco_binary(repo_root, tmp_path_factory):
    output = tmp_path_factory.mktemp('tempco') / 'test'
    fixture = repo_root / 'tests/fixtures/tempco'
    includes = [fixture, repo_root / 'modules/nsx-power/src/apollo4']
    includes += [repo_root / f'modules/{name}/includes-api'
                 for name in ('nsx-power', 'nsx-core', 'nsx-timer', 'nsx-interrupt')]
    sources = [fixture / 'main.c', repo_root / 'modules/nsx-timer/src/nsx_timer.c',
               repo_root / 'modules/nsx-timer/src/apollo4/nsx_timer_ap4.c']
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)
    compiler = shutil.which('cc')
    assert compiler
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DAM_PART_APOLLO4P',
                    *[flag for path in includes for flag in ('-I', str(path))],
                    *map(str, sources), '-o', str(output)], check=True, env=env,
                   capture_output=True, text=True)
    return output


@pytest.mark.parametrize('scenario', ['success', 'init-failures', 'cleanup-failures',
                                      'callback-failures', 'callback-status', 'unsupported', 'trim-read-failure', 'init-cleanup-failure', 'timer-owner', 'timer-reconfigure'])
def test_tempco_lifecycle(tempco_binary, scenario):
    subprocess.run([str(tempco_binary), scenario], check=True, capture_output=True, text=True, timeout=5)


def test_tempco_smoke_builds(repo_root, tmp_path):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import TEMPCO
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, 'apollo4p_blue_kxr_evb',
                                compiler.parent.parent, TEMPCO))
