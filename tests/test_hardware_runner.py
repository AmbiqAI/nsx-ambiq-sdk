"""Hardware-free checks that deliberately reject false-positive evidence."""
import hashlib
import json
from pathlib import Path
import struct
import sys

import pytest

from tools.hardware.protocol import (CASES, HEADER, MAGIC, RESULT_SIZE, VERSION,
                                     QualificationError, decode, wait_for_result)
from tools.hardware.runner import probe_lock

IDENTITY = 'a' * 32


def packet(*, state=2, total=3, failed=0, ignored=0, identity=IDENTITY, output=None, overflow=0):
    if output is None:
        output = ''.join(f'main.cpp:42:{name}:PASS\n' for name in CASES)
        output += '\n-----------------------\n3 Tests 0 Failures 0 Ignored \nOK\n'
    text = output.encode('ascii')
    header = HEADER.pack(MAGIC, VERSION, state, total, failed, ignored,
                         len(text), overflow, identity.encode('ascii'))
    return header + text + bytes(RESULT_SIZE - len(header) - len(text))


def test_valid_result():
    assert decode(packet(), IDENTITY).total == 3


@pytest.mark.parametrize('kwargs', [
    {'failed': 1}, {'ignored': 1}, {'total': 0}, {'state': 99},
    {'identity': 'b' * 32}, {'overflow': 1}, {'output': 'OK\n'},
    {'output': ''.join(f'main.cpp:42:{CASES[0]}:PASS\n' for _ in CASES)
               + '3 Tests 0 Failures 0 Ignored \nOK\n'},
])
def test_rejects_invalid_evidence(kwargs):
    with pytest.raises(QualificationError):
        decode(packet(**kwargs), IDENTITY)


def test_truncated_and_unknown_protocol():
    with pytest.raises(QualificationError, match='Truncated'):
        decode(packet()[:-1], IDENTITY)
    data = bytearray(packet())
    struct.pack_into('<I', data, 4, 100)
    with pytest.raises(QualificationError, match='protocol'):
        decode(bytes(data), IDENTITY)


def test_pending_then_completion():
    packets = iter([bytes(RESULT_SIZE), packet(state=1), packet()])
    result = wait_for_result(lambda: next(packets), IDENTITY, 1, sleep=lambda _: None)
    assert result.build_id == IDENTITY


def test_missing_completion_times_out():
    ticks = iter([0, 0.1, 0.2, 1.1])
    with pytest.raises(QualificationError, match='Timed out'):
        wait_for_result(lambda: packet(state=1), IDENTITY, 1,
                        clock=lambda: next(ticks), sleep=lambda _: None)


def test_disconnect_is_not_pass():
    def disconnected():
        raise OSError('probe disconnected')
    with pytest.raises(OSError, match='disconnected'):
        wait_for_result(disconnected, IDENTITY, 1)


@pytest.mark.skipif(sys.platform != 'linux', reason='Initial hardware host is Linux')
def test_probe_lease_conflict_and_cleanup():
    with probe_lock('999999999'):
        with pytest.raises(RuntimeError, match='already held'):
            with probe_lock('999999999'):
                pytest.fail('Acquired a probe twice')
    with pytest.raises(ValueError):
        with probe_lock('999999999'):
            raise ValueError('simulate failed flash')
    with probe_lock('999999999'):
        pass


def test_unity_dependency_integrity(repo_root: Path):
    root = repo_root / 'tests/hardware/vendor/unity'
    record = json.loads((root / 'provenance.json').read_text())
    for name, expected in record['sha256'].items():
        assert hashlib.sha256((root / name).read_bytes()).hexdigest() == expected


@pytest.mark.skipif(sys.platform != 'linux', reason='Initial hardware host is Linux')
@pytest.mark.parametrize('behavior', ['timeout', 'disconnect', 'missing', 'incomplete'])
def test_worker_failure_releases_probe_and_never_passes(tmp_path, monkeypatch, behavior):
    import subprocess
    from tools.hardware.build import Firmware
    from tools.hardware import runner

    firmware = Firmware(tmp_path / 'test.elf', 0x10000000, IDENTITY, 'apollo3p_evb', 'test-device', 4000)

    def fake_run(*args, **kwargs):
        if behavior == 'timeout':
            raise subprocess.TimeoutExpired('worker', 1)
        if behavior == 'disconnect':
            raise subprocess.CalledProcessError(1, 'worker')
        if behavior == 'incomplete':
            (tmp_path / 'result.json').write_text('{}')
            (tmp_path / 'last-result.bin').write_bytes(packet(state=1))

    monkeypatch.setattr(runner.subprocess, 'run', fake_run)
    with pytest.raises(RuntimeError):
        runner.execute(firmware, '999999998', 'test-device', 1, tmp_path)
    assert json.loads((tmp_path / 'execution.json').read_text())['status'] != 'passed'
    with probe_lock('999999998'):
        pass


@pytest.mark.parametrize('timeout', [0, -1, float('nan'), float('inf')])
def test_timeout_must_be_bounded(timeout):
    with pytest.raises(ValueError):
        wait_for_result(lambda: packet(), IDENTITY, timeout)


def test_device_mismatch_refused_before_transport(tmp_path, monkeypatch):
    from tools.hardware.build import Firmware
    from tools.hardware import runner
    firmware = Firmware(tmp_path / 'test.elf', 0, IDENTITY, 'apollo3p_evb', 'expected', 4000)
    def forbidden(*args, **kwargs):
        pytest.fail('Touched transport for a mismatched device')
    monkeypatch.setattr(runner.subprocess, 'run', forbidden)
    with pytest.raises(ValueError, match='disagrees'):
        runner.execute(firmware, '123', 'wrong', 1, tmp_path)
