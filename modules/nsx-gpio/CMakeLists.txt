nsx_require_target(nsx::soc_hal nsx-gpio)
nsx_require_target(nsx::interrupt nsx-gpio)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-gpio is added.")
endif()

add_library(nsx_gpio STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_gpio.c
)
set_target_properties(nsx_gpio PROPERTIES EXPORT_NAME gpio)
add_library(nsx::gpio ALIAS nsx_gpio)

target_link_libraries(nsx_gpio PUBLIC
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_include_directories(nsx_gpio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-gpio/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-gpio
)

install(TARGETS
    nsx_gpio
    EXPORT nsxTargets
)
