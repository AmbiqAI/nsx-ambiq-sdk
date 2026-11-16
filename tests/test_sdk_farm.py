"""Farm process/allocator boundaries; no physical probes or network access."""
from dataclasses import asdict, replace
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
from types import SimpleNamespace

import pytest

from tools.sdk_qualification import Plan, Revisions, write_plan
from tools.sdk_qualification import farm_bench, farm_run
from tools.sdk_qualification.farm_artifacts import Artifact
from tools.sdk_qualification.farm_inputs import AVAILABLE_BOARDS, State, passed_state, prepare, write
from tools.sdk_qualification.io import read_record, write_record


def host_record(path, plan):
    path.mkdir(parents=True)
    body = dict(plan_sha256=plan.digest, revisions=asdict(plan.revisions),
                scope={'mode': 'host', 'boards': []}, scope_status='passed', error=None,
                inputs_rechecked=True, cleanup={'process_groups': 'completed', 'board_state': 'not-accessed'},
                files={})
    return write_record(path, body)


@pytest.fixture
def lab(tmp_path, monkeypatch, repo_root):
    plan = Plan(Revisions('a'*40, 'b'*40, 'c'*40))
    for key, value in dict(GITHUB_REPOSITORY='AmbiqAI/nsx-ambiq-sdk', GITHUB_RUN_ID='42',
                           GITHUB_RUN_ATTEMPT='2', GITHUB_SHA='d'*40, GITHUB_SERVER_URL='https://github.com',
                           GITHUB_OUTPUT=str(tmp_path/'github-output'), SDK_BOARDS_JSON='["apollo510_evb"]',
                           SDK_PLAN_JSON=json.dumps(plan.to_dict()), SDK_DECISIONS_JSON='{}',
                           FAKE_STATE=str(tmp_path/'bench-state.json'), RUNNER_TRACKING_ID='tracked').items():
        monkeypatch.setenv(key, value)
    inputs = tmp_path/'inputs'
    dispatch = prepare(inputs)
    download = tmp_path/'host-download'
    origin = download/'payload'
    shutil.copytree(inputs, origin/'inputs')
    envelope = host_record(origin/'evidence/host', plan)
    state = State(dispatch.run, 'host', dispatch.digest, status='passed', host_record_sha256=envelope['sha256'])
    state.save(origin/'evidence')
    receipt = Artifact(dispatch.run, 'host', 1, dispatch.run.artifact_name('host'), 'sha256:'+'1'*64)
    write(download/'receipt.json', receipt.to_dict())
    inventory = tmp_path/'boards.json'
    write(inventory, {'boards': [{'id': 'ap510-2', 'name': 'apollo510_evb', 'serial': '123', 'runner': True}]})
    home = tmp_path/'home'
    overlay = home/'.config/SEGGER/JLinkDevices/Ambiq'
    overlay.mkdir(parents=True)
    (overlay/'Device.xml').write_text('reset script selection')
    library = tmp_path/'libjlinkarm.so'
    library.write_bytes(b'fixture, not loadable')
    for key, value in dict(HOME=str(home), BENCH_BOARD='ap510-2', HPX_BOARD='apollo510_evb',
                           HPX_JLINK_SERIAL='123', HPX_JLINK_DLL=str(library)).items():
        monkeypatch.setenv(key, value)
    bin_dir = tmp_path/'bin'
    bin_dir.mkdir()
    bench = bin_dir/'bench-agent'
    bench.write_text('#!' + sys.executable + '\n' + '''
import json,os,signal,sys
from pathlib import Path
args=sys.argv[1:]; path=Path(os.environ['FAKE_STATE']); mode=os.environ.get('FAKE_MODE')
assert 'RUNNER_TRACKING_ID' not in os.environ
if args[0]=='unlock' and '--help' in args:
    print('legacy unlock' if mode=='legacy' else '--expected-pid --expected-holder --expected-reason')
elif args[0]=='lock':
    if mode=='busy':sys.exit(3)
    value={'pid':987,'holder':args[args.index('--holder')+1],
           'reason':args[args.index('--reason')+1],'user':'runner',
           'id':'ap510-2','name':'apollo510_evb','serial':'123'}
    path.write_text(json.dumps(value))
    if mode=='lost-receipt':sys.exit(1)
    if mode=='cancel-acquire':os.kill(os.getppid(),signal.SIGTERM)
    print('invalid receipt' if mode=='malformed' else 'ap510-2 locked (pid 987)')
elif args[0]=='status':
    value=json.loads(path.read_text())
    if mode=='replaced' and value.get('checked'):value['pid']=999
    value['checked']=True;path.write_text(json.dumps(value));print(json.dumps({'boards':[value]}))
elif args[0]=='unlock':
    value=json.loads(path.read_text())
    assert args[args.index('--expected-pid')+1]==str(value['pid'])
    assert args[args.index('--expected-holder')+1]==value['holder']
    assert args[args.index('--expected-reason')+1]==value['reason']
    if mode=='unlock-failure':sys.exit(1)
    path.unlink()
else:sys.exit(2)
''')
    bench.chmod(0o755)
    monkeypatch.setenv('PATH', str(bin_dir)+os.pathsep+os.environ['PATH'])
    harness = tmp_path/'harness'
    package = harness/'tools/sdk_qualification'
    package.mkdir(parents=True)
    (harness/'tools/__init__.py').write_text(f'__path__.append({str(repo_root/"tools")!r})\n')
    (package/'__init__.py').write_text(f'__path__.append({str(repo_root/"tools/sdk_qualification")!r})\n')
    hardware = harness/'tools/hardware'
    hardware.mkdir()
    (hardware/'__init__.py').write_text(f'__path__.append({str(repo_root/"tools/hardware")!r})\n')
    for name in ('device_pack.py','device-pack.json'):
        shutil.copyfile(repo_root/'tools/hardware'/name,hardware/name)
    (package/'build_worker.py').write_text('''
import json,sys
from pathlib import Path
out=Path(sys.argv[sys.argv.index('--output')+1]);out.mkdir()
(out/'firmware.json').write_text(json.dumps({'device':'SDK-selected-device'}))
''')
    (package/'farm.py').write_text('''
import json,os,sys
from pathlib import Path
args=sys.argv[1:];out=Path(args[args.index('--output')+1])
if args[0]=='check-pack':
    from tools.sdk_qualification.farm_run import check_pack
    archive=Path(args[args.index('--archive')+1]) if '--archive' in args else None
    check_pack(args[args.index('--device')+1],archive,out);sys.exit()
out.mkdir()
if os.environ.get('FAKE_MODE')=='preflight':sys.exit(1)
host=Path(args[args.index('--host-record')+1]);record=json.loads((host/'record.json').read_text())
(out/'checked.json').write_text(json.dumps({'host_record_sha256':record['sha256']}))
''')
    (package/'__main__.py').write_text('''
import json,os,signal,sys
from pathlib import Path
from tools.sdk_qualification.plan import load_plan
from tools.sdk_qualification.report import initial_report
from tools.sdk_qualification.io import read_record,write_record
args=sys.argv[1:];action=args[0];out=Path(args[args.index('--output')+1]);out.mkdir()
plan=load_plan(Path(args[args.index('--plan')+1]));mode=os.environ.get('FAKE_MODE')
if mode=='cancel' and action=='run-board':os.kill(os.getppid(),signal.SIGTERM)
if mode=='failure' and action=='run-board':sys.exit(1)
if action=='assess':
    report=initial_report(plan);report['errors']=[]
    if mode=='invalid':report.update(status='failed',errors=['fixture evidence invalid'])
    boards=set()
    for i,a in enumerate(args):
        if a=='--record':boards.update(read_record(Path(args[i+1]))['body']['scope']['boards'])
    for item in report['requirements']:
        parts=item['requirement'].split('/')
        if len(parts)==1 or parts[0] in boards or parts[0]=='compiler' and parts[2] in boards:
            item['status']='passed'
    if not report['errors'] and all(i['status']=='passed' for i in report['requirements']):
        report.update(status='passed',qualification=True)
    (out/'assessment.json').write_text(json.dumps(report));sys.exit(0 if report['qualification'] else 1)
if action=='run-board':
    if mode=='missing':sys.exit(0)
    if mode=='overlay':(Path.home()/'.config/SEGGER/JLinkDevices/Ambiq/Device.xml').write_text('changed')
    assert '--jlink-library' in args
    host=Path(args[args.index('--host-record')+1]);board=args[args.index('--board')+1]
    body=dict(plan_sha256=plan.digest,revisions=plan.to_dict()['revisions'],
              scope={'mode':'hardware','boards':[board]},scope_status='passed',error=None,
              inputs_rechecked=True,files={},host_reference={'directory':'host','sha256':read_record(host)['sha256']},
              cleanup={'process_groups':'completed','board_state':'unknown' if mode=='cleanup' else 'tests-completed'})
    write_record(out,body)
''')
    paths = farm_run.Paths(inputs, tmp_path/'baseline', tmp_path/'candidate', harness,
                           Path(sys.executable), tmp_path/'gcc', tmp_path/'evidence')
    return SimpleNamespace(paths=paths, dispatch=dispatch, plan=plan, download=download, inventory=inventory)


