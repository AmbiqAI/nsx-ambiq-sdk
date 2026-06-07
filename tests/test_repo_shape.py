from __future__ import annotations

from pathlib import Path
import re


MODULE_REPO_RESIDUE = {
    ".github",
    ".git",
    ".gitignore",
    "release-please-config.json",
    ".release-please-manifest.json",
}

OUT_OF_CORE_NAMES = {
    "freertos",
    "freertosv10.5.1",
    "tinyusb",
    "cordio",
}

OUT_OF_CORE_PATH_SUFFIXES = {
    ("src", "usb"),
    ("cmsis", "nn"),
    ("cmsis", "dsp"),
}

OPTIONAL_MIDDLEWARE_MODULE_PREFIXES = {
    ("modules", "nsx-ambiq-usb-r5"),
    ("modules", "nsx-usb"),
}

FORBIDDEN_POLICY_TERMS = (
    "AM_BSP_GPIO_COM_UART",
    "AM_BSP_UART_PRINT_INST",
    "NS_AM_BSP_GPIO_IOM1",
    "ns_high_drive_pins_enable",
    "am_devices_em9305",
    "AM_BSP_EM9305",
    "am_bsp_iom_pins_enable(1",
    "pdm_workaround",
)

FORBIDDEN_CORE_MODULES = {
    "nsx-peripherals",
}


def test_modules_are_logical_directories(module_dirs: list[Path]) -> None:
    assert module_dirs, "expected logical modules under modules/"
    module_names = {module_dir.name for module_dir in module_dirs}
    assert module_names.isdisjoint(FORBIDDEN_CORE_MODULES)
    for module_dir in module_dirs:
        assert (module_dir / "CMakeLists.txt").is_file(), module_dir
        assert (module_dir / "nsx-module.yaml").is_file(), module_dir
        assert (module_dir / "README.md").is_file(), module_dir


def test_boards_are_logical_modules(board_dirs: list[Path]) -> None:
    assert board_dirs, "expected board modules under boards/"
    for board_dir in board_dirs:
        assert (board_dir / "board.cmake").is_file(), board_dir
        assert (board_dir / "nsx-module.yaml").is_file(), board_dir
        assert (board_dir / "README.md").is_file(), board_dir


def test_no_nested_repo_scaffolding(repo_root: Path) -> None:
    offenders = []
    for path in (repo_root / "modules").glob("*/*"):
        if path.name in MODULE_REPO_RESIDUE:
            offenders.append(path.relative_to(repo_root).as_posix())
    assert offenders == []


def test_no_disabled_placeholder_sources(repo_root: Path) -> None:
    offenders = [
        path.relative_to(repo_root).as_posix()
        for path in repo_root.rglob("*")
        if path.is_file()
        and ".git" not in path.parts
        and path.relative_to(repo_root).parts[:2] != ("sdk-intake", "local")
        and path.suffix in {".dnc", ".notyet"}
    ]
    assert offenders == []


def test_dead_legacy_harness_files_are_removed(repo_root: Path) -> None:
    assert not (repo_root / "modules" / "nsx-harness" / "includes-api" / "ns_debug_log.h").exists()
    assert not (repo_root / "modules" / "nsx-harness" / "src" / "ns_debug_log.cc").exists()
    assert not (repo_root / "modules" / "nsx-harness" / "src" / "ns_micro_profiler.cc").exists()


def test_python_package_version_matches_manifest_release(repo_root: Path) -> None:
    pyproject = (repo_root / "pyproject.toml").read_text(encoding="utf-8")
    assert 'version = "5.2.23"' in pyproject


