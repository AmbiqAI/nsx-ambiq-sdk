"""Successful staging binds promotion to the exact validated output bytes."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
from types import SimpleNamespace

import pytest
import yaml


@pytest.fixture
def flow(repo_root, tmp_path, monkeypatch):
    spec = importlib.util.spec_from_file_location("snapshot_workflow", repo_root / "sdk-intake/intake_workflow.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    root = tmp_path / "repo"
    root.mkdir()
    for owner in (module, module.bas, module.internal_markers):
        monkeypatch.setattr(owner, "repo_root", lambda: root)
    provider = root / "modules/nsx-ambiqsuite/sdk"
    provider.mkdir(parents=True)
    (provider / "original.txt").write_text("original")
    intake = root / "sdk-intake"
    intake.mkdir()
    baseline = intake / "internal-marker-baseline.yaml"
    baseline.write_text("version: 1\nfiles: {}\nunparseable_fences: []\n")
    replay = intake / "sanitation-replay.json"
    replay.write_text(json.dumps({"schema_version": 1, "source_commit": "0" * 40, "files": {}}))
    (root / "release").mkdir()
    (root / "release/source-ownership.yaml").write_text(yaml.safe_dump({"entries": [{
        "id": "generated-ambiqsuite-provider", "classification": "upstream-derived-generated",
        "generated": True, "direct_edit": "forbidden", "paths": ["modules/nsx-ambiqsuite/sdk"],
    }]}))
    source = tmp_path / "source"
    source.mkdir()
    (source / "ordinary.h").write_text("#define ORDINARY 1\n")
    archive = source / "lib/gcc/apollo2/libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"miniature archive")
    (source / "artifact-manifest.yaml").write_text(yaml.safe_dump({"parts": [{
        "logical_skew": "apollo2", "hal_artifacts": {"gcc": {
            "path": "gcc/lib/apollo2/libam_hal.a", "sha256": hashlib.sha256(archive.read_bytes()).hexdigest(),
        }},
    }]}))
    monkeypatch.setattr(module.bas, "provider_sdk_root", lambda train: provider)
    monkeypatch.setattr(module.bas, "missing_artifact_libraries", lambda train, version: [])
    monkeypatch.setattr(module.bas, "built_artifact_toolchains", lambda train, version: ["gcc"])

    def export(train, version, sdk_root, *, destination_root):
        if destination_root.exists():
            shutil.rmtree(destination_root)
        shutil.copytree(sdk_root, destination_root)

    monkeypatch.setattr(module.bas, "promote_provider_payload", export)
    train = module.bas.TRAINS["stable"]

    def stage():
        return module.stage_provider_payload(train, "test-version", source, patches_dir=None).staged_root

    return SimpleNamespace(helper=module, root=root, source=source, provider=provider,
                           baseline=baseline, replay=replay, train=train, stage=stage)


def assert_untouched(flow):
    assert {p.name for p in flow.provider.iterdir()} == {"original.txt"}
    assert (flow.provider / "original.txt").read_text() == "original"
    assert not flow.provider.with_name("sdk.promote-tmp").exists()
    assert not flow.provider.with_name("sdk.promote-backup").exists()


def test_actual_stage_to_promote(flow):
    staged = flow.stage()
    h = flow.helper
    snapshot = h.stage_snapshot.load(staged)
    assert snapshot.files == h.stage_snapshot.capture_files(flow.source)
    assert snapshot.provider_path == "modules/nsx-ambiqsuite/sdk"
    h.promote_from_staging(staged, flow.provider, confirm=True)
    assert h.stage_snapshot.capture_files(flow.provider) == snapshot.files
    assert not h.stage_snapshot.receipt_path(flow.provider).exists()


@pytest.mark.parametrize("change", ["add", "edit", "delete", "manifest", "archive"])
def test_payload_drift_refused(flow, change):
    staged = flow.stage()
    if change == "add":
        (staged / "added.h").write_text("added")
    elif change == "delete":
        (staged / "ordinary.h").unlink()
    else:
        name = {"edit": "ordinary.h", "manifest": "artifact-manifest.yaml",
                "archive": "lib/gcc/apollo2/libam_hal.a"}[change]
        with (staged / name).open("ab") as stream:
            stream.write(b"\nchanged")
    with pytest.raises(flow.helper.IntakeError, match="receipt"):
        flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("fault", ["missing", "malformed", "duplicate", "schema", "target", "policy", "unsafe-path", "bad-hash"])
def test_invalid_receipt_refused(flow, fault):
    staged = flow.stage()
    receipt = flow.helper.stage_snapshot.receipt_path(staged)
    data = json.loads(receipt.read_text())
    if fault == "missing":
        receipt.unlink()
    elif fault == "malformed":
        receipt.write_text("{")
    elif fault == "duplicate":
        receipt.write_text('{"schema_version": 1,' + receipt.read_text()[1:])
    else:
        if fault == "schema":
            data["schema_version"] = 2
        elif fault == "target":
            data["provider_path"] = "modules/other/sdk"
        elif fault == "policy":
            data["policy"]["sanitation-replay.json"] = "0" * 64
        elif fault == "unsafe-path":
            data["files"]["../outside"] = "0" * 64
        else:
            data["files"]["ordinary.h"] = "bad"
        receipt.write_text(json.dumps(data))
    with pytest.raises(flow.helper.IntakeError):
        flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("failure", ["source", "artifacts"])
def test_failed_restage_invalidates_previous_success(flow, monkeypatch, failure):
    staged = flow.stage()
    h = flow.helper
    if failure == "source":
        def fail(args):
            raise FileNotFoundError("missing source")
        monkeypatch.setattr(h.bas, "resolve_source_root", fail)
        action = lambda: h._cmd_stage(argparse.Namespace(train="stable", version="test-version", patches_dir=None))
    else:
        monkeypatch.setattr(h.bas, "missing_artifact_libraries", lambda t, v: ["missing archive"])
        action = flow.stage
    with pytest.raises(h.IntakeError):
        action()
    assert not h.stage_snapshot.receipt_path(staged).exists()
    with pytest.raises(h.IntakeError, match="receipt"):
        h.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("when", ["after-stage", "during-stage", "during-promotion"])
def test_policy_change_refused(flow, monkeypatch, when):
    h = flow.helper
    if when != "during-stage":
        staged = flow.stage()
    original = h.assert_no_new_internal_markers

    def changed(*args, **kwargs):
        result = original(*args, **kwargs)
        with flow.baseline.open("a") as stream:
            stream.write("\n# policy changed\n")
        return result

    if when == "after-stage":
        changed(staged, path_prefix="modules/nsx-ambiqsuite/sdk")
    else:
        monkeypatch.setattr(h, "assert_no_new_internal_markers", changed)
    with pytest.raises(h.IntakeError, match="policies changed"):
        if when == "during-stage":
            flow.stage()
        else:
            h.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("mutate", ["copy", "source"])
def test_validates_exact_copied_tree(flow, monkeypatch, mutate):
    staged = flow.stage()
    h = flow.helper
    original = h.shutil.copytree

    def copy(src, dst, *args, **kwargs):
        result = original(src, dst, *args, **kwargs)
        if Path(src) == staged:
            ((Path(dst) if mutate == "copy" else staged) / "ordinary.h").write_text("drift")
        return result

    monkeypatch.setattr(h.shutil, "copytree", copy)
    if mutate == "copy":
        with pytest.raises(h.IntakeError, match="receipt"):
            h.promote_from_staging(staged, flow.provider, confirm=True)
        assert_untouched(flow)
    else:
        h.promote_from_staging(staged, flow.provider, confirm=True)
        assert (flow.provider / "ordinary.h").read_text() == "#define ORDINARY 1\n"
        assert (staged / "ordinary.h").read_text() == "drift"


@pytest.mark.parametrize("kind", ["symlink", "fifo"])
def test_nonregular_staged_entries_refused(flow, kind):
    staged = flow.stage()
    entry = staged / "unexpected.h"
    if kind == "symlink":
        entry.symlink_to(staged / "ordinary.h")
    else:
        os.mkfifo(entry)
    with pytest.raises(flow.helper.IntakeError):
        flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("where", ["copy", "validation"])
@pytest.mark.parametrize("error", [KeyboardInterrupt, SystemExit, OSError])
def test_interruption_cleans_owned_scratch(flow, monkeypatch, where, error):
    staged = flow.stage()
    h = flow.helper

    def fail(*args, **kwargs):
        if where == "copy":
            (Path(args[1]) / "partial").write_text("partial")
        raise error("interrupted")

    if where == "copy":
        monkeypatch.setattr(h.shutil, "copytree", fail)
    else:
        monkeypatch.setattr(h, "assert_no_new_internal_markers", fail)
    with pytest.raises(h.IntakeError if error is OSError else error):
        h.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


def test_existing_scratch_is_preserved(flow):
    staged = flow.stage()
    scratch = flow.provider.with_name("sdk.promote-tmp")
    scratch.mkdir()
    (scratch / "salvage").write_text("keep")
    with pytest.raises(flow.helper.IntakeError):
        flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    assert (scratch / "salvage").read_text() == "keep"
    assert (flow.provider / "original.txt").read_text() == "original"


def test_reviewed_malformed_file_keeps_provider_prefix_at_promotion(flow):
    malformed = "// #### INTERNAL BEGIN ####\n#define EXISTING 1\n"
    (flow.source / "ordinary.h").write_text(malformed)
    (flow.provider / "ordinary.h").write_text(malformed)
    key = "modules/nsx-ambiqsuite/sdk/ordinary.h"
    flow.baseline.write_text(yaml.safe_dump({"version": 1, "files": {key: 1}, "unparseable_fences": [key]}))
    staged = flow.stage()
    flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    assert (flow.provider / "ordinary.h").read_text() == malformed


@pytest.mark.parametrize("when", ["stage", "promotion"])
def test_payload_mutation_during_final_validation_refused(flow, monkeypatch, when):
    h = flow.helper
    if when == "promotion":
        staged = flow.stage()
    original = h.verify_artifact_hashes

    def verify(root, *args, **kwargs):
        result = original(root, *args, **kwargs)
        (root / "ordinary.h").write_text("changed during archive validation")
        return result

    monkeypatch.setattr(h, "verify_artifact_hashes", verify)
    with pytest.raises(h.IntakeError, match="receipt"):
        if when == "stage":
            flow.stage()
        else:
            h.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


@pytest.mark.parametrize("which", ["staged-root", "staged-parent", "provider-root", "receipt"])
def test_symlinked_roots_ancestors_and_receipts_refused(flow, which):
    staged = flow.stage()
    h = flow.helper
    if which == "staged-root":
        actual = staged.with_name("actual-sdk")
        staged.rename(actual)
        staged.symlink_to(actual, target_is_directory=True)
    elif which == "staged-parent":
        actual = staged.parent.with_name("actual-version")
        staged.parent.rename(actual)
        staged.parent.symlink_to(actual, target_is_directory=True)
    elif which == "provider-root":
        actual = flow.provider.with_name("actual-provider")
        flow.provider.rename(actual)
        flow.provider.symlink_to(actual, target_is_directory=True)
    else:
        receipt = h.stage_snapshot.receipt_path(staged)
        actual = receipt.with_name("actual-receipt.json")
        receipt.rename(actual)
        receipt.symlink_to(actual)
    with pytest.raises(h.IntakeError):
        h.promote_from_staging(staged, flow.provider, confirm=True)
    assert_untouched(flow)


def test_overlapping_stage_and_scratch_is_not_removed(flow):
    staged = flow.stage()
    scratch = flow.provider.with_name("sdk.promote-tmp")
    staged.rename(scratch)
    receipt = flow.helper.stage_snapshot.receipt_path(staged)
    receipt.rename(flow.helper.stage_snapshot.receipt_path(scratch))
    with pytest.raises(flow.helper.IntakeError, match="overlap"):
        flow.helper.promote_from_staging(scratch, flow.provider, confirm=True)
    assert (scratch / "ordinary.h").read_text() == "#define ORDINARY 1\n"
    assert (flow.provider / "original.txt").read_text() == "original"


@pytest.mark.parametrize("existing_provider", [True, False])
def test_interruption_after_install_restores_previous_state(flow, monkeypatch, existing_provider):
    staged = flow.stage()
    if not existing_provider:
        shutil.rmtree(flow.provider)
    tmp = flow.provider.with_name("sdk.promote-tmp")
    original = Path.rename

    def interrupted_rename(self, target):
        result = original(self, target)
        if self == tmp and Path(target) == flow.provider:
            raise KeyboardInterrupt()
        return result

    monkeypatch.setattr(Path, "rename", interrupted_rename)
    with pytest.raises(KeyboardInterrupt):
        flow.helper.promote_from_staging(staged, flow.provider, confirm=True)
    if existing_provider:
        assert_untouched(flow)
    else:
        assert not flow.provider.exists()
        assert not tmp.exists()
        assert not flow.provider.with_name("sdk.promote-backup").exists()
