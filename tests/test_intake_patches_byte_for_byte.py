"""Reconstruct the 5.2.26 intake edits from the recorded release baselines.

Reverse-applying a patch proves its hunks are present, but cannot detect an
unrelated edit elsewhere in a patched file. Compare the complete bytes too.
"""

from __future__ import annotations

import difflib
import os
import subprocess
from pathlib import Path

import pytest
import yaml


REPO_ROOT = Path(__file__).resolve().parents[1]
PAYLOAD = Path("modules/nsx-ambiqsuite/sdk")
RELEASE = yaml.safe_load((REPO_ROOT / "release/nsx-ambiq-sdk-5.2.26.yaml").read_text(encoding="utf-8"))


def history_unavailable(reason: str) -> None:
    if os.environ.get("CI", "").lower() in {"1", "true", "yes"}:
        pytest.fail(f"{reason}; CI must check out full history and tags (fetch-depth: 0)")
    pytest.skip(f"{reason}; fetch full history and release tags to check intake patch bytes")


@pytest.mark.parametrize("baseline", RELEASE["consumer_upgrade"], ids=lambda entry: entry["from_tag"])
def test_intake_patches_match_release_baseline(repo_root: Path, tmp_path: Path, baseline: dict) -> None:
    patches = sorted((repo_root / "sdk-intake/patches/stable").glob("*.patch"))
    if not patches:
        pytest.skip("stable intake patch queue retired; no patched files to reconstruct")

    probe = subprocess.run(
        ["git", "rev-parse", "--is-shallow-repository"], cwd=repo_root, capture_output=True, text=True
    )
    if probe.returncode != 0 or probe.stdout.strip() != "false":
        history_unavailable("git history unavailable or shallow")
    tag = baseline["from_tag"]
    commit = baseline["from_commit"]
    revision = subprocess.run(
        ["git", "rev-parse", "--verify", f"refs/tags/{tag}^{{commit}}"],
        cwd=repo_root, capture_output=True, text=True,
    )
    if revision.returncode != 0:
        history_unavailable(f"release tag {tag} is missing")
    assert revision.stdout.strip() == commit, f"{tag} does not match the release record's {commit}"

    paths = {
        line[len("+++ b/"):].strip()
        for patch in patches
        for line in patch.read_text(encoding="utf-8").splitlines()
        if line.startswith("+++ b/")
    }
    # The record's older upgrade path excludes atomiq110: that payload was
    # replaced between 5.2.24 and 5.2.25. The immediately preceding release
    # supplies all 26 files; the older release supplies the 23 Apollo files.
    if tag != RELEASE["distribution"]["supersedes"]["tag"]:
        paths = {path for path in paths if "atomiq110" not in Path(path).parts}
    assert paths, "intake patches must name provider files"

    subprocess.run(["git", "init", "--quiet", str(tmp_path)], check=True, capture_output=True)
    for path in sorted(paths):
        result = subprocess.run(
            ["git", "show", f"{commit}:{PAYLOAD / path}"], cwd=repo_root, capture_output=True
        )
        assert result.returncode == 0, f"cannot read {tag}:{PAYLOAD / path}: {result.stderr!r}"
        destination = tmp_path / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(result.stdout)

    for patch in patches:
        result = subprocess.run(
            ["git", "apply", *[f"--include={path}" for path in sorted(paths)], str(patch)],
            cwd=tmp_path, capture_output=True, text=True,
        )
        assert result.returncode == 0, f"{patch.name} cannot apply to {tag}:\n{result.stderr}"

    differences = []
    for path in sorted(paths):
        expected = (tmp_path / path).read_bytes()
        actual = (repo_root / PAYLOAD / path).read_bytes()
        if expected != actual:
            diff = difflib.diff_bytes(
                difflib.unified_diff, expected.splitlines(keepends=True), actual.splitlines(keepends=True),
                fromfile=f"{tag} + intake patches/{path}".encode(),
                tofile=str(PAYLOAD / path).encode(),
            )
            differences.append(b"".join(diff).decode("utf-8", errors="backslashreplace"))
    assert not differences, "provider files differ from the recorded baseline plus patches:\n" + "\n".join(differences)
