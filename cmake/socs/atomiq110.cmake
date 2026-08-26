# SoC facts (NSX_SOC_* + NSX_CPU/NSX_FPU/NSX_FLOAT_ABI/NSX_ABI_FLAGS) are the
# single source of truth shared with downstream board.cmake files; see facts/.
include("${CMAKE_CURRENT_LIST_DIR}/facts/atomiq110.cmake")

if(NOT NSX_SDK_PROVIDER STREQUAL "ambiqsuite")
    message(FATAL_ERROR
        "atomiq110 requires NSX_SDK_PROVIDER=ambiqsuite, got '${NSX_SDK_PROVIDER}'."
    )
endif()

# Deadline for nsx-ethos-u-driver's Ethos-U85 inference wait, in milliseconds.
#
# The driver defaults NSX_ETHOSU_INFERENCE_TIMEOUT_MS to empty, which compiles
# upstream's ETHOSU_SEMAPHORE_WAIT_FOREVER into ethosu_driver.c: a wedged NPU
# then hangs the caller indefinitely instead of returning
# ETHOSU_JOB_RESULT_TIMEOUT and taking the ethosu_soft_reset() recovery path.
#
# It has to be seeded here rather than in modules/nsx-npu/CMakeLists.txt.
# nsx-ethos-u-driver is an external module materialized by the neuralspotx
# registry and consumes this value when *it* is configured, which happens before
# nsx-npu is added (nsx-npu only requires the resulting nsx::ethos_u_driver
# target). This SoC file is the earliest atomiq110-scoped hook in the flow.
#
# 5000 ms is a deliberately generous bring-up value: far beyond any plausible
# Ethos-U85 inference on this part, so it only ever fires on a genuine wedge.
# Override with -DNSX_ETHOSU_INFERENCE_TIMEOUT_MS=<ms>, or with "" to restore
# upstream's wait-forever behaviour.
#
# The timeout is inert unless a timebase is also supplied; nsx-npu provides the
# nsx_ethos_u_ticks()/nsx_ethos_u_ticks_per_ms() strong overrides for that.
if(NOT DEFINED NSX_ETHOSU_INFERENCE_TIMEOUT_MS)
    set(NSX_ETHOSU_INFERENCE_TIMEOUT_MS "5000" CACHE STRING
        "Inference-wait timeout in ms for ethosu_wait() (empty = wait forever)")
endif()

set(NSX_AMBIQ_PART_NAME "atomiq110")
set(NSX_AMBIQ_MCU_DIR "${NSX_AMBIQSUITE_ROOT}/mcu/${NSX_AMBIQ_PART_NAME}")
set(NSX_AMBIQ_HAL_LIB_PART_NAME "atomiq110")
set(NSX_AMBIQ_HAL_DIR "${NSX_AMBIQ_MCU_DIR}/hal")
set(NSX_AMBIQ_HAL_MCU_DIR "${NSX_AMBIQ_HAL_DIR}/mcu")

include("${NSX_CMAKE_DIR}/nsx_toolchain_flags.cmake")
include("${NSX_CMAKE_DIR}/nsx_soc_facts.cmake")

# The only atomiq110 board today is the FPGA (atomiq110_fpga_turbo), which loads
# directly via J-Link with no secure bootloader, so the default profile is "nbl"
# (app at the emulated MRAM base 0x22000000, matching the J-Link gStartAddr). The
# "sbl" scripts (app base 0x22010000) are kept for a future silicon AT110.
nsx_toolchain_is_armclang(NSX_TOOLCHAIN_IS_ARMCLANG)
if(NSX_TOOLCHAIN_IS_ARMCLANG)
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/atomiq110/armclang/startup_keil6.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_atomiq110.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/atomiq110/armclang/linker_script_nbl.sct")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/atomiq110/armclang/linker_script_itcm_nbl.sct")
else()
    set(NSX_STARTUP_SOURCE "${NSX_ROOT}/modules/nsx-core/src/atomiq110/gcc/startup_gcc.c")
    set(NSX_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/CMSIS/AmbiqMicro/Source/system_atomiq110.c")
    set(_nsx_linker_script_default "${NSX_ROOT}/modules/nsx-core/src/atomiq110/gcc/linker_script_nbl.ld")
    set(_nsx_linker_script_itcm "${NSX_ROOT}/modules/nsx-core/src/atomiq110/gcc/linker_script_itcm_nbl.ld")
endif()

if(NOT DEFINED NSX_LINKER_SCRIPT)
    nsx_select_linker_script(
        DEFAULT "${_nsx_linker_script_default}"
        ITCM "${_nsx_linker_script_itcm}"
    )
endif()
set(NSX_SOC_TARGET nsx_soc_atomiq110)
set(NSX_SOC_FLAGS_TARGET nsx_soc_atomiq110_flags)
set(NSX_SOC_DESCRIPTOR_TARGET_EXPORT_NAME "soc_atomiq110")
set(NSX_SOC_FLAGS_TARGET_EXPORT_NAME "soc_flags_atomiq110")
set(NSX_SOC_TARGET_EXPORT_NAME "soc_hal_atomiq110")
set(NSX_STARTUP_TARGET_EXPORT_NAME "startup_atomiq110")

nsx_assert_file_exists("${NSX_LINKER_SCRIPT}")
nsx_assert_file_exists("${NSX_STARTUP_SOURCE}")
nsx_assert_file_exists("${NSX_SYSTEM_SOURCE}")

if(NOT TARGET ${NSX_SOC_FLAGS_TARGET})
    nsx_soc_flags_target(${NSX_SOC_FLAGS_TARGET})
    set_target_properties(${NSX_SOC_FLAGS_TARGET} PROPERTIES EXPORT_NAME ${NSX_SOC_FLAGS_TARGET_EXPORT_NAME})
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
if(NOT TARGET nsx::soc_atomiq110)
    add_library(nsx::soc_atomiq110 ALIAS ${NSX_SOC_TARGET})
endif()

install(TARGETS
    ${NSX_SOC_TARGET}
    ${NSX_SOC_FLAGS_TARGET}
    EXPORT nsxTargets
)
