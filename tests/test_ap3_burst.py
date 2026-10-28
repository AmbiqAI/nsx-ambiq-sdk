"""AP3 normal/burst contract, including HAL success with incomplete outputs."""
import shutil
import subprocess

import pytest


@pytest.fixture(scope='module', params=('apollo3', 'apollo3p', 'apollo3-fpga', 'apollo3p-fpga'))
def ap3_selector(request, repo_root, tmp_path_factory):
    family = request.param.split('-')[0]
    fixture = repo_root / 'tests/fixtures/ap3-performance'
    target = tmp_path_factory.mktemp(request.param) / 'selector'
    compiler = shutil.which('cc')
    assert compiler
    flags = ['-D' + family.upper() + '_FPGA=1'] if request.param.endswith('-fpga') else []
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', *flags,
                    '-DAM_PART_' + family.upper(), '-I' + str(fixture),
                    '-I' + str(repo_root / f'modules/nsx-ambiqsuite/sdk/mcu/{family}/hal'),
                    '-I' + str(repo_root / 'modules/nsx-core/includes-api'),
                    '-I' + str(repo_root / 'modules/nsx-core/includes-internal'),
                    '-I' + str(repo_root / 'modules/nsx-power/includes-api'),
                    str(fixture / 'main.c'),
                    str(repo_root / 'modules/nsx-core/src/apollo3p/nsx_performance.c'),
                    str(repo_root / 'modules/nsx-power/src/apollo3/nsx_power_performance.c'),
                    '-o', str(target)], check=True, capture_output=True, text=True)
    return request.param, target


CASES = [('invalid', 0), *[('normal', n) for n in (0, 1)],
         *[('preflight', n) for n in range(5)], *[('init-fault', n) for n in (1, 2, 3)],
         *[('transition-fault', n) for n in (*range(4, 10), *range(14, 20))],
         ('mixed', 0), ('mixed', 1)]


@pytest.mark.parametrize('power', (False, True), ids=('core', 'power'))
@pytest.mark.parametrize(('scenario', 'option'), CASES)
def test_ap3_performance_contract(ap3_selector, scenario, option, power):
    family, target = ap3_selector
    if family.endswith('-fpga'):
        if scenario != 'invalid':
            scenario = 'fpga'
    subprocess.run([str(target), scenario, str(int(power)), str(option)],
                   check=True, capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('board', ('apollo3_evb', 'apollo3p_evb'))
def test_ap3_performance_builds(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import AP3_PERFORMANCE
    compiler = compiler_for_test('gcc')
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, AP3_PERFORMANCE))