def test_board_reuses_host_and_releases_only_verified_scope(lab):
    state = farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    assert state.status == 'passed' and state.qualification is False
    assert state.lease == 'released' and not Path(os.environ['FAKE_STATE']).exists()
    assert state.host_record_sha256 == read_record(lab.download/'payload/evidence/host')['sha256']
    assert state.holder.endswith('-apollo510_evb')


@pytest.mark.parametrize('mode', ['failure','invalid','replaced','cancel','missing','overlay','cleanup','unlock-failure'])
def test_failure_retains_unknown_without_unlock(lab, monkeypatch, mode):
    monkeypatch.setenv('FAKE_MODE', mode)
    before = signal.getsignal(signal.SIGTERM)
    with pytest.raises((ValueError, RuntimeError, OSError, InterruptedError, subprocess.CalledProcessError)):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    state = json.loads((lab.paths.output/'state.json').read_text())
    assert state['status'] == 'failed' and state['board_state'] == 'unknown'
    assert state['lease'] == 'retained-until-ttl-or-operator-recovery'
    assert Path(os.environ['FAKE_STATE']).exists()
    assert signal.getsignal(signal.SIGTERM) == before


@pytest.mark.parametrize('mode', ['lost-receipt','malformed','cancel-acquire'])
def test_interrupted_acquisition_preserves_recovery_identity(lab, monkeypatch, mode):
    monkeypatch.setenv('FAKE_MODE', mode)
    with pytest.raises((RuntimeError, InterruptedError, subprocess.CalledProcessError)):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    state = json.loads((lab.paths.output/'state.json').read_text())
    assert state['lease'] in ('acquiring','retained-until-ttl-or-operator-recovery')
    assert state['board_state'] == 'unknown' and state['acquired'] is None
    assert state['holder'] and 'apollo510_evb' in state['reason']
    assert Path(os.environ['FAKE_STATE']).exists()


