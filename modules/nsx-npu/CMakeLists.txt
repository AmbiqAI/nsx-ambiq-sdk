nsx_require_target(nsx::soc_hal nsx-npu)
nsx_require_target(nsx::core nsx-npu)
nsx_require_target(nsx::ethos_u_driver nsx-npu)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-npu is added.")
endif()

# The Ethos-U85 exists only on Atomiq targets; nsx_select_soc_arch_dir fails
# configuration for any other SoC family.
nsx_select_soc_arch_dir(NSX_NPU_ARCH_DIR
    ATOMIQ atomiq110
)

# nsx-npu no longer vendors (a modified copy of) Arm's ethos-u-core-driver.
# The pristine upstream driver is built once by nsx-ethos-u-driver (linked
# below via nsx::ethos_u_driver); this module contributes only the Atomiq110
# glue: power-domain sequencing, IRQ wiring, and performance-mode selection.
set(NSX_NPU_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_npu.c
)

add_library(nsx_npu STATIC ${NSX_NPU_SOURCES})
set_target_properties(nsx_npu PROPERTIES EXPORT_NAME npu)
add_library(nsx::npu ALIAS nsx_npu)

target_link_libraries(nsx_npu PUBLIC
    nsx::soc_hal
    nsx::core
    nsx::ethos_u_driver
)

target_include_directories(nsx_npu PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu
)

install(TARGETS
    nsx_npu
    EXPORT nsxTargets
)
