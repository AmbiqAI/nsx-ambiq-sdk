"""Explicit per-board qualification requirements and pytest evidence accounting."""
from dataclasses import dataclass, field
import json
from pathlib import Path

from .inventory import BOARDS


@dataclass(frozen=True)
class Suite:
    name: str
    nodeid: str
    cases: tuple[str, ...]


STARTUP = Suite('startup', 'tests/hardware/test_startup.py::test_startup',
                ('initialized_data', 'zeroed_bss', 'cpp_constructor'))
SUITES = (STARTUP,)


@dataclass(frozen=True)
class Profile:
    name: str
    board: str
    toolchain: str
    suites: tuple[Suite, ...]


PROFILES = {f'{board}-gcc-startup': Profile(f'{board}-gcc-startup', board, 'gcc', SUITES)
            for board in BOARDS}


@dataclass
class Qualification:
    profile: Profile
    phases: dict[str, dict[str, str]] = field(default_factory=dict)
    evidence: dict[str, str] = field(default_factory=dict)

    def record(self, nodeid: str, when: str, outcome: str) -> None:
        if nodeid in {suite.nodeid for suite in self.profile.suites}:
            self.phases.setdefault(nodeid, {})[when] = outcome

    def report(self, session_ok: bool, hardware: bool) -> dict:
        results = []
        for suite in self.profile.suites:
            phases = self.phases.get(suite.nodeid, {})
            if 'failed' in phases.values():
                status = 'failed'
            elif hardware and all(phases.get(p) == 'passed' for p in ('setup', 'call', 'teardown')):
                status = 'passed'
            else:
                status = 'incomplete'
            results.append({'suite': suite.name, 'expected_cases': suite.cases,
                            'status': status, 'phases': phases,
                            'evidence_directory': self.evidence.get(suite.name)})
        status = ('failed' if any(r['status'] == 'failed' for r in results) else
                  'passed' if session_ok and all(r['status'] == 'passed' for r in results)
                  else 'incomplete')
        return {'schema_version': 1, 'profile': self.profile.name,
                'board': self.profile.board, 'toolchain': self.profile.toolchain,
                'status': status, 'suites': results}

    def write(self, path: Path, session_ok: bool, hardware: bool) -> dict:
        report = self.report(session_ok, hardware)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2) + '\n')
        return report
