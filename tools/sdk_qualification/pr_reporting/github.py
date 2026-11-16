"""GitHub control-plane reads and narrowly scoped check writes; never runs firmware."""
from datetime import datetime, timezone
import json
import time
from urllib.parse import urlencode
import urllib.request

from ..farm_artifacts import GitHub
from .models import BINDING_JOB, BOARDS, CHECK_NAME, DISPATCH_JOB, WORKFLOW, Binding, Check


def timestamp():
    return datetime.now(timezone.utc).isoformat()


class API(GitHub):
    def call(self, path: str, method: str = 'GET', value: dict | None = None) -> dict:
        request = self.request(path)
        request.method = method
        if value is not None:
            request.data = json.dumps(value).encode()
            request.add_header('Content-Type', 'application/json')
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response)

    def rows(self, path: str, key: str, **query) -> list:
        values = []
        page = 1
        while True:
            batch = self.call(path + '?' + urlencode(dict(query, per_page=100, page=page)))[key]
            values.extend(batch)
            if len(batch) < 100:
                return values
            page += 1

    def pages(self, path: str, key: str) -> list:
        return self.rows(path, key)

    def current(self):
        return self.call(f'/actions/runs/{self.run.run_id}')

    def attempt(self):
        return self.call(f'/actions/runs/{self.run.run_id}/attempts/{self.run.attempt}')

    def ancestor(self, older: str, newer: str):
        value = self.call(f'/compare/{older}...{newer}')
        if (value['status'] not in ('identical', 'ahead')
                or value['base_commit']['sha'] != older
                or value['merge_base_commit']['sha'] != older):
            raise ValueError('PR reporting requires the driver/harness in reviewed main lineage')

    def source(self, metadata: dict) -> tuple[int, str]:
        repository = self.call('')
        workflow = self.call('/actions/workflows/' + WORKFLOW.rsplit('/', 1)[1])
        if (repository['full_name'] != self.run.repository or repository['default_branch'] != 'main'
                or metadata['repository']['full_name'] != self.run.repository
                or metadata['head_repository']['full_name'] != self.run.repository
                or str(metadata['id']) != self.run.run_id
                or str(metadata['run_attempt']) != self.run.attempt
                or metadata['head_sha'] != self.run.driver or metadata['head_branch'] != 'main'
                or metadata['event'] != 'workflow_dispatch' or metadata['path'] != WORKFLOW
                or metadata['workflow_id'] != workflow['id'] or workflow['path'] != WORKFLOW):
            raise ValueError('PR reporting requires this repository\'s exact main-owned farm workflow')
        snapshot = self.call('/git/ref/heads/main')['object']['sha']
        self.ancestor(self.run.driver, snapshot)
        return workflow['id'], snapshot

    def verify_binding_source(self, binding: Binding, metadata: dict):
        workflow, current_main = self.source(metadata)
        if binding.run != self.run or workflow != binding.workflow_id:
            raise ValueError('Binding does not identify the original workflow')
        self.ancestor(binding.main_snapshot, current_main)
        self.ancestor(binding.run.driver, binding.main_snapshot)
        self.ancestor(binding.revisions.harness, binding.main_snapshot)

    def pull_request(self, number: int) -> dict:
        return self.call(f'/pulls/{number}')

    def same_head(self, binding: Binding) -> bool:
        value = self.pull_request(binding.pull_request)
        if (value['number'] != binding.pull_request
                or value['base']['repo']['full_name'] != binding.run.repository
                or value['head']['repo'] is None
                or value['head']['repo']['full_name'] != binding.run.repository):
            raise ValueError('Only explicit same-repository PR bindings are supported')
        return value['state'] == 'open' and value['head']['sha'] == binding.head

    def same_attempt(self) -> bool:
        value = self.current()
        if value['head_sha'] != self.run.driver or str(value['id']) != self.run.run_id:
            raise ValueError('Original workflow identity changed')
        return str(value['run_attempt']) == self.run.attempt

    def completed_jobs(self, metadata: dict):
        if metadata['status'] != 'completed' or metadata['conclusion'] != 'success':
            raise ValueError('Original complete workflow did not succeed')
        jobs = self.pages(f'/actions/runs/{self.run.run_id}/attempts/{self.run.attempt}/jobs', 'jobs')
        wanted = {DISPATCH_JOB, BINDING_JOB, 'SDK host checks', 'SDK selected-scope assessment',
                  *(f'SDK board ({board})' for board in BOARDS)}
        if (len(jobs) != len(wanted) or {job['name'] for job in jobs} != wanted
                or any(job['status'] != 'completed' or job['conclusion'] != 'success' for job in jobs)):
            raise ValueError('Original binding/host/board/assessment jobs did not all complete successfully')

    def find_check(self, binding: Binding) -> Check | None:
        values = self.rows(f'/commits/{binding.head}/check-runs', 'check_runs', filter='all')
        matches = [v for v in values if v.get('external_id') == binding.external_id]
        if len(matches) > 1:
            raise ValueError('Ambiguous checks for the same run/attempt; no check was selected')
        return Check.read(matches[0], binding) if matches else None

    def rediscover(self, binding: Binding) -> Check | None:
        for delay in (0, 1, 2):
            if delay:
                time.sleep(delay)
            check = self.find_check(binding)
            if check is not None:
                return check
        return None

    def pending(self, binding: Binding) -> Check:
        existing = self.find_check(binding)
        if existing is not None:
            return existing
        payload = dict(name=CHECK_NAME, head_sha=binding.head, external_id=binding.external_id,
                       status='in_progress', started_at=timestamp(), details_url=binding.run.url,
                       output={'title': 'Three-board hardware checks pending',
                               'summary': 'AP3/AP330/AP510 only. Full five-board qualification and release approval are separate.'})
        try:
            value = self.call('/check-runs', 'POST', payload)
        except OSError:
            # A timeout may have committed the write. Never blindly create again.
            found = self.rediscover(binding)
            if found is None:
                raise
            return found
        created = Check.read(value, binding)
        found = self.rediscover(binding)
        if found is None or found.id != created.id:
            raise ValueError('Created check identity is not uniquely established')
        return found

    def finish(self, binding: Binding, conclusion: str, summary: str) -> Check:
        if conclusion not in ('success', 'failure', 'cancelled'):
            raise ValueError('Unsupported check conclusion')
        check = self.find_check(binding)
        if check is None:
            raise ValueError('Pending check was not established; inspect preflight and rerun all jobs')
        output = {'title': f'AP3/AP330/AP510: {conclusion}', 'summary': summary}
        payload = dict(status='completed', conclusion=conclusion, completed_at=timestamp(),
                       details_url=binding.run.url, output=output)
        try:
            result = Check.read(self.call(f'/check-runs/{check.id}', 'PATCH', payload), binding)
        except OSError:
            result = self.rediscover(binding)
            if result is None:
                raise
        if (result.id != check.id or result.status != 'completed' or result.conclusion != conclusion
                or result.value['output']['title'] != output['title']
                or result.value['output']['summary'] != output['summary']):
            raise ValueError('Check publication was not confirmed')
        return result
