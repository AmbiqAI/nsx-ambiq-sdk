#!/usr/bin/env python3
"""Hardened AmbiqSuite intake workflow: stage, diff, verify, and promote.

This tool sits on top of `sdk-intake/build_ambiqsuite.py` and never writes
into the promoted provider tree (`modules/<module>/sdk/`) except through the
explicit, confirmed `promote` subcommand. Its job is to make the risky part of
intake reviewable and fail-closed:

  stage            build the curated payload into a scratch staging
                   directory (never the real provider tree), optionally apply
                   an ordered patch hook, and verify artifact hashes.
  diff             produce a reviewable diff between a staged payload and the
                   currently promoted tree (read-only).
  verify-hashes    recompute sha256 for every promoted/staged HAL/BSP archive
                   and compare against its artifact manifest.
  verify-baseline  same hash verification, defaulted at the currently
                   promoted tree -- the reproducible integrity check for an
                   already-published golden baseline (e.g. v5.2.23). This
                   does not rebuild anything and requires no proprietary
                   source; it only proves the committed archives still match
                   the hashes recorded when they were built.
  verify-ownership cross-check the generated-provider boundary declared in
                   `release/source-ownership.yaml` and validate any patch
                   hook ownership metadata.
  promote          atomically swap a validated staging tree into the real
                   provider tree. Requires --yes. Rolls back on failure.

Security posture: pathlib throughout, subprocess argument lists only (no
shell=True/os.system), fail-closed on any missing/incomplete/ambiguous input,
and every filesystem target is checked to stay inside the repository root
before anything is written or removed.
"""
from __future__ import annotations

import argparse
import difflib
import importlib.util
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

import yaml


