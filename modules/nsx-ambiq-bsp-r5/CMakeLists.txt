if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before ns-ambiq-bsp-r5 is added.")
endif()
if(NOT TARGET nsx::board_flags)
    message(FATAL_ERROR "Board must define nsx::board_flags before ns-ambiq-bsp-r5 is added.")
endif()
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME OR NOT DEFINED NSX_AMBIQ_BSP_DIR OR NOT DEFINED NSX_AMBIQ_PART_NAME)
    message(FATAL_ERROR "Board must define NSX_AMBIQ_BOARD_NAME, NSX_AMBIQ_BSP_DIR, and NSX_AMBIQ_PART_NAME before ns-ambiq-bsp-r5 is added.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r5")
    message(FATAL_ERROR "ns-ambiq-bsp-r5 requires NSX_SDK_PROVIDER=ambiqsuite-r5.")
endif()

option(NSX_AMBIQSUITE_USE_PREBUILT "Use prebuilt AmbiqSuite static libraries" ON)

set(NSX_AMBIQ_INSTALL_BASE "${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiqsuite/${NSX_AMBIQSUITE_VERSION}")
if(NOT DEFINED NSX_AMBIQ_BSP_LIB_SUBDIR)
    set(NSX_AMBIQ_BSP_LIB_SUBDIR "evb")
endif()
if(NOT DEFINED NSX_AMBIQ_BSP_LIB_PART_NAME)
    set(NSX_AMBIQ_BSP_LIB_PART_NAME "${NSX_AMBIQ_PART_NAME}")
endif()

if(NOT DEFINED NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
    nsx_resolve_ambiqsuite_artifact_toolchain(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
endif()

nsx_assert_directory_exists("${NSX_AMBIQSUITE_ROOT}")
nsx_assert_directory_exists("${NSX_AMBIQ_BSP_DIR}")
nsx_assert_path_component(NSX_AMBIQ_BOARD_NAME)
nsx_assert_path_component(NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN)
nsx_assert_path_component(NSX_AMBIQ_BSP_LIB_PART_NAME)
nsx_assert_path_component(NSX_AMBIQ_BSP_LIB_SUBDIR)

set(NSX_BSP_LIB_DEFAULT "${NSX_AMBIQSUITE_ROOT}/lib/${NSX_AMBIQSUITE_ARTIFACT_TOOLCHAIN}/${NSX_AMBIQ_BSP_LIB_PART_NAME}/${NSX_AMBIQ_BSP_LIB_SUBDIR}/libam_bsp.a")
set(NSX_BSP_LIB "${NSX_BSP_LIB_DEFAULT}" CACHE FILEPATH "Path to AmbiqSuite BSP static library")

add_library(nsx_ambiq_bsp INTERFACE)
set_target_properties(nsx_ambiq_bsp PROPERTIES EXPORT_NAME ambiq_bsp_r5)
add_library(nsx::ambiq_bsp ALIAS nsx_ambiq_bsp)
add_library(nsx::ambiq_bsp_r5 ALIAS nsx_ambiq_bsp)
if(NOT TARGET nsx_ambiq_hal)
    message(FATAL_ERROR "ns-ambiq-bsp-r5 requires nsx_ambiq_hal to be added first.")
endif()
target_link_libraries(nsx_ambiq_bsp INTERFACE nsx::board_flags)
target_include_directories(nsx_ambiq_bsp INTERFACE
    $<BUILD_INTERFACE:${NSX_AMBIQ_BSP_DIR}>
    $<INSTALL_INTERFACE:${NSX_AMBIQ_INSTALL_BASE}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp>
)

if(NSX_AMBIQSUITE_USE_PREBUILT)
    nsx_validate_prebuilt_abi()
    nsx_assert_file_exists("${NSX_BSP_LIB}")
    target_link_libraries(nsx_ambiq_bsp INTERFACE "${NSX_BSP_LIB}" nsx_ambiq_hal)
else()
    message(FATAL_ERROR "ns-ambiq-bsp-r5 source fallback is not implemented yet.")
endif()

install(DIRECTORY
    ${NSX_AMBIQSUITE_ROOT}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}/boards/${NSX_AMBIQ_BOARD_NAME}
)

install(TARGETS
    nsx_ambiq_bsp
    EXPORT nsxTargets
)
