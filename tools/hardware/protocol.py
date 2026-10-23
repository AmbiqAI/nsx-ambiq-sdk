"""Versioned debugger-readable results; never infer success from completion alone."""
from dataclasses import dataclass
import math
import re
import struct
import time
from typing import Callable

from .qualification import STARTUP

MAGIC = 0x4E535854
VERSION = 1
TEXT_SIZE = 4096
HEADER = struct.Struct('<8I64s')
RESULT_SIZE = HEADER.size + TEXT_SIZE
CASES = STARTUP.cases


class QualificationError(RuntimeError):
    """Firmware failed, or evidence was insufficient to qualify it."""


@dataclass(frozen=True)
class Result:
    build_id: str
    total: int
    failed: int
    ignored: int
    output: str


def decode(data: bytes, build_id: str) -> Result | None:
    if len(data) != RESULT_SIZE:
        raise QualificationError('Truncated result buffer')
    magic, version, state, total, failed, ignored, length, overflow, identity = HEADER.unpack_from(data)
    if magic == 0:
        return None
    if magic != MAGIC or version != VERSION:
        raise QualificationError('Unknown result protocol')
    identity = identity.split(b'\0', 1)[0].decode('ascii', errors='strict')
    if identity != build_id:
        raise QualificationError('Firmware build identity mismatch')
    if state == 1:
        return None
    if state != 2:
        raise QualificationError(f'Invalid completion state {state}')
    if overflow or length > TEXT_SIZE:
        raise QualificationError('Firmware output overflow')
    output = data[HEADER.size:HEADER.size + length].decode('ascii', errors='strict')
    if (total, failed, ignored) != (len(CASES), 0, 0):
        raise QualificationError(f'Unity: total={total}, failed={failed}, ignored={ignored}\n{output}')
    passed = re.findall(r'^.*:\d+:([A-Za-z_]+):PASS\r?$', output, re.MULTILINE)
    if len(passed) != len(CASES) or set(passed) != set(CASES):
        raise QualificationError(f'Missing, duplicate or unexpected cases\n{output}')
    if not re.search(r'^3 Tests 0 Failures 0 Ignored[ \t]*\r?\nOK\r?$', output, re.MULTILINE):
        raise QualificationError('Missing Unity summary')
    return Result(identity, total, failed, ignored, output)


def wait_for_result(read: Callable[[], bytes], build_id: str, timeout: float,
                    clock: Callable[[], float] = time.monotonic,
                    sleep: Callable[[float], None] = time.sleep) -> Result:
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError('Timeout must be positive')
    deadline = clock() + timeout
    while clock() < deadline:
        result = decode(read(), build_id)
        if result is not None:
            return result
        sleep(0.05)
    raise QualificationError('Timed out waiting for complete firmware evidence')