# --------------------------------------------------------------------------
# Load sdk-intake/build_ambiqsuite.py as a sibling module by file location,
# not by package import, so this tool works the same way whether invoked as
# `python sdk-intake/intake_workflow.py`, via `uv run`, or loaded by tests.
# --------------------------------------------------------------------------
def _load_build_ambiqsuite():
    module_path = Path(__file__).resolve().parent / "build_ambiqsuite.py"
    spec = importlib.util.spec_from_file_location("nsx_intake_build_ambiqsuite", module_path)
    if spec is None or spec.loader is None:  # pragma: no cover - defensive
        raise ImportError(f"could not load build_ambiqsuite.py from {module_path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


bas = _load_build_ambiqsuite()

TEXT_DIFF_SUFFIXES = (".h", ".hpp", ".inc", ".c", ".md", ".yaml", ".yml", ".txt", ".cmake")
OWNERSHIP_MANIFEST_RELATIVE = Path("release") / "source-ownership.yaml"
GENERATED_PROVIDER_ENTRY_ID = "generated-ambiqsuite-provider"


class IntakeError(Exception):
    """Base class for hardened-intake failures. Always fail closed."""


class IntakeSecurityError(IntakeError):
    """A path-escape, missing-ownership-metadata, or boundary violation."""


class IntakeVerificationError(IntakeError):
    """A hash/manifest verification failure."""


class IntakePatchError(IntakeError):
    """A patch hook failed to validate or reapply."""


# --------------------------------------------------------------------------
# Typed models
# --------------------------------------------------------------------------
@dataclass(frozen=True)
class OwnershipEntry:
    entry_id: str
    classification: str
    generated: bool
    direct_edit: str
    paths: tuple[str, ...] = ()
    path_patterns: tuple[str, ...] = ()


@dataclass(frozen=True)
class PatchMetadata:
    slug: str
    patch_path: Path
    metadata_path: Path
    owner: str
    reason: str
    upstream_ref: str | None = None


@dataclass(frozen=True)
class PatchApplication:
    metadata: PatchMetadata
    applied: bool


@dataclass(frozen=True)
class HashVerificationResult:
    verified: tuple[str, ...] = ()
    mismatched: tuple[str, ...] = ()
    missing: tuple[str, ...] = ()

    @property
    def ok(self) -> bool:
        return not self.mismatched and not self.missing

    def raise_if_not_ok(self, *, label: str) -> None:
        if self.ok:
            return
        detail = []
        if self.missing:
            detail.append("missing: " + ", ".join(sorted(self.missing)))
        if self.mismatched:
            detail.append("hash mismatch: " + ", ".join(sorted(self.mismatched)))
        raise IntakeVerificationError(f"{label}: " + "; ".join(detail))


@dataclass(frozen=True)
class DiffEntry:
    relative_path: str
    change: str  # "added" | "removed" | "modified"
    text_diff: str | None = None
    promoted_hash: str | None = None
    staged_hash: str | None = None


@dataclass(frozen=True)
class DiffResult:
    added: tuple[DiffEntry, ...] = ()
    removed: tuple[DiffEntry, ...] = ()
    modified: tuple[DiffEntry, ...] = ()

    @property
    def is_empty(self) -> bool:
        return not (self.added or self.removed or self.modified)


@dataclass(frozen=True)
class StageResult:
    train_id: str
    version: str
    staged_root: Path
    patch_applications: tuple[PatchApplication, ...] = field(default_factory=tuple)
    hash_verification: HashVerificationResult = field(default_factory=HashVerificationResult)


# --------------------------------------------------------------------------
# Path/boundary guards (fail closed against path traversal / misconfiguration)
# --------------------------------------------------------------------------
def repo_root() -> Path:
    return bas.repo_root()


def assert_within_repo(path: Path, *, label: str) -> Path:
    root = repo_root().resolve()
    resolved = path.resolve()
    if resolved != root and root not in resolved.parents:
        raise IntakeSecurityError(f"{label} resolves outside the repository root: {resolved}")
    return resolved


_SAFE_PATH_SEGMENT = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")


def assert_safe_path_segment(value: str, *, label: str) -> str:
    """Fail closed unless `value` is safe to use as a single path segment.
    Rejects path separators, `..`, and anything else that could retarget a
    path built from `value` (e.g. staging_root's train_id/version) somewhere
    other than the one intended segment below its parent."""
    if not _SAFE_PATH_SEGMENT.match(value) or value in (".", ".."):
        raise IntakeSecurityError(
            f"{label} {value!r} is not a safe path segment (expected to match {_SAFE_PATH_SEGMENT.pattern!r})"
        )
    return value


def staging_root(train_id: str, version: str) -> Path:
    assert_safe_path_segment(train_id, label="train id")
    assert_safe_path_segment(version, label="version")
    return repo_root() / "sdk-intake" / "local" / "staging" / train_id / version


def staging_sdk_root(train_id: str, version: str) -> Path:
    return staging_root(train_id, version) / "sdk"


def default_patches_dir(train_id: str) -> Path:
    assert_safe_path_segment(train_id, label="train id")
    return repo_root() / "sdk-intake" / "patches" / train_id


# --------------------------------------------------------------------------
# Ownership boundary verification
# --------------------------------------------------------------------------
def load_ownership_entries(root: Path | None = None) -> dict[str, OwnershipEntry]:
    manifest_path = (root or repo_root()) / OWNERSHIP_MANIFEST_RELATIVE
    if not manifest_path.is_file():
        raise IntakeSecurityError(f"source-ownership manifest not found: {manifest_path}")
    data = yaml.safe_load(manifest_path.read_text(encoding="utf-8")) or {}
    entries: dict[str, OwnershipEntry] = {}
    for raw in data.get("entries", []):
        entry = OwnershipEntry(
            entry_id=raw["id"],
            classification=raw["classification"],
            generated=bool(raw.get("generated", False)),
            direct_edit=raw["direct_edit"],
            paths=tuple(raw.get("paths", ())),
            path_patterns=tuple(raw.get("path_patterns", ())),
        )
        entries[entry.entry_id] = entry
    return entries


def verify_generated_boundary(train: "bas.TrainSpec") -> OwnershipEntry:
    """Fail closed unless the train's promoted provider path is declared
    `generated: true` with `direct_edit: forbidden` in
    `release/source-ownership.yaml`. This is the guard that keeps the staged
    workflow's real `promote` step targeting only the one path the repository
    has explicitly committed to treating as generated, upstream-derived
    output."""
    entries = load_ownership_entries()
    entry = entries.get(GENERATED_PROVIDER_ENTRY_ID)
    if entry is None:
        raise IntakeSecurityError(
            f"release/source-ownership.yaml is missing the '{GENERATED_PROVIDER_ENTRY_ID}' entry"
        )
    if not entry.generated or entry.direct_edit != "forbidden":
        raise IntakeSecurityError(
            f"'{GENERATED_PROVIDER_ENTRY_ID}' must be generated=true, direct_edit=forbidden; "
            f"got generated={entry.generated}, direct_edit={entry.direct_edit!r}"
        )
    provider_relative = bas.provider_sdk_root(train).resolve().relative_to(repo_root().resolve()).as_posix()
    declared_paths = {p.rstrip("/") for p in entry.paths}
    if provider_relative not in declared_paths:
        raise IntakeSecurityError(
            f"provider path {provider_relative!r} for train {train.train_id!r} is not declared under "
            f"'{GENERATED_PROVIDER_ENTRY_ID}' paths {sorted(declared_paths)!r}; refusing to promote"
        )
    return entry


# --------------------------------------------------------------------------
# Artifact hash verification (works against a staged tree or the promoted
# tree -- the same function is the golden-baseline verification path).
# --------------------------------------------------------------------------
def manifest_path_to_promoted_relative(source_relative: str) -> Path:
    """Map an artifact-manifest.yaml `path` (e.g. `gcc/lib/apollo510/libam_hal.a`,
    relative to the build's artifact root) to the promoted-tree-relative path
    (e.g. `lib/gcc/apollo510/libam_hal.a`), matching
    `build_ambiqsuite.artifact_library_specs`'s destination layout."""
    parts = Path(source_relative).parts
    if len(parts) < 3 or parts[1] != "lib":
        raise IntakeVerificationError(f"unexpected artifact manifest path shape: {source_relative!r}")
    toolchain = parts[0]
    rest = parts[2:]
    return Path("lib") / toolchain / Path(*rest)


def verify_artifact_hashes(sdk_root: Path, manifest_path: Path | None = None) -> HashVerificationResult:
    manifest_path = manifest_path or (sdk_root / "artifact-manifest.yaml")
    if not manifest_path.is_file():
        raise IntakeVerificationError(f"artifact manifest not found: {manifest_path}")
    manifest = yaml.safe_load(manifest_path.read_text(encoding="utf-8")) or {}
    verified: list[str] = []
    mismatched: list[str] = []
    missing: list[str] = []
    for section, artifact_key in (("parts", "hal_artifacts"), ("boards", "bsp_artifacts")):
        for entry in manifest.get(section, []) or []:
            artifacts = entry.get(artifact_key) or {}
            for _toolchain, info in artifacts.items():
                source_relative = info["path"]
                expected_sha256 = info["sha256"]
                promoted_relative = manifest_path_to_promoted_relative(source_relative)
                target = sdk_root / promoted_relative
                label = promoted_relative.as_posix()
                if not target.is_file():
                    missing.append(label)
                    continue
                if bas.sha256(target) == expected_sha256:
                    verified.append(label)
                else:
                    mismatched.append(label)
    if not verified and not mismatched and not missing:
        # A manifest with no `parts`/`boards` artifact entries would otherwise
        # report an empty, vacuously "ok" result. Fail closed instead: a
        # generated payload must always declare at least one verifiable
        # archive, so declaring none is itself a verification failure.
        missing.append("<manifest declares no hal_artifacts/bsp_artifacts entries>")
    return HashVerificationResult(tuple(verified), tuple(mismatched), tuple(missing))


def verify_promoted_baseline(train: "bas.TrainSpec") -> HashVerificationResult:
    """Read-only verification that the already-committed provider payload's
    HAL/BSP archives still match the hashes recorded in its own
    artifact-manifest.yaml. This is the reproducible check for a published
    golden baseline (e.g. v5.2.23): it proves integrity of what is already in
    the repository. It does not and cannot prove the archives were built from
    the claimed upstream commit -- that requires access to the proprietary
    AmbiqSuite source at the exact recorded commit plus the exact licensed
    toolchains, which is a manual/internal step outside this repository and
    outside what this tool can verify from committed material alone."""
    provider_root = bas.provider_sdk_root(train)
    if not provider_root.is_dir():
        raise IntakeVerificationError(f"promoted provider tree not found: {provider_root}")
    return verify_artifact_hashes(provider_root)


# --------------------------------------------------------------------------
# Patch hook: ordered, ownership-tagged, fail-closed on reapplication failure
# --------------------------------------------------------------------------
# Patches are for exceptional fixes to generated headers/system sources, never
# to the prebuilt binaries or the manifest that hash-verifies them -- allowing
# either would let a patch make a tampered archive verify against a manifest
# it also rewrote. Enforced structurally: any patch that touches these paths
# is rejected before it is ever applied.
FORBIDDEN_PATCH_PATH_PREFIXES = ("lib/",)
FORBIDDEN_PATCH_PATH_NAMES = ("artifact-manifest.yaml",)


def _patch_target_paths(patch_path: Path) -> list[str]:
    """List the paths a patch would touch, without applying it, via
    `git apply --numstat` (works standalone; no repository context needed)."""
    result = subprocess.run(
        ["git", "apply", "--numstat", str(patch_path)],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        raise IntakeSecurityError(
            f"could not enumerate paths touched by patch {patch_path.name!r}: {result.stderr.strip()}"
        )
    paths: list[str] = []
    for line in result.stdout.splitlines():
        fields = line.split("\t")
        if len(fields) == 3:
            paths.append(fields[2])
    return paths


def _assert_patch_paths_allowed(patch_path: Path) -> None:
    for target in _patch_target_paths(patch_path):
        normalized = target.replace("\\", "/")
        if normalized in FORBIDDEN_PATCH_PATH_NAMES or any(
            normalized.startswith(prefix) for prefix in FORBIDDEN_PATCH_PATH_PREFIXES
        ):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} touches {target!r}, which the patch hook may not modify "
                "(prebuilt archives and the artifact manifest are hash-verified output, not patch targets)"
            )


