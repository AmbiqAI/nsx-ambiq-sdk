# CMake Platform Contract

This repo uses CMake as the source of truth for platform selection. The contract
is intentionally split into toolchain, SoC, SoC skew, and board layers so custom
boards can reuse silicon definitions without inheriting EVB defaults.

## Selection Order

A build should resolve platform facts in this order:

1. Toolchain profile
2. SoC family
3. SoC skew or variant
4. Board profile
5. Application overrides

The board profile may select a default SoC skew, but generic modules should not
require a board before they can learn what CPU, ISA, PMU, startup, or SDK part is
being used.

## Layer Responsibilities

| Layer | Owns | Must Not Own |
| --- | --- | --- |
| Toolchain | compiler family, language flags, linker flag syntax, ABI flag spelling | Ambiq part names, board names, pin maps |
| SoC family | core class, architecture class, common SDK provider, shared HAL layout, common capabilities | EVB package defaults, board peripherals |
| SoC skew | Ambiq part define, package-independent silicon differences, memory map, startup source, system source, default linker script, capability overrides | connector pin maps, buttons, LEDs, board sensors |
| Board | BSP board name, package, pins, default buses, external flash/PSRAM, debug/flash defaults, board power constraints | CPU/FPU selection, generic PMU/MVE/DSP capability facts |
| Application | final memory/linker overrides, selected transports, workload-specific options | reusable platform definitions |

## Canonical Targets

The long-term target shape should be:

```cmake
nsx::toolchain_flags  # compiler/linker syntax and ABI spelling
nsx::soc_flags        # CPU, FPU, ISA, part, and capability compile definitions
nsx::board_flags      # package, EVB/custom-board defines, pin/default-device facts
nsx::soc              # selected SoC/skew descriptor target
nsx::board            # selected board descriptor target
```

Generic modules should prefer `nsx::soc_flags`. Board-specific modules and BSP
wrappers may use `nsx::board_flags`.

## SoC Families And Skews

A SoC family is a reusable silicon line, such as Apollo5 or Apollo330. A SoC
skew is a concrete part or derivative, such as Apollo510, Apollo510B, Apollo5B,
or Apollo330P. Skews may share implementation directories while still exposing
different part defines, memory maps, linker scripts, or capabilities.

The split should support definitions like:

```cmake
# family-level
set(NSX_SOC_FAMILY "apollo5")
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_ARCH_CLASS "armv8_1m")
set(NSX_SOC_SDK_PROVIDER "ambiqsuite-r5")
set(NSX_SOC_CAPABILITIES
    core:m55
    isa:armv8.1-m
    dsp
    mve
    fpu
    pmu:armv8m
)

# skew-level
set(NSX_SOC_SKEW "apollo510b")
set(NSX_AMBIQ_PART_NAME "apollo510b")
set(NSX_SOC_HAL_SOURCE_ROOT "${NSX_AMBIQSUITE_ROOT}/mcu/apollo510")
set(NSX_SOC_SYSTEM_SOURCE "${NSX_AMBIQSUITE_ROOT}/src/apollo510/system_apollo510.c")
set(NSX_SOC_LINKER_SCRIPT_GCC "${NSX_ROOT}/modules/nsx-core/src/apollo510b/gcc/linker_script_sbl.ld")
```

This matters because an Apollo510B skew can share Apollo510 HAL/system sources
while still carrying its own part define and linker script.

## Capabilities

Capabilities should be explicit CMake facts, not inferred from board names.
Recommended naming:

```cmake
set(NSX_SOC_CORE "cortex-m55")
set(NSX_SOC_HAS_DSP TRUE)
set(NSX_SOC_HAS_MVE TRUE)
set(NSX_SOC_HAS_FPU TRUE)
set(NSX_SOC_PMU_TIER "armv8m")
set(NSX_SOC_CAPABILITIES core:m55 dsp mve fpu pmu:armv8m)
```

Modules should gate implementation choices on capabilities when possible:

```cmake
if(NOT NSX_SOC_HAS_MVE)
    message(FATAL_ERROR "nsx-foo-mve requires NSX_SOC_HAS_MVE")
endif()
```

Compile definitions should be generated from the selected SoC/skew target rather
than repeated in board modules:

```cmake
target_compile_definitions(nsx_soc_apollo510b_flags INTERFACE
    AM_PART_APOLLO5B
    AM_PART_APOLLO510
    AM_PART_APOLLO510B
    ARMCM55
    __FPU_PRESENT
    NSX_SOC_HAS_DSP=1
    NSX_SOC_HAS_MVE=1
    NSX_SOC_HAS_FPU=1
    NSX_SOC_PMU_ARMV8M=1
)
```

## RTOS Port Selection

FreeRTOS (and any future RTOS kernel) is intentionally *not* part of the core
SDK. The kernel, heap implementation, and `FreeRTOSConfig.h` live in a separate
optional module (`nsx-freertos`) and the application. The core SDK only owns the
silicon-facing facts a port needs, so port selection stays data-driven instead of
hard-coded per application.

Each SoC descriptor publishes two RTOS facts:

