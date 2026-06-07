if(NOT TARGET nsx::soc_hal)
    message(FATAL_ERROR "nsx::soc_hal must be defined before nsx-psram is added.")
endif()
if(NOT TARGET nsx::board_flags)
    message(FATAL_ERROR "nsx::board_flags must be defined before nsx-psram is added.")
endif()
if(NOT DEFINED NSX_SOC_FAMILY)
    message(FATAL_ERROR "NSX_SOC_FAMILY must be defined before nsx-psram is added.")
endif()
if(NOT DEFINED NSX_AMBIQ_BOARD_NAME)
    message(FATAL_ERROR "NSX_AMBIQ_BOARD_NAME must be defined before nsx-psram is added.")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_core)
    set(NSX_PSRAM_CORE_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_core}")
else()
    set(NSX_PSRAM_CORE_DIR "${NSX_ROOT}/modules/nsx-core")
endif()

if(DEFINED NSX_MODULE_DIR_nsx_interrupt)
    set(NSX_PSRAM_INTERRUPT_DIR "${NSX_ROOT}/${NSX_MODULE_DIR_nsx_interrupt}")
else()
    set(NSX_PSRAM_INTERRUPT_DIR "${NSX_ROOT}/modules/nsx-interrupt")
endif()

nsx_select_soc_arch_dir(NSX_PSRAM_ARCH_DIR
    APOLLO3 apollo3
    APOLLO4 apollo4
    APOLLO5 apollo5
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
    if(NOT DEFINED NSX_PSRAM_MSPI_MODULE)
        if(NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4l_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4l_blue_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4p_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4p_blue_kbr_evb"
           OR NSX_AMBIQ_BOARD_NAME STREQUAL "apollo4p_blue_kxr_evb")
            set(NSX_PSRAM_MSPI_MODULE 0)
        else()
            message(FATAL_ERROR "nsx-psram does not yet have an MSPI mapping for NSX_AMBIQ_BOARD_NAME='${NSX_AMBIQ_BOARD_NAME}'.")
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
    set(NSX_PSRAM_DEVICE_SOURCE ${CMAKE_CURRENT_LIST_DIR}/src/apollo5/devices/am_devices_mspi_psram_aps25616ba_1p2v.c)
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
    $<BUILD_INTERFACE:${NSX_PSRAM_CORE_DIR}/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-psram/includes-api>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-core/includes-api>
)

target_include_directories(nsx_psram PRIVATE
    $<BUILD_INTERFACE:${NSX_PSRAM_INTERRUPT_DIR}/includes-api>
)

install(DIRECTORY
    ${CMAKE_CURRENT_LIST_DIR}/includes-api
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/nsx/modules/nsx-psram
)

install(TARGETS
    nsx_psram
    EXPORT nsxTargets
)
