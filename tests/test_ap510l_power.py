"""AP510L backend policy, with HAL effects captured instead of touching memory."""
import os
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module', params=(True, False))
def ap510l_power(request, repo_root, tmp_path_factory):
    fixture = repo_root / 'tests/fixtures/ap510l-power'
    binary = tmp_path_factory.mktemp('ap510l-power') / 'probe'
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DAM_PART_APOLLO510L']
    if not request.param:
        command.append('-DNSX_DISABLE_API_VALIDATION')
    for directory in (fixture, repo_root / 'modules/nsx-core/includes-api',
                      repo_root / 'modules/nsx-core/includes-internal',
                      repo_root / 'modules/nsx-power/includes-api'):
        command.extend(('-I', str(directory)))
    command.extend(str(repo_root / source) for source in (
        'tests/fixtures/ap510l-power/main.c', 'modules/nsx-core/src/nsx_startup.c',
        'modules/nsx-power/src/nsx_power.c', 'modules/nsx-power/src/nsx_power_preflight.c',
        'modules/nsx-power/src/apollo5/nsx_power.c'))
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)
    result = subprocess.run([*command, '-o', str(binary)], env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return binary


@pytest.mark.parametrize('small', (0, 1))
@pytest.mark.parametrize('ssram', (0, 1))
@pytest.mark.parametrize('keep', (0, 1))
@pytest.mark.parametrize('failure', (0, 1, 2))
def test_ap510l_backend_policy(ap510l_power, small, ssram, keep, failure):
    subprocess.run([str(ap510l_power), *map(str, (small, ssram, keep, failure))],
                   check=True, capture_output=True, text=True, timeout=5)
