"""Artifact identity, digest and transport checks without network access."""
from dataclasses import replace
import hashlib
import io
import json
from pathlib import Path
import stat
import urllib.error
import zipfile

import pytest

from tools.sdk_qualification.farm_artifacts import Artifact, GitHub, extract, fetch, read_receipt
from tools.sdk_qualification.farm_inputs import Run


@pytest.fixture
def run():
    return Run('AmbiqAI/nsx-ambiq-sdk', '42', '2', 'd'*40, 'https://github.com')


def archive_bytes():
    stream=io.BytesIO()
    with zipfile.ZipFile(stream,'w') as zipped:
        zipped.writestr('inputs/plan.json','{}')
        zipped.writestr('evidence/firmware/.cmake/api/reply.json','generated')
        zipped.writestr('evidence/firmware/identity.h','#define BUILD_ID 123')
    return stream.getvalue()


def artifact(run, data):
    return Artifact(run,'host',123,run.artifact_name('host'),'sha256:'+hashlib.sha256(data).hexdigest())


def test_metadata_selects_exact_attempt_not_latest(run, monkeypatch):
    client=GitHub(run)
    value=dict(id=123,name=run.artifact_name('host'),digest='sha256:'+'1'*64,expired=False,
               workflow_run={'id':42,'head_sha':run.driver})
    prior=dict(value,name=replace(run,attempt='1').artifact_name('host'))
    monkeypatch.setattr(client,'pages',lambda *args:[prior,value])
    assert client.artifact('host').artifact_id==123
    monkeypatch.setattr(client,'pages',lambda *args:[prior])
    with pytest.raises(ValueError,match='exactly one'):client.artifact('host')


@pytest.mark.parametrize('change', ['expired','driver','duplicate','digest'])
def test_invalid_metadata_is_not_downloaded(run, monkeypatch, change):
    client=GitHub(run)
    value=dict(id=123,name=run.artifact_name('host'),digest='sha256:'+'1'*64,expired=False,
               workflow_run={'id':42,'head_sha':run.driver})
    if change=='expired':value['expired']=True
    if change=='driver':value['workflow_run']['head_sha']='e'*40
    if change=='digest':value['digest']='missing'
    monkeypatch.setattr(client,'pages',lambda *args:[value,value] if change=='duplicate' else [value])
    with pytest.raises(ValueError):client.artifact('host')


@pytest.mark.parametrize('bad_digest', [False,True])
def test_storage_download_has_no_api_token_and_verifies_zip_digest(run,tmp_path,monkeypatch,bad_digest):
    data=archive_bytes()
    receipt=artifact(run,data)
    if bad_digest:receipt=replace(receipt,digest='sha256:'+'0'*64)
    monkeypatch.setenv('GH_TOKEN','fixture-token')
    seen=[]
    class Opener:
        def open(self,request,timeout):
            assert request.get_header('Authorization')=='Bearer fixture-token'
            raise urllib.error.HTTPError(request.full_url,302,'redirect',{'Location':'https://storage.example/archive'},None)
    def storage(request,timeout):
        seen.append(request)
        assert request=='https://storage.example/archive'
        return io.BytesIO(data)
    monkeypatch.setattr('urllib.request.build_opener',lambda *args:Opener())
    monkeypatch.setattr('urllib.request.urlopen',storage)
    destination=tmp_path/'payload'
    if bad_digest:
        with pytest.raises(ValueError,match='published digest'):GitHub(run).download(receipt,destination)
        assert not destination.exists()
    else:
        GitHub(run).download(receipt,destination)
        assert (destination/'evidence/firmware/.cmake/api/reply.json').read_text()=='generated'
        assert (destination/'evidence/firmware/identity.h').read_text()=='#define BUILD_ID 123'
    assert len(seen)==1


@pytest.mark.parametrize('name', ['../escape','/absolute','a/../escape','a\\escape'])
def test_escaping_members_are_rejected(tmp_path,name):
    archive=tmp_path/'artifact.zip'
    with zipfile.ZipFile(archive,'w') as zipped:zipped.writestr(name,'bad')
    with pytest.raises(ValueError):extract(archive,tmp_path/'payload')
    assert not (tmp_path/'escape').exists()


def test_symlink_member_is_rejected(tmp_path):
    archive=tmp_path/'artifact.zip'
    entry=zipfile.ZipInfo('link')
    entry.create_system=3
    entry.external_attr=(stat.S_IFLNK|0o777)<<16
    with zipfile.ZipFile(archive,'w') as zipped:zipped.writestr(entry,'target')
    with pytest.raises(ValueError):extract(archive,tmp_path/'payload')


def test_receipt_cannot_be_reused_across_attempts(run,tmp_path):
    receipt=artifact(run,archive_bytes())
    path=tmp_path/'receipt.json'
    path.write_text(json.dumps(receipt.to_dict()))
    with pytest.raises(ValueError,match='rerun all jobs'):
        read_receipt(path,replace(run,attempt='3'),'host')


@pytest.mark.parametrize('conclusion', ['failure','cancelled','skipped',None])
def test_uploaded_artifact_is_not_a_successful_job(run,monkeypatch,conclusion):
    jobs=[{'name':'SDK host checks','status':'completed','conclusion':'success'},
          {'name':'SDK board (apollo510_evb)','status':'completed','conclusion':conclusion}]
    client=GitHub(run)
    monkeypatch.setattr(client,'pages',lambda *args:jobs)
    with pytest.raises(ValueError,match='did not complete'):
        client.require_successful_jobs(('apollo510_evb',))


def test_missing_peer_job_is_not_hidden_by_successful_peer(run,monkeypatch):
    jobs=[{'name':'SDK host checks','status':'completed','conclusion':'success'},
          {'name':'SDK board (apollo510_evb)','status':'completed','conclusion':'success'}]
    client=GitHub(run)
    monkeypatch.setattr(client,'pages',lambda *args:jobs)
    with pytest.raises(ValueError,match='apollo3p_evb'):
        client.require_successful_jobs(('apollo510_evb','apollo3p_evb'))
