"""Compile the complete power module and retain public calls in a real ELF."""
from pathlib import Path
import os
import shutil
import subprocess

import pytest

from test_small_m55_memory import compiler_for_test
from tools.nsx_link_smoke import write_smoke_source
from tools.nsx_toolchain_file import write_toolchain_file

BOARDS = ('apollo510_evb', 'apollo510b_evb', 'apollo330mP_evb', 'apollo510dL_evb')


@pytest.mark.parametrize('board', BOARDS)
@pytest.mark.parametrize('family', ('gcc', 'atfe'))
def test_power_module_links(repo_root: Path, tmp_path: Path, board: str, family: str):
    compiler = compiler_for_test(family)
    env = os.environ.copy()
    env['GCC_ROOT' if family == 'gcc' else 'ATFE_ROOT'] = str(compiler.parent.parent)
    source = tmp_path / 'source'
    build = tmp_path / 'build'
    write_smoke_source(source, repo_root, board, family)
    shutil.copyfile(repo_root / 'tests/fixtures/ap510l-power/link.c', source / 'main.c')
    with (source / 'CMakeLists.txt').open('a') as stream:
        for module in ('interrupt', 'timer', 'power'):
            stream.write(f'add_subdirectory("${{NSX_ROOT}}/modules/nsx-{module}" nsx-{module})\n')
        stream.write('target_link_libraries(smoke PRIVATE nsx::power)\n')
    toolchain = tmp_path / 'toolchain.cmake'
    write_toolchain_file(toolchain, family, **{family + "_root": compiler.parent.parent})
    for command in (["cmake", '-S', str(source), '-B', str(build),
                     f'-DCMAKE_TOOLCHAIN_FILE={toolchain}'],
                    ['cmake', '--build', str(build), '-j2']):
        result = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
        assert result.returncode == 0, result.stdout + result.stderr
    nm = compiler.with_name('arm-none-eabi-nm' if family == 'gcc' else 'llvm-nm')
    result = subprocess.run([str(nm), '--defined-only', str(build / 'smoke.elf')],
                            capture_output=True, text=True, check=True)
    symbols = {line.split()[-1] for line in result.stdout.splitlines() if line.split()}
    for symbol in ('nsx_power_configure', 'nsx_power_platform_config',
                   'nsx_power_profile_dump', 'nsx_power_set_performance_mode'):
        assert symbol in symbols
    if board != 'apollo330mP_evb':
        for symbol in ('nsx_power_shutdown_peripherals', 'nsx_power_minimize_memory',
                       'nsx_power_disable_nvm', 'nsx_power_disable_caches',
                       'nsx_power_disable_debug', 'nsx_power_tristate_gpios'):
            assert symbol in symbols
