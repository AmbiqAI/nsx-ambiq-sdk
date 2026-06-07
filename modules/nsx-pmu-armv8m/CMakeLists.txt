if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before nsx-pmu-armv8m is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-pmu-armv8m is added.")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_core)
    set(NSX_PMU_CORE_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_core}")
else()
    set(NSX_PMU_CORE_DIR "${NSX_ROOT}/modules/nsx-core")
endif()

nsx_select_soc_arch_dir(NSX_PMU_IMPL_DIR
    APOLLO5 armv8m
    APOLLO330 armv8m
)

add_library(nsx_pmu_armv8m STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}/nsx_pmu_utils.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}/nsx_pmu_accumulator.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}/nsx_pmu_capture.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}/nsx_pmu_transport.c
)
set_target_properties(nsx_pmu_armv8m PROPERTIES EXPORT_NAME pmu_armv8m)
add_library(nsx::pmu_armv8m ALIAS nsx_pmu_armv8m)

target_link_libraries(nsx_pmu_armv8m PUBLIC
    nsx::cmsis_core
    nsx::soc_hal
    nsx::core
)

target_include_directories(nsx_pmu_armv8m PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}>
    $<BUILD_INTERFACE:${NSX_PMU_CORE_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-pmu-armv8m/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-pmu-armv8m/src/${NSX_PMU_IMPL_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-pmu-armv8m
)
install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PMU_IMPL_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-pmu-armv8m/src
)

install(TARGETS
    nsx_pmu_armv8m
    EXPORT nsxTargets
)
