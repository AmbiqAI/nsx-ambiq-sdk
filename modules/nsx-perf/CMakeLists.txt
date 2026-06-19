nsx_require_target(nsx::soc_hal nsx-perf)

add_library(nsx_perf STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_perf_profile.c
)
set_target_properties(nsx_perf PROPERTIES EXPORT_NAME perf)
add_library(nsx::perf ALIAS nsx_perf)

target_link_libraries(nsx_perf PUBLIC
    nsx::soc_hal
    nsx::core
)

target_include_directories(nsx_perf PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-perf/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-perf
)

install(TARGETS
    nsx_perf
    EXPORT nsxTargets
)
