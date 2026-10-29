"""Bounded UART observation, separate from debugger qualification."""
from dataclasses import asdict, dataclass
import json
import math
from pathlib import Path
import re
import time

from .build import digest
from .protocol import QualificationError, Result, TEXT_SIZE, validate_result
from .suites import Suite, no_options

BOARDS = ('apollo4p_blue_kxr_evb', 'apollo510b_evb')
CASES = ('initialized_data', 'zeroed_bss', 'cpp_constructor',
         'debugger_detached', 'system_startup', 'interrupt_mask')


def configure(repo, output, board, fault, sdk_root=None):
    if board not in BOARDS:
        raise ValueError('Standalone boot fixture supports AP4 KXR and AP510B')
    return no_options(repo, output, board, fault, sdk_root)


# Deliberately absent from debugger SUITES/PROFILES and pytest hardware collection.
SUITE = Suite('standalone-boot', 'standalone-boot', CASES,
              'tests/hardware/standalone', 'standalone_boot.elf', configure=configure)


@dataclass(frozen=True)
class Artifact:
    board: str
    build_id: str
    manifest_sha256: str
    elf_sha256: str

    @classmethod
    def load(cls, directory: Path):
        path = directory / 'manifest.json'
        manifest = json.loads(path.read_text())
        if (manifest.get('schema_version') != 3 or manifest.get('suite') != SUITE.name
                or manifest.get('board') not in BOARDS
                or manifest.get('expected_cases') != list(CASES)
                or manifest.get('toolchain') != 'gcc'
                or manifest.get('fault_injection') != 'none'
                or not re.fullmatch('[0-9a-f]{32}', manifest.get('build_id', ''))):
            raise ValueError('Expected a standalone-boot build manifest')
        elf_hash = digest(directory / SUITE.elf_name)
        if elf_hash != manifest.get('elf_sha256'):
            raise ValueError('Firmware ELF differs from build manifest')
        return cls(manifest['board'], manifest['build_id'], digest(path), elf_hash)


def parse_frame(data: bytes, artifact: Artifact) -> Result | None:
    """Ignore a leading partial replay; validate the first complete framed result."""
    start = re.search(rb'(?m)^NSX_BOOT_BEGIN ', data)
    if start is None:
        return None
    frame = data[start.start():]
    end = re.search(rb'(?m)^NSX_BOOT_END [^\n]*\n', frame)
    if end is None:
        if len(frame) > TEXT_SIZE + 512:
            raise QualificationError('Oversized or incomplete UART frame')
        return None
    frame = frame[:end.end()]
    try:
        text = frame.decode('ascii').replace('\r\n', '\n')
    except UnicodeDecodeError as exc:
        raise QualificationError('Non-ASCII UART frame') from exc
    lines = text.splitlines(keepends=True)
    header = re.fullmatch(r'NSX_BOOT_BEGIN ([0-9a-f]{32}) ([01]) ([01]) ([01]) ([01])\n', lines[0])
    trailer = re.fullmatch(r'NSX_BOOT_END ([0-9a-f]{32}) (\d+) (\d+) (\d+)\n', lines[-1])
    if header is None or trailer is None or 'NSX_BOOT_BEGIN' in ''.join(lines[1:]):
        raise QualificationError('Malformed UART frame')
    identity, debug, entry, after, overflow = header.groups()
    if identity != artifact.build_id or trailer[1] != identity:
        raise QualificationError('Firmware build identity mismatch')
    if debug != '0' or overflow != '0' or entry != after:
        raise QualificationError('Debugger, transcript overflow or interrupt-mask failure')
    if artifact.board == 'apollo4p_blue_kxr_evb' and entry != '0':
        raise QualificationError('AP4 entered main with interrupts masked')
    output = ''.join(lines[1:-1])
    if len(output) > TEXT_SIZE:
        raise QualificationError('Oversized Unity transcript')
    return validate_result(Result(identity, *map(int, trailer.groups()[1:]), output), CASES)


def serial_port(device: Path):
    import serial  # Optional hardware dependency; never imported by host validation.
    return serial.Serial(str(device), baudrate=115200, timeout=0.1, exclusive=True)


def observe(artifact: Artifact, device: Path, output: Path, timeout: float = 120,
            *, open_serial=serial_port, present=None, clock=time.monotonic,
            sleep=time.sleep, ready=lambda: print('READY: cycle board power now', flush=True)) -> Result:
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError('Observation timeout must be finite and positive')
    if device.parent != Path('/dev/serial/by-id'):
        raise ValueError('Use a stable /dev/serial/by-id device path')
    if output.exists() and any(output.iterdir()):
        raise ValueError('Observation output must be new or empty')
    output.mkdir(parents=True, exist_ok=True)
    present = present or device.exists
    started = clock()
    deadline = started + timeout
    events = []
    metadata = {'schema_version': 1, 'artifact': asdict(artifact), 'serial_device': str(device),
                'transport': 'uart-after-usb-reconnect', 'status': 'incomplete',
                'power_cycle_verified': False, 'events': events}

    def event(name):
        events.append({'event': name, 'elapsed_seconds': clock() - started})

    def wait_for(expected):
        while clock() < deadline:
            if bool(present()) == expected:
                return
            sleep(0.05)
        raise QualificationError('Timed out waiting for UART disconnect/reconnect')

    try:
        with (output / 'uart.bin').open('wb') as raw:
            if not present():
                raise QualificationError('UART must be present before arming observation')
            event('armed')
            ready()
            wait_for(False)
            event('disconnected')
            wait_for(True)
            event('reconnected')
            with open_serial(device) as port:
                port.reset_input_buffer()
                event('input_flushed')
                data = bytearray()
                while clock() < deadline:
                    chunk = port.read(1024)
                    raw.write(chunk)
                    raw.flush()
                    data.extend(chunk)
                    if len(data) > 65536:
                        raise QualificationError('UART observation exceeded byte limit')
                    result = parse_frame(bytes(data), artifact)
                    if result is not None:
                        event('result_validated')
                        metadata.update(status='passed', result=asdict(result))
                        return result
                raise QualificationError('Timed out waiting for complete UART result')
    except BaseException as exc:
        metadata.update(status='cancelled' if isinstance(exc, KeyboardInterrupt) else 'failed', error=str(exc))
        raise
    finally:
        (output / 'observation.json').write_text(json.dumps(metadata, indent=2) + '\n')
