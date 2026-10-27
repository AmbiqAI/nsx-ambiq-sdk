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
    build_firmware(BuildRequest(repo_root, tmp_path, board, compiler.parent.parent, suite))
