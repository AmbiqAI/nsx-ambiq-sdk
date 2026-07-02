# nsx-cordio: optional Cordio/WSF Bluetooth LE host stack.
#
# Vendors the AmbiqSuite third_party Cordio stack (ble-host + ble-profiles + WSF)
# pinned to the same SDK drop as the provider (stable-2026.06.18), and builds it
# from source for the active SoC's BLE transport. This is the low-level BLE host
# stack; the app-facing convenience API lives in the separate nsx-ble module.
#
# Scope: Apollo4P Blue (Cooper external controller over IOM/SPI),
# Apollo3/Apollo3P Blue (integrated BLE), and Apollo510B (EM9305 over SPI).
# 510L/330P IPC radio remains a follow-up phase gated the same way.
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
# Apollo4P Blue uses the external Cooper controller over IOM/SPI. Apollo3 and
# Apollo3P Blue use the integrated Apollo3 BLE controller through the HAL BLEIF.
# Apollo510B uses the EM9305 controller over SPI. Other families are known but
# intentionally not wired yet; fail loudly rather than silently mis-build.
if(NSX_SOC_FAMILY STREQUAL "apollo4p")
    set(NSX_CORDIO_TRANSPORT "cooper")
elseif(NSX_SOC_FAMILY STREQUAL "apollo3" OR NSX_SOC_FAMILY STREQUAL "apollo3p")
    set(NSX_CORDIO_TRANSPORT "apollo3")
elseif(NSX_SOC_FAMILY STREQUAL "apollo510b")
    set(NSX_CORDIO_TRANSPORT "em9305")
else()
    message(FATAL_ERROR
        "nsx-cordio supports NSX_SOC_FAMILY=apollo4p (Cooper), "
        "apollo3/apollo3p (integrated BLE), and apollo510b (EM9305). "
        "Requested '${NSX_SOC_FAMILY}'. 510L transports are follow-ups.")
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
# directory. Keep the vendored equivalent explicit so SDK source changes are
# intentional and reviewable.
function(nsx_cordio_sources out_var base_dir)
    set(sources "")
    foreach(source IN LISTS ARGN)
        list(APPEND sources "${base_dir}/${source}")
    endforeach()
    set(${out_var} ${sources} PARENT_SCOPE)
endfunction()

nsx_cordio_sources(NSX_CORDIO_WSF_UTIL_SRC "${NSX_CORDIO_WSF}/sources/util"
    bda.c
    bstream.c
    calc128.c
    crc32.c
    print.c
    terminal.c
    wstr.c
)
nsx_cordio_sources(NSX_CORDIO_WSF_FREERTOS_SRC "${NSX_CORDIO_WSF}/sources/port/freertos"
    wsf_assert.c
    wsf_buf.c
    wsf_efs.c
    wsf_math.c
    wsf_msg.c
    wsf_os.c
    wsf_queue.c
    wsf_timer.c
    wsf_trace.c
)
set(NSX_CORDIO_WSF_SRC
    ${NSX_CORDIO_WSF_UTIL_SRC}
    ${NSX_CORDIO_WSF_FREERTOS_SRC}
)

