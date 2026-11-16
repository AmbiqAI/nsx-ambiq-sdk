"""Reviewed scheduling inputs never weaken the five-board acceptance profile."""
from dataclasses import replace
import json

import pytest
import yaml

from tools.sdk_qualification import Plan, Revisions
from tools.sdk_qualification.farm_inputs import AVAILABLE_BOARDS, Dispatch, Run, load_dispatch, prepare
from tools.sdk_qualification.farm_run import verify_selected
from tools.sdk_qualification.report import initial_report


@pytest.fixture
def inputs(tmp_path, monkeypatch):
    plan = Plan(Revisions('a'*40, 'b'*40, 'c'*40))
    for key, value in dict(GITHUB_REPOSITORY='AmbiqAI/nsx-ambiq-sdk', GITHUB_RUN_ID='42',
                           GITHUB_RUN_ATTEMPT='2', GITHUB_SHA='d'*40, GITHUB_SERVER_URL='https://github.com',
                           GITHUB_OUTPUT=str(tmp_path/'github-output'), GITHUB_ENV=str(tmp_path/'github-env'),
                           SDK_PLAN_JSON=json.dumps(plan.to_dict()), SDK_DECISIONS_JSON='{}',
                           SDK_BOARDS_JSON='["apollo510_evb"]', SDK_DEVICE_PACK_ARCHIVE='').items():
        monkeypatch.setenv(key, value)
    return tmp_path/'inputs', plan


@pytest.mark.parametrize('boards', [[],['apollo510_evb','apollo510_evb'],['apollo4l_blue_evb'],['apollo510L_evb'],{},[1]])
def test_invalid_or_unprovisioned_selection_is_refused(inputs, monkeypatch, boards):
    monkeypatch.setenv('SDK_BOARDS_JSON', json.dumps(boards))
    with pytest.raises(ValueError):
        prepare(inputs[0])


def test_prepare_binds_exact_scope_and_exports_only_validated_revisions(inputs):
    path, plan = inputs
    dispatch = prepare(path)
    assert dispatch.boards == ('apollo510_evb',)
    assert load_dispatch(path, Run.current()) == (dispatch, plan)
    assert len(plan.compilers[0].boards) == 5
    assert 'SDK_CANDIDATE='+'b'*40 in (path.parent/'github-env').read_text()


@pytest.mark.parametrize('board', ['apollo4p_blue_kxr_evb', 'apollo510b_evb'])
def test_new_variant_selection_preserves_exact_artifact_identity(inputs, monkeypatch, board):
    monkeypatch.setenv('SDK_BOARDS_JSON', json.dumps([board]))
    dispatch = prepare(inputs[0])
    assert load_dispatch(inputs[0], Run.current())[0].boards == (board,)
    assert dispatch.run.artifact_name(board) == f'sdk-farm-42-2-{board}'


@pytest.mark.parametrize('revision', ['main','abc','a'*39,'A'*40])
def test_moving_revisions_cannot_enter_checkout_outputs(inputs, monkeypatch, revision):
    value = inputs[1].to_dict()
    value['revisions']['candidate'] = revision
    monkeypatch.setenv('SDK_PLAN_JSON', json.dumps(value))
    with pytest.raises(ValueError):prepare(inputs[0])
    assert not (inputs[0].parent/'github-output').exists()


def test_ap330_requires_explicit_absolute_archive_source(inputs, monkeypatch):
    monkeypatch.setenv('SDK_BOARDS_JSON', json.dumps(AVAILABLE_BOARDS))
    with pytest.raises(ValueError, match='pinned'):prepare(inputs[0])
    monkeypatch.setenv('SDK_DEVICE_PACK_ARCHIVE', '/provisioned/pinned.zip')
    dispatch = prepare(inputs[0].parent/'second')
    assert len(dispatch.boards) == 5


def test_prior_attempt_and_damaged_inputs_refused(inputs):
    path, _ = inputs
    dispatch = prepare(path)
    with pytest.raises(ValueError, match='rerun all jobs'):
        load_dispatch(path, replace(dispatch.run, attempt='3'))
    (path/'decisions.json').write_text('{"changed":true}')
    with pytest.raises(ValueError, match='contents changed'):
        load_dispatch(path, dispatch.run)


