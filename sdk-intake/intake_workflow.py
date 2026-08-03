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
import os
import re
import shutil
import subprocess
import sys
import unicodedata
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

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


def _load_yaml_document(path: Path, *, error_cls: type[IntakeError]) -> object:
    """Read and parse a YAML file, translating decode/parse failures into
    this module's own exception hierarchy.

    Without this, a non-UTF-8 file raises an uncaught `UnicodeDecodeError`
    and a malformed YAML document raises an uncaught `yaml.YAMLError`, both
    escaping `main()`'s `except IntakeError` handler as raw tracebacks
    (exit code 1, no `error: ...` message) instead of being reported the
    same way as every other input-validation failure this tool guards
    against (exit code 2). This is a robustness/UX fix, not a security
    fix -- both failure modes already fail closed (the tool crashes rather
    than proceeding on unparseable input)."""
    try:
        raw_text = path.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        raise error_cls(f"{path} is not valid UTF-8: {error}") from error
    try:
        return yaml.safe_load(raw_text)
    except yaml.YAMLError as error:
        raise error_cls(f"{path} is not valid YAML: {error}") from error


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
    data = _load_yaml_document(manifest_path, error_cls=IntakeSecurityError) or {}
    if not isinstance(data, dict):
        raise IntakeSecurityError(
            f"source-ownership manifest {manifest_path} must be a mapping at its top level, got "
            f"{type(data).__name__}"
        )
    raw_entries = data.get("entries", []) or []
    if not isinstance(raw_entries, list):
        raise IntakeSecurityError(
            f"source-ownership manifest {manifest_path} 'entries' must be a list, got "
            f"{type(raw_entries).__name__}"
        )
    entries: dict[str, OwnershipEntry] = {}
    for raw in raw_entries:
        if not isinstance(raw, dict):
            raise IntakeSecurityError(
                f"source-ownership manifest {manifest_path} contains a malformed entry: expected a "
                f"mapping, got {type(raw).__name__}"
            )
        try:
            entry_id = raw["id"]
            if not isinstance(entry_id, str):
                raise TypeError(f"'id' must be a string, got {type(entry_id).__name__}")
            paths = raw.get("paths", ())
            path_patterns = raw.get("path_patterns", ())
            if not isinstance(paths, (list, tuple)) or not isinstance(path_patterns, (list, tuple)):
                raise TypeError("'paths'/'path_patterns' must be a list")
            if not all(isinstance(p, str) for p in paths) or not all(
                isinstance(p, str) for p in path_patterns
            ):
                raise TypeError("'paths'/'path_patterns' entries must all be strings")
            entry = OwnershipEntry(
                entry_id=entry_id,
                classification=raw["classification"],
                generated=bool(raw.get("generated", False)),
                direct_edit=raw["direct_edit"],
                paths=tuple(paths),
                path_patterns=tuple(path_patterns),
            )
        except (KeyError, TypeError, AttributeError) as error:
            raise IntakeSecurityError(
                f"source-ownership manifest {manifest_path} contains a malformed entry: {error}"
            ) from error
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
    provider_root = bas.provider_sdk_root(train).resolve()
    try:
        provider_relative = provider_root.relative_to(repo_root().resolve()).as_posix()
    except ValueError as error:
        # provider_sdk_root(train) resolved outside repo_root() (e.g. a
        # corrupted/misconfigured train.module_dir containing ".."). Fail
        # closed with a clear IntakeSecurityError instead of an unhandled
        # ValueError escaping this fail-closed boundary check.
        raise IntakeSecurityError(
            f"provider path for train {train.train_id!r} ({provider_root}) resolves outside the "
            f"repository root {repo_root()}; refusing to promote"
        ) from error
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
    if ".." in parts or any(PurePosixPath(part).is_absolute() for part in parts):
        # A manifest entry with a `..` (or an absolute-looking) segment could
        # otherwise resolve outside `sdk_root` entirely, letting
        # `verify_artifact_hashes` "verify" an attacker-controlled file
        # elsewhere on disk instead of the real promoted artifact. Fail
        # closed rather than silently following it.
        raise IntakeVerificationError(
            f"artifact manifest path {source_relative!r} contains an unsafe path segment; refusing to "
            "resolve a path that could escape the SDK root"
        )
    toolchain = parts[0]
    rest = parts[2:]
    return Path("lib") / toolchain / Path(*rest)