def load_patch_queue(patches_dir: Path) -> list[PatchMetadata]:
    if not patches_dir.is_dir():
        return []
    queue: list[PatchMetadata] = []
    for patch_path in sorted(patches_dir.glob("*.patch")):
        metadata_path = patch_path.with_suffix(".yaml")
        if not metadata_path.is_file():
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} has no ownership sidecar {metadata_path.name!r}; "
                "every patch must declare an owner and reason before it can be applied"
            )
        data = yaml.safe_load(metadata_path.read_text(encoding="utf-8")) or {}
        owner = str(data.get("owner") or "").strip()
        reason = str(data.get("reason") or "").strip()
        if not owner or not reason:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} sidecar {metadata_path.name!r} must declare non-empty "
                "'owner' and 'reason' fields"
            )
        _assert_patch_paths_allowed(patch_path)
        upstream_ref = data.get("upstream_ref")
        queue.append(
            PatchMetadata(
                slug=patch_path.stem,
                patch_path=patch_path,
                metadata_path=metadata_path,
                owner=owner,
                reason=reason,
                upstream_ref=str(upstream_ref) if upstream_ref else None,
            )
        )
    return queue


def _git_apply(target_root: Path, patch_path: Path, *, owner: str) -> None:
    check = subprocess.run(
        ["git", "-C", str(target_root), "apply", "--check", str(patch_path)],
        capture_output=True,
        text=True,
    )
    if check.returncode != 0 or "Skipped patch" in check.stderr:
        raise IntakePatchError(
            f"patch {patch_path.name!r} (owner={owner}) failed to reapply against the generated tree "
            f"-- upstream content likely drifted since the patch was written. "
            f"git apply --check stderr: {check.stderr.strip()}"
        )
    apply_result = subprocess.run(
        ["git", "-C", str(target_root), "apply", str(patch_path)],
        capture_output=True,
        text=True,
    )
    if apply_result.returncode != 0 or "Skipped patch" in apply_result.stderr:
        # `git apply` can exit 0 while printing "Skipped patch ..." and
        # leaving the tree unchanged (e.g. under path-exclusion rules).
        # Treat that the same as an outright failure: never report a patch as
        # applied when it was not, silently or otherwise.
        raise IntakePatchError(
            f"patch {patch_path.name!r} (owner={owner}) did not apply cleanly despite passing --check: "
            f"{apply_result.stderr.strip()}"
        )


