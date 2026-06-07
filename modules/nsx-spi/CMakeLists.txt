if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before ns-spi is added.")
endif()
if(NOT TARGET nsx::core)
    message(FATAL_ERROR "nsx::core must be defined before ns-spi is added.")
endif()

add_library(nsx_spi STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_spi.c
)
set_target_properties(nsx_spi PROPERTIES EXPORT_NAME spi)
add_library(nsx::spi ALIAS nsx_spi)

target_link_libraries(nsx_spi PUBLIC
    nsx::soc_hal
    nsx::core
)

target_include_directories(nsx_spi PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-spi/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-spi
)

install(TARGETS
    nsx_spi
    EXPORT nsxTargets
)
