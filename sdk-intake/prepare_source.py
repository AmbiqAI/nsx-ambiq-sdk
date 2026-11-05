#!/usr/bin/env python3
"""Prepare an immutable local AmbiqSuite source revision; never push upstream."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

RECEIPT = ".nsx-source-patches.json"


def git(root: Path, *args: str) -> str:
    return subprocess.check_output(["git", "-C", str(root), *args], text=True).strip()


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def safe_path(value: str) -> Path:
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or not path.parts or path.parts[0] == ".git":
        raise ValueError(f"invalid patch path: {value}")
    return path


def prepare(repo: Path, patchset: Path, output: Path) -> str:
    metadata = json.loads(patchset.read_text())
    if metadata["schema_version"] != 1:
        raise ValueError("unsupported patchset schema_version; expected 1")
    upstream = metadata["upstream_commit"]
    if not re.fullmatch(r"[0-9a-f]{40}", upstream) or git(repo, "rev-parse", upstream + "^{commit}") != upstream:
        raise ValueError("patchset requires an exact upstream commit")
    if output.exists():
        raise ValueError("source output already exists; select a new directory")
    files = metadata["files"]
    names = [str(safe_path(f["path"])) for f in files]
    if not names or len(set(names)) != len(names) or RECEIPT in names:
        raise ValueError("patchset requires unique declared source files")
    patches = []
    for item in metadata["patches"]:
        path = patchset.parent / safe_path(item["path"])
        if path.is_symlink() or digest(path) != item["sha256"]:
            raise ValueError(f"patch hash mismatch: {path}")
        patches.append(path.resolve())
    if not patches:
        raise ValueError("patchset is empty")
    # Use a new detached worktree; the user's working directory is never a source.
    git(repo, "-c", "core.hooksPath=/dev/null", "worktree", "add", "--detach", str(output), upstream)
    if git(output, "status", "--porcelain", "--untracked-files=all"):
        raise ValueError("new source worktree is not clean")
    for item in files:
        path = output / item["path"]
        if not path.resolve().is_relative_to(output) or path.is_symlink() or digest(path) != item["before_sha256"]:
            raise ValueError(f"source preimage mismatch: {item['path']}")
    for patch in patches:
        git(output, "apply", "--check", str(patch))
        git(output, "apply", str(patch))
    if set(git(output, "diff", "--name-only").splitlines()) != set(names):
        raise ValueError("patch changed undeclared paths")
    if git(output, "ls-files", "--others"):
        raise ValueError("patch introduced undeclared files")
    for item in files:
        path = output / item["path"]
        if path.is_symlink() or digest(path) != item["after_sha256"]:
            raise ValueError(f"source postimage mismatch: {item['path']}")
    if git(output, "diff", "--summary"):
        raise ValueError("source patch changed file modes or types")
    receipt = json.dumps(metadata, sort_keys=True, indent=2) + "\n"
    (output / RECEIPT).write_text(receipt)
    git(output, "add", "--", *names, RECEIPT)
    # Local provenance commit only. Do not run user hooks or prompt for signing.
    git(output, "-c", "core.hooksPath=/dev/null", "-c", "commit.gpgSign=false",
        "-c", "user.name=NSX SDK Intake", "-c", "user.email=nsx-intake@example.invalid",
        "commit", "-m", "Apply reviewed NSX source patchset to " + upstream)
    return git(output, "rev-parse", "HEAD")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", required=True, type=Path)
    parser.add_argument("--patchset", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        print(prepare(args.repo.resolve(), args.patchset.resolve(), args.output.resolve()))
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(2, f"error: source preparation failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