def verify_artifact_hashes(sdk_root: Path, manifest_path: Path | None = None) -> HashVerificationResult:
    manifest_path = manifest_path or (sdk_root / "artifact-manifest.yaml")
    if not manifest_path.is_file():
        raise IntakeVerificationError(f"artifact manifest not found: {manifest_path}")
    manifest = _load_yaml_document(manifest_path, error_cls=IntakeVerificationError) or {}
    if not isinstance(manifest, dict):
        raise IntakeVerificationError(
            f"artifact manifest {manifest_path} must be a mapping at its top level, got {type(manifest).__name__}"
        )
    verified: list[str] = []
    mismatched: list[str] = []
    missing: list[str] = []
    for section, artifact_key in (("parts", "hal_artifacts"), ("boards", "bsp_artifacts")):
        section_value = manifest.get(section, []) or []
        if not isinstance(section_value, list):
            raise IntakeVerificationError(
                f"artifact manifest {manifest_path} section {section!r} must be a list, got "
                f"{type(section_value).__name__}"
            )
        for entry in section_value:
            # Every container-shape access below (`entry.get`, `.items()`) is
            # wrapped so a malformed manifest (e.g. a section containing a
            # bare string, or an artifacts value that isn't a mapping) fails
            # closed with an actionable IntakeVerificationError instead of an
            # untyped AttributeError/TypeError escaping this verification
            # boundary uncaught.
            try:
                if not isinstance(entry, dict):
                    raise TypeError(f"{section[:-1]} entry must be a mapping, got {type(entry).__name__}")
                artifacts = entry.get(artifact_key) or {}
                if not isinstance(artifacts, dict):
                    raise TypeError(f"{artifact_key!r} must be a mapping, got {type(artifacts).__name__}")
            except (KeyError, TypeError, AttributeError) as error:
                raise IntakeVerificationError(
                    f"malformed {section[:-1]} entry in {manifest_path}: {error}"
                ) from error
            for toolchain, info in artifacts.items():
                try:
                    source_relative = info["path"]
                    expected_sha256 = info["sha256"]
                    promoted_relative = manifest_path_to_promoted_relative(source_relative)
                except (KeyError, TypeError, AttributeError) as error:
                    # A malformed manifest entry (missing/wrong-typed `path`
                    # or `sha256`) must fail closed with an actionable
                    # IntakeVerificationError, not an untyped KeyError/
                    # TypeError escaping this verification boundary.
                    raise IntakeVerificationError(
                        f"malformed {artifact_key!r} entry for {section[:-1]} "
                        f"{entry.get('logical_skew') or entry.get('nsx_board')!r} toolchain {toolchain!r} "
                        f"in {manifest_path}: {error}"
                    ) from error
                target = sdk_root / promoted_relative
                label = promoted_relative.as_posix()
                resolved_root = sdk_root.resolve()
                resolved_target = target.resolve()
                if resolved_target != resolved_root and resolved_root not in resolved_target.parents:
                    # Defense in depth alongside the '..'/absolute-segment
                    # rejection in `manifest_path_to_promoted_relative`: even
                    # a syntactically clean manifest path could resolve
                    # outside `sdk_root` via a symlink somewhere in the tree.
                    # Never "verify" a file outside the tree this function
                    # was asked to check.
                    raise IntakeVerificationError(
                        f"artifact manifest entry {label!r} in {manifest_path} resolves outside "
                        f"{sdk_root}; refusing to verify a path escaping the SDK root"
                    )
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


def _no_repo_discovery_env(anchor: Path) -> dict[str, str]:
    """Environment for every `git apply`/`git apply --numstat` subprocess this
    module runs, so Git's repository discovery can never walk up from
    `anchor` into whatever repository happens to enclose it.

    In production, the real staging tree (and the patches directory) always
    live *inside* this repository (e.g.
    `sdk-intake/local/staging/<train>/<version>/sdk`). When Git is invoked
    from a directory nested inside a discovered work tree, it computes a
    "prefix" (the path from that work tree's top level down to the invoked
    directory) and -- per `apply.c`'s `use_patch()` -- silently drops every
    hunk whose target path does not happen to start with that prefix. Since
    every patch here is authored with paths relative to the tree it targets
    (not relative to whatever repository contains that tree), this prefix
    filtering would otherwise:
      * make `git apply` "succeed" (exit 0) while leaving the real staged
        tree byte-for-byte unchanged -- a silent no-op, not a failure
        (`_git_apply`'s bug); and
      * make `git apply --numstat` report zero touched paths for the exact
        same patches, which would let a patch touching `lib/**` or
        `artifact-manifest.yaml` sail straight through
        `_assert_patch_paths_allowed` undetected (`_patch_target_paths`'s
        bug) -- the two functions must resolve paths with identical
        no-ambient-repository semantics, not just "the same root".
    Setting `GIT_CEILING_DIRECTORIES` to `anchor`'s parent makes discovery
    stop climbing there, so Git never finds any enclosing `.git` and treats
    `anchor` as if it were not inside any repository at all -- the same,
    unambiguous behavior regardless of the caller's own current working
    directory or how deeply `anchor` happens to be nested under this repo's
    root."""
    resolved_parent = str(anchor.resolve().parent)
    env = dict(os.environ)
    existing = env.get("GIT_CEILING_DIRECTORIES")
    env["GIT_CEILING_DIRECTORIES"] = f"{resolved_parent}{os.pathsep}{existing}" if existing else resolved_parent
    return env


def _normalize_patch_target_path(raw: str, *, patch_path: Path) -> str:
    """Normalize one `git apply --numstat` path field to a safe, POSIX-style
    relative path, or fail closed. An empty, absolute, or `..`-containing
    path is indeterminate -- it cannot be checked against
    `FORBIDDEN_PATCH_PATH_PREFIXES`/`FORBIDDEN_PATCH_PATH_NAMES` with any
    confidence -- so it must be refused rather than silently ignored."""
    candidate = raw.strip().replace("\\", "/")
    while candidate.startswith("./"):
        candidate = candidate[2:]
    parts = PurePosixPath(candidate).parts if candidate else ()
    if not candidate or candidate.startswith("/") or ".." in parts:
        raise IntakeSecurityError(
            f"patch {patch_path.name!r} reports an unsafe or indeterminate target path {raw!r} via "
            "'git apply --numstat'; refusing to evaluate a patch whose targets cannot be resolved "
            "unambiguously"
        )
    return candidate


