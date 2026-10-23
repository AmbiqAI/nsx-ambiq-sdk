"""Shared completion semantics; independent of pytest and transport."""
from collections.abc import Iterable, Mapping
from typing import Literal

Status = Literal['passed', 'failed', 'incomplete']


def phase_status(phases: Mapping[str, str], *, executed: bool = True) -> Status:
    if 'failed' in phases.values():
        return 'failed'
    if executed and all(phases.get(p) == 'passed' for p in ('setup', 'call', 'teardown')):
        return 'passed'
    return 'incomplete'


def completion(statuses: Iterable[Status], *, session_ok: bool, requirements_met: bool = True) -> Status:
    values = tuple(statuses)
    if 'failed' in values:
        return 'failed'
    if values and session_ok and requirements_met and all(v == 'passed' for v in values):
        return 'passed'
    return 'incomplete'