def test_repo_has_ci_and_pre_commit_entrypoints(repo_root: Path) -> None:
    workflow = repo_root / ".github" / "workflows" / "ci.yml"
    container_workflow = repo_root / ".github" / "workflows" / "container-validation.yml"
    pre_commit = repo_root / ".pre-commit-config.yaml"

    assert workflow.is_file()
    assert container_workflow.is_file()
    assert pre_commit.is_file()
    workflow_text = workflow.read_text(encoding="utf-8")
    container_workflow_text = container_workflow.read_text(encoding="utf-8")
    pre_commit_text = pre_commit.read_text(encoding="utf-8")
    assert "python -m pytest" in workflow_text
    assert "pre-commit run --all-files" in workflow_text
    assert "workflow_dispatch" in container_workflow_text
    assert "run_smoke" in container_workflow_text
    assert "run_sdk_build" in container_workflow_text
    assert "sdk_toolchain" in container_workflow_text
    assert "ambiqsuite_repo_path" in container_workflow_text
    assert "ambiqsuite_source_ref" in container_workflow_text
    assert "docker/dev-ci.Dockerfile" in container_workflow_text
    assert "GCC_ROOT/bin/arm-none-eabi-gcc" in container_workflow_text
    assert "ATFE_ROOT/bin/clang" in container_workflow_text
    assert "ACFE_ROOT/bin/armclang" in container_workflow_text
    assert "ARMLM_ACTIVATION_CODE" in container_workflow_text
    assert "tools/nsx_r5_link_smoke.py" in container_workflow_text
    assert "tools/nsx_r5_toolchain_file.py" in container_workflow_text
    assert "sdk-intake/build_ambiqsuite.py" in container_workflow_text
    assert "--toolchain" in container_workflow_text
    assert "--promote" in container_workflow_text
    assert "HOME=/root" in container_workflow_text
    assert "chown -R" in container_workflow_text
    assert "pytest contract tests" in pre_commit_text
    assert "sdk-intake/build_ambiqsuite.py" in pre_commit_text
    assert "tools/nsx_r5_link_smoke.py" in pre_commit_text
    assert "tools/nsx_r5_toolchain_file.py" in pre_commit_text


def test_toolchain_smoke_scaffold_is_present(repo_root: Path) -> None:
    legacy_license_env = "ARMLMD" + "_LICENSE_FILE"
    smoke_script = repo_root / "tools" / "nsx_r5_link_smoke.py"
    toolchain_script = repo_root / "tools" / "nsx_r5_toolchain_file.py"
    smoke_doc = repo_root / "docs" / "toolchain-smoke.md"
    dev_ci_dockerfile = repo_root / "docker" / "dev-ci.Dockerfile"
    devcontainer = repo_root / ".devcontainer" / "devcontainer.json"
    devcontainer_post_create = repo_root / ".devcontainer" / "post-create.sh"

    assert smoke_script.is_file()
    assert toolchain_script.is_file()
    assert smoke_doc.is_file()
    assert dev_ci_dockerfile.is_file()
    assert devcontainer.is_file()
    assert devcontainer_post_create.is_file()
    script_text = smoke_script.read_text(encoding="utf-8")
    toolchain_script_text = toolchain_script.read_text(encoding="utf-8")
    doc_text = smoke_doc.read_text(encoding="utf-8")
    dockerfile_text = dev_ci_dockerfile.read_text(encoding="utf-8")
    devcontainer_text = devcontainer.read_text(encoding="utf-8")
    devcontainer_post_create_text = devcontainer_post_create.read_text(encoding="utf-8")
    for board in ("apollo330mP_evb", "apollo510_evb", "apollo510b_evb", "apollo510dL_evb"):
        assert board in script_text
    assert "--toolchain-family" in doc_text
    assert "ARMLM_ACTIVATION_CODE" in doc_text
    assert legacy_license_env not in doc_text
    assert "CMAKE_EXECUTABLE_SUFFIX" in script_text
    assert "write_toolchain_file" in script_text
    assert "CMAKE_EXE_LINKER_FLAGS_INIT" in toolchain_script_text
    assert "thumbv8.1m.main-unknown-none-eabihf" in toolchain_script_text
    assert "arm-arm-none-eabi" in toolchain_script_text
    assert "ARM_GNU_VERSION=14.3.rel1" in dockerfile_text
    assert "developer.arm.com/-/media/Files/downloads/gnu" in dockerfile_text
    assert "GCC_ROOT" in dockerfile_text
    assert "arm/arm-toolchain/releases/download" in dockerfile_text
    assert "artifacts.tools.arm.com/arm-compiler" in dockerfile_text
    assert "ACFE_ROOT" in dockerfile_text
    assert "ATFE_ROOT" in dockerfile_text
    assert "dev-ci.Dockerfile" in devcontainer_text
    assert '"remoteUser": "root"' in devcontainer_text
    assert "UV_PROJECT_ENVIRONMENT" in devcontainer_text
    assert '"ARMLM_ACTIVATION_CODE": "${localEnv:ARMLM_ACTIVATION_CODE}"' in devcontainer_text
    assert legacy_license_env not in devcontainer_text
    assert "/opt/toolchains/gcc/bin:/opt/toolchains/acfe/bin:/opt/toolchains/atfe/bin" in devcontainer_text
    assert "/opt/venvs/nsx-r5" in devcontainer_text
    assert "bash .devcontainer/post-create.sh" in devcontainer_text
    assert "armlm" in devcontainer_post_create_text
    assert "activate -code" in devcontainer_post_create_text
    assert legacy_license_env not in devcontainer_post_create_text
    assert "safe.directory" in devcontainer_post_create_text
    assert "uv sync --group ci" in devcontainer_post_create_text


