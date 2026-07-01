# nsx-cordio: optional Cordio/WSF Bluetooth LE host stack (Phase 1 MVP).
#
# Vendors the AmbiqSuite third_party Cordio stack (ble-host + ble-profiles + WSF)
# pinned to the same SDK drop as the provider (stable-2026.06.18), and builds it
# from source for the active SoC's BLE transport. This is the low-level BLE host
# stack; the app-facing convenience API lives in the separate nsx-ble module.
#
# Phase 1 scope: Apollo4P Blue (Cooper external controller over IOM/SPI),
# GCC + ATFE + armclang/ACFE build-validated.
# EM9305 (Apollo510B), the integrated Apollo3 controller, and the 510L/330P IPC
# radio are follow-up phases gated the same way.
#
# The WSF port is the FreeRTOS port; nsx-cordio therefore requires nsx::freertos
# (and, transitively, the application-owned nsx::freertos_config).

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "nsx-cordio requires NSX_AMBIQSUITE_ROOT (the SDK provider payload root).")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "nsx-cordio requires NSX_SOC_FAMILY to select the BLE transport.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR "nsx-cordio requires NSX_SDK_PROVIDER=ambiqsuite.")
endif()

nsx_require_target(nsx::ambiq_hal nsx-cordio)
nsx_require_target(nsx::ambiq_bsp nsx-cordio)
nsx_require_target(nsx::board_flags nsx-cordio)
if(NOT TARGET nsx::freertos)
    message(FATAL_ERROR
        "nsx-cordio requires nsx::freertos (the WSF port is the FreeRTOS port). "
        "Add nsx-freertos before nsx-cordio.")
endif()

# ---------------------------------------------------------------------------
# BLE transport selection (SoC-family gated)
# ---------------------------------------------------------------------------
# Phase 1 supports only the Apollo4P Cooper transport. Other families are known
# but intentionally not wired yet; fail loudly rather than silently mis-build.
if(NSX_SOC_FAMILY STREQUAL "apollo4p")
    set(NSX_CORDIO_TRANSPORT "cooper")
else()
    message(FATAL_ERROR
        "nsx-cordio Phase 1 supports only NSX_SOC_FAMILY=apollo4p (Cooper). "
        "Requested '${NSX_SOC_FAMILY}'. EM9305/Apollo3/510L transports are follow-ups.")
endif()