def apply_patch_queue(staged_sdk_root: Path, patches_dir: Path | None) -> tuple[PatchApplication, ...]:
    """Apply every `*.patch` file under `patches_dir` (ordered by filename) to
    `staged_sdk_root`. The whole queue is first rehearsed end-to-end against a
    throwaway copy of the staged tree; only if every patch in the queue
    applies cleanly there does this function touch `staged_sdk_root` at all.
    That guarantees a mid-queue reapplication failure never leaves the real
    staged tree partially patched. `git apply` (without --unsafe-paths)
    refuses to write outside the directory it targets, so a patch cannot
    escape the tree it is rehearsed or applied against."""
    if patches_dir is None or not patches_dir.is_dir():
        return ()
    queue = load_patch_queue(patches_dir)
    if not queue:
        return ()

    rehearsal_root = staged_sdk_root.parent / f"{staged_sdk_root.name}.patch-rehearsal"
    if rehearsal_root.exists():
        shutil.rmtree(rehearsal_root)
    shutil.copytree(staged_sdk_root, rehearsal_root)
    try:
        for metadata in queue:
            _git_apply(rehearsal_root, metadata.patch_path, owner=metadata.owner)
    finally:
        shutil.rmtree(rehearsal_root, ignore_errors=True)

    # The full queue is now proven to apply cleanly in isolation, so applying
    # it for real cannot leave staged_sdk_root partially patched.
    for metadata in queue:
        _git_apply(staged_sdk_root, metadata.patch_path, owner=metadata.owner)
    return tuple(PatchApplication(metadata=metadata, applied=True) for metadata in queue)


