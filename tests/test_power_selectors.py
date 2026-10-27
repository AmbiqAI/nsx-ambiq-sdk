"""Run production AP3/AP4 selectors through the public NSX API on the host."""
import shutil
import subprocess

import pytest


COMMON = ('low', 'high', 'max', 'repeat', 'invalid', 'invalid-after-init',
          'enable-fails', 'disable-fails')
AP3 = ('init-fails', 'init-no-output', 'unavailable')


@pytest.fixture
def selector_binary(request, repo_root, tmp_path):
    family = request.param
    compiler = shutil.which('cc')
    assert compiler, 'A host C compiler is required for power-selector contract tests'
    arch = 'apollo3' if family.startswith('apollo3') else 'apollo4'
    fixture = repo_root / 'tests/fixtures/power-selector'
    binary = tmp_path / 'selector'
    command = [compiler, '-std=c11', '-Wall', '-Wextra',
               '-I', str(fixture), '-I', str(repo_root / 'modules/nsx-power/includes-api'),
               '-I', str(repo_root / 'modules/nsx-core/includes-api')]
    if arch == 'apollo3':
        command += ['-DTEST_APOLLO3', '-I', str(repo_root / f'modules/nsx-ambiqsuite/sdk/mcu/{family}/hal')]
    if arch == 'apollo4':
        command += ['-DAM_PART_APOLLO4P',
                    '-I', str(repo_root / 'modules/nsx-core/includes-internal'),
                    str(repo_root / 'modules/nsx-core/src/nsx_performance.c')]
    if family == 'apollo4-fpga':
        command += ['-DAPOLLO4_FPGA=48']
    command += [str(fixture / 'main.c'),
                str(repo_root / f'modules/nsx-power/src/{arch}/nsx_power_performance.c'), '-o', str(binary)]
    subprocess.run(command, check=True, capture_output=True, text=True)
    return family, binary


@pytest.mark.parametrize(('selector_binary', 'scenario'),
                         [(family, scenario) for family in ('apollo3', 'apollo3p', 'apollo4', 'apollo4-fpga')
                          for scenario in (COMMON + ('wrong-hardware', 'missing-ack') if family.startswith('apollo4') else COMMON + AP3)],
                         indirect=['selector_binary'])
def test_family_performance_contract(selector_binary, scenario):
    family, binary = selector_binary
    subprocess.run([str(binary), scenario], check=True, capture_output=True, text=True)