def test_user_readme_stays_concise(repo_root: Path) -> None:
    readme = repo_root / "README.md"
    text = readme.read_text(encoding="utf-8").lower()
    assert len(text.splitlines()) <= 90
    forbidden_phrases = (
        "initial migration",
        "repo readiness",
        "module survey",
        "follow-up work",
        "migration notes",
    )
    offenders = [phrase for phrase in forbidden_phrases if phrase in text]
    assert offenders == []


def test_maintainer_docs_are_indexed_and_pruned(repo_root: Path) -> None:
    docs_dir = repo_root / "docs"
    markdown_docs = {path.name for path in docs_dir.glob("*.md")}
    assert "README.md" in markdown_docs
    assert "module-survey.md" not in markdown_docs
    assert "repo-readiness-plan.md" not in markdown_docs

    index = (docs_dir / "README.md").read_text(encoding="utf-8")
    for doc_name in sorted(markdown_docs - {"README.md"}):
        assert doc_name in index


def test_module_readmes_describe_current_scope(repo_root: Path) -> None:
    forbidden_phrases = (
        "now live in",
        "relocated",
        "formerly",
        "no longer",
        "legacy neuralspot",
        "not part of the baseline",
        "do not belong",
    )
    offenders = []
    for path in sorted((repo_root / "modules").glob("*/README.md")):
        text = path.read_text(encoding="utf-8").lower()
        for phrase in forbidden_phrases:
            if phrase in text:
                offenders.append((path.relative_to(repo_root).as_posix(), phrase))
    assert offenders == []


def test_module_readmes_do_not_claim_unstaged_armclang_artifacts(repo_root: Path) -> None:
    forbidden_phrases = (
        "validated under `arm-none-eabi-gcc`, `armclang`",
        "built and validated under `arm-none-eabi-gcc`, `armclang`",
        "has its own prebuilt `libam_hal.a` variant",
        "sdk/lib/<part>/<board>/armclang",
    )
    offenders = []
    for path in sorted((repo_root / "modules").glob("*/README.md")):
        text = path.read_text(encoding="utf-8").lower()
        for phrase in forbidden_phrases:
            if phrase in text:
                offenders.append((path.relative_to(repo_root).as_posix(), phrase))
    assert offenders == []


def test_core_bundle_does_not_import_optional_middleware(repo_root: Path) -> None:
    offenders = []
    for path in repo_root.rglob("*"):
        relative_parts = path.relative_to(repo_root).parts
        if ".git" in path.parts or relative_parts[:2] == ("sdk-intake", "local"):
            continue
        if any(relative_parts[:len(prefix)] == prefix for prefix in OPTIONAL_MIDDLEWARE_MODULE_PREFIXES):
            continue
        relative_parts = tuple(part.lower() for part in relative_parts)
        if path.name.lower() in OUT_OF_CORE_NAMES:
            offenders.append(path.relative_to(repo_root).as_posix())
            continue
        for suffix in OUT_OF_CORE_PATH_SUFFIXES:
            if len(relative_parts) >= len(suffix) and relative_parts[-len(suffix):] == suffix:
                offenders.append(path.relative_to(repo_root).as_posix())
                break
    assert offenders == []


