# SoC facts (NSX_SOC_* + NSX_CPU/NSX_FPU/NSX_FLOAT_ABI/NSX_ABI_FLAGS) are the
# single source of truth shared with downstream board.cmake files; see _facts/.
include("${CMAKE_CURRENT_LIST_DIR}/_facts/apollo510.cmake")

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r5")
    message(FATAL_ERROR
        "apollo510 requires NSX_SDK_PROVIDER=ambiqsuite-r5, got '${NSX_SDK_PROVIDER}'."
    )
endif()

set(NSX_AMBIQ_PART_NAME "apollo510")
set(NSX_AMBIQ_MCU_DIR "${NSX_AMBIQSUITE_ROOT}/mcu/${NSX_AMBIQ_PART_NAME}")
set(NSX_AMBIQ_HAL_DIR "${NSX_AMBIQ_MCU_DIR}/hal")
set(NSX_AMBIQ_HAL_MCU_DIR "${NSX_AMBIQ_HAL_DIR}/mcu")

include("${NSX_CMAKE_DIR}/nsx_toolchain_flags.cmake")

nsx_toolchain_is_armclang(NSX_TOOLCHAIN_IS_ARMCLANG)
if(NSX_TOOLCHAIN_IS_ARMCLANG)
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo510/armclang/startup_keil6.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo510.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/apollo510/armclang/linker_script_sbl.sct")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/apollo510/armclang/linker_script_itcm_sbl.sct")
else()
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo510/gcc/startup_gcc.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo510.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/apollo510/gcc/linker_script_sbl.ld")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/apollo510/gcc/linker_script_itcm_sbl.ld")
endif()

if(NOT DEFINED NSX_LINKER_SCRIPT)
    nsx_select_linker_script(
        DEFAULT "${_nsx_linker_script_default}"
        ITCM "${_nsx_linker_script_itcm}"
    )
endif()

include("${NSX_CMAKE_DIR}/segger/socs/apollo5.cmake")

set(NSX_SOC_TARGET nsx_soc_apollo510)
set(NSX_SOC_FLAGS_TARGET nsx_soc_apollo510_flags)
set(NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME "soc_apollo510")
set(NSX_SOC_FLAGS_TARGET_EXPORT_NAME "soc_flags_apollo510")
set(NSX_SOC_TARGET_EXPORT_NAME "soc_hal_apollo510")
set(NSX_STARTUP_TARGET_EXPORT_NAME "startup_apollo510")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

if(NOT TARGET ${NSX_SOC_FLAGS_TARGET})
    add_library(${NSX_SOC_FLAGS_TARGET} INTERFACE)
    set_target_properties(${NSX_SOC_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_FLAGS_TARGET_EXPORT_NAME})
    target_compile_definitions(${NSX_SOC_FLAGS_TARGET} INTERFACE
        PART_apollo510
        AM_PART_APOLLO5B
        AM_PART_APOLLO510
        ARMCM55
        __FPU_PRESENT
        NSX_SOC_CORE_M55=1
        NSX_SOC_HAS_DSP=1
        NSX_SOC_HAS_MVE=1
        NSX_SOC_HAS_FPU=1
        NSX_SOC_PMU_ARMV8M=1
    )
    nsx_apply_toolchain_flags(${NSX_SOC_FLAGS_TARGET})
endif()

if(NOT TARGET ${NSX_SOC_TARGET})
    add_library(${NSX_SOC_TARGET} INTERFACE)
    set_target_properties(${NSX_SOC_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME})
    target_link_libraries(${NSX_SOC_TARGET} INTERFACE ${NSX_SOC_FLAGS_TARGET})
endif()

if(NOT TARGET nsx::soc_flags)
    add_library(nsx::soc_flags ALIAS ${NSX_SOC_FLAGS_TARGET})
endif()
if(NOT TARGET nsx::soc)
    add_library(nsx::soc ALIAS ${NSX_SOC_TARGET})
endif()
if(NOT TARGET nsx::soc_apollo510)
    add_library(nsx::soc_apollo510 ALIAS ${NSX_SOC_TARGET})
endif()

install(TARGETS
    ${NSX_SOC_TARGET}
    ${NSX_SOC_FLAGS_TARGET}
    EXPORT nsxTargets
)
