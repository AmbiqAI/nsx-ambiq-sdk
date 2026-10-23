"""Local, hardware-free acceptance gate for a candidate SDK checkout."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys

from tools.nsx_npu_compile_check import find_gcc
from tools.test_evidence import phase_status, completion

import pytest


@dataclass(frozen=True)
class Requirement:
    selector: str
    minimum: int = 1
    boards: tuple[str, ...] = ()
    parameters: tuple[str, ...] = ()
    variants: tuple[tuple[str, ...], ...] = ()
    compiler: str | None = None


PAYLOAD = tuple(Requirement(f'tests/{name}.py') for name in (
    'test_artifact_baseline', 'test_committed_intake_patches',
    'test_intake_patches_byte_for_byte', 'test_sdk_intake', 'test_intake_workflow',
    'test_cmake_contract', 'test_manifests'))
# Minimum counts deliberately describe the reviewed matrix, independently of
# the test parametrization. Removing coverage must not silently weaken the gate.
GCC = (
    Requirement('tests/test_vendor_abi.py::test_vendor_layout_from_sdk_target', 6,
                parameters=('language', 'selection'),
                variants=tuple((language, selection) for language in ('C', 'CXX')
                               for selection in ('default', 'atfe', 'conflicting-global')), compiler='gcc'),
    Requirement('tests/test_link_smoke.py::test_smoke_is_linked_firmware', 4,
                ('apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
    Requirement('tests/test_audio_buffers.py::test_audio_real_headers', 6,
                ('apollo3_evb', 'apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
    Requirement('tests/test_power_core_clock.py::test_performance_compiles_with_real_hal', 4,
                ('apollo330mP_evb', 'apollo510_evb', 'apollo510b_evb', 'apollo510dL_evb'), compiler='gcc'),
)
PROFILES = {'payload': PAYLOAD, 'gcc': PAYLOAD + GCC}


def belongs(nodeid: str, selector: str) -> bool:
    return nodeid == selector or nodeid.startswith(selector + '::') or nodeid.startswith(selector + '[')


def case_parameters(params: dict) -> dict:
    """Adapt the existing link-smoke fixture's tuple to named matrix dimensions."""
    result = dict(params)
    if 'firmware' in result:
        family, board = result.pop('firmware')
        result.update(family=family, board=board)
    return result


class Gate:
    def __init__(self, requirements: tuple[Requirement, ...]):
        self.requirements = requirements
        self.selected: list[str] = []
        self.excluded: list[str] = []
        self.boards: dict[str, str] = {}
        self.parameters: dict[str, dict] = {}
        self.phases: dict[str, dict[str, str]] = {}

    @pytest.hookimpl(trylast=True)
    def pytest_collection_modifyitems(self, config, items):
        retained, excluded = [], []
        for item in items:
            params = getattr(getattr(item, 'callspec', None), 'params', {})
            # Only compiler-bearing checks in GCC use these parameters. Do not
            # filter unrelated CMake configuration tests (they do not use the target compiler).
            required_compilers = {r.compiler for r in self.requirements
                                  if belongs(item.nodeid, r.selector) and r.compiler}
            params = case_parameters(params)
            if required_compilers and params.get('family') not in required_compilers:
                excluded.append(item)
            else:
                retained.append(item)
                self.parameters[item.nodeid] = params
                board = params.get("board")
                if board:
                    self.boards[item.nodeid] = board
        items[:] = retained
        config.hook.pytest_deselected(items=excluded)
        self.selected = [item.nodeid for item in retained]
        self.excluded = [item.nodeid for item in excluded]

    def pytest_runtest_logreport(self, report):
        self.phases.setdefault(report.nodeid, {})[report.when] = report.outcome

    def report(self, exit_code: int) -> dict:
        missing = [r.selector for r in self.requirements
                   if sum(belongs(n, r.selector) for n in self.selected) < r.minimum]
        missing_boards = {r.selector: sorted(set(r.boards) - {
            self.boards.get(n) for n in self.selected if belongs(n, r.selector)})
            for r in self.requirements if r.boards}
        missing_boards = {selector: boards for selector, boards in missing_boards.items() if boards}
        missing_variants = {}
        for requirement in self.requirements:
            observed = {tuple(self.parameters.get(n, {}).get(p) for p in requirement.parameters)
                        for n in self.selected if belongs(n, requirement.selector)}
            absent = [v for v in requirement.variants if v not in observed]
            if absent:
                missing_variants[requirement.selector] = absent
        statuses = {n: phase_status(self.phases.get(n, {})) for n in self.selected}
        incomplete = [n for n, status in statuses.items() if status != 'passed']
        status = completion(statuses.values(), session_ok=exit_code == 0,
                            requirements_met=not (missing or missing_boards or missing_variants))
        return {'status': status, 'pytest_exit_code': int(exit_code),
                'missing_requirements': missing, 'missing_boards': missing_boards, 'missing_variants': missing_variants, 'incomplete_cases': incomplete,
                'selected_count': len(self.selected), 'cases': self.phases,
                'excluded_other_compilers': self.excluded,
                'requirements': [{'selector': r.selector, 'minimum': r.minimum, 'boards': r.boards,
                                  'parameters': r.parameters, 'variants': r.variants, 'compiler': r.compiler}
                                 for r in self.requirements]}


def identity(repo: Path) -> dict:
    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()
    paths = ('modules/nsx-ambiqsuite/nsx-module.yaml',
             'modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml',
             'modules/nsx-ambiqsuite/sdk/mcu/am_sdk_version.h')
    return {'revision': git('rev-parse', 'HEAD'), 'dirty': bool(git('status', '--porcelain')),
            'sdk_identity_sha256': {p: hashlib.sha256((repo / p).read_bytes()).hexdigest() for p in paths}}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=PROFILES, required=True)
    parser.add_argument('--output', type=Path, required=True, help='New evidence directory (must not exist)')
    args = parser.parse_args()
    if os.environ.get('PYTEST_ADDOPTS', '').strip():
        parser.error('Unset PYTEST_ADDOPTS so it cannot alter required coverage')
    repo = Path(__file__).resolve().parents[1]
    if Path.cwd().resolve() != repo:
        parser.error('Run from the candidate SDK repository root')
    output = args.output.resolve()
    if output.exists():
        parser.error('--output must be a new directory; preserve prior run evidence')
    output.mkdir(parents=True)
    report_path = output / 'compatibility.json'
    report = {'schema_version': 1, 'profile': args.profile, 'status': 'incomplete'}
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    gate = Gate(PROFILES[args.profile])
    code = 2
    try:
        report['candidate'] = identity(repo)
        report['python'] = sys.version
        if args.profile == 'gcc':
            compiler = find_gcc(None)
            report['gcc'] = None if compiler is None else {
                'path': str(compiler),
                'version': subprocess.check_output([str(compiler), '--version'],
                                                 text=True, timeout=15).splitlines()[0]}

        code = pytest.main(['-q', '-o', 'addopts=', *[r.selector for r in gate.requirements],
                            f'--junitxml={output / "junit.xml"}',
                            f'--basetemp={output / "pytest-work"}'], plugins=[gate])
        report.update(gate.report(code))
    except Exception as exc:
        report['error'] = str(exc)
        raise
    finally:
        report_path.write_text(json.dumps(report, indent=2) + '\n')
        print(f'Compatibility report: {report_path} ({report["status"]})')
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
