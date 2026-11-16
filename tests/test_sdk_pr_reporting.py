"""Commit/result association controls using synthetic data and no network/hardware."""
from copy import deepcopy
from dataclasses import replace
import json

import pytest
import yaml

from pr_reporting_fakes import farm
from tools.sdk_qualification.farm_inputs import Run, write
from tools.sdk_qualification.pr_reporting import operations
from tools.sdk_qualification.pr_reporting.models import BOARDS, CHECK_NAME, read_binding


def test_binding_round_trip_keeps_independent_revisions_and_attempt(farm):
    assert read_binding(farm.binding.to_dict()) == farm.binding
    assert farm.binding.head == 'b'*40 and farm.binding.run.driver == 'd'*40
    assert farm.binding.revisions.harness == 'c'*40
    assert replace(farm.binding, run=replace(farm.run, attempt='3')).external_id != farm.binding.external_id


@pytest.mark.parametrize('change', ['schema','extra','pr-bool','pr-zero','boards','sha','hash','workflow'])
def test_invalid_binding_refused(farm, change):
    value = farm.binding.to_dict()
    if change=='schema': value['schema_version']=2
    if change=='extra': value['guessed_pr']=7
    if change=='pr-bool': value['pull_request']=True
    if change=='pr-zero': value['pull_request']=0
    if change=='boards': value['boards']=['apollo510_evb']
    if change=='sha': value['main_snapshot']='main'
    if change=='hash': value['plan_sha256']='not-a-hash'
    if change=='workflow': value['workflow_id']=0
    with pytest.raises(ValueError): read_binding(value)


def environment(farm, monkeypatch):
    for key,value in dict(GITHUB_REPOSITORY=farm.run.repository, GITHUB_RUN_ID=farm.run.run_id,
                          GITHUB_RUN_ATTEMPT=farm.run.attempt, GITHUB_SHA=farm.run.driver,
                          GITHUB_SERVER_URL=farm.run.server, SDK_PLAN_JSON=json.dumps(farm.plan.to_dict()),
                          SDK_DECISIONS_JSON='{}', SDK_BOARDS_JSON=json.dumps(BOARDS),
                          SDK_DEVICE_PACK_ARCHIVE='/pinned/pack.zip',
                          GITHUB_OUTPUT=str(farm.root/'outputs'),GITHUB_ENV=str(farm.root/'env')).items():
        monkeypatch.setenv(key,value)


def test_preflight_upload_data_does_not_create_check_or_execute_candidate(farm, monkeypatch):
    environment(farm,monkeypatch)
    output=farm.root/'preflight'
    binding=operations.preflight(output,7,farm.api)
    assert read_binding(json.loads((output/'binding.json').read_text()))==binding
    assert binding.boards==BOARDS and not farm.api.writes and not farm.api.downloads


@pytest.mark.parametrize('change', ['scope','fork','closed','head','harness','driver','branch','workflow','repo','attempt'])
def test_preflight_rejects_wrong_pr_source_or_scope_before_check(farm,monkeypatch,change):
    environment(farm,monkeypatch)
    if change=='scope': monkeypatch.setenv('SDK_BOARDS_JSON','["apollo510_evb"]')
    if change=='fork': farm.api.pr['head']['repo']['full_name']='someone/fork'
    if change=='closed': farm.api.pr['state']='closed'
    if change=='head': farm.api.pr['head']['sha']='f'*40
    if change=='harness': farm.api.diverged.add(farm.plan.revisions.harness)
    if change=='driver': farm.api.diverged.add(farm.run.driver)
    if change=='branch': farm.api.metadata['head_branch']='feature'
    if change=='workflow': farm.api.metadata['workflow_id']=99
    if change=='repo': farm.api.metadata['head_repository']['full_name']='someone/fork'
    if change=='attempt': farm.api.latest['run_attempt']=3
    output=farm.root/'rejected'
    with pytest.raises(ValueError):operations.preflight(output,7,farm.api)
    assert not (output/'binding.json').exists() and not farm.api.writes


def test_pending_requires_published_binding_not_local_mutable_outputs(farm):
    check=operations.pending(farm.root/'pending-download',farm.api)
    assert check.status=='in_progress' and farm.api.downloads==['pr-binding']
    assert farm.api.checks[0]['head_sha']==farm.binding.head


def test_report_uses_completed_upstream_not_reporter_environment(farm,monkeypatch):
    farm.api.pending(farm.binding)
    monkeypatch.setenv('GITHUB_RUN_ID','999')
    monkeypatch.setenv('GITHUB_RUN_ATTEMPT','9')
    monkeypatch.setenv('GITHUB_SHA','f'*40)
    value=operations.report(farm.event,farm.root/'report',farm.api)
    assert value.published and value.conclusion=='success'
    assert operations.upstream_run(farm.event)==farm.run
    check=farm.api.checks[0]
    assert check['name']==CHECK_NAME and check['head_sha']==farm.plan.revisions.candidate
    assert 'Full five-board qualification remains incomplete' in check['output']['summary']
    assert 'not full SDK qualification or release approval' in check['output']['summary']
    assert check['external_id']==farm.binding.external_id
    assert all(path.startswith('/check-runs') for _,path,_ in farm.api.writes)


