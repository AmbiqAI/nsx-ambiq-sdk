# SoC facts (NSX_SOC_* + NSX_CPU/NSX_FPU/NSX_FLOAT_ABI/NSX_ABI_FLAGS) are the
# single source of truth shared with downstream board.cmake files; see facts/.
include("${CMAKE_CURRENT_LIST_DIR}/facts/apollo330P.cmake")

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR
        "apollo330P requires NSX_SDK_PROVIDER=ambiqsuite, got '${NSX_SDK_PROVIDER}'."
    )
endif()

set(NSX_AMBIQ_PART_NAME "apollo330P")
set(NSX_AMBIQ_MCU_DIR "${NSX_AMBIQSUITE_ROOT}/mcu/${NSX_AMBIQ_PART_NAME}")
set(NSX_AMBIQ_HAL_DIR "${NSX_AMBIQ_MCU_DIR}/hal")
set(NSX_AMBIQ_HAL_MCU_DIR "${NSX_AMBIQ_HAL_DIR}/mcu")

include("${NSX_CMAKE_DIR}/nsx_toolchain_flags.cmake")
include("${NSX_CMAKE_DIR}/nsx_soc_facts.cmake")

nsx_toolchain_is_armclang(NSX_TOOLCHAIN_IS_ARMCLANG)
if(NSX_TOOLCHAIN_IS_ARMCLANG)
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo330P/armclang/startup_keil6.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo330P.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/apollo330P/armclang/linker_script_sbl.sct")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/apollo330P/armclang/linker_script_itcm_sbl.sct")
    set(_nsx_linker_script_nbl "${NSX_ROOT}/modules/nsx-core/src/apollo330P/armclang/linker_script_nbl.sct")
else()
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo330P/gcc/startup_gcc.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo330P.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/apollo330P/gcc/linker_script_sbl.ld")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/apollo330P/gcc/linker_script_itcm_sbl.ld")
    set(_nsx_linker_script_nbl "${NSX_ROOT}/modules/nsx-core/src/apollo330P/gcc/linker_script_nbl.ld")
endif()

if(NOT DEFINED NSX_LINKER_SCRIPT)
    nsx_select_linker_script(
        DEFAULT "${_nsx_linker_script_default}"
        ITCM "${_nsx_linker_script_itcm}"
    )
endif()

# The GCC/ATfE startup copies the load region selected by the linker profile.
# Keep the startup's linker-symbol references aligned with the active script.
set(NSX_STARTUP_COPY_ITCM_TEXT FALSE)
if(NSX_LINKER_SCRIPT STREQUAL "${_nsx_linker_script_itcm}" OR
   NSX_LINKER_SCRIPT STREQUAL "${_nsx_linker_script_nbl}")
    set(NSX_STARTUP_COPY_ITCM_TEXT TRUE)
endif()

set(NSX_SOC_TARGET nsx_soc_apollo330P)
set(NSX_SOC_FLAGS_TARGET nsx_soc_apollo330P_flags)
set(NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME "soc_apollo330P")
set(NSX_SOC_FLAGS_TARGET_EXPORT_NAME "soc_flags_apollo330P")
set(NSX_SOC_TARGET_EXPORT_NAME "soc_hal_apollo330P")
set(NSX_STARTUP_TARGET_EXPORT_NAME "startup_apollo330P")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

if(NOT TARGET ${NSX_SOC_FLAGS_TARGET})
    nsx_soc_flags_target(${NSX_SOC_FLAGS_TARGET})
    set_target_properties(${NSX_SOC_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_FLAGS_TARGET_EXPORT_NAME})
    nsx_apply_toolchain_flags(${NSX_SOC_FLAGS_TARGET})
    if(NSX_STARTUP_COPY_ITCM_TEXT)
        target_compile_definitions(${NSX_SOC_FLAGS_TARGET} INTERFACE
            NSX_STARTUP_COPY_ITCM_TEXT=1
        )
    endif()
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
if(NOT TARGET nsx::soc_apollo330P)
    add_library(nsx::soc_apollo330P ALIAS ${NSX_SOC_TARGET})
endif()

install(TARGETS
    ${NSX_SOC_TARGET}
    ${NSX_SOC_FLAGS_TARGET}
    EXPORT nsxTargets
)
