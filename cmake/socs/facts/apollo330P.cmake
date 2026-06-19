# Single source of truth for apollo330P SoC facts.
#
# Side-effect free: only set() of NSX_SOC_* facts and the NSX toolchain
# selectors (NSX_CPU / NSX_FPU / NSX_FLOAT_ABI / NSX_ABI_FLAGS). No targets,
# no includes. Consumed by cmake/socs/apollo330P.cmake and by downstream
# board.cmake files via nsx_load_soc_facts("apollo330P").
set(NSX_SOC_FAMILY "apollo330P")
set(NSX_SOC_SERIES "apollo330")
set(NSX_SOC_SKEW "apollo330P")
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_ARCH_CLASS "armv8_1m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE TRUE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "armv8m")
set(NSX_SOC_CAPABILITIES core:m55 isa:armv8.1-m dsp mve fpu pmu:armv8m)
set(NSX_SOC_RTOS_PORT_FAMILY "AMapollo5")
set(NSX_SOC_RTOS_PORT_GENERIC "ARM_CM55_NTZ")

set(NSX_CPU "cortex-m55")
set(NSX_FLOAT_ABI "hard")
# Armv8.1-M (Cortex-M55) selects its FPU/MVE unit from -mcpu, so no separate
# -mfpu flag is emitted. NSX_FPU is declared (empty) at the SoC layer so the
# core's FPU spelling is a SoC fact rather than a per-board decision.
set(NSX_FPU "")
set(NSX_ABI_FLAGS "thumbv8.1m-fpv5-hard")

# SoC compile definitions (single source) consumed by nsx_soc_flags_target();
# see ../../nsx_soc_facts.cmake. Each is gated to C by the helper.
set(NSX_SOC_COMPILE_DEFINITIONS
    PART_apollo330P
    AM_PART_APOLLO330P
    ARMCM55
    __FPU_PRESENT
    NSX_SOC_CORE_M55=1
    NSX_SOC_HAS_DSP=1
    NSX_SOC_HAS_MVE=1
    NSX_SOC_HAS_FPU=1
    NSX_SOC_PMU_ARMV8M=1
)

# SEGGER / J-Link configuration (SoC-level defaults).
#
# NSX_SEGGER_DEVICE and NSX_SEGGER_PF_ADDR are package/board-specific defaults; a
# board.cmake may override NSX_SEGGER_DEVICE for its exact silicon package (e.g.
# a -KXR vs -KBR part). NSX_SEGGER_CPUFREQ is the TPIU *trace clock*, used by the
# SWO viewer to derive the baud scaler, not the CPU clock.
set(NSX_SEGGER_CPU "Cortex-M55")
set(NSX_SEGGER_DEVICE "AP330MPA-CBR")
set(NSX_SEGGER_IF_SPEED "4000")
set(NSX_SEGGER_PF_ADDR "0x00410000")
set(NSX_SEGGER_CPUFREQ "48000000")
set(NSX_SEGGER_SWOFREQ "1000000")