def test_curated_modules_do_not_ship_non_r5_implementation_residue(repo_root: Path) -> None:
    non_r5_path_names = {"apollo3", "apollo3p", "apollo4", "apollo4l", "apollo4p"}
    non_r5_text = re.compile(
        r"\b(AP3|AP4|Apollo3|Apollo4|apollo3p|apollo3|apollo4l|apollo4p|apollo4|"
        r"AM_PART_APOLLO3|AM_PART_APOLLO4|APOLLO3|APOLLO4|"
        r"NSX_SOC_FAMILIES_APOLLO3|NSX_SOC_FAMILIES_APOLLO4)\b|release_sdk_[34]"
    )
    offenders = []
    for path in (repo_root / "modules").rglob("*"):
        relative = path.relative_to(repo_root)
        relative_parts = relative.parts
        if ".git" in relative_parts:
            continue
        if relative_parts[:3] == ("modules", "nsx-ambiqsuite-r5", "sdk"):
            continue
        if relative_parts[:3] == ("modules", "nsx-ambiq-usb-r5", "sdk"):
            continue
        if path.is_dir() and path.name in non_r5_path_names:
            offenders.append(relative.as_posix())
            continue
        if not path.is_file() or path.name == "CHANGELOG.md":
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        if non_r5_text.search(text):
            offenders.append(relative.as_posix())
    assert offenders == []


def test_nsx_modules_do_not_ship_freertos_allocator_residue(repo_root: Path) -> None:
    offenders = []
    for path in (repo_root / "modules").rglob("*"):
        if not path.is_file() or "nsx-ambiqsuite-r5/sdk" in path.as_posix():
            continue
        relative_parts = path.relative_to(repo_root).parts
        if any(relative_parts[:len(prefix)] == prefix for prefix in OPTIONAL_MIDDLEWARE_MODULE_PREFIXES):
            continue
        text = path.read_text(encoding="utf-8", errors="ignore")
        if "FreeRTOS" in text or "pvTasklessPortMalloc" in text or "vTasklessPortFree" in text:
            offenders.append(path.relative_to(repo_root).as_posix())
    assert offenders == []


def test_raw_sdk_drop_staging_is_ignored_by_local_ingest_gitignore(repo_root: Path) -> None:
    gitignore = (repo_root / ".gitignore").read_text(encoding="utf-8")
    local_gitignore = (repo_root / "sdk-intake" / "local" / ".gitignore").read_text(encoding="utf-8")
    assert ".local-sdk-drops/" not in gitignore
    assert ".sdk-intake-work/" not in gitignore
    assert "*" in local_gitignore
    assert "!README.md" in local_gitignore


def test_sdk_intake_helper_is_source_controlled(repo_root: Path) -> None:
    helper = repo_root / "sdk-intake" / "build_ambiqsuite.py"
    readme = repo_root / "sdk-intake" / "README.md"
    assert helper.is_file()
    assert readme.is_file()
    helper_text = helper.read_text(encoding="utf-8")
    assert "--toolchain" in helper_text
    assert "apollo510dL_evb" in helper_text
    assert "bin-acfe" in helper_text
    assert "-g0" in helper_text
    assert "--debug-symbols" in helper_text
    assert "armlink" in helper_text
    assert "fromelf" in helper_text
    assert "ensure_acfe_armar_wrapper" in helper_text
    assert "normalize_args" in helper_text
    assert "is_compact_gnu_ar_flags" in helper_text
    assert '"AR", f"{sys.executable} {armar_wrapper} {armar}"' in helper_text


def test_i2c_module_contains_only_bus_contract(repo_root: Path) -> None:
    i2c_dir = repo_root / "modules" / "nsx-i2c"
    files = {path.relative_to(i2c_dir).as_posix() for path in i2c_dir.rglob("*") if path.is_file()}
    assert "includes-api/nsx_i2c.h" in files
    assert "includes-api/nsx_i2c_register_driver.h" in files
    assert "src/nsx_i2c.c" in files
    assert "src/nsx_i2c_register_driver.c" in files

    forbidden_fragments = {
        "max86150",
        "mpu6050",
    }
    offenders = [name for name in files if any(fragment in name.lower() for fragment in forbidden_fragments)]
    assert offenders == []


