"""Thin manual farm commands over the existing local qualification operations."""
import argparse
from pathlib import Path
import shutil
import sys
import subprocess

from .farm_artifacts import fetch
from .farm_inputs import AVAILABLE_BOARDS, Run, export_revisions, load_dispatch, prepare
from .plan import load_plan


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='action', required=True)
    prep = sub.add_parser('prepare')
    prep.add_argument('--output', type=Path, required=True)
    download = sub.add_parser('fetch-host')
    download.add_argument('--output', type=Path, required=True)
    download.add_argument('--inputs', type=Path, required=True)
    check = sub.add_parser('check-host')
    for name in ('plan', 'baseline', 'candidate', 'gcc-root', 'output', 'host-record'):
        check.add_argument('--' + name, type=Path, required=True)
    pack = sub.add_parser('check-pack')
    pack.add_argument('--device', required=True)
    pack.add_argument('--archive', type=Path)
    pack.add_argument('--output', type=Path, required=True)
    execute = sub.add_parser('execute')
    execute.add_argument('--stage', choices=('host', 'assessment', *AVAILABLE_BOARDS), required=True)
    for name in ('inputs', 'baseline', 'candidate', 'harness', 'python', 'gcc-root', 'output'):
        execute.add_argument('--' + name, type=Path, required=True)
    execute.add_argument('--host-download', type=Path)
    args = parser.parse_args()
    try:
        if args.action == 'prepare':
            prepare(args.output)
        elif args.action == 'fetch-host':
            fetch(Run.current(), 'host', args.output)
            source = args.output / 'payload/inputs'
            _, plan = load_dispatch(source, Run.current())
            shutil.copytree(source, args.inputs)
            export_revisions(plan)
        else:
            from . import farm_run
            if args.action == 'check-host':
                farm_run.check_host(load_plan(args.plan), args.baseline, args.candidate,
                                    args.gcc_root, args.output, args.host_record)
            elif args.action == 'check-pack':
                farm_run.check_pack(args.device, args.archive, args.output)
            else:
                paths = farm_run.Paths(args.inputs.resolve(), args.baseline.resolve(), args.candidate.resolve(),
                                       args.harness.resolve(), args.python.absolute(), args.gcc_root.resolve(),
                                       args.output.resolve())
                if args.stage == 'host':
                    farm_run.host(paths)
                elif args.host_download is None:
                    raise ValueError('Board/assessment stages require the exact canonical host download')
                elif args.stage == 'assessment':
                    farm_run.aggregate(paths, args.host_download)
                else:
                    farm_run.board(paths, args.stage, args.host_download)
        return 0
    except (ValueError, OSError, KeyError, TypeError, RuntimeError, subprocess.SubprocessError) as error:
        print(f'Farm stage stopped: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
