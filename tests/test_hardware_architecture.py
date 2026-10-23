"""A second synthetic suite proves shared code has no startup-case contract."""
from dataclasses import replace
import json
import sys
from types import SimpleNamespace
from pathlib import Path

import pytest

from tools.hardware.api import BoardRequest, BuildRequest, build_firmware, open_board
from tools.hardware.build import Firmware
from tools.hardware import jlink_worker
from tools.hardware.preparation import MemorySeed
from tools.hardware.protocol import HEADER, MAGIC, VERSION, RESULT_SIZE, STATE_OFFSET, QualificationError, decode
from tools.hardware.suites import Seed, Suite
from test_small_m55_memory import compiler_for_test

SECOND = Suite('second', 'synthetic::second', ('check_1', 'check_2'),
               'tests/fixtures/hardware_second', 'second_suite.elf', (Seed('second_seed', 0xDEADBEEF),))


def packet(cases):
    text = (''.join(f'second.cpp:1:{name}:PASS\n' for name in cases) +
            f'{len(cases)} Tests 0 Failures 0 Ignored\nOK\n').encode()
    head = HEADER.pack(MAGIC, VERSION, 2, len(cases), 0, 0, len(text), 0, b'synthetic')
    return head + text + bytes(RESULT_SIZE - len(head) - len(text))


class FakeProbe:
    def __init__(self):
        self.words = {}
        self.running = False
        self.events = []

    def open(self, serial_no): self.events.append(('open', serial_no))
    def set_tif(self, interface): pass
    def connect(self, device, speed): self.events.append(('connect', device))
    def close(self): self.events.append('close')
    def halt(self): self.events.append('halt')
    def flash_file(self, elf, address): self.events.append(('flash', elf))
    def reset(self, halt): self.events.append(('reset', halt))
    def restart(self):
        self.events.append('run')
        self.running = True
    def memory_write8(self, address, values):
        self.result_address = address
        self.cleared = bytes(values)
    def memory_read8(self, address, count):
        return packet(SECOND.cases) if self.running else self.cleared
    def memory_write32(self, address, values): self.words[address] = values[0]
    def memory_read32(self, address, count):
        if address == self.result_address + STATE_OFFSET:
            return [2 if self.running else 0]
        return [self.words[address]]


@pytest.mark.parametrize('relative_output', [False, True])
@pytest.mark.parametrize('seeds', [(), (MemorySeed('second_seed', 4, 123),)])
def test_second_suite_capture_and_parent_validation(tmp_path, monkeypatch, seeds, relative_output):
    from tools.hardware import runner
    firmware = Firmware(tmp_path / 'second_suite.elf', 32, 'synthetic', 'board', 'device', 1000, SECOND.cases, seeds)
    output = tmp_path / 'evidence'
    output.mkdir()
    monkeypatch.chdir(tmp_path)
    probe = FakeProbe()
    monkeypatch.setitem(sys.modules, 'pylink', SimpleNamespace(
        JLink=lambda: probe, enums=SimpleNamespace(JLinkInterfaces=SimpleNamespace(SWD=1))))
    def worker(command, **kwargs):
        assert command[command.index('--cases') + 1:command.index('--cases') + 3] == list(SECOND.cases)
        with monkeypatch.context() as worker_context:
            worker_context.chdir(kwargs['cwd'])
            worker_context.setattr(sys, 'argv', ['worker', *command[3:]])
            jlink_worker.main()
    monkeypatch.setattr(runner, 'run_worker', worker)
    with open_board(BoardRequest('999999991', timeout=1)) as session:
        assert session.run(firmware, Path('evidence') if relative_output else output).total == 2
    assert json.loads((output / 'seeded.json').read_text()) == {s.symbol: s.value for s in seeds}
    assert probe.events[-2:] == ['run', 'close']
    with pytest.raises(RuntimeError, match='closed'):
        session.run(firmware, Path('evidence') if relative_output else output)
    with pytest.raises(QualificationError):
        decode(packet(SECOND.cases), 'synthetic', ('initialized_data', 'zeroed_bss', 'cpp_constructor'))


def test_second_suite_builds_without_startup_symbols(repo_root, tmp_path):
    compiler = compiler_for_test('gcc')
    image = build_firmware(BuildRequest(repo_root, tmp_path, 'apollo3p_evb', compiler.parent.parent, SECOND))
    assert image.elf.name == 'second_suite.elf'
    assert image.cases == SECOND.cases
    assert len(image.seeds) == 1
    assert image.seeds[0].symbol == 'second_seed'
    manifest = json.loads((tmp_path / 'manifest.json').read_text())
    assert manifest['suite'] == 'second'
    assert any(path.endswith('result_reporter.cpp') for path in manifest['inputs_sha256'])


@pytest.mark.parametrize('cases', [(), ('same', 'same'), ('bad-case',)])
def test_invalid_suite_contract(cases):
    with pytest.raises(ValueError):
        replace(SECOND, cases=cases)


@pytest.mark.parametrize('timeout', [0, float('inf'), float('nan')])
def test_invalid_session_request(timeout):
    with pytest.raises(ValueError):
        BoardRequest('123', timeout=timeout)


def test_build_refuses_cached_options(repo_root, tmp_path):
    (tmp_path / 'CMakeCache.txt').write_text('old startup omission option')
    with pytest.raises(ValueError, match='new or empty'):
        build_firmware(BuildRequest(repo_root, tmp_path, 'apollo3p_evb', Path('/unused'), SECOND))
    assert (tmp_path / 'CMakeCache.txt').read_text() == 'old startup omission option'


def test_capture_waits_for_atomic_completion(tmp_path):
    class PublishingProbe(FakeProbe):
        def __init__(self):
            super().__init__()
            self.states = iter((0, 1, 2))
            self.state = 0
        def memory_read32(self, address, count):
            if self.running and address == self.result_address + STATE_OFFSET:
                self.state = next(self.states)
                return [self.state]
            return super().memory_read32(address, count)
        def memory_read8(self, address, count):
            if self.running:
                assert self.state == 2, 'Bulk read raced the producer'
            return super().memory_read8(address, count)
    image = Firmware(tmp_path / 'test.elf', 32, 'synthetic', 'board', 'device', 1000, SECOND.cases)
    probe = PublishingProbe()
    assert jlink_worker.capture(probe, image, 1, tmp_path).total == 2
    assert probe.events[:4] == ['halt', ('reset', True), ('flash', str(image.elf)), ('reset', True)]


def test_capture_rejects_fault_completion(tmp_path):
    class FailedProbe(FakeProbe):
        def memory_read32(self, address, count):
            return [3]
    image = Firmware(tmp_path / 'test.elf', 32, 'synthetic', 'board', 'device', 1000, SECOND.cases)
    with pytest.raises(QualificationError, match='Invalid completion'):
        jlink_worker.capture(FailedProbe(), image, 1, tmp_path)
