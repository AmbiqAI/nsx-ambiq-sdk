"""Baseline integrity: the committed provider payload must match its own manifest.

This is the guard that PR #22 (`ACfE/armclang bring-up`, ddb8864) needed and did
not have. That PR replaced 22 committed `acfe` HAL/BSP archives with ABI-corrected
rebuilds but left `modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml` describing
the superseded bytes. Nothing in CI compared the real committed archives against
the real committed manifest, so the drift survived every subsequent PR and shipped
in the `v5.2.23` tag and release.

`tests/test_intake_workflow.py` exercises `verify_artifact_hashes` against
synthetic fixtures in temporary directories, which proves the verifier's logic but
never looks at the payload this repository actually publishes. These tests close
that gap by running the same verifier over the real promoted tree.
"""
from __future__ import annotations

import hashlib
import importlib.util
import sys
from pathlib import Path

import pytest
import yaml


SDK_REL = Path("modules") / "nsx-ambiqsuite" / "sdk"
MANIFEST_REL = SDK_REL / "artifact-manifest.yaml"


def _load_intake_workflow(repo_root: Path):
    module_path = repo_root / "sdk-intake" / "intake_workflow.py"
    spec = importlib.util.spec_from_file_location("nsx_intake_workflow_baseline", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _manifest(repo_root: Path) -> dict:
    return yaml.safe_load((repo_root / MANIFEST_REL).read_text(encoding="utf-8"))


def _declared_artifacts(repo_root: Path) -> list[tuple[str, str, str, str]]:
    """Yield (section_id, toolchain, manifest_path, expected_sha256) for every
    declared HAL/BSP archive, resolved independently of the intake helper."""
    manifest = _manifest(repo_root)
    rows: list[tuple[str, str, str, str]] = []
    for section, artifact_key, id_field in (
        ("parts", "hal_artifacts", "logical_skew"),
        ("boards", "bsp_artifacts", "nsx_board"),
    ):
        for entry in manifest.get(section) or []:
            for toolchain, info in (entry.get(artifact_key) or {}).items():
                rows.append((entry[id_field], toolchain, info["path"], info["sha256"]))
    return rows


def test_promoted_baseline_matches_its_artifact_manifest(repo_root: Path) -> None:
    """Run the shipped verifier over the real promoted payload.

    This is the exact check a consumer runs with
    `python sdk-intake/intake_workflow.py verify-baseline --train stable`, so a
    green CI run and a green consumer verification can never disagree again.
    """
    helper = _load_intake_workflow(repo_root)
    result = helper.verify_artifact_hashes(repo_root / SDK_REL)

    assert list(result.mismatched) == [], (
        "committed archives no longer match artifact-manifest.yaml; regenerate the "
        "payload through sdk-intake/build_ambiqsuite.py (or intake_workflow.py "
        "stage/promote) so archives and manifest are published together"
    )
    assert list(result.missing) == []
    assert result.verified, "the manifest declared no verifiable archives"


def test_every_declared_artifact_is_present_and_hash_correct(repo_root: Path) -> None:
    """Independent re-derivation of the same property.

    Deliberately does not import the intake helper: if `verify_artifact_hashes`
    itself ever regresses into a fail-open state, this test still fails.
    """
    offenders: list[str] = []
    for identifier, toolchain, manifest_path, expected in _declared_artifacts(repo_root):
        parts = Path(manifest_path).parts
        assert len(parts) >= 3 and parts[1] == "lib", (identifier, toolchain, manifest_path)
        promoted = repo_root / SDK_REL / "lib" / parts[0] / Path(*parts[2:])
        if not promoted.is_file():
            offenders.append(f"{identifier}/{toolchain}: missing {promoted}")
            continue
        actual = _sha256(promoted)
        if actual != expected:
            offenders.append(f"{identifier}/{toolchain}: expected {expected} actual {actual}")
    assert offenders == []


def test_no_promoted_archive_is_undeclared(repo_root: Path) -> None:
    """Every committed archive must be covered by the manifest.

    Without this, an archive could be added to the payload with no provenance
    record at all and still pass the two checks above.
    """
    declared = {
        (repo_root / SDK_REL / "lib" / Path(path).parts[0] / Path(*Path(path).parts[2:])).resolve()
        for _, _, path, _ in _declared_artifacts(repo_root)
    }
    on_disk = {path.resolve() for path in (repo_root / SDK_REL / "lib").rglob("*.a")}
    assert on_disk - declared == set()


def test_toolchains_declaring_abi_cflags_record_them(repo_root: Path) -> None:
    """ABI-affecting flags are provenance, not build trivia.

    The `acfe` archives shipped in `v5.2.23` differ from their predecessors only
    because of `-fshort-wchar -fshort-enums`; recording that in the manifest is
    what makes two same-source archives distinguishable to a consumer.
    """
    helper_spec = importlib.util.spec_from_file_location(
        "nsx_build_ambiqsuite_baseline", repo_root / "sdk-intake" / "build_ambiqsuite.py"
    )
    assert helper_spec is not None and helper_spec.loader is not None
    bas = importlib.util.module_from_spec(helper_spec)
    sys.modules[helper_spec.name] = bas
    helper_spec.loader.exec_module(bas)

    toolchains = _manifest(repo_root)["build"]["toolchains"]
    for name, expected_flags in bas.TOOLCHAIN_ABI_CFLAGS.items():
        if toolchains.get(name, {}).get("status") != "built":
            continue
        assert toolchains[name].get("abi_cflags") == " ".join(expected_flags), name


@pytest.mark.parametrize("toolchain", ["gcc", "atfe", "acfe"])
def test_each_declared_toolchain_has_artifacts(repo_root: Path, toolchain: str) -> None:
    rows = [row for row in _declared_artifacts(repo_root) if row[1] == toolchain]
    assert rows, f"no artifacts declared for toolchain {toolchain}"
