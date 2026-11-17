"""Explicit farm scheduling inputs; the five-board Plan still owns acceptance."""
from dataclasses import asdict, dataclass, field
import json
import os
from pathlib import Path
import re

from .io import identity, read_json, sha256
from .plan import Plan, load_plan
from .policy import BOARDS


AVAILABLE_BOARDS = tuple(sorted(BOARDS))


def write(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value, indent=2) + '\n')


@dataclass(frozen=True)
class Run:
    repository: str
    run_id: str
    attempt: str
    driver: str
    server: str

    def __post_init__(self):
        if (not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', self.repository)
                or any(not re.fullmatch('[1-9][0-9]*', v) for v in (self.run_id, self.attempt))
                or not re.fullmatch('[0-9a-f]{40}', self.driver)
                or self.server != 'https://github.com'):
            raise ValueError('Expected explicit GitHub repository/run/attempt/driver identities')

    @classmethod
    def current(cls):
        return cls(*(os.environ[name] for name in (
            'GITHUB_REPOSITORY', 'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_SHA', 'GITHUB_SERVER_URL')))

    @property
    def url(self):
        return f'{self.server}/{self.repository}/actions/runs/{self.run_id}/attempts/{self.attempt}'

    def artifact_name(self, stage: str) -> str:
        if stage not in ('host', 'assessment', 'pr-binding', *AVAILABLE_BOARDS):
            raise ValueError('Unknown farm artifact stage')
        return f'sdk-farm-{self.run_id}-{self.attempt}-{stage}'


@dataclass(frozen=True)
class Dispatch:
    run: Run
    boards: tuple[str, ...]
    plan_sha256: str
    decisions_sha256: str
    device_pack_archive: str

    def __post_init__(self):
        if (not self.boards or len(set(self.boards)) != len(self.boards)
                or any(board not in AVAILABLE_BOARDS for board in self.boards)
                or self.boards != tuple(sorted(self.boards))):
            raise ValueError('Select a unique explicit subset of the five-board qualification profile')
        if any(not re.fullmatch('[0-9a-f]{64}', v) for v in (self.plan_sha256, self.decisions_sha256)):
            raise ValueError('Dispatch requires plan/decision content identities')
        if self.device_pack_archive and not Path(self.device_pack_archive).is_absolute():
            raise ValueError('Device-pack archive must be an explicit absolute farm path')
        if 'apollo330mP_evb' in self.boards and not self.device_pack_archive:
            raise ValueError('AP330 requires an owner-provisioned pinned device-pack archive path')

    @property
    def digest(self):
        return identity(self.to_dict())

    def to_dict(self):
        return json.loads(json.dumps(dict(schema_version=1, **asdict(self))))


def load_dispatch(inputs: Path, run: Run) -> tuple[Dispatch, Plan]:
    value = read_json(inputs / 'dispatch.json')
    if set(value) != {'schema_version', 'run', 'boards', 'plan_sha256', 'decisions_sha256', 'device_pack_archive'}:
        raise ValueError('Invalid farm dispatch schema')
    if type(value['schema_version']) is not int or value['schema_version'] != 1:
        raise ValueError('Unsupported farm dispatch schema')
    if not isinstance(value['boards'], list) or not all(isinstance(b, str) for b in value['boards']):
        raise ValueError('Dispatch boards must be a string array')
    dispatch = Dispatch(Run(**value['run']), tuple(value['boards']), value['plan_sha256'],
                        value['decisions_sha256'], value['device_pack_archive'])
    if dispatch.run != run:
        raise ValueError('Artifact belongs to another run/attempt/driver; rerun all jobs')
    plan = load_plan(inputs / 'plan.json')
    if plan.digest != dispatch.plan_sha256 or sha256(inputs / 'decisions.json') != dispatch.decisions_sha256:
        raise ValueError('Reviewed farm input contents changed')
    return dispatch, plan


def prepare(output: Path) -> Dispatch:
    output.mkdir(parents=True, exist_ok=False)
    for name, variable in (('plan', 'SDK_PLAN_JSON'), ('decisions', 'SDK_DECISIONS_JSON')):
        (output / f'{name}.json').write_text(os.environ[variable])
    plan = load_plan(output / 'plan.json')
    read_json(output / 'decisions.json')
    boards = json.loads(os.environ.get('SDK_BOARDS_JSON', '["apollo510_evb"]'))
    if not isinstance(boards, list) or not all(isinstance(b, str) for b in boards):
        raise ValueError('Selected boards must be a JSON string array')
    dispatch = Dispatch(Run.current(), tuple(sorted(boards)), plan.digest,
                        sha256(output / 'decisions.json'), os.environ.get('SDK_DEVICE_PACK_ARCHIVE', ''))
    write(output / 'dispatch.json', dispatch.to_dict())
    export_revisions(plan)
    with Path(os.environ['GITHUB_OUTPUT']).open('a') as stream:
        for name, sha in asdict(plan.revisions).items():
            stream.write(f'{name}={sha}\n')
        stream.write('matrix=' + json.dumps({'board': dispatch.boards}, separators=(',', ':')) + '\n')
    return dispatch


def export_revisions(plan: Plan):
    if os.environ.get('GITHUB_ENV'):
        with Path(os.environ['GITHUB_ENV']).open('a') as stream:
            for name, sha in asdict(plan.revisions).items():
                stream.write(f'SDK_{name.upper()}={sha}\n')


@dataclass
class State:
    run: Run
    stage: str
    dispatch_sha256: str
    board: str | None = None
    status: str = 'incomplete'
    qualification: bool = False
    global_status: str = 'incomplete'
    board_state: str = 'not-accessed'
    lease: str = 'not-acquired'
    error: str | None = None
    holder: str | None = None
    reason: str | None = None
    acquired: dict | None = None
    binding: dict | None = None
    debugger: dict | None = None
    host_artifact: dict | None = None
    host_record_sha256: str | None = None
    elapsed_seconds: float = 0
    timings: dict[str, float] = field(default_factory=dict)

    def save(self, output: Path):
        write(output / 'state.json', dict(schema_version=1, **asdict(self)))


def passed_state(output: Path, dispatch: Dispatch, stage: str, board: str | None = None) -> dict:
    value = read_json(output / 'state.json')
    if (set(value) != {'schema_version', *State.__dataclass_fields__}
            or type(value['schema_version']) is not int or value['schema_version'] != 1
            or value['run'] != asdict(dispatch.run) or value['dispatch_sha256'] != dispatch.digest
            or value['stage'] != stage or value['board'] != board
            or value['status'] != 'passed' or value['error'] is not None
            or value['qualification'] is not False or value['global_status'] != 'incomplete'):
        raise ValueError('Farm stage did not complete for the selected run/attempt/dispatch')
    if stage == 'board' and (value['lease'] != 'released' or value['board_state'] != 'tests-completed'):
        raise ValueError('Board lease release/cleanup is not established')
    if stage == 'board':
        expected_holder = f'sdk-{dispatch.run.run_id}-{dispatch.run.attempt}-{board}'
        expected_reason = f'UNKNOWN/recovery-required if interrupted: {dispatch.run.url} board={board}'
        acquired, binding = value['acquired'], value['binding']
        if (not isinstance(acquired, dict) or not isinstance(binding, dict)
                or binding.get('board') != board or value['holder'] != expected_holder
                or value['reason'] != expected_reason or acquired.get('holder') != expected_holder
                or acquired.get('reason') != expected_reason or type(acquired.get('pid')) is not int
                or acquired['pid'] <= 0 or not acquired.get('user')):
            raise ValueError('Board release evidence lacks its acquired reservation identity')
    return value