@pytest.mark.parametrize('change', ['head','closed','attempt'])
def test_superseded_request_never_updates_new_head_or_attempt(farm,change):
    old=farm.api.pending(farm.binding)
    if change=='head':farm.api.pr['head']['sha']='f'*40
    if change=='closed':farm.api.pr['state']='closed'
    if change=='attempt':farm.api.latest['run_attempt']=3
    result=operations.report(farm.event,farm.root/'report',farm.api)
    assert result.conclusion=='cancelled' and result.check_id==old.id
    assert farm.api.checks[0]['head_sha']==farm.binding.head
    assert farm.api.writes[-1][1]==f'/check-runs/{old.id}'


@pytest.mark.parametrize('failure', ['failure','cancelled','timed_out'])
def test_final_workflow_failure_after_positive_assessment_cannot_pass(farm,failure):
    farm.api.pending(farm.binding)
    farm.api.metadata['conclusion']=farm.api.latest['conclusion']=failure
    result=operations.report(farm.event,farm.root/'report',farm.api)
    assert result.conclusion=='failure'
    assert farm.api.downloads==['pr-binding']  # No inference from earlier positive data.


@pytest.mark.parametrize('change', ['assessment','preflight','cleanup','missing-job','duplicate-job'])
def test_all_original_jobs_including_assessment_and_cleanup_are_required(farm,change):
    farm.api.pending(farm.binding)
    if change=='assessment':farm.api.jobs[2]['conclusion']='failure'
    if change=='preflight':farm.api.jobs[0]['conclusion']='cancelled'
    if change=='cleanup':farm.api.jobs[-1]['status']='in_progress'
    if change=='missing-job':farm.api.jobs.pop()
    if change=='duplicate-job':farm.api.jobs[-1]=deepcopy(farm.api.jobs[-2])
    result=operations.report(farm.event,farm.root/'report',farm.api)
    assert result.conclusion=='failure' and 'evidence could not establish' in result.diagnostic


def test_unbound_or_early_failed_run_never_guesses_pr(farm):
    farm.api.receipts.pop('pr-binding')
    result=operations.report(farm.event,farm.root/'report',farm.api)
    assert result.published is False and result.conclusion is None
    assert not farm.api.writes and not farm.api.downloads


@pytest.mark.parametrize('change', ['event','status','repository','driver','attempt-api','branch','workflow'])
def test_wrong_trigger_or_source_cannot_publish(farm,change):
    if change=='event':farm.event['action']='requested'
    if change=='status':farm.event['workflow_run']['status']='in_progress'
    if change=='repository':farm.event['repository']['full_name']='someone/else'
    if change=='driver':farm.event['workflow_run']['head_sha']='f'*40
    if change=='attempt-api':farm.api.metadata['run_attempt']=3
    if change=='branch':farm.api.metadata['head_branch']='candidate'
    if change=='workflow':farm.api.metadata['path']='.github/workflows/other.yml'
    with pytest.raises(ValueError):operations.report(farm.event,farm.root/'report',farm.api)
    assert not farm.api.writes


def test_final_head_read_occurs_after_data_validation(farm,monkeypatch):
    original=operations.validate
    def validate_then_move(*args):
        value=original(*args)
        farm.api.pr['head']['sha']='f'*40
        return value
    monkeypatch.setattr(operations,'validate',validate_then_move)
    farm.api.pending(farm.binding)
    assert operations.report(farm.event,farm.root/'report',farm.api).conclusion=='cancelled'


def test_workflow_binding_upload_precedes_check_and_writer_jobs_are_hosted(repo_root):
    manual=yaml.safe_load((repo_root/'.github/workflows/sdk-hardware-qualification.yml').read_text())
    binding=manual['jobs']['binding']
    assert binding['runs-on']=='ubuntu-latest' and binding['permissions']['checks']=='write'
    assert 'checks' not in manual['permissions']
    steps=binding['steps']
    upload=next(i for i,s in enumerate(steps) if s.get('uses','').startswith('actions/upload-artifact@'))
    assert upload < next(i for i,s in enumerate(steps) if ' pending ' in s.get('run',''))
    host=manual['jobs']['host']
    assert host['needs']=='binding' and '!cancelled()' in host['if']
    assert "inputs.pull_request_number == ''" in host['if']
    reporter=yaml.safe_load((repo_root/'.github/workflows/sdk-pr-farm-reporting.yml').read_text())
    trigger=reporter.get('on',reporter.get(True))
    assert set(trigger)=={'workflow_run'}
    assert trigger['workflow_run']['types']==['completed']
    assert trigger['workflow_run']['branches']==['main']
    job=reporter['jobs']['report']
    assert job['runs-on']=='ubuntu-latest' and job['concurrency']['cancel-in-progress'] is False
    assert job['concurrency']['group'].replace('github.event.workflow_run.id','github.run_id')==binding['concurrency']['group']
    checkout=next(s for s in job['steps'] if s.get('uses','').startswith('actions/checkout@'))
    assert checkout['with']['ref']=='${{ github.sha }}' and checkout['with']['persist-credentials'] is False