def test_active_module_code_has_no_hard_coded_board_policy(repo_root: Path) -> None:
    offenders = []
    for module_dir in sorted((repo_root / "modules").glob("*")):
        for relative_dir in ("src", "includes-api"):
            root = module_dir / relative_dir
            if not root.is_dir():
                continue
            for path in sorted(root.rglob("*")):
                if not path.is_file():
                    continue
                text = path.read_text(encoding="utf-8", errors="ignore")
                for term in FORBIDDEN_POLICY_TERMS:
                    if term in text:
                        offenders.append((path.relative_to(repo_root).as_posix(), term))
    assert offenders == []


def test_platform_coverage_names_unstaged_r5_variants(repo_root: Path) -> None:
    coverage = (repo_root / "docs" / "platform-coverage.md").read_text(encoding="utf-8")
    for expected in ("apollo510dL_evb", "apollo510L", "apollo330P", "apollo330mP_evb"):
        assert expected in coverage
    assert "artifact-built" in coverage


def test_staged_platform_coverage_matches_payload(repo_root: Path) -> None:
    payload = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk"
    assert (payload / "artifact-manifest.yaml").is_file()
    assert (payload.parent / "UPSTREAM-LICENSE-NOTICE.md").is_file()
    assert (payload.parent / "THIRD-PARTY-PAYLOAD.md").is_file()
    assert not (payload / "docs" / "licenses" / "LICENSE.pdf").exists()
    for license_file in (
        "LICENSE.rtf",
        "apache-2.0.txt",
        "MBEDtls_Apache2.txt",
        "OpenAmp_BSD-3.txt",
        "SEGGER-RTT-license.txt",
        "ThinkSi-license.txt",
    ):
        assert (payload / "docs" / "licenses" / license_file).is_file()

    assert (payload / "mcu" / "apollo510").is_dir()
    assert (payload / "CMSIS" / "AmbiqMicro" / "Source" / "system_apollo510.c").is_file()
    assert (payload / "mcu" / "apollo330P").is_dir()
    assert (payload / "CMSIS" / "AmbiqMicro" / "Source" / "system_apollo330P.c").is_file()
    assert (payload / "mcu" / "apollo510L").is_dir()
    assert (payload / "CMSIS" / "AmbiqMicro" / "Source" / "system_apollo510L.c").is_file()
    assert not (payload / "src" / "apollo510").exists()
    assert not (payload / "src" / "apollo330P").exists()
    assert not (payload / "src" / "apollo510L").exists()
    assert (payload / "boards" / "apollo510_evb" / "bsp").is_dir()
    assert (payload / "boards" / "apollo510b_evb" / "bsp").is_dir()
    assert (payload / "boards" / "apollo330mP_evb" / "bsp").is_dir()
    assert (payload / "boards" / "apollo510dL_evb" / "bsp").is_dir()
    assert not (payload / "utils" / "am_util_id.h").exists()
    assert '#include "am_util_id.h"' not in (payload / "utils" / "am_util.h").read_text(encoding="utf-8")
    assert '#include "am_util_multi_boot_secure.h"' not in (payload / "utils" / "am_util_multi_boot_private.h").read_text(encoding="utf-8")
    for header_name in (
        "am_devices_510L_radio.h",
        "am_devices_display_generic.h",
        "am_devices_dc_dbi_novatek.h",
        "am_devices_dc_dpi_japandisplayinc.h",
        "am_devices_dc_dsi_forcelead.h",
        "am_devices_dc_dsi_novatek.h",
        "am_devices_dc_dsi_raydium.h",
        "am_devices_dc_jdi_sharp.h",
        "am_devices_dc_xspi_raydium.h",
    ):
        assert not (payload / "devices" / header_name).exists()
    for board_name in ("apollo510_evb", "apollo510b_evb", "apollo510dL_evb"):
        board_text = (payload / "boards" / board_name / "bsp" / "am_bsp.h").read_text(encoding="utf-8")
        assert '#include "am_devices_display_generic.h"' not in board_text
        assert "am_devices_display_hw_config_t g_sDispCfg;" not in board_text

    toolchains = ["gcc", "atfe"]
    if (payload / "lib" / "acfe").is_dir():
        toolchains.append("acfe")

    for toolchain in toolchains:
        assert (payload / "lib" / toolchain / "apollo510" / "libam_hal.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510" / "apollo510_evb" / "libam_bsp.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510" / "apollo510b_evb" / "libam_bsp.a").is_file()
        assert (payload / "lib" / toolchain / "apollo330P" / "libam_hal.a").is_file()
        assert (payload / "lib" / toolchain / "apollo330P" / "apollo330mP_evb" / "libam_bsp.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510L" / "libam_hal.a").is_file()
        assert (payload / "lib" / toolchain / "apollo510L" / "apollo510dL_evb" / "libam_bsp.a").is_file()


