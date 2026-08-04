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

# The published inventory of the golden baseline. Pinned deliberately: without
# it, deleting an archive *and* its manifest row keeps every consistency check
# green while silently dropping a supported board from the distribution. These
# numbers change only when an intake intentionally adds or removes coverage.
EXPECTED_ARTIFACT_COUNTS = {"gcc": 25, "atfe": 25, "acfe": 23}
EXPECTED_TOTAL_ARTIFACTS = 73

# `acfe` archives must be built for the NSX/heliaRT ABI (wchar_t = 2,
# smallest-container enums). Hardcoded here rather than read from
# build_ambiqsuite.py so that deleting or emptying TOOLCHAIN_ABI_CFLAGS makes
# this test fail instead of passing vacuously.
REQUIRED_ABI_CFLAGS = {"acfe": "-fshort-wchar -fshort-enums"}


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


def test_toolchains_requiring_abi_cflags_record_them(repo_root: Path) -> None:
    """ABI-affecting flags are provenance, not build trivia.

    The `acfe` archives shipped in `v5.2.23` differ from their predecessors only
    because of `-fshort-wchar -fshort-enums`; recording that in the manifest is
    what makes two same-source archives distinguishable to a consumer.
    """
    toolchains = _manifest(repo_root)["build"]["toolchains"]
    for name, expected in REQUIRED_ABI_CFLAGS.items():
        assert toolchains[name]["status"] == "built", name
        assert toolchains[name].get("abi_cflags") == expected, name


def test_generator_emits_the_required_abi_cflags(repo_root: Path) -> None:
    """The manifest value must come from the generator, not from a hand edit.

    Checked separately from the manifest assertion above so that removing the
    generator mapping fails loudly instead of making the requirement vacuous.
    """
    helper_spec = importlib.util.spec_from_file_location(
        "nsx_build_ambiqsuite_baseline", repo_root / "sdk-intake" / "build_ambiqsuite.py"
    )
    assert helper_spec is not None and helper_spec.loader is not None
    bas = importlib.util.module_from_spec(helper_spec)
    sys.modules[helper_spec.name] = bas
    helper_spec.loader.exec_module(bas)

    for name, expected in REQUIRED_ABI_CFLAGS.items():
        assert " ".join(bas.TOOLCHAIN_ABI_CFLAGS[name]) == expected, name


@pytest.mark.parametrize("toolchain", sorted(EXPECTED_ARTIFACT_COUNTS))
def test_declared_artifact_inventory_is_complete(repo_root: Path, toolchain: str) -> None:
    """Pin the inventory so coverage cannot shrink silently."""
    rows = [row for row in _declared_artifacts(repo_root) if row[1] == toolchain]
    assert len(rows) == EXPECTED_ARTIFACT_COUNTS[toolchain]


def test_total_declared_artifact_count_is_pinned(repo_root: Path) -> None:
    rows = _declared_artifacts(repo_root)
    assert len(rows) == EXPECTED_TOTAL_ARTIFACTS
    assert len({(identifier, toolchain) for identifier, toolchain, _, _ in rows}) == EXPECTED_TOTAL_ARTIFACTS


def test_every_part_and_board_declares_at_least_one_archive(repo_root: Path) -> None:
    manifest = _manifest(repo_root)
    skews = {entry["logical_skew"] for entry in manifest["parts"]}
    for entry in manifest["parts"]:
        assert entry.get("hal_artifacts"), entry["logical_skew"]
    for entry in manifest["boards"]:
        assert entry.get("bsp_artifacts"), entry["nsx_board"]
        assert entry["logical_skew"] in skews, entry["nsx_board"]


def test_ble_dis_firmware_revision_tracks_the_distribution_version(repo_root: Path) -> None:
    """The BLE Device Information Service default advertises the distribution
    version over the air. It silently stayed at the previous version through a
    release once already; pin it so a version bump cannot forget it."""
    try:
        import tomllib
    except ModuleNotFoundError:  # pragma: no cover - Python 3.10
        import tomli as tomllib

    version = tomllib.loads((repo_root / "pyproject.toml").read_text(encoding="utf-8"))["project"]["version"]
    sources = {
        "modules/nsx-ble/src/ns_ble.c": 2,
        "modules/nsx-cordio/sdk/third_party/cordio/ble-profiles/sources/services/svc_dis.c": 1,
    }
    for relative, occurrences in sources.items():
        text = (repo_root / relative).read_text(encoding="utf-8")
        assert text.count(f'"{version}"') == occurrences, relative

    dis = (repo_root / "modules/nsx-cordio/sdk/third_party/cordio/ble-profiles/sources/services/svc_dis.c").read_text(
        encoding="utf-8"
    )
    declared_length = int(dis.split("DIS_DEFAULT_FW_REV_LEN")[1].split("\n")[0].strip())
    assert declared_length == len(version), "DIS_DEFAULT_FW_REV_LEN must match the advertised string length"


def test_no_promoted_archive_path_traverses_a_symlink(repo_root: Path) -> None:
    """A symlinked archive or directory would let the hash checks above verify a
    file outside the committed payload."""
    lib_root = repo_root / SDK_REL / "lib"
    offenders = [
        path.relative_to(repo_root).as_posix()
        for path in lib_root.rglob("*")
        if path.is_symlink()
    ]
    assert offenders == []
    for _, _, manifest_path, _ in _declared_artifacts(repo_root):
        parts = Path(manifest_path).parts
        promoted = lib_root / parts[0] / Path(*parts[2:])
        assert promoted.is_file() and not promoted.is_symlink(), manifest_path
