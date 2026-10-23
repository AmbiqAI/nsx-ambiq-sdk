import math
from pathlib import Path
import shutil
import uuid

import pytest

from tools.hardware.build import BOARDS, build
from tools.hardware.qualification import PROFILES, Qualification


def pytest_addoption(parser):
    group = parser.getgroup('sdk hardware')
    group.addoption('--qualification-profile', choices=tuple(PROFILES),
                    help='Require every suite in an exact board/toolchain profile')
    group.addoption('--qualification-report', type=Path,
                    help='Fresh aggregate JSON report (required with a profile)')
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
    if _QUALIFICATION in config.stash:
        config.stash[_QUALIFICATION].evidence['startup'] = str(output)
    firmware = build(repo_root, output, board, root.resolve(), config.getoption('--fault-injection'))
    return firmware, output


_QUALIFICATION = pytest.StashKey[Qualification]()


def pytest_configure(config):
    name = config.getoption('--qualification-profile')
    if not name:
        return
    profile = PROFILES[name]
    if not config.getoption('--qualification-report'):
        raise pytest.UsageError('--qualification-profile requires --qualification-report')
    config.stash[_QUALIFICATION] = Qualification(profile)
    # Clear stale success before validating the rest of this invocation.
    config.stash[_QUALIFICATION].write(config.getoption('--qualification-report'), False, False)
    board = config.getoption('--board')
    if board and board != profile.board:
        raise pytest.UsageError('--board disagrees with qualification profile')
    if config.getoption('--fault-injection') != 'none':
        raise pytest.UsageError('Fault injection is a harness check, not qualification')
    config.option.board = profile.board


@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    outcome = yield
    if _QUALIFICATION in item.config.stash:
        report = outcome.get_result()
        item.config.stash[_QUALIFICATION].record(report.nodeid, report.when, report.outcome)


def pytest_sessionfinish(session, exitstatus):
    config = session.config
    if _QUALIFICATION not in config.stash:
        return
    report = config.stash[_QUALIFICATION].write(
        config.getoption('--qualification-report'), exitstatus == 0,
        config.getoption('--hardware') and not config.getoption('--build-only'))
    if report['status'] != 'passed' and exitstatus == 0:
        session.exitstatus = pytest.ExitCode.TESTS_FAILED


def pytest_terminal_summary(terminalreporter):
    config = terminalreporter.config
    if _QUALIFICATION in config.stash:
        terminalreporter.write_line(f"Qualification report: {config.getoption('--qualification-report')}")