@pytest.mark.parametrize('mode', ['legacy','preflight'])
def test_preflight_failure_cannot_reserve(lab, monkeypatch, mode):
    monkeypatch.setenv('FAKE_MODE', mode)
    with pytest.raises((RuntimeError, subprocess.CalledProcessError)):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_wrong_attempt_rejected_before_allocator(lab, monkeypatch):
    monkeypatch.setenv('GITHUB_RUN_ATTEMPT', '3')
    with pytest.raises(ValueError, match='rerun all jobs'):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    assert not Path(os.environ['FAKE_STATE']).exists()


@pytest.mark.parametrize('key,value', [('HPX_BOARD','apollo510L_evb'),('HPX_JLINK_SERIAL','456'),('BENCH_BOARD','missing')])
def test_wrong_runner_rejected_before_reservation(lab, monkeypatch, key, value):
    monkeypatch.setenv(key, value)
    with pytest.raises(ValueError):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_overlay_changes_invalidate_debugger_identity(lab, tmp_path):
    library = Path(os.environ['HPX_JLINK_DLL'])
    overlay = Path.home()/'.config/SEGGER/JLinkDevices'
    before = farm_bench.debugger_inputs(library, overlay)
    (overlay/'Ambiq/Device.xml').write_text('changed')
    assert before != farm_bench.debugger_inputs(library, overlay)
    external = tmp_path/'external'
    external.mkdir()
    (overlay/'linked').symlink_to(external, target_is_directory=True)
    with pytest.raises(ValueError, match='symlinks'):
        farm_run.board(lab.paths, 'apollo510_evb', lab.download, lab.inventory)
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_root_and_regular_file_links_are_fingerprinted(lab, tmp_path):
    overlay = Path.home()/'.config/SEGGER/JLinkDevices'
    actual = tmp_path/'deployed'
    overlay.rename(actual)
    overlay.symlink_to(actual, target_is_directory=True)
    script = tmp_path/'reset.JLinkScript'
    script.write_text('reset')
    (actual/'reset.JLinkScript').symlink_to(script)
    library = Path(os.environ['HPX_JLINK_DLL'])
    before = farm_bench.debugger_inputs(library, overlay)
    assert before['overlay']['resolved'] == str(actual)
    script.write_text('changed')
    assert before != farm_bench.debugger_inputs(library, overlay)


