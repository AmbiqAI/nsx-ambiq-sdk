"""Read-only SDK snapshot inventory and explicit patch-disposition review."""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import yaml


PAYLOAD = Path('modules/nsx-ambiqsuite/sdk')


def _intake():
    path = Path(__file__).resolve().parents[1] / 'sdk-intake/intake_workflow.py'
    spec = importlib.util.spec_from_file_location('nsx_review_intake', path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


intake = _intake()


@dataclass(frozen=True)
class FileInput:
    sha256: str
    mode: str


@dataclass(frozen=True)
class FileChange:
    path: str
    category: str
    change: str
    before_sha256: str | None
    after_sha256: str | None
    before_mode: str | None
    after_mode: str | None


@dataclass(frozen=True)
class Patch:
    path: str
    sha256: str
    metadata_sha256: str
    owner: str
    reason: str
    upstream_ref: str | None


@dataclass(frozen=True)
class Snapshot:
    root: Path
    revision: str
    upstream_commit: str
    files: dict[str, FileInput]
    archives: tuple[str, ...]
    patches: dict[str, Patch]

    def identity(self) -> dict:
        return {'root': str(self.root), 'revision': self.revision,
                'upstream_commit': self.upstream_commit,
                'inventory_sha256': hashlib.sha256(
                    json.dumps({path: asdict(value) for path, value in self.files.items()}, sort_keys=True).encode()).hexdigest()}


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(['git', '-C', str(root), *args], text=True, timeout=30)


def clean_revision(root: Path) -> str:
    if Path(git(root, 'rev-parse', '--show-toplevel').strip()).resolve() != root:
        raise ValueError(f'Expected a checkout root: {root}')
    if git(root, 'status', '--porcelain', '--untracked-files=all').strip():
        raise ValueError(f'Commit or discard changes before reviewing {root}')
    return git(root, 'rev-parse', 'HEAD').strip()


def category(path: str) -> str:
    if path.startswith('sdk-intake/patches/'):
        return 'patch'
    if path.endswith('.a') and path.startswith(f'{PAYLOAD}/'):
        return 'archive'
    if path.endswith(('.h', '.hpp', '.inc')):
        return 'header'  # File-level inventory, not a semantic API/ABI comparison.
    if path.startswith(('boards/', 'cmake/')) or path.endswith(('.cmake', '.yaml', '.yml')) or Path(path).name == 'CMakeLists.txt':
        return 'configuration'
    if path.startswith(('tests/', 'tools/', '.github/')):
        return 'validation'
    return 'source-or-other'


def verify_patch_queue(root: Path, queue: list, files: dict[str, FileInput]) -> None:
    """Reconstruct the pre-patch tree and replay the queue only in scratch space."""
    with tempfile.TemporaryDirectory(prefix='nsx-patch-review-') as name:
        scratch = Path(name) / 'sdk'
        scratch.mkdir()
        targets = {path for patch in queue for path in intake._patch_target_paths(patch.patch_path)}
        for relative in targets:
            source = root / PAYLOAD / relative
            if source.is_file():
                if (PAYLOAD / relative).as_posix() not in files:
                    raise ValueError(f'Patch target must be tracked: {source}')
                target = scratch / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, target)
        for patch in reversed(queue):
            result = subprocess.run(
                ['git', '-C', str(scratch), 'apply', '--reverse', '--verbose', str(patch.patch_path)],
                env=intake._no_repo_discovery_env(scratch), capture_output=True, text=True, timeout=30)
            if result.returncode or 'Skipped patch' in result.stdout + result.stderr:
                raise ValueError(f'Patch not present in {root}: {patch.patch_path.name}: {result.stderr.strip()}')
        for patch in queue:
            intake._git_apply(scratch, patch.patch_path, owner=patch.owner)


def snapshot(root: Path) -> Snapshot:
    root = root.resolve()
    revision = clean_revision(root)
    files = {}
    for entry in git(root, 'ls-files', '--stage', '-z').split('\0'):
        if not entry:
            continue
        metadata, relative = entry.split('\t', 1)
        mode = metadata.split()[0]
        path = root / relative
        if path.is_symlink() or not path.is_file() or not path.resolve().is_relative_to(root):
            raise ValueError(f'Expected a regular tracked file: {path}')
        files[relative] = FileInput(intake.bas.sha256(path), mode)
    manifest_path = root / PAYLOAD / 'artifact-manifest.yaml'
    if str(PAYLOAD / 'artifact-manifest.yaml') not in files:
        raise ValueError(f'Missing tracked artifact manifest: {manifest_path}')
    manifest = yaml.safe_load(manifest_path.read_text())
    sdk = manifest.get('sdk', {}) if isinstance(manifest, dict) else {}
    commit = sdk.get('source_commit') if isinstance(sdk, dict) else None
    if not isinstance(commit, str) or not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError(f'{manifest_path}: record an immutable upstream source_commit before review')
    artifacts = intake.verify_artifact_hashes(root / PAYLOAD)
    artifacts.raise_if_not_ok(label=str(root))
    archives = tuple(sorted(artifacts.verified))
    actual_archives = {str(Path(path).relative_to(PAYLOAD)) for path in files
                       if path.startswith(f'{PAYLOAD}/') and path.endswith('.a')}
    if set(archives) != actual_archives or len(archives) != len(set(archives)):
        raise ValueError(f'{root}: manifest must declare every tracked archive exactly once')
    patches = {}
    patch_root = root / 'sdk-intake/patches'
    for train in sorted(patch_root.iterdir()) if patch_root.is_dir() else ():
        if not train.is_dir():
            continue
        queue = intake.load_patch_queue(train)
        for patch in queue:
            relative = patch.patch_path.relative_to(root).as_posix()
            metadata = patch.metadata_path.relative_to(root).as_posix()
            if relative not in files or metadata not in files:
                raise ValueError(f'Patch and sidecar must be tracked: {relative}')
            patches[relative] = Patch(relative, files[relative].sha256, files[metadata].sha256,
                                      patch.owner, patch.reason, patch.upstream_ref)
        verify_patch_queue(root, queue, files)
    if clean_revision(root) != revision:
        raise ValueError(f'Checkout changed during inventory: {root}')
    return Snapshot(root, revision, commit, files, archives, patches)


