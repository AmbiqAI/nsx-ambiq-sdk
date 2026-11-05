"""Offline provenance tests using real Git commits and patch application."""
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

import pytest
import yaml


def load(repo_root, name):
    spec = importlib.util.spec_from_file_location(name, repo_root / "sdk-intake" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


def sha(text):
    return hashlib.sha256(text.encode()).hexdigest()


@pytest.fixture
def source(tmp_path, repo_root):
    helper = load(repo_root, "prepare_source")
    repo = tmp_path / "upstream"
    repo.mkdir()
    helper.git(repo, "init", "-q")
    helper.git(repo, "config", "user.name", "Test")
    helper.git(repo, "config", "user.email", "test@example.invalid")
    (repo / "driver.h").write_text("size=1\n")
    helper.git(repo, "add", ".")
    helper.git(repo, "commit", "-qm", "upstream")
    commit = helper.git(repo, "rev-parse", "HEAD")
    patch = tmp_path / "fix.patch"
    patch.write_text("--- a/driver.h\n+++ b/driver.h\n@@ -1 +1 @@\n-size=1\n+size=2\n")
    metadata = {"schema_version": 1, "upstream_commit": commit,
                "patches": [{"path": patch.name, "sha256": sha(patch.read_text())}],
                "files": [{"path": "driver.h", "before_sha256": sha("size=1\n"),
                           "after_sha256": sha("size=2\n")}]}
    patchset = tmp_path / "patchset.json"
    patchset.write_text(json.dumps(metadata))
    return helper, repo, patchset, tmp_path / "prepared", metadata


def test_prepare_and_bind_artifacts(source, repo_root, tmp_path):
    helper, repo, patchset, output, metadata = source
    commit = helper.prepare(repo, patchset, output)
    assert helper.git(output, "rev-parse", "HEAD^") == metadata["upstream_commit"]
    assert (repo / "driver.h").read_text() == "size=1\n"
    assert (output / "driver.h").read_text() == "size=2\n"
    assert not helper.git(output, "status", "--porcelain")
    builder = load(repo_root, "build_ambiqsuite")
    artifacts = tmp_path / "artifacts"
    builder.bind_source_artifacts(artifacts, output, commit)
    (artifacts / "test.a").write_bytes(b"archive")
    builder.bind_source_artifacts(artifacts, output, commit)
    with pytest.raises(ValueError, match="prepared git source"):
        builder.bind_source_artifacts(artifacts, output, metadata["upstream_commit"])
    (output / "driver.h").write_text("modified\n")
    with pytest.raises(ValueError, match="patched source changed"):
        builder.source_patch_provenance(output)


@pytest.mark.parametrize("failure", ["hash", "preimage", "postimage", "extra-path", "existing", "upstream"])
def test_prepare_refuses_invalid_inputs(source, failure):
    helper, repo, patchset, output, data = source
    if failure == "hash":
        data["patches"][0]["sha256"] = "0" * 64
    elif failure in ("preimage", "postimage"):
        key = "before_sha256" if failure == "preimage" else "after_sha256"
        data["files"][0][key] = "0" * 64
    elif failure == "extra-path":
        patch = patchset.parent / "fix.patch"
        patch.write_text(patch.read_text() + "--- /dev/null\n+++ b/extra.h\n@@ -0,0 +1 @@\n+extra\n")
        data["patches"][0]["sha256"] = sha(patch.read_text())
    elif failure == "existing":
        output.mkdir()
    else:
        data["upstream_commit"] = "HEAD"
    patchset.write_text(json.dumps(data))
    with pytest.raises(ValueError):
        helper.prepare(repo, patchset, output)
    if (output / ".git").exists():
        assert helper.git(output, "rev-parse", "HEAD") == helper.git(repo, "rev-parse", "HEAD")
    assert (repo / "driver.h").read_text() == "size=1\n"


def test_stale_archives_and_receipt_rejected(source, repo_root, tmp_path):
    helper, repo, patchset, output, _ = source
    commit = helper.prepare(repo, patchset, output)
    builder = load(repo_root, "build_ambiqsuite")
    artifacts = tmp_path / "artifacts"
    artifacts.mkdir()
    (artifacts / "stale.a").write_bytes(b"old")
    with pytest.raises(ValueError, match="no prepared-source provenance"):
        builder.bind_source_artifacts(artifacts, output, commit)
    (output / helper.RECEIPT).write_text("{}")
    with pytest.raises(ValueError, match="differs from committed"):
        builder.source_patch_provenance(output)


def test_stage_checks_source_before_copy(source, repo_root, tmp_path, monkeypatch):
    helper, repo, patchset, output, data = source
    commit = helper.prepare(repo, patchset, output)
    workflow = load(repo_root, "intake_workflow")
    artifacts = tmp_path / "artifacts"
    workflow.bas.bind_source_artifacts(artifacts, output, commit)
    manifest = {"sdk": {"source_commit": data["upstream_commit"],
                         "source_patch_provenance": data}}
    (artifacts / "manifest.yaml").write_text(yaml.safe_dump(manifest))
    monkeypatch.setattr(workflow, "verify_generated_boundary", lambda train: None)
    monkeypatch.setattr(workflow.bas, "artifact_root", lambda *args: artifacts)
    with pytest.raises(workflow.IntakeVerificationError, match="staging source differs"):
        workflow.stage_provider_payload(workflow.bas.TRAINS["stable"], "test", output, patches_dir=None)
    manifest["sdk"]["source_commit"] = commit
    (artifacts / "manifest.yaml").write_text(yaml.safe_dump(manifest))
    monkeypatch.setattr(workflow.bas, "missing_artifact_libraries", lambda *args: ["missing.a"])
    with pytest.raises(workflow.IntakeVerificationError, match="incomplete artifact set"):
        workflow.stage_provider_payload(workflow.bas.TRAINS["stable"], "test", output, patches_dir=None)
    (artifacts / "source-patches.json").unlink()
    with pytest.raises(workflow.IntakeVerificationError, match="provenance is missing"):
        workflow.stage_provider_payload(workflow.bas.TRAINS["stable"], "test", output, patches_dir=None)


def test_deleted_receipt_cannot_downgrade_prepared_source(source, repo_root, tmp_path):
    helper, repo, patchset, output, _ = source
    commit = helper.prepare(repo, patchset, output)
    (output / helper.RECEIPT).unlink()
    builder = load(repo_root, "build_ambiqsuite")
    with pytest.raises(ValueError, match="receipt is missing"):
        builder.bind_source_artifacts(tmp_path / "new-artifacts", output, commit)


def test_prepare_rejects_ignored_patch_additions(source):
    helper, repo, patchset, output, data = source
    (repo / ".gitignore").write_text("extra.h\n")
    helper.git(repo, "add", ".gitignore")
    helper.git(repo, "commit", "-qm", "ignore generated header")
    data["upstream_commit"] = helper.git(repo, "rev-parse", "HEAD")
    patch = patchset.parent / "fix.patch"
    patch.write_text(patch.read_text() + "--- /dev/null\n+++ b/extra.h\n@@ -0,0 +1 @@\n+extra\n")
    data["patches"][0]["sha256"] = sha(patch.read_text())
    patchset.write_text(json.dumps(data))
    with pytest.raises(ValueError, match="undeclared files"):
        helper.prepare(repo, patchset, output)


def test_bind_rejects_unpatched_tracked_edits(source, repo_root, tmp_path):
    helper, repo, patchset, output, data = source
    (repo / "driver.c").write_text("original\n")
    helper.git(repo, "add", "driver.c")
    helper.git(repo, "commit", "-qm", "driver source")
    data["upstream_commit"] = helper.git(repo, "rev-parse", "HEAD")
    patchset.write_text(json.dumps(data))
    commit = helper.prepare(repo, patchset, output)
    builder = load(repo_root, "build_ambiqsuite")
    artifacts = tmp_path / "artifacts"
    builder.bind_source_artifacts(artifacts, output, commit)
    (output / "driver.c").write_text("modified\n")
    with pytest.raises(ValueError, match="unexpected tracked changes"):
        builder.bind_source_artifacts(artifacts, output, commit)


def test_provenance_allows_only_exact_build_adaptations(source, repo_root):
    helper, repo, patchset, output, data = source
    builder = load(repo_root, "build_ambiqsuite")
    boards = {b.name: b for t in builder.TRAINS.values() for b in t.boards
              if b.inject_gpio_pincfg_shim or b.strip_display_include}
    for name in boards:
        header = repo / "boards" / name / "bsp" / "am_bsp.h"
        header.parent.mkdir(parents=True)
        header.write_text('#include "am_devices.h"\n#include "am_devices_display_generic.h"\n')
    helper.git(repo, "add", ".")
    helper.git(repo, "commit", "-qm", "board fixtures")
    data["upstream_commit"] = helper.git(repo, "rev-parse", "HEAD")
    patchset.write_text(json.dumps(data))
    helper.prepare(repo, patchset, output)
    for name, board in boards.items():
        header = output / "boards" / name / "bsp" / "am_bsp.h"
        if board.inject_gpio_pincfg_shim:
            builder.inject_gpio_pincfg_shim(header)
        if board.strip_display_include:
            builder.strip_display_include(header)
    assert builder.source_patch_provenance(output) == data
    header.write_text(header.read_text() + "unrelated edit\n")
    with pytest.raises(ValueError, match="unexpected tracked changes"):
        builder.source_patch_provenance(output)


def test_provenance_rejects_amended_extra_source(source, repo_root):
    helper, repo, patchset, output, _ = source
    helper.prepare(repo, patchset, output)
    (output / "extra.c").write_text("extra source\n")
    helper.git(output, "add", "extra.c")
    helper.git(output, "commit", "--amend", "--no-edit")
    builder = load(repo_root, "build_ambiqsuite")
    with pytest.raises(ValueError, match="undeclared source changes"):
        builder.source_patch_provenance(output)


def test_fresh_staging_matches_build_adaptations(repo_root, tmp_path, monkeypatch):
    builder = load(repo_root, "build_ambiqsuite")
    monkeypatch.setattr(builder, "ensure_bsp_pins", lambda *args: None)
    monkeypatch.setattr(builder, "ensure_generated_makefile", lambda *args: None)
    def generate_gpio(sdk_root, bsp_dir):
        (bsp_dir / "am_bsp_gpio.h").write_text("generated GPIO declarations\n")
    monkeypatch.setattr(builder, "ensure_bsp_gpio_header", generate_gpio)
    source = tmp_path / "source"
    first, second = tmp_path / "first", tmp_path / "second"
    train = builder.TRAINS["stable"]
    for board in train.boards:
        if not (board.inject_gpio_pincfg_shim or board.strip_display_include):
            continue
        header = source / "boards" / board.name / "bsp" / "am_bsp.h"
        header.parent.mkdir(parents=True)
        header.write_text('#include "am_devices.h"\n#include "am_devices_display_generic.h"\n')
        builder.promote_board_headers(train, "test", source, board, destination_root=first)
        if board.inject_gpio_pincfg_shim:
            assert builder.GPIO_PINCFG_SHIM in header.read_text()
        if board.strip_display_include:
            assert "am_devices_display_generic.h" not in header.read_text()
        builder.promote_board_headers(train, "test", source, board, destination_root=second)
        relative = header.relative_to(source)
        assert (first / relative.parent / "am_bsp_gpio.h").read_text() == "generated GPIO declarations\n"
        assert (first / relative).read_bytes() == (second / relative).read_bytes()


def test_provenance_checks_committed_postimage(source, repo_root):
    helper, repo, patchset, output, _ = source
    helper.prepare(repo, patchset, output)
    header = output / "driver.h"
    header.write_text("size=3\n")
    helper.git(output, "add", "driver.h")
    helper.git(output, "commit", "--amend", "--no-edit")
    header.write_text("size=2\n")
    builder = load(repo_root, "build_ambiqsuite")
    with pytest.raises(ValueError, match="committed source postimage"):
        builder.source_patch_provenance(output)