def _patch_target_paths(patch_path: Path) -> list[str]:
    """List the normalized, POSIX-relative paths a patch would touch, without
    applying it, via `git apply --numstat -z`. Anchored on `patch_path.parent`
    (a stable location independent of the caller's own current working
    directory) with repository discovery disabled (see
    `_no_repo_discovery_env`), so this reports the same paths regardless of
    where this tool is invoked from. `-z` (NUL-terminated, unquoted records)
    is required, not optional: without it, Git C-quotes any path containing
    a non-ASCII byte (e.g. `"lib/gcc/apollo\\303\\251/libam_hal.a"`), which
    would never match `FORBIDDEN_PATCH_PATH_PREFIXES`/`_NAMES` even though
    the literal path plainly does -- a silent, fail-open gap in the
    forbidden-path check for any path outside plain ASCII."""
    anchor = patch_path.parent
    result = subprocess.run(
        ["git", "-C", str(anchor), "apply", "--numstat", "-z", str(patch_path)],
        capture_output=True,
        env=_no_repo_discovery_env(anchor),
    )
    if result.returncode != 0:
        raise IntakeSecurityError(
            f"could not enumerate paths touched by patch {patch_path.name!r}: "
            f"{result.stderr.decode('utf-8', errors='replace').strip()}"
        )
    paths: list[str] = []
    for record in result.stdout.split(b"\0"):
        if not record:
            continue
        fields = record.split(b"\t", 2)
        if len(fields) != 3:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} produced an unparseable 'git apply --numstat -z' record; "
                "refusing to apply a patch whose targets cannot be verified"
            )
        raw_path = fields[2].decode("utf-8", errors="replace")
        paths.append(_normalize_patch_target_path(raw_path, patch_path=patch_path))
    return paths


def _fold_path_for_comparison(value: str) -> str:
    """Canonicalize a path for a forbidden-path membership test that must
    hold regardless of filesystem case-sensitivity or Unicode
    representation. The staged/promoted trees live on the developer's own
    filesystem, which on macOS (APFS/HFS+) is case-insensitive and may
    decompose Unicode (NFD) by default -- so a patch touching `LIB/...` or
    `Artifact-Manifest.yaml`, or a path using a different but
    canonically-equivalent Unicode form, resolves to the exact same
    protected file even though a byte-exact comparison would miss it."""
    return unicodedata.normalize("NFC", value).casefold()


def _assert_patch_paths_allowed(patch_path: Path) -> None:
    targets = _patch_target_paths(patch_path)
    if not targets:
        # A patch that touches nothing (or whose targets could not be
        # enumerated) cannot be proven safe. Fail closed instead of treating
        # "no reported paths" as "nothing forbidden was touched" -- that
        # inference is exactly what silently let forbidden-path patches
        # evade this check when `git apply --numstat` was run from a
        # subdirectory (see `_no_repo_discovery_env`).
        raise IntakeSecurityError(
            f"patch {patch_path.name!r} reports zero target paths via 'git apply --numstat'; refusing "
            "to apply a patch whose targets cannot be verified"
        )
    forbidden_names_folded = {_fold_path_for_comparison(name) for name in FORBIDDEN_PATCH_PATH_NAMES}
    forbidden_prefixes_folded = tuple(_fold_path_for_comparison(p) for p in FORBIDDEN_PATCH_PATH_PREFIXES)
    for normalized in targets:
        folded = _fold_path_for_comparison(normalized)
        if folded in forbidden_names_folded or any(folded.startswith(prefix) for prefix in forbidden_prefixes_folded):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} touches {normalized!r}, which the patch hook may not modify "
                "(prebuilt archives and the artifact manifest are hash-verified output, not patch targets)"
            )


