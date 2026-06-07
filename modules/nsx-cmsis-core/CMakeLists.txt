# nsx-cmsis-core
#
# Standalone CMSIS-Core(M) header package, decoupled from any specific
# AmbiqSuite SDK release. Provides the ARM-published Core headers (core_cm*,
# cmsis_compiler/gcc/armclang/iccarm/clang, mpu/pmu/cache/pac via m-profile/)
# as a single INTERFACE target that other NSX modules consume in place of the
# Core headers historically vendored inside nsx-ambiqsuite-r{3,4,5}/sdk/CMSIS/.
#
# Source: ARM-software/CMSIS_6 v6.2.0 (CMSIS-Core(M) v6.1).

set(NSX_CMSIS_CORE_INSTALL_BASE
    "${CMAKE_INSTALL_INCLUDEDIR}/nsx/cmsis-core"
    CACHE PATH "Install destination for CMSIS-Core headers"
)

add_library(nsx_cmsis_core INTERFACE)
set_target_properties(nsx_cmsis_core PROPERTIES EXPORT_NAME cmsis_core)
add_library(nsx::cmsis_core ALIAS nsx_cmsis_core)

target_include_directories(nsx_cmsis_core INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Include>
    $<INSTALL_INTERFACE:${NSX_CMSIS_CORE_INSTALL_BASE}/Include>
)

install(TARGETS
    nsx_cmsis_core
    EXPORT nsxTargets
)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/Include/
    DESTINATION ${NSX_CMSIS_CORE_INSTALL_BASE}/Include
    FILES_MATCHING PATTERN "*.h"
)
