"""Run one immutable harness against two explicit production SDK checkouts."""
import argparse
from dataclasses import dataclass
import json
from pathlib import Path

from .api import BuildRequest, BoardRequest, build_firmware, open_board
from .inventory import BOARDS
from .provenance import checkout
from .suites import Suite, SUITES, CLOCK

HARNESS_ROOT = Path(__file__).resolve().parents[2]


@dataclass(frozen=True)
class ComparisonRequest:
    baseline_sdk: Path
    candidate_sdk: Path
    output: Path
    board: str
    gcc_root: Path
    suite: Suite
    board_request: BoardRequest | None = None


def compare(request: ComparisonRequest) -> dict:
    output = request.output.resolve()
    if output.exists() and any(output.iterdir()):
        raise ValueError('Comparison output must be a new or empty directory')
    output.mkdir(parents=True, exist_ok=True)
    report = {'schema_version': 1, 'status': 'incomplete', 'suite': request.suite.name,
              'board': request.board, 'hardware': request.board_request is not None,
              'qualification': False, 'runs': {}}
    report_path = output / 'comparison.json'
    def save():
        report_path.write_text(json.dumps(report, indent=2) + '\n')
    save()
    try:
        roots = {'harness': HARNESS_ROOT, 'baseline': request.baseline_sdk.resolve(),
                 'candidate': request.candidate_sdk.resolve()}
        before = {name: checkout(root) for name, root in roots.items()}
        report['checkouts'] = before
        if any(info['dirty'] for info in before.values()):
            raise ValueError('Comparison requires clean committed harness and SDK checkouts')
        save()
        images = {}
        for name in ('baseline', 'candidate'):
            path = output / name
            report['runs'][name] = {'status': 'building', 'evidence': str(path)}
            save()
            images[name] = build_firmware(BuildRequest(HARNESS_ROOT, path, request.board,
                request.gcc_root, request.suite, sdk_root=roots[name]))
            manifest = json.loads((path / 'manifest.json').read_text())
            if manifest.get('harness') != before['harness'] or manifest.get('sdk') != before[name]:
                raise RuntimeError(f'{name} build provenance differs from initial clean checkouts')
            report['runs'][name].update(status='built', build_id=images[name].build_id)
            save()
        # Build both images before acquiring the local lease. Caller owns the lab claim.
        if request.board_request is not None:
            if images['baseline'].device != images['candidate'].device:
                raise ValueError('Baseline and candidate disagree on the physical SEGGER device')
            with open_board(request.board_request) as session:
                for name in ('baseline', 'candidate'):
                    report['runs'][name]['status'] = 'running'
                    save()
                    session.run(images[name], output / name)
                    report['runs'][name]['status'] = 'passed'
                    save()
        after = {name: checkout(root) for name, root in roots.items()}
        if before != after or any(info['dirty'] for info in after.values()):
            raise RuntimeError('A checkout changed during comparison; results invalid')
        report['status'] = 'built' if request.board_request is None else (
            'experimental_pass' if request.suite == CLOCK else 'passed')
    except BaseException as exc:
        report['status'] = 'failed'
        report['error'] = str(exc)
        raise
    finally:
        save()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline-sdk-root', type=Path, required=True)
    parser.add_argument('--candidate-sdk-root', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--board', choices=BOARDS, required=True)
    parser.add_argument('--gcc-root', type=Path, required=True)
    parser.add_argument('--suite', choices=tuple(s.name for s in SUITES), required=True)
    parser.add_argument('--clock-experimental', action='store_true')
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--build-only', action='store_true')
    mode.add_argument('--hardware', action='store_true')
    parser.add_argument('--probe')
    parser.add_argument('--hardware-timeout', type=float, default=40)
    args = parser.parse_args()
    suite = next(s for s in SUITES if s.name == args.suite)
    if suite == CLOCK and not args.clock_experimental:
        parser.error('Clock comparison requires --clock-experimental; it is not qualification')
    if args.hardware and not args.probe:
        parser.error('--hardware requires an explicit --probe and an existing shared lab claim')
    if args.build_only and args.probe:
        parser.error('--probe is only meaningful with --hardware')
    board = BoardRequest(args.probe, timeout=args.hardware_timeout) if args.hardware else None
    result = compare(ComparisonRequest(args.baseline_sdk_root, args.candidate_sdk_root,
        args.output, args.board, args.gcc_root, suite, board))
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