def test_later_upload_failure_does_not_erase_released_board(lab):
    state = State(lab.dispatch.run, 'board', lab.dispatch.digest, board='apollo510_evb')
    with pytest.raises(OSError):
        with farm_run.journal(lab.paths.output, state):
            state.lease, state.board_state = 'released', 'tests-completed'
            raise OSError('artifact upload failed')
    result = json.loads((lab.paths.output/'state.json').read_text())
    assert result['status'] == 'failed' and result['lease'] == 'released'
    assert result['board_state'] == 'tests-completed'


@pytest.mark.parametrize('error', [OSError('late failure'), farm_run.FarmCancelled('cancelled')])
def test_late_failure_clears_positive_qualification(lab, error):
    state = State(lab.dispatch.run, 'assessment', lab.dispatch.digest)
    with pytest.raises(type(error)):
        with farm_run.journal(lab.paths.output, state):
            state.global_status, state.qualification = 'passed', True
            raise error
    result = json.loads((lab.paths.output/'state.json').read_text())
    assert result['qualification'] is False
    assert result['status'] == result['global_status'] == 'failed'


@pytest.mark.parametrize('stage,board', [('host', None), ('board', 'apollo510b_evb')])
def test_scoped_envelopes_cannot_claim_full_qualification(lab, tmp_path, stage, board):
    state = State(lab.dispatch.run, stage, lab.dispatch.digest, board=board,
                  status='passed', qualification=True, global_status='passed')
    state.save(tmp_path)
    with pytest.raises(ValueError, match='did not complete'):
        passed_state(tmp_path, lab.dispatch, stage, board)


@pytest.mark.parametrize('board', ['apollo4p_blue_kxr_evb', 'apollo510b_evb'])
def test_new_variants_require_their_exact_runner_inventory(lab, monkeypatch, board):
    monkeypatch.setenv('HPX_BOARD', board)
    write(lab.inventory, {'boards': [{'id': 'ap510-2', 'name': board, 'serial': '123', 'runner': True}]})
    assert farm_bench.runner_binding(lab.inventory, board) == {'id': 'ap510-2', 'board': board, 'probe': '123'}
    substitute = 'apollo4l_blue_evb' if board.startswith('apollo4') else 'apollo510L_evb'
    write(lab.inventory, {'boards': [{'id': 'ap510-2', 'name': substitute, 'serial': '123', 'runner': True}]})
    with pytest.raises(ValueError, match='authoritative'):
        farm_bench.runner_binding(lab.inventory, board)


@pytest.mark.parametrize('failed', [False, True])
def test_summary_frames_positive_raw_report_with_stage_outcome(lab, repo_root, tmp_path, failed):
    import yaml
    if shutil.which('jq') is None:
        pytest.skip('workflow summary uses jq')
    root = tmp_path/'summary-root'
    evidence = root/'evidence'
    assessment = evidence/'assessment'
    assessment.mkdir(parents=True)
    state = State(lab.dispatch.run, 'assessment', lab.dispatch.digest,
                  status='failed' if failed else 'passed', qualification=not failed,
                  global_status='failed' if failed else 'passed')
    state.save(evidence)
    write(assessment/'assessment.json', {'status':'passed', 'qualification':True, 'requirements':[]})
    workflow = yaml.safe_load((repo_root/'.github/workflows/sdk-hardware-qualification.yml').read_text())
    script = next(s['run'] for s in workflow['jobs']['assessment']['steps']
                  if s.get('name') == 'Summarize stage and board state')
    summary = tmp_path/'summary.md'
    env = dict(os.environ, SDK_FARM_ROOT=str(root), SDK_STAGE='assessment', GITHUB_STEP_SUMMARY=str(summary))
    subprocess.run(['bash', '-e', '-o', 'pipefail', '-c', script], env=env, check=True)
    text = summary.read_text()
    stage_text, raw_text = text.split('Raw assessment below;', 1)
    stage_value = json.loads(stage_text[stage_text.index('{'):])
    assert stage_value['qualification'] is (not failed)
    assert stage_value['status'] == stage_value['global_status'] == ('failed' if failed else 'passed')
    assert 'Release approval is separate.' in raw_text


