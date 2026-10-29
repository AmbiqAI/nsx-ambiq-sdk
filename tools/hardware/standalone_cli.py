"""Local build/observe commands. Flashing and power control stay separate."""
import argparse
from pathlib import Path

from .build import build
from .standalone import Artifact, BOARDS, SUITE, observe


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    prepare = commands.add_parser('build')
    prepare.add_argument('--board', choices=BOARDS, required=True)
    prepare.add_argument('--gcc-root', type=Path, required=True)
    prepare.add_argument('--output', type=Path, required=True)
    prepare.add_argument('--candidate-sdk-root', type=Path)
    capture = commands.add_parser('observe')
    capture.add_argument('--firmware', type=Path, required=True)
    capture.add_argument('--serial', type=Path, required=True)
    capture.add_argument('--output', type=Path, required=True)
    capture.add_argument('--timeout', type=float, default=120)
    args = parser.parse_args()
    try:
        if args.command == 'build':
            firmware = build(Path(__file__).resolve().parents[2], args.output.resolve(), args.board,
                             args.gcc_root.resolve(), SUITE,
                             sdk_root=args.candidate_sdk_root.resolve() if args.candidate_sdk_root else None)
            print(f'Built {firmware.elf}; no hardware execution performed')
        else:
            result = observe(Artifact.load(args.firmware), args.serial, args.output, args.timeout)
            print(f'UART evidence passed: {result.total} cases. SoC power cycling requires separate operator attestation.')
    except KeyboardInterrupt:
        parser.exit(130, 'Observation cancelled\n')
    except Exception as exc:
        parser.exit(1, f'{exc}\n')


if __name__ == '__main__':
    main()