nsx_cordio_sources(NSX_CORDIO_ATT_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/att"
    att_eatt.c
    att_main.c
    att_uuid.c
    attc_disc.c
    attc_eatt.c
    attc_main.c
    attc_proc.c
    attc_read.c
    attc_sign.c
    attc_write.c
    atts_ccc.c
    atts_csf.c
    atts_dyn.c
    atts_eatt.c
    atts_ind.c
    atts_main.c
    atts_proc.c
    atts_read.c
    atts_sign.c
    atts_write.c
)
nsx_cordio_sources(NSX_CORDIO_CFG_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/cfg"
    cfg_stack.c
)
nsx_cordio_sources(NSX_CORDIO_DM_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/dm"
    dm_adv.c
    dm_adv_ae.c
    dm_adv_leg.c
    dm_bis_master.c
    dm_bis_slave.c
    dm_cis.c
    dm_cis_master.c
    dm_cis_slave.c
    dm_cis_sm.c
    dm_conn.c
    dm_conn_cte.c
    dm_conn_master.c
    dm_conn_master_ae.c
    dm_conn_master_leg.c
    dm_conn_slave.c
    dm_conn_slave_ae.c
    dm_conn_slave_leg.c
    dm_conn_sm.c
    dm_dev.c
    dm_dev_priv.c
    dm_ead.c
    dm_iso.c
    dm_main.c
    dm_past.c
    dm_phy.c
    dm_priv.c
    dm_scan.c
    dm_scan_ae.c
    dm_scan_leg.c
    dm_sec.c
    dm_sec_lesc.c
    dm_sec_master.c
    dm_sec_slave.c
    dm_sync_ae.c
)
nsx_cordio_sources(NSX_CORDIO_HCI_STACK_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/hci"
    hci_main.c
)
nsx_cordio_sources(NSX_CORDIO_L2C_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/l2c"
    l2c_coc.c
    l2c_main.c
    l2c_master.c
    l2c_slave.c
)
nsx_cordio_sources(NSX_CORDIO_SMP_SRC "${NSX_CORDIO_BLE_HOST}/sources/stack/smp"
    smp_act.c
    smp_db.c
    smp_main.c
    smp_non.c
    smp_sc_act.c
    smp_sc_main.c
    smpi_act.c
    smpi_sc_act.c
    smpi_sc_sm.c
    smpi_sm.c
    smpr_act.c
    smpr_sc_act.c
    smpr_sc_sm.c
    smpr_sm.c
)
nsx_cordio_sources(NSX_CORDIO_SEC_COMMON_SRC "${NSX_CORDIO_BLE_HOST}/sources/sec/common"
    sec_aes.c
    sec_aes_rev.c
    sec_ccm_hci.c
    sec_cmac_hci.c
    sec_ecc_hci.c
    sec_main.c
)
nsx_cordio_sources(NSX_CORDIO_SEC_UECC_SRC "${NSX_CORDIO_BLE_HOST}/sources/sec/uecc"
    sec_ecc.c
)
nsx_cordio_sources(NSX_CORDIO_AMBIQ_HCI_SRC "${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq"
    hci_cmd.c
    hci_cmd_ae.c
    hci_cmd_bis.c
    hci_cmd_cis.c
    hci_cmd_cte.c
    hci_cmd_iso.c
    hci_cmd_past.c
    hci_cmd_phy.c
    hci_core.c
    hci_core_ps.c
    hci_evt.c
    hci_tr.c
    hci_vs_ae.c
)
set(NSX_CORDIO_HOST_SRC
    ${NSX_CORDIO_ATT_SRC}
    ${NSX_CORDIO_CFG_SRC}
    ${NSX_CORDIO_DM_SRC}
    ${NSX_CORDIO_HCI_STACK_SRC}
    ${NSX_CORDIO_L2C_SRC}
    ${NSX_CORDIO_SMP_SRC}
    ${NSX_CORDIO_SEC_COMMON_SRC}
    ${NSX_CORDIO_SEC_UECC_SRC}
    ${NSX_CORDIO_AMBIQ_HCI_SRC}
)

nsx_cordio_sources(NSX_CORDIO_APP_SRC "${NSX_CORDIO_BLE_PROFILES}/sources/apps/app"
    app_disc.c
    app_main.c
    app_master.c
    app_master_ae.c
    app_master_leg.c
    app_server.c
    app_slave.c
    app_slave_ae.c
    app_slave_leg.c
    app_terminal.c
)
nsx_cordio_sources(NSX_CORDIO_APP_COMMON_SRC "${NSX_CORDIO_BLE_PROFILES}/sources/apps/app/common"
    app_db.c
    app_hw.c
    app_ui.c
    ui_console.c
    ui_lcd.c
    ui_main.c
    ui_platform.c
    ui_timer.c
)
nsx_cordio_sources(NSX_CORDIO_GATT_PROFILE_SRC "${NSX_CORDIO_BLE_PROFILES}/sources/profiles/gatt"
    gatt_main.c
)
nsx_cordio_sources(NSX_CORDIO_SERVICES_SRC "${NSX_CORDIO_BLE_PROFILES}/sources/services"
    svc_alert.c
    svc_batt.c
    svc_bps.c
    svc_core.c
    svc_cps.c
    svc_cscs.c
    svc_cte.c
    svc_dis.c
    svc_gls.c
    svc_gyro.c
    svc_hid.c
    svc_hrs.c
    svc_hts.c
    svc_ipss.c
    svc_plxs.c
    svc_px.c
    svc_rscs.c
    svc_scpss.c
    svc_temp.c
    svc_time.c
    svc_uricfg.c
    svc_wdxs.c
    svc_wp.c
    svc_wss.c
)
set(NSX_CORDIO_PROFILES_SRC
    ${NSX_CORDIO_APP_SRC}
    ${NSX_CORDIO_APP_COMMON_SRC}
    ${NSX_CORDIO_GATT_PROFILE_SRC}
    ${NSX_CORDIO_SERVICES_SRC}
)

