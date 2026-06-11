# nsx-freertos: optional FreeRTOS kernel middleware (Phase 1 MVP).
#
# Vendors a pinned upstream FreeRTOS-Kernel (V11.1.0) and builds the generic
# CMSIS port for the active SoC. The core SDK owns no kernel and no tick
# source: kernel/heap policy and FreeRTOSConfig.h are application-owned and
# supplied through the nsx::freertos_config interface target.
#
# Port selection prefers the Phase 0 SoC fact NSX_SOC_RTOS_PORT_GENERIC (set by
# cmake/socs/<soc>.cmake) when the active descriptor publishes it, and otherwise
# derives the port from the CPU so the module also works behind board
# descriptors that do not surface RTOS facts (e.g. generated app boards).

if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR
        "nsx-freertos requires nsx::soc_hal (the SoC HAL flags/defines carrier) "
        "to be added before it. Add nsx-soc-hal first.")
endif()
if(NOT TARGET nsx::freertos_config)
    message(FATAL_ERROR
        "nsx-freertos requires an application-provided nsx::freertos_config interface "
        "target that exposes the directory containing FreeRTOSConfig.h. "
        "See modules/nsx-freertos/templates/FreeRTOSConfig.h.template.")
endif()

# Resolve the generic CMSIS port. Prefer the SoC descriptor fact when present;
# otherwise derive it from the CPU (NSX_SOC_CORE, else NSX_CPU).
if(DEFINED NSX_SOC_RTOS_PORT_GENERIC AND NOT NSX_SOC_RTOS_PORT_GENERIC STREQUAL "")
    set(_nsx_freertos_port "${NSX_SOC_RTOS_PORT_GENERIC}")
else()
    set(_nsx_freertos_cpu "${NSX_SOC_CORE}")
    if(NOT _nsx_freertos_cpu)
        set(_nsx_freertos_cpu "${NSX_CPU}")
    endif()
    if(_nsx_freertos_cpu STREQUAL "cortex-m55")
        set(_nsx_freertos_port "ARM_CM55_NTZ")
    else()
        set(_nsx_freertos_port "")
    endif()
endif()

if(NOT _nsx_freertos_port)
    message(FATAL_ERROR
        "nsx-freertos could not resolve a FreeRTOS port: the active SoC/board "
        "descriptor provided neither NSX_SOC_RTOS_PORT_GENERIC nor a recognised "
        "CPU (NSX_SOC_CORE/NSX_CPU).")
endif()

# Phase 1 MVP support gate: generic Cortex-M55 GCC port only.
if(NOT _nsx_freertos_port STREQUAL "ARM_CM55_NTZ")
    message(FATAL_ERROR
        "nsx-freertos Phase 1 supports the ARM_CM55_NTZ generic port only; "
        "resolved generic port '${_nsx_freertos_port}'.")
endif()

set(NSX_FREERTOS_ROOT "${CMAKE_CURRENT_LIST_DIR}/sdk/third_party/FreeRTOS-Kernel")
set(NSX_FREERTOS_INCLUDE_DIR "${NSX_FREERTOS_ROOT}/include")
set(NSX_FREERTOS_PORT_DIR "${NSX_FREERTOS_ROOT}/portable/GCC/${_nsx_freertos_port}/non_secure")

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
# is required: FreeRTOS.h cannot compile without it. nsx::soc_hal carries the
# CPU/FPU/MVE defines and toolchain flags (via the board/SoC flags it links)
# the kernel and port are built with.
target_link_libraries(nsx_freertos PUBLIC
    nsx::soc_hal
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
