"""Optional PR association and completed-run reporting over existing farm evidence."""
from pathlib import Path

from ..farm_artifacts import fetch
from ..farm_inputs import Run, load_dispatch, prepare, write
from ..io import read_json
from .evidence import validate
from .github import API
from .models import BINDING_STAGE, BOARDS, Binding, Publication, read_binding


def preflight(output: Path, pull_request: int, api: API | None = None) -> Binding:
    run = Run.current()
    api = api or API(run)
    output.mkdir(parents=True, exist_ok=False)
    prepare(output / 'inputs')
    dispatch, plan = load_dispatch(output / 'inputs', run)
    if dispatch.boards != BOARDS:
        raise ValueError('PR mode requires explicit AP3/AP330/AP510 selection; normal manual mode is unchanged')
    metadata = api.attempt()
    workflow, snapshot = api.source(metadata)
    api.ancestor(plan.revisions.harness, snapshot)
    binding = Binding(run, pull_request, plan.revisions, plan.digest,
                      dispatch.decisions_sha256, dispatch.boards, workflow, snapshot)
    if not api.same_attempt() or not api.same_head(binding):
        raise ValueError('PR head/attempt changed or PR is closed; dispatch its reviewed current commit')
    write(output / 'binding.json', binding.to_dict())
    return binding


def downloaded_binding(output: Path, api: API) -> Binding:
    fetch(api.run, BINDING_STAGE, output, api)
    payload = output / 'payload'
    binding = read_binding(read_json(payload / 'binding.json'))
    dispatch, plan = load_dispatch(payload / 'inputs', api.run)
    binding.verify_inputs(dispatch, plan)
    api.verify_binding_source(binding, api.attempt())
    return binding


def pending(output: Path, api: API | None = None):
    api = api or API(Run.current())
    binding = downloaded_binding(output, api)
    if not api.same_attempt() or not api.same_head(binding):
        raise ValueError('PR head/attempt changed before creating its check; rerun the reviewed scope')
    return api.pending(binding)


def upstream_run(event: dict) -> Run:
    value = event['workflow_run']
    if event['action'] != 'completed' or value['status'] != 'completed':
        raise ValueError('Only completed upstream workflow events can publish farm results')
    run = Run(event['repository']['full_name'], str(value['id']), str(value['run_attempt']),
              value['head_sha'], 'https://github.com')
    if value['repository']['full_name'] != run.repository:
        raise ValueError('Workflow event repository differs from the original run')
    return run


def report(event: dict, output: Path, api: API | None = None) -> Publication:
    run = upstream_run(event)
    api = api or API(run)
    if api.run != run:
        raise ValueError('Reporter API is not bound to the original upstream run')
    output.mkdir(parents=True, exist_ok=False)
    metadata = api.attempt()
    api.source(metadata)
    if metadata['status'] != 'completed':
        raise ValueError('Original workflow has not completed')
    artifacts = api.pages(f'/actions/runs/{run.run_id}/artifacts', 'artifacts')
    if not any(v['name'] == run.artifact_name(BINDING_STAGE) for v in artifacts):
        # Unbound manual runs and failures before upload have no proved PR target.
        return Publication(False, None, 'No proved PR binding; no PR result published. Inspect the original workflow.')
    binding = downloaded_binding(output / 'binding', api)
    write(output / 'binding-receipt.json', read_json(output / 'binding/receipt.json'))
    conclusion = 'failure'
    detail = f'Original farm workflow concluded {metadata["conclusion"]}; selected coverage is not established.'
    if metadata['conclusion'] == 'success':
        try:
            api.completed_jobs(metadata)
            result = validate(api, binding, output / 'evidence')
            write(output / 'validated-scope.json', result)
            conclusion = 'success'
            detail = (f'{result["images"]} firmware images / {result["target_case_executions"]} target case executions passed. '
                      f'Requirements: {result["requirements"]["passed"]} passed / '
                      f'{result["requirements"]["incomplete"]} incomplete. '
                      'Full five-board qualification remains incomplete: AP4 KXR and AP510B are absent.')
        except (ValueError, OSError, KeyError, TypeError, RuntimeError) as error:
            detail = f'Farm evidence could not establish selected coverage: {type(error).__name__}: {error}'
    # Read these immediately before writing; update only this binding's check ID.
    latest = api.current()
    if latest['head_sha'] != run.driver or str(latest['id']) != run.run_id:
        raise ValueError('Original workflow identity changed before publication')
    if str(latest['run_attempt']) != run.attempt or not api.same_head(binding):
        conclusion, detail = 'cancelled', 'Superseded PR head, closed PR or newer workflow attempt; this result cannot qualify the current request.'
    elif latest['status'] != 'completed' or latest['conclusion'] != metadata['conclusion']:
        raise ValueError('Final original workflow outcome is not established')
    summary = (f'**AP3 Blue Plus / AP330 Plus / AP510 only.**\n\n{detail}\n\n'
               f'Candidate `{binding.head}`; baseline `{binding.revisions.baseline}`; '
               f'harness `{binding.revisions.harness}`; driver `{run.driver}`.\n\n'
               f'Run {run.run_id}, attempt {run.attempt}; plan `{binding.plan_sha256}`. '
               f'[Original workflow and evidence]({run.url}).\n\n'
               'This scoped result is not full SDK qualification or release approval.')
    check = api.finish(binding, conclusion, summary)
    return Publication(True, conclusion, detail, check.id)
