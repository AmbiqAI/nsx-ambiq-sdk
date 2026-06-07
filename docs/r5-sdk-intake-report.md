# R5.2.0 SDK Intake Report

Input zip: `sdk-intake/local/drops/AmbiqSuite_R5.2.0.zip`

Extracted locally to `sdk-intake/local/work/AmbiqSuite_R5.2.0/AmbiqSuite_5.2.0`.
Raw source remains in ignored local staging and is not part of the curated repo
payload.

Upstream revision from `VERSION.txt`:

```text
release_sdk5p2p0-bee737faa
```

## Native Layout Found

MCU/source roots:

| Native part | Notes |
| --- | --- |
| `mcu/apollo330P` | AP330P HAL source and registers. |
| `mcu/apollo510` | AP510/AP510B HAL source and registers. |
| `mcu/apollo510L` | AP510L/AP510DL HAL source and registers. |

Board BSP roots:

| Native board | Family from `board-defs.mk` |
| --- | --- |
| `apollo330mP_evb` | `apollo330P` |
| `apollo510_evb` | `apollo510` |
| `apollo510b_evb` | `apollo510` |
| `apollo510dL_evb` | `apollo510L` |

The AP510D-family board in this SDK is named `apollo510dL_evb` and maps to the
`apollo510L` HAL family in AmbiqSuite's native makefiles.

## Build Results

Built static libraries are collected under `artifacts/ambiqsuite/R5.2.0/`.
See `artifacts/ambiqsuite/R5.2.0/manifest.yaml` for checksums.

| Toolchain | HAL | BSP | Status |
| --- | --- | --- | --- |
| GCC | `apollo330P`, `apollo510`, `apollo510L` | `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb` | built |
| ATfE | `apollo330P`, `apollo510`, `apollo510L` | `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb` | built |
| ACfE | `apollo330P`, `apollo510`, `apollo510L` | `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, `apollo510dL_evb` | built |

## Build Notes

GCC builds used AmbiqSuite's native GCC makefiles directly.

ATfE builds reused the same GCC-style makefiles with command-line tool overrides:

```text
CONFIG=bin-atfe
FPU=fp-armv8-fullfp16-sp-d16
CC="/home/adamp/.local/ATfE/ATfE-22.1.0/bin/clang --target=thumbv8.1m.main-unknown-none-eabihf"
GCC="/home/adamp/.local/ATfE/ATfE-22.1.0/bin/clang --target=thumbv8.1m.main-unknown-none-eabihf"
AR=/home/adamp/.local/ATfE/ATfE-22.1.0/bin/llvm-ar
EXTRA_CFLAGS="-Wno-unused-command-line-argument -Wno-c11-extensions -Wno-c23-extensions -Wno-gnu-zero-variadic-macro-arguments -Wno-strict-prototypes"
```

The extra warning flags are needed because AmbiqSuite's GCC makefiles include
`-pedantic-errors`, while the AP330P/AP510L headers use anonymous unions, large
enum constants, GNU zero-argument variadic macros, and K&R-style empty parameter
lists that clang diagnoses more strictly than GCC.

ACfE builds reuse the same GCC-style makefiles with Arm Compiler 6 tool
overrides:

```text
CONFIG=bin-acfe
FPU=fp-armv8-fullfp16-sp-d16
CC="/Applications/ARMClangToolchain/6.24Rel19/bin/armclang --target=arm-arm-none-eabi"
GCC="/Applications/ARMClangToolchain/6.24Rel19/bin/armclang --target=arm-arm-none-eabi"
LD=/Applications/ARMClangToolchain/6.24Rel19/bin/armlink
AR="<python> sdk-intake/local/tools/acfe_armar_wrapper.py /Applications/ARMClangToolchain/6.24Rel19/bin/armar"
EXTRA_CFLAGS="-g0 -Wno-c11-extensions -Wno-c23-extensions -Wno-pedantic"
```

The wrapper normalizes AmbiqSuite's GNU-style archive invocation into the
argument order required by `armar`, without editing the upstream SDK drop.

## Link Smoke Validation

An ACfE `armclang` CMake smoke project was configured and linked on macOS with
Arm Compiler for Embedded 6.24. The smoke project linked a minimal executable
through the NSX board, BSP, HAL, and SoC-HAL targets for every staged board:

| Board | Linked ACfE artifacts |
| --- | --- |
| `apollo330mP_evb` | `sdk/lib/acfe/apollo330P/apollo330mP_evb/libam_bsp.a`, `sdk/lib/acfe/apollo330P/libam_hal.a` |
| `apollo510_evb` | `sdk/lib/acfe/apollo510/apollo510_evb/libam_bsp.a`, `sdk/lib/acfe/apollo510/libam_hal.a` |
| `apollo510b_evb` | `sdk/lib/acfe/apollo510/apollo510b_evb/libam_bsp.a`, `sdk/lib/acfe/apollo510/libam_hal.a` |
| `apollo510dL_evb` | `sdk/lib/acfe/apollo510L/apollo510dL_evb/libam_bsp.a`, `sdk/lib/acfe/apollo510L/libam_hal.a` |

All four smoke builds produced `smoke.elf` with `armlink --cpu=cortex-m55`.

## Remaining Work

- Decide whether future releases should keep consuming the promoted provider
  payload under `modules/nsx-ambiqsuite-r5/sdk/lib/...` or switch adapters to
  consume `artifacts/ambiqsuite/<version>/<toolchain>/lib/...` directly.
