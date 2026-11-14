"""Initial requirements report only. Evidence verification belongs to the runner."""
from dataclasses import asdict, dataclass

from .plan import Plan


@dataclass(frozen=True)
class Pending:
    requirement: str
    kind: str
    side: str
    revision: str | None
    cases: tuple[str, ...] = ()


def initial_report(plan: Plan) -> dict:
    pending = [Pending(gate.id, gate.kind,
                       'candidate' if gate.kind == 'host' else 'plan',
                       plan.revisions.candidate if gate.kind == 'host' else None)
               for gate in sorted(plan.gates, key=lambda g: g.id)]
    pending.extend(Pending(row.id, row.execution, side, getattr(plan.revisions, side), row.cases)
                   for row in sorted(plan.rows, key=lambda r: r.id) for side in row.sides)
    pending.extend(Pending(name, 'compiler', 'candidate', plan.revisions.candidate)
                   for compiler in plan.compilers for name in compiler.ids)
    return {'schema_version': 1, 'profile': plan.profile, 'plan_sha256': plan.digest,
            'revisions': asdict(plan.revisions), 'status': 'incomplete',
            'qualification': False, 'executed': False,
            'requirements': [dict(asdict(item), cases=list(item.cases), status='incomplete') for item in pending],
            'exclusions': plan.to_dict()['exclusions']}
