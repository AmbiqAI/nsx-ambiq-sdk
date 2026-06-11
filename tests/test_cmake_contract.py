from __future__ import annotations

from pathlib import Path
import re
import shutil
import subprocess


def read(repo_root: Path, relative_path: str) -> str:
    return (repo_root / relative_path).read_text(encoding="utf-8")


def soc_facts_text(repo_root: Path, skew: str) -> str:
    """Return the text that declares a SoC's facts.

    SoC facts are the single source of truth shared between the SDK's own
    cmake/socs/<skew>.cmake descriptor and downstream board.cmake files. They
    may be extracted into a side-effect-free cmake/socs/_facts/<skew>.cmake or
    still inlined in cmake/socs/<skew>.cmake. Prefer the extracted file when it
    exists so the contract holds during a mixed (partially-extracted) state.
    """
    facts = repo_root / "cmake" / "socs" / "_facts" / f"{skew}.cmake"
    if facts.exists():
        return facts.read_text(encoding="utf-8")
    return read(repo_root, f"cmake/socs/{skew}.cmake")


def test_hal_and_bsp_use_provider_local_toolchain_artifacts(repo_root: Path) -> None:
    hal = read(repo_root, "modules/nsx-ambiq-hal-r5/CMakeLists.txt")
    bsp = read(repo_root, "modules/nsx-ambiq-bsp-r5/CMakeLists.txt")

    assert "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN}/${NSX_AMBIQ_HAL_LIB_PART_NAME}/libam_hal.a" in hal
    assert "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN}/${NSX_AMBIQ_BSP_LIB_PART_NAME}/${NSX_AMBIQ_BSP_LIB_SUBDIR}/libam_bsp.a" in bsp
    assert "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQ_HAL_LIB_PART_NAME}/libam_hal.a" not in hal
    assert "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQ_BSP_LIB_PART_NAME}/${NSX_AMBIQ_BSP_LIB_SUBDIR}/libam_bsp.a" not in bsp


def test_soc_hal_alias_is_descriptor_driven(repo_root: Path) -> None:
    cmake = read(repo_root, "modules/nsx-soc-hal/CMakeLists.txt")
    manifest = read(repo_root, "modules/nsx-soc-hal/nsx-module.yaml")

    assert "NSX_SOC_TARGET_EXPORT_NAME" in cmake
    assert "soc_hal_apollo510 ALIAS" not in cmake
    assert "apollo2" in manifest
    assert "apollo3p" in manifest
    assert "apollo4p" in manifest
    assert "apollo330P" in manifest
    assert "apollo510L" in manifest


def test_native_artifact_names_are_explicit(repo_root: Path) -> None:
    apollo330p = read(repo_root, "cmake/socs/apollo330P.cmake")
    apollo510_soc = read(repo_root, "cmake/socs/apollo510.cmake")
    apollo510b = read(repo_root, "cmake/socs/apollo510b.cmake")
    apollo510l = read(repo_root, "cmake/socs/apollo510L.cmake")
    apollo5b = read(repo_root, "cmake/socs/apollo5b.cmake")
    apollo510b_board = read(repo_root, "boards/apollo510b_evb/board.cmake")
    apollo510_board = read(repo_root, "boards/apollo510_evb/board.cmake")
    apollo330 = read(repo_root, "boards/apollo330mP_evb/board.cmake")
    apollo510dl = read(repo_root, "boards/apollo510dL_evb/board.cmake")

    assert "CMSIS/AmbiqMicro/Source/system_apollo330P.c" in apollo330p
    assert "CMSIS/AmbiqMicro/Source/system_apollo510.c" in apollo510_soc
    assert 'set(NSX_AMBIQ_HAL_LIB_PART_NAME "apollo510")' in apollo510b
    assert "CMSIS/AmbiqMicro/Source/system_apollo510.c" in apollo510b
    assert "src/apollo510b/gcc/startup_gcc.c" in apollo510b
    assert "src/apollo510b/armclang/startup_keil6.c" in apollo510b
    assert "CMSIS/AmbiqMicro/Source/system_apollo510L.c" in apollo510l
    assert "src/apollo510L/gcc/startup_gcc.c" in apollo510l
    assert "src/apollo510L/armclang/startup_keil6.c" in apollo510l
    assert "CMSIS/AmbiqMicro/Source/system_apollo510.c" in apollo5b
    assert 'set(NSX_AMBIQ_BSP_LIB_PART_NAME "apollo510")' in apollo510b_board
    assert 'set(NSX_AMBIQ_BSP_LIB_SUBDIR "apollo510b_evb")' in apollo510b_board
    assert 'set(NSX_AMBIQ_BSP_LIB_SUBDIR "apollo510_evb")' in apollo510_board
    assert 'set(NSX_AMBIQ_BSP_LIB_SUBDIR "apollo330mP_evb")' in apollo330
    assert 'set(NSX_AMBIQ_BSP_LIB_PART_NAME "apollo510L")' in apollo510dl
    assert 'set(NSX_AMBIQ_BSP_LIB_SUBDIR "apollo510dL_evb")' in apollo510dl