@pytest.mark.parametrize('outcome', ['failure', 'cancelled', 'success'])
def test_workflow_failure_after_upload_supersedes_positive_scope(lab, repo_root, tmp_path, outcome):
    import yaml
    if shutil.which('jq') is None:
        pytest.skip('workflow outcome uses jq')
    root = tmp_path/'completed-scope'
    evidence = root/'evidence'
    evidence.mkdir(parents=True)
    state = State(lab.dispatch.run, 'assessment', lab.dispatch.digest, status='passed',
                  global_status='passed', qualification=True, lease='released')
    state.save(evidence)
    workflow = yaml.safe_load((repo_root/'.github/workflows/sdk-hardware-qualification.yml').read_text())
    for job in workflow['jobs'].values():
        steps = job['steps']
        upload_index = next(i for i,s in enumerate(steps) if s.get('uses','').startswith('actions/upload-artifact@'))
        final = steps[upload_index+1]
        assert final['name'] == 'Record enclosing workflow stage outcome'
        assert final['if'] == 'always()' and final['env']['SDK_JOB_STATUS'] == '${{ job.status }}'
    summary = tmp_path/'summary.md'
    env = dict(os.environ, SDK_FARM_ROOT=str(root), SDK_STAGE='assessment',
               SDK_JOB_STATUS=outcome, GITHUB_STEP_SUMMARY=str(summary))
    subprocess.run(['bash','-e','-o','pipefail','-c',final['run']], env=env, check=True)
    value = json.loads((evidence/'state.json').read_text())
    assert value['qualification'] is (outcome == 'success')
    assert value['status'] == value['global_status'] == ('passed' if outcome == 'success' else 'failed')
    assert value['lease'] == 'released'
    text = summary.read_text()
    assert 'final successful GitHub job/workflow conclusions' in text
    if outcome != 'success':
        assert 'supersedes provisional scope evidence' in text


def selected_board_artifacts(lab, tmp_path, boards=('apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb')):
    dispatch = replace(lab.dispatch, boards=tuple(sorted(boards)),
                       device_pack_archive='/provisioned/pinned.zip')
    write(lab.paths.inputs/'dispatch.json', dispatch.to_dict())
    write(lab.download/'payload/inputs/dispatch.json', dispatch.to_dict())
    host_sha = read_record(lab.download/'payload/evidence/host')['sha256']
    State(dispatch.run, 'host', dispatch.digest, status='passed', host_record_sha256=host_sha).save(
        lab.download/'payload/evidence')
    host_artifact = json.loads((lab.download/'receipt.json').read_text())
    remote = {}
    for index, board in enumerate(dispatch.boards):
        payload = tmp_path/'remote'/board
        shutil.copytree(lab.paths.inputs, payload/'inputs')
        directory = payload/'evidence/board'
        directory.mkdir(parents=True)
        shutil.copytree(lab.download/'payload/evidence/host', directory/'host')
        write_record(directory, dict(plan_sha256=lab.plan.digest, revisions=asdict(lab.plan.revisions),
                     scope={'mode':'hardware','boards':[board]},scope_status='passed',error=None,
                     inputs_rechecked=True,files={},host_reference={'directory':'host','sha256':host_sha},
                     cleanup={'process_groups':'completed','board_state':'tests-completed'}))
        holder=f'sdk-{dispatch.run.run_id}-{dispatch.run.attempt}-{board}'
        reason=f'UNKNOWN/recovery-required if interrupted: {dispatch.run.url} board={board}'
        state=State(dispatch.run,'board',dispatch.digest,board=board,status='passed',
                    lease='released',board_state='tests-completed',host_artifact=host_artifact,
                    host_record_sha256=host_sha,holder=holder,reason=reason,
                    acquired={'holder':holder,'reason':reason,'pid':987+index,'user':'runner'},
                    binding={'id':board,'board':board,'probe':str(123+index)})
        state.save(payload/'evidence')
        remote[board]=payload
    return dispatch, remote


