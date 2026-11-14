"""Versioned plan IO and identity; declarations do not verify Git or evidence."""
from dataclasses import asdict, dataclass, field
import hashlib
import json
from pathlib import Path

from .models import CompilerCoverage, Exclusion, Gate, Revisions, Row
from .policy import BOARDS, CORE_COMPILER, CORE_GATES, PROFILE, core_rows, validate_rows


SCHEMA_VERSION = 1


@dataclass(frozen=True)
class Plan:
    revisions: Revisions
    rows: tuple[Row, ...] = field(default_factory=core_rows)
    gates: tuple[Gate, ...] = CORE_GATES
    exclusions: tuple[Exclusion, ...] = ()
    profile: str = PROFILE
    reset_transport: str = 'debugger-reset-sram'
    compilers: tuple[CompilerCoverage, ...] = (CORE_COMPILER,)

    def __post_init__(self):
        if not isinstance(self.revisions, Revisions):
            raise ValueError('Expected typed revisions')
        for values, cls in ((self.rows, Row), (self.gates, Gate), (self.exclusions, Exclusion),
                            (self.compilers, CompilerCoverage)):
            if not isinstance(values, tuple) or any(not isinstance(v, cls) for v in values):
                raise ValueError(f'Expected an immutable tuple of {cls.__name__}')
        if self.profile != PROFILE:
            raise ValueError(f'Unknown qualification profile; expected {PROFILE}')
        if self.reset_transport != 'debugger-reset-sram':
            raise ValueError('Only the existing debugger-reset/SRAM policy is declared')
        validate_rows(self.rows)
        by_id = {gate.id: gate for gate in self.gates}
        if len(by_id) != len(self.gates):
            raise ValueError('Duplicate gate identifiers')
        for required in CORE_GATES:
            if by_id.get(required.id) != required:
                raise ValueError(f'Missing or changed required intake/host gate: {required.id}')
        compilers = {item.toolchain: item for item in self.compilers}
        if len(compilers) != len(self.compilers):
            raise ValueError('Duplicate compiler coverage')
        if compilers.get('gcc') != CORE_COMPILER:
            raise ValueError('Required GCC board/build/link/ABI coverage missing or weakened')
        if any(set(item.boards) - set(BOARDS) for item in self.compilers):
            raise ValueError('Compiler board outside the declared five-board profile')
        for row in self.rows:
            for name in row.prerequisites:
                if name not in by_id or by_id[name].kind != 'prerequisite':
                    raise ValueError(f'Undeclared prerequisite: {name}')
        scopes = [item.scope for item in self.exclusions]
        if len(scopes) != len(set(scopes)):
            raise ValueError('Duplicate exclusion scopes')
        execution_ids = {name for item in self.compilers for name in item.ids}
        execution_ids.update(name + '/candidate' for item in self.compilers for name in item.ids)
        for row in self.rows:
            execution_ids.add(row.id)
            execution_ids.update(f'{row.id}/{side}' for side in row.sides)
        gate_sides = {f'{gate.id}/{"candidate" if gate.kind == "host" else "plan"}'
                      for gate in self.gates}
        if set(by_id).intersection(execution_ids | gate_sides):
            raise ValueError('Gate and execution requirement identifiers must not collide')
        required_ids = set(by_id) | gate_sides | execution_ids
        if required_ids.intersection(scopes):
            raise ValueError('Exclusions cannot replace required coverage')
        object.__setattr__(self, 'rows', tuple(sorted(self.rows, key=lambda row: row.id)))
        object.__setattr__(self, 'gates', tuple(sorted(self.gates, key=lambda gate: gate.id)))
        object.__setattr__(self, 'exclusions', tuple(sorted(self.exclusions, key=lambda item: item.scope)))
        object.__setattr__(self, 'compilers', tuple(sorted(self.compilers, key=lambda item: item.toolchain)))

    def to_dict(self) -> dict:
        value = asdict(self)
        value['schema_version'] = SCHEMA_VERSION
        value['rows'] = sorted(value['rows'], key=lambda r: (r['board'], r['toolchain'], r['suite']))
        for row in value['rows']:
            row['prerequisites'] = sorted(row['prerequisites'])
        value['gates'] = sorted(value['gates'], key=lambda g: g['id'])
        value['exclusions'] = sorted(value['exclusions'], key=lambda e: e['scope'])
        value['compilers'] = sorted(value['compilers'], key=lambda item: item['toolchain'])
        # Return the JSON representation (arrays, not internal tuples).
        return json.loads(json.dumps(value))

    @property
    def digest(self) -> str:
        encoded = json.dumps(self.to_dict(), sort_keys=True, separators=(',', ':')).encode()
        return hashlib.sha256(encoded).hexdigest()


def _fields(value: object, names: set[str], label: str) -> dict:
    if not isinstance(value, dict) or set(value) != names:
        raise ValueError(f'{label}: expected exactly fields {sorted(names)}')
    return value


def _array(value: object, label: str) -> list:
    if not isinstance(value, list):
        raise ValueError(f'{label} must be a JSON array')
    return value


def plan_from_dict(value: object) -> Plan:
    data = _fields(value, {'schema_version', 'profile', 'reset_transport', 'revisions',
                           'rows', 'gates', 'exclusions', 'compilers'}, 'Plan')
    if type(data['schema_version']) is not int or data['schema_version'] != SCHEMA_VERSION:
        raise ValueError(f'Unsupported plan schema_version; expected {SCHEMA_VERSION}')
    revisions = Revisions(**_fields(data['revisions'], {'baseline', 'candidate', 'harness'}, 'Revisions'))
    rows = []
    for item in _array(data['rows'], 'Rows'):
        row = dict(_fields(item, {'board', 'suite', 'cases', 'comparison', 'execution',
                                  'toolchain', 'reason', 'prerequisites'}, 'Row'))
        row['cases'] = tuple(_array(row['cases'], 'Cases'))
        row['prerequisites'] = tuple(_array(row['prerequisites'], 'Prerequisites'))
        rows.append(Row(**row))
    gates = tuple(Gate(**_fields(item, {'id', 'kind', 'description'}, 'Gate'))
                  for item in _array(data['gates'], 'Gates'))
    exclusions = tuple(Exclusion(**_fields(item, {'scope', 'kind', 'reason'}, 'Exclusion'))
                       for item in _array(data['exclusions'], 'Exclusions'))
    compilers = []
    for item in _array(data['compilers'], 'Compilers'):
        compiler = _fields(item, {'toolchain', 'boards', 'checks'}, 'Compiler coverage')
        compilers.append(CompilerCoverage(compiler['toolchain'],
            tuple(_array(compiler['boards'], 'Compiler boards')),
            tuple(_array(compiler['checks'], 'Compiler checks'))))
    return Plan(revisions, tuple(rows), gates, exclusions, data['profile'],
                data['reset_transport'], tuple(compilers))


def _unique_object(pairs: list[tuple[str, object]]) -> dict:
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'Duplicate JSON field: {key}')
        result[key] = value
    return result


def load_plan(path: Path) -> Plan:
    return plan_from_dict(json.loads(path.read_text(), object_pairs_hook=_unique_object))


def write_plan(plan: Plan, path: Path) -> None:
    # Do not silently replace an approved plan.
    with path.open('x') as output:
        output.write(json.dumps(plan.to_dict(), indent=2) + '\n')
