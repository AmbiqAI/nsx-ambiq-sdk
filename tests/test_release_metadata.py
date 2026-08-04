from __future__ import annotations

from pathlib import Path

try:
    import tomllib
except ModuleNotFoundError:
    import tomli as tomllib

import pytest
import yaml


RELEASE_MANIFEST = Path("release/nsx-ambiq-sdk-5.2.24.yaml")
SUPERSEDED_RELEASE_MANIFEST = Path("release/nsx-ambiq-sdk-5.2.23.yaml")
ARTIFACT_MANIFEST = Path("modules/nsx-ambiqsuite/sdk/artifact-manifest.yaml")
PROVIDER_MANIFEST = Path("modules/nsx-ambiqsuite/nsx-module.yaml")
OWNERSHIP_INVENTORY = Path("release/source-ownership.yaml")


def load_yaml(repo_root: Path, path: Path) -> dict:
    return yaml.safe_load((repo_root / path).read_text(encoding="utf-8"))


def test_distribution_versions_are_consistent(repo_root: Path, module_dirs: list[Path]) -> None:
    pyproject = tomllib.loads((repo_root / "pyproject.toml").read_text(encoding="utf-8"))
    root_version = pyproject["project"]["version"]
    release = load_yaml(repo_root, RELEASE_MANIFEST)

    assert root_version == "5.2.24"
    assert release["distribution"]["version"] == root_version
    assert release["distribution"]["tag"] == f"v{root_version}"

    offenders = {}
    for module_dir in module_dirs:
        manifest_path = module_dir / "nsx-module.yaml"
        manifest = yaml.safe_load(manifest_path.read_text(encoding="utf-8"))
        if manifest["module"]["version"] != root_version:
            offenders[module_dir.name] = manifest["module"]["version"]
    assert offenders == {}


