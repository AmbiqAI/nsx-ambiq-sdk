nsx_require_target(nsx::soc_hal nsx-emmc)
nsx_require_target(nsx::board_flags nsx-emmc)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-emmc is added.")
endif()
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME)
    message(FATAL_ERROR "NSX_AMBIQ_BOARD_NAME must be defined before nsx-emmc is added.")
endif()

nsx_select_soc_arch_dir(NSX_EMMC_ARCH_DIR
    APOLLO5 apollo5
)

if(NOT NSX_EMMC_ARCH_DIR STREQUAL "apollo5")
    message(FATAL_ERROR "nsx-emmc currently supports only Apollo5-family boards with a populated eMMC device. Got NSX_SOC_FAMILY='${NSX_SOC_FAMILY}'.")
endif()

if(NOT (NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510_evb"
        OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510b_evb"))
    message(FATAL_ERROR "nsx-emmc does not yet have a confirmed eMMC-populated board mapping for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
endif()

# am_hal_card_*/am_hal_sdhc_* are fully precompiled into libam_hal.a for
# Apollo5 -- unlike nsx-nvm/nsx-psram, no vendored or directly referenced
# AmbiqSuite device-driver source file is required here.
add_library(nsx_emmc STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_emmc.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_EMMC_ARCH_DIR}/nsx_emmc.c
)
set_target_properties(nsx_emmc PROPERTIES EXPORT_NAME emmc)
add_library(nsx::emmc ALIAS nsx_emmc)

target_link_libraries(nsx_emmc PUBLIC
    nsx::board_flags
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_include_directories(nsx_emmc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-emmc/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-emmc
)

install(TARGETS
    nsx_emmc
    EXPORT nsxTargets
)