@pytest.mark.parametrize('failure', [None,'missing','lease','host','record','attempt','job'])
@pytest.mark.parametrize('boards', [
    ('apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb'), AVAILABLE_BOARDS,
], ids=['three-board', 'five-board'])
def test_collection_requires_complete_coherent_released_peer_set(lab,tmp_path,failure,boards):
    dispatch, remote=selected_board_artifacts(lab,tmp_path,boards)
    board='apollo4p_blue_kxr_evb' if len(boards)==5 else dispatch.boards[0]
    if failure in ('lease','host'):
        path=remote[board]/'evidence/state.json'
        state=json.loads(path.read_text())
        if failure=='lease':state['lease']='retained-until-ttl-or-operator-recovery'
        else:state['host_artifact']['artifact_id']=999
        write(path,state)
    if failure=='record':
        path=remote[board]/'evidence/board/record.json'
        value=json.loads(path.read_text());value['body']['inputs_rechecked']=False;write(path,value)
    if failure=='attempt':
        path=remote[board]/'inputs/dispatch.json'
        value=json.loads(path.read_text());value['run']['attempt']='1';write(path,value)
    class Client:
        verified_jobs=False
        def artifact(self,stage):
            if failure=='missing' and stage==board:raise ValueError('missing selected artifact')
            return Artifact(dispatch.run,stage,2+dispatch.boards.index(stage),
                            dispatch.run.artifact_name(stage),'sha256:'+'2'*64)
        def download(self,artifact,destination):shutil.copytree(remote[artifact.stage],destination)
        def require_successful_jobs(self,boards):
            assert boards==dispatch.boards
            if failure=='job':raise ValueError('selected job failed after upload')
            self.verified_jobs=True
    client=Client()
    if failure:
        with pytest.raises(ValueError):farm_run.aggregate(lab.paths,lab.download,client)
        state=json.loads((lab.paths.output/'state.json').read_text())
        assert state['status']=='failed' and state['qualification'] is False
    else:
        state=farm_run.aggregate(lab.paths,lab.download,client)
        complete = len(boards) == 5
        assert state.status=='passed' and state.qualification is complete and client.verified_jobs
        report=json.loads((lab.paths.output/'assessment/assessment.json').read_text())
        assert state.global_status == report['status'] == ('passed' if complete else 'incomplete')


def test_ap330_missing_pinned_archive_stops_before_reservation(lab,tmp_path,monkeypatch):
    dispatch,_=selected_board_artifacts(lab,tmp_path)
    monkeypatch.setenv('HPX_BOARD','apollo330mP_evb')
    write(lab.inventory,{'boards':[{'id':'ap510-2','name':'apollo330mP_evb','serial':'123','runner':True}]})
    manifest=lab.paths.harness/'tools/hardware/device-pack.json'
    write(manifest,{'devices':['SDK-selected-device'],'archive_sha256':'1'*64})
    with pytest.raises(subprocess.CalledProcessError):
        farm_run.board(lab.paths,'apollo330mP_evb',lab.download,lab.inventory)
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_pack_preflight_uses_selected_harness_pins_and_private_cache(lab,tmp_path,monkeypatch):
    import hashlib
    import zipfile
    archive=tmp_path/'selected-pack.zip'
    content=b'reviewed different harness pack'
    with zipfile.ZipFile(archive,'w') as zipped:zipped.writestr('pack/script.txt',content)
    manifest={'name':'selected-harness-pack','devices':['SDK-selected-device'],
              'archive_sha256':hashlib.sha256(archive.read_bytes()).hexdigest(),'archive_prefix':'pack/',
              'files':{'script.txt':hashlib.sha256(content).hexdigest()}}
    write(lab.paths.harness/'tools/hardware/device-pack.json',manifest)
    dispatch=replace(lab.dispatch,device_pack_archive=str(archive))
    write(lab.paths.inputs/'dispatch.json',dispatch.to_dict())
    write(lab.download/'payload/inputs/dispatch.json',dispatch.to_dict())
    host_sha=read_record(lab.download/'payload/evidence/host')['sha256']
    State(dispatch.run,'host',dispatch.digest,status='passed',host_record_sha256=host_sha).save(
        lab.download/'payload/evidence')
    cache=tmp_path/'private-job-cache'
    monkeypatch.setenv('XDG_CACHE_HOME',str(cache))
    state=farm_run.board(lab.paths,'apollo510_evb',lab.download,lab.inventory)
    pack=state.debugger['sdk_device_pack']
    assert pack['manifest']==manifest
    assert Path(pack['directory']).is_relative_to(cache)
    assert (Path(pack['directory'])/'script.txt').read_bytes()==content
