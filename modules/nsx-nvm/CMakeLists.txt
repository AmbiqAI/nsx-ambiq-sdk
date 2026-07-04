nsx_require_target(nsx::soc_hal nsx-nvm)
nsx_require_target(nsx::board_flags nsx-nvm)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-nvm is added.")
endif()
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME)
    message(FATAL_ERROR "NSX_AMBIQ_BOARD_NAME must be defined before nsx-nvm is added.")
endif()

nsx_select_soc_arch_dir(NSX_NVM_ARCH_DIR
    APOLLO5 apollo5
)

if(NOT NSX_NVM_ARCH_DIR STREQUAL "apollo5")
    message(FATAL_ERROR "nsx-nvm currently supports only Apollo5-family boards with an IS25WX064 flash device. Got NSX_SOC_FAMILY='${NSX_SOC_FAMILY}'.")
endif()

# MSPI instance varies per board layout; each board wires the IS25WX064 to a
# different MSPI number in its BSP (see AM_BSP_MSPI_FLASH_MODULE).
if(NOT DEFINED NSX_NVM_MSPI_MODULE)
    if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510_evb"
       OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510b_evb")
        set(NSX_NVM_MSPI_MODULE 1)
    elseif(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510dL_evb")
        set(NSX_NVM_MSPI_MODULE 2)
    else()
        message(FATAL_ERROR "nsx-nvm does not yet have an MSPI mapping for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
    endif()
endif()

if(NOT DEFINED NSX_NVM_DEVICE_SIZE_BYTES)
    set(NSX_NVM_DEVICE_SIZE_BYTES 8388608u) # IS25WX064 is 8MB
endif()

# am_devices_mspi_is25wx064.c is staged in the provider payload but not
# compiled by any core module; nsx-nvm depends on it directly, mirroring how
# nsx-cordio references transport-specific Ambiq device drivers (e.g.
# am_devices_em9305.c) straight from NSX_AMBIQSUITE_ROOT rather than vendoring
# a local copy.
set(NSX_NVM_DEVICE_SRC "${NSX_AMBIQSUITE_ROOT}/devices/am_devices_mspi_is25wx064.c")
nsx_assert_file_exists("${NSX_NVM_DEVICE_SRC}")

add_library(nsx_nvm STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_nvm.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_NVM_ARCH_DIR}/nsx_nvm.c
    ${NSX_NVM_DEVICE_SRC}
)
set_target_properties(nsx_nvm PROPERTIES EXPORT_NAME nvm)
add_library(nsx::nvm ALIAS nsx_nvm)

target_link_libraries(nsx_nvm PUBLIC
    nsx::board_flags
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_include_directories(nsx_nvm PRIVATE
    ${NSX_AMBIQSUITE_ROOT}/devices
)

target_compile_definitions(nsx_nvm PRIVATE
    NSX_NVM_MSPI_MODULE=${NSX_NVM_MSPI_MODULE}
    NSX_NVM_DEVICE_SIZE_BYTES=${NSX_NVM_DEVICE_SIZE_BYTES}
)

target_include_directories(nsx_nvm PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-nvm/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-nvm
)

install(TARGETS
    nsx_nvm
    EXPORT nsxTargets
)
