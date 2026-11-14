"""Farm boundary checks use subprocess fixtures, never physical probes."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys

import pytest

from tools.sdk_qualification import farm


@pytest.fixture
def lab(tmp_path, monkeypatch):
    inventory = tmp_path / 'boards.json'
    inventory.write_text(json.dumps({'boards': [{'id': 'ap510-2', 'name': farm.BOARD,
                                               'serial': '123', 'runner': True}]}))
    home = tmp_path / 'home'
    overlay = home / '.config/SEGGER/JLinkDevices/Ambiq'
    overlay.mkdir(parents=True)
    (overlay / 'Device.xml').write_text('reset script selection')
    library = tmp_path / 'libjlinkarm.so'
    library.write_bytes(b'fixture, not loadable')
    for key, value in dict(HOME=str(home), BENCH_BOARD='ap510-2', HPX_BOARD=farm.BOARD,
                           HPX_JLINK_SERIAL='123', HPX_JLINK_DLL=str(library),
                           GITHUB_SERVER_URL='https://github.com', GITHUB_REPOSITORY='AmbiqAI/nsx-ambiq-sdk',
                           GITHUB_RUN_ID='42', GITHUB_RUN_ATTEMPT='2', RUNNER_TRACKING_ID='tracked',
                           FAKE_STATE=str(tmp_path / 'bench-state.json')).items():
        monkeypatch.setenv(key, value)
    bin_dir = tmp_path / 'bin'
    bin_dir.mkdir()
    bench = bin_dir / 'bench-agent'
    bench.write_text('#!' + sys.executable + '\n' + '''
import json,os,sys
from pathlib import Path
args=sys.argv[1:]; path=Path(os.environ['FAKE_STATE'])
assert 'RUNNER_TRACKING_ID' not in os.environ
if args[0]=='unlock' and '--help' in args:
    print('legacy unlock' if os.environ.get('FAKE_MODE')=='legacy' else '--expected-pid --expected-holder --expected-reason')
elif args[0]=='lock':
    if os.environ.get('FAKE_MODE')=='busy': sys.exit(3)
    value={'pid':987,'holder':args[args.index('--holder')+1],
           'reason':args[args.index('--reason')+1],'user':'runner',
           'id':'ap510-2','name':'apollo510_evb','serial':'123'}
    path.write_text(json.dumps(value)); print('ap510-2 locked (pid 987)')
elif args[0]=='status':
    value=json.loads(path.read_text())
    if os.environ.get('FAKE_MODE')=='replaced' and value.get('checked'): value['pid']=999
    value['checked']=True; path.write_text(json.dumps(value))
    print(json.dumps({'boards':[value]}))
elif args[0]=='unlock':
    value=json.loads(path.read_text())
    assert args[args.index('--expected-pid')+1]==str(value['pid'])
    assert args[args.index('--expected-holder')+1]==value['holder']
    assert args[args.index('--expected-reason')+1]==value['reason']
    path.unlink()
else: sys.exit(2)
''')
    bench.chmod(0o755)
    monkeypatch.setenv('PATH', str(bin_dir) + os.pathsep + os.environ['PATH'])
    harness = tmp_path / 'harness'
    package = harness / 'tools/sdk_qualification'
    package.mkdir(parents=True)
    (harness / 'tools/__init__.py').touch()
    (package / '__init__.py').touch()
    (package / 'build_worker.py').write_text('''
import json,sys
from pathlib import Path
out=Path(sys.argv[sys.argv.index('--output')+1]); out.mkdir()
(out/'firmware.json').write_text(json.dumps({'device':'SDK-selected-device'}))
''')
    (package / '__main__.py').write_text('''
import hashlib,json,os,signal,sys
from pathlib import Path
args=sys.argv[1:]; action=args[0]; out=Path(args[args.index('--output')+1]); out.mkdir()
mode=os.environ.get('FAKE_MODE')
if mode=='cancel' and action=='run-board': os.kill(os.getppid(),signal.SIGTERM)
if mode=='failure' and action=='run-board': sys.exit(1)
if action=='assess':
    report={'status':'failed' if mode=='invalid' else 'incomplete','qualification':False,
            'errors':['fixture evidence invalid'] if mode=='invalid' else []}
    (out/'assessment.json').write_text(json.dumps(report)); sys.exit(1)
if action=='run-board':
    if mode=='missing': sys.exit(0)
    if mode=='overlay': (Path.home()/'.config/SEGGER/JLinkDevices/Ambiq/Device.xml').write_text('changed')
    assert '--jlink-library' in args
    body={'cleanup':{'process_groups':'completed','board_state':'tests-completed'}}
    digest=hashlib.sha256(json.dumps(body,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    (out/'record.json').write_text(json.dumps({'schema_version':1,'kind':'sdk-qualification-execution',
                                            'body':body,'sha256':digest}))
''')
    inputs = tmp_path / 'inputs'
    inputs.mkdir()
    (inputs / 'plan.json').write_text('{}')
    (inputs / 'decisions.json').write_text('{}')
    return dict(inputs=inputs, baseline=tmp_path / 'baseline', candidate=tmp_path / 'candidate',
                harness=harness, python=Path(sys.executable), gcc=tmp_path / 'gcc',
                output=tmp_path / 'evidence', inventory=inventory)


def test_subprocess_pilot_reuses_cli_and_releases_only_valid_scope(lab):
    result = farm.run(**lab)
    assert result['pilot_status'] == 'passed'
    assert result['qualification'] is False and result['global_status'] == 'incomplete'
    assert result['lease'] == 'released'
    assert not Path(os.environ['FAKE_STATE']).exists()
    binding = json.loads((lab['output'] / 'inventory.json').read_text())['boards'][0]
    assert binding == {'board': farm.BOARD, 'probe': '123', 'device': 'SDK-selected-device'}


@pytest.mark.parametrize('mode', ['failure', 'invalid', 'replaced', 'cancel', 'missing', 'overlay'])
def test_failure_never_unlocks_or_qualifies(lab, monkeypatch, mode):
    monkeypatch.setenv('FAKE_MODE', mode)
    before = signal.getsignal(signal.SIGTERM)
    with pytest.raises((RuntimeError, OSError, InterruptedError, subprocess.CalledProcessError)):
        farm.run(**lab)
    result = json.loads((lab['output'] / 'pilot.json').read_text())
    assert result['pilot_status'] == 'failed' and result['qualification'] is False
    assert result['board_state'] == 'unknown'
    assert result['lease'] == 'retained-until-ttl-or-operator-recovery'
    assert Path(os.environ['FAKE_STATE']).is_file()
    assert 'UNKNOWN/recovery-required' in json.loads(Path(os.environ['FAKE_STATE']).read_text())['reason']
    assert signal.getsignal(signal.SIGTERM) == before


def test_busy_does_not_access_or_unlock_other_claim(lab, monkeypatch):
    monkeypatch.setenv('FAKE_MODE', 'busy')
    with pytest.raises(subprocess.CalledProcessError):
        farm.run(**lab)
    assert not (lab['output'] / 'board').exists()
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_old_allocator_fails_before_host_checks_and_reservation(lab, monkeypatch):
    monkeypatch.setenv('FAKE_MODE', 'legacy')
    with pytest.raises(RuntimeError, match='conditional unlock'):
        farm.run(**lab)
    assert not (lab['output'] / 'host').exists()
    assert not Path(os.environ['FAKE_STATE']).exists()


@pytest.mark.parametrize('key,value', [('HPX_BOARD', 'apollo510L_evb'), ('HPX_JLINK_SERIAL', '456'),
                                      ('BENCH_BOARD', 'missing')])
def test_wrong_runner_fails_before_qualification_or_lease(lab, monkeypatch, key, value):
    monkeypatch.setenv(key, value)
    with pytest.raises(ValueError):
        farm.run(**lab)
    assert not (lab['output'] / 'host').exists()
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_overlay_changes_invalidate_debugger_provenance(lab):
    library = Path(os.environ['HPX_JLINK_DLL'])
    overlay = Path.home() / '.config/SEGGER/JLinkDevices'
    before = farm.debugger_inputs(library, overlay)
    (overlay / 'Ambiq/Device.xml').write_text('different reset script')
    assert farm.debugger_inputs(library, overlay) != before
    library.unlink()
    with pytest.raises(ValueError):
        farm.debugger_inputs(library, overlay)


def test_nested_overlay_directory_link_fails_before_reservation(lab, tmp_path):
    scripts = tmp_path / 'external-scripts'
    scripts.mkdir()
    (scripts / 'reset.JLinkScript').write_text('reset implementation')
    overlay = Path.home() / '.config/SEGGER/JLinkDevices'
    (overlay / 'scripts').symlink_to(scripts, target_is_directory=True)
    with pytest.raises(ValueError, match='symlinks'):
        farm.run(**lab)
    assert not (lab['output'] / 'host').exists()
    assert not Path(os.environ['FAKE_STATE']).exists()


def test_root_overlay_link_and_regular_file_links_are_fingerprinted(lab, tmp_path):
    overlay = Path.home() / '.config/SEGGER/JLinkDevices'
    actual = tmp_path / 'deployed-devices'
    overlay.rename(actual)
    overlay.symlink_to(actual, target_is_directory=True)
    script = tmp_path / 'reset.JLinkScript'
    script.write_text('first reset')
    (actual / 'reset.JLinkScript').symlink_to(script)
    library = Path(os.environ['HPX_JLINK_DLL'])
    before = farm.debugger_inputs(library, overlay)
    assert before['overlay']['resolved'] == str(actual)
    assert before['overlay']['files']['reset.JLinkScript']['path'] == str(script)
    script.write_text('changed reset')
    assert farm.debugger_inputs(library, overlay) != before


@pytest.mark.parametrize('revision', ['main', 'abc', 'a' * 39, 'A' * 40])
def test_dispatch_rejects_moving_or_malformed_refs(tmp_path, monkeypatch, revision):
    monkeypatch.setenv('SDK_PLAN_JSON', json.dumps({'revisions': dict(baseline=revision, candidate='b'*40, harness='c'*40)}))
    monkeypatch.setenv('SDK_DECISIONS_JSON', '{}')
    monkeypatch.setenv('GITHUB_OUTPUT', str(tmp_path / 'github-output'))
    with pytest.raises(ValueError):
        farm.prepare(tmp_path / 'inputs')
    assert not (tmp_path / 'github-output').exists()


def test_dispatch_passes_json_as_data_and_exports_only_shas(tmp_path, monkeypatch):
    revisions = dict(baseline='a'*40, candidate='b'*40, harness='c'*40)
    monkeypatch.setenv('SDK_PLAN_JSON', json.dumps({'revisions': revisions, 'note': '$(false)'}))
    monkeypatch.setenv('SDK_DECISIONS_JSON', '{}')
    monkeypatch.setenv('GITHUB_OUTPUT', str(tmp_path / 'github-output'))
    farm.prepare(tmp_path / 'inputs')
    assert 'note' not in (tmp_path / 'github-output').read_text()
    assert json.loads((tmp_path / 'inputs/plan.json').read_text())['note'] == '$(false)'


def test_workflow_is_manual_scoped_and_retains_failed_evidence(repo_root):
    import yaml
    workflow = yaml.safe_load((repo_root / '.github/workflows/sdk-hardware-qualification.yml').read_text())
    # YAML 1.1 treats `on` as a boolean; GitHub uses the literal key.
    assert set(workflow.get('on', workflow.get(True))) == {'workflow_dispatch'}
    job = workflow['jobs']['ap510-pilot']
    assert job['runs-on'] == ['self-hosted', 'hpx-hardware', 'apollo510_evb']
    assert job['timeout-minutes'] < 70
    upload = next(step for step in job['steps'] if step.get('uses', '').startswith('actions/upload-artifact@'))
    assert upload['if'] == 'always()'
    assert upload['with']['include-hidden-files'] is True
    assert upload['with']['if-no-files-found'] == 'error'
    assert '${{ github.run_attempt }}' in upload['with']['name']
    # A failed staging step may never export GITHUB_ENV. Upload must still be
    # constrained to this run's temp directory, never an empty root plus '/'.
    assert upload['with']['path'].startswith('${{ runner.temp }}/sdk-farm-')
    assert '${{ github.run_attempt }}' in upload['with']['path']
