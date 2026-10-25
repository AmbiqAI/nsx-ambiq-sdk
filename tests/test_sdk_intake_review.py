"""Offline intake reviews of real git snapshots, artifacts and patch queues."""
import hashlib
import json
from pathlib import Path
import shutil

import pytest
import yaml

from tools import sdk_intake_review as review


PATCH = 'sdk-intake/patches/stable/001-fix.patch'
HEADER = review.PAYLOAD / 'mcu/part/hal.h'
ARCHIVE = review.PAYLOAD / 'lib/gcc/part/libam_hal.a'
MANIFEST = review.PAYLOAD / 'artifact-manifest.yaml'
TEST = 'tests/test_adapter.py'


def write(root, relative, content):
    path = root / relative
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content)


def commit(root):
    review.git(root, 'add', '.')
    review.git(root, '-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
               '-c', 'commit.gpgsign=false', 'commit', '--allow-empty', '-m', 'fixture')


def manifest(root):
    return yaml.safe_load((root / MANIFEST).read_text())


def save_manifest(root, value):
    write(root, MANIFEST, yaml.safe_dump(value))


@pytest.fixture
def checkouts(tmp_path):
    before = tmp_path / 'baseline'
    before.mkdir()
    review.git(before, 'init')
    write(before, HEADER, '#define VALUE 2\n')
    write(before, ARCHIVE, 'archive')
    save_manifest(before, {
        'sdk': {'source_commit': 'a' * 40},
        'parts': [{'logical_skew': 'part', 'hal_artifacts': {'gcc': {
            'path': 'gcc/lib/part/libam_hal.a',
            'sha256': hashlib.sha256(b'archive').hexdigest()}}}],
        'boards': []})
    write(before, PATCH, '''diff --git a/mcu/part/hal.h b/mcu/part/hal.h
--- a/mcu/part/hal.h
+++ b/mcu/part/hal.h
@@ -1 +1 @@
-#define VALUE 1
+#define VALUE 2
''')
    write(before, Path(PATCH).with_suffix('.yaml'), 'owner: test\nreason: fix value\n')
    write(before, TEST, 'def test_value():\n    assert True\n')
    commit(before)
    after = tmp_path / 'candidate'
    shutil.copytree(before, after)
    return before, after


def decisions(report):
    return {'schema_version': 1, 'reviewer': 'fixture reviewer',
            **{name: {key: report[name][key] for key in ('revision', 'inventory_sha256')}
               for name in ('baseline', 'candidate')},
            'patches': {path: {'action': entry['expected_action'], 'reason': 'Reviewed fixture',
                               'regression_tests': [TEST]}
                        for path, entry in report['patches'].items()}}


def test_inventory_requires_review_and_does_not_modify_inputs(checkouts):
    before, after = checkouts
    write(after, 'boards/test/board.cmake', 'set(TEST ON)\n')
    write(after, review.PAYLOAD / 'new.h', '#define NEW 1\n')
    write(after, 'src/adapter.c', 'void adapter(void) {}\n')
    data = manifest(after)
    data['sdk']['source_commit'] = 'b' * 40
    save_manifest(after, data)
    commit(after)
    report = review.review(before, after)
    assert report['status'] == 'needs-review'
    with pytest.raises(ValueError, match='reviewer'):
        review.review(before, after, report['decision_template'])
    assert report['different_upstream_commit'] is True
    assert report['qualification_complete'] is False
    changes = {entry['path']: entry for entry in report['changes']}
    assert changes['boards/test/board.cmake']['category'] == 'configuration'
    assert changes[str(review.PAYLOAD / 'new.h')]['category'] == 'header'
    assert changes[str(MANIFEST)]['change'] == 'modified'
    assert report['patches'][PATCH]['expected_action'] == 'retain'
    assert review.review(before, after, decisions(report))['status'] == 'review-recorded'
    assert review.git(before, 'status', '--porcelain') == ''
    assert review.git(after, 'status', '--porcelain') == ''


