# Single source of truth for apollo510 SoC facts.
#
# Side-effect free: only set() of NSX_SOC_* facts and the NSX toolchain
# selectors (NSX_CPU / NSX_FPU / NSX_FLOAT_ABI / NSX_ABI_FLAGS). No targets,
# no includes.
# Consumed by:
#   - cmake/socs/apollo510.cmake (the SDK's own board/SoC descriptor), and
#   - downstream board.cmake files via nsx_load_soc_facts("apollo510").
set(NSX_SOC_FAMILY "apollo510")
set(NSX_SOC_SERIES "apollo5")
set(NSX_SOC_SKEW "apollo510")
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_ARCH_CLASS "armv8_1m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite-r5")
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
# core's FPU spelling is a SoC fact rather than a per-board decision; Armv7E-M
# cores (e.g. Cortex-M4) set a concrete value such as "fpv4-sp-d16".
set(NSX_FPU "")
set(NSX_ABI_FLAGS "thumbv8.1m-fpv5-hard")
