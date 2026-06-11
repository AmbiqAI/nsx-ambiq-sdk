set(NSX_SOC_FAMILY "apollo2")
set(NSX_SOC_SERIES "apollo2")
set(NSX_SOC_SKEW "apollo2")
set(NSX_SOC_CORE "cortex-m4")
set(NSX_SOC_ARCH_CLASS "armv7e_m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite-r2")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE FALSE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "none")
set(NSX_SOC_CAPABILITIES core:m4 isa:armv7e-m dsp fpu)
set(NSX_SOC_RTOS_PORT_FAMILY "AMapollo2")
set(NSX_SOC_RTOS_PORT_GENERIC "ARM_CM4F")

set(NSX_CPU "cortex-m4")
set(NSX_FLOAT_ABI "hard")
set(NSX_ABI_FLAGS "thumbv7em-fpv4-hard")

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r2")
    message(FATAL_ERROR
        "apollo2 requires NSX_SDK_PROVIDER=ambiqsuite-r2, got '${NSX_SDK_PROVIDER}'."
    )
endif()

set(NSX_AMBIQ_PART_NAME "apollo2")
set(NSX_AMBIQ_MCU_DIR "${NSX_AMBIQSUITE_ROOT}/mcu/${NSX_AMBIQ_PART_NAME}")
set(NSX_AMBIQ_HAL_DIR "${NSX_AMBIQ_MCU_DIR}/hal")

include("${NSX_CMAKE_DIR}/nsx_toolchain_flags.cmake")

nsx_toolchain_is_armclang(NSX_TOOLCHAIN_IS_ARMCLANG)
if(NSX_TOOLCHAIN_IS_ARMCLANG)
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo2/armclang/startup_keil6.s")
    set(NSX_LINKER_SCRIPT "${NSX_ROOT}/modules/nsx-core/src/apollo2/armclang/linker_script.sct")
else()
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo2/gcc/startup_gcc.c")
    set(NSX_LINKER_SCRIPT "${NSX_ROOT}/modules/nsx-core/src/apollo2/gcc/linker_script.ld")
endif()
set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo2.c")

include("${NSX_CMAKE_DIR}/segger/socs/apollo2.cmake")

set(NSX_SOC_TARGET nsx_soc_apollo2)
set(NSX_SOC_FLAGS_TARGET nsx_soc_apollo2_flags)
set(NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME "soc_apollo2")
set(NSX_SOC_FLAGS_TARGET_EXPORT_NAME "soc_flags_apollo2")
set(NSX_SOC_TARGET_EXPORT_NAME "soc_hal_apollo2")
set(NSX_STARTUP_TARGET_EXPORT_NAME "startup_apollo2")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

if(NOT TARGET ${NSX_SOC_FLAGS_TARGET})
    add_library(${NSX_SOC_FLAGS_TARGET} INTERFACE)
    set_target_properties(${NSX_SOC_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_FLAGS_TARGET_EXPORT_NAME})
    target_compile_definitions(${NSX_SOC_FLAGS_TARGET} INTERFACE
        $<$<COMPILE_LANGUAGE:C>:PART_apollo2>
        $<$<COMPILE_LANGUAGE:C>:AM_PART_APOLLO2>
        $<$<COMPILE_LANGUAGE:C>:ARMCM4>
        $<$<COMPILE_LANGUAGE:C>:__FPU_PRESENT>
        $<$<COMPILE_LANGUAGE:C>:NSX_SOC_CORE_M4=1>
        $<$<COMPILE_LANGUAGE:C>:NSX_SOC_HAS_DSP=1>
        $<$<COMPILE_LANGUAGE:C>:NSX_SOC_HAS_MVE=0>
        $<$<COMPILE_LANGUAGE:C>:NSX_SOC_HAS_FPU=1>
        $<$<COMPILE_LANGUAGE:C>:NSX_SOC_PMU_ARMV8M=0>
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
if(NOT TARGET nsx::soc_apollo2)
    add_library(nsx::soc_apollo2 ALIAS ${NSX_SOC_TARGET})
endif()

install(TARGETS
    ${NSX_SOC_TARGET}
    ${NSX_SOC_FLAGS_TARGET}
    EXPORT nsxTargets
)
