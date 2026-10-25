"""Explicit per-board qualification requirements and pytest evidence accounting."""
from dataclasses import dataclass, field
import json
from pathlib import Path

from .inventory import BOARDS
from .build import Firmware
from .protocol import Result, decode
from .suites import Suite, STARTUP, RUNTIME, TEMPCO, OWNERSHIP
from tools.test_evidence import phase_status, completion


@dataclass(frozen=True)
class Profile:
    name: str
    board: str
    toolchain: str
    suites: tuple[Suite, ...]


PROFILES = {f'{board}-gcc-startup': Profile(f'{board}-gcc-startup', board, 'gcc', (STARTUP,))
            for board in BOARDS}
PROFILES.update({f'{board}-gcc-core': Profile(f'{board}-gcc-core', board, 'gcc', (STARTUP, RUNTIME))
                 for board in BOARDS})

PROFILES['apollo4p_blue_kxr_evb-gcc-tempco-lifecycle'] = Profile(
    'apollo4p_blue_kxr_evb-gcc-tempco-lifecycle', 'apollo4p_blue_kxr_evb', 'gcc', (TEMPCO,))
for board in ('apollo3p_evb', 'apollo4p_blue_kxr_evb'):
    name = f'{board}-gcc-startup-ownership'
    PROFILES[name] = Profile(name, board, 'gcc', (OWNERSHIP,))


@dataclass
class Qualification:
    profile: Profile
    phases: dict[str, dict[str, str]] = field(default_factory=dict)
    evidence: dict[str, str] = field(default_factory=dict)
    builds: dict[str, tuple[Firmware, Path]] = field(default_factory=dict)
    completed: set[str] = field(default_factory=set)

    def require_suite(self, nodeid: str, suite: Suite) -> bool:
        expected = next((s for s in self.profile.suites if s.nodeid == nodeid), None)
        if expected is None:
            return False
        if suite != expected:
            raise ValueError(f'Suite for {nodeid} does not match qualification profile {self.profile.name}')
        return True

    def built(self, nodeid: str, suite: Suite, firmware: Firmware, output: Path) -> None:
        if not self.require_suite(nodeid, suite):
            return
        if firmware.board != self.profile.board or firmware.cases != suite.cases:
            raise ValueError('Built firmware does not match the qualification contract')
        self.evidence[suite.name] = str(output.resolve())
        self.builds[nodeid] = (firmware, output.resolve())
        self.completed.discard(nodeid)

    def executed(self, nodeid: str, result: Result) -> None:
        if nodeid not in {s.nodeid for s in self.profile.suites}:
            return
        if nodeid not in self.builds:
            raise ValueError('Qualification execution has no bound build')
        firmware, output = self.builds[nodeid]
        metadata = json.loads((output / 'execution.json').read_text())
        if metadata.get('status') != 'passed' or metadata.get('board') != firmware.board:
            raise ValueError('Qualification requires matching successful execution evidence')
        decoded = decode((output / 'last-result.bin').read_bytes(), firmware.build_id, firmware.cases)
        if decoded is None or decoded != result:
            raise ValueError('Execution result does not match complete firmware evidence')
        self.completed.add(nodeid)


    def record(self, nodeid: str, when: str, outcome: str) -> None:
        if nodeid in {suite.nodeid for suite in self.profile.suites}:
            self.phases.setdefault(nodeid, {})[when] = outcome

    def report(self, session_ok: bool, hardware: bool) -> dict:
        results = []
        for suite in self.profile.suites:
            phases = self.phases.get(suite.nodeid, {})
            status = phase_status(phases, executed=hardware and suite.nodeid in self.completed)
            results.append({'suite': suite.name, 'expected_cases': suite.cases,
                            'status': status, 'phases': phases,
                            'evidence_directory': self.evidence.get(suite.name),
                            'build_id': self.builds[suite.nodeid][0].build_id if suite.nodeid in self.builds else None})
        status = completion((r['status'] for r in results), session_ok=session_ok)
        return {'schema_version': 2, 'profile': self.profile.name,
                'board': self.profile.board, 'toolchain': self.profile.toolchain,
                'status': status, 'suites': results}

    def write(self, path: Path, session_ok: bool, hardware: bool) -> dict:
        report = self.report(session_ok, hardware)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(report, indent=2) + '\n')
        return report
