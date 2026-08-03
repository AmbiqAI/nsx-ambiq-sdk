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


def _write_ownership_manifest(root: Path, content: str) -> None:
    ownership_dir = root / "release"
    ownership_dir.mkdir(parents=True, exist_ok=True)
    (ownership_dir / "source-ownership.yaml").write_text(content, encoding="utf-8")


def test_load_ownership_entries_rejects_top_level_not_a_mapping(tmp_path: Path, helper) -> None:
    """A source-ownership manifest whose top level is a list (or any other
    non-mapping) must fail closed with an actionable IntakeSecurityError,
    not an untyped AttributeError escaping from `data.get(...)`."""
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(root, "- just\n- a\n- list\n")

    with pytest.raises(helper.IntakeSecurityError, match="mapping"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_entries_value_not_a_list(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(root, "entries: 5\n")

    with pytest.raises(helper.IntakeSecurityError, match="must be a list"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_entry_not_a_mapping(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(root, "entries:\n  - just-a-string\n")

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_entry_missing_required_field(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(root, "entries:\n  - id: foo\n    classification: bar\n")

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_paths_value_not_a_list(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(
        root,
        "entries:\n"
        "  - id: foo\n"
        "    classification: bar\n"
        "    direct_edit: forbidden\n"
        "    paths: not-a-list\n",
    )

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_paths_element_not_a_string(tmp_path: Path, helper) -> None:
    """`paths`/`path_patterns` passing the container-shape (list) check is
    not enough: a non-string element (e.g. an int, from a manifest typo
    like `paths: [5]`) would otherwise reach `verify_generated_boundary`'s
    `p.rstrip("/")` uncaught, raising a raw AttributeError instead of this
    module's own IntakeSecurityError."""
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(
        root,
        "entries:\n"
        "  - id: foo\n"
        "    classification: bar\n"
        "    direct_edit: forbidden\n"
        "    paths:\n"
        "      - 5\n",
    )

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_rejects_path_patterns_element_not_a_string(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(
        root,
        "entries:\n"
        "  - id: foo\n"
        "    classification: bar\n"
        "    direct_edit: forbidden\n"
        "    path_patterns:\n"
        "      - true\n",
    )

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_raises_intake_error_for_invalid_yaml(tmp_path: Path, helper) -> None:
    """Malformed YAML syntax must surface as this module's own
    IntakeSecurityError (exit code 2, `error: ...` message), not an
    uncaught `yaml.YAMLError` traceback escaping `main()`'s `except
    IntakeError` handler."""
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(root, "entries: [unterminated\n")

    with pytest.raises(helper.IntakeSecurityError, match="not valid YAML"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_raises_intake_error_for_non_utf8_file(tmp_path: Path, helper) -> None:
    """Malformed encoding must surface as this module's own
    IntakeSecurityError, not an uncaught `UnicodeDecodeError` traceback."""
    root = tmp_path / "repo"
    ownership_dir = root / "release"
    ownership_dir.mkdir(parents=True)
    (ownership_dir / "source-ownership.yaml").write_bytes(b"entries: [\xff\xfe]\n")

    with pytest.raises(helper.IntakeSecurityError, match="not valid UTF-8"):
        helper.load_ownership_entries(root)


def test_load_ownership_entries_accepts_well_formed_manifest(tmp_path: Path, helper) -> None:
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(
        root,
        "entries:\n"
        "  - id: foo\n"
        "    classification: bar\n"
        "    direct_edit: forbidden\n"
        "    generated: true\n"
        "    paths:\n"
        "      - some/path\n",
    )

    entries = helper.load_ownership_entries(root)

    assert entries["foo"].generated is True
    assert entries["foo"].direct_edit == "forbidden"
    assert entries["foo"].paths == ("some/path",)


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


def test_load_patch_queue_rejects_sidecar_that_is_not_a_mapping(tmp_path: Path, helper) -> None:
    """A `.yaml` sidecar whose top level is a list (or any other
    non-mapping) must fail closed with an actionable IntakeSecurityError,
    not an untyped AttributeError escaping from `data.get(...)`."""
    patches_dir = tmp_path / "patches"
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-bad.patch").write_text("diff --git a/x b/x\n", encoding="utf-8")
    (patches_dir / "001-bad.yaml").write_text("- just\n- a\n- list\n", encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError, match="mapping"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_raises_intake_error_for_invalid_sidecar_yaml(tmp_path: Path, helper) -> None:
    """Malformed YAML syntax in a patch's `.yaml` sidecar must surface as
    this module's own `IntakeSecurityError`, not an uncaught
    `yaml.YAMLError` traceback escaping `main()`'s `except IntakeError`
    handler."""
    patches_dir = tmp_path / "patches"
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-bad.patch").write_text("diff --git a/x b/x\n", encoding="utf-8")
    (patches_dir / "001-bad.yaml").write_text("owner: [unterminated\n", encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError, match="not valid YAML"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_raises_intake_error_for_non_utf8_sidecar(tmp_path: Path, helper) -> None:
    """Malformed encoding in a patch's `.yaml` sidecar must surface as this
    module's own `IntakeSecurityError`, not an uncaught
    `UnicodeDecodeError` traceback."""
    patches_dir = tmp_path / "patches"
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-bad.patch").write_text("diff --git a/x b/x\n", encoding="utf-8")
    (patches_dir / "001-bad.yaml").write_bytes(b"owner: [\xff\xfe]\n")

    with pytest.raises(helper.IntakeSecurityError, match="not valid UTF-8"):
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


def test_cli_git_apply_non_utf8_error_is_typed_without_traceback(
    tmp_path: Path, helper, monkeypatch, capsys
) -> None:
    source_root = tmp_path / "AmbiqSuite"
    (source_root / "mcu").mkdir(parents=True)
    (source_root / "boards").mkdir()
    target_root = tmp_path / "staged"
    target_root.mkdir()
    patch_path = tmp_path / "bad-path.patch"
    patch_path.write_text("diff --git a/include/x.h b/include/x.h\n", encoding="utf-8")

    def fake_run(command, **kwargs):
        assert "text" not in kwargs
        return subprocess.CompletedProcess(
            command,
            returncode=1,
            stdout=b"",
            stderr=b"error: include/non-utf8-\xff.h: No such file\n",
        )

    def fail_during_patch_application(*_args, **_kwargs):
        helper._git_apply(target_root, patch_path, owner="jane")

    monkeypatch.setattr(helper.subprocess, "run", fake_run)
    monkeypatch.setattr(helper, "stage_provider_payload", fail_during_patch_application)

    rc = helper.main(["stage", "--version", "test", "--source-root", str(source_root)])

    captured = capsys.readouterr()
    assert rc == 2
    assert "error: patch 'bad-path.patch'" in captured.err
    assert "include/non-utf8-\ufffd.h" in captured.err
    assert "Traceback" not in captured.err


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


def test_load_patch_queue_rejects_implicit_rename_hidden_by_trailing_space_in_old_header(
    tmp_path: Path, helper
) -> None:
    """Git's own '---'/'+++' header-name extraction (`name_terminate` in
    `apply.c`) does NOT terminate a name at a plain space -- only at '\\t'
    or '\\r' -- so `--- a/notes.md ` (note the trailing space) and
    `+++ b/notes.md` (no trailing space) name two *different* files to
    git: it reads the trailing-space name and writes the no-space name,
    exactly like any other implicit rename. A prior version of
    `_diff_header_path` used `.strip()`, which silently discarded that
    trailing space, making the two names compare equal and letting this
    rename evade the naked-rename guard entirely (this could not, in any
    construction, reach an actually-forbidden path, since the space-suffixed
    OLD name is still the literal string the forbidden-path check inspects
    unchanged -- confirmed separately: the same construction against a
    `lib/**` path is rejected earlier, by the forbidden-path check itself,
    regardless of this fix). This is now rejected via direct header-pair
    comparison, matching git's real name-termination semantics."""
    patches_dir = tmp_path / "patches"
    implicit_rename_patch = (
        "diff --git a/notes.md b/notes.md\n"
        "--- a/notes.md \n"
        "+++ b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-old text\n"
        "+new text\n"
    )
    _write_patch(
        patches_dir,
        "001-trailing-space-rename",
        implicit_rename_patch,
        owner="jane",
        reason="should be rejected",
    )

    with pytest.raises(helper.IntakeSecurityError, match="changes path from"):
        helper.load_patch_queue(patches_dir)


def test_diff_header_path_does_not_strip_trailing_space_from_the_literal_path(
    tmp_path: Path, helper
) -> None:
    """Direct unit coverage of the fix location: a trailing space is part of
    the literal filename to git (terminated only at '\\t'/'\\r'), so it must
    survive extraction rather than being silently stripped."""
    result = helper._diff_header_path(
        "a/x.h ", side="old ('---')", prefix="a/", patch_path=tmp_path / "x.patch"
    )

    assert result == "x.h "


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


def test_load_patch_queue_rejects_implicit_rename_hidden_by_fake_hunk_marker_lookalike(
    tmp_path: Path, helper
) -> None:
    """A line beginning `@@ ` that is not actually a valid Git hunk header
    (e.g. `@@ not a real hunk`) must not be mistaken for one when bounding
    the header-scan region -- doing so previously let such a line falsely
    truncate the scan *before* the real `---`/`+++` pair, hiding an
    ambiguous rename from detection entirely."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/notes.md b/notes.md\n"
        "@@ not a real hunk header, just decoy text\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-SECRET-ARCHIVE\n"
        "+SECRET-ARCHIVE-MOVED\n"
    )
    _write_patch(patches_dir, "001-fake-hunk-marker", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="changes path from"):
        helper.load_patch_queue(patches_dir)


def test_assert_no_ambiguous_path_change_rejects_content_change_with_no_header_pair(
    tmp_path: Path, helper
) -> None:
    """Defense in depth: an entry that reports nonzero added/deleted lines
    via `git apply --numstat` but for which no `---`/`+++` header pair could
    be found in its bounded header-scan region must fail closed rather than
    be silently treated as a no-content entry (e.g. a genuinely empty file
    add/delete), since that combination cannot arise from an honest
    patch."""
    patch_path = tmp_path / "no-header-but-content.patch"
    patch_path.write_text(
        "diff --git a/notes.md b/notes.md\n"
        "@@ -1 +1 @@\n"
        "-old line\n"
        "+new line\n",
        encoding="utf-8",
    )

    with pytest.raises(helper.IntakeSecurityError, match="no '---'/'\\+\\+\\+' header pair"):
        helper._assert_no_ambiguous_path_change(
            patch_path, expected_entry_count=1, entry_has_content_change=[True]
        )


def test_assert_no_ambiguous_path_change_accepts_no_header_pair_when_no_content_change(
    tmp_path: Path, helper
) -> None:
    """Non-regression: an entry that genuinely has no `---`/`+++` header pair
    (e.g. a mode-only change already vetted by `--summary`) and reports no
    added/deleted content lines must still be accepted."""
    patch_path = tmp_path / "mode-only.patch"
    patch_path.write_text(
        "diff --git a/script.sh b/script.sh\nold mode 100644\nnew mode 100755\n",
        encoding="utf-8",
    )

    helper._assert_no_ambiguous_path_change(
        patch_path, expected_entry_count=1, entry_has_content_change=[False]
    )


def test_load_patch_queue_rejects_implicit_rename_hidden_by_bare_carriage_return_desync(
    tmp_path: Path, helper
) -> None:
    """Git splits patch lines on '\\n' only (see git's `linelen()`); a bare
    '\\r' (a CR not immediately followed by '\\n') is just an ordinary byte
    to git, never a line boundary. Before this fix, the header-scan read
    the patch with Python's universal-newlines `read_text()`, which
    silently translates such a bare CR into '\\n' -- letting this decoy
    'index' line's embedded '\\r'-joined '---'/'+++' pair (plus a
    hunk-marker-lookalike truncator) look like a benign, self-consistent
    entry, while git itself never sees that pair at all and instead honors
    the real, later '--- a/lib/...' / '+++ b/allowed.h' pair as an
    unchecked rename that deletes a forbidden `lib/**` archive. Confirmed
    against real `git apply` (not just this scan) via `apply_patch_queue`
    below."""
    patches_dir = tmp_path / "patches"
    patch_bytes = (
        b"diff --git a/allowed.h b/allowed.h\n"
        b"index 1111111..2222222 100644"
        b"\r--- a/allowed.h\r+++ b/allowed.h\r@@ -x\n"
        b"--- a/lib/gcc/apollo510/libprotected.a\n"
        b"+++ b/allowed.h\n"
        b"@@ -1 +1 @@\n"
        b"-SECRET-ARCHIVE\n"
        b"+new\n"
    )
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-cr-desync.patch").write_bytes(patch_bytes)
    sidecar = {"owner": "jane", "reason": "should be rejected"}
    (patches_dir / "001-cr-desync.yaml").write_text(yaml.safe_dump(sidecar), encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError, match="carriage return"):
        helper.load_patch_queue(patches_dir)


def test_apply_patch_queue_does_not_apply_bare_carriage_return_desync_rename(tmp_path: Path, helper) -> None:
    """End-to-end confirmation of the same bypass: even if the header-scan
    guard were somehow skipped, the staged tree must never actually lose
    the forbidden `lib/**` file to this patch, because `load_patch_queue`
    (which every real call path goes through before `apply_patch_queue`)
    must reject it first."""
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "allowed.h").write_text("old\n", encoding="utf-8")
    lib_dir = staged / "lib" / "gcc" / "apollo510"
    lib_dir.mkdir(parents=True)
    (lib_dir / "libprotected.a").write_text("SECRET-ARCHIVE\n", encoding="utf-8")

    patches_dir = tmp_path / "patches"
    patch_bytes = (
        b"diff --git a/allowed.h b/allowed.h\n"
        b"index 1111111..2222222 100644"
        b"\r--- a/allowed.h\r+++ b/allowed.h\r@@ -x\n"
        b"--- a/lib/gcc/apollo510/libprotected.a\n"
        b"+++ b/allowed.h\n"
        b"@@ -1 +1 @@\n"
        b"-SECRET-ARCHIVE\n"
        b"+new\n"
    )
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-cr-desync.patch").write_bytes(patch_bytes)
    sidecar = {"owner": "jane", "reason": "should be rejected"}
    (patches_dir / "001-cr-desync.yaml").write_text(yaml.safe_dump(sidecar), encoding="utf-8")

    with pytest.raises(helper.IntakeSecurityError, match="carriage return"):
        helper.apply_patch_queue(staged, patches_dir)

    # The forbidden archive must still be present and untouched.
    assert (lib_dir / "libprotected.a").read_text(encoding="utf-8") == "SECRET-ARCHIVE\n"


def test_assert_no_index_only_mode_change_rejects_bare_carriage_return(tmp_path: Path, helper) -> None:
    """The same bare-CR desynchronization risk applies to the mode-only-change
    scan: a decoy 'index' line joined by a bare '\\r' could otherwise hide a
    symlink/submodule mode from `_INDEX_LINE_MODE` while git's own parser
    reads a different mode from what it treats as a single logical line."""
    patch_path = tmp_path / "cr-desync-mode.patch"
    patch_path.write_bytes(
        b"diff --git a/x b/x\nindex 1111111..2222222 100644\r--- a/x\r+++ b/x\r@@ -x\n"
    )

    with pytest.raises(helper.IntakeSecurityError, match="carriage return"):
        helper._assert_no_index_only_mode_change(patch_path)


def test_load_patch_queue_accepts_patch_with_crlf_line_endings(tmp_path: Path, helper) -> None:
    """Non-regression: an ordinary patch using CRLF ('\\r\\n', not a bare
    '\\r') line endings throughout -- as e.g. a Windows-authored patch might
    use -- must still be accepted normally. Every '\\r' here is immediately
    followed by '\\n', so it is not the desynchronization case rejected
    above, and the header/mode scans already tolerate a trailing '\\r'
    before end-of-line via `.strip()` / `\\s*$`."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/include/a.h b/include/a.h\r\n"
        "index e69de29..1111111 100644\r\n"
        "--- a/include/a.h\r\n"
        "+++ b/include/a.h\r\n"
        "@@ -1 +1 @@\r\n"
        "-#define A 1\r\n"
        "+#define A 2\r\n"
    )
    patches_dir.mkdir(parents=True)
    (patches_dir / "001-crlf.patch").write_bytes(patch_text.encode("utf-8"))
    sidecar = {"owner": "jane", "reason": "CRLF patch"}
    (patches_dir / "001-crlf.yaml").write_text(yaml.safe_dump(sidecar), encoding="utf-8")

    queue = helper.load_patch_queue(patches_dir)

    assert [p.slug for p in queue] == ["001-crlf"]


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


def test_load_patch_queue_rejects_uncorroborated_dev_null_deletion_of_forbidden_path(
    tmp_path: Path, helper
) -> None:
    """A `--- a/lib/...` / `+++ /dev/null` pair with no `deleted file mode`
    header for this entry is NOT a safe, unambiguous deletion: `git apply`
    honors it as an ordinary rename onto a real file literally named
    `dev/null` on disk (consuming the forbidden source's content), while
    `git apply --numstat` only ever reports the destination name
    (`dev/null`) -- never the forbidden `lib/...` source -- letting this
    entirely evade `_assert_patch_paths_allowed`'s forbidden-path check.
    Confirmed empirically against real `git apply` (git 2.50.1): applying
    this exact patch text deletes the source file and creates a real file
    named `dev/null` with the patched content, with `--numstat`/`--summary`
    reporting nothing about the source path at all."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "index 1111111..2222222 100644\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ /dev/null\n"
        "@@ -1 +1,2 @@\n"
        " PROPRIETARY-ARCHIVE-BYTES\n"
        "+INJECTED-BY-ATTACKER\n"
    )
    _write_patch(patches_dir, "001-devnull-launder", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="deleted file mode"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_uncorroborated_dev_null_creation_reading_forbidden_content(
    tmp_path: Path, helper
) -> None:
    """The mirror-image bypass: a `--- /dev/null` / `+++ b/allowed` pair with
    no `new file mode` header for this entry is likewise not a safe,
    unambiguous creation -- `git apply` will honor it as a rename *from* a
    real file literally named `dev/null` (e.g. one created by a prior,
    similarly-uncorroborated entry), completing a two-patch laundering
    chain that moves a forbidden source's content into an allowed
    destination while every gate reports only allowed touched paths."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/dev/null b/am_hal.h\n"
        "index 1111111..2222222 100644\n"
        "--- /dev/null\n"
        "+++ b/am_hal.h\n"
        "@@ -1,2 +1,2 @@\n"
        " PROPRIETARY-ARCHIVE-BYTES\n"
        "-INJECTED-BY-ATTACKER\n"
        "+LAUNDERED-INTO-ALLOWED-PATH\n"
    )
    _write_patch(patches_dir, "001-devnull-launder-2", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="new file mode"):
        helper.load_patch_queue(patches_dir)


def test_apply_patch_queue_does_not_launder_forbidden_archive_through_dev_null_chain(
    tmp_path: Path, helper
) -> None:
    """End-to-end confirmation of the full two-patch laundering chain: even
    if the header-scan guard were somehow skipped for one patch, the
    forbidden `lib/**` archive's content must never actually reach an
    allowed destination, because `load_patch_queue` (which every real call
    path goes through before `apply_patch_queue`) must reject the very
    first uncorroborated `/dev/null` entry."""
    staged = tmp_path / "staged"
    staged.mkdir()
    lib_dir = staged / "lib" / "gcc" / "apollo510"
    lib_dir.mkdir(parents=True)
    (lib_dir / "libam_hal.a").write_text("PROPRIETARY-ARCHIVE-BYTES\n", encoding="utf-8")

    patches_dir = tmp_path / "patches"
    patch_one = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "index 1111111..2222222 100644\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ /dev/null\n"
        "@@ -1 +1,2 @@\n"
        " PROPRIETARY-ARCHIVE-BYTES\n"
        "+INJECTED-BY-ATTACKER\n"
    )
    patch_two = (
        "diff --git a/dev/null b/am_hal.h\n"
        "index 1111111..2222222 100644\n"
        "--- /dev/null\n"
        "+++ b/am_hal.h\n"
        "@@ -1,2 +1,2 @@\n"
        " PROPRIETARY-ARCHIVE-BYTES\n"
        "-INJECTED-BY-ATTACKER\n"
        "+LAUNDERED-INTO-ALLOWED-PATH\n"
    )
    _write_patch(patches_dir, "001-devnull-launder", patch_one, owner="jane", reason="should be rejected")
    _write_patch(patches_dir, "002-devnull-launder", patch_two, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError):
        helper.apply_patch_queue(staged, patches_dir)

    # The forbidden archive must still be present and untouched, and no
    # laundered destination file must exist.
    assert (lib_dir / "libam_hal.a").read_text(encoding="utf-8") == "PROPRIETARY-ARCHIVE-BYTES\n"
    assert not (staged / "am_hal.h").exists()
    assert not (staged / "dev" / "null").exists()


def test_load_patch_queue_accepts_genuine_file_deletion_with_dev_null(tmp_path: Path, helper) -> None:
    """Non-regression: a real, unambiguous file deletion (`deleted file
    mode` header present, matching `git apply --summary`'s own report) must
    still be accepted; the deleted path is reported by `--numstat` and thus
    still subject to the ordinary forbidden-path check on its own merits
    (not exercised by this test, which uses an allowed path)."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/include/old.h b/include/old.h\n"
        "deleted file mode 100644\n"
        "index 1111111..0000000\n"
        "--- a/include/old.h\n"
        "+++ /dev/null\n"
        "@@ -1 +0,0 @@\n"
        "-#define OLD 1\n"
    )
    _write_patch(patches_dir, "001-delete", patch_text, owner="jane", reason="remove stale header")

    queue = helper.load_patch_queue(patches_dir)

    assert [m.slug for m in queue] == ["001-delete"]


def test_load_patch_queue_accepts_genuine_file_creation_from_dev_null(tmp_path: Path, helper) -> None:
    """Non-regression: a real, unambiguous file creation (`new file mode`
    header present) must still be accepted."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/include/new.h b/include/new.h\n"
        "new file mode 100644\n"
        "index 0000000..1111111\n"
        "--- /dev/null\n"
        "+++ b/include/new.h\n"
        "@@ -0,0 +1 @@\n"
        "+#define NEW 1\n"
    )
    _write_patch(patches_dir, "001-create", patch_text, owner="jane", reason="add new header")

    queue = helper.load_patch_queue(patches_dir)

    assert [m.slug for m in queue] == ["001-create"]


def test_assert_no_index_only_mode_change_rejects_symlink_mode_with_trailing_garbage(
    tmp_path: Path, helper
) -> None:
    """Git's `gitdiff_index()` locates the mode token after the '..' and
    calls `strtoul()` on it, which stops at the first non-octal-digit
    character and ignores anything after -- e.g. a trailing tab plus
    arbitrary text. The old, end-of-line-anchored `_INDEX_LINE_MODE`
    pattern missed this and treated the line as having no mode at all,
    letting an in-place symlink-mode change past this guard (though
    `_assert_no_symlinks` on the resulting tree remains a second layer)."""
    patch_path = tmp_path / "index-mode-trailing-garbage.patch"
    patch_path.write_text(
        "diff --git a/x b/x\nindex 1111111..2222222 120000\tjunk\n--- a/x\n+++ b/x\n@@ -1 +1 @@\n-old\n+new\n",
        encoding="utf-8",
    )

    with pytest.raises(helper.IntakeSecurityError, match="non-regular-file mode"):
        helper._assert_no_index_only_mode_change(patch_path)


def test_load_ownership_entries_rejects_non_string_id(tmp_path: Path, helper) -> None:
    """An `id` that is not a string (e.g. a YAML list or mapping) must fail
    closed with an actionable IntakeSecurityError inside the same
    malformed-entry handling as every other shape check here, not an
    untyped, uncaught `TypeError: unhashable type` escaping from the
    `entries[entry.entry_id] = entry` dict insertion one line outside the
    original `try` block."""
    root = tmp_path / "repo"
    root.mkdir()
    _write_ownership_manifest(
        root,
        "entries:\n"
        "  - id: [not, a, string]\n"
        "    classification: bar\n"
        "    direct_edit: forbidden\n",
    )

    with pytest.raises(helper.IntakeSecurityError, match="malformed entry"):
        helper.load_ownership_entries(root)


def test_load_patch_queue_rejects_deletion_where_summary_and_numstat_paths_disagree(
    tmp_path: Path, helper
) -> None:
    """When an entry's 'deleted file mode' extended-header line is placed
    *after* its '---'/'+++' pair (a syntactically valid but non-standard
    ordering no honest 'git diff'/'format-patch' ever produces), Git parses
    the '---'/'+++' pair first -- setting old_name/new_name from whatever
    they literally say -- and only overwrites old_name from the 'diff
    --git' line's own path afterwards. This makes 'git apply --numstat'
    report a literal 'dev/null' path (from the uncorroborated-at-parse-time
    '+++ /dev/null' line) while 'git apply --summary' correctly reports
    the true deleted path (the forbidden 'lib/...' archive). Confirmed
    empirically against real 'git apply' (git 2.50.1): this patch deletes
    the real forbidden archive on disk while '--numstat' never once
    reports its path, evading `_assert_patch_paths_allowed` entirely
    (which only ever inspects '--numstat' output). Requiring `--summary`'s
    and `--numstat`'s reported paths to agree for every create/delete
    entry closes this."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ /dev/null\n"
        "deleted file mode 100644\n"
        "@@ -1 +0,0 @@\n"
        "-PROPRIETARY-ARCHIVE-BYTES\n"
    )
    _write_patch(patches_dir, "001-devnull-order-launder", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="not among the target paths"):
        helper.load_patch_queue(patches_dir)


def test_load_patch_queue_rejects_forbidden_deletion_disguised_behind_allowed_headers(
    tmp_path: Path, helper
) -> None:
    """The general form of the same bypass, with no '/dev/null' involved at
    all: the 'diff --git' line (and thus 'git apply --summary', which
    resolves create/delete identity from it) names the real, forbidden
    manifest path, while the '---'/'+++' pair -- placed before the
    'deleted file mode' line -- names a completely different, allowed
    decoy path. Because both header names agree with each other
    ('allowed.h' == 'allowed.h'), `_assert_no_ambiguous_path_change`'s
    rename check does not fire either; only cross-checking `--summary`'s
    authoritative create/delete path against `--numstat`'s reported
    target catches this."""
    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/artifact-manifest.yaml b/artifact-manifest.yaml\n"
        "--- a/allowed.h\n"
        "+++ b/allowed.h\n"
        "deleted file mode 100644\n"
        "@@ -1 +0,0 @@\n"
        "-hello\n"
    )
    _write_patch(patches_dir, "001-decoy-headers-launder", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError, match="not among the target paths"):
        helper.load_patch_queue(patches_dir)


def test_apply_patch_queue_does_not_delete_forbidden_archive_via_reordered_dev_null_header(
    tmp_path: Path, helper
) -> None:
    """End-to-end confirmation that the forbidden archive's content never
    actually reaches disk deletion via the reordered-header bypass, even
    if the header-scan guard alone were somehow skipped: every real call
    path goes through `load_patch_queue` before `apply_patch_queue`, which
    must reject this patch outright."""
    staged = tmp_path / "staged"
    staged.mkdir()
    lib_dir = staged / "lib" / "gcc" / "apollo510"
    lib_dir.mkdir(parents=True)
    (lib_dir / "libam_hal.a").write_text("PROPRIETARY-ARCHIVE-BYTES\n", encoding="utf-8")

    patches_dir = tmp_path / "patches"
    patch_text = (
        "diff --git a/lib/gcc/apollo510/libam_hal.a b/lib/gcc/apollo510/libam_hal.a\n"
        "--- a/lib/gcc/apollo510/libam_hal.a\n"
        "+++ /dev/null\n"
        "deleted file mode 100644\n"
        "@@ -1 +0,0 @@\n"
        "-PROPRIETARY-ARCHIVE-BYTES\n"
    )
    _write_patch(patches_dir, "001-devnull-order-launder", patch_text, owner="jane", reason="should be rejected")

    with pytest.raises(helper.IntakeSecurityError):
        helper.apply_patch_queue(staged, patches_dir)

    assert (lib_dir / "libam_hal.a").read_text(encoding="utf-8") == "PROPRIETARY-ARCHIVE-BYTES\n"


def test_assert_no_index_only_mode_change_rejects_symlink_mode_with_non_hex_object_ids(
    tmp_path: Path, helper
) -> None:
    """Git's own `gitdiff_index()` performs no hex-digit or non-empty
    validation on the object-id substrings either side of the '..' -- it
    only checks for a '.' followed by another '.', then reads the mode
    token after the next space. A line like 'index ..2222222 120000' (an
    empty object id on the old side) is read by git exactly like a
    well-formed line -- mode 0o120000 -- while the old, hex-anchored
    `_INDEX_LINE_MODE` pattern required one-or-more hex digits on both
    sides and so matched nothing at all for it, hiding a real symlink-mode
    in-place change from this guard entirely."""
    patch_path = tmp_path / "index-mode-non-hex-object-id.patch"
    patch_path.write_text(
        "diff --git a/x b/x\nindex ..2222222 120000\n--- a/x\n+++ b/x\n@@ -1 +1 @@\n-old\n+new\n",
        encoding="utf-8",
    )

    with pytest.raises(helper.IntakeSecurityError, match="non-regular-file mode"):
        helper._assert_no_index_only_mode_change(patch_path)


@pytest.mark.parametrize(
    "separator",
    [
        "\x0c",  # form feed -- the originally-reproduced bypass character
        "\x0b",  # vertical tab
        "\x1c",
        "\x1d",
        "\x1e",
        "\x85",  # NEL
        "\u2028",  # Unicode line separator
        "\u2029",  # Unicode paragraph separator
        "\r",  # plain CR
        "\n",  # git's OWN record terminator for '--summary' -- the one
        # character `split("\n")` (round 8's fix) cannot itself remove;
        # closed instead by `_normalize_patch_target_path` refusing any
        # numstat-reported path that isn't equal to its own `.strip()`,
        # which runs unconditionally before the summary parser ever does
    ],
    ids=lambda s: f"U+{ord(s):04X}",
)
def test_load_patch_queue_rejects_symlink_creation_hidden_by_leading_separator_in_path(
    tmp_path: Path, helper, separator: str
) -> None:
    """`str.splitlines()` treats nine characters as line boundaries,
    including '\\x0c' (form feed), '\\x1e', and '\\u2028' -- but Git's
    `git apply --summary` terminates every record with '\\n' only, and
    never C-quotes the reported path (confirmed empirically: a path
    containing any of these separators, including a real newline via a
    C-quoted 'diff --git' header, prints raw). A 'create mode'/'delete
    mode' path that begins with one of these separators used to split a
    single summary line into two unmatched fragments (e.g. 'create mode
    120000' and the rest of the path), silently skipping both the
    non-regular-file mode check and the numstat cross-check for that
    entry -- letting a brand-new symlink (with no 'index' line for
    `_assert_no_index_only_mode_change` to scan, since it is newly
    created rather than modified in place) through undetected. Every one
    of these characters -- including '\\n' itself, which no amount of
    line-splitting-function substitution can ever disambiguate in
    '--summary' output -- is now refused up front as an unsafe/
    indeterminate numstat target path, before the summary parser runs."""
    patches_dir = tmp_path / "patches"
    if separator == "\n":
        # A literal '\n' inside the 'diff --git' line would corrupt the
        # patch's own line structure, so it must be expressed via Git's
        # C-quoted header form (`"a/...\n...\""`), which Git C-unquotes
        # back to a real newline byte in `def_name`/the reported path.
        patch_text = 'diff --git "a/\\nevil" "b/\\nevil"\nnew file mode 120000\n'
    else:
        patch_text = (
            f"diff --git a/{separator}evil b/{separator}evil\n"
            "new file mode 120000\n"
            "--- /dev/null\n"
            f"+++ b/{separator}evil\n"
            "@@ -0,0 +1 @@\n"
            "+/etc/passwd\n"
            "\\ No newline at end of file\n"
        )
    _write_patch(patches_dir, "001-separator-symlink", patch_text, owner="jane", reason="should be rejected")

    # A raw CR in the path makes Git's own '--git-diff' header parser refuse
    # the patch outright (a distinct, but equally fail-closed, error message)
    # before this module's own whitespace check is ever reached; every other
    # separator is caught by this module's `_normalize_patch_target_path`
    # check.
    expected_message = "could not enumerate paths" if separator == "\r" else "leading or trailing whitespace"
    with pytest.raises(helper.IntakeSecurityError, match=expected_message):
        helper.load_patch_queue(patches_dir)


@pytest.mark.parametrize("raw", ["\nevil", "evil\n", "\x0cevil", "evil ", " evil"])
def test_normalize_patch_target_path_rejects_leading_or_trailing_whitespace(
    tmp_path: Path, helper, raw: str
) -> None:
    """Direct unit coverage of the fix location: any numstat-reported path
    that isn't equal to its own `.strip()` is refused outright, rather than
    silently stripped -- because a leading/trailing whitespace-like
    character (including '\\n' itself) is not unambiguously representable
    in Git's unquoted, '\\n'-terminated 'git apply --summary' output."""
    with pytest.raises(helper.IntakeSecurityError, match="leading or trailing whitespace"):
        helper._normalize_patch_target_path(raw, patch_path=tmp_path / "x.patch")


def test_normalize_patch_target_path_accepts_plain_relative_path(tmp_path: Path, helper) -> None:
    assert helper._normalize_patch_target_path("lib/foo.a", patch_path=tmp_path / "x.patch") == "lib/foo.a"


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
        helper._assert_no_ambiguous_path_change(
            patch_path, expected_entry_count=2, entry_has_content_change=[True]
        )


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


def test_promote_from_staging_fails_closed_with_intake_error_when_staged_root_missing(
    tmp_path: Path, helper, monkeypatch
) -> None:
    """A missing staged payload directory must be reported as an actionable
    `IntakeVerificationError`, not an untyped `FileNotFoundError` escaping
    this fail-closed boundary uncaught."""
    monkeypatch.setattr(helper, "repo_root", lambda: tmp_path)
    staged = tmp_path / "does-not-exist"
    provider = tmp_path / "provider"
    provider.mkdir()

    with pytest.raises(helper.IntakeVerificationError, match="staged payload not found"):
        helper.promote_from_staging(staged, provider, confirm=True)


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


def test_verify_artifact_hashes_raises_intake_error_when_section_value_is_not_a_list(
    tmp_path: Path, helper
) -> None:
    """A `parts`/`boards` section whose value is a scalar or mapping instead
    of a list (e.g. `parts: 5` or `parts: "oops"`) must fail closed with an
    actionable `IntakeVerificationError` before ever attempting to iterate
    it, not an untyped `TypeError`/`AttributeError` escaping the `for entry
    in ...` loop (a bare string is otherwise silently iterable character by
    character in Python, which would produce a confusing downstream
    error)."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text("parts: 5\nboards: []\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="must be a list"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_when_section_value_is_a_string(
    tmp_path: Path, helper
) -> None:
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text('parts: "not-a-list"\nboards: []\n', encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="must be a list"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_for_invalid_yaml(tmp_path: Path, helper) -> None:
    """Malformed YAML syntax in `artifact-manifest.yaml` must surface as
    this module's own `IntakeVerificationError`, not an uncaught
    `yaml.YAMLError` traceback escaping `main()`'s `except IntakeError`
    handler."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_text("parts: [unterminated\n", encoding="utf-8")

    with pytest.raises(helper.IntakeVerificationError, match="not valid YAML"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_verify_artifact_hashes_raises_intake_error_for_non_utf8_file(tmp_path: Path, helper) -> None:
    """Malformed encoding in `artifact-manifest.yaml` must surface as this
    module's own `IntakeVerificationError`, not an uncaught
    `UnicodeDecodeError` traceback."""
    sdk_root = tmp_path / "sdk"
    sdk_root.mkdir()
    manifest_path = sdk_root / "artifact-manifest.yaml"
    manifest_path.write_bytes(b"parts: [\xff\xfe]\n")

    with pytest.raises(helper.IntakeVerificationError, match="not valid UTF-8"):
        helper.verify_artifact_hashes(sdk_root, manifest_path)


def test_cmd_stage_wraps_missing_source_configuration_as_intake_error(
    helper, monkeypatch
) -> None:
    monkeypatch.delenv("AMBIQSUITE_REPO", raising=False)
    args = helper.parse_args(["stage", "--version", "test"])

    with pytest.raises(helper.IntakeError, match="could not resolve AmbiqSuite source") as raised:
        helper._cmd_stage(args)

    assert isinstance(raised.value.__cause__, ValueError)
    assert "pass --source-root, --zip, or --ambiqsuite-repo" in str(raised.value)


def test_cli_stage_reports_nonexistent_source_root_as_typed_error(
    tmp_path: Path, helper, capsys
) -> None:
    missing_source = tmp_path / "missing-AmbiqSuite"

    rc = helper.main(
        ["stage", "--version", "test", "--source-root", str(missing_source)]
    )

    captured = capsys.readouterr()
    assert rc == 2
    assert "error: could not resolve AmbiqSuite source:" in captured.err
    assert str(missing_source) in captured.err
    assert "Traceback" not in captured.err
