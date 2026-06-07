option(NSX_AMBIQSUITE_USE_PREBUILT "Use prebuilt AmbiqSuite static libraries" ON)
option(NSX_AMBIQSUITE_USE_SOURCE_FALLBACK "Use local source fallback if prebuilt is disabled" ON)

if(NOT TARGET nsx::board_flags)
    message(FATAL_ERROR "Board must define nsx::board_flags before nsx-soc-hal is added.")
endif()

add_library(nsx_soc_hal INTERFACE)
set_target_properties(nsx_soc_hal PROPERTIES EXPORT_NAME ${NSX_SOC_TARGET_EXPORT_NAME})
add_library(nsx::soc_hal ALIAS nsx_soc_hal)
if(DEFINED NSX_SOC_TARGET_EXPORT_NAME AND NOT TARGET nsx::${NSX_SOC_TARGET_EXPORT_NAME})
    add_library(nsx::${NSX_SOC_TARGET_EXPORT_NAME} ALIAS nsx_soc_hal)
endif()
add_library(nsx::ambiqsuite_core ALIAS nsx_soc_hal)

target_link_libraries(nsx_soc_hal INTERFACE nsx::board_flags)
if(NOT TARGET nsx_ambiq_hal)
    message(FATAL_ERROR "ns-soc-hal requires a release-specific ambiq HAL module to be added first.")
endif()
if(NOT TARGET nsx_ambiq_bsp)
    message(FATAL_ERROR "ns-soc-hal requires a release-specific ambiq BSP module to be added first.")
endif()
target_link_libraries(nsx_soc_hal INTERFACE nsx_ambiq_bsp nsx_ambiq_hal)

install(TARGETS
    nsx_soc_hal
    EXPORT nsxTargets
)
