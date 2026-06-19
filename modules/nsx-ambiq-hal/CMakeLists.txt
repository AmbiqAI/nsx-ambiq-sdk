if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before nsx-ambiq-hal is added.")
endif()
nsx_require_target(nsx::board_flags nsx-ambiq-hal)
if(NOT DEFINED NSX_AMBIQ_PART_NAME OR NOT DEFINED NSX_AMBIQ_MCU_DIR OR NOT DEFINED NSX_AMBIQ_HAL_DIR)
    message(FATAL_ERROR "Board must define NSX_AMBIQ_* MCU/HAL paths before nsx-ambiq-hal is added.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR "nsx-ambiq-hal requires NSX_SDK_PROVIDER=ambiqsuite.")
endif()

# The PMU helper surface (includes-api + nsx_ambiq_pmu.c + am_util_pmu.c) ships
# only for SoCs whose HAL exposes the armv8m PMU. Derive that capability from
# the published SoC fact (NSX_SOC_PMU_TIER, set by cmake/socs/facts/<skew>.cmake
# via nsx_load_soc_facts) so neither the SDK SoC descriptors nor downstream
# board.cmake files have to set NSX_AMBIQ_HAL_HAS_PMU by hand. An explicit value
# set before this module is added still wins.
if(NOT DEFINED NSX_AMBIQ_HAL_HAS_PMU)
    if(DEFINED NSX_SOC_PMU_TIER AND NSX_SOC_PMU_TIER STREQUAL "armv8m")
        set(NSX_AMBIQ_HAL_HAS_PMU TRUE)
    else()
        set(NSX_AMBIQ_HAL_HAS_PMU FALSE)
    endif()
endif()

option(NSX_AMBIQSUITE_USE_PREBUILT "Use prebuilt AmbiqSuite static libraries" ON)
option(NSX_AMBIQSUITE_USE_SOURCE_FALLBACK "Use local source fallback if prebuilt is disabled" ON)

set(NSX_AMBIQ_INSTALL_BASE "${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiqsuite/${NSX_AMBIQSUITE_VERSION}")
if(NOT DEFINED NSX_AMBIQ_HAL_LIB_PART_NAME)
    set(NSX_AMBIQ_HAL_LIB_PART_NAME "${NSX_AMBIQ_PART_NAME}")
endif()
if(NOT DEFINED NSX_AMBIQ_MCU_INSTALL_NAME)
    get_filename_component(NSX_AMBIQ_MCU_INSTALL_NAME "${NSX_AMBIQ_MCU_DIR}" NAME)
endif()

if(NOT DEFINED NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
    nsx_resolve_ambiqsuite_artifact_toolchain(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
endif()

nsx_assert_directory_exists("${NSX_AMBIQSUITE_ROOT}")
nsx_assert_directory_exists("${NSX_AMBIQ_MCU_DIR}")
nsx_assert_directory_exists("${NSX_AMBIQ_HAL_DIR}")
# Newer SoCs (apollo4+) split the HAL into an additional hal/mcu tree; older
# parts (apollo2/apollo3) do not. The SoC cmake defines NSX_AMBIQ_HAL_MCU_DIR
# only when that tree is present.
if(DEFINED NSX_AMBIQ_HAL_MCU_DIR)
    nsx_assert_directory_exists("${NSX_AMBIQ_HAL_MCU_DIR}")
endif()
nsx_assert_path_component(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
nsx_assert_path_component(NSX_AMBIQ_HAL_LIB_PART_NAME)
nsx_assert_path_component(NSX_AMBIQ_MCU_INSTALL_NAME)

# Prebuilt libraries are laid out per-toolchain: lib/<toolchain>/<part>/libam_hal.a.
# The atfe->gcc USB CDC workaround is applied centrally by
# nsx_resolve_ambiqsuite_artifact_toolchain.
set(NSX_HAL_LIB_DEFAULT "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN}/${NSX_AMBIQ_HAL_LIB_PART_NAME}/libam_hal.a")
set(NSX_HAL_LIB "${NSX_HAL_LIB_DEFAULT}" CACHE FILEPATH "Path to AmbiqSuite HAL static library")

# Core CMSIS headers (core_cm*, cmsis_compiler.h, m-profile/*) come from
# nsx-cmsis-core via target_link_libraries below. Only AmbiqMicro device,
# MCU/HAL, and utility headers remain owned here. The PMU helper headers
# (includes-api) ship only for SoCs whose HAL exposes the armv8m PMU.
set(NSX_AMBIQ_HAL_BUILD_INCLUDES "")
if(NSX_AMBIQ_HAL_HAS_PMU)
    list(APPEND NSX_AMBIQ_HAL_BUILD_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/includes-api)
endif()
list(APPEND NSX_AMBIQ_HAL_BUILD_INCLUDES
    ${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Include
    ${NSX_AMBIQSUITE_ROOT}/devices
    ${NSX_AMBIQ_MCU_DIR}
    ${NSX_AMBIQ_HAL_DIR}
)
if(DEFINED NSX_AMBIQ_HAL_MCU_DIR)
    list(APPEND NSX_AMBIQ_HAL_BUILD_INCLUDES ${NSX_AMBIQ_HAL_MCU_DIR})
endif()
list(APPEND NSX_AMBIQ_HAL_BUILD_INCLUDES ${NSX_AMBIQSUITE_ROOT}/utils)

set(NSX_AMBIQ_HAL_INSTALL_INCLUDES "")
if(NSX_AMBIQ_HAL_HAS_PMU)
    list(APPEND NSX_AMBIQ_HAL_INSTALL_INCLUDES ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-ambiq-hal/includes-api)
endif()
list(APPEND NSX_AMBIQ_HAL_INSTALL_INCLUDES
    ${NSX_AMBIQ_INSTALL_BASE}/CMSIS/AmbiqMicro/Include
    ${NSX_AMBIQ_INSTALL_BASE}/devices
    ${NSX_AMBIQ_INSTALL_BASE}/mcu/${NSX_AMBIQ_MCU_INSTALL_NAME}
    ${NSX_AMBIQ_INSTALL_BASE}/mcu/${NSX_AMBIQ_MCU_INSTALL_NAME}/hal
)
if(DEFINED NSX_AMBIQ_HAL_MCU_DIR)
    list(APPEND NSX_AMBIQ_HAL_INSTALL_INCLUDES ${NSX_AMBIQ_INSTALL_BASE}/mcu/${NSX_AMBIQ_MCU_INSTALL_NAME}/hal/mcu)
endif()
list(APPEND NSX_AMBIQ_HAL_INSTALL_INCLUDES ${NSX_AMBIQ_INSTALL_BASE}/utils)

set(NSX_AMBIQ_HAL_INCLUDE_ARGS "")
foreach(p IN LISTS NSX_AMBIQ_HAL_BUILD_INCLUDES)
    list(APPEND NSX_AMBIQ_HAL_INCLUDE_ARGS "$<BUILD_INTERFACE:${p}>")
endforeach()
foreach(p IN LISTS NSX_AMBIQ_HAL_INSTALL_INCLUDES)
    list(APPEND NSX_AMBIQ_HAL_INCLUDE_ARGS "$<INSTALL_INTERFACE:${p}>")
endforeach()

set(NSX_AMBIQ_HAL_SOURCES
    ${NSX_AMBIQSUITE_ROOT}/src/am_util_stdio.c
    ${NSX_AMBIQSUITE_ROOT}/src/am_util_delay.c
)
if(NSX_AMBIQ_HAL_HAS_PMU)
    list(APPEND NSX_AMBIQ_HAL_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/nsx_ambiq_pmu.c
        ${NSX_AMBIQSUITE_ROOT}/src/am_util_pmu.c
    )
endif()

add_library(nsx_ambiq_hal STATIC ${NSX_AMBIQ_HAL_SOURCES})
set_target_properties(nsx_ambiq_hal PROPERTIES EXPORT_NAME ambiq_hal)
add_library(nsx::ambiq_hal ALIAS nsx_ambiq_hal)
target_link_libraries(nsx_ambiq_hal PUBLIC
    nsx::board_flags
    nsx::cmsis_core
)
target_include_directories(nsx_ambiq_hal PUBLIC ${NSX_AMBIQ_HAL_INCLUDE_ARGS})
target_compile_definitions(nsx_ambiq_hal PUBLIC NSX_AMBIQSUITE)

if(NSX_AMBIQSUITE_USE_PREBUILT)
    nsx_validate_prebuilt_abi()
    nsx_assert_file_exists("${NSX_HAL_LIB}")
    target_link_libraries(nsx_ambiq_hal PUBLIC "${NSX_HAL_LIB}")
    nsx_toolchain_uses_newlib(NSX_TOOLCHAIN_USES_NEWLIB)
    if(NSX_TOOLCHAIN_USES_NEWLIB)
        target_link_libraries(nsx_ambiq_hal PUBLIC m)
    endif()
elseif(NSX_AMBIQSUITE_USE_SOURCE_FALLBACK)
    message(FATAL_ERROR "nsx-ambiq-hal source fallback is not implemented yet.")
else()
    message(FATAL_ERROR "Prebuilt disabled and source fallback disabled for nsx-ambiq-hal.")
endif()

# CMSIS/ARM/Include now ships from nsx-cmsis-core; only AmbiqMicro device
# headers are installed by this module.
install(DIRECTORY
    ${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Include
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}/CMSIS/AmbiqMicro
)
install(DIRECTORY
    ${NSX_AMBIQSUITE_ROOT}/devices
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}
)
install(DIRECTORY
    ${NSX_AMBIQ_MCU_DIR}
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}/mcu
)
install(FILES
    ${NSX_AMBIQSUITE_ROOT}/mcu/am_sdk_version.h
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}/mcu
)
install(DIRECTORY
    ${NSX_AMBIQSUITE_ROOT}/utils
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}
)
if(NSX_AMBIQ_HAL_HAS_PMU)
    install(DIRECTORY
        ${CMAKE_CURRENT_LIST_DIR}/includes-api
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-ambiq-hal
    )
endif()

install(TARGETS
    nsx_ambiq_hal
    EXPORT nsxTargets
)
