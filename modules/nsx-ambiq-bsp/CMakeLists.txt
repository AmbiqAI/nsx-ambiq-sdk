if(NOT DEFINED NSX_AMBIQSUITE_ROOT)
    message(FATAL_ERROR "NSX_AMBIQSUITE_ROOT must be configured before nsx-ambiq-bsp is added.")
endif()
nsx_require_target(nsx::board_flags nsx-ambiq-bsp)
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME OR NOT DEFINED NSX_AMBIQ_BSP_DIR OR NOT DEFINED NSX_AMBIQ_PART_NAME)
    message(FATAL_ERROR "Board must define NSX_AMBIQ_BOARD_NAME, NSX_AMBIQ_BSP_DIR, and NSX_AMBIQ_PART_NAME before nsx-ambiq-bsp is added.")
endif()
if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR "nsx-ambiq-bsp requires NSX_SDK_PROVIDER=ambiqsuite.")
endif()

option(NSX_AMBIQSUITE_USE_PREBUILT "Use prebuilt AmbiqSuite static libraries" ON)

set(NSX_AMBIQ_INSTALL_BASE "${CMAKE_INSTALL_INCLUDEDIR}/nsx/ambiqsuite/${NSX_AMBIQSUITE_VERSION}")
# Prebuilt BSP libs ship under lib/<toolchain>/<part>/<board>/libam_bsp.a, so the
# board name is the correct default subdir. A board fragment may override this for
# vendor folders that diverge from the board name (e.g. apollo5b_evb -> eb_revb).
# The legacy bare "evb" default produced wrong paths for boards like apollo4p_evb
# when the fragment did not pre-set the subdir.
if(NOT DEFINED NSX_AMBIQ_BSP_LIB_SUBDIR)
    set(NSX_AMBIQ_BSP_LIB_SUBDIR "${NSX_AMBIQ_BOARD_NAME}")
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
set_target_properties(nsx_ambiq_bsp PROPERTIES EXPORT_NAME ambiq_bsp)
add_library(nsx::ambiq_bsp ALIAS nsx_ambiq_bsp)
if(NOT TARGET nsx_ambiq_hal)
    message(FATAL_ERROR "nsx-ambiq-bsp requires nsx_ambiq_hal to be added first.")
endif()
target_link_libraries(nsx_ambiq_bsp INTERFACE nsx::board_flags)
target_include_directories(nsx_ambiq_bsp INTERFACE
    $<BUILD_INTERFACE:${NSX_AMBIQ_BSP_DIR}>
    $<INSTALL_INTERFACE:${NSX_AMBIQ_INSTALL_BASE}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp>
)

# --- nsx::bsp role seam (issue #154a.2) -------------------------------------
# Provider-neutral alias so consumers can depend on the BSP role without
# binding to the AmbiqSuite-specific target name. A configure may only have a
# single BSP producer (a duplicate ALIAS is a hard CMake error), which enforces
# the single-producer rule between the SDK provider and any custom BSP module.
set(NSX_BSP_CONTRACT_VERSION 1 CACHE INTERNAL "nsx::bsp role contract version" FORCE)
add_library(nsx::bsp ALIAS nsx_ambiq_bsp)

# Capability advertisement. Board-layer fragments declare hardware facts as
# plain variables before this module is added; we surface them as an open,
# additive token list and as interface compile definitions on the BSP target.
# Unknown tokens are never an error for consumers.
set(_nsx_bsp_caps "")
if(NSX_BOARD_HAS_BUTTONS)
    if(NOT DEFINED NSX_BOARD_BUTTON_COUNT OR NOT DEFINED NSX_BOARD_BUTTON_PINS)
        message(FATAL_ERROR
            "Board set NSX_BOARD_HAS_BUTTONS but did not set "
            "NSX_BOARD_BUTTON_COUNT and NSX_BOARD_BUTTON_PINS.")
    endif()
    list(LENGTH NSX_BOARD_BUTTON_PINS _nsx_button_pin_count)
    if(NOT _nsx_button_pin_count EQUAL NSX_BOARD_BUTTON_COUNT)
        message(FATAL_ERROR
            "NSX_BOARD_BUTTON_COUNT (${NSX_BOARD_BUTTON_COUNT}) does not match the "
            "number of entries in NSX_BOARD_BUTTON_PINS (${_nsx_button_pin_count}).")
    endif()
    list(APPEND _nsx_bsp_caps buttons)
    target_compile_definitions(nsx_ambiq_bsp INTERFACE
        $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_HAS_BUTTONS=1>
        $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_BUTTON_COUNT=${NSX_BOARD_BUTTON_COUNT}>
    )
    math(EXPR _nsx_button_last "${NSX_BOARD_BUTTON_COUNT} - 1")
    foreach(_nsx_button_idx RANGE 0 ${_nsx_button_last})
        list(GET NSX_BOARD_BUTTON_PINS ${_nsx_button_idx} _nsx_button_pin)
        target_compile_definitions(nsx_ambiq_bsp INTERFACE
            $<$<COMPILE_LANGUAGE:C>:NSX_BOARD_BUTTON${_nsx_button_idx}_PIN=${_nsx_button_pin}>)
    endforeach()
    unset(_nsx_button_pin_count)
    unset(_nsx_button_last)
    unset(_nsx_button_idx)
    unset(_nsx_button_pin)
endif()
set(NSX_BSP_CAPABILITIES "${_nsx_bsp_caps}" CACHE INTERNAL
    "Capability tokens advertised by nsx::bsp" FORCE)
unset(_nsx_bsp_caps)
# ---------------------------------------------------------------------------

if(NSX_AMBIQSUITE_USE_PREBUILT)
    nsx_validate_prebuilt_abi()
    nsx_assert_file_exists("${NSX_BSP_LIB}")
    target_link_libraries(nsx_ambiq_bsp INTERFACE "${NSX_BSP_LIB}" nsx_ambiq_hal)
else()
    message(FATAL_ERROR "nsx-ambiq-bsp source fallback is not implemented yet.")
endif()

install(DIRECTORY
    ${NSX_AMBIQSUITE_ROOT}/boards/${NSX_AMBIQ_BOARD_NAME}/bsp
    DESTINATION ${NSX_AMBIQ_INSTALL_BASE}/boards/${NSX_AMBIQ_BOARD_NAME}
)

install(TARGETS
    nsx_ambiq_bsp
    EXPORT nsxTargets
)
