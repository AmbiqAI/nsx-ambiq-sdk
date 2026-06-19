nsx_require_target(nsx::soc_hal nsx-spi)
nsx_require_target(nsx::core nsx-spi)

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
