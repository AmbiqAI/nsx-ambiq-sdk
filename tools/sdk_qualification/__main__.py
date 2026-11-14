"""Thin command adapters for the local qualification operations."""
import argparse
from pathlib import Path
import sys
import subprocess

from .plan import load_plan
from .policy import BOARDS
from .runtime import Config, inventory


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description='Local SDK intake/shim qualification (Linux, explicit inputs).')
    sub = root.add_subparsers(dest='action', required=True)
    plan = sub.add_parser('plan', help='Declare exact clean source commits; does not execute tests')
    for name in ('baseline', 'candidate', 'output'):
        plan.add_argument('--' + name, type=Path, required=True)
    for action in ('host', 'build', 'run-board', 'run', 'assess'):
        command = sub.add_parser(action)
        for name in ('plan', 'baseline', 'candidate', 'gcc-root', 'output'):
            command.add_argument('--' + name, type=Path, required=True)
        command.add_argument('--host-timeout', type=float, default=1200)
        command.add_argument('--hardware-timeout', type=float, default=40)
        if action in ('host', 'run'):
            command.add_argument('--decisions', type=Path, required=True,
                                 help='Reviewed intake patch decisions for these exact revisions')
        if action in ('run-board', 'run'):
            command.add_argument('--inventory', type=Path, required=True,
                                 help='Explicit board/probe/device bindings; caller holds shared lab claims')
        if action == 'run-board':
            command.add_argument('--board', choices=BOARDS, required=True)
            command.add_argument('--host-record', type=Path, required=True)
        if action == 'build':
            command.add_argument('--board', choices=BOARDS, action='append',
                                 help='Repeat to limit the build rehearsal; defaults to all five boards')
        if action == 'assess':
            command.add_argument('--record', type=Path, action='append', required=True,
                                 help='Explicit finalized execution directory; repeat as needed')
    return root


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    from . import operations
    try:
        if args.action == 'plan':
            plan = operations.create_plan(args.baseline, args.candidate, args.output)
            print(f'Plan {plan.digest}: {args.output}. Not executed; review before running.')
            return 0
        plan = load_plan(args.plan)
        config = Config(args.baseline, args.candidate, args.gcc_root, args.output,
                        args.host_timeout, args.hardware_timeout)
        if args.action == 'host':
            report = operations.host(plan, config, args.decisions)
        elif args.action == 'build':
            report = operations.build(plan, config, tuple(args.board) if args.board else BOARDS)
        elif args.action == 'run-board':
            entries = [entry for entry in inventory(args.inventory) if entry.board == args.board]
            if len(entries) != 1:
                raise ValueError('Inventory must contain the requested board exactly once')
            report = operations.run_board(plan, config, args.board, entries[0], args.host_record)
        elif args.action == 'run':
            report = operations.run(plan, config, inventory(args.inventory), args.decisions)
        else:
            from .assessment import assess
            report = assess(plan, config, tuple(args.record))
        if args.action in ('assess', 'run'):
            print(f"Qualification: {report['status']}; evidence: {config.output}")
            for error in report['errors']:
                print(error, file=sys.stderr)
            return 0 if report['qualification'] is True else 1
        print(f"Scope: {report['body']['scope_status']}; full qualification: incomplete. Evidence: {config.output}")
        return 0 if report['body']['scope_status'] == 'passed' else 1
    except (ValueError, RuntimeError, OSError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(f'Qualification stopped: {error}', file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print('Qualification cancelled; inspect retained evidence before retrying in a fresh directory.', file=sys.stderr)
        return 130


if __name__ == '__main__':
    raise SystemExit(main())
