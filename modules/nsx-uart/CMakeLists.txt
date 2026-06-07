if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before ns-uart is added.")
endif()
if(NOT TARGET nsx::interrupt)
    message(FATAL_ERROR "nsx::interrupt must be defined before ns-uart is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before ns-uart is added.")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_core)
    set(NSX_UART_CORE_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_core}")
else()
    set(NSX_UART_CORE_DIR "${NSX_ROOT}/modules/nsx-core")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_interrupt)
    set(NSX_UART_INTERRUPT_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_interrupt}")
else()
    set(NSX_UART_INTERRUPT_DIR "${NSX_ROOT}/modules/nsx-interrupt")
endif()

nsx_select_soc_arch_dir(NSX_UART_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    APOLLO330 apollo330
)

add_library(nsx_uart STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_uart.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_UART_ARCH_DIR}/nsx_uart.c
)
set_target_properties(nsx_uart PROPERTIES EXPORT_NAME uart)
add_library(nsx::uart ALIAS nsx_uart)

target_link_libraries(nsx_uart PUBLIC
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_include_directories(nsx_uart PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_UART_CORE_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_UART_INTERRUPT_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-uart/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-interrupt/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-uart
)

install(TARGETS
    nsx_uart
    EXPORT nsxTargets
)
