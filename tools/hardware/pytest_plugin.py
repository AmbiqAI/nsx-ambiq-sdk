import math
from pathlib import Path
import shutil
import uuid

import pytest

from tools.hardware.build import BOARDS, build


def pytest_addoption(parser):
    group = parser.getgroup('sdk hardware')
    group.addoption('--hardware', action='store_true', help='Explicitly enable board execution')
    group.addoption('--build-only', action='store_true', help='Build firmware without opening a probe')
    group.addoption('--board', choices=BOARDS)
    group.addoption('--fault-injection', choices=('none', 'assertion', 'timeout', 'omit-data', 'omit-bss', 'omit-constructors'), default='none',
                    help='Harness self-check: deliberately produce a failing hardware run')
    group.addoption('--probe', help='Exact J-Link serial, never auto-select')
    group.addoption('--device', help='Optional check against descriptor SEGGER device; never an override')
    group.addoption('--gcc-root', type=Path)
    group.addoption('--hardware-artifacts', type=Path, default=Path('artifacts/hardware'))
    group.addoption('--hardware-timeout', type=float, default=15.0)


@pytest.fixture
def startup_firmware(request, repo_root):
    config = request.config
    hardware = config.getoption('--hardware')
    build_only = config.getoption('--build-only')
    if not hardware and not build_only:
        pytest.skip('Hardware tests are opt-in: --hardware or --build-only')
    if hardware and build_only:
        raise pytest.UsageError('Select --hardware or --build-only, not both')
    board = config.getoption('--board')
    if not board:
        raise pytest.UsageError('--board is required')
    if hardware and not config.getoption('--probe'):
        raise pytest.UsageError('--hardware requires explicit --probe')
    if not math.isfinite(config.getoption('--hardware-timeout')) or config.getoption('--hardware-timeout') <= 0:
        raise pytest.UsageError('--hardware-timeout must be positive')
    root = config.getoption('--gcc-root')
    if root is None:
        cc = shutil.which('arm-none-eabi-gcc')
        if not cc:
            raise pytest.UsageError('Install Arm GCC or supply --gcc-root')
        root = Path(cc).resolve().parent.parent
    output = (config.getoption('--hardware-artifacts') / board / uuid.uuid4().hex).resolve()
    firmware = build(repo_root, output, board, root.resolve(), config.getoption('--fault-injection'))
    return firmware, output
