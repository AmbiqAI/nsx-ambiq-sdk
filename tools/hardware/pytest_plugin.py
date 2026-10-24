import math
from pathlib import Path
import shutil
import uuid

import pytest

from dataclasses import dataclass

from tools.hardware.inventory import BOARDS
from tools.hardware.api import BuildRequest, BoardRequest, build_firmware, open_board
from tools.hardware.qualification import PROFILES, Qualification


def pytest_addoption(parser):
    group = parser.getgroup('sdk hardware')
    group.addoption('--qualification-profile', choices=tuple(PROFILES),
                    help='Require every suite in an exact board/toolchain profile')
    group.addoption('--qualification-report', type=Path,
                    help='Fresh aggregate JSON report (required with a profile)')
    group.addoption('--clock-experimental', action='store_true',
                    help='Enable clock diagnostic with provisional reference bounds; not qualification')
    group.addoption('--hardware', action='store_true', help='Explicitly enable board execution')
    group.addoption('--build-only', action='store_true', help='Build firmware without opening a probe')
    group.addoption('--board', choices=BOARDS)
    group.addoption('--fault-injection', default='none',
                    help='Suite-specific negative control (validated before building)')
    group.addoption('--probe', help='Exact J-Link serial, never auto-select')
    group.addoption('--device', help='Optional check against descriptor SEGGER device; never an override')
    group.addoption('--gcc-root', type=Path)
    group.addoption('--candidate-sdk-root', type=Path,
                    help='Production SDK checkout; tests and runner remain in this harness')
    group.addoption('--hardware-artifacts', type=Path, default=Path('artifacts/hardware'))
    group.addoption('--hardware-timeout', type=float, default=15.0)


@dataclass(frozen=True)
class RunConfig:
    board: str
    gcc_root: Path
    hardware: bool
    artifacts: Path
    fault: str
    board_request: BoardRequest | None
    sdk_root: Path | None = None


@pytest.fixture
def run_config(request):
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
    timeout = config.getoption('--hardware-timeout')
    if not math.isfinite(timeout) or timeout <= 0:
        raise pytest.UsageError('--hardware-timeout must be positive')
    root = config.getoption('--gcc-root')
    if root is None:
        cc = shutil.which('arm-none-eabi-gcc')
        if not cc:
            raise pytest.UsageError('Install Arm GCC or supply --gcc-root')
        root = Path(cc).resolve().parent.parent
    try:
        board_request = BoardRequest(config.getoption('--probe'), config.getoption('--device'), timeout) if hardware else None
    except ValueError as exc:
        raise pytest.UsageError(str(exc)) from exc
    return RunConfig(board, root.resolve(), hardware, config.getoption('--hardware-artifacts'),
                     config.getoption('--fault-injection'), board_request, config.getoption('--candidate-sdk-root'))


@pytest.fixture
def firmware(request, repo_root, run_config, suite_spec):
    output = (run_config.artifacts / run_config.board / uuid.uuid4().hex).resolve()
    qualification = request.config.stash.get(_QUALIFICATION, None)
    if qualification is not None and qualification.require_suite(request.node.nodeid, suite_spec):
        qualification.evidence[suite_spec.name] = str(output)
    image = build_firmware(BuildRequest(repo_root, output, run_config.board,
                                       run_config.gcc_root, suite_spec, run_config.fault, run_config.sdk_root))
    if qualification is not None:
        qualification.built(request.node.nodeid, suite_spec, image, output)
    return image, output



@pytest.fixture
def board_session(run_config, firmware):
    # Building happens first; build-only mode never acquires a probe lease.
    if not run_config.hardware:
        pytest.skip(f'Build succeeded; hardware execution not requested. Evidence: {firmware[1]}')
    with open_board(run_config.board_request) as session:
        yield session


@pytest.fixture
def suite_result(request, firmware, board_session):
    image, output = firmware
    result = board_session.run(image, output)
    if _QUALIFICATION in request.config.stash:
        request.config.stash[_QUALIFICATION].executed(request.node.nodeid, result)
    return result


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
