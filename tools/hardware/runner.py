"""Exclusive local probe access and bounded transport execution."""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import sys
import tempfile
from typing import Iterator

from .process import RunCancelled, run_worker
from .build import Firmware
from .protocol import decode, QualificationError


@contextmanager
def probe_lock(serial: str) -> Iterator[int]:
    if not serial.isdecimal():
        raise ValueError('Probe serial must contain only decimal digits')
    # Linux is the initial supported host; hardware-free tests remain portable.
    import fcntl
    path = Path(tempfile.gettempdir()) / f'nsx-hardware-{os.getuid()}-{int(serial)}.lock'
    with path.open('a') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise RuntimeError(f'Probe {serial} already held by another SDK test') from exc
        # Close only our descriptor on leaving this context. An inherited worker
        # descriptor retains the same flock if the parent dies without cleanup.
        yield stream.fileno()


def execute(firmware: Firmware, probe: str, device: str | None, timeout: float, output: Path) -> None:
    if device is not None and device != firmware.device:
        raise ValueError(f'Device {device} disagrees with board descriptor {firmware.device}')
    device = firmware.device
    metadata = {'schema_version': 1, 'board': firmware.board, 'probe': probe,
                'device': device, 'swd_speed_khz': firmware.speed, 'reset': 'jlink-reset-halt-then-run',
                'debugger_connected': True, 'cold_boot': False,
                'status': 'infrastructure_error'}
    try:
        with probe_lock(probe) as lease_fd:
            with (output / 'transport.log').open('w') as log:
                run_worker([
                    sys.executable, '-m', 'tools.hardware.jlink_worker',
                    '--elf', str(firmware.elf), '--probe', probe, '--device', device,
                    '--speed', str(firmware.speed), '--address', str(firmware.address), '--build-id', firmware.build_id,
                    '--timeout', str(timeout), '--output', str(output),
                    '--sentinels', *(str(a) for a in firmware.sentinels),
                ], log=log, lease_fd=lease_fd, timeout=timeout + 60,
                   cwd=Path(__file__).resolve().parents[2])
            if not (output / 'result.json').is_file():
                raise RuntimeError('Worker exited without result evidence')
            if decode((output / 'last-result.bin').read_bytes(), firmware.build_id) is None:
                raise QualificationError('Worker exited with incomplete evidence')
            metadata['status'] = 'passed'
    except RunCancelled as exc:
        metadata['status'] = 'cancelled'
        metadata['error'] = str(exc)
        raise
    except Exception as exc:
        failure = output / 'failure.json'
        if failure.is_file():
            metadata.update(json.loads(failure.read_text()))
        else:
            metadata['error'] = str(exc)
        raise RuntimeError(f"{metadata['error']}; evidence: {output}") from exc
    finally:
        (output / 'execution.json').write_text(json.dumps(metadata, indent=2) + '\n')