# --------------------------------------------------------------------------
# Reviewable diff between a staged payload and the currently promoted tree
# --------------------------------------------------------------------------
def _tracked_files(root: Path) -> set[str]:
    if not root.is_dir():
        return set()
    return {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}


def _is_text(path: Path) -> bool:
    return path.suffix in TEXT_DIFF_SUFFIXES


def compare_trees(staged_root: Path, promoted_root: Path) -> DiffResult:
    """Read-only comparison; never writes to either tree."""
    staged_files = _tracked_files(staged_root)
    promoted_files = _tracked_files(promoted_root)

    added = tuple(
        DiffEntry(relative_path=rel, change="added", staged_hash=bas.sha256(staged_root / rel))
        for rel in sorted(staged_files - promoted_files)
    )
    removed = tuple(
        DiffEntry(relative_path=rel, change="removed", promoted_hash=bas.sha256(promoted_root / rel))
        for rel in sorted(promoted_files - staged_files)
    )
    modified: list[DiffEntry] = []
    for rel in sorted(staged_files & promoted_files):
        promoted_file = promoted_root / rel
        staged_file = staged_root / rel
        promoted_hash = bas.sha256(promoted_file)
        staged_hash = bas.sha256(staged_file)
        if promoted_hash == staged_hash:
            continue
        text_diff = None
        if _is_text(staged_file):
            try:
                promoted_lines = promoted_file.read_text(encoding="utf-8").splitlines(keepends=True)
                staged_lines = staged_file.read_text(encoding="utf-8").splitlines(keepends=True)
                text_diff = "".join(
                    difflib.unified_diff(
                        promoted_lines,
                        staged_lines,
                        fromfile=f"promoted/{rel}",
                        tofile=f"staged/{rel}",
                    )
                )
            except UnicodeDecodeError:
                text_diff = None
        modified.append(
            DiffEntry(
                relative_path=rel,
                change="modified",
                text_diff=text_diff,
                promoted_hash=promoted_hash,
                staged_hash=staged_hash,
            )
        )
    return DiffResult(added=added, removed=removed, modified=tuple(modified))


