from __future__ import annotations

import hashlib
import importlib.util
import subprocess
import sys
from dataclasses import replace
from pathlib import Path

import pytest
import yaml


def load_intake_workflow(repo_root: Path):
    module_path = repo_root / "sdk-intake" / "intake_workflow.py"
    spec = importlib.util.spec_from_file_location("test_intake_workflow_module", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _apollo2_train(helper):
    """Narrow the real `stable` train to its single legacy part/board so tests
    stay small without hand-authoring a parallel TrainSpec."""
    full = helper.bas.TRAINS["stable"]
    return helper.bas.replace(
        full,
        parts=tuple(p for p in full.parts if p.name == "apollo2"),
        boards=tuple(b for b in full.boards if b.part == "apollo2"),
    )


@pytest.fixture()
def helper(repo_root: Path):
    return load_intake_workflow(repo_root)


@pytest.fixture()
def fake_repo(tmp_path: Path, helper, monkeypatch):
    """A throwaway fake repository root wired into intake_workflow's and
    build_ambiqsuite's path-resolution functions, so tests never touch the
    real `modules/nsx-ambiqsuite/sdk/` tree."""
    fake_root = tmp_path / "fake-repo"
    fake_root.mkdir()
    monkeypatch.setattr(helper, "repo_root", lambda: fake_root)
    monkeypatch.setattr(helper.bas, "repo_root", lambda: fake_root)

    ownership_dir = fake_root / "release"
    ownership_dir.mkdir(parents=True)
    ownership = {
        "schema_version": 1,
        "entries": [
            {
                "id": "generated-ambiqsuite-provider",
                "classification": "upstream-derived-generated",
                "paths": ["modules/nsx-ambiqsuite/sdk"],
                "generated": True,
                "direct_edit": "forbidden",
            }
        ],
    }
    (ownership_dir / "source-ownership.yaml").write_text(yaml.safe_dump(ownership), encoding="utf-8")
    return fake_root


def _write_manifest(manifest_path: Path, *, toolchain: str, part: str, hal_sha256: str) -> None:
    manifest = {
        "sdk": {"provider": "ambiqsuite", "version": "test"},
        "parts": [
            {
                "logical_skew": part,
                "hal_artifacts": {
                    toolchain: {
                        "path": f"{toolchain}/lib/{part}/libam_hal.a",
                        "sha256": hal_sha256,
                    }
                },
            }
        ],
        "boards": [],
    }
    manifest_path.write_text(yaml.safe_dump(manifest), encoding="utf-8")


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


# --------------------------------------------------------------------------
# Path/boundary guard
# --------------------------------------------------------------------------
def test_assert_within_repo_accepts_nested_path(fake_repo: Path, helper) -> None:
    nested = fake_repo / "sdk-intake" / "local" / "staging"
    assert helper.assert_within_repo(nested, label="staging") == nested.resolve()


def test_assert_within_repo_rejects_escaping_path(fake_repo: Path, helper, tmp_path: Path) -> None:
    outside = tmp_path / "somewhere-else"
    outside.mkdir()
    with pytest.raises(helper.IntakeSecurityError):
        helper.assert_within_repo(outside, label="staging")


# --------------------------------------------------------------------------
# Ownership boundary verification
# --------------------------------------------------------------------------
def test_verify_generated_boundary_passes_for_declared_path(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    entry = helper.verify_generated_boundary(train)
    assert entry.direct_edit == "forbidden"


def test_verify_generated_boundary_rejects_undeclared_path(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-other" / "sdk")
    with pytest.raises(helper.IntakeSecurityError):
        helper.verify_generated_boundary(train)


def test_verify_generated_boundary_rejects_non_forbidden_direct_edit(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    ownership_path = fake_repo / "release" / "source-ownership.yaml"
    data = yaml.safe_load(ownership_path.read_text(encoding="utf-8"))
    data["entries"][0]["direct_edit"] = "allowed"
    ownership_path.write_text(yaml.safe_dump(data), encoding="utf-8")
    with pytest.raises(helper.IntakeSecurityError):
        helper.verify_generated_boundary(train)


def test_verify_generated_boundary_requires_manifest_entry(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    (fake_repo / "release" / "source-ownership.yaml").write_text(
        yaml.safe_dump({"schema_version": 1, "entries": []}), encoding="utf-8"
    )
    with pytest.raises(helper.IntakeSecurityError):
        helper.verify_generated_boundary(train)


# --------------------------------------------------------------------------
# Artifact hash verification
# --------------------------------------------------------------------------
def test_manifest_path_to_promoted_relative_maps_toolchain_and_part(helper) -> None:
    result = helper.manifest_path_to_promoted_relative("gcc/lib/apollo510/libam_hal.a")
    assert result == Path("lib/gcc/apollo510/libam_hal.a")


def test_manifest_path_to_promoted_relative_maps_board_bsp(helper) -> None:
    result = helper.manifest_path_to_promoted_relative("atfe/lib/apollo510/apollo510_evb/libam_bsp.a")
    assert result == Path("lib/atfe/apollo510/apollo510_evb/libam_bsp.a")


def test_manifest_path_to_promoted_relative_rejects_malformed_path(helper) -> None:
    with pytest.raises(helper.IntakeVerificationError):
        helper.manifest_path_to_promoted_relative("libam_hal.a")


def test_verify_artifact_hashes_reports_verified(tmp_path: Path, helper) -> None:
    sdk_root = tmp_path / "sdk"
    archive = sdk_root / "lib" / "gcc" / "apollo510" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"archive-contents")
    manifest_path = sdk_root / "artifact-manifest.yaml"
    _write_manifest(manifest_path, toolchain="gcc", part="apollo510", hal_sha256=_sha256_bytes(b"archive-contents"))

    result = helper.verify_artifact_hashes(sdk_root, manifest_path)

    assert result.ok
    assert result.verified == ("lib/gcc/apollo510/libam_hal.a",)


def test_verify_artifact_hashes_reports_mismatch(tmp_path: Path, helper) -> None:
    sdk_root = tmp_path / "sdk"
    archive = sdk_root / "lib" / "gcc" / "apollo510" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"tampered-contents")
    manifest_path = sdk_root / "artifact-manifest.yaml"
    _write_manifest(manifest_path, toolchain="gcc", part="apollo510", hal_sha256=_sha256_bytes(b"original-contents"))

    result = helper.verify_artifact_hashes(sdk_root, manifest_path)

    assert not result.ok
    assert result.mismatched == ("lib/gcc/apollo510/libam_hal.a",)
    with pytest.raises(helper.IntakeVerificationError):
        result.raise_if_not_ok(label="test")


def test_verify_artifact_hashes_reports_missing(tmp_path: Path, helper) -> None:
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    _write_manifest(manifest_path, toolchain="gcc", part="apollo510", hal_sha256="deadbeef")

    result = helper.verify_artifact_hashes(sdk_root, manifest_path)

    assert not result.ok
    assert result.missing == ("lib/gcc/apollo510/libam_hal.a",)


# --------------------------------------------------------------------------
# Reviewable diff (read-only, non-destructive)
# --------------------------------------------------------------------------
def test_compare_trees_reports_added_removed_modified(tmp_path: Path, helper) -> None:
    promoted = tmp_path / "promoted"
    staged = tmp_path / "staged"
    (promoted / "include").mkdir(parents=True)
    (staged / "include").mkdir(parents=True)

    (promoted / "include" / "keep.h").write_text("unchanged\n", encoding="utf-8")
    (staged / "include" / "keep.h").write_text("unchanged\n", encoding="utf-8")

    (promoted / "include" / "old.h").write_text("old only\n", encoding="utf-8")

    (staged / "include" / "new.h").write_text("new only\n", encoding="utf-8")

    (promoted / "include" / "changed.h").write_text("before\n", encoding="utf-8")
    (staged / "include" / "changed.h").write_text("after\n", encoding="utf-8")

    (promoted / "lib_gcc.a").write_bytes(b"before-binary")
    (staged / "lib_gcc.a").write_bytes(b"after-binary")

    diff = helper.compare_trees(staged, promoted)

    assert not diff.is_empty
    assert {e.relative_path for e in diff.added} == {"include/new.h"}
    assert {e.relative_path for e in diff.removed} == {"include/old.h"}
    modified_paths = {e.relative_path for e in diff.modified}
    assert modified_paths == {"include/changed.h", "lib_gcc.a"}

    text_entry = next(e for e in diff.modified if e.relative_path == "include/changed.h")
    assert text_entry.text_diff is not None
    assert "-before" in text_entry.text_diff
    assert "+after" in text_entry.text_diff

    binary_entry = next(e for e in diff.modified if e.relative_path == "lib_gcc.a")
    assert binary_entry.text_diff is None
    assert binary_entry.promoted_hash != binary_entry.staged_hash


def test_compare_trees_reports_no_diff_for_identical_trees(tmp_path: Path, helper) -> None:
    promoted = tmp_path / "promoted"
    staged = tmp_path / "staged"
    promoted.mkdir()
    staged.mkdir()
    (promoted / "same.h").write_text("same\n", encoding="utf-8")
    (staged / "same.h").write_text("same\n", encoding="utf-8")

    diff = helper.compare_trees(staged, promoted)

    assert diff.is_empty
    assert "no differences" in helper.render_diff_report(diff)


def test_compare_trees_does_not_write_to_either_tree(tmp_path: Path, helper) -> None:
    promoted = tmp_path / "promoted"
    staged = tmp_path / "staged"
    promoted.mkdir()
    staged.mkdir()
    (promoted / "a.h").write_text("a\n", encoding="utf-8")
    (staged / "a.h").write_text("b\n", encoding="utf-8")

    before_promoted = sorted(promoted.rglob("*"))
    before_staged = sorted(staged.rglob("*"))
    helper.compare_trees(staged, promoted)
    assert sorted(promoted.rglob("*")) == before_promoted
    assert sorted(staged.rglob("*")) == before_staged
    assert (promoted / "a.h").read_text(encoding="utf-8") == "a\n"
    assert (staged / "a.h").read_text(encoding="utf-8") == "b\n"


def test_compare_trees_fails_closed_when_staged_root_missing(tmp_path: Path, helper) -> None:
    promoted = tmp_path / "promoted"
    promoted.mkdir()
    (promoted / "a.h").write_text("a\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="staged tree not found"):
        helper.compare_trees(tmp_path / "does-not-exist-staged", promoted)


def test_compare_trees_fails_closed_when_promoted_root_missing(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "a.h").write_text("a\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="promoted tree not found"):
        helper.compare_trees(staged, tmp_path / "does-not-exist-promoted")


def test_compare_trees_fails_closed_when_both_roots_missing(tmp_path: Path, helper) -> None:
    with pytest.raises(helper.IntakeVerificationError, match="staged tree not found"):
        helper.compare_trees(tmp_path / "missing-staged", tmp_path / "missing-promoted")


def test_compare_trees_fails_closed_when_root_is_a_file_not_a_directory(tmp_path: Path, helper) -> None:
    promoted = tmp_path / "promoted"
    promoted.mkdir()
    staged_file = tmp_path / "staged-is-a-file"
    staged_file.write_text("not a directory\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="staged tree not found"):
        helper.compare_trees(staged_file, promoted)


def test_compare_trees_rejects_symlink_in_staged_tree_instead_of_crashing(tmp_path: Path, helper) -> None:
    """`_tracked_files` deliberately reports symlinks (so a symlinked
    payload is never invisible to this diff), but `compare_trees` must
    still refuse a tree containing one with a clear, actionable error
    instead of letting a dangling (or otherwise unsupported) symlink raise
    a raw, unhandled OSError out of `bas.sha256`."""
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "f.txt").write_text("hi\n", encoding="utf-8")
    (staged / "dangling").symlink_to(staged / "does-not-exist")
    promoted = tmp_path / "promoted"
    promoted.mkdir()
    (promoted / "f.txt").write_text("hi\n", encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError, match="symlink"):
        helper.compare_trees(staged, promoted)


def test_compare_trees_rejects_symlink_in_promoted_tree_instead_of_crashing(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "f.txt").write_text("hi\n", encoding="utf-8")
    promoted = tmp_path / "promoted"
    promoted.mkdir()
    (promoted / "f.txt").write_text("hi\n", encoding="utf-8")
    (promoted / "dangling").symlink_to(promoted / "does-not-exist")

    with pytest.raises(helper.IntakeSecurityError, match="symlink"):
        helper.compare_trees(staged, promoted)


# --------------------------------------------------------------------------
# Staging orchestration (mocks build_ambiqsuite's heavy build/promote step)
# --------------------------------------------------------------------------
def _stub_promote_provider_payload(hal_bytes: bytes = b"hal-archive"):
    """A stand-in for build_ambiqsuite.promote_provider_payload that writes a
    minimal, internally-consistent payload (manifest + matching archive)
    directly to whatever destination_root it is given, without needing a real
    upstream source tree or toolchain."""

    def _stub(train, version, sdk_root, *, destination_root=None):
        assert destination_root is not None
        destination_root.mkdir(parents=True, exist_ok=True)
        archive = destination_root / "lib" / "gcc" / "apollo2" / "libam_hal.a"
        archive.parent.mkdir(parents=True, exist_ok=True)
        archive.write_bytes(hal_bytes)
        manifest_path = destination_root / "artifact-manifest.yaml"
        _write_manifest(manifest_path, toolchain="gcc", part="apollo2", hal_sha256=_sha256_bytes(hal_bytes))

    return _stub


@pytest.fixture()
def staging_env(fake_repo: Path, helper, monkeypatch):
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    monkeypatch.setattr(helper.bas, "missing_artifact_libraries", lambda t, v: [])
    monkeypatch.setattr(helper.bas, "built_artifact_toolchains", lambda t, v: ["gcc"])
    return train


def test_stage_provider_payload_targets_staging_not_provider_root(staging_env, fake_repo, helper, monkeypatch) -> None:
    train = staging_env
    calls: list[Path] = []

    def recording_promote(t, v, sdk_root, *, destination_root=None):
        calls.append(destination_root)
        _stub_promote_provider_payload()(t, v, sdk_root, destination_root=destination_root)

    monkeypatch.setattr(helper.bas, "promote_provider_payload", recording_promote)

    result = helper.stage_provider_payload(train, "test-version", Path("/fake/sdk-root"), patches_dir=None)

    provider_root = fake_repo / "modules" / "nsx-ambiqsuite" / "sdk"
    assert calls == [result.staged_root]
    assert result.staged_root != provider_root
    assert not provider_root.exists()
    assert result.hash_verification.ok


def test_stage_provider_payload_refuses_incomplete_artifact_set(fake_repo, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    monkeypatch.setattr(helper.bas, "missing_artifact_libraries", lambda t, v: ["gcc/lib/apollo2/libam_hal.a"])
    monkeypatch.setattr(helper.bas, "built_artifact_toolchains", lambda t, v: ["gcc"])
    called = []
    monkeypatch.setattr(
        helper.bas,
        "promote_provider_payload",
        lambda *a, **k: called.append(True),
    )

    with pytest.raises(helper.IntakeVerificationError):
        helper.stage_provider_payload(train, "test-version", Path("/fake/sdk-root"), patches_dir=None)

    # Non-destructive failure path: the heavy build/promote step never ran.
    assert called == []


def test_stage_provider_payload_raises_on_hash_mismatch(staging_env, helper, monkeypatch) -> None:
    train = staging_env

    def tampering_promote(t, v, sdk_root, *, destination_root=None):
        _stub_promote_provider_payload(hal_bytes=b"hal-archive")(t, v, sdk_root, destination_root=destination_root)
        # Simulate an archive that ends up not matching its own manifest.
        (destination_root / "lib" / "gcc" / "apollo2" / "libam_hal.a").write_bytes(b"different-bytes")

    monkeypatch.setattr(helper.bas, "promote_provider_payload", tampering_promote)

    with pytest.raises(helper.IntakeVerificationError):
        helper.stage_provider_payload(train, "test-version", Path("/fake/sdk-root"), patches_dir=None)


# --------------------------------------------------------------------------
# Patch hook: ownership metadata, ordering, and fail-closed reapplication
# --------------------------------------------------------------------------
def _init_git_dir(path: Path) -> None:
    subprocess.run(["git", "init", "-q"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.email", "test@example.invalid"], cwd=path, check=True)
    subprocess.run(["git", "config", "user.name", "Test"], cwd=path, check=True)


def _git_diff_patch(repo: Path, relative_path: str, *, before: str | None, after: str) -> str:
    """Produce a real, git-apply-compatible unified diff for `relative_path`
    going from `before` (None means the file does not exist yet) to `after`,
    using an isolated scratch git repo so the patch text is authentic. Staged
    (`--cached`) diffs are used so both "new file" and "modified file" cases
    produce a correct diff regardless of whether `before` was committed."""
    target = repo / relative_path
    target.parent.mkdir(parents=True, exist_ok=True)
    if before is not None:
        target.write_text(before, encoding="utf-8")
        subprocess.run(["git", "add", "-A"], cwd=repo, check=True)
        subprocess.run(["git", "commit", "-q", "-m", "base"], cwd=repo, check=True)
    target.write_text(after, encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=repo, check=True)
    diff = subprocess.run(
        ["git", "diff", "--cached", "--", relative_path], cwd=repo, check=True, capture_output=True, text=True
    )
    subprocess.run(["git", "reset", "-q"], cwd=repo, check=True)
    if before is None:
        target.unlink()
    else:
        subprocess.run(["git", "checkout", "-q", "--", relative_path], cwd=repo, check=True)
    return diff.stdout


def _write_patch(patches_dir: Path, slug: str, patch_text: str, *, owner: str, reason: str, upstream_ref: str | None = None) -> None:
    patches_dir.mkdir(parents=True, exist_ok=True)
    (patches_dir / f"{slug}.patch").write_text(patch_text, encoding="utf-8")
    sidecar = {"owner": owner, "reason": reason}
    if upstream_ref:
        sidecar["upstream_ref"] = upstream_ref
    (patches_dir / f"{slug}.yaml").write_text(yaml.safe_dump(sidecar), encoding="utf-8")


def test_load_patch_queue_requires_ownership_sidecar(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    patches_dir.mkdir()
    (patches_dir / "001-missing-sidecar.patch").write_text("diff --git a/x b/x\n", encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_requires_non_empty_owner_and_reason(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    _write_patch(patches_dir, "001-bad", "diff --git a/x b/x\n", owner="", reason="something")

    with pytest.raises(helper.IntakeSecurityError):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_returns_empty_for_missing_directory(tmp_path: Path, helper) -> None:
    assert helper.load_patch_queue(tmp_path / "does-not-exist") == []


def test_apply_patch_queue_applies_ordered_and_dependent_patches(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    staged.mkdir()
    scratch_repo = tmp_path / "_scratch"
    scratch_repo.mkdir()
    _init_git_dir(scratch_repo)

    # Patch 001 creates a new file; patch 002 depends on 001 having already
    # run (it edits the file 001 introduces). Applying out of order would
    # fail patch 002's `git apply --check`.
    patch_one = _git_diff_patch(scratch_repo, "include/new_header.h", before=None, after="#define FOO 1\n")
    patch_two = _git_diff_patch(scratch_repo, "include/new_header.h", before="#define FOO 1\n", after="#define FOO 1\n#define BAR 2\n")

    patches_dir = tmp_path / "patches"
    _write_patch(patches_dir, "001-add-header", patch_one, owner="jane", reason="add FOO define")
    _write_patch(patches_dir, "002-extend-header", patch_two, owner="jane", reason="add BAR define")

    applications = helper.apply_patch_queue(staged, patches_dir)

    assert [a.metadata.slug for a in applications] == ["001-add-header", "002-extend-header"]
    assert all(a.applied for a in applications)
    assert (staged / "include" / "new_header.h").read_text(encoding="utf-8") == "#define FOO 1\n#define BAR 2\n"


def test_apply_patch_queue_returns_empty_when_no_patches_dir(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    staged.mkdir()
    assert helper.apply_patch_queue(staged, None) == ()
    assert helper.apply_patch_queue(staged, tmp_path / "does-not-exist") == ()


def test_apply_patch_queue_fails_closed_on_reapplication_failure_without_partial_application(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    (staged / "include").mkdir(parents=True)
    (staged / "include" / "existing.h").write_text("#define ONE 1\n", encoding="utf-8")

    scratch_repo = tmp_path / "_scratch"
    scratch_repo.mkdir()
    _init_git_dir(scratch_repo)
    # This patch matches the staged tree and would succeed on its own.
    good_patch = _git_diff_patch(
        scratch_repo, "include/existing.h", before="#define ONE 1\n", after="#define ONE 1\n#define TWO 2\n"
    )
    # This patch's context does not match anything in the staged tree (or the
    # result of the first patch), simulating upstream drift.
    bad_patch = (
        "diff --git a/include/existing.h b/include/existing.h\n"
        "index 0000000..1111111 100644\n"
        "--- a/include/existing.h\n"
        "+++ b/include/existing.h\n"
        "@@ -1,3 +1,4 @@\n"
        " #define ONE 1\n"
        " #define TWO 2\n"
        " #define THREE 3\n"
        "+#define FOUR 4\n"
    )

    patches_dir = tmp_path / "patches"
    _write_patch(patches_dir, "001-good", good_patch, owner="jane", reason="add TWO define")
    _write_patch(patches_dir, "002-drifted", bad_patch, owner="jane", reason="add FOUR define (stale context)")

    with pytest.raises(helper.IntakePatchError, match="002-drifted"):
        helper.apply_patch_queue(staged, patches_dir)

    # No partial application: the real staged tree must be untouched because
    # the whole queue is rehearsed in a scratch copy first.
    assert (staged / "include" / "existing.h").read_text(encoding="utf-8") == "#define ONE 1\n"
    # The rehearsal scratch directory must not leak next to the staged tree.
    assert not (staged.parent / f"{staged.name}.patch-rehearsal").exists()


def test_apply_patch_queue_actually_modifies_target_when_staging_is_nested_inside_a_real_repository(
    tmp_path: Path, helper
) -> None:
    """Production-layout regression test for the `_git_apply` repository-
    discovery bug: the real staging tree always lives several directories
    inside this repository (e.g.
    `sdk-intake/local/staging/<train>/<version>/sdk`). When Git discovers an
    enclosing `.git` from a nested invocation directory, it computes a
    "prefix" and silently drops every hunk whose path does not start with it
    -- which for these patches (always written relative to the staged sdk
    root) is every hunk, in every patch. `git apply` then exits 0 and prints
    nothing without `--verbose`, so a naive check of only the return code
    would treat this as success while the header is left completely
    unmodified. This test fails against that bug and must pass against the
    fix (`_no_repo_discovery_env` + the reverse-apply postcondition check in
    `_git_apply`)."""
    outer_repo = tmp_path / "outer-repo"
    outer_repo.mkdir()
    _init_git_dir(outer_repo)
    (outer_repo / "README.md").write_text("placeholder\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=outer_repo, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "init"], cwd=outer_repo, check=True)

    # Mirror the real production layout: several directories of nesting
    # between the repository root and the staged sdk root.
    staged = outer_repo / "sdk-intake" / "local" / "staging" / "stable" / "test-version" / "sdk"
    include_dir = staged / "CMSIS" / "AmbiqMicro" / "Include"
    include_dir.mkdir(parents=True)
    header = include_dir / "apollo510.h"
    header.write_text("#define OLD_DEFINE 1\n", encoding="utf-8")

    scratch_repo = tmp_path / "_scratch"
    scratch_repo.mkdir()
    _init_git_dir(scratch_repo)
    patch_text = _git_diff_patch(
        scratch_repo,
        "CMSIS/AmbiqMicro/Include/apollo510.h",
        before="#define OLD_DEFINE 1\n",
        after="#define NEW_DEFINE 1\n",
    )

    patches_dir = tmp_path / "patches"
    _write_patch(patches_dir, "001-fix-define", patch_text, owner="jane", reason="fix upstream define")

    applications = helper.apply_patch_queue(staged, patches_dir)

    assert [a.metadata.slug for a in applications] == ["001-fix-define"]
    assert header.read_text(encoding="utf-8") == "#define NEW_DEFINE 1\n"


def test_git_apply_raises_when_patch_context_cannot_be_found_even_nested_in_a_real_repository(
    tmp_path: Path, helper
) -> None:
    """A patch whose context genuinely does not match the tree must still
    fail loudly (never silently) when the target is nested inside a real
    repository -- guards against the fix over-suppressing real failures."""
    outer_repo = tmp_path / "outer-repo"
    outer_repo.mkdir()
    _init_git_dir(outer_repo)
    (outer_repo / "README.md").write_text("placeholder\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=outer_repo, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "init"], cwd=outer_repo, check=True)

    target_root = outer_repo / "sdk-intake" / "local" / "staging" / "stable" / "test-version" / "sdk"
    target_root.mkdir(parents=True)
    (target_root / "foo.h").write_text("#define ACTUAL 1\n", encoding="utf-8")

    non_matching_patch = tmp_path / "non-matching.patch"
    non_matching_patch.write_text(
        "diff --git a/foo.h b/foo.h\n"
        "index 0000000..1111111 100644\n"
        "--- a/foo.h\n"
        "+++ b/foo.h\n"
        "@@ -1 +1 @@\n"
        "-#define SOMETHING_ELSE 1\n"
        "+#define REPLACED 1\n",
        encoding="utf-8",
    )

    with pytest.raises(helper.IntakePatchError):
        helper._git_apply(target_root, non_matching_patch, owner="jane")
    assert (target_root / "foo.h").read_text(encoding="utf-8") == "#define ACTUAL 1\n"


# --------------------------------------------------------------------------
# Patch hook: forbidden-path checks must hold regardless of invocation cwd
# --------------------------------------------------------------------------
def _make_committed_git_repo(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)
    _init_git_dir(path)
    (path / ".keep").write_text("x\n", encoding="utf-8")
    subprocess.run(["git", "add", "-A"], cwd=path, check=True)
    subprocess.run(["git", "commit", "-q", "-m", "init"], cwd=path, check=True)


@pytest.mark.parametrize("invoke_from", ["repo_root", "nested_subdirectory"])
def test_load_patch_queue_rejects_lib_path_regardless_of_invocation_cwd(
    tmp_path: Path, helper, monkeypatch, invoke_from: str
) -> None:
    """Regression test for the `_patch_target_paths`/`_git_apply` root
    mismatch: `git apply --numstat` silently reports zero touched paths when
    run (with no explicit `-C`) from a directory nested inside a
    repository, which would let a patch touching `lib/**` evade
    `_assert_patch_paths_allowed` entirely when this tool happens to be
    invoked from a subdirectory (e.g. `sdk-intake/`) of the enclosing repo."""
    repo = tmp_path / "repo"
    _make_committed_git_repo(repo)

    patches_dir = repo / "sdk-intake" / "patches" / "stable"
    lib_patch = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "index e69de29..1111111 100644\n"
        "Binary files a/lib/gcc/apollo510/libam_hal.a and b/lib/gcc/apollo510/libam_hal.a differ\n"
    )
    _write_patch(patches_dir, "001-tamper-lib", lib_patch, owner="jane", reason="should be rejected")

    if invoke_from == "repo_root":
        cwd = repo
    else:
        cwd = repo / "sdk-intake" / "local"
        cwd.mkdir(parents=True)
    monkeypatch.chdir(cwd)

    with pytest.raises(helper.IntakeSecurityError, match="lib/gcc/apollo510/libam_hal.a"):
        helper.load_patch_queue(patches_dir)


@pytest.mark.parametrize("invoke_from", ["repo_root", "nested_subdirectory"])
def test_cli_verify_ownership_rejects_forbidden_patch_regardless_of_invocation_cwd(
    fake_repo: Path, helper, monkeypatch, invoke_from: str
) -> None:
    """CLI-level counterpart of the above: `verify-ownership` must reject a
    patch touching a forbidden `lib/**` path whether invoked from the
    repository root or from a directory nested inside it."""
    _make_committed_git_repo(fake_repo)
    # `fake_repo`'s ownership manifest already declares
    # `modules/nsx-ambiqsuite/sdk`, matching the real `stable` train's
    # `module_dir`, so `verify_generated_boundary` passes without further
    # setup.
    patches_dir = fake_repo / "sdk-intake" / "patches" / "stable"
    manifest_patch = (
        "diff --git a/artifact-manifest.yaml b/artifact-manifest.yaml\n"
        "index e69de29..1111111 100644\n"
        "--- a/artifact-manifest.yaml\n"
        "+++ b/artifact-manifest.yaml\n"
        "@@ -1 +1 @@\n"
        "-sha256: aaaa\n"
        "+sha256: bbbb\n"
    )
    _write_patch(patches_dir, "001-tamper-manifest", manifest_patch, owner="jane", reason="should be rejected")

    if invoke_from == "repo_root":
        cwd = fake_repo
    else:
        cwd = fake_repo / "sdk-intake" / "local"
        cwd.mkdir(parents=True)
    monkeypatch.chdir(cwd)

    rc = helper.main(["verify-ownership", "--train", "stable", "--patches-dir", str(patches_dir)])

    assert rc == 1


# --------------------------------------------------------------------------
# Patch hook: reject unsupported/ambiguous patch shapes fail-closed
# --------------------------------------------------------------------------
def test_load_patch_queue_rejects_rename(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    rename_patch = (
        "diff --git a/include/old_name.h b/include/new_name.h\n"
        "similarity index 100%\n"
        "rename from include/old_name.h\n"
        "rename to include/new_name.h\n"
    )
    _write_patch(patches_dir, "001-rename", rename_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="rename"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_rename_that_moves_forbidden_path_to_an_allowed_name(
    tmp_path: Path, helper
) -> None:
    """A rename's `git apply --numstat` output reports only the destination
    path, so a rename moving a forbidden `lib/**` archive to an
    otherwise-allowed name must still be rejected -- via the unconditional
    rename rejection, not the (blind, in this case) forbidden-path check."""
    patches_dir = tmp_path / "patches"
    rename_patch = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/notes.md\n"
        "similarity index 100%\n"
        "rename from lib/gcc/apollo510/libam_hal.a\n"
        "rename to notes.md\n"
    )
    _write_patch(patches_dir, "001-rename-away", rename_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="rename"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_copy(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    copy_patch = (
        "diff --git a/include/a.h b/include/b.h\n"
        "similarity index 100%\n"
        "copy from include/a.h\n"
        "copy to include/b.h\n"
    )
    _write_patch(patches_dir, "001-copy", copy_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="copy"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_mode_only_change(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    mode_patch = "diff --git a/script.sh b/script.sh\nold mode 100644\nnew mode 100755\n"
    _write_patch(patches_dir, "001-chmod", mode_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="mode"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_new_symlink(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    symlink_patch = (
        "diff --git a/include/link.h b/include/link.h\n"
        "new file mode 120000\n"
        "index 0000000..abc1234\n"
        "--- /dev/null\n"
        "+++ b/include/link.h\n"
        "@@ -0,0 +1 @@\n"
        "+/etc/passwd\n"
        "\\ No newline at end of file\n"
    )
    _write_patch(patches_dir, "001-symlink", symlink_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="non-regular-file mode"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_modified_symlink(tmp_path: Path, helper) -> None:
    """A symlink modify has no `new file mode`/`rename`/`old mode` line at
    all -- the symlink mode only appears on the `index` line -- so it must
    be caught there too."""
    patches_dir = tmp_path / "patches"
    symlink_patch = (
        "diff --git a/include/link.h b/include/link.h\n"
        "index 1234567..abcdef0 120000\n"
        "--- a/include/link.h\n"
        "+++ b/include/link.h\n"
        "@@ -1 +1 @@\n"
        "-/etc/old\n"
        "\\ No newline at end of file\n"
        "+/etc/passwd\n"
        "\\ No newline at end of file\n"
    )
    _write_patch(patches_dir, "001-symlink-modify", symlink_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="non-regular-file mode"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_binary_content_outside_forbidden_paths(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    binary_patch = (
        "diff --git a/include/blob.bin b/include/blob.bin\n"
        "index e69de29..1111111 100644\n"
        "Binary files a/include/blob.bin and b/include/blob.bin differ\n"
    )
    _write_patch(patches_dir, "001-binary", binary_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="binary"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_accepts_plain_text_modify_of_regular_file(tmp_path: Path, helper) -> None:
    """Non-regression: an ordinary text modify (the only shape this patch
    hook is meant to support) must not be rejected by the new shape check."""
    patches_dir = tmp_path / "patches"
    header_patch = (
        "diff --git a/CMSIS/AmbiqMicro/Include/apollo510.h b/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "index e69de29..1111111 100644\n"
        "--- a/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "+++ b/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "@@ -1 +1 @@\n"
        "-#define OLD 1\n"
        "+#define NEW 1\n"
    )
    _write_patch(patches_dir, "001-fix-define", header_patch, owner="jane", reason="fix upstream define")

    queue = helper.load_patch_queue(patches_dir)

    assert [m.slug for m in queue] == ["001-fix-define"]


def test_load_patch_queue_rejects_legacy_rename_old_new_spelling(tmp_path: Path, helper) -> None:
    """Git's older `rename old`/`rename new` spelling (pre-dating the now
    common `rename from`/`rename to`) must be caught by the same
    `git apply --summary`-based rename detection, not just the modern
    keywords."""
    patches_dir = tmp_path / "patches"
    rename_patch = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/notes.md\n"
        "similarity index 100%\n"
        "rename old lib/gcc/apollo510/libam_hal.a\n"
        "rename new notes.md\n"
    )
    _write_patch(patches_dir, "001-legacy-rename", rename_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="rename"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_implicit_rename_without_any_rename_keyword(tmp_path: Path, helper) -> None:
    """A "naked" path mismatch -- `diff --git a/OLD b/NEW` with differing
    `---`/`+++` headers but *no* rename/copy keyword anywhere -- is honored
    by `git apply` exactly like a rename (it reads OLD's content and writes
    to NEW) but is invisible to both `git apply --summary` and
    `--numstat` (which only ever reports NEW). This must still be rejected
    via direct `---`/`+++` header-pair comparison, or a forbidden `lib/**`
    archive could be smuggled out under an allowed name with no rename
    keyword at all."""
    patches_dir = tmp_path / "patches"
    implicit_rename_patch = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/notes.md\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-SECRET-ARCHIVE\n"
        "+SECRET-ARCHIVE-MOVED\n"
    )
    _write_patch(
        patches_dir, "001-implicit-rename", implicit_rename_patch, owner="jane", reason="should be rejected"
    )

    with pytest.raises(helper.IntakeSecurityError, match="changes path from"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_implicit_rename_hidden_by_counterfeit_headers_before_the_real_entry(
    tmp_path: Path, helper
) -> None:
    """A whole-file, unanchored scan for `---`/`+++` lines is itself
    bypassable: a counterfeit `--- a/notes.md` line placed in a
    commit-message preamble *before* the real `diff --git` line -- text Git
    itself never treats as a diff header -- would positionally pair with
    the real entry's headers and make an actual ambiguous rename look
    balanced. Detection must be anchored on the `diff --git` line that
    actually opens each entry, ignoring anything before the first one."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "Subject: [PATCH] benign-looking header tweak\n"
        "\n"
        "This commit message region is skipped by git apply.\n"
        "--- a/notes.md\n"
        "(this line prevents the pair above from parsing as a traditional header)\n"
        "\n"
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/notes.md\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-SECRET-ARCHIVE\n"
        "+SECRET-ARCHIVE-MOVED\n"
    )
    _write_patch(patches_dir, "001-preamble-injection", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="changes path from"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_implicit_rename_hidden_by_counterfeit_headers_after_the_hunk(
    tmp_path: Path, helper
) -> None:
    """The same unanchored-scan bypass, using a trailing counterfeit
    `+++ b/OLD_PATH` line placed *after* the real entry's hunk -- text Git
    also never treats as a diff header, since a file's extended header
    region ends at its first `@@` line. Detection must stop scanning each
    entry's headers at its first hunk marker."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/notes.md\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-SECRET-ARCHIVE\n"
        "+SECRET-ARCHIVE-MOVED\n"
        "\n"
        "trailing commentary ignored by git apply:\n"
        "+++ b/lib/gcc/apollo510/libam_hal.a\n"
    )
    _write_patch(patches_dir, "001-trailing-injection", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="changes path from"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_accepts_legitimate_multi_file_patch(tmp_path: Path, helper) -> None:
    """Non-regression: the `diff --git`-anchored, numstat-cross-checked scan
    in `_assert_no_ambiguous_path_change` must still accept an ordinary
    patch touching more than one file in a single `.patch`."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/include/a.h b/include/a.h\n"
        "index e69de29..1111111 100644\n"
        "--- a/include/a.h\n"
        "+++ b/include/a.h\n"
        "@@ -1 +1 @@\n"
        "-#define A 1\n"
        "+#define A 2\n"
        "diff --git a/include/b.h b/include/b.h\n"
        "index e69de29..2222222 100644\n"
        "--- a/include/b.h\n"
        "+++ b/include/b.h\n"
        "@@ -1 +1 @@\n"
        "-#define B 1\n"
        "+#define B 2\n"
    )
    _write_patch(patches_dir, "001-multi-file", patch_text, owner="jane", reason="fix two defines")

    queue = helper.load_patch_queue(patches_dir)

    assert [m.slug for m in queue] == ["001-multi-file"]


def test_diff_header_path_returns_none_for_dev_null(tmp_path: Path, helper) -> None:
    result = helper._diff_header_path("/dev/null", side="old ('---')", prefix="a/", patch_path=tmp_path / "x.patch")

    assert result is None


def test_diff_header_path_rejects_unrecognized_prefix(tmp_path: Path, helper) -> None:
    """A `---`/`+++` header line whose path isn't `/dev/null` and doesn't
    have the `a/`/`b/` prefix this tool's own generated patches always use
    (e.g. a patch authored with a different `-p` depth) must fail closed
    rather than silently guessing at the intended path."""
    with pytest.raises(helper.IntakeSecurityError, match="unrecognized"):
        helper._diff_header_path(
            "some/path/without/prefix.h", side="old ('---')", prefix="a/", patch_path=tmp_path / "x.patch"
        )


def test_assert_no_ambiguous_path_change_rejects_mismatched_diff_git_and_numstat_counts(
    tmp_path: Path, helper
) -> None:
    """If the number of `diff --git` header lines doesn't match the number
    of entries `git apply --numstat -z` itself reports, the patch's shape
    cannot be trusted -- fail closed rather than silently scanning whatever
    `diff --git` lines happen to be present."""
    patch_path = tmp_path / "mismatched.patch"
    patch_path.write_text(
        "diff --git a/include/a.h b/include/a.h\n"
        "index e69de29..1111111 100644\n"
        "--- a/include/a.h\n"
        "+++ b/include/a.h\n"
        "@@ -1 +1 @@\n"
        "-#define A 1\n"
        "+#define A 2\n",
        encoding="utf-8",
    )

    with pytest.raises(helper.IntakeSecurityError, match="diff --git"):
        helper._assert_no_ambiguous_path_change(patch_path, expected_entry_count=2)


@pytest.mark.parametrize("mode_field", ["120000", "+120000"])
def test_load_patch_queue_rejects_new_symlink_with_whitespace_or_sign_tolerant_mode_line(
    tmp_path: Path, helper, mode_field: str
) -> None:
    """Git's own mode-line parser (`strtoul`) tolerates extra whitespace and
    a leading `+`/`-` sign; a hand-rolled regex requiring exactly one space
    after `new file mode` does not, and so can be evaded by a mode line like
    `new file mode  120000` (two spaces) or `new file mode +120000`. Shape
    detection must come from `git apply --summary` (which uses Git's own
    parser), not a fragile regex over the raw mode line."""
    patches_dir = tmp_path / "patches"
    symlink_patch = (
        "diff --git a/include/link.h b/include/link.h\n"
        f"new file mode  {mode_field}\n"
        "index 0000000..abc1234\n"
        "--- /dev/null\n"
        "+++ b/include/link.h\n"
        "@@ -0,0 +1 @@\n"
        "+/etc/passwd\n"
        "\\ No newline at end of file\n"
    )
    _write_patch(patches_dir, "001-symlink-whitespace", symlink_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="non-regular-file mode"):
        helper.load_patch_queue(patches_dir)


@pytest.mark.parametrize(
    "forbidden_path",
    ["LIB/gcc/apollo510/libam_hal.a", "Artifact-Manifest.yaml"],
)
def test_load_patch_queue_rejects_case_variant_of_forbidden_path(
    tmp_path: Path, helper, forbidden_path: str
) -> None:
    """The staging/provider trees live on a case-insensitive filesystem
    (e.g. default macOS APFS), so a byte-exact forbidden-path comparison can
    be bypassed by a case variant (`LIB/...` or `Artifact-Manifest.yaml`)
    that resolves to the exact same protected file on disk. The comparison
    must fold both sides to a canonical case (and Unicode normal form)
    before comparing."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        f"diff --git a/{forbidden_path} b/{forbidden_path}\n"
        "index e69de29..1111111 100644\n"
        f"--- a/{forbidden_path}\n"
        f"+++ b/{forbidden_path}\n"
        "@@ -0,0 +1 @@\n"
        "+TAMPERED\n"
    )
    _write_patch(patches_dir, "001-case-variant", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="patch hook may not modify"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_forbidden_path_with_non_ascii_characters(tmp_path: Path, helper) -> None:
    """`git apply --numstat` (without `-z`) C-quotes non-ASCII paths (e.g.
    `"lib/gcc/apollo\\303\\251/libam_hal.a"`), which then never matches a
    plain `lib/` prefix check even though the real, literal path is
    forbidden. `-z` must be used so paths are reported as raw, unquoted
    bytes."""
    patches_dir = tmp_path / "patches"
    forbidden_path = "lib/gcc/apollo\u00e9/libam_hal.a"
    patch_text = (
        f'diff --git "a/{forbidden_path}" "b/{forbidden_path}"\n'
        "index e69de29..1111111 100644\n"
        f'--- "a/{forbidden_path}"\n'
        f'+++ "b/{forbidden_path}"\n'
        "@@ -0,0 +1 @@\n"
        "+TAMPERED\n"
    )
    _write_patch(patches_dir, "001-unicode-path", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="patch hook may not modify"):
        helper.load_patch_queue(patches_dir)


def test_tracked_files_reports_symlinked_entries(tmp_path: Path, helper) -> None:
    """A symlinked directory is not itself `is_file()`, and `rglob` does not
    recurse into a symlinked directory -- so unless symlinks are explicitly
    included, they (and anything they appear to contain) are silently
    invisible to `compare_trees`'s human-review diff, even though
    `promote_from_staging` would otherwise write them (dereferenced) into
    the committed tree."""
    root = tmp_path / "root"
    root.mkdir()
    real_target = tmp_path / "outside_target"
    real_target.write_text("secret\n", encoding="utf-8")
    (root / "evil_link").symlink_to(real_target)

    tracked = helper._tracked_files(root)

    assert "evil_link" in tracked


def test_assert_no_symlinks_rejects_file_symlink(tmp_path: Path, helper) -> None:
    root = tmp_path / "staged"
    root.mkdir()
    outside_secret = tmp_path / "outside_secret.txt"
    outside_secret.write_text("host secret\n", encoding="utf-8")
    (root / "evil_link").symlink_to(outside_secret)

    with pytest.raises(helper.IntakeSecurityError, match="symlink"):
        helper._assert_no_symlinks(root, label="staged payload")


def test_assert_no_symlinks_rejects_directory_symlink(tmp_path: Path, helper) -> None:
    root = tmp_path / "staged"
    root.mkdir()
    real_dir = tmp_path / "real_dir_target"
    real_dir.mkdir()
    (real_dir / "secret.txt").write_text("dir secret\n", encoding="utf-8")
    (root / "evil_dir_link").symlink_to(real_dir, target_is_directory=True)

    with pytest.raises(helper.IntakeSecurityError, match="symlink"):
        helper._assert_no_symlinks(root, label="staged payload")


def test_assert_no_symlinks_accepts_tree_without_symlinks(tmp_path: Path, helper) -> None:
    root = tmp_path / "staged"
    (root / "sub").mkdir(parents=True)
    (root / "sub" / "file.txt").write_text("ordinary\n", encoding="utf-8")

    helper._assert_no_symlinks(root, label="staged payload")


def test_promote_from_staging_rejects_symlinks_in_staged_tree(tmp_path: Path, helper, monkeypatch) -> None:
    """`promote_from_staging` must refuse to `copytree` a staged tree
    containing a symlink -- otherwise the default `shutil.copytree`
    dereferences it into a regular file/directory holding whatever content
    its target points at, materializing arbitrary host content into the
    committed provider tree."""
    fake_root = tmp_path / "fake-repo"
    fake_root.mkdir()
    monkeypatch.setattr(helper, "repo_root", lambda: fake_root)

    staged = fake_root / "staged_sdk"
    staged.mkdir()
    (staged / "artifact-manifest.yaml").write_text("artifacts: []\n", encoding="utf-8")
    outside_secret = tmp_path / "outside_secret.txt"
    outside_secret.write_text("host secret\n", encoding="utf-8")
    (staged / "evil_link").symlink_to(outside_secret)

    provider_root = fake_root / "provider_sdk"

    with pytest.raises(helper.IntakeSecurityError, match="symlink"):
        helper.promote_from_staging(staged, provider_root, confirm=True)

    assert not provider_root.exists()


def test_manifest_path_to_promoted_relative_rejects_parent_traversal(helper) -> None:
    with pytest.raises(helper.IntakeVerificationError, match="unsafe path segment"):
        helper.manifest_path_to_promoted_relative("gcc/lib/../../../../etc/hosts")


def test_manifest_path_to_promoted_relative_accepts_ordinary_path(helper) -> None:
    resolved = helper.manifest_path_to_promoted_relative("gcc/lib/apollo510/libam_hal.a")

    assert resolved == Path("lib/gcc/apollo510/libam_hal.a")


def test_verify_artifact_hashes_rejects_manifest_entry_escaping_sdk_root_via_symlink(
    tmp_path: Path, helper
) -> None:
    """Defense in depth alongside `manifest_path_to_promoted_relative`'s
    `..`/absolute-segment rejection: even a syntactically clean manifest
    `path` could resolve outside `sdk_root` if a symlink sits somewhere on
    the way there (e.g. `lib/gcc` itself replaced with a symlink pointing
    outside the tree). `verify_artifact_hashes` must resolve the target and
    reject it if it escapes the resolved root, rather than "verifying" an
    attacker-controlled file elsewhere on disk."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "apollo510").mkdir()
    (outside / "apollo510" / "libam_hal.a").write_bytes(b"attacker-controlled-content")

    lib_dir = sdk_root / "lib"
    lib_dir.mkdir()
    (lib_dir / "gcc").symlink_to(outside, target_is_directory=True)

    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest = {
        "parts": [
            {
                "logical_skew": "apollo510",
                "hal_artifacts": {
                    "gcc": {"path": "gcc/lib/apollo510/libam_hal.a", "sha256": "0" * 64},
                },
            }
        ],
        "boards": [],
    }
    manifest_path.write_text(yaml.safe_dump(manifest), encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="escaping the SDK root"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_assert_patch_paths_allowed_fails_closed_on_zero_reported_paths(tmp_path: Path, helper, monkeypatch) -> None:
    """If `_patch_target_paths` ever reports zero paths for a patch (e.g. an
    empty patch file, or some future Git quirk), the forbidden-path check
    must fail closed instead of silently treating "nothing reported" as
    "nothing forbidden touched"."""
    patch_path = tmp_path / "empty.patch"
    patch_path.write_text("", encoding="utf-8")
    monkeypatch.setattr(helper, "_patch_target_paths", lambda p: [])

    with pytest.raises(helper.IntakeSecurityError, match="zero target paths"):
        helper._assert_patch_paths_allowed(patch_path)


# --------------------------------------------------------------------------
# Promotion: explicit confirmation, atomicity, and rollback
# --------------------------------------------------------------------------
def test_promote_from_staging_requires_explicit_confirmation(tmp_path: Path, helper) -> None:
    staged = tmp_path / "staged"
    staged.mkdir()
    provider = tmp_path / "provider"
    with pytest.raises(helper.IntakeSecurityError):
        helper.promote_from_staging(staged, provider, confirm=False)
    assert not provider.exists()


def test_promote_from_staging_swaps_directories(tmp_path: Path, helper, monkeypatch) -> None:
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    staged = tmp_path / "staged"
    staged.mkdir()
    archive = staged / "lib" / "gcc" / "apollo2" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"new-archive")
    _write_manifest(staged / "artifact-manifest.yaml", toolchain="gcc", part="apollo2", hal_sha256=_sha256_bytes(b"new-archive"))

    provider = tmp_path / "provider"
    provider.mkdir()
    (provider / "old-only.txt").write_text("stale content\n", encoding="utf-8")

    helper.promote_from_staging(staged, provider, confirm=True)

    assert (provider / "lib" / "gcc" / "apollo2" / "libam_hal.a").read_bytes() == b"new-archive"
    assert not (provider / "old-only.txt").exists()
    # No leftover scratch directories.
    assert not provider.with_name(provider.name + ".promote-tmp").exists()
    assert not provider.with_name(provider.name + ".promote-backup").exists()


def test_promote_from_staging_refuses_hash_mismatched_staging(tmp_path: Path, helper, monkeypatch) -> None:
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    staged = tmp_path / "staged"
    archive = staged / "lib" / "gcc" / "apollo2" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"actual-bytes")
    _write_manifest(staged / "artifact-manifest.yaml", toolchain="gcc", part="apollo2", hal_sha256="0" * 64)

    provider = tmp_path / "provider"
    provider.mkdir()
    (provider / "keep.txt").write_text("keep\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError):
        helper.promote_from_staging(staged, provider, confirm=True)

    # Non-destructive failure path: the existing provider tree is untouched.
    assert (provider / "keep.txt").is_file()


def test_promote_from_staging_rejects_paths_outside_repo(tmp_path: Path, helper, monkeypatch) -> None:
    inside_repo = tmp_path / "repo"
    inside_repo.mkdir()
    monkeypatch.setattr(helper, "repo_root", lambda: inside_repo)

    outside_staged = tmp_path / "outside-staged"
    outside_staged.mkdir()
    provider = inside_repo / "modules" / "nsx-ambiqsuite" / "sdk"

    with pytest.raises(helper.IntakeSecurityError):
        helper.promote_from_staging(outside_staged, provider, confirm=True)
    assert not provider.exists()


# --------------------------------------------------------------------------
# Golden-baseline verification path (read-only)
# --------------------------------------------------------------------------
def test_verify_promoted_baseline_uses_provider_root(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    provider_root = fake_repo / "modules" / "nsx-ambiqsuite" / "sdk"
    archive = provider_root / "lib" / "gcc" / "apollo2" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"baseline-bytes")
    _write_manifest(
        provider_root / "artifact-manifest.yaml", toolchain="gcc", part="apollo2", hal_sha256=_sha256_bytes(b"baseline-bytes")
    )
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: provider_root)

    result = helper.verify_promoted_baseline(train)

    assert result.ok
    assert result.verified == ("lib/gcc/apollo2/libam_hal.a",)


def test_verify_promoted_baseline_raises_if_provider_tree_missing(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")

    with pytest.raises(helper.IntakeVerificationError):
        helper.verify_promoted_baseline(train)


# --------------------------------------------------------------------------
# Hardening follow-ups from independent security review
# --------------------------------------------------------------------------
def test_verify_artifact_hashes_fails_closed_on_empty_manifest(tmp_path: Path, helper) -> None:
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text(yaml.safe_dump({"sdk": {"provider": "ambiqsuite"}}), encoding="utf-8")

    result = helper.verify_artifact_hashes(sdk_root, manifest_path)

    # A manifest declaring no hal_artifacts/bsp_artifacts must never be
    # reported as "ok" just because there was nothing to mismatch.
    assert not result.ok
    assert result.verified == ()


def test_assert_safe_path_segment_rejects_traversal(helper) -> None:
    with pytest.raises(helper.IntakeSecurityError):
        helper.assert_safe_path_segment("../escape", label="version")
    with pytest.raises(helper.IntakeSecurityError):
        helper.assert_safe_path_segment("a/b", label="version")
    with pytest.raises(helper.IntakeSecurityError):
        helper.assert_safe_path_segment("..", label="version")


def test_assert_safe_path_segment_accepts_ordinary_version(helper) -> None:
    assert helper.assert_safe_path_segment("stable-2026.06.18", label="version") == "stable-2026.06.18"


def test_staging_root_rejects_path_traversal_in_version(fake_repo: Path, helper) -> None:
    with pytest.raises(helper.IntakeSecurityError):
        helper.staging_root("stable", "../../../../modules/nsx-ambiqsuite")


def test_stage_provider_payload_rejects_unsafe_version(fake_repo: Path, helper, monkeypatch) -> None:
    train = _apollo2_train(helper)
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: fake_repo / "modules" / "nsx-ambiqsuite" / "sdk")
    monkeypatch.setattr(helper.bas, "missing_artifact_libraries", lambda t, v: [])
    monkeypatch.setattr(helper.bas, "built_artifact_toolchains", lambda t, v: ["gcc"])
    called = []
    monkeypatch.setattr(helper.bas, "promote_provider_payload", lambda *a, **k: called.append(True))

    with pytest.raises(helper.IntakeSecurityError):
        helper.stage_provider_payload(train, "../escape", Path("/fake/sdk-root"), patches_dir=None)

    assert called == []


def test_load_patch_queue_rejects_patch_touching_lib(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    lib_patch = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "index e69de29..1111111 100644\n"
        "Binary files a/lib/gcc/apollo510/libam_hal.a and b/lib/gcc/apollo510/libam_hal.a differ\n"
    )
    _write_patch(patches_dir, "001-tamper-lib", lib_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="lib/gcc/apollo510/libam_hal.a"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_patch_touching_manifest(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    manifest_patch = (
        "diff --git a/artifact-manifest.yaml b/artifact-manifest.yaml\n"
        "index e69de29..1111111 100644\n"
        "--- a/artifact-manifest.yaml\n"
        "+++ b/artifact-manifest.yaml\n"
        "@@ -1 +1 @@\n"
        "-sha256: aaaa\n"
        "+sha256: bbbb\n"
    )
    _write_patch(patches_dir, "001-tamper-manifest", manifest_patch, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="artifact-manifest.yaml"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_allows_patch_touching_headers(tmp_path: Path, helper) -> None:
    patches_dir = tmp_path / "patches"
    header_patch = (
        "diff --git a/CMSIS/AmbiqMicro/Include/apollo510.h b/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "index e69de29..1111111 100644\n"
        "--- a/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "+++ b/CMSIS/AmbiqMicro/Include/apollo510.h\n"
        "@@ -1 +1 @@\n"
        "-#define OLD 1\n"
        "+#define NEW 1\n"
    )
    _write_patch(patches_dir, "001-fix-define", header_patch, owner="jane", reason="fix upstream define")

    queue = helper.load_patch_queue(patches_dir)

    assert [m.slug for m in queue] == ["001-fix-define"]


def test_promote_from_staging_recovers_backup_by_baseexception_not_just_oserror(tmp_path: Path, helper, monkeypatch) -> None:
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    staged = tmp_path / "staged"
    archive = staged / "lib" / "gcc" / "apollo2" / "libam_hal.a"
    archive.parent.mkdir(parents=True)
    archive.write_bytes(b"new-archive")
    (staged / "artifact-manifest.yaml").write_text(
        yaml.safe_dump(
            {
                "parts": [
                    {
                        "logical_skew": "apollo2",
                        "hal_artifacts": {
                            "gcc": {
                                "path": "gcc/lib/apollo2/libam_hal.a",
                                "sha256": _sha256_bytes(b"new-archive"),
                            }
                        },
                    }
                ],
                "boards": [],
            }
        ),
        encoding="utf-8",
    )

    provider = tmp_path / "provider"
    provider.mkdir()
    (provider / "keep.txt").write_text("original\n", encoding="utf-8")

    original_rename = Path.rename

    def failing_rename(self, target):
        # Simulate the swap being interrupted (e.g. KeyboardInterrupt) right
        # after the provider tree is renamed aside as a backup, but before the
        # new tree is renamed into place.
        if self == provider and Path(target) == provider.with_name(provider.name + ".promote-backup"):
            original_rename(self, target)
            raise KeyboardInterrupt()
        return original_rename(self, target)

    monkeypatch.setattr(Path, "rename", failing_rename)

    with pytest.raises(KeyboardInterrupt):
        helper.promote_from_staging(staged, provider, confirm=True)

    # Rolled back: the original provider tree must be restored, not left
    # missing, even though the failure was not an OSError.
    assert (provider / "keep.txt").is_file()


def test_promote_from_staging_refuses_to_run_over_leftover_backup(tmp_path: Path, helper, monkeypatch) -> None:
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    provider = tmp_path / "provider"
    backup = provider.with_name(provider.name + ".promote-backup")
    backup.mkdir(parents=True)
    (backup / "salvage-me.txt").write_text("important\n", encoding="utf-8")

    staged = tmp_path / "staged"
    staged.mkdir()

    with pytest.raises(helper.IntakeSecurityError, match="leftover promotion backup"):
        helper.promote_from_staging(staged, provider, confirm=True)

    # Refusing to proceed must not touch the leftover backup.
    assert (backup / "salvage-me.txt").is_file()
    assert not provider.exists()


def test_promote_from_staging_refuses_when_backup_and_provider_both_exist(tmp_path: Path, helper, monkeypatch) -> None:
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    provider = tmp_path / "provider"
    provider.mkdir()
    (provider / "current.txt").write_text("current\n", encoding="utf-8")
    backup = provider.with_name(provider.name + ".promote-backup")
    backup.mkdir()
    (backup / "stale.txt").write_text("stale\n", encoding="utf-8")

    staged = tmp_path / "staged"
    staged.mkdir()

    with pytest.raises(helper.IntakeSecurityError, match="leftover promotion backup"):
        helper.promote_from_staging(staged, provider, confirm=True)

    # Refusing to proceed must not touch either the current provider tree or
    # the stale backup; a maintainer resolves the ambiguity explicitly.
    assert (provider / "current.txt").is_file()
    assert (backup / "stale.txt").is_file()


def test_verify_generated_boundary_raises_intake_error_for_escaping_provider_path(
    fake_repo: Path, helper, monkeypatch
) -> None:
    train = _apollo2_train(helper)
    outside = fake_repo.parent / "outside-repo"
    monkeypatch.setattr(helper.bas, "provider_sdk_root", lambda t: outside)

    with pytest.raises(helper.IntakeSecurityError):
        helper.verify_generated_boundary(train)


def test_verify_artifact_hashes_raises_intake_error_for_malformed_entry(tmp_path: Path, helper) -> None:
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest = {
        "parts": [
            {
                "logical_skew": "apollo510",
                # Missing the required 'sha256' key -- malformed manifest.
                "hal_artifacts": {"gcc": {"path": "gcc/lib/apollo510/libam_hal.a"}},
            }
        ],
        "boards": [],
    }
    manifest_path.write_text(yaml.safe_dump(manifest), encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="malformed"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_when_top_level_is_not_a_mapping(
    tmp_path: Path, helper
) -> None:
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text("- just\n- a\n- list\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="mapping"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_when_artifacts_value_is_not_a_mapping(
    tmp_path: Path, helper
) -> None:
    """`entry.get(artifact_key)` being a list (instead of the expected
    toolchain->info mapping) must fail closed with an actionable
    `IntakeVerificationError`, not an untyped `AttributeError` escaping from
    `.items()`."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest = {
        "parts": [{"logical_skew": "apollo510", "hal_artifacts": ["not", "a", "mapping"]}],
        "boards": [],
    }
    manifest_path.write_text(yaml.safe_dump(manifest), encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="malformed"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_when_entry_is_not_a_mapping(tmp_path: Path, helper) -> None:
    """A `parts`/`boards` entry that is a bare scalar (instead of a mapping)
    must fail closed with an actionable `IntakeVerificationError`, not an
    untyped `AttributeError` escaping from `entry.get(...)`."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text('parts:\n  - "just-a-string"\nboards: []\n', encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="malformed"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)
