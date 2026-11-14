"""Bounded child adapter around the existing firmware build API (never flashes)."""
import argparse
from dataclasses import asdict
import json
from pathlib import Path

from tools.hardware.api import BuildRequest, build_firmware
from tools.hardware.suites import SUITES
from .abi import enabled_suite
from .runtime import HARNESS_ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk-root', type=Path, required=True)
    parser.add_argument('--gcc-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--board', required=True)
    parser.add_argument('--suite', choices=tuple(s.name for s in SUITES), required=True)
    args = parser.parse_args()
    suite = next(s for s in SUITES if s.name == args.suite)
    image = build_firmware(BuildRequest(HARNESS_ROOT, args.output, args.board,
                                       args.gcc_root, enabled_suite(suite), sdk_root=args.sdk_root))
    value = asdict(image)
    value['elf'] = image.elf.name
    value['schema_version'] = 1
    (args.output / 'firmware.json').write_text(json.dumps(value, indent=2) + '\n')


if __name__ == '__main__':
    main()