def render_diff_report(diff: DiffResult) -> str:
    if diff.is_empty:
        return "no differences between the staged payload and the promoted tree\n"
    lines: list[str] = []
    if diff.added:
        lines.append(f"Added ({len(diff.added)}):")
        lines.extend(f"  + {entry.relative_path}  sha256={entry.staged_hash}" for entry in diff.added)
    if diff.removed:
        lines.append(f"Removed ({len(diff.removed)}):")
        lines.extend(f"  - {entry.relative_path}  sha256={entry.promoted_hash}" for entry in diff.removed)
    if diff.modified:
        lines.append(f"Modified ({len(diff.modified)}):")
        for entry in diff.modified:
            lines.append(f"  ~ {entry.relative_path}")
            if entry.text_diff:
                lines.append(entry.text_diff.rstrip("\n"))
            else:
                lines.append(f"    (binary changed) sha256 {entry.promoted_hash} -> {entry.staged_hash}")
    return "\n".join(lines) + "\n"


# --------------------------------------------------------------------------
# Staging: generate the full curated payload into a scratch directory
# --------------------------------------------------------------------------
def stage_provider_payload(
    train: "bas.TrainSpec",
    version: str,
    sdk_root: Path,
    *,
    patches_dir: Path | None,
) -> StageResult:
    verify_generated_boundary(train)

    incomplete = bas.missing_artifact_libraries(train, version)
    if incomplete or not bas.built_artifact_toolchains(train, version):
        detail = ", ".join(incomplete) if incomplete else "no built toolchains"
        raise IntakeVerificationError(
            f"refusing to stage {train.train_id} {version}: incomplete artifact set on disk ({detail}); "
            "run build_ambiqsuite.py to build the full train for this version before staging"
        )

    dest = staging_sdk_root(train.train_id, version)
    assert_within_repo(dest, label="staging destination")
    bas.promote_provider_payload(train, version, sdk_root, destination_root=dest)

    resolved_patches_dir = patches_dir if patches_dir is not None else default_patches_dir(train.train_id)
    if resolved_patches_dir.is_dir():
        assert_within_repo(resolved_patches_dir, label="patches directory")
    patch_applications = apply_patch_queue(dest, resolved_patches_dir)

    verification = verify_artifact_hashes(dest, dest / "artifact-manifest.yaml")
    verification.raise_if_not_ok(label=f"staged artifact hashes for {train.train_id} {version}")

    return StageResult(
        train_id=train.train_id,
        version=version,
        staged_root=dest,
        patch_applications=patch_applications,
        hash_verification=verification,
    )