def test_provider_payload_third_party_boundaries(repo_root: Path) -> None:
    payload = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk"
    files = [path for path in payload.rglob("*") if path.is_file()]

    segger_payload = [
        path.relative_to(payload).as_posix()
        for path in files
        if any(term in path.as_posix().lower() for term in ("segger", "rtt", "jlink", "systemview"))
    ]
    assert segger_payload == ["docs/licenses/SEGGER-RTT-license.txt"]

    forbidden_fragments = (
        "third_party/thinksi",
        "nemagfx_sdk",
        "nemagfx_extensions",
    )
    offenders = [path.relative_to(payload).as_posix() for path in files if any(fragment in path.as_posix().lower() for fragment in forbidden_fragments)]
    assert offenders == []


def test_provider_payload_source_boundary_is_explicit(repo_root: Path) -> None:
    payload = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk"
    top_level_sources = sorted(path.name for path in (payload / "src").glob("*.c"))
    assert top_level_sources == [
        "am_util_delay.c",
        "am_util_pmu.c",
        "am_util_stdio.c",
    ]
    cmsis_system_sources = sorted(path.name for path in (payload / "CMSIS" / "AmbiqMicro" / "Source").glob("system_*.c"))
    assert cmsis_system_sources == [
        "system_apollo330P.c",
        "system_apollo510.c",
        "system_apollo510L.c",
    ]

    forbidden_fragments = (
        "am_devices_",
        "am_resources",
        "am_util_id",
    )
    offenders = [
        path.relative_to(repo_root).as_posix()
        for path in (payload / "src").rglob("*.c")
        if any(fragment in path.name for fragment in forbidden_fragments)
    ]
    assert offenders == []


def test_r5_artifact_manifest_matches_built_libraries(repo_root: Path) -> None:
    import yaml

    artifact_root = repo_root / "artifacts" / "ambiqsuite" / "R5.2.0"
    manifest = yaml.safe_load((artifact_root / "manifest.yaml").read_text(encoding="utf-8"))
    assert manifest["sdk"]["source_kind"] == "git_ref"
    assert manifest["sdk"]["source_ref"] == "release_sdk5p2p0_rc4"
    assert manifest["sdk"].get("upstream_revision") != "${version}"
    assert manifest["build"]["toolchains"]["gcc"]["status"] == "built"
    assert manifest["build"]["toolchains"]["atfe"]["status"] == "built"
    assert manifest["build"]["toolchains"]["acfe"]["status"] in {"blocked", "not-built", "built"}
    assert manifest["build"]["toolchains"]["gcc"]["debug_symbols"] is False
    assert manifest["build"]["toolchains"]["atfe"]["debug_symbols"] is False
    assert manifest["build"]["toolchains"]["acfe"]["debug_symbols"] is False

    artifact_paths = []
    for part in manifest["parts"]:
        for toolchain in ("gcc", "atfe", "acfe"):
            if toolchain not in part["hal_artifacts"]:
                continue
            artifact_paths.append(part["hal_artifacts"][toolchain]["path"])
    for board in manifest["boards"]:
        for toolchain in ("gcc", "atfe", "acfe"):
            if toolchain not in board["bsp_artifacts"]:
                continue
            artifact_paths.append(board["bsp_artifacts"][toolchain]["path"])

    for relative_path in artifact_paths:
        assert (artifact_root / relative_path).is_file(), relative_path
