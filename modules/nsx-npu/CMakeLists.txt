nsx_require_target(nsx::soc_hal nsx-npu)
nsx_require_target(nsx::core nsx-npu)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-npu is added.")
endif()

# The Ethos-U85 and its vendored core driver exist only on Atomiq targets;
# nsx_select_soc_arch_dir fails configuration for any other SoC family.
nsx_select_soc_arch_dir(NSX_NPU_ARCH_DIR
    ATOMIQ atomiq110
)

set(NSX_NPU_DRIVER_DIR ${CMAKE_CURRENT_LIST_DIR}/src/ethos-u-core-driver)

set(NSX_NPU_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_npu.c
    ${NSX_NPU_DRIVER_DIR}/src/ethosu_driver.c
    ${NSX_NPU_DRIVER_DIR}/src/ethosu_device_u85.c
    ${NSX_NPU_DRIVER_DIR}/src/ethosu_pmu.c
)

add_library(nsx_npu STATIC ${NSX_NPU_SOURCES})
set_target_properties(nsx_npu PROPERTIES EXPORT_NAME npu)
add_library(nsx::npu ALIAS nsx_npu)

target_compile_definitions(nsx_npu PUBLIC
    # PUBLIC, not PRIVATE: pmu_ethosu.h branches its event enum (and counter
    # masks) on the NPU generation macro. A consumer that includes the
    # driver headers without ETHOSU85 would silently compile the U55/U65
    # enum with different ordinals than the library was built with.
    ETHOSU85
    ENABLE_U85_PMU
)

target_link_libraries(nsx_npu PUBLIC
    nsx::soc_hal
    nsx::core
)

target_include_directories(nsx_npu PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_NPU_DRIVER_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu/src/ethos-u-core-driver/include>
)

target_include_directories(nsx_npu PRIVATE
    ${NSX_NPU_DRIVER_DIR}/src
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu
)
install(DIRECTORY
    ${NSX_NPU_DRIVER_DIR}/include
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-npu/src/ethos-u-core-driver
)

install(TARGETS
    nsx_npu
    EXPORT nsxTargets
)
