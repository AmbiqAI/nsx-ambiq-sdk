from __future__ import annotations

from pathlib import Path


REQUIRED_TOP_LEVEL_KEYS = {
    "schema_version",
    "module",
    "support",
    "build",
    "depends",
    "compatibility",
}

REQUIRED_MODULE_KEYS = {
    "name",
    "type",
    "version",
}

VALID_MODULE_TYPES = {
    "backend_specific",
    "board",
    "runtime",
    "sdk_provider",
    "soc",
}

FORBIDDEN_CORE_CAPABILITIES = {
    "sensor_driver",
    "device_driver",
    "cmsis_nn",
    "cmsis_dsp",
    "freertos",
    "tinyusb",
    "bluetooth",
}

OPTIONAL_MIDDLEWARE_MODULES = {
    "nsx-ambiq-usb-r5",
    "nsx-audio",
    "nsx-usb",
}

STAGED_PROVIDER_BOARDS = {"apollo330mP_evb", "apollo510_evb", "apollo510b_evb", "apollo510dL_evb"}
STAGED_PROVIDER_SOCS = {"apollo330P", "apollo510", "apollo510b", "apollo510L"}
STAGED_PROVIDER_TOOLCHAINS = {"arm-none-eabi-gcc", "atfe", "armclang"}
EXPECTED_NSX_RELEASE_VERSION = "5.2.23"
EXPECTED_SDK_RELEASE = "R5.2.0"
EXPECTED_UPSTREAM_REVISION = "release_sdk5p2p0-bee737faa"


