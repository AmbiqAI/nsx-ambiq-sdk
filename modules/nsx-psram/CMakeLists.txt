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
    if(NOT NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4p_evb_disp_shield_rev2")
        message(FATAL_ERROR "nsx-psram has no supported Apollo4 PSRAM board staged for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
    endif()
    if(NOT DEFINED NSX_PSRAM_MSPI_MODULE)
        set(NSX_PSRAM_MSPI_MODULE 0)
    endif()
    if(NOT DEFINED NSX_PSRAM_DEVICE_CONFIG)
        set(NSX_PSRAM_DEVICE_CONFIG AM_BSP_MSPI_PSRAM_MODULE_HEX_DDR_CE)
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
    # All Apollo5-class EVBs staged here share the same
    # AM_BSP_MSPI_PSRAM_DEVICE_APS25616BA BSP presence macro and are populated
    # with an AP Memory APS512-class Hex (x16) DDR PSRAM on MSPI0. The correct
    # AmbiqSuite driver family for these parts is the "BA" (1.2V) family
    # (am_devices_mspi_psram_aps25616ba_1p2v).
    #
    # HARDWARE-VALIDATED (apollo510b_evb rev 2.0, MSPI0, J-Link probe
    # 1160002954, 2026-07). Findings, from most to least decisive:
    #
    #  * DDR timing calibration is the clean discriminator. The "N" driver's
    #    am_devices_mspi_psram_aps25616n_ddr_init_timing_check() reports "no valid
    #    setting" across its ENTIRE RXDQS sweep on this board -- it cannot find a
    #    working DDR read window at all. Verified both standalone and end-to-end
    #    through heliaPROFILER (building the profiler firmware with the N driver +
    #    scan makes PSRAM init fail with HPX_ERROR=psram_init_failed). The "BA"
    #    driver's scan SUCCEEDS on the same board (valid RXDQS window), so the BA
    #    driver's timing model matches this silicon and the N driver's does not.
    #
    #  * The N driver is therefore only MARGINALLY functional here. With the scan
    #    skipped, plain am_devices_mspi_psram_aps25616n_ddr_init() at 48 MHz can
    #    still bring XIP up and, at room temperature, read back correct data (a
    #    52 KB model was observed byte-exact over XIP in one session). But because
    #    the part has no valid calibrated window under the N driver, those reads
    #    run uncalibrated and are not robust -- this matches the field report of
    #    INTERMITTENT PSRAM corruption / HardFaults ("worked sometimes, corrupted
    #    others; DDR flaky, SDR was more reliable"). Do not rely on it.
    #
    #  * The BA driver round-trips this board's memory with ZERO mismatches (walking
    #    patterns across the aperture, CPU and SWD), and reports the true device
    #    size of 65536 KB = 64 MB. The 64 MB size was independently confirmed by an
    #    XIP address-aliasing test: 0x62000000 (+32 MB) holds data distinct from
    #    0x60000000 and 0x63FFFFF0 (+64 MB) is live, so the part does NOT wrap at
    #    32 MB. APS512 = 512 Mbit = 64 MB. So for apollo510b_evb the correct driver
    #    is BA and the correct size is 64 MB (not 32 MB).
    #
    # nsx-psram additionally now runs the DDR timing scan by default on Apollo5
    # (NSX_PSRAM_RUN_DDR_TIMING_SCAN, see nsx_psram.c) so DDR reads are calibrated
    # rather than left at the marginal power-on default.
    #
    # This CONTRADICTS old neuralSPOT's ns_psram.c (which branched N-vs-BA on
    # `#ifdef apollo510_evb`) and the earlier assumption recorded here. The
    # bsp_pins.src part-name comments remain UNRELIABLE and must not be used as
    # evidence -- only real hardware validation should decide the driver. (Note the
    # QSG names the fitted part "APS512XXN...", an "N"-suffix marking, yet the BA
    # driver is the correct one: part marking is NOT the driver family.)
    #
    # apollo510_evb (regular, non-Blue) was already defaulted to the BA driver
    # here and remains so (it has not been re-validated in this session); its
    # size default is left at the conservative Apollo5 fallback (32 MB) below
    # since only apollo510b_evb's 64 MB size has been confirmed on hardware.
    #
    # apollo330mP_evb has NOT been hardware-validated, but per product-team
    # guidance it is very similar to apollo510b_evb, so it tracks apollo510b_evb
    # and is assigned the "BA" driver (previously "N", on the same similarity
    # rationale, before apollo510b_evb itself was validated as BA). apollo510dL_evb
    # has no such guidance and no hardware validation, so it is deliberately NOT
    # defaulted either way: NSX_PSRAM_USE_BA_DRIVER must be set explicitly by the
    # caller (e.g. via -DNSX_PSRAM_USE_BA_DRIVER=0/1) until it is validated one way
    # or the other, at which point it should move into one of the explicit branches
    # below like apollo330mP_evb did.
    if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510_evb")
        set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
        set(NSX_PSRAM_USE_BA_DRIVER 1)
    elseif(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo510b_evb")
        # apollo510b_evb: BA driver + 64 MB, hardware-validated (see note above).
        set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
        set(NSX_PSRAM_USE_BA_DRIVER 1)
        if(NOT DEFINED NSX_PSRAM_DEVICE_SIZE_BYTES)
            set(NSX_PSRAM_DEVICE_SIZE_BYTES 67108864u)
        endif()
    elseif(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo330mP_evb")
        # apollo330mP_evb: driver family inferred from apollo510b_evb
        # similarity, NOT hardware-validated. Size deliberately stays at the
        # conservative 32 MB default until proven on hardware — advertising
        # 64 MB without validation would let callers place data in an upper
        # half that may alias or fault.
        set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
        set(NSX_PSRAM_USE_BA_DRIVER 1)
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
    if(NOT DEFINED NSX_PSRAM_RUN_DDR_TIMING_SCAN)
        set(NSX_PSRAM_RUN_DDR_TIMING_SCAN 1)
    endif()
    set(NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS
        NSX_PSRAM_MSPI_MODULE=${NSX_PSRAM_MSPI_MODULE}
        NSX_PSRAM_DEVICE_SIZE_BYTES=${NSX_PSRAM_DEVICE_SIZE_BYTES}
        NSX_PSRAM_USE_BA_DRIVER=${NSX_PSRAM_USE_BA_DRIVER}
        NSX_PSRAM_RUN_DDR_TIMING_SCAN=${NSX_PSRAM_RUN_DDR_TIMING_SCAN}
    )
else()
    set(NSX_PSRAM_EXTRA_COMPILE_DEFINITIONS
        NSX_PSRAM_MSPI_MODULE=${NSX_PSRAM_MSPI_MODULE}
        NSX_PSRAM_DEVICE_CONFIG=${NSX_PSRAM_DEVICE_CONFIG}
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
