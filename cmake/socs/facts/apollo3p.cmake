# Single source of truth for apollo3p SoC facts.
#
# Side-effect free: only set() of NSX_SOC_* facts and the NSX toolchain
# selectors (NSX_CPU / NSX_FPU / NSX_FLOAT_ABI / NSX_ABI_FLAGS). No targets,
# no includes. Consumed by cmake/socs/apollo3p.cmake and by downstream
# board.cmake files via nsx_load_soc_facts("apollo3p").
set(NSX_SOC_FAMILY "apollo3p")
set(NSX_SOC_SERIES "apollo3")
set(NSX_SOC_SKEW "apollo3p")
set(NSX_SOC_CORE "cortex-m4")
set(NSX_SOC_ARCH_CLASS "armv7e_m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE FALSE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "none")
set(NSX_SOC_CAPABILITIES core:m4 isa:armv7e-m dsp fpu)
set(NSX_SOC_RTOS_PORT_FAMILY "AMapollo")
set(NSX_SOC_RTOS_PORT_GENERIC "ARM_CM4F")

set(NSX_CPU "cortex-m4")
set(NSX_FLOAT_ABI "hard")
# Armv7E-M (Cortex-M4F) requires an explicit -mfpu spelling; this is a function
# of the core, so it is declared here at the SoC layer rather than per board.
set(NSX_FPU "fpv4-sp-d16")
set(NSX_ABI_FLAGS "thumbv7em-fpv4sp-hard")

# SoC compile definitions (single source) consumed by nsx_soc_flags_target();
# see ../../nsx_soc_facts.cmake. Each is gated to C by the helper.
set(NSX_SOC_COMPILE_DEFINITIONS
    PART_apollo3p
    AM_PART_APOLLO3P
    ARMCM4
    __FPU_PRESENT
    NSX_SOC_CORE_M4=1
    NSX_SOC_HAS_DSP=1
    NSX_SOC_HAS_MVE=0
    NSX_SOC_HAS_FPU=1
    NSX_SOC_PMU_ARMV8M=0
)

# SEGGER / J-Link configuration (SoC-level defaults).
#
# NSX_SEGGER_DEVICE and NSX_SEGGER_PF_ADDR are package/board-specific defaults; a
# board.cmake may override NSX_SEGGER_DEVICE for its exact silicon package (e.g.
# a -KXR vs -KBR part). NSX_SEGGER_CPUFREQ is the TPIU *trace clock*, used by the
# SWO viewer to derive the baud scaler, not the CPU clock.
set(NSX_SEGGER_CPU "Cortex-M4")
set(NSX_SEGGER_DEVICE "AMA3B2KK-KBR")
set(NSX_SEGGER_IF_SPEED "4000")
set(NSX_SEGGER_PF_ADDR "0xC000")
set(NSX_SEGGER_CPUFREQ "48000000")
set(NSX_SEGGER_SWOFREQ "1000000")
