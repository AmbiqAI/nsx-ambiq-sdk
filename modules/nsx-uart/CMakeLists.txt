nsx_require_target(nsx::soc_hal nsx-uart)
nsx_require_target(nsx::interrupt nsx-uart)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-uart is added.")
endif()

# apollo330P uses its dedicated apollo330 arch sources.
nsx_select_soc_arch_dir(NSX_UART_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    ATOMIQ apollo5
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-uart/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-uart
)

install(TARGETS
    nsx_uart
    EXPORT nsxTargets
)
