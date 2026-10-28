"""Exercise actual TIMER adapters at their HAL boundary, including units/range."""
import os
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module', params=[
    (family, disabled) for family in ('apollo4', 'apollo5', 'apollo330')
    for disabled in (False, True)
], ids=lambda value: f'{value[0]}-validation-{not value[1]}')
def timer_binary(request, repo_root, tmp_path_factory):
    family, disabled = request.param
    source, macro = {
        'apollo4': ('apollo4/nsx_timer_ap4.c', 'AM_PART_APOLLO4P'),
        'apollo5': ('apollo5/nsx_timer_ap5.c', 'AM_PART_APOLLO510'),
        'apollo330': ('apollo330/nsx_timer_ap330.c', 'AM_PART_APOLLO330P'),
    }[family]
    fixture = repo_root / 'tests/fixtures/timer-period'
    output = tmp_path_factory.mktemp('timer-period') / 'test'
    compiler = shutil.which('cc')
    assert compiler
    includes = [fixture, *(repo_root / f'modules/nsx-{name}/includes-api'
                           for name in ('core', 'timer', 'interrupt'))]
    command = [compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', f'-D{macro}']
    if disabled:
        command.append('-DNSX_DISABLE_API_VALIDATION')
    command += [flag for path in includes for flag in ('-I', str(path))]
    command += [str(fixture / 'main.c'), str(repo_root / 'modules/nsx-timer/src/nsx_timer.c'),
                str(repo_root / 'modules/nsx-timer/src' / source), '-o', str(output)]
    host_env = os.environ.copy()
    host_env.pop('GCC_ROOT', None)
    subprocess.run(command, check=True, capture_output=True, text=True, env=host_env)
    return output


@pytest.mark.parametrize('period', [0, 1, 5, 6, 7, 1000, 50000, 715827882, 715827883, 4294967295])
@pytest.mark.parametrize('timer', [1, 3, 4])
def test_timer_period(timer_binary, timer, period):
    subprocess.run([str(timer_binary), 'period', str(timer), str(period)],
                   check=True, capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('scenario', ['counter', 'default-failure'])
def test_timer_nonperiodic_and_defaults(timer_binary, scenario):
    subprocess.run([str(timer_binary), scenario, '0', '4294967295' if scenario=='counter' else '1000'],
                   check=True, capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('board', ['apollo4p_blue_kxr_evb', 'apollo330mP_evb',
                                  'apollo510_evb', 'apollo510b_evb'])
def test_timer_smoke_builds(repo_root, tmp_path, board):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import TIMER_PERIOD
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, TIMER_PERIOD))


@pytest.mark.parametrize('board', ['apollo4l_evb', 'apollo510dL_evb'])
def test_timer_other_variants_compile(repo_root, tmp_path, board):
    import json
    import shlex
    from test_cmake_contract import configure_contract_project
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc',
        ('nsx-core', 'nsx-interrupt', 'nsx-timer'),
        prelude=('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)',))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc' / board / 'build/compile_commands.json').read_text())
    timer = [entry for entry in entries if '/modules/nsx-timer/src/' in entry['file']]
    assert len(timer) == 2
    for index, entry in enumerate(timer):
        command = shlex.split(entry['command'])
        command[0] = str(compiler)
        command[command.index('-o') + 1] = str(tmp_path / f'timer-{index}.o')
        subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
