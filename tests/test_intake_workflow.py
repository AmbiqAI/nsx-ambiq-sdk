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
