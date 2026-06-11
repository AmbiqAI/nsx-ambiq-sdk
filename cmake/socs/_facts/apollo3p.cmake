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
set(NSX_SOC_SDK_PROVIDER "ambiqsuite-r3")
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