# --------------------------------------------------------------------------
# Promotion: explicit, confirmed, atomic swap with rollback on failure
# --------------------------------------------------------------------------
def promote_from_staging(staged_sdk_root: Path, provider_root: Path, *, confirm: bool) -> None:
    if not confirm:
        raise IntakeSecurityError("promotion requires explicit confirmation (--yes); refusing to proceed")
    assert_within_repo(staged_sdk_root, label="staging source")
    assert_within_repo(provider_root, label="promotion destination")
    if not staged_sdk_root.is_dir():
        raise FileNotFoundError(f"staged payload not found: {staged_sdk_root}")

    tmp = provider_root.with_name(provider_root.name + ".promote-tmp")
    backup = provider_root.with_name(provider_root.name + ".promote-backup")
    if not provider_root.exists() and backup.exists():
        # A previous promotion was interrupted between renaming the provider
        # tree aside and renaming the new tree into place. Refuse to proceed
        # (fail closed) rather than silently discarding that backup or
        # guessing what state the repository is in; a maintainer must resolve
        # it explicitly. Checked before any other work so a broken system
        # state is never masked by an unrelated hash-verification failure.
        raise IntakeSecurityError(
            f"found leftover promotion backup {backup} with no provider tree at {provider_root}; "
            f"a previous promotion was interrupted mid-swap. Restore it manually "
            f"(e.g. `mv {backup} {provider_root}`) and confirm the tree is correct before retrying."
        )

    verification = verify_artifact_hashes(staged_sdk_root, staged_sdk_root / "artifact-manifest.yaml")
    verification.raise_if_not_ok(label=f"staged payload at {staged_sdk_root}")

    if tmp.exists():
        shutil.rmtree(tmp)

    shutil.copytree(staged_sdk_root, tmp)
    try:
        if provider_root.exists():
            provider_root.rename(backup)
        tmp.rename(provider_root)
    except BaseException:
        # Roll back to the pre-promotion state on ANY interruption -- not just
        # OSError, but also KeyboardInterrupt/SystemExit -- so a maintainer
        # hitting Ctrl-C mid-swap can never leave the provider tree missing.
        if not provider_root.exists() and backup.exists():
            backup.rename(provider_root)
        raise
    else:
        if backup.exists():
            shutil.rmtree(backup)
    finally:
        if tmp.exists():
            shutil.rmtree(tmp)


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------
def _add_source_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--source-root", type=Path, help="Path to an AmbiqSuite source tree.")
    parser.add_argument("--zip", dest="zip_path", type=Path, help="Path to an AmbiqSuite zip drop.")
    parser.add_argument("--extract-dir", type=Path, help="Local extraction directory for --zip.")
    parser.add_argument("--force-extract", action="store_true", help="Remove and re-extract the SDK drop before staging.")
    parser.add_argument("--ambiqsuite-repo", type=Path, help="Path to the AmbiqSuite Git checkout (or set AMBIQSUITE_REPO).")
    parser.add_argument("--source-ref", help="AmbiqSuite Git tag, branch, or commit to materialize. Default: stable.")
    parser.add_argument("--source-worktree-dir", type=Path, help="Directory for the materialized AmbiqSuite Git worktree.")
    parser.add_argument("--force-source-ref", action="store_true", help="Recreate the materialized AmbiqSuite Git worktree before staging.")


def _resolve_train(train_id: str) -> "bas.TrainSpec":
    if train_id not in bas.TRAINS:
        raise IntakeError(f"unknown train {train_id!r}; known trains: {sorted(bas.TRAINS)}")
    return bas.TRAINS[train_id]


def _cmd_stage(args: argparse.Namespace) -> int:
    train = _resolve_train(args.train)
    sdk_root, _kind, _ref, _commit = bas.resolve_source_root(args)
    patches_dir = args.patches_dir
    result = stage_provider_payload(train, args.version, sdk_root, patches_dir=patches_dir)
    print(f"==> Staged {result.train_id} {result.version} at {bas.display_path(result.staged_root)}")
    print(f"    artifact hashes verified: {len(result.hash_verification.verified)}")
    if result.patch_applications:
        print(f"    patches applied ({len(result.patch_applications)}):")
        for application in result.patch_applications:
            print(f"      {application.metadata.patch_path.name} (owner={application.metadata.owner})")
    else:
        print("    patches applied: none")
    return 0


def _cmd_diff(args: argparse.Namespace) -> int:
    train = _resolve_train(args.train)
    if args.staged_dir is None and args.version is None:
        raise IntakeError("diff requires either --staged-dir or --version")
    staged_root = args.staged_dir or staging_sdk_root(train.train_id, args.version)
    promoted_root = args.promoted_dir or bas.provider_sdk_root(train)
    diff = compare_trees(staged_root, promoted_root)
    report = render_diff_report(diff)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
        print(f"==> Wrote diff report to {args.output}")
    else:
        print(report, end="")
    return 0


def _cmd_verify_hashes(args: argparse.Namespace) -> int:
    result = verify_artifact_hashes(args.sdk_dir, args.manifest)
    label = f"artifact hashes under {args.sdk_dir}"
    if result.ok:
        print(f"==> OK: {label} ({len(result.verified)} verified)")
        return 0
    print(f"error: {label}: missing={list(result.missing)} mismatched={list(result.mismatched)}", file=sys.stderr)
    return 1


