# nsx-ble: optional app-facing BLE convenience API (Phase 1 MVP).
#
# Ports the legacy neuralSPOT ns-ble wrapper onto NSX. It provides the simple
# "define a service + characteristics with read/write/notify callbacks" API on
# top of the Cordio host stack (nsx::cordio), and owns the radio IRQ glue and
# the WSF dispatcher plumbing for the active SoC's BLE transport.
#
# Phase 1 scope: Apollo4P Blue (Cooper transport), GCC + ATFE +
# armclang/ACFE build-validated. Matches nsx-cordio.

if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "nsx-ble requires NSX_SOC_FAMILY.")
endif()

nsx_require_target(nsx::core nsx-ble)
if(NOT TARGET nsx::cordio)
    message(FATAL_ERROR
        "nsx-ble requires nsx::cordio (the Cordio BLE host stack). "
        "Add nsx-cordio before nsx-ble.")
endif()
if(NOT TARGET nsx::freertos)
    message(FATAL_ERROR
        "nsx-ble requires nsx::freertos (the WSF dispatcher runs as a FreeRTOS task). "
        "Add nsx-freertos before nsx-ble.")
endif()

# Phase 1 support gate: same transport coverage as nsx-cordio.
if(NOT NSX_SOC_FAMILY STREQUAL "apollo4p")
    message(FATAL_ERROR
        "nsx-ble Phase 1 supports only NSX_SOC_FAMILY=apollo4p (Cooper). "
        "Requested '${NSX_SOC_FAMILY}'.")
endif()

add_library(nsx_ble STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/ns_ble.c
)
set_target_properties(nsx_ble PROPERTIES EXPORT_NAME ble)
add_library(nsx::ble ALIAS nsx_ble)

target_include_directories(nsx_ble PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-ble/includes-api>
)

# nsx::cordio transitively brings ambiq_hal/bsp/board_flags/freertos (PUBLIC).
# nsx::core provides nsx_core.h (logging, interrupt master, status codes).
target_link_libraries(nsx_ble PUBLIC
    nsx::cordio
    nsx::core
    nsx::freertos
)

target_compile_definitions(nsx_ble PUBLIC NSX_BLE)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-ble
)
install(TARGETS
    nsx_ble
    EXPORT nsxTargets
)