def test_promoted_provider_payload_keeps_header_only_buckets_source_free(repo_root: Path) -> None:
    payload = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk"
    forbidden_sources = []
    for root in [payload / "mcu", payload / "boards", payload / "devices", payload / "utils"]:
        for path in root.rglob("*.c"):
            forbidden_sources.append(path.relative_to(repo_root).as_posix())
    for path in payload.rglob("*.mk"):
        forbidden_sources.append(path.relative_to(repo_root).as_posix())
    for path in payload.rglob("Makefile"):
        forbidden_sources.append(path.relative_to(repo_root).as_posix())
    for path in (payload / "boards").rglob("*.src"):
        forbidden_sources.append(path.relative_to(repo_root).as_posix())
    for name in ("iar", "keil6"):
        for path in (payload / "boards").rglob(name):
            forbidden_sources.append(path.relative_to(repo_root).as_posix())
    assert forbidden_sources == []


def test_cmake_descriptor_includes_resolve_inside_repo(repo_root: Path) -> None:
    include_pattern = re.compile(r'include\("\$\{NSX_CMAKE_DIR\}/([^"()]+)"\)')
    missing = []
    for path in (repo_root / "cmake").rglob("*.cmake"):
        for include_path in include_pattern.findall(path.read_text(encoding="utf-8")):
            if not (repo_root / "cmake" / include_path).is_file():
                missing.append(include_path)
    assert sorted(missing) == []


def test_toolchain_helper_defines_required_contract_functions(repo_root: Path) -> None:
    helper = read(repo_root, "cmake/nsx_toolchain_flags.cmake")
    assert "function(nsx_assert_file_exists path)" in helper
    assert "function(nsx_assert_directory_exists path)" in helper
    assert "function(nsx_assert_path_component var_name)" in helper
    assert "function(nsx_require_enum_value var_name)" in helper
    assert "function(nsx_toolchain_uses_newlib out_var)" in helper
    assert "function(nsx_toolchain_is_armclang out_var)" in helper
    assert "function(nsx_select_soc_arch_dir out_var)" in helper
    assert "function(nsx_validate_prebuilt_abi)" in helper
    assert "function(nsx_resolve_ambiqsuite_artifact_toolchain out_var)" in helper
    assert "function(nsx_apply_toolchain_flags target)" in helper
    assert "set(NSX_AMBIQSUITE_R5_TOOLCHAIN_FAMILIES gcc atfe armclang)" in helper
    assert "thumbv8.1m.main-unknown-none-eabihf" in helper
    assert "arm-arm-none-eabi" in helper
    assert "set(nsx_link_flags --cpu=${NSX_CPU})" in helper
    assert "target_compile_options(${target} INTERFACE ${nsx_compile_flags})" in helper