def expected_action(before: Patch | None, after: Patch | None) -> str:
    if before is None:
        return 'add'
    if after is None:
        return 'retire'
    return 'retain' if before == after else 'update'


def review(baseline: Path, candidate: Path, decisions: dict | None = None) -> dict:
    before, after = snapshot(baseline), snapshot(candidate)
    missing = set(before.archives) - set(after.archives)
    if missing:
        raise ValueError('Candidate removed required baseline archives: ' + ', '.join(sorted(missing)))
    changes = [FileChange(path, category(path),
                          'added' if path not in before.files else 'removed' if path not in after.files else 'modified',
                          before.files[path].sha256 if path in before.files else None,
                          after.files[path].sha256 if path in after.files else None,
                          before.files[path].mode if path in before.files else None,
                          after.files[path].mode if path in after.files else None)
               for path in sorted(before.files.keys() | after.files.keys())
               if before.files.get(path) != after.files.get(path)]
    patches = {path: {'before': asdict(before.patches[path]) if path in before.patches else None,
                      'after': asdict(after.patches[path]) if path in after.patches else None,
                      'expected_action': expected_action(before.patches.get(path), after.patches.get(path))}
               for path in sorted(before.patches.keys() | after.patches.keys())}
    report = {'schema_version': 1, 'status': 'needs-review',
              'baseline': before.identity(), 'candidate': after.identity(),
              'different_upstream_commit': before.upstream_commit != after.upstream_commit,
              'changes': [asdict(change) for change in changes], 'patches': patches,
              'verified_archive_counts': {'baseline': len(before.archives), 'candidate': len(after.archives)},
              'qualification_complete': False}
    report['decision_template'] = {
        'schema_version': 1, 'reviewer': '',
        **{name: {key: snap.identity()[key] for key in ('revision', 'inventory_sha256')}
           for name, snap in (('baseline', before), ('candidate', after))},
        'patches': {path: {'action': entry['expected_action'], 'reason': '', 'regression_tests': []}
                    for path, entry in patches.items()}}
    if decisions is not None:
        validate_decisions(decisions, before, after, patches)
        report['decisions'] = decisions
        report['status'] = 'review-recorded'
    return report


def validate_decisions(decisions: dict, before: Snapshot, after: Snapshot, patches: dict) -> None:
    if not isinstance(decisions, dict) or decisions.get('schema_version') != 1:
        raise ValueError('Decisions require schema_version: 1')
    for name, snap in (('baseline', before), ('candidate', after)):
        identity = decisions.get(name)
        expected = {key: snap.identity()[key] for key in ('revision', 'inventory_sha256')}
        if identity != expected:
            raise ValueError(f'Decisions do not match the {name} revision/inventory; review the new report')
    if not isinstance(decisions.get('reviewer'), str) or not decisions['reviewer'].strip():
        raise ValueError('Decisions require a reviewer')
    records = decisions.get('patches')
    if not isinstance(records, dict) or records.keys() != patches.keys():
        raise ValueError('Decisions must cover exactly the union of baseline and candidate patches')
    for path, record in records.items():
        if not isinstance(record, dict) or record.get('action') != patches[path]['expected_action']:
            raise ValueError(f'{path}: expected action {patches[path]["expected_action"]}')
        if not isinstance(record.get('reason'), str) or not record['reason'].strip():
            raise ValueError(f'{path}: record the disposition reason')
        tests = record.get('regression_tests')
        if not isinstance(tests, list) or not tests:
            raise ValueError(f'{path}: retain regression test references, including for retired patches')
        for test in tests:
            test_path = test if isinstance(test, str) else ''
            if not test_path.startswith('tests/') or not test_path.endswith('.py') or test_path not in after.files:
                raise ValueError(f'{path}: regression test is not tracked in the candidate: {test}')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True, help='Clean baseline SDK repository root')
    parser.add_argument('--candidate', type=Path, required=True, help='Clean candidate SDK repository root')
    parser.add_argument('--output', type=Path, required=True, help='New JSON report outside both checkouts')
    parser.add_argument('--decisions', type=Path, help='Reviewed JSON decisions bound to the report identities')
    args = parser.parse_args(argv)
    try:
        output = args.output.resolve()
        if any(output.is_relative_to(root.resolve()) for root in (args.baseline, args.candidate)):
            raise ValueError('Write the report outside both input checkouts')
        if output.exists():
            raise ValueError(f'Output already exists; choose a new report path: {output}')
        decisions = json.loads(args.decisions.read_text()) if args.decisions else None
        report = review(args.baseline, args.candidate, decisions)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open('x') as stream:
            json.dump(report, stream, indent=2)
            stream.write('\n')
        print(f'{report["status"]}: {output}')
        return 0 if report['status'] == 'review-recorded' else 2
    except (ValueError, OSError, subprocess.SubprocessError, yaml.YAMLError, intake.IntakeError) as error:
        print(f'error: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