def test_release_provenance_matches_promoted_payload(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    artifact = load_yaml(repo_root, ARTIFACT_MANIFEST)
    provider = load_yaml(repo_root, PROVIDER_MANIFEST)
    upstream = release["upstream"]

    assert Path(upstream["artifact_manifest"]) == ARTIFACT_MANIFEST
    assert upstream["provider"] == artifact["sdk"]["provider"]
    assert upstream["snapshot_identity"] == artifact["sdk"]["version"]
    assert upstream["source_kind"] == artifact["sdk"]["source_kind"]
    assert upstream["source_ref"] == artifact["sdk"]["source_ref"]
    assert upstream["source_commit"] == artifact["sdk"]["source_commit"]
    assert upstream["raw_source_committed"] == artifact["sdk"]["raw_source_committed"]
    assert upstream["official_numbered_release_claimed"] is False

    provider_module = provider["module"]
    assert provider_module["sdk_release"] == upstream["snapshot_identity"]
    assert provider_module["upstream_revision"] == upstream["source_commit"]


def test_release_uses_tag_target_commit_resolution(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    distribution = release["distribution"]
    git = distribution["git"]

    assert git["commit"] is None
    assert git["commit_resolution"]["source"] == "immutable_tag_target"
    assert git["commit_resolution"]["command"] == f"git rev-list -n 1 {distribution['tag']}"

    def mapping_keys(value: object) -> set[str]:
        if isinstance(value, dict):
            return set(value) | {key for child in value.values() for key in mapping_keys(child)}
        if isinstance(value, list):
            return {key for child in value for key in mapping_keys(child)}
        return set()

    assert "curation_revision" not in mapping_keys(release)


def test_qualified_descriptors_exist_and_match_provider_scope(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    provider = load_yaml(repo_root, PROVIDER_MANIFEST)
    scope = release["qualification"]["scope"]
    exclusions = release["exclusions"]

    qualified_socs = set(scope["socs"])
    qualified_boards = set(scope["boards"])
    experimental_socs = set(exclusions["experimental"]["socs"])
    experimental_boards = set(exclusions["experimental"]["boards"])

    assert qualified_socs == set(provider["compatibility"]["socs"]) - experimental_socs
    assert qualified_boards == set(provider["compatibility"]["boards"]) - experimental_boards

    for soc in qualified_socs:
        assert (repo_root / "cmake" / "socs" / f"{soc}.cmake").is_file()
        assert (repo_root / "cmake" / "socs" / "facts" / f"{soc}.cmake").is_file()
    for board in qualified_boards:
        assert (repo_root / "boards" / board / "board.cmake").is_file()

    assert qualified_socs.isdisjoint(experimental_socs)
    assert qualified_boards.isdisjoint(experimental_boards)
    assert qualified_socs.isdisjoint(exclusions["descriptor_only"]["socs"])
    assert qualified_boards.isdisjoint(exclusions["descriptor_only"]["boards"])


def test_release_toolchains_match_built_artifact_families(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    artifact = load_yaml(repo_root, ARTIFACT_MANIFEST)
    toolchains = release["qualification"]["toolchains"]

    assert {item["artifact_manifest_key"] for item in toolchains} == set(artifact["build"]["toolchains"])
    for item in toolchains:
        artifact_toolchain = artifact["build"]["toolchains"][item["artifact_manifest_key"]]
        assert item["artifact_status"] == artifact_toolchain["status"] == "built"


def test_release_evidence_exists(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    missing = [path for path in release["release_evidence"] if not (repo_root / path).is_file()]
    assert missing == []


def test_superseded_release_records_are_preserved(repo_root: Path) -> None:
    """A corrected release must never erase the record it corrects.

    `5.2.23` shipped a stale artifact manifest; `5.2.24` fixes it. Both release
    manifests and both qualification reports stay in the tree so the published,
    immutable `v5.2.23` remains describable from any later checkout.
    """
    assert (repo_root / SUPERSEDED_RELEASE_MANIFEST).is_file()
    superseded = load_yaml(repo_root, SUPERSEDED_RELEASE_MANIFEST)
    assert superseded["distribution"]["version"] == "5.2.23"
    assert (repo_root / superseded["qualification"]["report"]).is_file()

    release = load_yaml(repo_root, RELEASE_MANIFEST)
    supersedes = release["distribution"]["supersedes"]
    assert supersedes["version"] == superseded["distribution"]["version"]
    assert supersedes["tag"] == superseded["distribution"]["tag"]
    assert supersedes["commit"] == "2eba24ad776096784764cbe91c8176b434dd3bdf"
    assert supersedes["reason"].strip()


def test_payload_provenance_matches_the_promoted_artifact_manifest(repo_root: Path) -> None:
    """Release-level provenance must agree with the payload it describes.

    The `v5.2.23` defect was exactly a disagreement between recorded provenance
    and shipped bytes, so every provenance claim the release manifest makes is
    cross-checked against the artifact manifest rather than trusted.
    """
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    artifact = load_yaml(repo_root, ARTIFACT_MANIFEST)
    provenance = release["payload_provenance"]
    upstream = release["upstream"]

    assert provenance["artifact_manifest_generated_at"] == artifact["build"]["generated_at"]

    declared = {train["manifest_key"]: train for train in provenance["toolchain_trains"]}
    assert set(declared) == set(artifact["build"]["toolchains"])

    for key, train in declared.items():
        assert train["built_from_source_commit"] == upstream["source_commit"]
        assert train["archives_introduced_in"], key
        for commit in train["archives_introduced_in"]:
            assert len(commit) == 40 and set(commit) <= set("0123456789abcdef"), (key, commit)
        recorded_abi = artifact["build"]["toolchains"][key].get("abi_cflags")
        assert train.get("abi_cflags") == recorded_abi, key

    acfe = declared["acfe"]
    assert acfe["manifest_hashes_correct_in_5_2_23"] is False
    assert (repo_root / acfe["forensic_report"]).is_file()
    assert "ddb88640e61660edc65ebc956b65dcbd6804d2e6" in acfe["archives_introduced_in"]


def test_known_deviations_are_explicit_and_point_at_real_paths(repo_root: Path) -> None:
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    for deviation in release.get("known_deviations", []):
        assert deviation["id"]
        assert deviation["description"].strip()
        for path in deviation["paths"]:
            assert (repo_root / path).exists(), path


def test_archives_introduced_in_matches_git_history(repo_root: Path) -> None:
    """Verify the release manifest's per-train attribution against git.

    The first draft of this manifest attributed the `atfe` archives to the wrong
    commit by copying the `gcc` block. Recording provenance that nobody checks is
    how the defect this release corrects happened in the first place, so the
    claim is verified rather than trusted.

    Skipped when the checkout has no usable history (for example a shallow CI
    clone), since the property is about history, not the worktree.
    """
    import subprocess

    artifact = load_yaml(repo_root, ARTIFACT_MANIFEST)
    release = load_yaml(repo_root, RELEASE_MANIFEST)
    sdk_rel = Path("modules") / "nsx-ambiqsuite" / "sdk"

    probe = subprocess.run(
        ["git", "-C", str(repo_root), "rev-parse", "--is-shallow-repository"],
        capture_output=True,
        text=True,
    )
    if probe.returncode != 0 or probe.stdout.strip() != "false":
        pytest.skip("git history unavailable or shallow")

    declared = {train["manifest_key"]: train for train in release["payload_provenance"]["toolchain_trains"]}
    for section, artifact_key in (("parts", "hal_artifacts"), ("boards", "bsp_artifacts")):
        for entry in artifact.get(section) or []:
            for toolchain, info in (entry.get(artifact_key) or {}).items():
                parts = Path(info["path"]).parts
                promoted = sdk_rel / "lib" / parts[0] / Path(*parts[2:])
                result = subprocess.run(
                    ["git", "-C", str(repo_root), "log", "-1", "--format=%H", "HEAD", "--", promoted.as_posix()],
                    capture_output=True,
                    text=True,
                )
                if result.returncode != 0 or not result.stdout.strip():
                    pytest.skip("git history unavailable for promoted archives")
                introducing = result.stdout.strip()
                assert introducing in declared[toolchain]["archives_introduced_in"], (
                    f"{promoted.as_posix()} was last changed by {introducing}, which is not listed under "
                    f"payload_provenance.toolchain_trains[{toolchain}].archives_introduced_in"
                )

    for train in declared.values():
        assert train["archives_introduced_in"], train["manifest_key"]


def test_source_ownership_inventory_covers_required_boundaries(repo_root: Path) -> None:
    inventory = load_yaml(repo_root, OWNERSHIP_INVENTORY)
    entries = {entry["id"]: entry for entry in inventory["entries"]}

    assert entries["generated-ambiqsuite-provider"]["direct_edit"] == "forbidden"
    assert entries["upstream-ambiq-usb-copy"]["direct_edit"] == "restricted"
    assert entries["upstream-psram-device-drivers"]["direct_edit"] == "restricted"
    assert entries["nsx-module-integration"]["classification"] == "nsx-owned"
    assert entries["nsx-soc-descriptors"]["paths"] == ["cmake/socs"]
    assert entries["nsx-board-descriptors"]["paths"] == ["boards"]

    concrete_paths = [
        path
        for entry in entries.values()
        for path in entry.get("paths", [])
    ]
    assert all((repo_root / path).exists() for path in concrete_paths)
