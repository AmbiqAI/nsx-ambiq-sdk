import pytest

from tools.hardware.runner import execute


@pytest.mark.hardware
@pytest.mark.smoke
def test_startup(startup_firmware, request):
    firmware, output = startup_firmware
    if request.config.getoption('--build-only'):
        # A build is evidence only of compilation, never a hardware pass.
        pytest.skip(f'Build succeeded; hardware execution not requested. Evidence: {output}')
    execute(firmware, request.config.getoption('--probe'), request.config.getoption('--device'),
            request.config.getoption('--hardware-timeout'), output)
