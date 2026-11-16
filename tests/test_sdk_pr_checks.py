"""Check writes are isolated by original head/run/attempt and never blindly retried."""
from copy import deepcopy
import io
import json

import pytest

from pr_reporting_fakes import farm
from tools.sdk_qualification.pr_reporting.github import API


def test_pending_and_completed_rediscovery_does_not_create_another_check(farm):
    first=farm.api.pending(farm.binding)
    second=farm.api.pending(farm.binding)
    assert first.id==second.id and len(farm.api.checks)==1
    farm.api.finish(farm.binding,'success','proved selected scope')
    assert farm.api.pending(farm.binding).status=='completed'
    assert len([w for w in farm.api.writes if w[0]=='POST'])==1


def test_lost_create_response_is_rediscovered_without_second_post(farm):
    farm.api.lost_create=True
    check=farm.api.pending(farm.binding)
    assert check.id==100 and len(farm.api.writes)==1 and len(farm.api.checks)==1


def test_failed_create_never_blindly_retries(farm):
    farm.api.fail_write=True
    with pytest.raises(TimeoutError):farm.api.pending(farm.binding)
    assert len(farm.api.writes)==1 and farm.api.checks==[]


def test_lost_update_response_is_confirmed_from_the_exact_check(farm):
    check=farm.api.pending(farm.binding)
    farm.api.lost_update=True
    result=farm.api.finish(farm.binding,'failure','missing evidence')
    assert result.id==check.id and result.conclusion=='failure'
    assert len([w for w in farm.api.writes if w[0]=='PATCH'])==1


def test_unconfirmed_update_is_not_reported_success(farm):
    farm.api.pending(farm.binding)
    farm.api.fail_write=True
    with pytest.raises(ValueError,match='not confirmed'):
        farm.api.finish(farm.binding,'success','proof')
    assert farm.api.checks[0]['status']=='in_progress'
    assert len([w for w in farm.api.writes if w[0]=='PATCH'])==1


@pytest.mark.parametrize('change', ['duplicate','name','head','app','id'])
def test_ambiguous_or_foreign_check_is_never_selected(farm,change):
    farm.api.pending(farm.binding)
    if change=='duplicate':farm.api.checks.append(deepcopy(farm.api.checks[0]))
    if change=='name':farm.api.checks[0]['name']='other check'
    if change=='head':farm.api.checks[0]['head_sha']='f'*40
    if change=='app':farm.api.checks[0]['app']['slug']='another-app'
    if change=='id':farm.api.checks[0]['id']=True
    with pytest.raises(ValueError):farm.api.finish(farm.binding,'success','proof')
    assert not any(w[0]=='PATCH' for w in farm.api.writes)


def test_missing_pending_check_cannot_be_reconstructed_as_success(farm):
    with pytest.raises(ValueError,match='not established'):
        farm.api.finish(farm.binding,'success','proof')
    assert not farm.api.writes


def test_real_http_adapter_uses_json_and_preserves_read_query_parameters(farm,monkeypatch):
    monkeypatch.setenv('GH_TOKEN','fixture-token')
    calls=[]
    def request(value,timeout):
        calls.append(value)
        assert value.get_header('Authorization')=='Bearer fixture-token' and timeout==30
        if value.method=='POST':
            assert json.loads(value.data)=={'test':'payload'}
            return io.BytesIO(b'{"id":123}')
        return io.BytesIO(b'{"check_runs":[]}')
    monkeypatch.setattr('urllib.request.urlopen',request)
    api=API(farm.run)
    assert api.call('/check-runs','POST',{'test':'payload'})=={'id':123}
    assert api.rows('/commits/'+'b'*40+'/check-runs','check_runs',filter='all')==[]
    assert 'filter=all&per_page=100&page=1' in calls[-1].full_url