def _cmd_verify_baseline(args: argparse.Namespace) -> int:
    train = _resolve_train(args.train)
    result = verify_promoted_baseline(train)
    if result.ok:
        print(f"==> OK: promoted {train.train_id} baseline matches its artifact manifest ({len(result.verified)} archives)")
        print(
            "    note: this proves integrity of the committed archives against their recorded "
            "hashes. It does not re-derive them from AmbiqSuite source; full reproduction requires "
            "manual/internal access to the exact upstream commit and licensed toolchains."
        )
        return 0
    print(f"error: promoted {train.train_id} baseline does not match its artifact manifest: "
          f"missing={list(result.missing)} mismatched={list(result.mismatched)}", file=sys.stderr)
    return 1


def _cmd_verify_ownership(args: argparse.Namespace) -> int:
    train = _resolve_train(args.train)
    try:
        verify_generated_boundary(train)
        if args.patches_dir and args.patches_dir.is_dir():
            load_patch_queue(args.patches_dir)
    except IntakeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"==> OK: ownership boundary for {train.train_id} is intact")
    return 0


def _cmd_promote(args: argparse.Namespace) -> int:
    train = _resolve_train(args.train)
    try:
        verify_generated_boundary(train)
        promote_from_staging(args.staged_dir, bas.provider_sdk_root(train), confirm=args.yes)
    except IntakeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2
    print(f"==> Promoted {bas.display_path(args.staged_dir)} to {bas.display_path(bas.provider_sdk_root(train))}")
    return 0


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    subparsers = parser.add_subparsers(dest="command", required=True)

    stage = subparsers.add_parser("stage", help="Generate the curated payload into a scratch staging directory.")
    stage.add_argument("--train", choices=tuple(bas.TRAINS), default="stable")
    stage.add_argument("--version", required=True, help="Already-built artifact version, e.g. stable-2026.06.18.")
    stage.add_argument("--patches-dir", type=Path, default=None, help="Ordered patch hook directory. Default: sdk-intake/patches/<train>.")
    _add_source_args(stage)
    stage.set_defaults(func=_cmd_stage)

    diff = subparsers.add_parser("diff", help="Show a reviewable diff between a staged payload and the promoted tree.")
    diff.add_argument("--train", choices=tuple(bas.TRAINS), default="stable")
    diff.add_argument("--version", help="Staged version to diff, used to derive the default --staged-dir.")
    diff.add_argument("--staged-dir", type=Path, default=None)
    diff.add_argument("--promoted-dir", type=Path, default=None)
    diff.add_argument("--output", type=Path, default=None, help="Write the report to this file instead of stdout.")
    diff.set_defaults(func=_cmd_diff)

    verify_hashes = subparsers.add_parser("verify-hashes", help="Recompute and compare artifact hashes for an sdk tree.")
    verify_hashes.add_argument("--sdk-dir", type=Path, required=True)
    verify_hashes.add_argument("--manifest", type=Path, default=None, help="Default: <sdk-dir>/artifact-manifest.yaml.")
    verify_hashes.set_defaults(func=_cmd_verify_hashes)

    verify_baseline = subparsers.add_parser("verify-baseline", help="Verify the promoted tree matches its own artifact manifest.")
    verify_baseline.add_argument("--train", choices=tuple(bas.TRAINS), default="stable")
    verify_baseline.set_defaults(func=_cmd_verify_baseline)

    verify_ownership = subparsers.add_parser("verify-ownership", help="Validate the generated-provider ownership boundary.")
    verify_ownership.add_argument("--train", choices=tuple(bas.TRAINS), default="stable")
    verify_ownership.add_argument("--patches-dir", type=Path, default=None)
    verify_ownership.set_defaults(func=_cmd_verify_ownership)

    promote = subparsers.add_parser("promote", help="Atomically promote a validated staged payload into the provider tree.")
    promote.add_argument("--train", choices=tuple(bas.TRAINS), default="stable")
    promote.add_argument("--staged-dir", type=Path, required=True)
    promote.add_argument("--yes", action="store_true", help="Required to confirm the promotion actually runs.")
    promote.set_defaults(func=_cmd_promote)

    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        return args.func(args)
    except IntakeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
