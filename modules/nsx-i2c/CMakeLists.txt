if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before ns-i2c is added.")
endif()

add_library(nsx_i2c STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_i2c_register_driver.c
)
set_target_properties(nsx_i2c PROPERTIES EXPORT_NAME i2c)
add_library(nsx::i2c ALIAS nsx_i2c)

target_link_libraries(nsx_i2c PUBLIC
    nsx::soc_hal
    nsx_core
)

target_include_directories(nsx_i2c PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<BUILD_INTERFACE:${NSX_ROOT}/modules/nsx-core/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-i2c/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-i2c
)

install(TARGETS
    nsx_i2c
    EXPORT nsxTargets
)