set(NSX_CORDIO_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/cordio")
set(NSX_CORDIO_UECC_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/uecc")
set(NSX_CORDIO_WSF "${NSX_CORDIO_ROOT}/wsf")
set(NSX_CORDIO_BLE_HOST "${NSX_CORDIO_ROOT}/ble-host")
set(NSX_CORDIO_BLE_PROFILES "${NSX_CORDIO_ROOT}/ble-profiles")

nsx_assert_directory_exists("${NSX_CORDIO_WSF}")
nsx_assert_directory_exists("${NSX_CORDIO_BLE_HOST}")
nsx_assert_directory_exists("${NSX_CORDIO_BLE_PROFILES}")
nsx_assert_directory_exists("${NSX_CORDIO_UECC_ROOT}")

# ---------------------------------------------------------------------------
# Source selection
# ---------------------------------------------------------------------------
# The upstream prebuilt cordio.a compiled the full wildcard set of each stack
# directory; we mirror that with GLOB over the vendored tree so behaviour matches
# the historical library. Transport-specific HCI driver sources are added by the
# selected transport only.
file(GLOB NSX_CORDIO_WSF_SRC CONFIGURE_DEPENDS
    ${NSX_CORDIO_WSF}/sources/util/*.c
    ${NSX_CORDIO_WSF}/sources/port/freertos/*.c
)
file(GLOB NSX_CORDIO_HOST_SRC CONFIGURE_DEPENDS
    ${NSX_CORDIO_BLE_HOST}/sources/stack/att/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/stack/cfg/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/stack/dm/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/stack/hci/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/stack/l2c/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/stack/smp/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/sec/common/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/sec/uecc/*.c
    ${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/*.c
)
file(GLOB NSX_CORDIO_PROFILES_SRC CONFIGURE_DEPENDS
    ${NSX_CORDIO_BLE_PROFILES}/sources/apps/app/*.c
    ${NSX_CORDIO_BLE_PROFILES}/sources/apps/app/common/*.c
    ${NSX_CORDIO_BLE_PROFILES}/sources/profiles/gatt/*.c
    ${NSX_CORDIO_BLE_PROFILES}/sources/services/*.c
)
file(GLOB NSX_CORDIO_UECC_SRC CONFIGURE_DEPENDS
    ${NSX_CORDIO_UECC_ROOT}/*.c
)

# Transport-specific HCI driver + Ambiq device driver.
if(NSX_CORDIO_TRANSPORT STREQUAL "cooper")
    file(GLOB NSX_CORDIO_TRANSPORT_SRC CONFIGURE_DEPENDS
        ${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/cooper/*.c
    )
    # am_devices_cooper.c is staged in the provider payload but not compiled by
    # any core module; the Cooper HCI driver depends on it.
    set(NSX_CORDIO_DEVICE_SRC "${NSX_AMBIQSUITE_ROOT}/devices/am_devices_cooper.c")
    nsx_assert_file_exists("${NSX_CORDIO_DEVICE_SRC}")
    # am_util_ble_cooper.c provides am_util_ble_tx_power_set() and the Cooper
    # NVDS/trim/rollback helpers used by HciDrvRadioBoot(). The provider staged
    # only its header (BLE was out of scope), so the BLE-specific source is
    # vendored here alongside the stack. (It is self-contained; the generic
    # am_util_ble.c is Apollo3-only and must NOT be compiled for Cooper.)
    list(APPEND NSX_CORDIO_DEVICE_SRC
        ${CMAKE_CURRENT_LIST_DIR}/sdk/ambiq/am_util_ble_cooper.c)
    set(NSX_CORDIO_TRANSPORT_INCLUDE
        ${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/cooper)
endif()

add_library(nsx_cordio STATIC
    ${NSX_CORDIO_WSF_SRC}
    ${NSX_CORDIO_HOST_SRC}
    ${NSX_CORDIO_PROFILES_SRC}
    ${NSX_CORDIO_UECC_SRC}
    ${NSX_CORDIO_TRANSPORT_SRC}
    ${NSX_CORDIO_DEVICE_SRC}
)
set_target_properties(nsx_cordio PROPERTIES EXPORT_NAME cordio)
add_library(nsx::cordio ALIAS nsx_cordio)

# ---------------------------------------------------------------------------
# Include directories (mirrors the historical cordio module include surface)
# ---------------------------------------------------------------------------
target_include_directories(nsx_cordio PUBLIC
    $<BUILD_INTERFACE:${NSX_CORDIO_WSF}/include>
    $<BUILD_INTERFACE:${NSX_CORDIO_WSF}/sources>
    $<BUILD_INTERFACE:${NSX_CORDIO_WSF}/sources/util>
    $<BUILD_INTERFACE:${NSX_CORDIO_WSF}/sources/port/freertos>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/include>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/cfg>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/hci>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/dm>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/l2c>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/att>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/stack/smp>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/sec/common>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq>
    $<BUILD_INTERFACE:${NSX_CORDIO_TRANSPORT_INCLUDE}>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/include/app>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/apps>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/apps/app>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles/include>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles/gatt>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/services>
    $<BUILD_INTERFACE:${NSX_CORDIO_UECC_ROOT}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio>
)

# ---------------------------------------------------------------------------
# Link deps
# ---------------------------------------------------------------------------
# nsx::ambiq_hal carries the AM_PART_* defines, HAL headers, and the provider
# devices/ + utils/ include dirs (am_devices_cooper.h, am_util_ble_cooper.h).
# nsx::freertos provides FreeRTOS.h for the WSF FreeRTOS port.
target_link_libraries(nsx_cordio PUBLIC
    nsx::board_flags
    nsx::ambiq_hal
    nsx::ambiq_bsp
    nsx::freertos
)

target_compile_definitions(nsx_cordio PUBLIC NSX_CORDIO)

# The vendored stack has many upstream-style warnings; keep them non-fatal so a
# strict app (-Werror) does not fail on third-party code we do not own.
target_compile_options(nsx_cordio PRIVATE -w)

install(TARGETS
    nsx_cordio
    EXPORT nsxTargets
)
