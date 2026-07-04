nsx_require_target(nsx::soc_hal nsx-psram)
nsx_require_target(nsx::board_flags nsx-psram)
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-psram is added.")
endif()
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME)
    message(FATAL_ERROR "NSX_AMBIQ_BOARD_NAME must be defined before nsx-psram is added.")
endif()

# apollo330P reuses the Apollo5-class PSRAM sources (apollo5 arch dir).
nsx_select_soc_arch_dir(NSX_PSRAM_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
    ATOMIQ apollo5
    APOLLO330 apollo5
)

set(NSX_PSRAM_DEVICE_SOURCE "")

if(NSX_PSRAM_ARCH_DIR STREQUAL "apollo3")
    if(NOT (NSX_AMBIQ_BOARD_NAME STREQUAL "apollo3p_evb"
            OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo3p_evb_cygnus"))
        message(FATAL_ERROR "nsx-psram is currently supported only on Apollo3p PSRAM boards. Got NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
    endif()
    set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo3/devices/am_devices_mspi_psram_aps6404l.c)
    if(NOT DEFINED NSX_PSRAM_DEVICE_SIZE_BYTES)
        set(NSX_PSRAM_DEVICE_SIZE_BYTES 8388608u)
    endif()
elseif(NSX_PSRAM_ARCH_DIR STREQUAL "apollo4")
    # apollo4p_evb_disp_shield_rev2 is the only Apollo4 board currently
    # staged in this SDK that declares a populated PSRAM device in its
    # AmbiqSuite BSP (AM_BSP_MSPI_PSRAM_DEVICE_APS25616N, hardware-validated
    # zero-mismatch XIP read/write). The plain apollo4l/apollo4p EVB and
    # Blue variants do not populate PSRAM and remain unsupported here.
    if(NOT DEFINED NSX_PSRAM_MSPI_MODULE)
        if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4p_evb_disp_shield_rev2")
            set(NSX_PSRAM_MSPI_MODULE 0)
            set(NSX_PSRAM_DEVICE_CONFIG AM_BSP_MSPI_PSRAM_MODULE_HEX_DDR_CE)
        else()
            message(FATAL_ERROR "nsx-psram has no supported Apollo4 PSRAM board staged for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
        endif()
    endif()
    set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo4/devices/am_devices_mspi_psram_aps25616n.c)
    if(NOT DEFINED NSX_PSRAM_DEVICE_SIZE_BYTES)
        set(NSX_PSRAM_DEVICE_SIZE_BYTES 33554432u)
    endif()
elseif(NSX_PSRAM_ARCH_DIR STREQUAL "apollo5")
    if(NOT DEFINED NSX_PSRAM_MSPI_MODULE)
        if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510b_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510dL_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo330mP_evb")
            set(NSX_PSRAM_MSPI_MODULE 0)
        else()
            message(FATAL_ERROR "nsx-psram does not yet have an MSPI mapping for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
        endif()
    endif()
    # The populated PSRAM part differs by exact board even though all four
    # currently share the same AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA BSP
    # presence macro: apollo510_evb (regular, non-Blue) is populated with an
    # AP Memory APS512XXB part and requires the "BA" (1.2V) driver family,
    # while apollo510b_evb is populated with an APS512XXN part and requires
    # the "N" driver family. This split matches old neuralSPOT's
    # ns_psram.c, which explicitly branches its driver function selection
    # on `#ifdef apollo510_evb` for exactly this reason. Using the wrong
    # driver for a given board still lets am_devices_mspi_psram_*_ddr_init()
    # succeed, but produces corrupted (non-zero mismatch) data on readback.
    #
    # NOTE: each vendored AmbiqSuite BSP's bsp_pins.src has a comment naming
    # the exact populated part per board (e.g. "Hex AP PSRAM APS256XXN-..."),
    # but this comment is UNRELIABLE: on apollo510_evb/apollo510b_evb it is
    # backwards relative to the driver actually confirmed correct on real
    # hardware (apollo510_evb's comment claims an "N" part despite requiring
    # the BA driver; apollo510b_evb's comment claims a "B" part despite
    # requiring the N driver). Do not use bsp_pins.src comments as evidence
    # for any board's driver requirement -- only real hardware validation
    # (or explicit product-team confirmation) should decide this.
    #
    # apollo330mP_evb has NOT been hardware-validated, but per product-team
    # guidance it is very similar to apollo510b_evb, so it is assigned the
    # "N" driver here on that basis. apollo510dL_evb has no such guidance and
    # no hardware validation, so it is deliberately NOT defaulted either way:
    # NSX_PSRAM_USE_BA_DRIVER must be set explicitly by the caller (e.g. via
    # -DNSX_PSRAM_USE_BA_DRIVER=0/1) until it is validated one way or the
    # other, at which point it should move into one of the explicit branches
    # below like apollo330mP_evb did.
    if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510_evb")
        set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
        set(NSX_PSRAM_USE_BA_DRIVER 1)
    elseif(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510b_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo330mP_evb")
        set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616n.c)
        set(NSX_PSRAM_USE_BA_DRIVER 0)
    elseif(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510dL_evb")
        if(NOT DEFINED NSX_PSRAM_USE_BA_DRIVER)
            message(FATAL_ERROR "nsx-psram has no validated PSRAM driver choice for apollo510dL_evb yet. Pass -DNSX_PSRAM_USE_BA_DRIVER=0 (N driver) or =1 (BA driver) explicitly once this has been confirmed on real hardware, then update this CMakeLists.txt to add an explicit branch for it.")
        endif()
        if(NSX_PSRAM_USE_BA_DRIVER)
            set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
        else()
            set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616n.c)
        endif()
    else()
        message(FATAL_ERROR "nsx-psram does not yet have a PSRAM driver mapping for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
    endif()
    if(NOT DEFINED NSX_PSRAM_DEVICE_SIZE_BYTES)
        set(NSX_PSRAM_DEVICE_SIZE_BYTES 33554432u)
    endif()
else()
    message(FATAL_ERROR "Unsupported NSX_SOC_FAMILY for nsx-psram: ${NSX_SOC_FAMILY}.")
endif()

if(NSX_PSRAM_ARCH_DIR STREQUAL "apollo3")
    set(NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS
        NSX_PSRAM_DEVICE_SIZE_BYTES=${NSX_PSRAM_DEVICE_SIZE_BYTES}
    )
elseif(NSX_PSRAM_ARCH_DIR STREQUAL "apollo5")
    set(NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS
        NSX_PSRAM_MSPI_MODULE=${NSX_PSRAM_MSPI_MODULE}
        NSX_PSRAM_DEVICE_SIZE_BYTES=${NSX_PSRAM_DEVICE_SIZE_BYTES}
        NSX_PSRAM_USE_BA_DRIVER=${NSX_PSRAM_USE_BA_DRIVER}
    )
else()
    set(NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS
        NSX_PSRAM_MSPI_MODULE=${NSX_PSRAM_MSPI_MODULE}
        NSX_PSRAM_DEVICE_SIZE_BYTES=${NSX_PSRAM_DEVICE_SIZE_BYTES}
    )
endif()

add_library(nsx_psram STATIC
    ${CMAKE_CURRENT_LIST_DIR}/src/nsx_psram.c
    ${CMAKE_CURRENT_LIST_DIR}/src/${NSX_PSRAM_ARCH_DIR}/nsx_psram.c
    ${NSX_PSRAM_DEVICE_SOURCE}
)
set_target_properties(nsx_psram PROPERTIES EXPORT_NAME psram)
add_library(nsx::psram ALIAS nsx_psram)

target_link_libraries(nsx_psram PUBLIC
    nsx::board_flags
    nsx::soc_hal
    nsx::core
    nsx::interrupt
)

target_compile_definitions(nsx_psram PRIVATE ${NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS})

target_include_directories(nsx_psram PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-psram/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-psram
)

install(TARGETS
    nsx_psram
    EXPORT nsxTargets
)
