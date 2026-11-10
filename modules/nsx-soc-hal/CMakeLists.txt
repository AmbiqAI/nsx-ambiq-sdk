nsx_require_target(nsx::board_flags nsx-soc-hal)

add_library(nsx_soc_hal INTERFACE)
set_target_properties(nsx_soc_hal PROPERTIES EXPORT_NAME ${NSX_SOC_TARGET_EXPORT_NAME})
add_library(nsx::soc_hal ALIAS nsx_soc_hal)
if(DEFINED NSX_SOC_TARGET_EXPORT_NAME AND NOT TARGET nsx::${NSX_SOC_TARGET_EXPORT_NAME})
    add_library(nsx::${NSX_SOC_TARGET_EXPORT_NAME} ALIAS nsx_soc_hal)
endif()
add_library(nsx::ambiqsuite_core ALIAS nsx_soc_hal)

target_link_libraries(nsx_soc_hal INTERFACE nsx::board_flags)
if(NOT TARGET nsx_ambiq_hal)
    message(FATAL_ERROR "nsx-soc-hal requires a release-specific ambiq HAL module to be added first.")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/nsx_bsp_contract.cmake")
nsx_validate_bsp(NSX_SOC_BSP_TARGET)
target_link_libraries(nsx_soc_hal INTERFACE nsx::bsp nsx_ambiq_hal)

install(TARGETS
    nsx_soc_hal
    EXPORT nsxTargets
)