def test_unified_cmake_selectors_cover_multi_tier_soc_families(repo_root: Path) -> None:
    helper = read(repo_root, "cmake/nsx_toolchain_flags.cmake")
    assert "set(NSX_SOC_FAMILIES_APOLLO2 apollo2)" in helper
    assert "set(NSX_SOC_FAMILIES_APOLLO3 apollo3 apollo3p)" in helper
    assert "set(NSX_SOC_FAMILIES_APOLLO4 apollo4l apollo4p)" in helper
    assert "set(NSX_SOC_FAMILIES_APOLLO5 apollo5b apollo510 apollo510b apollo510L)" in helper

    legacy_selector = re.compile(r"\b(APOLLO3|APOLLO4|NSX_SOC_FAMILIES_APOLLO3|NSX_SOC_FAMILIES_APOLLO4)\b")
    offenders = []
    for module_name in ("nsx-ambiq-hal-r5", "nsx-ambiq-bsp-r5", "nsx-ambiq-usb-r5"):
        path = repo_root / "modules" / module_name / "CMakeLists.txt"
        text = path.read_text(encoding="utf-8")
        if legacy_selector.search(text):
            offenders.append(path.relative_to(repo_root).as_posix())
    assert offenders == []
    assert "target_link_options(${target} INTERFACE ${nsx_link_flags})" in helper


def test_sdk_artifact_paths_validate_components_before_construction(repo_root: Path) -> None:
    hal = read(repo_root, "modules/nsx-ambiq-hal-r5/CMakeLists.txt")
    bsp = read(repo_root, "modules/nsx-ambiq-bsp-r5/CMakeLists.txt")

    for expected in (
        "nsx_assert_path_component(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)",
        "nsx_assert_path_component(NSX_AMBIQ_HAL_LIB_PART_NAME)",
        "nsx_assert_path_component(NSX_AMBIQ_MCU_INSTALL_NAME)",
    ):
        assert expected in hal

    for expected in (
        "nsx_assert_path_component(NSX_AMBIQ_BOARD_NAME)",
        "nsx_assert_path_component(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)",
        "nsx_assert_path_component(NSX_AMBIQ_BSP_LIB_PART_NAME)",
        "nsx_assert_path_component(NSX_AMBIQ_BSP_LIB_SUBDIR)",
    ):
        assert expected in bsp


