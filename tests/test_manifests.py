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
    "nsx-ambiq-usb",
    "nsx-audio",
    "nsx-usb",
    "nsx-freertos",
}

STAGED_PROVIDER_BOARDS = {
    "apollo2_evb",
    "apollo3_evb",
    "apollo3_evb_cygnus",
    "apollo3p_evb",
    "apollo3p_evb_cygnus",
    "apollo4l_evb",
    "apollo4l_blue_evb",
    "apollo4p_evb",
    "apollo4p_blue_kbr_evb",
    "apollo4p_blue_kxr_evb",
    "apollo4p_evb_disp_shield_rev2",
    "apollo330mP_evb",
    "apollo510_evb",
    "apollo510b_evb",
    "apollo510dL_evb",
    "atomiq110_fpga_turbo",
}
STAGED_PROVIDER_SOCS = {
    "apollo2",
    "apollo3",
    "apollo3p",
    "apollo4l",
    "apollo4p",
    "apollo330P",
    "apollo510",
    "apollo510b",
    "apollo510L",
    "atomiq110",
}
STAGED_PROVIDER_TOOLCHAINS = {"arm-none-eabi-gcc", "atfe", "armclang"}
EXPECTED_NSX_RELEASE_VERSION = "5.2.23"
EXPECTED_BOARD_DESCRIPTOR_VERSION = "0.1.0"


def expected_manifest_version(manifest: dict) -> str:
    if manifest["module"]["type"] == "board":
        return EXPECTED_BOARD_DESCRIPTOR_VERSION

    return EXPECTED_NSX_RELEASE_VERSION


def expected_provider_toolchains(repo_root: Path) -> list[str]:
    toolchains = ["arm-none-eabi-gcc", "atfe"]
    if (repo_root / "modules" / "nsx-ambiqsuite" / "sdk" / "lib" / "acfe").is_dir():
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
        expected_version = expected_manifest_version(manifest)
        if manifest["module"]["version"] != expected_version:
            offenders.append((path.name, manifest_name(manifest), manifest["module"]["version"]))
    assert offenders == []


def test_provider_manifest_matches_promoted_artifact_manifest(repo_root: Path, manifests: dict[Path, dict]) -> None:
    import yaml

    provider = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-ambiqsuite")
    artifact_path = repo_root / "modules" / "nsx-ambiqsuite" / "sdk" / "artifact-manifest.yaml"
    artifact = yaml.safe_load(artifact_path.read_text(encoding="utf-8"))

    assert provider["module"]["sdk_release"] == artifact["sdk"]["version"]
    assert provider["module"]["upstream_revision"] == artifact["sdk"]["source_commit"]


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


def test_power_manifest_declares_timer_dependency(manifests: dict[Path, dict]) -> None:
    power = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-power")

    assert "nsx-timer" in power["depends"].get("required", [])


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
    provider = next(manifest for manifest in manifests.values() if manifest_name(manifest) == "nsx-ambiqsuite")
    compatibility = provider["compatibility"]
    assert set(compatibility["socs"]) == STAGED_PROVIDER_SOCS
    assert set(compatibility["boards"]) == STAGED_PROVIDER_BOARDS
    assert compatibility["toolchains"] == expected_provider_toolchains(repo_root)

    payload = repo_root / "modules" / "nsx-ambiqsuite" / "sdk"
    # Precompiled HAL/BSP buckets are part-keyed; verify each staged part has
    # an mcu tree plus a HAL archive for every native toolchain.
    lib_parts = sorted(path.name for path in (payload / "lib" / "gcc").iterdir() if path.is_dir())
    for part in lib_parts:
        assert (payload / "mcu" / part).is_dir()
    for board in compatibility["boards"]:
        assert (payload / "boards" / board / "bsp").is_dir()
    for toolchain in ("gcc", "atfe"):
        for part in lib_parts:
            assert (payload / "lib" / toolchain / part / "libam_hal.a").is_file()


def test_ambiqsuite_dependent_manifests_advertise_staged_compatibility(manifests: dict[Path, dict]) -> None:
    offenders = []
    for path, manifest in manifests.items():
        dependencies = set(manifest["depends"].get("required", []))
        is_provider = manifest_name(manifest) == "nsx-ambiqsuite"
        if not is_provider and "nsx-ambiqsuite" not in dependencies:
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
    assert manifest["sdk"]["provider"] == "ambiqsuite"
    assert manifest["artifacts"]["output_root"]
    assert manifest["parts"]
    assert manifest["boards"]
    assert "scope_classification" in manifest["validation"]

    scope = manifest["scope"]
    assert "core_artifacts" in scope
    assert "optional_module_candidates" in scope
    assert "ignored_upstream_content" in scope
