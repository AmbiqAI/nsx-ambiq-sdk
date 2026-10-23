"""Isolated J-Link worker; parent enforces a wall-clock bound on vendor calls."""
import argparse
from dataclasses import asdict
import json
import signal
from pathlib import Path

from .preparation import MemorySeed, seed_memory
from .build import Firmware
from .protocol import Result, RESULT_SIZE, STATE_OFFSET, QualificationError, wait_for_result


def capture(probe, firmware: Firmware, timeout: float, output: Path) -> Result:
    """Run one suite through a connected debugger; caller owns connection lifetime."""
    probe.halt()
    probe.reset(halt=True)
    probe.flash_file(str(firmware.elf), 0)
    probe.reset(halt=True)
    # Erase previous evidence before execution; never accept retained SRAM.
    probe.memory_write8(firmware.address, [0] * RESULT_SIZE)
    if bytes(probe.memory_read8(firmware.address, RESULT_SIZE)) != bytes(RESULT_SIZE):
        raise RuntimeError('Could not clear stale results')
    seeded = seed_memory(probe, firmware.seeds)
    (output / 'seeded.json').write_text(json.dumps(seeded, indent=2) + '\n')
    probe.restart()

    def read() -> bytes:
        # The producer publishes completion only after all payload writes.
        # Bytewise reads while it is running can otherwise tear the header.
        state = probe.memory_read32(firmware.address + STATE_OFFSET, 1)[0]
        if state in (0, 1):
            return bytes(RESULT_SIZE)
        if state != 2:
            raise QualificationError(f'Invalid completion state {state}')
        data = bytes(probe.memory_read8(firmware.address, RESULT_SIZE))
        (output / 'last-result.bin').write_bytes(data)
        return data

    result = wait_for_result(read, firmware.build_id, timeout, firmware.cases)
    (output / 'result.json').write_text(json.dumps(asdict(result), indent=2) + '\n')
    (output / 'unity.log').write_text(result.output)
    return result


def main() -> None:
    # The parent blocks cancellation while spawning; unblock in the child.
    signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGTERM, signal.SIGINT})
    parser = argparse.ArgumentParser()
    parser.add_argument('--seed', nargs=3, action='append', default=[], metavar=('SYMBOL', 'ADDRESS', 'VALUE'))
    parser.add_argument('--cases', nargs='+', required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--probe', type=int, required=True)
    parser.add_argument('--device', required=True)
    parser.add_argument('--speed', type=int, required=True)
    parser.add_argument('--address', type=int, required=True)
    parser.add_argument('--build-id', required=True)
    parser.add_argument('--timeout', type=float, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    firmware = Firmware(args.elf, args.address, args.build_id, '', args.device, args.speed,
                        tuple(args.cases), tuple(MemorySeed(n, int(a), int(v)) for n, a, v in args.seed))
    # Optional dependency: regular SDK tests never import the SEGGER transport.
    import pylink
    probe = pylink.JLink()
    try:
        probe.open(serial_no=args.probe)
        probe.set_tif(pylink.enums.JLinkInterfaces.SWD)
        probe.connect(args.device, speed=args.speed)
        capture(probe, firmware, args.timeout, args.output)
    except QualificationError as exc:
        (args.output / 'failure.json').write_text(json.dumps({'status': 'qualification_failed', 'error': str(exc)}) + '\n')
        raise
    finally:
        probe.close()


if __name__ == '__main__':
    main()