def configure_contract_project(
    repo_root: Path,
    tmp_path: Path,
    board: str,
    toolchain_family: str,
    modules: tuple[str, ...] = (),
    *,
    provider: str = "ambiqsuite-r5",
    ambiqsuite_version: str = "R5.2.0",
    provider_module: str = "nsx-ambiqsuite-r5",
    hal_module: str = "nsx-ambiq-hal-r5",
    bsp_module: str = "nsx-ambiq-bsp-r5",
) -> subprocess.CompletedProcess[str]:
    source_dir = tmp_path / toolchain_family / board
    build_dir = source_dir / "build"
    source_dir.mkdir(parents=True)
    module_lines = [
        f'add_subdirectory("{(repo_root / "modules" / module).as_posix()}" {module})'
        for module in modules
    ]
    (source_dir / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "if(POLICY CMP0123)",
                "    cmake_policy(SET CMP0123 NEW)",
                "endif()",
                "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)",
                "project(nsx_contract C)",
                "include(GNUInstallDirs)",
                f'set(NSX_ROOT "{repo_root.as_posix()}")',
                f'set(NSX_CMAKE_DIR "{(repo_root / "cmake").as_posix()}")',
                f'set(NSX_SDK_PROVIDER "{provider}")',
                f'set(NSX_TOOLCHAIN_FAMILY "{toolchain_family}")',
                f'set(NSX_AMBIQSUITE_VERSION "{ambiqsuite_version}")',
                f'set(NSX_AMBIQSUITE_ROOT "{(repo_root / "modules" / provider_module / "sdk").as_posix()}")',
                f'include("{(repo_root / "boards" / board / "board.cmake").as_posix()}")',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-cmsis-core").as_posix()}" nsx-cmsis-core)',
                f'add_subdirectory("{(repo_root / "modules" / hal_module).as_posix()}" {hal_module})',
                f'add_subdirectory("{(repo_root / "modules" / bsp_module).as_posix()}" {bsp_module})',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-soc-hal").as_posix()}" nsx-soc-hal)',
                *module_lines,
                "",
            ]
        ),
        encoding="utf-8",
    )
    return subprocess.run(
        ["cmake", "-S", str(source_dir), "-B", str(build_dir)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )


def test_staged_boards_configure_with_promoted_gcc_and_atfe_artifacts(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    boards = ("apollo330mP_evb", "apollo510_evb", "apollo510b_evb", "apollo510dL_evb")
    for toolchain_family in ("gcc", "atfe"):
        for board in boards:
            result = configure_contract_project(repo_root, tmp_path, board, toolchain_family)
            assert result.returncode == 0, result.stdout


def test_armclang_configure_reflects_acfe_artifact_availability(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    result = configure_contract_project(repo_root, tmp_path, "apollo510_evb", "armclang")
    acfe_hal = repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk" / "lib" / "acfe" / "apollo510" / "libam_hal.a"
    if acfe_hal.exists():
        assert result.returncode == 0, result.stdout
    else:
        assert result.returncode != 0
        assert "sdk/lib/acfe/apollo510/libam_hal.a" in result.stdout


def test_runtime_modules_configure_through_soc_hal_contract(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    modules = (
        "nsx-core",
        "nsx-interrupt",
        "nsx-timer",
        "nsx-perf",
        "nsx-audio",
        "nsx-i2c",
        "nsx-spi",
        "nsx-power",
        "nsx-uart",
    )
    for board in ("apollo510_evb", "apollo510dL_evb"):
        result = configure_contract_project(repo_root, tmp_path, board, "gcc", modules)
        assert result.returncode == 0, result.stdout


def test_usb_substrate_configures_as_optional_sdk_module(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    result = configure_contract_project(repo_root, tmp_path, "apollo510_evb", "gcc", ("nsx-ambiq-usb-r5",))
    assert result.returncode == 0, result.stdout


def test_usb_module_configures_inside_sdk_repo(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    result = configure_contract_project(
        repo_root,
        tmp_path,
        "apollo510_evb",
        "gcc",
        (
            "nsx-core",
            "nsx-interrupt",
            "nsx-timer",
            "nsx-perf",
            "nsx-ambiq-usb-r5",
            "nsx-usb",
        ),
    )
    assert result.returncode == 0, result.stdout


def test_r4_runtime_modules_configure_through_soc_hal_contract(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    modules = (
        "nsx-core",
        "nsx-interrupt",
        "nsx-timer",
        "nsx-i2c",
        "nsx-spi",
        "nsx-uart",
        "nsx-psram",
    )
    result = configure_contract_project(
        repo_root,
        tmp_path,
        "apollo4p_evb",
        "gcc",
        modules,
        provider="ambiqsuite-r4",
        ambiqsuite_version="R4.5.0",
        provider_module="nsx-ambiqsuite-r4",
        hal_module="nsx-ambiq-hal-r4",
        bsp_module="nsx-ambiq-bsp-r4",
    )
    assert result.returncode == 0, result.stdout


def test_r3_runtime_modules_configure_through_soc_hal_contract(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    modules = (
        "nsx-core",
        "nsx-interrupt",
        "nsx-timer",
        "nsx-perf",
        "nsx-audio",
        "nsx-gpio",
        "nsx-i2c",
        "nsx-spi",
        "nsx-uart",
        "nsx-psram",
    )
    result = configure_contract_project(
        repo_root,
        tmp_path,
        "apollo3p_evb",
        "gcc",
        modules,
        provider="ambiqsuite-r3",
        ambiqsuite_version="R3.2.0",
        provider_module="nsx-ambiqsuite-r3",
        hal_module="nsx-ambiq-hal-r3",
        bsp_module="nsx-ambiq-bsp-r3",
    )
    assert result.returncode == 0, result.stdout


def test_r4_usb_substrate_configures_as_optional_sdk_module(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    result = configure_contract_project(
        repo_root,
        tmp_path,
        "apollo4p_evb",
        "gcc",
        ("nsx-ambiq-usb-r4",),
        provider="ambiqsuite-r4",
        ambiqsuite_version="R4.5.0",
        provider_module="nsx-ambiqsuite-r4",
        hal_module="nsx-ambiq-hal-r4",
        bsp_module="nsx-ambiq-bsp-r4",
    )
    assert result.returncode == 0, result.stdout


def test_r4_usb_module_configures_inside_sdk_repo(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    result = configure_contract_project(
        repo_root,
        tmp_path,
        "apollo4p_evb",
        "gcc",
        (
            "nsx-core",
            "nsx-interrupt",
            "nsx-timer",
            "nsx-ambiq-usb-r4",
            "nsx-usb",
        ),
        provider="ambiqsuite-r4",
        ambiqsuite_version="R4.5.0",
        provider_module="nsx-ambiqsuite-r4",
        hal_module="nsx-ambiq-hal-r4",
        bsp_module="nsx-ambiq-bsp-r4",
    )
    assert result.returncode == 0, result.stdout


def test_runtime_modules_do_not_link_board_flags_directly(repo_root: Path) -> None:
    runtime_modules = (
        "nsx-core",
        "nsx-timer",
        "nsx-perf",
        "nsx-i2c",
        "nsx-spi",
        "nsx-power",
        "nsx-uart",
    )
    offenders = []
    for module in runtime_modules:
        cmake = read(repo_root, f"modules/{module}/CMakeLists.txt")
        if "NSX_BOARD_FLAGS_TARGET" in cmake or "nsx::board_flags" in cmake:
            offenders.append(module)
    assert offenders == []


def test_module_sources_are_explicit_in_cmake(repo_root: Path) -> None:
    offenders = []
    for path in sorted((repo_root / "modules").glob("*/CMakeLists.txt")):
        cmake = path.read_text(encoding="utf-8")
        if "file(GLOB" in cmake or "CONFIGURE_DEPENDS" in cmake:
            offenders.append(path.relative_to(repo_root).as_posix())
    assert offenders == []


def test_generic_public_headers_do_not_include_bsp(repo_root: Path) -> None:
    generic_modules = (
        "nsx-i2c",
        "nsx-perf",
        "nsx-power",
        "nsx-spi",
        "nsx-uart",
        "nsx-timer",
    )
    offenders = []
    for module in generic_modules:
        for header_dir_name in ("includes-api", "include"):
            header_dir = repo_root / "modules" / module / header_dir_name
            if not header_dir.is_dir():
                continue
            for path in sorted(header_dir.rglob("*.h")):
                if '#include "am_bsp.h"' in path.read_text(encoding="utf-8"):
                    offenders.append(path.relative_to(repo_root).as_posix())
    assert offenders == []


def test_public_headers_do_not_include_bsp(repo_root: Path) -> None:
    offenders = []
    for module_dir in (repo_root / "modules").glob("*"):
        if module_dir.name == "nsx-ambiqsuite-r5":
            continue
        for header_dir_name in ("includes-api", "include"):
            header_dir = module_dir / header_dir_name
            if not header_dir.is_dir():
                continue
            for path in sorted(header_dir.rglob("*.h")):
                relative = path.relative_to(repo_root).as_posix()
                if '#include "am_bsp.h"' in path.read_text(encoding="utf-8"):
                    offenders.append(relative)
    assert offenders == []


def test_power_public_api_uses_nsx_prefix(repo_root: Path) -> None:
    header = read(repo_root, "modules/nsx-power/includes-api/nsx_power.h")
    source_text = "\n".join(
        path.read_text(encoding="utf-8")
        for path in sorted((repo_root / "modules" / "nsx-power").rglob("*.c"))
    )

    for forbidden in (
        "ns_power_config_t",
        "ns_power_config(",
        "ns_deep_sleep(",
        "ns_set_performance_mode(",
        "nsx_perf_mode_e",
        "NSX_PERF_LOW",
        "NSX_PERF_HIGH",
        "NSX_PERF_MAX",
    ):
        assert forbidden not in header
        assert forbidden not in source_text

    for expected in (
        "nsx_power_config_t",
        "nsx_power_configure(",
        "nsx_power_deep_sleep(",
        "nsx_power_set_performance_mode(",
        "nsx_power_perf_mode_t",
        "NSX_POWER_PERF_LOW",
        "NSX_POWER_PERF_HIGH",
        "NSX_POWER_PERF_MAX",
    ):
        assert expected in header


# RTOS port-selection facts published by each SoC descriptor. These are facts
# only: the core SDK does not link a kernel or claim a tick source. The optional
# nsx-freertos module consumes them to resolve an AmbiqSuite or generic CMSIS
# FreeRTOS port. See cmake/README.md "RTOS Port Selection".
EXPECTED_RTOS_PORT_FACTS = {
    "apollo2": ("AMapollo2", "ARM_CM4F"),
    "apollo3": ("AMapollo", "ARM_CM4F"),
    "apollo3p": ("AMapollo", "ARM_CM4F"),
    "apollo4l": ("AMapollo4", "ARM_CM4F"),
    "apollo4p": ("AMapollo4", "ARM_CM4F"),
    "apollo5b": ("AMapollo5", "ARM_CM55_NTZ"),
    "apollo510": ("AMapollo5", "ARM_CM55_NTZ"),
    "apollo510b": ("AMapollo5", "ARM_CM55_NTZ"),
    "apollo510L": ("AMapollo5", "ARM_CM55_NTZ"),
    "apollo330P": ("AMapollo5", "ARM_CM55_NTZ"),
}


def test_soc_descriptors_publish_rtos_port_facts(repo_root: Path) -> None:
    soc_dir = repo_root / "cmake" / "socs"
    descriptors = sorted(path.stem for path in soc_dir.glob("*.cmake"))
    assert descriptors == sorted(EXPECTED_RTOS_PORT_FACTS), (
        "Every SoC descriptor must declare RTOS port facts; update "
        "EXPECTED_RTOS_PORT_FACTS when adding or removing a SoC."
    )

    for skew, (family, generic) in EXPECTED_RTOS_PORT_FACTS.items():
        text = soc_facts_text(repo_root, skew)
        assert f'set(NSX_SOC_RTOS_PORT_FAMILY "{family}")' in text, skew
        assert f'set(NSX_SOC_RTOS_PORT_GENERIC "{generic}")' in text, skew


def test_soc_rtos_generic_port_matches_core_class(repo_root: Path) -> None:
    core_to_generic = {
        "cortex-m4": "ARM_CM4F",
        "cortex-m55": "ARM_CM55_NTZ",
    }
    for skew in EXPECTED_RTOS_PORT_FACTS:
        text = soc_facts_text(repo_root, skew)
        core_match = re.search(r'set\(NSX_SOC_CORE "([^"]+)"\)', text)
        generic_match = re.search(r'set\(NSX_SOC_RTOS_PORT_GENERIC "([^"]+)"\)', text)
        assert core_match is not None, skew
        assert generic_match is not None, skew
        assert generic_match.group(1) == core_to_generic[core_match.group(1)], skew


def test_core_sdk_does_not_link_or_define_freertos(repo_root: Path) -> None:
    # The RTOS port facts must remain facts only: no kernel link, no RTOS
    # compile definitions injected by the SoC descriptors themselves.
    for skew in EXPECTED_RTOS_PORT_FACTS:
        text = read(repo_root, f"cmake/socs/{skew}.cmake") + soc_facts_text(repo_root, skew)
        assert "FreeRTOS" not in text, skew
        assert "vTaskStartScheduler" not in text, skew


def test_freertos_vendored_kernel_is_pinned(repo_root: Path) -> None:
    root = repo_root / "modules" / "nsx-freertos" / "sdk" / "third_party" / "FreeRTOS-Kernel"
    version = (root / "NSX_VENDORED_VERSION.txt").read_text(encoding="utf-8").strip()
    assert version == "V11.1.0"
    for rel in (
        "tasks.c",
        "queue.c",
        "list.c",
        "timers.c",
        "include/FreeRTOS.h",
        "portable/MemMang/heap_4.c",
        "portable/GCC/ARM_CM4F/port.c",
        "portable/GCC/ARM_CM4F/portmacro.h",
        "portable/GCC/ARM_CM55_NTZ/non_secure/port.c",
        "portable/GCC/ARM_CM55_NTZ/non_secure/portasm.c",
    ):
        assert (root / rel).is_file(), rel
    # Upstream MIT license retained alongside the vendored sources.
    assert (root / "LICENSE.md").is_file()
    # The manifest records the same pin.
    manifest = read(repo_root, "modules/nsx-freertos/nsx-module.yaml")
    assert "tag: V11.1.0" in manifest


def test_freertos_module_configures_inside_sdk_repo(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    board = "apollo510_evb"
    source_dir = tmp_path / "freertos" / board
    config_dir = source_dir / "config"
    build_dir = source_dir / "build"
    config_dir.mkdir(parents=True)

    # The application supplies FreeRTOSConfig.h; seed it from the shipped template.
    template = repo_root / "modules" / "nsx-freertos" / "templates" / "FreeRTOSConfig.h.template"
    (config_dir / "FreeRTOSConfig.h").write_text(template.read_text(encoding="utf-8"), encoding="utf-8")

    (source_dir / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "if(POLICY CMP0123)",
                "    cmake_policy(SET CMP0123 NEW)",
                "endif()",
                "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)",
                "project(nsx_freertos_contract C)",
                "include(GNUInstallDirs)",
                f'set(NSX_ROOT "{repo_root.as_posix()}")',
                f'set(NSX_CMAKE_DIR "{(repo_root / "cmake").as_posix()}")',
                'set(NSX_SDK_PROVIDER "ambiqsuite-r5")',
                'set(NSX_TOOLCHAIN_FAMILY "gcc")',
                'set(NSX_AMBIQSUITE_VERSION "R5.2.0")',
                f'set(NSX_AMBIQSUITE_ROOT "{(repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk").as_posix()}")',
                f'include("{(repo_root / "boards" / board / "board.cmake").as_posix()}")',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-cmsis-core").as_posix()}" nsx-cmsis-core)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-hal-r5").as_posix()}" nsx-ambiq-hal-r5)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-bsp-r5").as_posix()}" nsx-ambiq-bsp-r5)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-soc-hal").as_posix()}" nsx-soc-hal)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-core").as_posix()}" nsx-core)',
                # Application-owned FreeRTOSConfig.h provider contract.
                "add_library(app_freertos_config INTERFACE)",
                f'target_include_directories(app_freertos_config INTERFACE "{config_dir.as_posix()}")',
                "add_library(nsx::freertos_config ALIAS app_freertos_config)",
                f'add_subdirectory("{(repo_root / "modules" / "nsx-freertos").as_posix()}" nsx-freertos)',
                "",
            ]
        ),
        encoding="utf-8",
    )

    result = subprocess.run(
        ["cmake", "-S", str(source_dir), "-B", str(build_dir)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert result.returncode == 0, result.stdout


def test_freertos_module_configures_for_cm4f_soc_hal_contract(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    board = "apollo4p_evb"
    source_dir = tmp_path / "freertos_cm4f" / board
    config_dir = source_dir / "config"
    build_dir = source_dir / "build"
    config_dir.mkdir(parents=True)

    template = repo_root / "modules" / "nsx-freertos" / "templates" / "FreeRTOSConfig.h.template"
    (config_dir / "FreeRTOSConfig.h").write_text(template.read_text(encoding="utf-8"), encoding="utf-8")

    (source_dir / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "if(POLICY CMP0123)",
                "    cmake_policy(SET CMP0123 NEW)",
                "endif()",
                "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)",
                "project(nsx_freertos_cm4f_contract C)",
                "include(GNUInstallDirs)",
                f'set(NSX_ROOT "{repo_root.as_posix()}")',
                f'set(NSX_CMAKE_DIR "{(repo_root / "cmake").as_posix()}")',
                'set(NSX_SDK_PROVIDER "ambiqsuite-r4")',
                'set(NSX_TOOLCHAIN_FAMILY "gcc")',
                'set(NSX_AMBIQSUITE_VERSION "R4.5.0")',
                f'set(NSX_AMBIQSUITE_ROOT "{(repo_root / "modules" / "nsx-ambiqsuite-r4" / "sdk").as_posix()}")',
                f'include("{(repo_root / "boards" / board / "board.cmake").as_posix()}")',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-cmsis-core").as_posix()}" nsx-cmsis-core)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-hal-r4").as_posix()}" nsx-ambiq-hal-r4)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-bsp-r4").as_posix()}" nsx-ambiq-bsp-r4)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-soc-hal").as_posix()}" nsx-soc-hal)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-core").as_posix()}" nsx-core)',
                "add_library(app_freertos_config INTERFACE)",
                f'target_include_directories(app_freertos_config INTERFACE "{config_dir.as_posix()}")',
                "add_library(nsx::freertos_config ALIAS app_freertos_config)",
                f'add_subdirectory("{(repo_root / "modules" / "nsx-freertos").as_posix()}" nsx-freertos)',
                "",
            ]
        ),
        encoding="utf-8",
    )

    result = subprocess.run(
        ["cmake", "-S", str(source_dir), "-B", str(build_dir)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert result.returncode == 0, result.stdout



def test_freertos_module_requires_config_provider(repo_root: Path, tmp_path: Path) -> None:
    if shutil.which("cmake") is None:
        raise AssertionError("cmake is required for NSX CMake contract tests")

    board = "apollo510_evb"
    source_dir = tmp_path / "freertos_noconfig" / board
    build_dir = source_dir / "build"
    source_dir.mkdir(parents=True)

    # Same graph but WITHOUT defining nsx::freertos_config; the module must reject it.
    (source_dir / "CMakeLists.txt").write_text(
        "\n".join(
            [
                "cmake_minimum_required(VERSION 3.20)",
                "set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)",
                "project(nsx_freertos_noconfig C)",
                "include(GNUInstallDirs)",
                f'set(NSX_ROOT "{repo_root.as_posix()}")',
                f'set(NSX_CMAKE_DIR "{(repo_root / "cmake").as_posix()}")',
                'set(NSX_SDK_PROVIDER "ambiqsuite-r5")',
                'set(NSX_TOOLCHAIN_FAMILY "gcc")',
                'set(NSX_AMBIQSUITE_VERSION "R5.2.0")',
                f'set(NSX_AMBIQSUITE_ROOT "{(repo_root / "modules" / "nsx-ambiqsuite-r5" / "sdk").as_posix()}")',
                f'include("{(repo_root / "boards" / board / "board.cmake").as_posix()}")',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-cmsis-core").as_posix()}" nsx-cmsis-core)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-hal-r5").as_posix()}" nsx-ambiq-hal-r5)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-ambiq-bsp-r5").as_posix()}" nsx-ambiq-bsp-r5)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-soc-hal").as_posix()}" nsx-soc-hal)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-core").as_posix()}" nsx-core)',
                f'add_subdirectory("{(repo_root / "modules" / "nsx-freertos").as_posix()}" nsx-freertos)',
                "",
            ]
        ),
        encoding="utf-8",
    )

    result = subprocess.run(
        ["cmake", "-S", str(source_dir), "-B", str(build_dir)],
        check=False,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    assert result.returncode != 0
    assert "nsx::freertos_config" in result.stdout
