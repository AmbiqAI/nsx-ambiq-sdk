set(NSX_SOC_FAMILY "apollo510b")
set(NSX_SOC_SERIES "apollo5")
set(NSX_SOC_SKEW "apollo510b")
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_ARCH_CLASS "armv8_1m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite-r5")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE TRUE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "armv8m")
set(NSX_SOC_CAPABILITIES core:m55 isa:armv8.1-m dsp mve fpu pmu:armv8m)

set(NSX_CPU "cortex-m55")
set(NSX_FLOAT_ABI "hard")
set(NSX_ABI_FLAGS "thumbv8.1m-fpv5-hard")

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite-r5")
    message(FATAL_ERROR
        "apollo510b requires NSX_SDK_PROVIDER=ambiqsuite-r5, got '${NSX_SDK_PROVIDER}'."
    )
endif()

set(NSX_AMBIQ_PART_NAME "apollo510b")
set(NSX_AMBIQ_MCU_DIR "${NSX_AMBIQSUITE_ROOT}/mcu/apollo510")
set(NSX_AMBIQ_HAL_LIB_PART_NAME "apollo510")
set(NSX_AMBIQ_HAL_DIR "${NSX_AMBIQ_MCU_DIR}/hal")
set(NSX_AMBIQ_HAL_MCU_DIR "${NSX_AMBIQ_HAL_DIR}/mcu")

include("${NSX_CMAKE_DIR}/nsx_toolchain_flags.cmake")

nsx_toolchain_is_armclang(NSX_TOOLCHAIN_IS_ARMCLANG)
if(NSX_TOOLCHAIN_IS_ARMCLANG)
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo510b/armclang/startup_keil6.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo510.c")
    set(NSX_LINKER_SCRIPT "${NSX_ROOT}/modules/nsx-core/src/apollo510b/armclang/linker_script_sbl.sct")
else()
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/apollo510b/gcc/startup_gcc.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_apollo510.c")
    set(NSX_LINKER_SCRIPT "${NSX_ROOT}/modules/nsx-core/src/apollo510b/gcc/linker_script_sbl.ld")
endif()

include("${NSX_CMAKE_DIR}/segger/socs/apollo5.cmake")

set(NSX_SOC_TARGET nsx_soc_apollo510b)
set(NSX_SOC_FLAGS_TARGET nsx_soc_apollo510b_flags)
set(NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME "soc_apollo510b")
set(NSX_SOC_FLAGS_TARGET_EXPORT_NAME "soc_flags_apollo510b")
set(NSX_SOC_TARGET_EXPORT_NAME "soc_hal_apollo510b")
set(NSX_STARTUP_TARGET_EXPORT_NAME "startup_apollo510b")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

if(NOT TARGET ${NSX_SOC_FLAGS_TARGET})
    add_library(${NSX_SOC_FLAGS_TARGET} INTERFACE)
    set_target_properties(${NSX_SOC_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_FLAGS_TARGET_EXPORT_NAME})
    target_compile_definitions(${NSX_SOC_FLAGS_TARGET} INTERFACE
        PART_apollo510b
        AM_PART_APOLLO5B
        AM_PART_APOLLO510
        AM_PART_APOLLO510B
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
if(NOT TARGET nsx::soc_apollo510b)
    add_library(nsx::soc_apollo510b ALIAS ${NSX_SOC_TARGET})
endif()

install(TARGETS
    ${NSX_SOC_TARGET}
    ${NSX_SOC_FLAGS_TARGET}
    EXPORT nsxTargets
)
