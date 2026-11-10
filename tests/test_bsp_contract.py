"""Selected BSP contracts: configure rejection and actual core symbol closure."""
import json
import shlex
import subprocess

import pytest


def configure(repo_root, tmp_path, body):
    (tmp_path / 'CMakeLists.txt').write_text(
        'cmake_minimum_required(VERSION 3.20)\nproject(bsp NONE)\n'
        f'include("{repo_root}/cmake/nsx_bsp_contract.cmake")\n'
        'set(NSX_SOC_SKEW apollo330P)\nset(NSX_BSP_CONTRACT_REQUIRED 2)\n'
        'add_library(custom INTERFACE)\n' + body)
    return subprocess.run(['cmake', '-S', str(tmp_path), '-B', str(tmp_path / 'build')],
                          capture_output=True, text=True)


@pytest.mark.parametrize('itm,uart', ((0, 0), (1, 0), (0, 1), (1, 1)))
def test_capability_target_properties(repo_root, tmp_path, itm, uart):
    result = configure(repo_root, tmp_path, f'''
nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM {itm} DEBUG_UART {uart})
nsx_validate_bsp(selected)
if(NOT selected STREQUAL "custom")
    message(FATAL_ERROR "Wrong selected BSP")
endif()
''')
    assert result.returncode == 0, result.stdout + result.stderr


@pytest.mark.parametrize('body,error', (
    ('set(NSX_SOC_SKEW apollo3)\nnsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)', 'not qualified'),
    ('nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0)', 'explicit literal'),
    ('nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM ON DEBUG_UART 0)', 'explicit literal'),
    ('nsx_register_bsp(TARGET custom CONTRACT 3 DEBUG_ITM 0 DEBUG_UART 0)', 'not qualified'),
    ('unset(NSX_BSP_CONTRACT_REQUIRED)\nnsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)', 'explicit NSX_BSP'),
    ('set(NSX_BSP_CONTRACT_VERSION 2 CACHE INTERNAL "stale")\nadd_library(nsx::bsp ALIAS custom)\nnsx_validate_bsp(selected)', 'mismatch'),
    ('nsx_validate_bsp(selected)', 'missing nsx::bsp'),
    ('nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)\nnsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)', 'already registered'),
    ('nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)\nset_property(TARGET custom PROPERTY NSX_BSP_DEBUG_UART "")\nnsx_validate_bsp(selected)', 'must be literal'),
    ('nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM 0 DEBUG_UART 0)\nset(NSX_SOC_SKEW apollo510L)\nnsx_validate_bsp(selected)', 'not qualified'),
))
def test_reject_invalid_bsp(repo_root, tmp_path, body, error):
    result = configure(repo_root, tmp_path, body)
    assert result.returncode != 0
    assert error in result.stdout + result.stderr


@pytest.mark.parametrize('board', ('apollo330mP_evb', 'apollo510_evb'))
def test_builtin_adapter_opt_in(repo_root, tmp_path, board):
    from test_cmake_contract import configure_contract_project
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc', ('nsx-core',),
                                        prelude=('set(NSX_BSP_CONTRACT_REQUIRED 2)',
                                                 'set(CMAKE_EXPORT_COMPILE_COMMANDS ON)'))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc' / board / 'build/compile_commands.json').read_text())
    debug = next(e for e in entries if e['file'].endswith('/nsx_debug.c'))
    assert '-DNSX_CORE_DEBUG_ITM=1' in debug['command']
    assert '-DNSX_CORE_DEBUG_UART=1' in debug['command']
    runtime = next(e for e in entries if e['file'].endswith('/nsx_core_portable.c'))
    assert 'NSX_CORE_DEBUG_' not in runtime['command']  # private consumer definitions


@pytest.mark.parametrize('board', ('apollo330mP_evb', 'apollo510_evb'))
@pytest.mark.parametrize('itm,uart', ((0, 0), (1, 0), (0, 1), (1, 1)))
def test_actual_platform_without_evb_header(repo_root, tmp_path, board, itm, uart):
    """At -O0 inspect every undefined symbol; section GC/archive extraction cannot hide it."""
    from test_cmake_contract import configure_contract_project
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    result = configure_contract_project(repo_root, tmp_path, board, 'gcc', ('nsx-core',),
                                        prelude=('set(CMAKE_EXPORT_COMPILE_COMMANDS ON)',))
    assert result.returncode == 0, result.stdout
    entries = json.loads((tmp_path / 'gcc' / board / 'build/compile_commands.json').read_text())
    entry = next(e for e in entries if e['file'].endswith('/apollo510/nsx_system_platform.c'))
    include = tmp_path / 'bsp'
    include.mkdir()
    header = '#include "am_mcu_apollo.h"\nvoid am_bsp_low_power_init(void);\n'
    if itm:
        header += '#define AM_BSP_GPIO_ITM_SWO 28\nextern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ITM_SWO;\n'
    if uart:
        header += 'int32_t am_bsp_uart_printf_enable(void);\nvoid am_bsp_uart_printf_disable(void);\n'
    (include / 'am_bsp.h').write_text(header)
    command = shlex.split(entry['command'])
    command[0] = str(compiler)
    command = [arg for arg in command if not (arg.startswith('-I') and '/boards/' in arg)]
    obj = tmp_path / 'platform.o'
    command[command.index('-o') + 1] = str(obj)
    command += [f'-I{include}', '-O0', '-Werror=implicit-function-declaration',
                '-DNSX_CORE_BSP_CONTRACT=2', f'-DNSX_CORE_DEBUG_ITM={itm}', f'-DNSX_CORE_DEBUG_UART={uart}']
    built = subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True)
    assert built.returncode == 0, built.stderr
    nm = compiler.with_name('arm-none-eabi-nm')
    undefined = subprocess.check_output([str(nm), '-u', str(obj)], text=True)
    bsp_symbols = {line.split()[-1] for line in undefined.splitlines() if 'am_bsp' in line.lower()}
    expected = {'am_bsp_low_power_init'}
    if itm:
        expected.add('g_AM_BSP_GPIO_ITM_SWO')
    if uart:
        expected.update(('am_bsp_uart_printf_enable', 'am_bsp_uart_printf_disable'))
    assert bsp_symbols == expected
    if itm:
        (include / 'am_bsp.h').write_text(header.replace('#define AM_BSP_GPIO_ITM_SWO 28\n', ''))
        failed = subprocess.run(command, cwd=entry['directory'], capture_output=True, text=True)
        assert failed.returncode != 0 and 'AM_BSP_GPIO_ITM_SWO' in failed.stderr


