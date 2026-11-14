"""Typed local build/session API; no pytest or board allocator dependency."""
from contextlib import contextmanager
from dataclasses import dataclass
import math
from pathlib import Path
from typing import Iterator

from .build import Firmware, build
from .protocol import Result
from .runner import execute, probe_lock
from .suites import Suite


@dataclass(frozen=True)
class BuildRequest:
    repo: Path
    output: Path
    board: str
    gcc_root: Path
    suite: Suite
    fault: str = 'none'
    sdk_root: Path | None = None


@dataclass(frozen=True)
class BoardRequest:
    probe: str
    device: str | None = None
    timeout: float = 15.0
    jlink_library: Path | None = None

    def __post_init__(self):
        if not self.probe.isdecimal():
            raise ValueError('Probe serial must contain only decimal digits')
        if not math.isfinite(self.timeout) or self.timeout <= 0:
            raise ValueError('Hardware timeout must be finite and positive')
        if self.jlink_library is not None:
            library = self.jlink_library.resolve(strict=True)
            if not library.is_file():
                raise ValueError('J-Link library must be a file')
            object.__setattr__(self, 'jlink_library', library)


def build_firmware(request: BuildRequest) -> Firmware:
    return build(request.repo.resolve(), request.output.resolve(), request.board, request.gcc_root.resolve(),
                 request.suite, request.fault,
                 sdk_root=request.sdk_root.resolve() if request.sdk_root is not None else None)


class BoardSession:
    def __init__(self, request: BoardRequest, lease_fd: int):
        self.request = request
        self._lease_fd = lease_fd
        self._open = True

    def run(self, firmware: Firmware, output: Path) -> Result:
        if not self._open:
            raise RuntimeError('Board session is closed')
        return execute(firmware, self.request.probe, self.request.device,
                       self.request.timeout, output.resolve(), lease_fd=self._lease_fd,
                       jlink_library=self.request.jlink_library)


@contextmanager
def open_board(request: BoardRequest) -> Iterator[BoardSession]:
    """Hold a local probe lease; the caller must already have a shared-lab claim."""
    with probe_lock(request.probe) as lease_fd:
        session = BoardSession(request, lease_fd)
        try:
            yield session
        finally:
            session._open = False