# `git apply --summary`/`--numstat` are Git's own authoritative parse of a
# patch's shape (rename/copy/mode-change/create/delete/binary), which is far
# more robust than pattern-matching the raw patch text: Git tolerates
# whitespace and sign variations in mode lines (e.g. "new file mode  120000"
# with two spaces, or a leading "+") that a hand-rolled regex can miss
# entirely, silently letting a symlink through. Two gaps remain even so:
#   * `--summary` says nothing at all about a symlink/submodule whose mode is
#     *unchanged* (a plain in-place modification) -- the only place that
#     mode appears is the `index <old>..<new> <mode>` line, which we still
#     have to scan directly (see `_assert_no_index_only_mode_change`).
#   * a patch can move a file's content to a different path just by writing
#     different `--- a/OLD` / `+++ b/NEW` names, with none of the `rename
#     from`/`rename to`/`rename old`/`rename new` keywords `--summary`
#     recognizes -- `git apply` honors this exactly like a real rename (it
#     reads from OLD, writes to NEW), so `--summary`'s silence here is not
#     safety, and `_patch_target_paths` would only ever see the *new* name
#     (the same destination-only blind spot that motivated this whole
#     function). This must be checked directly too (see
#     `_assert_no_ambiguous_path_change`).
# Rather than trying to reason about every such case safely, reject all of
# them outright: every patch this hook applies must be an unambiguous
# add/modify/delete of a single, regular (100644) text file.
# Captures the path so `_assert_patch_shape_supported` can cross-check it
# against `git apply --numstat`'s own reported target for this same entry
# -- see the comment on that cross-check for the desynchronization it
# prevents.
_MODE_CHANGE_LINE = re.compile(r"^(create|delete) mode (\d+) (.+)$", re.MULTILINE)
# Deliberately not anchored to end-of-line ('\s*$') -- git's own
# `gitdiff_index()` locates the first whitespace-delimited token after the
# '..' and calls `strtoul()` on it, which stops at the first non-octal-digit
# character and silently ignores anything after (e.g. a trailing tab plus
# garbage). Anchoring this regex to end-of-line let a mode token followed by
# trailing junk (e.g. "120000\tjunk") evade this scan entirely while git
# still read the mode as 0o120000 -- so this pattern is intentionally at
# least as permissive as git's own parser, matching the digits and ignoring
# what (if anything) git would also ignore.
# Also deliberately not requiring hex digits ('[0-9a-fA-F]+') on either side
# of the '..' -- git's own `gitdiff_index()` performs no such validation: it
# just does `strchr(line, '.')`, checks the following character is also
# '.', and reads the mode token after the next space, with no non-empty or
# hex-digit check on either object-id substring. A line like
# "index ..2222222 120000" (or with junk hex, or an empty object id on
# either side) is read by git exactly like a well-formed line -- as mode
# 0o120000 -- while the old, hex-anchored pattern here matched nothing at
# all for it, hiding the mode entirely. This pattern is intentionally at
# least as permissive as git's own parser on the object-id side too.
_INDEX_LINE_MODE = re.compile(r"^index \S*\.\.\S*\s+([+-]?\d+)", re.MULTILINE)
_OLD_FILE_HEADER = re.compile(r"^--- (.+)$", re.MULTILINE)
_NEW_FILE_HEADER = re.compile(r"^\+\+\+ (.+)$", re.MULTILINE)
_DIFF_GIT_HEADER = re.compile(r"^diff --git .*$", re.MULTILINE)
# Anchored on git's actual hunk-start syntax ("@@ -old_range +new_range @@"),
# not merely any line beginning "@@ " -- a line like "@@ not a real hunk"
# matches the latter but is not a hunk header to Git at all, and using the
# looser pattern here let such a line falsely truncate the header-scan
# region *before* the real '---'/'+++' pair, hiding it from inspection.
_HUNK_HEADER = re.compile(r"^@@ -", re.MULTILINE)
_REGULAR_FILE_MODE = 0o100644
_BARE_CR = re.compile(r"\r(?!\n)")
# `_diff_header_path` returns `None` for a literal '/dev/null' header, which
# `_assert_no_ambiguous_path_change` below only trusts as an unambiguous
# creation (old side '/dev/null') or deletion (new side '/dev/null') when
# corroborated by one of these entry-header lines. Without that
# corroboration, `git apply` does NOT reliably treat '/dev/null' as "this
# file doesn't exist" -- verified empirically: a '--- a/X' / '+++ /dev/null'
# pair with no 'deleted file mode' header is honored by `git apply` as an
# ordinary rename onto a real file literally named 'dev/null' on disk
# (consuming X's content), and a subsequent '--- /dev/null' / '+++ b/Y' pair
# (again with no 'new file mode' header) is honored as a rename *from* that
# real 'dev/null' file's on-disk content into Y -- while both entries'
# `--numstat` only ever reports the destination name ('dev/null' or 'Y'),
# letting a two-patch pair completely launder a forbidden source's content
# into an allowed path with every gate reporting only allowed touched paths.
_NEW_FILE_MARKER = re.compile(r"^new file mode \d+\s*$", re.MULTILINE)
_DELETED_FILE_MARKER = re.compile(r"^deleted file mode \d+\s*$", re.MULTILINE)


def _read_patch_text_for_header_scan(patch_path: Path) -> str:
    """Read patch text using git-identical line-splitting semantics, for use
    by `_assert_no_index_only_mode_change` and `_assert_no_ambiguous_path_change`.

    `Path.read_text()` opens in universal-newlines mode, which silently
    translates a bare '\\r' (a CR not followed by '\\n') into '\\n'. Git
    splits patch lines on '\\n' only (see `linelen()`); a bare CR is just an
    ordinary byte to git, never a line boundary. If this scan translated it
    to '\\n' the way `read_text()` does, an attacker could hide a decoy
    '---'/'+++' header pair (plus a hunk-header-lookalike truncator) inside
    what git treats as a single logical '`index ...`' line: this scan would
    stop at the decoy pair while git itself honors a different, later
    '---'/'+++' pair as the real (and, in that case, unchecked) rename --
    defeating the very guard this function exists to provide. Reading the
    raw bytes and decoding without newline translation makes this scan see
    exactly the same line boundaries git does. As defense in depth, any
    bare CR is rejected outright below: legitimate patches this tool
    generates or accepts never contain one, and tolerating it only ever
    reopens this desynchronization risk.
    """
    text = patch_path.read_bytes().decode("utf-8", errors="replace")
    if _BARE_CR.search(text):
        raise IntakeSecurityError(
            f"patch {patch_path.name!r} contains a bare carriage return (a '\\r' not immediately "
            "followed by '\\n'); refusing to evaluate a patch whose line boundaries cannot be "
            "determined identically to 'git apply'"
        )
    return text


