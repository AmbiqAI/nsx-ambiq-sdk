"""Status propagation through the actual startup coordinator and family backends."""
import os
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module', params=('apollo3p', 'apollo4p', 'apollo510', 'apollo330p'))
def system_backend(request, repo_root, tmp_path_factory):
    family = request.param
    arch = family if family in ('apollo3p', 'apollo4p') else 'apollo510'
    fixture = repo_root / 'tests/fixtures/system-status'
    core = repo_root / 'modules/nsx-core'
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required for startup status tests'
    output = tmp_path_factory.mktemp(family) / 'status'
    environment = os.environ.copy()
    environment.pop('GCC_ROOT', None)
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-DNSX_DISABLE_API_VALIDATION', '-DAM_PART_' + family.upper(),
                    '-I' + str(fixture), '-I' + str(core / 'includes-api'),
                    '-I' + str(core / 'includes-internal'), str(fixture / 'main.c'),
                    str(core / 'src/nsx_system.c'),
                    str(core / 'src/nsx_debug.c'), str(core / 'src/nsx_startup.c'),
                    str(core / f'src/{arch}/nsx_system_platform.c'), '-o', str(output)],
                   check=True, capture_output=True, text=True, env=environment)
    return family, output


def run_backend(system_backend, *arguments):
    _, binary = system_backend
    subprocess.run([str(binary), *map(str, arguments)], check=True,
                   capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('scenario', ('invalid', 'success'))
def test_system_preflight_and_order(system_backend, scenario):
    run_backend(system_backend, scenario)


def test_minimal_failure_stops_setup(system_backend):
    family, _ = system_backend
    for step in (0, 3, 4) if family in ('apollo3p', 'apollo4p') else (0, 5, 6):
        run_backend(system_backend, 'minimal', step)


@pytest.mark.parametrize('system_backend', ('apollo510', 'apollo330p'), indirect=True)
@pytest.mark.parametrize('masked', (0, 1))
def test_dcu_cleanup_and_debug_failures(system_backend, masked):
    family, _ = system_backend
    for step in (0, 10, 11, 12, 13, 14, 15):
        run_backend(system_backend, 'debug', step, 0, 0, masked, 0)
    # Earlier error wins over cleanup failure, but both cleanup calls run.
    for first, second in ((12, 14), (13, 15), (14, 15)):
        run_backend(system_backend, 'debug', first, second, 0, masked, 0)
    for domains in (1, 2, 3):
        for step in (0, 12, 13):
            run_backend(system_backend, 'debug', step, 0, domains, masked, 0)
    run_backend(system_backend, 'debug', 0, 0, 0, masked, 1)
    if family == 'apollo510':
        for step in (16, 17, 18):
            run_backend(system_backend, 'debug', step, 0, 0, masked, 0)


@pytest.mark.parametrize('board', ('apollo3p_evb', 'apollo4p_blue_kxr_evb', 'apollo510b_evb', 'apollo330mP_evb'))
def test_system_status_real_header_builds(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import SYSTEM_FULL, SYSTEM_MINIMAL
    compiler = compiler_for_test('gcc')
    for suite in (SYSTEM_FULL, SYSTEM_MINIMAL):
        build_firmware(BuildRequest(repo_root, tmp_path / suite.name, board, compiler.parent.parent, suite))


@pytest.mark.parametrize('system_backend', ('apollo510',), indirect=True)
def test_manual_debug_disable(system_backend):
    for step in (0, 22, 23, 24, 25, 27):
        run_backend(system_backend, 'disable', step)
