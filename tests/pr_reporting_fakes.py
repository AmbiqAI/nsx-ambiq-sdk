"""Synthetic sealed farm data and fake GitHub API; no source/tool/silicon proof."""
from copy import deepcopy
from dataclasses import asdict
import hashlib
import json
from pathlib import Path
import shutil
from types import SimpleNamespace
from urllib.parse import urlsplit
import pytest

from tools.hardware.protocol import HEADER, MAGIC, VERSION, RESULT_SIZE, decode
from tools.sdk_qualification.farm_artifacts import Artifact
from tools.sdk_qualification.farm_inputs import Dispatch, Run, State, write
from tools.sdk_qualification.io import sha256, write_record
from tools.sdk_qualification.models import Revisions
from tools.sdk_qualification.plan import Plan
from tools.sdk_qualification.pr_reporting.github import API
from tools.sdk_qualification.pr_reporting.models import BINDING_JOB, BOARDS, WORKFLOW, Binding
from tools.sdk_qualification.report import initial_report


class FakeAPI(API):
    def __init__(self, run):
        super().__init__(run)
        self.main = 'e'*40
        self.workflow = {'id': 17, 'path': WORKFLOW}
        self.repository = {'full_name': run.repository, 'default_branch': 'main'}
        self.metadata = dict(id=int(run.run_id), run_attempt=int(run.attempt), workflow_id=17,
                             head_sha=run.driver, head_branch='main', event='workflow_dispatch',
                             path=WORKFLOW, repository={'full_name': run.repository},
                             head_repository={'full_name': run.repository}, status='completed', conclusion='success')
        self.latest = deepcopy(self.metadata)
        self.pr = dict(number=7, state='open', base={'repo': {'full_name': run.repository}},
                       head={'sha': 'b'*40, 'repo': {'full_name': run.repository}})
        self.jobs = [dict(name=name, status='completed', conclusion='success') for name in
                     (BINDING_JOB, 'SDK host checks', 'SDK selected-scope assessment',
                      *(f'SDK board ({b})' for b in BOARDS))]
        self.checks = []
        self.writes = []
        self.downloads = []
        self.payloads = {}
        self.receipts = {}
        self.diverged = set()
        self.lost_create = self.lost_update = self.fail_write = False

    def call(self, path, method='GET', value=None):
        route = urlsplit(path).path
        if method == 'POST':
            self.writes.append((method, path, deepcopy(value)))
            if self.fail_write:
                raise TimeoutError('API unavailable')
            result = dict(value, id=100+len(self.checks), app={'slug': 'github-actions'}, conclusion=None)
            self.checks.append(result)
            if self.lost_create:
                self.lost_create = False
                raise TimeoutError('create response lost after commit')
            return deepcopy(result)
        if method == 'PATCH':
            self.writes.append((method, path, deepcopy(value)))
            if self.fail_write:
                raise TimeoutError('API unavailable')
            result = next(c for c in self.checks if c['id'] == int(path.rsplit('/', 1)[1]))
            result.update(deepcopy(value))
            if self.lost_update:
                self.lost_update = False
                raise TimeoutError('update response lost after commit')
            return deepcopy(result)
        if route == '': return deepcopy(self.repository)
        if route.startswith('/actions/workflows/'): return deepcopy(self.workflow)
        if route == '/git/ref/heads/main': return {'object': {'sha': self.main}}
        if route.startswith('/compare/'):
            old, new = route.split('/')[-1].split('...')
            return dict(status='diverged' if old in self.diverged else 'ahead',
                        base_commit={'sha': old}, merge_base_commit={'sha': old})
        if route == '/pulls/7': return deepcopy(self.pr)
        if route == f'/actions/runs/{self.run.run_id}': return deepcopy(self.latest)
        if route.endswith(f'/attempts/{self.run.attempt}'): return deepcopy(self.metadata)
        if route.endswith('/artifacts'):
            return {'artifacts': [dict(id=a.artifact_id, name=a.name, digest=a.digest, expired=False,
                                       workflow_run={'id': int(self.run.run_id), 'head_sha': self.run.driver})
                                  for a in self.receipts.values()]}
        if route.endswith('/jobs'): return {'jobs': deepcopy(self.jobs)}
        if route.endswith('/check-runs'): return {'check_runs': deepcopy(self.checks)}
        raise AssertionError((path, method, value))

    def download(self, artifact, destination):
        self.downloads.append(artifact.stage)
        shutil.copytree(self.payloads[artifact.stage], destination)

    def add_artifact(self, stage, payload):
        self.payloads[stage] = payload
        number = 1+len(self.receipts)
        self.receipts[stage] = Artifact(self.run, stage, number, self.run.artifact_name(stage),
                                       'sha256:'+f'{number:064x}')
        return self.receipts[stage]


