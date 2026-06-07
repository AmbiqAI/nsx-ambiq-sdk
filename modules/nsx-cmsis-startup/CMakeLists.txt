if(NOT DEFINED NSX_STARTUP_SOURCE)
    message(FATAL_ERROR "Board must define NSX_STARTUP_SOURCE before cmsis_startup is added.")
endif()
if(NOT TARGET nsx::board_flags)
    message(FATAL_ERROR "Board must define nsx::board_flags before cmsis_startup is added.")
endif()
if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before cmsis_startup is added.")
endif()

set(NSX_AMBIQ_INSTALL_BASE "${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiqsuite/${NSX_AMBIQSUITE_VERSION}")

set(nsx_startup_sources
    ${NSX_STARTUP_SOURCE}
)
if(DEFINED NSX_SYSTEM_SOURCE AND NOT NSX_SYSTEM_SOURCE STREQUAL "")
    list(APPEND nsx_startup_sources ${NSX_SYSTEM_SOURCE})
endif()

add_library(nsx_startup STATIC ${nsx_startup_sources})
set_target_properties(nsx_startup PROPERTIES EXPORT_NAME ${NSX_STARTUP_TARGET_EXPORT_NAME})
add_library(nsx::startup ALIAS nsx_startup)
add_library(nsx::cmsis_startup ALIAS nsx_startup)
add_library(nsx::${NSX_STARTUP_TARGET_EXPORT_NAME} ALIAS nsx_startup)

target_link_libraries(nsx_startup PUBLIC
    nsx::board_flags
    nsx::cmsis_core
)

# Core CMSIS headers (core_cm*, cmsis_compiler.h, m-profile/*) are exported by
# nsx-cmsis-core. Only AmbiqMicro device-specific headers remain owned here.
target_include_directories(nsx_startup PUBLIC
    $<BUILD_INTERFACE:${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Include>
    $<INSTALL_INTERFACE:${NSX_AMBIQ_INSTALL_BASE}/CMSIS/AmbiqMicro/Include>
)

install(TARGETS
    nsx_startup
    EXPORT nsxTargets
)
