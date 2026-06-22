nsx_require_target(nsx::soc_hal nsx-timer)
nsx_require_target(nsx::interrupt nsx-timer)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-timer is added.")
endif()

# apollo330P uses its dedicated apollo330 arch sources.
nsx_select_soc_arch_dir(NSX_TIMER_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    ATOMIQ apollo5
    APOLLO330 apollo330
)

set(NSX_TIMER_ARCH_SOURCE "")
if(NSX_TIMER_ARCH_DIR STREQUAL "apollo3")
    set(NSX_TIMER_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo3/nsx_timer_ap3.c)
elseif(NSX_TIMER_ARCH_DIR STREQUAL "apollo4")
    set(NSX_TIMER_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo4/nsx_timer_ap4.c)
elseif(NSX_TIMER_ARCH_DIR STREQUAL "apollo5")
    set(NSX_TIMER_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/nsx_timer_ap5.c)
elseif(NSX_TIMER_ARCH_DIR STREQUAL "apollo330")
    set(NSX_TIMER_ARCH_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo330/nsx_timer_ap330.c)
endif()

add_library(nsx_timer STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_timer.c
    ${NSX_TIMER_ARCH_SOURCE}
)
set_target_properties(nsx_timer PROPERTIES EXPORT_NAME timer)
add_library(nsx::timer ALIAS nsx_timer)

target_link_libraries(nsx_timer PUBLIC
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_include_directories(nsx_timer PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-timer/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-timer
)

install(TARGETS
    nsx_timer
    EXPORT nsxTargets
)
