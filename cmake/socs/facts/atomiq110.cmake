# Single source of truth for atomiq110 SoC facts.
#
# Side-effect free: only set() of NSX_SOC_* facts and the NSX toolchain
# selectors (NSX_CPU / NSX_FPU / NSX_FLOAT_ABI / NSX_ABI_FLAGS). No targets,
# no includes. Consumed by cmake/socs/atomiq110.cmake and by downstream
# board.cmake files via nsx_load_soc_facts("atomiq110").
#
# atomiq110 is the Atomiq (R6 generation) Cortex-M55 SoC. The only upstream
# realization today is the atomiq110_fpga_turbo FPGA board; FPGA-specific
# overrides (clock, SEGGER device, peripheral availability) live in
# boards/atomiq110_fpga_turbo/board.cmake, not here.
set(NSX_SOC_FAMILY "atomiq110")
set(NSX_SOC_SERIES "atomiq")
set(NSX_SOC_SKEW "atomiq110")
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_ARCH_CLASS "armv8_1m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE TRUE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "armv8m")
set(NSX_SOC_CAPABILITIES core:m55 isa:armv8.1-m dsp mve fpu pmu:armv8m npu)
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

# SoC compile definitions (single source) consumed by nsx_soc_flags_target();
# see ../../nsx_soc_facts.cmake. Each is gated to C by the helper.
#
# Unlike the Apollo parts, the atomiq110 HAL is selected by its mcu/ directory
# rather than an AM_PART_* gate, so no AM_PART_* macro is injected here; the
# device header (am_mcu_apollo.h) owns AM_PART_ATOMIQ11X_API. PART_atomiq110 is
# the nsx-side selector for portable code.
set(NSX_SOC_COMPILE_DEFINITIONS
    PART_atomiq110
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
# Sourced from the AmbiqSuite atomiq110 project templates (the source of truth):
# scripts/templates/atomiq110/keil6/JLinkSettings.ini.template declares the
# J-Link device as "Atomiq110" (CPU Cortex-M55F, WorkRAM at 0x20000000), and
# the bare_gcc flash.jlink template connects over SWD at 4000 kHz. The FPGA
# "turbo" core runs at 25 MHz (the ATOMIQ11X_FPGA value baked into the HAL
# header), which sets NSX_SEGGER_CPUFREQ. PF_ADDR is the MRAM/flash base: on the
# FPGA, MRAM is emulated at 0x22000000 (the app IROM base in the keil6
# jlink.project and the J-Link gStartAddr), not the Apollo5 silicon 0x00400000.
# boards/atomiq110_fpga_turbo/board.cmake may override these for board-specific
# debug probes.
set(NSX_SEGGER_CPU "Cortex-M55")
set(NSX_SEGGER_DEVICE "Atomiq110")
set(NSX_SEGGER_IF_SPEED "4000")
set(NSX_SEGGER_PF_ADDR "0x22000000")
set(NSX_SEGGER_CPUFREQ "25000000")
set(NSX_SEGGER_SWOFREQ "1000000")