nsx_cordio_sources(NSX_CORDIO_UECC_SRC "${NSX_CORDIO_UECC_ROOT}"
    uECC.c
    uECC_ll.c
)

# Transport-specific HCI driver + Ambiq device driver.
if(NSX_CORDIO_TRANSPORT STREQUAL "cooper")
    nsx_cordio_sources(NSX_CORDIO_TRANSPORT_SRC "${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/cooper"
        hci_dbg_trc.c
        hci_drv_cooper.c
        hci_vs_cooper.c
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
elseif(NSX_CORDIO_TRANSPORT STREQUAL "apollo3")
    nsx_cordio_sources(NSX_CORDIO_TRANSPORT_SRC "${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/apollo3"
        hci_drv_apollo3.c
        hci_vs_apollo3.c
    )
    # The Apollo3 HCI driver calls the generic Ambiq BLE utility helpers. The
    # provider payload stages the header but not this BLE-specific source, so we
    # vendor it here with the transport just as we do for Cooper utilities.
    set(NSX_CORDIO_DEVICE_SRC
        ${CMAKE_CURRENT_LIST_DIR}/sdk/ambiq/am_util_ble.c)
    nsx_assert_file_exists("${NSX_CORDIO_DEVICE_SRC}")
    set(NSX_CORDIO_TRANSPORT_INCLUDE
        ${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/apollo3
        ${CMAKE_CURRENT_LIST_DIR}/sdk/ambiq)
elseif(NSX_CORDIO_TRANSPORT STREQUAL "em9305")
    nsx_cordio_sources(NSX_CORDIO_TRANSPORT_SRC "${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/em9305"
        hci_drv_em9305.c
        hci_vs_em9305.c
    )
    set(NSX_CORDIO_DEVICE_SRC "${NSX_AMBIQSUITE_ROOT}/devices/am_devices_em9305.c")
    nsx_assert_file_exists("${NSX_CORDIO_DEVICE_SRC}")
    set(NSX_CORDIO_TRANSPORT_INCLUDE
        ${NSX_CORDIO_BLE_HOST}/sources/hci/ambiq/em9305)
endif()

foreach(NSX_CORDIO_TRANSPORT_INCLUDE_DIR IN LISTS NSX_CORDIO_TRANSPORT_INCLUDE)
    list(APPEND NSX_CORDIO_TRANSPORT_BUILD_INCLUDE
        $<BUILD_INTERFACE:${NSX_CORDIO_TRANSPORT_INCLUDE_DIR}>)
endforeach()

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
    ${NSX_CORDIO_TRANSPORT_BUILD_INCLUDE}
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/include/app>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/apps>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/apps/app>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles/include>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/profiles/gatt>
    $<BUILD_INTERFACE:${NSX_CORDIO_BLE_PROFILES}/sources/services>
    $<BUILD_INTERFACE:${NSX_CORDIO_UECC_ROOT}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/wsf/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/wsf/sources>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/wsf/sources/util>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/wsf/sources/port/freertos>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/cfg>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/hci>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/dm>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/l2c>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/att>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/stack/smp>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/sec/common>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/hci/ambiq>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/hci/ambiq/cooper>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/hci/ambiq/apollo3>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-host/sources/hci/ambiq/em9305>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/ambiq>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/include/app>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/apps>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/apps/app>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/profiles/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/profiles>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/profiles/gatt>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/cordio/ble-profiles/sources/services>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party/uecc>
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
target_compile_definitions(nsx_cordio PRIVATE SEC_ECC_CFG=SEC_ECC_CFG_UECC)

# The vendored stack has many upstream-style warnings; keep them non-fatal so a
# strict app (-Werror) does not fail on third-party code we do not own.
target_compile_options(nsx_cordio PRIVATE -w)

install(TARGETS
    nsx_cordio
    EXPORT nsxTargets
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/cordio
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/uecc
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/third_party
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/sdk/ambiq/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/cordio/sdk/ambiq
    FILES_MATCHING PATTERN "*.h"
)
