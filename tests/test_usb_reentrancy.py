"""Exercise actual USB task/ISR ownership and TinyUSB callback overrides."""
import shutil
import subprocess
import pytest


@pytest.fixture(scope='module')
def usb_binary(repo_root, tmp_path_factory):
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    fixture = repo_root / 'tests/fixtures/usb-reentrancy'
    module = repo_root / 'modules/nsx-usb'
    binary = tmp_path_factory.mktemp('usb-reentrancy') / 'usb'
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(fixture), '-I' + str(module / 'includes-api'),
                    str(fixture / 'main.c'), str(module / 'src/nsx_usb.c'),
                    str(module / 'src/overrides/nsx_usb_overrides.c'), '-o', str(binary)],
                   check=True, capture_output=True, text=True)
    return binary


@pytest.mark.parametrize('transport', ('cdc', 'vendor'))
@pytest.mark.parametrize('scenario', ('task', 'timer', 'fallback', 'send', 'query', 'flush', 'connected'))
def test_usb_reentrancy(usb_binary, transport, scenario):
    subprocess.run([str(usb_binary), transport, scenario], check=True,
                   capture_output=True, text=True, timeout=5)


@pytest.mark.parametrize('board', ('apollo4p_blue_kxr_evb', 'apollo510_evb', 'apollo510b_evb'))
def test_usb_real_header_links(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.api import BuildRequest, build_firmware
    from tools.hardware.suites import Suite
    compiler = compiler_for_test('gcc')
    suite = Suite('usb-link', 'tests/test_usb_reentrancy.py::test_usb_real_header_links',
                  ('invalid_handle',), 'tests/fixtures/usb-link', 'usb_link.elf')
    firmware = build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, suite))
    symbols = subprocess.check_output([str(compiler.parent / 'arm-none-eabi-nm'),
                                       str(firmware.elf)], text=True)
    definitions = {parts[2]: parts[1] for line in symbols.splitlines()
                   if len(parts := line.split()) == 3}
    # RX hooks inherit TU_ATTR_WEAK from TinyUSB's declarations, but must
    # have actual definitions (uppercase W), not absent/unresolved weak refs.
    for callback in ('tud_cdc_rx_cb', 'tud_vendor_rx_cb'):
        assert definitions.get(callback) == 'W', f'{callback} definition missing'
    for callback in ('tud_vendor_control_xfer_cb', 'tud_mount_cb', 'tud_suspend_cb'):
        assert definitions.get(callback) == 'T', f'{callback} missing or weak'
    assert definitions.get('nsx_usb_init') == 'T'

