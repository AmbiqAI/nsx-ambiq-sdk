if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before nsx-gpio is added.")
endif()
if(NOT TARGET nsx::interrupt)
    message(FATAL_ERROR "nsx::interrupt must be defined before nsx-gpio is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-gpio is added.")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_core)
    set(NSX_GPIO_CORE_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_core}")
else()
    set(NSX_GPIO_CORE_DIR "${NSX_ROOT}/modules/nsx-core")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_interrupt)
    set(NSX_GPIO_INTERRUPT_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_interrupt}")
else()
    set(NSX_GPIO_INTERRUPT_DIR "${NSX_ROOT}/modules/nsx-interrupt")
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
    $<BUILD_INTERFACE:${NSX_GPIO_CORE_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_GPIO_INTERRUPT_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-gpio/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-interrupt/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-gpio
)

install(TARGETS
    nsx_gpio
    EXPORT nsxTargets
)
