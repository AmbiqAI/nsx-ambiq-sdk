"""Thin trusted-workflow adapters for optional SDK PR reporting."""
import argparse
from dataclasses import asdict
import os
from pathlib import Path
import sys

from ..farm_inputs import write
from ..io import read_json
from .operations import pending, preflight, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    start = sub.add_parser('preflight')
    start.add_argument('--pull-request', type=int, required=True)
    start.add_argument('--output', type=Path, required=True)
    queued = sub.add_parser('pending')
    queued.add_argument('--output', type=Path, required=True)
    finish = sub.add_parser('report')
    finish.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'preflight':
            binding = preflight(args.output, args.pull_request)
            print(f'Bound PR #{binding.pull_request} at {binding.head}; upload this receipt before creating its check.')
        elif args.action == 'pending':
            print(f'PR check established: {pending(args.output).id}')
        else:
            value = report(read_json(Path(os.environ['GITHUB_EVENT_PATH'])), args.output)
            write(args.output / 'publication.json', asdict(value))
            print(value.diagnostic)
            if os.environ.get('GITHUB_STEP_SUMMARY'):
                with Path(os.environ['GITHUB_STEP_SUMMARY']).open('a') as stream:
                    stream.write(f'Published PR result: {value.published}; conclusion: {value.conclusion}.\n\n{value.diagnostic}\n')
        return 0
    except (ValueError, OSError, KeyError, TypeError, RuntimeError) as error:
        print(f'PR farm reporting stopped: {type(error).__name__}: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
