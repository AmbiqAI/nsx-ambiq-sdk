"""Cross-repository manifest checks; no network or sibling checkout required."""
from __future__ import annotations

import copy
import json
from pathlib import Path
import subprocess

import pytest
import yaml

from tools import check_at110_board_compatibility as checker


def test_committed_counterpart_matches_sdk() -> None:
    checker.check()


@pytest.mark.parametrize("path,replacement", [
    (("compatibility", "socs"), ["apollo510"]),
    (("compatibility", "toolchains"), ["arm-none-eabi-gcc"]),
    (("constraints", "required_sdk_provider"), "other-sdk"),
    (("depends", "required"), ["nsx-ambiqsuite>=99"]),
    (("build", "cmake", "targets"), ["nsx::wrong_board"]),
    (("module", "name"), "different-board"),
    (("support", "ambiqsuite"), "true"),
])
def test_incompatible_counterpart_rejected(path: tuple[str, ...], replacement: object) -> None:
    original = checker.read_fixture(checker.FIXTURE)["manifest"]
    changed = copy.deepcopy(original)
    parent = changed
    for key in path[:-1]:
        parent = parent[key]
    parent[path[-1]] = replacement
    with pytest.raises(ValueError):
        checker.compare(original, changed)


def test_set_order_and_independent_module_versions() -> None:
    original = checker.read_fixture(checker.FIXTURE)["manifest"]
    changed = copy.deepcopy(original)
    changed["compatibility"]["toolchains"].reverse()
    changed["depends"]["required"].reverse()
    changed["module"]["version"] = "9.0.0"
    checker.compare(original, changed)


@pytest.mark.parametrize("bad", [None, {}, [], {"schema_version": True}])
def test_missing_fields_cannot_compare_equal(bad: object) -> None:
    with pytest.raises(ValueError):
        checker.compare(bad, bad)


@pytest.mark.parametrize("key,bad", [("revision", "main"), ("schema_version", 2),
                                      ("repository", "other/repo"), ("path", "other.yaml")])
def test_fixture_provenance_rejected(tmp_path: Path, key: str, bad: object) -> None:
    data = checker.read_fixture(checker.FIXTURE)
    data[key] = bad
    target = tmp_path / "fixture.json"
    target.write_text(json.dumps(data))
    with pytest.raises(ValueError):
        checker.read_fixture(target)


def test_explicit_commit_check_and_failed_refresh_preserves_fixture(tmp_path: Path) -> None:
    repo = tmp_path / "counterpart"
    repo.mkdir()

    def git(*args: str) -> str:
        return subprocess.check_output(["git", "-C", str(repo), *args], text=True).strip()

    git("init", "-q")
    manifest = repo / checker.NSX_PATH
    manifest.parent.mkdir(parents=True)
    original = checker.read_fixture(checker.FIXTURE)["manifest"]
    manifest.write_text(yaml.safe_dump(original))

    def commit() -> str:
        git("add", ".")
        git("-c", "user.name=Test", "-c", "user.email=test@example.invalid",
            "-c", "commit.gpgsign=false", "commit", "-qm", "fixture")
        return git("rev-parse", "HEAD")

    good = commit()
    fixture = tmp_path / "snapshot.json"
    checker.check(nsx_repo=repo, revision=good, fixture=fixture, refresh=True)
    saved = fixture.read_bytes()
    assert json.loads(saved)["revision"] == good
    changed = copy.deepcopy(original)
    changed["constraints"]["required_sdk_provider"] = "wrong-provider"
    manifest.write_text(yaml.safe_dump(changed))
    # Dirty working-tree content cannot contaminate an immutable snapshot.
    checker.check(nsx_repo=repo, revision=good, fixture=fixture)
    bad = commit()
    with pytest.raises(ValueError, match="provider"):
        checker.check(nsx_repo=repo, revision=bad, fixture=fixture, refresh=True)
    assert fixture.read_bytes() == saved
    with pytest.raises(ValueError, match="full lowercase"):
        checker.check(nsx_repo=repo, revision="HEAD", fixture=fixture, refresh=True)
    assert fixture.read_bytes() == saved