@pytest.mark.parametrize('action', ['retain', 'update', 'retire', 'add'])
def test_patch_dispositions(checkouts, action):
    before, after = checkouts
    if action == 'update':
        write(after, Path(PATCH).with_suffix('.yaml'), 'owner: test\nreason: revised explanation\n')
    if action in ('retire', 'add'):
        root = after if action == 'retire' else before
        (root / PATCH).unlink()
        (root / Path(PATCH).with_suffix('.yaml')).unlink()
        commit(root)
    commit(after)
    report = review.review(before, after)
    assert report['patches'][PATCH]['expected_action'] == action
    assert review.review(before, after, decisions(report))['status'] == 'review-recorded'


@pytest.mark.parametrize('fault', ['missing', 'extra', 'wrong-action', 'missing-test', 'stale', 'no-reviewer', 'no-reason'])
def test_incomplete_or_stale_decisions_fail(checkouts, fault):
    before, after = checkouts
    record = decisions(review.review(before, after))
    if fault == 'missing':
        record['patches'] = {}
    elif fault == 'extra':
        record['patches']['unknown'] = record['patches'][PATCH]
    elif fault == 'wrong-action':
        record['patches'][PATCH]['action'] = 'retire'
    elif fault == 'missing-test':
        record['patches'][PATCH]['regression_tests'] = ['tests/removed.py']
    elif fault == 'stale':
        write(after, HEADER, '#define VALUE 2\n#define NEW 1\n')
        commit(after)
    elif fault == 'no-reviewer':
        record['reviewer'] = ''
    else:
        record['patches'][PATCH]['reason'] = ''
    with pytest.raises(ValueError):
        review.review(before, after, record)


def test_retirement_does_not_remove_regression_coverage(checkouts):
    before, after = checkouts
    (after / PATCH).unlink()
    (after / Path(PATCH).with_suffix('.yaml')).unlink()
    (after / TEST).unlink()
    commit(after)
    report = review.review(before, after)
    with pytest.raises(ValueError, match='not tracked'):
        review.review(before, after, decisions(report))


@pytest.mark.parametrize('fault', ['missing', 'hash', 'unlisted', 'duplicate', 'removed-from-manifest'])
def test_archive_failures_stop_review(checkouts, fault):
    before, after = checkouts
    if fault in ('missing', 'removed-from-manifest'):
        (after / ARCHIVE).unlink()
    elif fault == 'hash':
        write(after, ARCHIVE, 'bad')
    elif fault == 'unlisted':
        write(after, review.PAYLOAD / 'lib/gcc/extra.a', 'extra')
    if fault == 'duplicate':
        data = manifest(after)
        data['parts'] *= 2
        save_manifest(after, data)
    if fault == 'removed-from-manifest':
        data = manifest(after)
        # A replacement archive passes its own manifest but loses baseline coverage.
        data['parts'][0]['hal_artifacts']['gcc']['path'] = 'gcc/lib/other/libam_hal.a'
        write(after, review.PAYLOAD / 'lib/gcc/other/libam_hal.a', 'archive')
        save_manifest(after, data)
    commit(after)
    with pytest.raises((ValueError, review.intake.IntakeError)):
        review.review(before, after)


def test_patch_not_present_stops_review(checkouts):
    before, after = checkouts
    write(after, HEADER, '#define VALUE 1\n')
    commit(after)
    with pytest.raises(ValueError, match='Patch not present'):
        review.review(before, after)


def test_overlapping_queue_is_reversed_in_order(checkouts):
    before, after = checkouts
    write(after, 'sdk-intake/patches/stable/002-next.patch',
          (after / PATCH).read_text().replace('VALUE 2', 'VALUE 3').replace('VALUE 1', 'VALUE 2'))
    write(after, 'sdk-intake/patches/stable/002-next.yaml', 'owner: test\nreason: followup\n')
    write(after, HEADER, '#define VALUE 3\n')
    commit(after)
    assert len(review.review(before, after)['patches']) == 2