| Variable | Meaning |
| --- | --- |
| `NSX_SOC_RTOS_PORT_FAMILY` | AmbiqSuite FreeRTOS port directory family for this silicon line: `AMapollo2`, `AMapollo`, `AMapollo4`, or `AMapollo5` |
| `NSX_SOC_RTOS_PORT_GENERIC` | Upstream CMSIS fallback port when an Ambiq/toolchain-specific port is unavailable: `ARM_CM4F` or `ARM_CM55_NTZ` |

Current mapping:

| SoC skew | Core | `NSX_SOC_RTOS_PORT_FAMILY` | `NSX_SOC_RTOS_PORT_GENERIC` |
| --- | --- | --- | --- |
| apollo2 | cortex-m4 | `AMapollo2` | `ARM_CM4F` |
| apollo3 | cortex-m4 | `AMapollo` | `ARM_CM4F` |
| apollo3p | cortex-m4 | `AMapollo` | `ARM_CM4F` |
| apollo4l | cortex-m4 | `AMapollo4` | `ARM_CM4F` |
| apollo4p | cortex-m4 | `AMapollo4` | `ARM_CM4F` |
| apollo5b | cortex-m55 | `AMapollo5` | `ARM_CM55_NTZ` |
| apollo510 | cortex-m55 | `AMapollo5` | `ARM_CM55_NTZ` |
| apollo510b | cortex-m55 | `AMapollo5` | `ARM_CM55_NTZ` |
| apollo510L | cortex-m55 | `AMapollo5` | `ARM_CM55_NTZ` |
| apollo330P | cortex-m55 | `AMapollo5` | `ARM_CM55_NTZ` |

Resolution rules the consuming RTOS module is expected to follow:

- The Ambiq port family is the preferred source when a port exists for the
  selected toolchain. AmbiqSuite ships these ports for `gcc`, `armclang`
  (Keil6), `iar`, and `segger`.
- `gcc` has no `AMapollo` (Apollo3) port; apollo3/apollo3p builds must fall back
  to `NSX_SOC_RTOS_PORT_GENERIC` (`ARM_CM4F`).
- `atfe` has no AmbiqSuite port tree at all; ATfE builds must consume the
  generic ports and validate that the GCC port assembly compiles under ATfE.
- These are facts only. The SoC descriptor does not add RTOS compile
  definitions, link any kernel, or claim a default tick source. Tick source,
  heap policy, and `FreeRTOSConfig.h` values are owned by the RTOS module and
  the application.

## Migration Rule

`NSX_BOARD_FLAGS_TARGET` currently contains both SoC and board facts. During the
migration, split it mechanically:

- Move CPU, FPU, ABI, part, PMU, MVE/DSP, startup, system source, and default
  linker script facts to `nsx::soc_flags` or `nsx::soc`.
- Keep package, EVB/custom-board macros, pin maps, default external devices,
  and debug/flash defaults in `nsx::board_flags` or `nsx::board`.
- Keep compiler-family flag spelling in `nsx::toolchain_flags`.

Once the split exists, generic modules should stop requiring
`NSX_BOARD_FLAGS_TARGET` and should instead require the narrow target they
actually need.

## AmbiqSuite Vendor Layout

The original AmbiqSuite zip is allowed to remain board-oriented. NSX should not
rewrite or mirror all vendor board code just to expose a cleaner platform model.
Instead, SoC and board descriptors adapt the native AmbiqSuite names at the
vendor boundary.

Typical AmbiqSuite layout:

```text
AmbiqSuite/
    mcu/<mcu-source-name>/hal/...
    src/<system-source-name>/system_<part>.c
    boards/<ambiq-board-name>/bsp/...
    lib/<hal-lib-part-name>/libam_hal.a
    lib/<bsp-lib-part-name>/<bsp-lib-subdir>/libam_bsp.a
```

Those names may differ from the logical NSX SoC skew. For example, a derivative
skew may expose its own Ambiq part define and linker script while reusing a
parent MCU source directory or prebuilt library directory.

Descriptors should use these variables to describe the native layout:

| Variable | Meaning |
| --- | --- |
| `NSX_SOC_SKEW` | Logical NSX skew, such as `apollo510b` |
| `NSX_AMBIQ_PART_NAME` | Ambiq part identity and compile define basis |
| `NSX_AMBIQ_MCU_DIR` | Actual vendor MCU/HAL source directory |
| `NSX_AMBIQ_HAL_LIB_PART_NAME` | Vendor `lib/<toolchain>/<part>/libam_hal.a` part directory, defaults to `NSX_AMBIQ_PART_NAME` |
| `NSX_AMBIQ_BOARD_NAME` | Vendor `boards/<board>/bsp` identity |
| `NSX_AMBIQ_BSP_LIB_PART_NAME` | Vendor `lib/<toolchain>/<part>/<subdir>/libam_bsp.a` part directory, defaults to `NSX_AMBIQ_PART_NAME` |
| `NSX_AMBIQ_BSP_LIB_SUBDIR` | Vendor BSP library subdirectory, such as `evb`, `eb_revb`, or `blue_eb` |

This keeps the high-level NSX graph SoC-first while letting AmbiqSuite stay in
its native shape. The adapter layer should prefer variables and imported paths
over copied vendor source edits.