@pytest.mark.parametrize('soc', ('apollo330P', 'apollo510'))
@pytest.mark.parametrize('itm,uart', ((0, 0), (1, 0), (0, 1), (1, 1)))
def test_custom_provider_core_build(repo_root, tmp_path, soc, itm, uart):
    """Force core objects into an ARM image without EVB headers/archive or section GC."""
    from test_small_m55_memory import compiler_for_test
    compiler = compiler_for_test('gcc')
    header = '#include "am_mcu_apollo.h"\nvoid am_bsp_low_power_init(void);\n'
    source = '#include "am_bsp.h"\nvoid am_bsp_low_power_init(void) {}\n'
    # Link-only provider: no hardware execution or claim of real initialization.
    if itm:
        header += '#define AM_BSP_GPIO_ITM_SWO 28\nextern am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ITM_SWO;\n'
        source += 'am_hal_gpio_pincfg_t g_AM_BSP_GPIO_ITM_SWO = {0};\n'
    if uart:
        header += 'int32_t am_bsp_uart_printf_enable(void);\nvoid am_bsp_uart_printf_disable(void);\n'
        source += 'int32_t am_bsp_uart_printf_enable(void) {return 0;}\nvoid am_bsp_uart_printf_disable(void) {}\n'
    (tmp_path / 'am_bsp.h').write_text(header)
    (tmp_path / 'bsp.c').write_text(source)
    (tmp_path / 'main.c').write_text('int main(void) {return 0;}\n')
    (tmp_path / 'CMakeLists.txt').write_text(f'''
cmake_minimum_required(VERSION 3.20)
set(CMAKE_C_COMPILER "{compiler}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
project(custom_bsp C)
include(GNUInstallDirs)
set(NSX_ROOT "{repo_root}")
set(NSX_CMAKE_DIR "{repo_root}/cmake")
set(NSX_TOOLCHAIN_FAMILY gcc)
set(NSX_SDK_PROVIDER ambiqsuite)
set(NSX_AMBIQSUITE_ROOT "{repo_root}/modules/nsx-ambiqsuite/sdk")
include("{repo_root}/cmake/socs/{soc}.cmake")
add_library(board_flags INTERFACE)
add_library(nsx::board_flags ALIAS board_flags)
target_link_libraries(board_flags INTERFACE ${{NSX_SOC_FLAGS_TARGET}})
target_compile_definitions(board_flags INTERFACE STACK_SIZE=4096)
add_subdirectory("{repo_root}/modules/nsx-cmsis-core" cmsis)
add_subdirectory("{repo_root}/modules/nsx-ambiq-hal" hal)
include("{repo_root}/cmake/nsx_bsp_contract.cmake")
set(NSX_BSP_CONTRACT_REQUIRED 2)
add_library(custom STATIC bsp.c)
target_link_libraries(custom PUBLIC nsx::board_flags nsx_ambiq_hal)
target_include_directories(custom PUBLIC "{tmp_path}")
nsx_register_bsp(TARGET custom CONTRACT 2 DEBUG_ITM {itm} DEBUG_UART {uart})
add_subdirectory("{repo_root}/modules/nsx-soc-hal" soc)
add_subdirectory("{repo_root}/modules/nsx-core" core)
add_subdirectory("{repo_root}/modules/nsx-cmsis-startup" startup)
add_executable(image main.c)
target_link_libraries(image PRIVATE nsx::cmsis_startup
    -Wl,--whole-archive nsx::system nsx::runtime -Wl,--no-whole-archive)
target_link_options(image PRIVATE -nostartfiles "LINKER:-T,${{NSX_LINKER_SCRIPT}}"
    LINKER:-u,Reset_Handler LINKER:--no-gc-sections)
''')
    build = tmp_path / 'build'
    result = subprocess.run(['cmake', '-S', str(tmp_path), '-B', str(build)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    result = subprocess.run(['cmake', '--build', str(build), '-j2'], capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
    objects = sorted((build / 'core').rglob('*.o'))
    assert objects
    undefined = subprocess.check_output([str(compiler.with_name('arm-none-eabi-nm')), '-u',
                                         *map(str, objects)], text=True)
    bsp_symbols = {line.split()[-1] for line in undefined.splitlines() if 'am_bsp' in line.lower()}
    expected = {'am_bsp_low_power_init'}
    if itm:
        expected.add('g_AM_BSP_GPIO_ITM_SWO')
    if uart:
        expected.update(('am_bsp_uart_printf_enable', 'am_bsp_uart_printf_disable'))
    assert bsp_symbols == expected
    assert (build / 'image').is_file()
    # A required provider entrypoint must not be hidden by archive extraction.
    (tmp_path / 'bsp.c').write_text(source.replace('void am_bsp_low_power_init(void) {}', ''))
    failed = subprocess.run(['cmake', '--build', str(build), '-j2'], capture_output=True, text=True)
    assert failed.returncode != 0 and 'am_bsp_low_power_init' in failed.stderr