def _assert_patch_shape_supported(patch_path: Path) -> None:
    anchor = patch_path.parent
    env = _no_repo_discovery_env(anchor)

    numstat = subprocess.run(
        ["git", "-C", str(anchor), "apply", "--numstat", "-z", str(patch_path)],
        capture_output=True,
        env=env,
    )
    if numstat.returncode != 0:
        raise IntakeSecurityError(
            f"could not enumerate the shape of patch {patch_path.name!r}: "
            f"{numstat.stderr.decode('utf-8', errors='replace').strip()}"
        )
    entry_count = 0
    entry_has_content_change: list[bool] = []
    numstat_paths: set[str] = set()
    for record in numstat.stdout.split(b"\0"):
        if not record:
            continue
        entry_count += 1
        fields = record.split(b"\t", 2)
        if len(fields) != 3:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} produced an unparseable 'git apply --numstat -z' record; "
                "refusing to evaluate a patch whose shape cannot be determined unambiguously"
            )
        added, deleted, _raw_path = fields
        if added == b"-" or deleted == b"-":
            # Git's own numstat convention for a binary file: the
            # added/deleted counts are literally "-" instead of a number.
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} contains binary content, which this patch hook does not "
                "support; every patch must be a simple, unambiguous add/modify/delete of a regular "
                "text file"
            )
        entry_has_content_change.append(int(added) != 0 or int(deleted) != 0)
        numstat_paths.add(
            _normalize_patch_target_path(
                _raw_path.decode("utf-8", errors="replace"), patch_path=patch_path
            )
        )

    summary = subprocess.run(
        ["git", "-C", str(anchor), "apply", "--summary", str(patch_path)],
        capture_output=True,
        text=True,
        env=env,
    )
    if summary.returncode != 0:
        raise IntakeSecurityError(
            f"could not enumerate the shape of patch {patch_path.name!r}: {summary.stderr.strip()}"
        )
    for line in summary.stdout.splitlines():
        stripped = line.strip()
        if stripped.startswith("rename "):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} contains a file rename ({stripped!r}), which this patch "
                "hook does not support"
            )
        if stripped.startswith("copy "):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} contains a file copy ({stripped!r}), which this patch hook "
                "does not support"
            )
        if stripped.startswith("mode change "):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} contains a permission-only mode change ({stripped!r}), "
                "which this patch hook does not support"
            )
        mode_match = _MODE_CHANGE_LINE.match(stripped)
        if mode_match:
            # `--summary`'s "create mode"/"delete mode" path is Git's own
            # authoritative report of which file this entry actually
            # creates/deletes -- derived from `def_name` (the 'diff --git'
            # line), not from the '---'/'+++' pair `--numstat` reports. The
            # two normally agree, but when an entry's 'new file mode'/
            # 'deleted file mode' extended-header line is placed *after*
            # its '---'/'+++' pair (a syntactically valid but non-standard
            # ordering no honest 'git diff'/'format-patch' ever produces),
            # Git parses the '---'/'+++' pair first (setting old_name/
            # new_name from whatever they literally say, which can be a
            # decoy allowed-looking path or a literal '/dev/null'), and
            # only overwrites old_name (for a delete) or new_name (for a
            # create) from `def_name` afterwards -- so `--numstat`, which
            # reports `new_name or old_name`, ends up reporting the decoy
            # path while `--summary` reports the true one. Empirically
            # confirmed against real 'git apply': this exact ordering makes
            # 'git apply --numstat' report only an allowed-looking decoy
            # path for an entry that 'git apply --summary'/`git apply`
            # itself resolves against a forbidden path such as 'lib/**' or
            # 'artifact-manifest.yaml' -- silently defeating
            # `_assert_patch_paths_allowed`, which only ever inspects
            # `--numstat`'s reported paths. Requiring the two authoritative
            # reports to agree on every create/delete target closes this:
            # any entry where they disagree is refused outright rather than
            # trusting either one alone.
            summary_path = _normalize_patch_target_path(mode_match.group(3), patch_path=patch_path)
            if summary_path not in numstat_paths:
                raise IntakeSecurityError(
                    f"patch {patch_path.name!r} reports {stripped!r} via 'git apply --summary' but "
                    f"{summary_path!r} is not among the target paths reported by 'git apply --numstat'; "
                    "refusing to evaluate a patch whose file identity cannot be determined unambiguously"
                )
            if int(mode_match.group(2), 8) != _REGULAR_FILE_MODE:
                raise IntakeSecurityError(
                    f"patch {patch_path.name!r} contains a non-regular-file mode ({stripped!r}; e.g. a "
                    "symlink or submodule), which this patch hook does not support"
                )

    _assert_no_index_only_mode_change(patch_path)
    _assert_no_ambiguous_path_change(
        patch_path, expected_entry_count=entry_count, entry_has_content_change=entry_has_content_change
    )


def _assert_no_index_only_mode_change(patch_path: Path) -> None:
    """A symlink or submodule modified *in place* (mode unchanged) produces
    no `create mode`/`delete mode`/`mode change` line in `--summary` output
    at all -- the mode only appears on the `index <old>..<new> <mode>` line,
    which is silent (empty `--summary`) in exactly this case. Scan it
    directly rather than trusting `--summary`'s silence to mean "regular
    file"."""
    text = _read_patch_text_for_header_scan(patch_path)
    for match in _INDEX_LINE_MODE.finditer(text):
        mode_token = match.group(1)
        try:
            mode_value = int(mode_token.lstrip("+"), 8)
        except ValueError as error:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} has an unparseable mode {mode_token!r} on an 'index' line; "
                "refusing to evaluate a patch whose file mode cannot be determined unambiguously"
            ) from error
        if mode_value != _REGULAR_FILE_MODE:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} touches a non-regular-file mode ({mode_token!r} on an "
                "'index' line; e.g. a symlink or submodule), which this patch hook does not support"
            )


