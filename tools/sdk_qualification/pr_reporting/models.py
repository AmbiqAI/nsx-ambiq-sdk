"""Typed PR association; it never changes the qualification Plan or its scope."""
from dataclasses import asdict, dataclass
import re

from ..farm_inputs import Dispatch, Run
from ..models import Revisions
from ..plan import Plan


BOARDS = tuple(sorted(('apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb')))
CHECK_NAME = 'SDK farm (AP3/AP330/AP510)'
WORKFLOW = '.github/workflows/sdk-hardware-qualification.yml'
BINDING_STAGE = 'pr-binding'
BINDING_JOB = 'PR farm binding'


@dataclass(frozen=True)
class Binding:
    run: Run
    pull_request: int
    revisions: Revisions
    plan_sha256: str
    decisions_sha256: str
    boards: tuple[str, ...]
    workflow_id: int
    main_snapshot: str

    def __post_init__(self):
        if (not isinstance(self.run, Run) or not isinstance(self.revisions, Revisions)
                or type(self.pull_request) is not int or self.pull_request <= 0
                or type(self.workflow_id) is not int or self.workflow_id <= 0
                or self.boards != BOARDS
                or not re.fullmatch('[0-9a-f]{40}', self.main_snapshot)
                or any(not re.fullmatch('[0-9a-f]{64}', v)
                       for v in (self.plan_sha256, self.decisions_sha256))):
            raise ValueError('Invalid immutable PR farm binding')

    @property
    def head(self) -> str:
        return self.revisions.candidate

    @property
    def external_id(self) -> str:
        return f'nsx-sdk-farm:{self.run.run_id}:{self.run.attempt}:{self.plan_sha256}'

    def to_dict(self) -> dict:
        value = asdict(self)
        value['boards'] = list(self.boards)
        return dict(schema_version=1, **value)

    def verify_inputs(self, dispatch: Dispatch, plan: Plan):
        if (dispatch.run != self.run or dispatch.boards != self.boards
                or dispatch.plan_sha256 != self.plan_sha256
                or dispatch.decisions_sha256 != self.decisions_sha256
                or plan.digest != self.plan_sha256 or plan.revisions != self.revisions):
            raise ValueError('PR binding differs from reviewed farm inputs')


def read_binding(value: dict) -> Binding:
    if (set(value) != {'schema_version', *Binding.__dataclass_fields__}
            or type(value['schema_version']) is not int or value['schema_version'] != 1
            or not isinstance(value['boards'], list)
            or set(value['run']) != set(Run.__dataclass_fields__)
            or set(value['revisions']) != set(Revisions.__dataclass_fields__)):
        raise ValueError('Unsupported PR binding schema; rerun the complete workflow')
    fields = {k: v for k, v in value.items() if k != 'schema_version'}
    fields.update(run=Run(**value['run']), revisions=Revisions(**value['revisions']),
                  boards=tuple(value['boards']))
    return Binding(**fields)


@dataclass(frozen=True)
class Check:
    id: int
    status: str
    conclusion: str | None
    value: dict

    @classmethod
    def read(cls, value: dict, binding: Binding):
        if (type(value['id']) is not int or value['id'] <= 0
                or value['head_sha'] != binding.head or value['name'] != CHECK_NAME
                or value['external_id'] != binding.external_id
                or value['app']['slug'] != 'github-actions'
                or value['status'] not in ('queued', 'in_progress', 'completed')):
            raise ValueError('Check does not belong to this exact PR/run/attempt')
        return cls(value['id'], value['status'], value.get('conclusion'), value)


@dataclass(frozen=True)
class Publication:
    published: bool
    conclusion: str | None
    diagnostic: str
    check_id: int | None = None