@pytest.mark.parametrize('fault', ['dirty', 'untracked', 'upstream', 'symlink'])
def test_unidentified_inputs_rejected(checkouts, fault):
    before, after = checkouts
    if fault == 'dirty':
        write(after, HEADER, 'dirty')
    elif fault == 'untracked':
        write(after, 'new.txt', 'untracked')
    elif fault == 'upstream':
        data = manifest(after)
        data['sdk']['source_commit'] = 'stable'
        save_manifest(after, data)
        commit(after)
    else:
        (after / 'link').symlink_to(HEADER)
        commit(after)
    with pytest.raises(ValueError):
        review.review(before, after)


def test_cli_exit_status_and_new_output(checkouts, tmp_path):
    before, after = checkouts
    output = tmp_path / 'report.json'
    args = ['--baseline', str(before), '--candidate', str(after), '--output', str(output)]
    assert review.main(args) == 2
    record = decisions(json.loads(output.read_text()))
    assert review.main(args) == 1  # Never replace an earlier result.
    decision_file = tmp_path / 'decisions.json'
    decision_file.write_text(json.dumps(record))
    assert review.main(args[:-1] + [str(tmp_path / 'reviewed.json'), '--decisions', str(decision_file)]) == 0
    assert review.main(args[:-1] + [str(after / 'report.json')]) == 1


def test_patch_hunk_update_is_reported(checkouts):
    before, after = checkouts
    write(after, PATCH, (after / PATCH).read_text().replace('+#define VALUE 2', '+#define VALUE 3'))
    write(after, HEADER, '#define VALUE 3\n')
    commit(after)
    report = review.review(before, after)
    assert report['patches'][PATCH]['expected_action'] == 'update'
    assert report['patches'][PATCH]['before']['sha256'] != report['patches'][PATCH]['after']['sha256']
    assert review.review(before, after, decisions(report))['status'] == 'review-recorded'


def test_removed_header_is_in_inventory(checkouts):
    before, after = checkouts
    write(before, review.PAYLOAD / 'removed.h', 'old header')
    commit(before)
    report = review.review(before, after)
    assert {'path': str(review.PAYLOAD / 'removed.h'), 'category': 'header',
            'change': 'removed', 'before_sha256': hashlib.sha256(b'old header').hexdigest(),
            'after_sha256': None, 'before_mode': '100644', 'after_mode': None} in report['changes']


def test_missing_patch_sidecar_stops_review(checkouts):
    before, after = checkouts
    (after / Path(PATCH).with_suffix('.yaml')).unlink()
    commit(after)
    with pytest.raises(review.intake.IntakeError, match='sidecar'):
        review.review(before, after)


def test_same_snapshot_is_not_update_qualification(checkouts):
    before, after = checkouts
    report = review.review(before, after)
    assert report['changes'] == []
    recorded = review.review(before, after, decisions(report))
    assert recorded['status'] == 'review-recorded'
    assert recorded['different_upstream_commit'] is False
    assert recorded['qualification_complete'] is False


def test_executable_mode_change_is_in_inventory(checkouts):
    before, after = checkouts
    (after / TEST).chmod(0o755)
    commit(after)
    changes = review.review(before, after)['changes']
    assert len(changes) == 1
    assert changes[0]['before_sha256'] == changes[0]['after_sha256']
    assert changes[0]['before_mode'] == '100644'
    assert changes[0]['after_mode'] == '100755'


def test_ignored_patch_target_cannot_stand_in_for_tracked_payload(checkouts):
    before, after = checkouts
    review.git(after, 'rm', '--cached', str(HEADER))
    write(after, '.gitignore', str(HEADER) + '\n')
    commit(after)
    with pytest.raises(ValueError, match='Patch target must be tracked'):
        review.review(before, after)


def test_snapshot_matches_git_keys_with_windows_payload_paths(checkouts, monkeypatch):
    from pathlib import PureWindowsPath

    before, after = checkouts
    monkeypatch.setattr(review, 'PAYLOAD', PureWindowsPath(review.PAYLOAD))
    report = review.review(before, after)
    assert report['changes'] == []
    assert report['verified_archive_counts'] == {'baseline': 1, 'candidate': 1}
    assert review.category(ARCHIVE.as_posix()) == 'archive'
    assert review.review(before, after, decisions(report))['status'] == 'review-recorded'
