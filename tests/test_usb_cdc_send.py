"""CDC sends across FIFO boundaries using the actual NSX implementation."""
import shutil
import subprocess
import pytest


@pytest.fixture(scope='module', params=('ordinary', 'apollo5-flush'))
def cdc_send_binary(request, repo_root, tmp_path_factory):
    compiler = shutil.which('cc')
    assert compiler, 'Host C compiler required'
    binary = tmp_path_factory.mktemp('cdc-send') / request.param
    headers = repo_root / 'tests/fixtures/usb-reentrancy'
    module = repo_root / 'modules/nsx-usb'
    defines = ['-DAM_PART_APOLLO5B'] if request.param == 'apollo5-flush' else []
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', *defines,
                    '-I' + str(headers), '-I' + str(module / 'includes-api'),
                    str(repo_root / 'tests/fixtures/usb-cdc-send/main.c'),
                    str(module / 'src/nsx_usb.c'), '-o', str(binary)],
                   check=True, capture_output=True, text=True)
    return binary


@pytest.mark.parametrize('scenario', ('fit', 'boundary', 'large', 'wide-length',
                                     'initial-full', 'short-write', 'partial',
                                     'stalled', 'default-timeout'))
def test_cdc_send_fifo_progress(cdc_send_binary, scenario):
    subprocess.run([str(cdc_send_binary), scenario], check=True,
                   capture_output=True, text=True, timeout=5)
