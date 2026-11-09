"""Success receipts bind promotion to validated payload bytes, not transformations.

Receipts detect accidental drift. They are not signatures and do not support
concurrent writers modifying a candidate while it is being validated/promoted.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
import stat
import tempfile
from dataclasses import asdict, dataclass
from pathlib import Path, PurePosixPath


class SnapshotError(ValueError):
    """The candidate needs to be staged successfully again."""


def receipt_path(root: Path) -> Path:
    return root.with_name(root.name + ".stage-receipt.json")


def assert_plain_path(path: Path) -> None:
    for component in (path, *path.parents):
        if component.is_symlink():
            raise SnapshotError(f"symlinked snapshot path: {component}")


def file_hash(path: Path) -> str:
    assert_plain_path(path)
    if not stat.S_ISREG(path.stat().st_mode):
        raise SnapshotError(f"not a regular file: {path}")
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def capture_files(root: Path) -> dict[str, str]:
    assert_plain_path(root)
    if not root.is_dir():
        raise SnapshotError(f"snapshot tree missing: {root}")
    result = {}
    def fail(error):
        raise error

    for directory, dirs, files in os.walk(root, followlinks=False, onerror=fail):
        for name in dirs:
            path = Path(directory) / name
            assert_plain_path(path)
            if not stat.S_ISDIR(path.stat().st_mode):
                raise SnapshotError(f"not a directory: {path}")
        for name in files:
            path = Path(directory) / name
            result[path.relative_to(root).as_posix()] = file_hash(path)
    return dict(sorted(result.items()))


@dataclass(frozen=True)
class StageSnapshot:
    provider_path: str
    train: str
    version: str
    source_commit: str | None
    policy: dict[str, str]
    files: dict[str, str]


def _relative(value: object) -> bool:
    if not isinstance(value, str) or not value or "\\" in value:
        return False
    path = PurePosixPath(value)
    return bool(path.parts) and not path.is_absolute() and ".." not in path.parts and path.as_posix() == value


def _hashes(value: object) -> bool:
    return isinstance(value, dict) and bool(value) and all(
        _relative(key) and isinstance(digest, str) and re.fullmatch(r"[0-9a-f]{64}", digest)
        for key, digest in value.items()
    )


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SnapshotError(f"duplicate receipt key: {key}")
        result[key] = value
    return result


def load(root: Path) -> StageSnapshot:
    path = receipt_path(root)
    try:
        file_hash(path)  # Refuse links/special files before opening JSON.
        data = json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=_unique_object)
        fields = {"schema_version", "provider_path", "train", "version", "source_commit", "policy", "files"}
        if not isinstance(data, dict) or set(data) != fields:
            raise SnapshotError("unexpected receipt fields")
        if type(data["schema_version"]) is not int or data.pop("schema_version") != 1:
            raise SnapshotError("unsupported receipt schema_version")
        if not _relative(data["provider_path"]):
            raise SnapshotError("invalid provider_path")
        for key in ("train", "version"):
            if not isinstance(data[key], str) or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.+-]*", data[key]):
                raise SnapshotError(f"invalid {key}")
        source = data["source_commit"]
        if source is not None and (not isinstance(source, str) or not re.fullmatch(r"[0-9a-f]{40}", source)):
            raise SnapshotError("invalid source_commit")
        if not _hashes(data["policy"]) or set(data["policy"]) != {"sanitation-replay.json", "internal-marker-baseline.yaml"}:
            raise SnapshotError("invalid policy fingerprints")
        if not _hashes(data["files"]) or "artifact-manifest.yaml" not in data["files"]:
            raise SnapshotError("invalid payload inventory")
        return StageSnapshot(**data)
    except (OSError, ValueError) as error:
        raise SnapshotError(f"invalid or missing successful-stage receipt {path}: {error}; run stage again") from error


def write(root: Path, snapshot: StageSnapshot) -> None:
    path = receipt_path(root)
    assert_plain_path(path)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent,
                                         prefix=".stage-receipt-", delete=False) as stream:
            temporary = Path(stream.name)
            json.dump({"schema_version": 1, **asdict(snapshot)}, stream, indent=2, sort_keys=True)
            stream.write("\n")
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def invalidate(root: Path) -> None:
    path = receipt_path(root)
    assert_plain_path(path)
    path.unlink(missing_ok=True)


def verify(root: Path, snapshot: StageSnapshot) -> None:
    actual = capture_files(root)
    if actual != snapshot.files:
        changed = sorted(set(actual) ^ set(snapshot.files) | {
            path for path in actual.keys() & snapshot.files.keys() if actual[path] != snapshot.files[path]
        })
        raise SnapshotError(f"staged payload differs from successful-stage receipt: {changed[:10]}; run stage again")
