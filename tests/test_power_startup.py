"""Public power configuration against real NSX lifecycle and controlled HAL calls."""
import json
import os
import shlex
import shutil
import subprocess

import pytest

from test_cmake_contract import configure_contract_project
from test_small_m55_memory import compiler_for_test


COMMON = ('null', 'null-api', 'bad-api', 'bad-version', 'invalid-mode', 'no-core',
          'success', 'success-keep', 'fail-minimal', 'fail-keep')
FAMILIES = ('apollo3', 'apollo3p', 'apollo4p', 'apollo4l')
APOLLO5 = ('apollo510', 'apollo510b', 'apollo5a', 'apollo5b', 'apollo330p', 'apollo510l')


@pytest.fixture
def api_validation():
    return True


@pytest.fixture
def startup_binary(request, repo_root, tmp_path, api_validation):
    family = request.param
    arch = 'apollo3' if family.startswith('apollo3') else 'apollo4'
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required for startup contract tests'
    fixture = repo_root / 'tests/fixtures/power-startup'
    includes = ['-I', str(fixture), '-I', str(repo_root / 'modules/nsx-power/includes-api'),
                '-I', str(repo_root / 'modules/nsx-power/src'),
                '-I', str(repo_root / 'modules/nsx-core/includes-api'),
                '-I', str(repo_root / 'modules/nsx-core/includes-internal')]
    flags = ['-std=c11', '-Wall', '-Wextra', '-DAM_PART_' + family.upper()]
    if family in APOLLO5:
        flags.append('-DTEST_APOLLO5_OWNERSHIP')
    if not api_validation:
        flags.append('-DNSX_DISABLE_API_VALIDATION')
    host_env = os.environ.copy()
    host_env.pop('GCC_ROOT', None)
    core = tmp_path / 'core.o'
    subprocess.run([compiler, *flags, *includes, '-Dnsx_core_fail_loop=nsx_test_original_fail_loop',
                    '-c', str(repo_root / 'modules/nsx-core/src/nsx_core_lifecycle.c'), '-o', str(core)],
                   check=True, capture_output=True, text=True, env=host_env)
    binary = tmp_path / 'startup'
    subprocess.run([compiler, *flags, *includes, str(fixture / 'main.c'), str(core),
                    str(repo_root / 'modules/nsx-core/src/nsx_startup.c'),
                    str(repo_root / 'modules/nsx-core/src/nsx_system.c'),
                    str(repo_root / 'modules/nsx-core/src/nsx_debug.c'),
                    str(repo_root / 'modules/nsx-power/src/nsx_power.c'),
                    str(repo_root / ('modules/nsx-power/src/nsx_power_preflight.c' if family in APOLLO5
                                    else f'modules/nsx-power/src/{arch}/nsx_power.c')), '-o', str(binary)],
                   check=True, capture_output=True, text=True, env=host_env)
    return binary


@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, scenario) for family in FAMILIES
                          for scenario in COMMON + (('success-tempco', 'fail-tempco') if family == 'apollo4p'
                                                    else ('unsupported-tempco',))],
                         indirect=['startup_binary'])
def test_power_startup_contract(startup_binary, scenario):
    result = subprocess.run([str(startup_binary), scenario], check=True, capture_output=True, text=True, timeout=5)
    if scenario.startswith('fail-'):
        for step in range(1, int(result.stdout.strip()) + 1):
            subprocess.run([str(startup_binary), scenario, str(step)], check=True,
                           capture_output=True, text=True, timeout=5)


def test_apollo4l_power_sources_compile(repo_root, tmp_path):
    compiler = compiler_for_test('gcc')
    board = 'apollo4l_evb'
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc',
        ('nsx-core', 'nsx-interrupt', 'nsx-timer', 'nsx-power'),
        prelude=('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)',))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc' / board / 'build/compile_commands.json').read_text())
    power = [entry for entry in entries if '/modules/nsx-power/src/' in entry['file']]
    assert any(entry['file'].endswith('/apollo4/nsx_power.c') for entry in power)
    for index, entry in enumerate(power):
        command = shlex.split(entry['command'])
        command[0] = str(compiler)
        command[command.index('-o') + 1] = str(tmp_path / f'power-{index}.o')
        subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)


OWNERSHIP = ('system', 'system-minimal', 'hardware', 'minimal', 'power-first',
             'reentry', 'system-failure-1', 'system-failure-2', 'system-failure-3',
             'system-failure-4', 'minimal-failure', 'preflight', 'masked', 'masked-failure')


