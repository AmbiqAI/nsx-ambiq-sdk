"""Public debug APIs share actual NSX runtime/coordinator transport ownership."""
import os
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module')
def debug_binary(repo_root, tmp_path_factory):
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required for debug state tests'
    core = repo_root / 'modules/nsx-core'
    fixture = repo_root / 'tests/fixtures'
    output = tmp_path_factory.mktemp('debug-state') / 'debug'
    env = os.environ.copy()
    env.pop('GCC_ROOT', None)
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-DAM_PART_APOLLO510', '-DNSX_DISABLE_API_VALIDATION',
                    '-I' + str(fixture / 'system-status'), '-I' + str(core / 'includes-api'),
                    '-I' + str(core / 'includes-internal'),
                    '-I' + str(repo_root / 'modules/nsx-power/includes-api'),
                    str(repo_root / 'modules/nsx-power/src/nsx_power.c'),
                    str(repo_root / 'modules/nsx-power/src/nsx_power_preflight.c'),
                    str(fixture / 'debug-state/main.c'),
                    *(str(core / 'src' / source) for source in
                      ('nsx_debug.c', 'nsx_system.c', 'nsx_startup.c', 'nsx_core_portable.c')),
                    '-o', str(output)], check=True, capture_output=True, text=True, env=env)
    return output


@pytest.mark.parametrize('scenario', ('system', 'system-reject', 'resume', 'switch', 'noop', 'setup-failure',
                                     'switch-failure', 'disable-failure', 'hardware-reset', 'system-reapply', 'hardware-release-failure', 'power-release', 'power-release-failure'))
def test_debug_transport_contract(debug_binary, scenario):
    subprocess.run([str(debug_binary), scenario], check=True, capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('board', ('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510b_evb', 'apollo330mP_evb'))
def test_debug_state_real_header_builds(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import DEBUG_STATE
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, DEBUG_STATE))


def test_apollo2_debug_backend_compiles(repo_root, tmp_path):
    import json
    import shlex
    from test_cmake_contract import configure_contract_project
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    result = configure_contract_project(repo_root, tmp_path, 'apollo2_evb', 'gcc', ('nsx-core',),
                                        prelude=('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)',))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc/apollo2_evb/build/compile_commands.json').read_text())
    entry = next(e for e in entries if e['file'].endswith('/apollo2/nsx_system_platform.c'))
    command = shlex.split(entry['command'])
    command[0] = str(compiler)
    command[command.index('-o') + 1] = str(tmp_path / 'apollo2.o')
    command.append('-Werror=implicit-function-declaration')
    subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
