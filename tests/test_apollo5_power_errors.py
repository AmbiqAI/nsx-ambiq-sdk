"""First-error contract against each real Apollo5-class power startup backend."""
import os
import shutil
import subprocess

import pytest

FAMILIES = ('apollo510', 'apollo510b', 'apollo5b', 'apollo330p', 'apollo510l')


@pytest.fixture(scope='module', params=[(family, validation) for family in FAMILIES for validation in (True, False)])
def power_errors(request, repo_root, tmp_path_factory):
    family, validation = request.param
    arch = 'apollo330' if family == 'apollo330p' else 'apollo5'
    root = tmp_path_factory.mktemp('power-errors')
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-DAM_PART_' + family.upper()]
    if family in ('apollo330p', 'apollo510l'):
        command.append('-DAM_PART_APOLLO330P_510L')
    if family == 'apollo5b':
        command.append('-DAM_PART_APOLLO510')
    if not validation:
        command.append('-DNSX_DISABLE_API_VALIDATION')
    for path in ('tests/fixtures/apollo5-power-errors', 'tests/fixtures/ap510l-power',
                 'modules/nsx-core/includes-api', 'modules/nsx-core/includes-internal',
                 'modules/nsx-power/includes-api', 'modules/nsx-power/src'):
        command.extend(('-I', str(repo_root / path)))
    command.extend(str(repo_root / path) for path in (
        'tests/fixtures/apollo5-power-errors/main.c', 'modules/nsx-core/src/nsx_startup.c',
        'modules/nsx-power/src/nsx_power.c', 'modules/nsx-power/src/nsx_power_preflight.c',
        f'modules/nsx-power/src/{arch}/nsx_power.c'))
    binary = root / 'test'
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)
    result = subprocess.run([*command, '-o', str(binary)], env=env, capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    return binary, family


def expected_calls(family, keep):
    calls = ['debug-release', 'bsp']
    if family not in ('apollo330p', 'apollo510l'):
        calls.append('spotmgr')
    calls += (['icache', 'dcache', 'cpdlp'] if family == 'apollo330p'
              else ['cpdlp', 'icache', 'dcache'])
    if not keep:
        calls += ['rtc-select', 'rtc-disable', 'periph-10', 'periph-11']
    calls += ['periph-12', 'mcu-memory', 'sram', 'fpu', 'fpu-stacking']
    if not keep:
        calls += ['clkgen', 'periph-8', 'periph-9', *[f'periph-{n}' for n in range(6)]]
        if family not in ('apollo330p', 'apollo510l'):
            calls += ['periph-20', 'periph-21']
        calls += ['periph-6', 'periph-7']
        if family not in ('apollo330p', 'apollo510l'):
            calls += ['periph-22', 'periph-23', 'periph-24']
        else:
            calls.append('rss')
    return calls + ['delay', 'performance', 'printf']


@pytest.mark.parametrize('keep', (0, 1))
@pytest.mark.parametrize('mask', (0, 1))
@pytest.mark.parametrize(('small', 'ssram'), ((0, 1), (1, 0)))
def test_apollo5_first_error(power_errors, keep, mask, small, ssram):
    binary, family = power_errors
    def run(failure):
        return subprocess.run([str(binary), *map(str, (keep, failure, mask, small, ssram))],
                              check=True, capture_output=True, text=True, timeout=5).stdout.splitlines()
    expected = expected_calls(family, keep)
    assert run(0) == expected
    void_calls = {'bsp', 'spotmgr', 'rtc-select', 'fpu', 'fpu-stacking', 'delay', 'printf'}
    for step, name in enumerate(expected, 1):
        if name not in void_calls:
            assert run(step) == expected[:step], (family, name)


@pytest.mark.parametrize('board', ('apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb'))
def test_power_startup_smoke_builds(repo_root, tmp_path, board):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import POWER_STARTUP
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, POWER_STARTUP))
