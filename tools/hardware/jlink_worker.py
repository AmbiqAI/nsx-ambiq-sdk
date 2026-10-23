"""Isolated J-Link worker; parent enforces a wall-clock bound on vendor calls."""
import argparse
from dataclasses import asdict
import json
import signal
from pathlib import Path

from .startup import poison_sentinels
from .protocol import RESULT_SIZE, QualificationError, wait_for_result


def main() -> None:
    # The parent blocks cancellation while spawning; unblock in the child.
    signal.pthread_sigmask(signal.SIG_UNBLOCK, {signal.SIGTERM, signal.SIGINT})
    parser = argparse.ArgumentParser()
    parser.add_argument('--sentinels', nargs=3, type=int, required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--probe', type=int, required=True)
    parser.add_argument('--device', required=True)
    parser.add_argument('--speed', type=int, required=True)
    parser.add_argument('--address', type=int, required=True)
    parser.add_argument('--build-id', required=True)
    parser.add_argument('--timeout', type=float, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    # Optional dependency: regular SDK tests never import the SEGGER transport.
    import pylink
    probe = pylink.JLink()
    try:
        probe.open(serial_no=args.probe)
        probe.set_tif(pylink.enums.JLinkInterfaces.SWD)
        probe.connect(args.device, speed=args.speed)
        probe.halt()
        probe.flash_file(str(args.elf), 0)
        probe.reset(halt=True)
        # Erase previous evidence before execution; never accept retained SRAM.
        probe.memory_write8(args.address, [0] * RESULT_SIZE)
        if bytes(probe.memory_read8(args.address, RESULT_SIZE)) != bytes(RESULT_SIZE):
            raise RuntimeError('Could not clear stale results')
        seeded = poison_sentinels(probe, tuple(args.sentinels))
        (args.output / 'seeded.json').write_text(json.dumps(seeded, indent=2) + '\n')
        probe.restart()

        def read() -> bytes:
            data = bytes(probe.memory_read8(args.address, RESULT_SIZE))
            (args.output / 'last-result.bin').write_bytes(data)
            return data

        result = wait_for_result(read, args.build_id, args.timeout)
        (args.output / 'result.json').write_text(json.dumps(asdict(result), indent=2) + '\n')
        (args.output / 'unity.log').write_text(result.output)
    except QualificationError as exc:
        (args.output / 'failure.json').write_text(json.dumps({'status': 'qualification_failed', 'error': str(exc)}) + '\n')
        raise
    finally:
        probe.close()


if __name__ == '__main__':
    main()
