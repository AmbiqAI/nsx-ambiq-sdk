"""Declaration-only qualification contracts, independent of lab inventory."""
from dataclasses import dataclass
import re
from typing import Literal


def text(value: str, label: str) -> None:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f'{label} must be a nonempty string')


def identifier(value: str, label: str) -> None:
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._:/-]*', value):
        raise ValueError(f'{label} must be a stable identifier')


def strings(values: tuple[str, ...], label: str, *, empty: bool = False) -> None:
    if not isinstance(values, tuple) or (not values and not empty):
        raise ValueError(f'{label} must be an immutable tuple')
    for value in values:
        text(value, label)
    if len(set(values)) != len(values):
        raise ValueError(f'{label} must be unique')


@dataclass(frozen=True)
class Revisions:
    baseline: str
    candidate: str
    harness: str

    def __post_init__(self):
        for name in ('baseline', 'candidate', 'harness'):
            value = getattr(self, name)
            if not isinstance(value, str) or not re.fullmatch(r'[0-9a-f]{40}', value):
                raise ValueError(f'{name} requires a full lowercase Git commit SHA')


@dataclass(frozen=True)
class Gate:
    id: str
    kind: Literal['intake', 'host', 'prerequisite']
    description: str

    def __post_init__(self):
        identifier(self.id, 'Gate id')
        if self.kind not in ('intake', 'host', 'prerequisite'):
            raise ValueError('Unknown gate kind')
        text(self.description, 'Gate description')


@dataclass(frozen=True)
class Row:
    board: str
    suite: str
    cases: tuple[str, ...]
    comparison: Literal['paired', 'candidate-only'] = 'paired'
    execution: Literal['hardware', 'build'] = 'hardware'
    toolchain: str = 'gcc'
    reason: str = ''
    prerequisites: tuple[str, ...] = ()

    def __post_init__(self):
        identifier(self.board, 'Board')
        identifier(self.suite, 'Suite')
        strings(self.cases, 'Expected cases')
        strings(self.prerequisites, 'Prerequisites', empty=True)
        if self.comparison not in ('paired', 'candidate-only'):
            raise ValueError('Unknown comparison kind')
        if self.execution not in ('hardware', 'build'):
            raise ValueError('Unknown execution kind')
        if self.toolchain != 'gcc':
            raise ValueError('Firmware execution/build backend currently supports GCC only')
        if not isinstance(self.reason, str):
            raise ValueError('Row reason must be a string')
        if self.comparison == 'candidate-only':
            text(self.reason, 'Candidate-only justification')

    @property
    def id(self) -> str:
        return f'{self.board}/{self.toolchain}/{self.suite}'

    @property
    def sides(self) -> tuple[str, ...]:
        return ('baseline', 'candidate') if self.comparison == 'paired' else ('candidate',)


@dataclass(frozen=True)
class Exclusion:
    scope: str
    kind: Literal['deferred', 'unsupported', 'conditional-not-selected']
    reason: str

    def __post_init__(self):
        identifier(self.scope, 'Exclusion scope')
        if self.kind not in ('deferred', 'unsupported', 'conditional-not-selected'):
            raise ValueError('Unknown exclusion kind')
        text(self.reason, 'Exclusion reason')