@pytest.mark.parametrize('api_validation', (True, False))
@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, scenario) for family in FAMILIES + APOLLO5 for scenario in OWNERSHIP],
                         indirect=['startup_binary'])
def test_startup_ownership(startup_binary, scenario, api_validation):
    subprocess.run([str(startup_binary), 'ownership-' + scenario], check=True,
                   capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('board', ['apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb'])
def test_ownership_smoke_builds(repo_root, tmp_path, board):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import OWNERSHIP
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, OWNERSHIP))


PERIPHERAL_FIELDS = ('need_usb', 'need_iom', 'need_uart', 'need_audadc', 'need_crypto',
                     'need_xtal', 'need_itm', 'need_ble', 'spotmgr_collapse')
PERIPHERAL_CASES = tuple(f'{field}-{value}' for field in PERIPHERAL_FIELDS for value in (0, 1)) + (
    'preset-all_on', 'preset-minimal', 'preset-audio')


@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, scenario) for family in FAMILIES for scenario in PERIPHERAL_CASES],
                         indirect=['startup_binary'])
def test_peripheral_policy(startup_binary, scenario):
    subprocess.run([str(startup_binary), 'policy-' + scenario], check=True,
                   capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('mode', ('keep', 'off'))
def test_usb_policy_smoke_builds(repo_root, tmp_path, mode):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import USB_KEEP, USB_OFF
    compiler = compiler_for_test('gcc')
    suite = USB_KEEP if mode == 'keep' else USB_OFF
    build_firmware(BuildRequest(repo_root, tmp_path, 'apollo4p_blue_kxr_evb', compiler.parent.parent, suite))


@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, f'{small}-{ssram}') for family in FAMILIES
                          for small in (0, 1) for ssram in (0, 1)], indirect=['startup_binary'])
def test_memory_preflight(startup_binary, scenario):
    subprocess.run([str(startup_binary), 'memory-' + scenario], check=True,
                   capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('api_validation', (False,))
@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, f'{small}-{ssram}') for family in FAMILIES
                          for small in (0, 1) for ssram in (0, 1)], indirect=['startup_binary'])
def test_memory_preflight_without_api_validation(startup_binary, scenario, api_validation):
    subprocess.run([str(startup_binary), 'memory-' + scenario], check=True,
                   capture_output=True, text=True, timeout=5)


def test_memory_preflight_smoke_builds(repo_root, tmp_path):
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import MEMORY_PREFLIGHT
    compiler = compiler_for_test('gcc')
    firmware = build_firmware(BuildRequest(repo_root, tmp_path, 'apollo4p_blue_kxr_evb',
                                          compiler.parent.parent, MEMORY_PREFLIGHT))
    output = subprocess.run([str(compiler.with_name('arm-none-eabi-nm')), '-n', str(firmware.elf)],
                            check=True, capture_output=True, text=True).stdout
    symbols = {parts[-1]: int(parts[0], 16) for line in output.splitlines()
               if len(parts := line.split()) == 3}
    assert symbols['__HeapLimit'] == 0x10060000
    assert symbols['__HeapBase'] < symbols['__HeapLimit']
    for name in ('shared_initialized', 'shared_zeroed'):
        assert 0x10060000 <= symbols[name] < 0x10160000


@pytest.mark.parametrize('api_validation', (True, False))
@pytest.mark.parametrize(('startup_binary', 'scenario'),
                         [(family, scenario) for family in APOLLO5
                          for scenario in ('power-not-set', 'power-failure', 'power-reentry')],
                         indirect=['startup_binary'])
def test_apollo5_power_ownership(startup_binary, scenario, api_validation):
    subprocess.run([str(startup_binary), 'ownership-' + scenario], check=True,
                   capture_output=True, text=True, timeout=5)


def test_apollo510l_startup_headers(repo_root, tmp_path):
    compiler = compiler_for_test('gcc')
    board = 'apollo510dL_evb'
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc',
        ('nsx-core', 'nsx-interrupt', 'nsx-timer', 'nsx-power'),
        prelude=('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)',))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc' / board / 'build/compile_commands.json').read_text())
    selected = [entry for entry in entries if entry['file'].endswith(
        ('/nsx_startup.c', '/nsx_power_preflight.c', '/nsx_system.c'))]
    assert len(selected) == 3
    for index, entry in enumerate(selected):
        command = shlex.split(entry['command'])
        command[0] = str(compiler)
        command[command.index('-o') + 1] = str(tmp_path / f'startup-{index}.o')
        subprocess.run(command, cwd=entry['directory'], check=True, capture_output=True, text=True)