def _diff_header_path(raw: str, *, side: str, prefix: str, patch_path: Path) -> str | None:
    """Extract the path from one `--- `/`+++ ` diff header line (dropping any
    trailing tab-separated timestamp), or `None` for `/dev/null`. Fails
    closed if the line doesn't have the expected `a/`/`b/` prefix this tool's
    patches always use, rather than guessing at some other `-p` depth."""
    value = raw.split("\t", 1)[0].strip()
    if value == "/dev/null":
        return None
    if not value.startswith(prefix):
        raise IntakeSecurityError(
            f"patch {patch_path.name!r} has an unrecognized {side} diff header path {raw!r} (expected "
            f"a {prefix!r}-prefixed path or /dev/null); refusing to evaluate a patch whose file "
            "identity cannot be determined unambiguously"
        )
    return value[len(prefix):]


def _assert_no_ambiguous_path_change(
    patch_path: Path, *, expected_entry_count: int, entry_has_content_change: list[bool]
) -> None:
    """A patch can move a file's content to a different path just by writing
    different `--- a/OLD` / `+++ b/NEW` names -- no `rename from`/`rename
    to` (or legacy `rename old`/`rename new`) keyword required at all. `git
    apply` honors this exactly like a rename: it reads context from OLD and
    writes the result to NEW. `--summary` says nothing about it (it is
    silent unless a rename/copy keyword is present), and `--numstat` would
    only ever report NEW -- the same destination-only blind spot that let a
    patch move `lib/foo.a` to an allowed name evade the forbidden-path
    check.

    A naive whole-file regex scan for '---'/'+++' lines is itself bypassable:
    text outside any real diff entry (a commit-message preamble before the
    first 'diff --git' line, or trailing commentary after a file's last
    hunk) can contain counterfeit '---'/'+++'-looking lines that Git's own
    parser ignores but a positional pairing (e.g. zip()) would not -- shifting
    every real pair into spurious agreement while the actual ambiguous rename
    slides through. Git only ever honors a '---'/'+++' pair as part of the
    single extended-header block that immediately follows a 'diff --git'
    line and precedes that entry's first '@@' hunk header, so old/new names
    are extracted from exactly (and only) that bounded region for each
    entry, anchored on the same 'diff --git' lines Git itself uses to open a
    new file entry. Any text elsewhere in the file (before the first entry,
    inside a hunk body, or trailing after it) is never inspected here, just
    as Git itself never inspects it for this purpose. The number of entries
    found this way is cross-checked against `git apply --numstat -z`'s own
    entry count (computed by the caller) as a further guard against a
    structurally malformed patch. As additional defense in depth, an entry
    that reports no '---'/'+++' header pair at all is only accepted when
    `git apply --numstat`'s own added/deleted line counts for that same
    entry are both zero (i.e. the entry genuinely has no content hunk, such
    as a truly empty file add/delete) -- an entry with real line changes
    but no discoverable header pair is refused rather than silently
    skipped, since that combination is not producible by an honest patch
    and would otherwise indicate the header-scan region was fooled.

    The patch text is read with `_read_patch_text_for_header_scan`, which
    preserves git's own line-splitting semantics (splits on '\\n' only,
    never translating a bare '\\r') -- see that helper's docstring for the
    desynchronization bypass this prevents."""
    text = _read_patch_text_for_header_scan(patch_path)
    entry_starts = list(_DIFF_GIT_HEADER.finditer(text))
    if len(entry_starts) != expected_entry_count:
        raise IntakeSecurityError(
            f"patch {patch_path.name!r} has {len(entry_starts)} 'diff --git' header(s) but "
            f"{expected_entry_count} entr(y/ies) reported by 'git apply --numstat -z'; refusing to "
            "evaluate a patch whose file entries cannot be counted unambiguously"
        )
    for index, match in enumerate(entry_starts):
        block_start = match.end()
        block_end = entry_starts[index + 1].start() if index + 1 < len(entry_starts) else len(text)
        block = text[block_start:block_end]
        hunk_match = _HUNK_HEADER.search(block)
        header_region = block[: hunk_match.start()] if hunk_match else block

        old_headers = _OLD_FILE_HEADER.findall(header_region)
        new_headers = _NEW_FILE_HEADER.findall(header_region)
        if len(old_headers) > 1 or len(new_headers) > 1:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} entry {index + 1} has more than one '---'/'+++' header "
                "before its first hunk; refusing to evaluate a patch whose file identity cannot be "
                "determined unambiguously"
            )
        if bool(old_headers) != bool(new_headers):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} entry {index + 1} has a '---' header without a matching "
                "'+++' header (or vice versa); refusing to evaluate a patch whose file identity cannot "
                "be determined unambiguously"
            )
        if not old_headers:
            # No content hunk for this entry at all (e.g. a genuinely empty
            # file addition/deletion) -- nothing to compare, and the shape
            # (mode/rename/copy) of this entry was already authoritatively
            # checked via `--summary`/`--numstat` above. Only accept this
            # when numstat itself reported no content change for this
            # entry; a nonzero line count with no discoverable header pair
            # means the header-scan region cannot be trusted here.
            if entry_has_content_change[index]:
                raise IntakeSecurityError(
                    f"patch {patch_path.name!r} entry {index + 1} reports added/deleted content lines "
                    "but no '---'/'+++' header pair could be found before its first hunk; refusing to "
                    "evaluate a patch whose file identity cannot be determined unambiguously"
                )
            continue
        old_name = _diff_header_path(old_headers[0], side="old ('---')", prefix="a/", patch_path=patch_path)
        new_name = _diff_header_path(new_headers[0], side="new ('+++')", prefix="b/", patch_path=patch_path)
        if old_name is None or new_name is None:
            # A literal '/dev/null' header is only trusted as an
            # unambiguous creation/deletion when this entry's own header
            # block corroborates it -- see the module-level comment on
            # `_NEW_FILE_MARKER`/`_DELETED_FILE_MARKER` for the bypass
            # this prevents.
            if old_name is None and not _NEW_FILE_MARKER.search(header_region):
                raise IntakeSecurityError(
                    f"patch {patch_path.name!r} entry {index + 1} has an old ('---') diff header of "
                    "'/dev/null' without a corresponding 'new file mode' header for this entry; "
                    "refusing to evaluate a patch whose file identity cannot be determined "
                    "unambiguously (a literal '/dev/null' header is only trusted here for a genuine, "
                    "unambiguous file creation)"
                )
            if new_name is None and not _DELETED_FILE_MARKER.search(header_region):
                raise IntakeSecurityError(
                    f"patch {patch_path.name!r} entry {index + 1} has a new ('+++') diff header of "
                    "'/dev/null' without a corresponding 'deleted file mode' header for this entry; "
                    "refusing to evaluate a patch whose file identity cannot be determined "
                    "unambiguously (a literal '/dev/null' header is only trusted here for a genuine, "
                    "unambiguous file deletion)"
                )
        elif old_name != new_name:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} changes path from {old_name!r} to {new_name!r} within a "
                "single diff entry, which this patch hook treats the same as an unsupported rename "
                "(no rename keyword is required for 'git apply' to honor this as one)"
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
        data = _load_yaml_document(metadata_path, error_cls=IntakeSecurityError) or {}
        if not isinstance(data, dict):
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} sidecar {metadata_path.name!r} must be a mapping at its "
                f"top level, got {type(data).__name__}"
            )
        owner = str(data.get("owner") or "").strip()
        reason = str(data.get("reason") or "").strip()
        if not owner or not reason:
            raise IntakeSecurityError(
                f"patch {patch_path.name!r} sidecar {metadata_path.name!r} must declare non-empty "
                "'owner' and 'reason' fields"
            )
        _assert_patch_paths_allowed(patch_path)
        _assert_patch_shape_supported(patch_path)
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
    """Apply `patch_path` against `target_root` with unambiguous semantics:
    repository discovery is disabled (see `_no_repo_discovery_env`) so this
    behaves identically whether `target_root` sits at the top of some
    repository or -- as it always does in production -- several directories
    inside this one; `--verbose` is always passed so a skipped hunk is never
    silent regardless of Git's default verbosity; and, because none of that
    is a substitute for actually checking the result, a postcondition
    verifies the patch's changes are now genuinely present in `target_root`
    (not just that `git apply` exited 0)."""
    env = _no_repo_discovery_env(target_root)
    check = subprocess.run(
        ["git", "-C", str(target_root), "apply", "--check", "--verbose", str(patch_path)],
        capture_output=True,
        text=True,
        env=env,
    )
    if check.returncode != 0 or "Skipped patch" in check.stdout + check.stderr:
        raise IntakePatchError(
            f"patch {patch_path.name!r} (owner={owner}) failed to reapply against the generated tree "
            f"-- upstream content likely drifted since the patch was written. "
            f"git apply --check stderr: {check.stderr.strip()}"
        )
    apply_result = subprocess.run(
        ["git", "-C", str(target_root), "apply", "--verbose", str(patch_path)],
        capture_output=True,
        text=True,
        env=env,
    )
    if apply_result.returncode != 0 or "Skipped patch" in apply_result.stdout + apply_result.stderr:
        # `git apply` can exit 0 while printing "Skipped patch ..." and
        # leaving the tree unchanged (e.g. under path-exclusion rules).
        # Treat that the same as an outright failure: never report a patch as
        # applied when it was not, silently or otherwise.
        raise IntakePatchError(
            f"patch {patch_path.name!r} (owner={owner}) did not apply cleanly despite passing --check: "
            f"{apply_result.stderr.strip()}"
        )
    # Postcondition: a patch that "applied cleanly" per the above must now be
    # cleanly *reversible* against target_root -- i.e. its changes must
    # actually be present in the tree. This is independent of parsing any
    # particular stdout/stderr message (the exact wording/verbosity of which
    # is a Git-version implementation detail): if the patch's changes were
    # not actually written, reversing it will fail to apply, and that failure
    # is what turns "git apply reported success" into "the patch actually
    # took effect" -- catching any other way `git apply` might silently no-op
    # (now or in a future Git version) rather than trusting the exit code and
    # message text alone.
    postcondition = subprocess.run(
        ["git", "-C", str(target_root), "apply", "--check", "--reverse", "--verbose", str(patch_path)],
        capture_output=True,
        text=True,
        env=env,
    )
    if postcondition.returncode != 0 or "Skipped patch" in postcondition.stdout + postcondition.stderr:
        raise IntakePatchError(
            f"patch {patch_path.name!r} (owner={owner}) reported success but its changes could not be "
            f"verified as present in {target_root} (reverse-apply postcondition check failed): "
            f"{postcondition.stderr.strip()}"
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
    # symlinks=True: preserve any symlink in the staged tree as a symlink in
    # the rehearsal copy instead of silently dereferencing it into a regular
    # file/directory. Without this, `git apply` would happily write "beyond"
    # a dereferenced copy where it refuses to write beyond a real symlink,
    # so the rehearsal could pass while the real, symlink-containing tree
    # fails mid-queue -- breaking the very guarantee this rehearsal exists
    # to provide.
    shutil.copytree(staged_sdk_root, rehearsal_root, symlinks=True)
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
    # A symlinked directory is not itself `is_file()`, and `rglob` does not
    # recurse *into* a symlinked directory -- so a symlink used in place of a
    # regular file/directory must be included explicitly here, or it (and
    # anything it appears to contain) would be silently invisible to
    # `compare_trees`, which is exactly the review gate a maintainer relies
    # on to see everything a `promote` is about to write.
    return {
        path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file() or path.is_symlink()
    }


def _is_text(path: Path) -> bool:
    return path.suffix in TEXT_DIFF_SUFFIXES


def compare_trees(staged_root: Path, promoted_root: Path) -> DiffResult:
    """Read-only comparison; never writes to either tree.

    Both roots must exist as directories. `_tracked_files` treats a missing
    directory the same as an empty one, which would otherwise make a
    missing/mistyped `--staged-dir`/`--promoted-dir` (or a promoted tree that
    was never staged) silently report "no differences" instead of the
    indeterminate-input error it actually is -- exactly the report a reviewer
    must not be able to mistake for "this payload is identical to what's
    promoted"."""
    if not staged_root.is_dir():
        raise IntakeVerificationError(f"staged tree not found or not a directory: {staged_root}")
    if not promoted_root.is_dir():
        raise IntakeVerificationError(f"promoted tree not found or not a directory: {promoted_root}")
    # `_tracked_files` (deliberately) reports symlinks so a symlinked payload
    # is never invisible to this diff -- but a symlink (dangling, or
    # pointing outside either tree) is not otherwise supported anywhere in
    # this tool, and `bas.sha256` below would raise a raw, unhandled OSError
    # for one instead of the clear, actionable message the rest of this
    # function gives for every other indeterminate-input case.
    _assert_no_symlinks(staged_root, label="staged tree")
    _assert_no_symlinks(promoted_root, label="promoted tree")
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
def _assert_no_symlinks(root: Path, *, label: str) -> None:
    """Fail closed if `root` contains any symlink, anywhere. This tool does
    not support promoting or diffing symlinks: `shutil.copytree` with the
    default `symlinks=False` would dereference a symlink into a regular
    file/directory containing whatever bytes its target holds -- including
    host paths well outside the staged tree -- and silently materialize that
    content into the committed provider tree. A symlinked directory is also
    invisible to naive `rglob`-based tree walks, so it could otherwise slip
    past review undetected even with `symlinks=True`. Every path this tool
    ever writes into the repository must be a plain regular file or
    directory."""
    if not root.is_dir():
        return
    offenders = sorted(path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_symlink())
    if offenders:
        raise IntakeSecurityError(
            f"{label} at {root} contains symlink(s), which this tool does not support: {offenders!r}"
        )


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

    _assert_no_symlinks(dest, label="staged payload")
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
        raise IntakeVerificationError(f"staged payload not found: {staged_sdk_root}")
    _assert_no_symlinks(staged_sdk_root, label="staging source")

    tmp = provider_root.with_name(provider_root.name + ".promote-tmp")
    backup = provider_root.with_name(provider_root.name + ".promote-backup")
    if backup.exists():
        # A previous promotion was interrupted somewhere in the rename swap:
        # either between renaming the provider tree aside and renaming the
        # new tree into place (provider_root now missing), or between
        # completing the swap and removing the backup (both now present, and
        # a plain rename below would otherwise fail later with an opaque
        # "File exists" error). Either way, refuse to proceed (fail closed)
        # rather than guess which state the repository is in; a maintainer
        # must resolve it explicitly. Checked before any other work so a
        # broken system state is never masked by an unrelated
        # hash-verification failure.
        if not provider_root.exists():
            raise IntakeSecurityError(
                f"found leftover promotion backup {backup} with no provider tree at {provider_root}; "
                f"a previous promotion was interrupted mid-swap. Restore it manually "
                f"(e.g. `mv {backup} {provider_root}`) and confirm the tree is correct before retrying."
            )
        raise IntakeSecurityError(
            f"found leftover promotion backup {backup} alongside an existing provider tree at "
            f"{provider_root}; a previous promotion was interrupted after completing its swap but "
            f"before cleaning up. Confirm {provider_root} is correct, then remove {backup} manually "
            "before retrying."
        )

    verification = verify_artifact_hashes(staged_sdk_root, staged_sdk_root / "artifact-manifest.yaml")
    verification.raise_if_not_ok(label=f"staged payload at {staged_sdk_root}")

    if tmp.exists():
        shutil.rmtree(tmp)

    # symlinks=True: defense in depth alongside the `_assert_no_symlinks`
    # check above -- never silently dereference a symlink into a regular
    # file/directory containing arbitrary host content.
    shutil.copytree(staged_sdk_root, tmp, symlinks=True)
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
