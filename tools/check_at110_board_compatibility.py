"""Check the bounded SDK/NSX AT110 manifest contract without network access."""
from __future__ import annotations

import argparse
from dataclasses import dataclass, fields
import json
from pathlib import Path
import re
import subprocess
import sys

import yaml

ROOT = Path(__file__).resolve().parents[1]
BOARD = "atomiq110_fpga_turbo"
MODULE = "nsx-board-atomiq110-fpga-turbo"
NSX_PATH = f"src/neuralspotx/boards/{BOARD}/nsx-module.yaml"
SDK_PATH = ROOT / "boards" / BOARD / "nsx-module.yaml"
FIXTURE = ROOT / "tests/fixtures/board-compatibility/at110-nsx.json"
SOURCE = "AmbiqAI/neuralspotx"


def value(document: object, path: str) -> object:
    for key in path.split("."):
        if not isinstance(document, dict) or key not in document:
            raise ValueError(f"missing field: {path}")
        document = document[key]
    return document


def string(document: object, path: str) -> str:
    result = value(document, path)
    if not isinstance(result, str) or not result:
        raise ValueError(f"{path}: expected nonempty string")
    return result


def strings(document: object, path: str) -> frozenset[str]:
    result = value(document, path)
    if not isinstance(result, list) or not result or any(
        not isinstance(item, str) or not item for item in result
    ):
        raise ValueError(f"{path}: expected nonempty string list")
    if len(set(result)) != len(result):
        raise ValueError(f"{path}: duplicate entries")
    return frozenset(result)


@dataclass(frozen=True)
class Contract:
    package: str
    targets: frozenset[str]
    required: frozenset[str]
    provider: str
    boards: frozenset[str]
    socs: frozenset[str]
    toolchains: frozenset[str]
    ambiqsuite: bool
    zephyr: bool

    @classmethod
    def parse(cls, document: object) -> Contract:
        if type(value(document, "schema_version")) is not int or value(document, "schema_version") != 1:
            raise ValueError("manifest schema_version must be 1")
        if string(document, "module.name") != MODULE or string(document, "module.type") != "board":
            raise ValueError(f"expected board module {MODULE}")
        support = [value(document, f"support.{key}") for key in ("ambiqsuite", "zephyr")]
        if any(type(item) is not bool for item in support):
            raise ValueError("support values must be booleans")
        return cls(
            string(document, "build.cmake.package"),
            strings(document, "build.cmake.targets"),
            strings(document, "depends.required"),
            string(document, "constraints.required_sdk_provider"),
            strings(document, "compatibility.boards"),
            strings(document, "compatibility.socs"),
            strings(document, "compatibility.toolchains"),
            support[0], support[1],
        )


def compare(sdk: object, nsx: object) -> None:
    left, right = Contract.parse(sdk), Contract.parse(nsx)
    differences = [field.name for field in fields(Contract)
                   if getattr(left, field.name) != getattr(right, field.name)]
    if differences:
        raise ValueError("AT110 compatibility mismatch: " + ", ".join(differences))


def revision_sha(revision: object) -> str:
    if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("revision must be a full lowercase 40-character commit SHA")
    return revision


def read_fixture(path: Path) -> dict:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict) or set(data) != {"schema_version", "repository", "revision", "path", "manifest"}:
        raise ValueError("invalid AT110 fixture fields")
    if type(data["schema_version"]) is not int or data["schema_version"] != 1:
        raise ValueError("unsupported fixture schema_version; expected 1")
    if data["repository"] != SOURCE or data["path"] != NSX_PATH:
        raise ValueError("unexpected counterpart repository/path")
    revision_sha(data["revision"])
    Contract.parse(data["manifest"])
    return data


def snapshot(repo: Path, revision: str) -> dict:
    revision_sha(revision)
    # Read committed objects, not working-tree files. No fetch or remote inference.
    resolved = subprocess.check_output(
        ["git", "-C", str(repo), "rev-parse", "--verify", f"{revision}^{{commit}}"], text=True
    ).strip()
    if resolved != revision:
        raise ValueError("revision must identify a commit, not a tag object")
    raw = subprocess.check_output(
        ["git", "-C", str(repo), "show", f"{revision}:{NSX_PATH}"], text=True
    )
    return {"schema_version": 1, "repository": SOURCE, "revision": revision,
            "path": NSX_PATH, "manifest": yaml.safe_load(raw)}


def check(*, sdk_path: Path = SDK_PATH, fixture: Path = FIXTURE,
          nsx_repo: Path | None = None, revision: str | None = None,
          refresh: bool = False) -> str:
    if (nsx_repo is None) != (revision is None) or (refresh and nsx_repo is None):
        raise ValueError("--nsx-repo and --revision are required together, including for --refresh")
    data = snapshot(nsx_repo, revision) if nsx_repo is not None and revision is not None else read_fixture(fixture)
    compare(yaml.safe_load(sdk_path.read_text(encoding="utf-8")), data["manifest"])
    if refresh:
        # Validate before touching the existing reviewed fixture.
        fixture.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    return data["revision"]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nsx-repo", type=Path)
    parser.add_argument("--revision", help="exact NSX commit SHA available locally")
    parser.add_argument("--refresh", action="store_true", help="replace fixture only after a successful check")
    args = parser.parse_args()
    try:
        revision = check(nsx_repo=args.nsx_repo, revision=args.revision, refresh=args.refresh)
    except (ValueError, OSError, subprocess.CalledProcessError, yaml.YAMLError) as exc:
        print(f"AT110 compatibility check failed: {exc}", file=sys.stderr)
        return 1
    print(f"AT110 manifest compatibility passed against {SOURCE}@{revision}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