def expected_provider_toolchains(repo_root: Path) -> list[str]:
    toolchains = ["arm-none-eabi-gcc", "atfe"]
    if (repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk" / "lib" / "acfe").is_dir():
        toolchains.append("armclang")
    return toolchains


def manifest_name(manifest: dict) -> str:
    return manifest["module"]["name"]


def test_all_manifests_parse(manifests: dict[Path, dict]) -> None:
    assert manifests, "expected nsx-module.yaml manifests"
    for path, manifest in manifests.items():
        assert isinstance(manifest, dict), path
        assert REQUIRED_TOP_LEVEL_KEYS <= set(manifest), path
        assert REQUIRED_MODULE_KEYS <= set(manifest["module"]), path
        assert manifest["schema_version"] == 1, path
        assert manifest["module"]["type"] in VALID_MODULE_TYPES, path


def test_module_manifest_names_match_directories(module_dirs: list[Path], manifests: dict[Path, dict]) -> None:
    for module_dir in module_dirs:
        manifest = manifests[module_dir / "nsx-module.yaml"]
        assert manifest_name(manifest) == module_dir.name


def test_board_manifest_names_are_namespaced(board_dirs: list[Path], manifests: dict[Path, dict]) -> None:
    for board_dir in board_dirs:
        manifest = manifests[board_dir / "nsx-module.yaml"]
        assert manifest["module"]["type"] == "board"
        assert manifest_name(manifest).startswith("nsx-board-")


def test_manifest_names_are_unique(manifests: dict[Path, dict]) -> None:
    names = [manifest_name(manifest) for manifest in manifests.values()]
    assert len(names) == len(set(names))


def test_manifest_versions_are_aligned_to_sdk_release(manifests: dict[Path, dict]) -> None:
    offenders = []
    for path, manifest in manifests.items():
        if manifest["module"]["version"] != EXPECTED_NSX_RELEASE_VERSION:
            offenders.append((path.name, manifest_name(manifest), manifest["module"]["version"]))
    assert offenders == []

    provider = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-ambiqsuite-r5")
    assert provider["module"]["sdk_release"] == EXPECTED_SDK_RELEASE
    assert provider["module"]["upstream_revision"] == EXPECTED_UPSTREAM_REVISION


def test_manifest_build_targets_are_declared(manifests: dict[Path, dict]) -> None:
    for path, manifest in manifests.items():
        cmake = manifest["build"].get("cmake")
        assert isinstance(cmake, dict), path
        assert isinstance(cmake.get("package"), str) and cmake["package"], path
        targets = cmake.get("targets")
        assert isinstance(targets, list) and targets, path
        for target in targets:
            assert isinstance(target, str) and target.startswith("nsx::"), path


def test_manifest_dependency_blocks_are_lists(manifests: dict[Path, dict]) -> None:
    for path, manifest in manifests.items():
        depends = manifest["depends"]
        assert isinstance(depends.get("required"), list), path
        assert isinstance(depends.get("optional"), list), path


def test_required_manifest_dependencies_resolve(manifests: dict[Path, dict]) -> None:
    known_names = {manifest_name(manifest) for manifest in manifests.values()}
    unresolved = []
    for path, manifest in manifests.items():
        for dependency in manifest["depends"].get("required", []):
            if dependency not in known_names:
                unresolved.append((path.name, manifest_name(manifest), dependency))
    assert unresolved == []


def test_cmsis_core_policy_is_explicit(manifests: dict[Path, dict]) -> None:
    cmsis_core = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-cmsis-core")
    capabilities = set(cmsis_core.get("capabilities", []))
    assert "cmsis_core" in capabilities
    assert "cmsis_nn" not in capabilities
    assert "cmsis_dsp" not in capabilities


def test_core_manifests_do_not_advertise_out_of_scope_capabilities(manifests: dict[Path, dict]) -> None:
    offenders = []
    for path, manifest in manifests.items():
        if manifest_name(manifest) in OPTIONAL_MIDDLEWARE_MODULES:
            continue
        capabilities = {str(capability).lower() for capability in manifest.get("capabilities", [])}
        forbidden = sorted(capabilities & FORBIDDEN_CORE_CAPABILITIES)
        if forbidden:
            offenders.append((path.relative_to(path.parents[2]).as_posix(), forbidden))
    assert offenders == []


def test_sdk_provider_advertises_only_staged_payload(repo_root: Path, manifests: dict[Path, dict]) -> None:
    provider = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-ambiqsuite-r5")
    compatibility = provider["compatibility"]
    assert compatibility["socs"] == ["apollo330P", "apollo510", "apollo510b", "apollo510L"]
    assert compatibility["boards"] == ["apollo330mP_evb", "apollo510_evb", "apollo510b_evb", "apollo510dL_evb"]
    assert compatibility["toolchains"] == expected_provider_toolchains(repo_root)

    payload = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk"
    for soc in ("apollo330P", "apollo510", "apollo510L"):
        assert (payload / "mcu" / soc).is_dir()
    for board in compatibility["boards"]:
        assert (payload / "boards" / board / "bsp").is_dir()
    for toolchain in ("gcc", "atfe"):
        assert (payload / "lib" / toolchain / "apollo330P" / "libam_hal.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510" / "libam_hal.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510L" / "libam_hal.a").is_file()


def test_ambiqsuite_dependent_manifests_advertise_staged_compatibility(manifests: dict[Path, dict]) -> None:
    offenders = []
    for path, manifest in manifests.items():
        dependencies = set(manifest["depends"].get("required", []))
        is_provider = manifest_name(manifest) == "nsx-ambiqsuite-r5"
        if not is_provider and "nsx-ambiqsuite-r5" not in dependencies:
            continue

        compatibility = manifest["compatibility"]
        boards = set(compatibility.get("boards", []))
        socs = set(compatibility.get("socs", []))
        toolchains = set(compatibility.get("toolchains", []))

        if "*" in boards:
            boards.remove("*")
        if boards and not boards <= STAGED_PROVIDER_BOARDS:
            offenders.append((path.name, "boards", sorted(boards - STAGED_PROVIDER_BOARDS)))
        if socs and not socs <= STAGED_PROVIDER_SOCS:
            offenders.append((path.name, "socs", sorted(socs - STAGED_PROVIDER_SOCS)))
        if toolchains and not toolchains <= STAGED_PROVIDER_TOOLCHAINS:
            offenders.append((path.name, "toolchains", sorted(toolchains - STAGED_PROVIDER_TOOLCHAINS)))
    assert offenders == []


def test_sdk_drop_manifest_template_parseable(repo_root: Path) -> None:
    import yaml

    manifest_path = repo_root / "docs" / "sdk-drop-manifest.example.yaml"
    manifest = yaml.safe_load(manifest_path.read_text(encoding="utf-8"))
    assert manifest["sdk"]["provider"] == "ambiqsuite-r5"
    assert manifest["artifacts"]["output_root"]
    assert manifest["parts"]
    assert manifest["boards"]
    assert "scope_classification" in manifest["validation"]

    scope = manifest["scope"]
    assert "core_artifacts" in scope
    assert "optional_module_candidates" in scope
    assert "ignored_upstream_content" in scope
