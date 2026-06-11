# nsx-freertos: optional FreeRTOS kernel middleware (Phase 1 MVP).
#
# Vendors a pinned upstream FreeRTOS-Kernel (V11.1.0) and builds the generic
# CMSIS port selected by the SoC descriptor (NSX_SOC_RTOS_PORT_GENERIC, a
# Phase 0 fact). The core SDK owns no kernel and no tick source: kernel/heap
# policy and FreeRTOSConfig.h are application-owned and supplied through the
# nsx::freertos_config interface target.

if(NOT DEFINED NSX_SOC_CORE)
    message(FATAL_ERROR "nsx-freertos requires a SoC descriptor (NSX_SOC_CORE) to be selected first.")
endif()
if(NOT DEFINED NSX_SOC_RTOS_PORT_GENERIC)
    message(FATAL_ERROR
        "nsx-freertos requires NSX_SOC_RTOS_PORT_GENERIC from the SoC descriptor. "
        "Update cmake/socs/<soc>.cmake to publish the RTOS port facts.")
endif()
if(NOT TARGET nsx::soc_flags)
    message(FATAL_ERROR "nsx-freertos requires nsx::soc_flags to be defined first.")
endif()
if(NOT TARGET nsx::freertos_config)
    message(FATAL_ERROR
        "nsx-freertos requires an application-provided nsx::freertos_config interface "
        "target that exposes the directory containing FreeRTOSConfig.h. "
        "See modules/nsx-freertos/templates/FreeRTOSConfig.h.template.")
endif()

# Phase 1 MVP support gate: generic Cortex-M55 GCC port only.
if(NOT NSX_SOC_RTOS_PORT_GENERIC STREQUAL "ARM_CM55_NTZ")
    message(FATAL_ERROR
        "nsx-freertos Phase 1 supports the ARM_CM55_NTZ generic port only; "
        "SoC '${NSX_SOC_SKEW}' requests generic port '${NSX_SOC_RTOS_PORT_GENERIC}'.")
endif()

set(NSX_FREERTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/FreeRTOS-Kernel")
set(NSX_FREERTOS_INCLUDE_DIR "${NSX_FREERTOS_ROOT}/include")
set(NSX_FREERTOS_PORT_DIR "${NSX_FREERTOS_ROOT}/portable/GCC/${NSX_SOC_RTOS_PORT_GENERIC}/non_secure")

nsx_assert_directory_exists("${NSX_FREERTOS_INCLUDE_DIR}")
nsx_assert_directory_exists("${NSX_FREERTOS_PORT_DIR}")
nsx_assert_file_exists("${NSX_FREERTOS_PORT_DIR}/port.c")
nsx_assert_file_exists("${NSX_FREERTOS_PORT_DIR}/portasm.c")
nsx_assert_file_exists("${NSX_FREERTOS_ROOT}/portable/MemMang/heap_4.c")

add_library(nsx_freertos STATIC
    ${NSX_FREERTOS_ROOT}/croutine.c
    ${NSX_FREERTOS_ROOT}/event_groups.c
    ${NSX_FREERTOS_ROOT}/list.c
    ${NSX_FREERTOS_ROOT}/queue.c
    ${NSX_FREERTOS_ROOT}/stream_buffer.c
    ${NSX_FREERTOS_ROOT}/tasks.c
    ${NSX_FREERTOS_ROOT}/timers.c
    ${NSX_FREERTOS_PORT_DIR}/port.c
    ${NSX_FREERTOS_PORT_DIR}/portasm.c
    ${NSX_FREERTOS_ROOT}/portable/MemMang/heap_4.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_freertos.c
)
set_target_properties(nsx_freertos PROPERTIES EXPORT_NAME freertos)
add_library(nsx::freertos ALIAS nsx_freertos)

target_include_directories(nsx_freertos PUBLIC
    $<BUILD_INTERFACE:${NSX_FREERTOS_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${NSX_FREERTOS_PORT_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos/port>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos>
)

# nsx::freertos_config carries the application FreeRTOSConfig.h include dir and
# is required: FreeRTOS.h cannot compile without it. nsx::soc_flags carries the
# CPU/FPU/MVE defines and toolchain flags the kernel and port are built with.
target_link_libraries(nsx_freertos PUBLIC
    nsx::soc_flags
    nsx::freertos_config
)

target_compile_definitions(nsx_freertos PUBLIC NSX_FREERTOS)

install(DIRECTORY ${NSX_FREERTOS_INCLUDE_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos
)
install(DIRECTORY ${NSX_FREERTOS_PORT_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos/port
    FILES_MATCHING PATTERN "*.h"
)
install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/includes-api/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/freertos
    FILES_MATCHING PATTERN "*.h"
)
install(TARGETS
    nsx_freertos
    EXPORT nsxTargets
)
