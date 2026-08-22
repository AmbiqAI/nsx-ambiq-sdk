nsx_require_target(nsx::soc_hal nsx-power)
if(NOT TARGET nsx::timer AND DEFINED NSX_SOC_FAMILY AND NSX_SOC_FAMILY IN_LIST NSX_SOC_FAMILIES_APOLLO4)
    message(FATAL_ERROR "nsx::timer must be defined before Apollo4 nsx-power is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-power is added.")
endif()

# apollo330P uses its dedicated apollo330 arch sources.
nsx_select_soc_arch_dir(NSX_POWER_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    ATOMIQ atomiq110
    APOLLO330 apollo330
)

set(NSX_POWER_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_power.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_power_monitor.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_power_profile.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_POWER_ARCH_DIR}/nsx_power.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_POWER_ARCH_DIR}/nsx_power_profile.c
)

if(NSX_POWER_ARCH_DIR STREQUAL "apollo4")
    list(APPEND NSX_POWER_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/src/apollo4/nsx_power_tempco.c
    )
endif()

add_library(nsx_power STATIC ${NSX_POWER_SOURCES})
set_target_properties(nsx_power PROPERTIES EXPORT_NAME power)
add_library(nsx::power ALIAS nsx_power)

target_link_libraries(nsx_power PUBLIC
    nsx::soc_hal
    nsx::core
)

if(NSX_POWER_ARCH_DIR STREQUAL "apollo4")
    target_link_libraries(nsx_power PUBLIC nsx::timer)
endif()

target_include_directories(nsx_power PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/${NSX_POWER_ARCH_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-power/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-power/src/${NSX_POWER_ARCH_DIR}>
)

# Module-private headers (nsx_power_internal.h) shared by the common sources
# and the per-arch sources.
target_include_directories(nsx_power PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-power
)
install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_POWER_ARCH_DIR}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-power/src
)

install(TARGETS
    nsx_power
    EXPORT nsxTargets
)