def report_for(plan, boards):
    report = initial_report(plan)
    report['errors'] = []
    for item in report['requirements']:
        parts = item['requirement'].split('/')
        if len(parts)==1 or parts[0] in boards or parts[0]=='compiler' and parts[2] in boards:
            item['status'] = 'passed'
    if all(item['status'] == 'passed' for item in report['requirements']):
        report.update(status='passed', qualification=True)
    return report


def test_three_board_acceptance_preserves_missing_hardware(inputs):
    plan = inputs[1]
    boards = ('apollo3p_evb', 'apollo330mP_evb', 'apollo510_evb')
    report = report_for(plan, boards)
    verify_selected(report, plan, boards)
    assert report['qualification'] is False
    assert any(item['status']=='incomplete' for item in report['requirements'])


def test_complete_report_acceptance_keeps_original_requirement_floor(inputs):
    plan = inputs[1]
    report = report_for(plan, AVAILABLE_BOARDS)
    verify_selected(report, plan, AVAILABLE_BOARDS)
    assert report['qualification'] is True and report['status'] == 'passed'
    assert len(report['requirements']) == len(initial_report(plan)['requirements'])


@pytest.mark.parametrize('change', ['side', 'case', 'compiler', 'gate', 'error', 'status',
                                   'qualification', 'revision', 'exclusions', 'profile'])
def test_five_board_names_cannot_override_missing_or_changed_obligations(inputs, change):
    plan = inputs[1]
    report = report_for(plan, AVAILABLE_BOARDS)
    if change == 'side':
        next(i for i in report['requirements'] if i['side'] == 'baseline')['status'] = 'incomplete'
    if change == 'case':
        next(i for i in report['requirements'] if i['cases'])['cases'] = []
    if change == 'compiler':
        next(i for i in report['requirements'] if i['kind'] == 'compiler')['status'] = 'incomplete'
    if change == 'gate':
        report['requirements'][0]['status'] = 'incomplete'
    if change == 'error': report['errors'] = ['incomplete evidence']
    if change == 'status': report['status'] = 'incomplete'
    if change == 'qualification': report['qualification'] = False
    if change == 'revision': report['requirements'][0]['revision'] = 'e'*40
    if change == 'exclusions': report['exclusions'] = [{'scope': 'changed'}]
    if change == 'profile': report['profile'] = 'smaller-profile'
    with pytest.raises(ValueError):
        verify_selected(report, plan, AVAILABLE_BOARDS)


@pytest.mark.parametrize('change', ['missing','duplicate','case','selected-incomplete','extra-pass','qualification'])
def test_incomplete_or_changed_selected_report_cannot_pass(inputs, change):
    plan = inputs[1]
    report = report_for(plan, ('apollo510_evb',))
    if change=='missing':report['requirements'].pop()
    if change=='duplicate':report['requirements'].append(report['requirements'][0])
    if change=='case':report['requirements'][0]['cases']=['unexpected']
    if change=='selected-incomplete':report['requirements'][0]['status']='incomplete'
    if change=='extra-pass':
        next(i for i in report['requirements'] if i['status']=='incomplete')['status']='passed'
    if change=='qualification':report['qualification']=True
    with pytest.raises(ValueError):verify_selected(report, plan, ('apollo510_evb',))


def test_workflow_is_explicit_manual_and_keeps_failure_evidence(repo_root):
    workflow=yaml.safe_load((repo_root/'.github/workflows/sdk-hardware-qualification.yml').read_text())
    assert set(workflow.get('on',workflow.get(True)))=={'workflow_dispatch'}
    jobs=workflow['jobs']
    assert set(jobs)=={'host','boards','assessment'}
    assert jobs['boards']['strategy']['fail-fast'] is False
    assert jobs['assessment']['if']=='always()'
    assert jobs['host']['runs-on']==['self-hosted','hpx-hardware','apollo510_evb']
    for job in jobs.values():
        assert job['timeout-minutes']<70
        first=job['steps'][0]
        assert 'NOT_COMPLETED' in first['run']
        upload=next(s for s in job['steps'] if s.get('uses','').startswith('actions/upload-artifact@'))
        assert upload['if']=='always()' and upload['with']['include-hidden-files'] is True
        assert upload['with']['if-no-files-found']=='error'
        assert '${{ github.run_attempt }}' in upload['with']['name']
        assert all(p.startswith('${{ runner.temp }}/sdk-farm-') for p in upload['with']['path'].splitlines())
