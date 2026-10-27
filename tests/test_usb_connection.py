"""Connection snapshots and buffered-transfer status contract."""
import shutil
import subprocess
import pytest


@pytest.fixture(scope='module', params=('nsx-unmount', 'dcd-unmount'))
def connection_binary(request, repo_root, tmp_path_factory):
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    binary = tmp_path_factory.mktemp('usb-connection') / request.param
    headers = repo_root / 'tests/fixtures/usb-reentrancy'
    module = repo_root / 'modules/nsx-usb'
    defines = ['-DTUSB_ADDED_FUNCTIONS=1'] if request.param == 'dcd-unmount' else []
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', *defines,
                    '-I' + str(headers), '-I' + str(module / 'includes-api'),
                    str(repo_root / 'tests/fixtures/usb-connection/main.c'),
                    str(module / 'src/nsx_usb.c'),
                    str(module / 'src/overrides/nsx_usb_overrides.c'), '-o', str(binary)],
                   check=True, capture_output=True, text=True)
    return binary


@pytest.mark.parametrize('scenario', ('invalid', 'cdc-dtr', 'vendor-lifecycle',
                                     'queued-unplug', 'queued-suspend', 'queued-open',
                                     'cdc-buffers', 'vendor-buffers', 'receive-status', 'reconnect'))
def test_usb_connection_contract(connection_binary, scenario):
    subprocess.run([str(connection_binary), scenario], check=True,
                   capture_output=True, text=True, timeout=5)
