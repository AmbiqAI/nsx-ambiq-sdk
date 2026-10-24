"""Link the complete production power module; no probes or mocked HAL headers."""
from dataclasses import replace
import json
import subprocess

import pytest

from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.suites import STARTUP, no_options
from test_small_m55_memory import compiler_for_test


@pytest.mark.parametrize('board', [
    'apollo3_evb', 'apollo3p_evb', 'apollo4p_evb',
    'apollo4p_blue_kbr_evb', 'apollo4p_blue_kxr_evb',
    'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb',
])
def test_power_module_links_real_sdk(repo_root, tmp_path, board):
    compiler = compiler_for_test('gcc')
    suite = replace(STARTUP, name='power-link', source_dir='tests/fixtures/power-link',
                    elf_name='power_link.elf', configure=no_options)
    image = build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, suite))
    listing = subprocess.check_output([str(compiler.parent / 'arm-none-eabi-nm'), str(image.elf)], text=True)
    symbols = {parts[2]: parts[1] for line in listing.splitlines() if len(parts := line.split()) == 3}
    for symbol in ('nsx_power_monitor_init', 'nsx_power_monitor_set_state',
                   'nsx_power_configure', 'nsx_power_set_performance_mode',
                   'nsx_power_profile_dump', 'am_hal_gpio_pinconfig'):
        assert symbols[symbol] == 'T', symbol
    config = ('g_AM_HAL_GPIO_OUTPUT' if board in ('apollo3_evb', 'apollo3p_evb')
              else 'am_hal_gpio_pincfg_output')
    assert symbols[config] in ('R', 'T')  # Linker scripts may merge rodata into text.
    manifest = json.loads((tmp_path / 'manifest.json').read_text())
    assert any(path.endswith('/nsx-power/src/nsx_power_monitor.c')
               for path in manifest['inputs_sha256'])