def synthetic(tmp_path):
    run = Run('AmbiqAI/nsx-ambiq-sdk', '42', '2', 'd'*40, 'https://github.com')
    plan = Plan(Revisions('a'*40, 'b'*40, 'c'*40))
    api = FakeAPI(run)
    inputs = tmp_path/'inputs'
    inputs.mkdir()
    write(inputs/'plan.json', plan.to_dict())
    write(inputs/'decisions.json', {})
    dispatch = Dispatch(run, BOARDS, plan.digest, sha256(inputs/'decisions.json'), '/pinned/pack.zip')
    write(inputs/'dispatch.json', dispatch.to_dict())
    binding = Binding(run, 7, plan.revisions, plan.digest, dispatch.decisions_sha256, BOARDS, 17, api.main)

    def payload(stage):
        path = tmp_path/'remote'/stage
        shutil.copytree(inputs, path/'inputs')
        api.add_artifact(stage, path)
        return path

    bound = payload('pr-binding')
    write(bound/'binding.json', binding.to_dict())
    host = payload('host')
    host_dir = host/'evidence/host'
    host_dir.mkdir(parents=True)
    (host_dir/'host.txt').write_text('synthetic host evidence, not executed tests')

    def body(mode, boards, execution, files):
        return dict(execution_id=execution, plan_sha256=plan.digest, revisions=asdict(plan.revisions),
                    scope={'mode': mode, 'boards': list(boards)}, scope_status='passed', error=None,
                    inputs_rechecked=True, files=files,
                    cleanup={'process_groups': 'completed',
                             'board_state': 'tests-completed' if mode=='hardware' else 'not-accessed'})

    host_record = write_record(host_dir, body('host', (), '1'*32, {'host.txt': sha256(host_dir/'host.txt')}))
    State(run, 'host', dispatch.digest, status='passed', host_record_sha256=host_record['sha256']).save(host/'evidence')
    executions = [{'execution_id': '1'*32, 'sha256': host_record['sha256']}]
    for index, board in enumerate(BOARDS):
        root = payload(board)
        directory = root/'evidence/board'
        directory.mkdir(parents=True)
        shutil.copytree(host_dir, directory/'host')
        entries = []
        for row in (r for r in plan.rows if r.board == board):
            for side in row.sides:
                relative = f'firmware/{row.id}/{side}'
                output = directory/relative
                output.mkdir(parents=True)
                build_id = hashlib.sha256((row.id+side).encode()).hexdigest()[:32]
                text = (''.join(f'test.c:1:{case}:PASS\n' for case in row.cases)
                        + f'{len(row.cases)} Tests 0 Failures 0 Ignored\nOK\n').encode()
                header = HEADER.pack(MAGIC, VERSION, 2, len(row.cases), 0, 0, len(text), 0, build_id.encode())
                packet = header+text+bytes(RESULT_SIZE-len(header)-len(text))
                (output/'last-result.bin').write_bytes(packet)
                result = decode(packet, build_id, row.cases)
                assert result is not None
                write(output/'result.json', asdict(result))
                write(output/'manifest.json', dict(sdk={'revision': getattr(plan.revisions, side), 'dirty': False},
                      harness={'revision': plan.revisions.harness, 'dirty': False}, board=board,
                      build_id=build_id, expected_cases=list(row.cases)))
                files = {name: sha256(output/name) for name in ('last-result.bin','result.json','manifest.json')}
                entries.append(dict(row=row.id, side=side, directory=relative, build_id=build_id,
                                    files=files, hardware=True, binding={'board':board,'probe':str(123+index)}))
        files = {str(f.relative_to(directory)): sha256(f) for f in directory.rglob('*') if f.is_file()}
        execution_id = hashlib.sha256(board.encode()).hexdigest()[:32]
        record = write_record(directory, dict(body('hardware', (board,), execution_id, files),
                              firmware=entries, host_reference={'directory':'host','sha256':host_record['sha256']}))
        executions.append({'execution_id': execution_id, 'sha256': record['sha256']})
        holder = f'sdk-{run.run_id}-{run.attempt}-{board}'
        reason = f'UNKNOWN/recovery-required if interrupted: {run.url} board={board}'
        State(run, 'board', dispatch.digest, board=board, status='passed', lease='released',
              board_state='tests-completed', host_artifact=api.receipts['host'].to_dict(),
              host_record_sha256=host_record['sha256'], holder=holder, reason=reason,
              acquired={'pid':999+index,'user':'runner','holder':holder,'reason':reason},
              binding={'id':board,'board':board,'probe':str(123+index)}).save(root/'evidence')
    assessment = payload('assessment')
    (assessment/'evidence').mkdir()
    State(run, 'assessment', dispatch.digest, status='passed', host_artifact=api.receipts['host'].to_dict(),
          host_record_sha256=host_record['sha256']).save(assessment/'evidence')
    for board in BOARDS:
        download = assessment/'evidence/downloaded'/board
        shutil.copytree(api.payloads[board], download/'payload')
        write(download/'receipt.json', api.receipts[board].to_dict())
    report = dict(initial_report(plan), errors=[], executions=executions)
    for item in report['requirements']:
        parts = item['requirement'].split('/')
        if len(parts)==1 or parts[0] in BOARDS or parts[0]=='compiler' and parts[2] in BOARDS:
            item['status'] = 'passed'
    report_dir = assessment/'evidence/assessment'
    report_dir.mkdir()
    write(report_dir/'assessment.json', report)
    event = {'action':'completed','repository':{'full_name':run.repository}, 'workflow_run':deepcopy(api.metadata)}
    return SimpleNamespace(run=run, plan=plan, dispatch=dispatch, binding=binding, api=api,
                           event=event, inputs=inputs, root=tmp_path)


@pytest.fixture
def farm(tmp_path, monkeypatch):
    monkeypatch.setattr('tools.sdk_qualification.pr_reporting.github.time.sleep', lambda _: None)
    return synthetic(tmp_path)
