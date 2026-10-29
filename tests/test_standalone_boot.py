"""Standalone results must come from a fresh observation window, not a debugger."""
from dataclasses import replace
import json
from pathlib import Path

import pytest

from tools.hardware.standalone import Artifact, BOARDS, CASES, SUITE, observe, parse_frame
from tools.hardware.protocol import QualificationError


ARTIFACT = Artifact(BOARDS[0], 'a' * 32, 'manifest-hash', 'elf-hash')
DEVICE = Path('/dev/serial/by-id/test-uart')


def frame(artifact=ARTIFACT):
    cases = ''.join(f'boot.cpp:{i+1}:{case}:PASS\n' for i, case in enumerate(CASES))
    return (f'NSX_BOOT_BEGIN {artifact.build_id} 0 0 0 0\n' + cases
            + '\n6 Tests 0 Failures 0 Ignored\nOK\n'
            + f'NSX_BOOT_END {artifact.build_id} 6 0 0\n').encode()


def test_partial_and_replayed_result():
    data = frame()
    assert parse_frame(data[:-1], ARTIFACT) is None
    assert parse_frame(data[-50:] + data + data, ARTIFACT).total == 6
    assert parse_frame(data.replace(b'\n', b'\r\n'), ARTIFACT).total == 6


@pytest.mark.parametrize('bad', [
    frame().replace(b'a' * 32, b'b' * 32),
    frame().replace(b'0 0 0 0\n', b'1 0 0 0\n'),
    frame().replace(b'0 0 0 0\n', b'0 1 1 0\n'),
    frame().replace(b'0 0 0 0\n', b'0 0 1 0\n'),
    frame().replace(b'0 0 0 0\n', b'0 0 0 1\n'),
    frame().replace(b'6 0 0\n', b'6 1 0\n'),
    frame().replace(b'6 0 0\n', b'6 0 1\n'),
    frame().replace(b'zeroed_bss:PASS', b'initialized_data:PASS'),
    frame().replace(b':PASS', b':IGNORE', 1),
    frame().replace(b'6 Tests', b'5 Tests'),
    frame().replace(b'cpp_constructor:PASS', b'\xff'),
    frame().replace(b'NSX_BOOT_END', b'NSX_BOOT_BEGIN malformed\nNSX_BOOT_END'),
])
def test_invalid_evidence_cannot_pass(bad):
    with pytest.raises(QualificationError):
        parse_frame(bad, ARTIFACT)


def test_ap510_mask_is_observed_without_inventing_ap4_policy():
    artifact = replace(ARTIFACT, board=BOARDS[1])
    assert parse_frame(frame(artifact).replace(b'0 0 0 0\n', b'0 1 1 0\n'), artifact).total == 6


class Clock:
    now = 0.0

    def __call__(self):
        return self.now

    def sleep(self, seconds):
        self.now += seconds


class Port:
    def __init__(self, clock, content):
        self.clock, self.content = clock, content
        self.flushed = self.closed = False

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.closed = True

    def reset_input_buffer(self):
        self.flushed = True

    def read(self, count):
        assert self.flushed
        self.clock.sleep(0.1)
        chunk, self.content = self.content[:count], self.content[count:]
        return chunk


@pytest.mark.parametrize('content,expected', [(frame(), 'passed'), (frame()[:-1], 'failed'),
                                           (frame().replace(b'a' * 32, b'b' * 32), 'failed')])
def test_capture_gates_reconnect_and_retains_failure(tmp_path, content, expected):
    clock = Clock()
    port = Port(clock, content)
    def present():
        return clock.now < 0.1 or clock.now >= 0.3
    def open_serial(device):
        assert device == DEVICE and clock.now >= 0.3
        return port
    kwargs = dict(open_serial=open_serial, present=present, clock=clock, sleep=clock.sleep, ready=lambda: None)
    if expected == 'passed':
        assert observe(ARTIFACT, DEVICE, tmp_path, 1, **kwargs).total == 6
    else:
        with pytest.raises(QualificationError):
            observe(ARTIFACT, DEVICE, tmp_path, 1, **kwargs)
    report = json.loads((tmp_path / 'observation.json').read_text())
    assert report['status'] == expected
    assert not report['power_cycle_verified']
    assert [e['event'] for e in report['events']][:4] == ['armed', 'disconnected', 'reconnected', 'input_flushed']
    assert (tmp_path / 'uart.bin').read_bytes() == content
    assert port.closed


@pytest.mark.parametrize('present', [lambda: True, lambda: False])
def test_no_disconnect_or_initial_device_cannot_pass(tmp_path, present):
    clock = Clock()
    def forbidden(_):
        pytest.fail('Must not open UART before observing disconnect and reconnect')
    with pytest.raises(QualificationError):
        observe(ARTIFACT, DEVICE, tmp_path, 0.5, open_serial=forbidden, present=present,
                clock=clock, sleep=clock.sleep, ready=lambda: None)
    assert json.loads((tmp_path / 'observation.json').read_text())['status'] == 'failed'


@pytest.mark.parametrize('exception', [OSError('disconnected'), KeyboardInterrupt()])
def test_serial_failure_and_cancel_close_and_preserve_report(tmp_path, exception):
    clock = Clock()
    port = Port(clock, b'')
    def read(_):
        raise exception
    port.read = read
    with pytest.raises(type(exception)):
        observe(ARTIFACT, DEVICE, tmp_path, 1, open_serial=lambda _: port,
                present=lambda: clock.now < 0.1 or clock.now >= 0.2,
                clock=clock, sleep=clock.sleep, ready=lambda: None)
    assert port.closed
    assert json.loads((tmp_path / 'observation.json').read_text())['status'] in ('failed', 'cancelled')


@pytest.mark.parametrize('board', BOARDS)
def test_standalone_real_build(repo_root, tmp_path, board):
    from test_small_m55_memory import compiler_for_test
    from tools.hardware.build import build
    firmware = build(repo_root, tmp_path, board, compiler_for_test('gcc').parent.parent, SUITE)
    artifact = Artifact.load(tmp_path)
    assert artifact.build_id == firmware.build_id
    firmware.elf.write_bytes(b'changed')
    with pytest.raises(ValueError, match='differs'):
        Artifact.load(tmp_path)
