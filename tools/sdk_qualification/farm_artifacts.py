"""Exact run/attempt artifact receipts and portable evidence transport."""
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import tempfile
import urllib.error
import urllib.request
import zipfile

from .farm_inputs import Run, write


@dataclass(frozen=True)
class Artifact:
    run: Run
    stage: str
    artifact_id: int
    name: str
    digest: str

    def __post_init__(self):
        if (type(self.artifact_id) is not int or self.artifact_id <= 0
                or self.name != self.run.artifact_name(self.stage)
                or not re.fullmatch('sha256:[0-9a-f]{64}', self.digest)):
            raise ValueError('Invalid run/attempt artifact receipt')

    def to_dict(self):
        return asdict(self)


def read_receipt(path: Path, run: Run, stage: str) -> Artifact:
    value = json.loads(path.read_text())
    if not isinstance(value, dict) or set(value) != {'run', 'stage', 'artifact_id', 'name', 'digest'}:
        raise ValueError('Invalid artifact receipt schema')
    artifact = Artifact(Run(**value['run']), value['stage'], value['artifact_id'], value['name'], value['digest'])
    if artifact.run != run or artifact.stage != stage:
        raise ValueError('Artifact receipt is from another run/attempt/stage; rerun all jobs')
    return artifact


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class GitHub:
    def __init__(self, run: Run):
        self.run = run
        self.base = f'https://api.github.com/repos/{run.repository}'

    def request(self, path: str):
        return urllib.request.Request(self.base + path, headers={
            'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
            'Accept': 'application/vnd.github+json', 'X-GitHub-Api-Version': '2022-11-28',
            'User-Agent': 'nsx-sdk-qualification'})

    def pages(self, path: str, key: str) -> list:
        values = []
        page = 1
        while True:
            with urllib.request.urlopen(self.request(f'{path}?per_page=100&page={page}'), timeout=30) as response:
                batch = json.load(response)[key]
            values.extend(batch)
            if len(batch) < 100:
                return values
            page += 1

    def artifact(self, stage: str) -> Artifact:
        expected = self.run.artifact_name(stage)
        values = self.pages(f'/actions/runs/{self.run.run_id}/artifacts', 'artifacts')
        matches = [value for value in values if value['name'] == expected]
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one {expected} artifact; rerun all jobs')
        value = matches[0]
        origin = value['workflow_run']
        if (value['expired'] or str(origin['id']) != self.run.run_id
                or origin['head_sha'] != self.run.driver):
            raise ValueError('Artifact workflow identity differs from dispatch')
        return Artifact(self.run, stage, value['id'], value['name'], value['digest'])

    def download(self, artifact: Artifact, destination: Path):
        # The API redirects to object storage. Never forward the API token there.
        try:
            with urllib.request.build_opener(NoRedirect).open(
                    self.request(f'/actions/artifacts/{artifact.artifact_id}/zip'), timeout=30):
                raise ValueError('Expected an artifact download redirect')
        except urllib.error.HTTPError as error:
            if error.code != 302:
                raise
            location = error.headers['Location']
        if not location.startswith('https://'):
            raise ValueError('Artifact storage must use HTTPS')
        with tempfile.TemporaryDirectory(prefix='sdk-artifact-', dir=destination.parent) as name:
            archive = Path(name) / 'artifact.zip'
            digest = hashlib.sha256()
            with urllib.request.urlopen(location, timeout=60) as response, archive.open('wb') as stream:
                for chunk in iter(lambda: response.read(1024 * 1024), b''):
                    stream.write(chunk)
                    digest.update(chunk)
            if 'sha256:' + digest.hexdigest() != artifact.digest:
                raise ValueError(f'Artifact contents differ from published digest: {artifact.name}')
            extract(archive, destination)

    def require_successful_jobs(self, boards: tuple[str, ...]):
        jobs = self.pages(f'/actions/runs/{self.run.run_id}/attempts/{self.run.attempt}/jobs', 'jobs')
        for name in ('SDK host checks', *(f'SDK board ({board})' for board in boards)):
            matches = [job for job in jobs if job['name'] == name]
            if (len(matches) != 1 or matches[0]['status'] != 'completed'
                    or matches[0]['conclusion'] != 'success'):
                raise ValueError(f'Selected job did not complete successfully in this attempt: {name}')


def extract(archive: Path, destination: Path):
    """Preserve hidden/generated evidence, refusing ambiguous or escaping members."""
    destination.mkdir(exist_ok=False)
    seen = set()
    with zipfile.ZipFile(archive) as zipped:
        for entry in zipped.infolist():
            path = PurePosixPath(entry.filename)
            mode = entry.external_attr >> 16
            if (not path.parts or path.is_absolute() or '..' in path.parts or '\\' in entry.filename
                    or stat.S_ISLNK(mode) or path in seen):
                raise ValueError('Invalid/duplicate artifact member')
            seen.add(path)
            target = destination / path
            if entry.is_dir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with zipped.open(entry) as source, target.open('xb') as output:
                    for chunk in iter(lambda: source.read(1024 * 1024), b''):
                        output.write(chunk)


def fetch(run: Run, stage: str, output: Path, client: GitHub | None = None) -> Artifact:
    output.mkdir(parents=True, exist_ok=False)
    client = client or GitHub(run)
    artifact = client.artifact(stage)
    client.download(artifact, output / 'payload')
    write(output / 'receipt.json', artifact.to_dict())
    print(f'Artifact {stage}: id={artifact.artifact_id} digest={artifact.digest}', flush=True)
    return artifact
