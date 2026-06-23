nsx_require_target(nsx::soc_hal nsx-interrupt)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-interrupt is added.")
endif()

# apollo330P uses its dedicated apollo330 arch sources.
nsx_select_soc_arch_dir(NSX_INTERRUPT_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    ATOMIQ apollo5
    APOLLO330 apollo330
)

set(NSX_INTERRUPT_ARCH_SOURCE "")
if(NSX_INTERRUPT_ARCH_DIR STREQUAL "apollo3")
    set(NSX_INTERRUPT_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo3/nsx_interrupt_vectors.c)
elseif(NSX_INTERRUPT_ARCH_DIR STREQUAL "apollo4")
    set(NSX_INTERRUPT_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo4/nsx_interrupt_vectors.c)
elseif(NSX_INTERRUPT_ARCH_DIR STREQUAL "apollo5")
    set(NSX_INTERRUPT_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/nsx_interrupt_vectors.c)
elseif(NSX_INTERRUPT_ARCH_DIR STREQUAL "apollo330")
    set(NSX_INTERRUPT_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo330/nsx_interrupt_vectors.c)
endif()

add_library(nsx_interrupt STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_interrupt.c
    ${NSX_INTERRUPT_ARCH_SOURCE}
)
set_target_properties(nsx_interrupt PROPERTIES EXPORT_NAME interrupt)
add_library(nsx::interrupt ALIAS nsx_interrupt)

target_link_libraries(nsx_interrupt PUBLIC
    nsx::soc_hal
    nsx::core
)

target_include_directories(nsx_interrupt PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-interrupt/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-interrupt
)

install(TARGETS
    nsx_interrupt
    EXPORT nsxTargets
)
