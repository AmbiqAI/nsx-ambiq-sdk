"""Completed-run reporting rejects missing, conflicting and altered data without executing it."""
import json

import pytest

from pr_reporting_fakes import farm
from tools.sdk_qualification.io import read_json, read_record, sha256, write_record
from tools.sdk_qualification.farm_inputs import write
from tools.sdk_qualification.pr_reporting.operations import report


@pytest.mark.parametrize('change', ['missing-board','binding-attempt','plan','decisions','lease','host',
                                   'record','file','cases','qualification','compiler','missing-side',
                                   'extra-requirement','execution-reference','assessment-state'])
def test_corrupt_or_incomplete_evidence_never_publishes_pass(farm,change):
    farm.api.pending(farm.binding)
    assessment=farm.api.payloads['assessment']
    board=farm.binding.boards[0]
    nested=assessment/'evidence/downloaded'/board
    payload=nested/'payload'
    state=payload/'evidence/state.json'
    final=assessment/'evidence/assessment/assessment.json'
    if change=='missing-board':farm.api.receipts.pop(board)
    if change=='binding-attempt':
        path=farm.api.payloads['pr-binding']/'binding.json'
        value=read_json(path);value['run']['attempt']='1';write(path,value)
    if change=='plan':
        path=payload/'inputs/plan.json'
        value=read_json(path);value['revisions']['candidate']='f'*40;write(path,value)
    if change=='decisions':write(payload/'inputs/decisions.json',{'changed':True})
    if change=='lease':
        value=read_json(state);value['lease']='retained';write(state,value)
    if change=='host':
        value=read_json(state);value['host_record_sha256']='f'*64;write(state,value)
    if change=='record':
        path=payload/'evidence/board/record.json'
        value=read_json(path);value['body']['inputs_rechecked']=False;write(path,value)
    if change=='file':
        path=payload/'evidence/board/host/host.txt';path.write_text('changed')
    if change in ('cases','qualification','compiler','missing-side','extra-requirement','execution-reference'):
        value=read_json(final)
        if change=='cases':next(i for i in value['requirements'] if i['cases'])['cases']=[]
        if change=='qualification':value.update(status='passed',qualification=True)
        if change=='compiler':next(i for i in value['requirements'] if i['kind']=='compiler')['status']='incomplete'
        if change=='missing-side':value['requirements'].pop()
        if change=='extra-requirement':value['requirements'].append(dict(value['requirements'][0],requirement='unreviewed'))
        if change=='execution-reference':value['executions'][0]['sha256']='f'*64
        write(final,value)
    if change=='assessment-state':
        path=assessment/'evidence/state.json'
        value=read_json(path);value['status']='failed';write(path,value)
    if change=='binding-attempt':
        with pytest.raises(ValueError):report(farm.event,farm.root/'report',farm.api)
        assert farm.api.checks[0]['status']=='in_progress'
    else:
        result=report(farm.event,farm.root/'report',farm.api)
        assert result.conclusion=='failure'
        assert farm.api.checks[0]['conclusion']=='failure'


@pytest.mark.parametrize('change', ['raw-incomplete','raw-failed','json-mismatch'])
def test_self_consistent_file_hashes_cannot_hide_bad_target_result(farm,change):
    farm.api.pending(farm.binding)
    board=farm.binding.boards[0]
    directory=farm.api.payloads['assessment']/'evidence/downloaded'/board/'payload/evidence/board'
    envelope=read_record(directory);body=envelope['body']
    entry=body['firmware'][0];image=directory/entry['directory']
    if change=='json-mismatch':
        value=read_json(image/'result.json');value['failed']=1;write(image/'result.json',value)
        changed='result.json'
    else:
        packet=bytearray((image/'last-result.bin').read_bytes())
        packet[8:12]=(1 if change=='raw-incomplete' else 99).to_bytes(4,'little')
        (image/'last-result.bin').write_bytes(packet)
        changed='last-result.bin'
    entry['files'][changed]=sha256(image/changed)
    body['files'][str((image/changed).relative_to(directory))]=sha256(image/changed)
    (directory/'record.json').unlink();write_record(directory,body)
    assert report(farm.event,farm.root/'report',farm.api).conclusion=='failure'


def test_privileged_reporting_does_not_start_workers_or_assess_again(farm,monkeypatch):
    def forbidden(*args,**kwargs):raise AssertionError('reporter executed worker/tool code')
    monkeypatch.setattr('subprocess.run',forbidden)
    monkeypatch.setattr('subprocess.Popen',forbidden)
    monkeypatch.setattr('tools.sdk_qualification.assessment.assess',forbidden)
    monkeypatch.setattr('tools.sdk_qualification.farm_run.aggregate',forbidden)
    farm.api.pending(farm.binding)
    result=report(farm.event,farm.root/'report',farm.api)
    assert result.conclusion=='success'
    assert json.loads((farm.root/'report/validated-scope.json').read_text())['full_qualification'] is False


@pytest.mark.parametrize('change', ['missing','unknown','gate-board','board-host','board-other',
                                   'compiler-host','compiler-other','incomplete-owner'])
def test_passed_requirements_must_belong_to_their_verified_execution(farm,change):
    farm.api.pending(farm.binding)
    path=farm.api.payloads['assessment']/'evidence/assessment/assessment.json'
    value=read_json(path)
    host=value['executions'][0]['execution_id']
    first=value['executions'][1]['execution_id']
    other=value['executions'][2]['execution_id']
    gate=next(i for i in value['requirements'] if i['status']=='passed' and '/' not in i['requirement'])
    board=next(i for i in value['requirements'] if i['status']=='passed' and i['kind']=='hardware'
               and i['execution_id']==first)
    compiler=next(i for i in value['requirements'] if i['status']=='passed' and i['kind']=='compiler'
                  and i['execution_id']==first)
    if change=='missing':gate.pop('execution_id')
    if change=='unknown':gate['execution_id']='f'*32
    if change=='gate-board':gate['execution_id']=first
    if change=='board-host':board['execution_id']=host
    if change=='board-other':board['execution_id']=other
    if change=='compiler-host':compiler['execution_id']=host
    if change=='compiler-other':compiler['execution_id']=other
    if change=='incomplete-owner':
        next(i for i in value['requirements'] if i['status']=='incomplete')['execution_id']=first
    write(path,value)
    result=report(farm.event,farm.root/'report',farm.api)
    assert result.conclusion=='failure' and 'execution' in result.diagnostic
