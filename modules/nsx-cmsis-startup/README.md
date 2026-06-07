# nsx-cmsis-startup

`nsx-cmsis-startup` builds the board-selected startup and system sources needed
to boot an NSX bare-metal application.

The actual source files are provided by the selected board and SDK provider. This
module supplies the shared CMake target shape and export surface used by generated
apps.

## What it provides

- A single CMake target (`nsx::cmsis_startup`) that compiles whichever
  toolchain-specific startup file the active board points at.
- The system init source (`system_<part>.c`) from the AmbiqSuite provider.
- The reset/entry symbols (`Reset_Handler`, `__main`, `_start`) and the
  default vector table.
- The linker script wired up by the board (`linker_script_sbl.ld` for GCC/ATfE,
  `linker_script_sbl.sct` for armclang).

The per-toolchain startup variants live in `nsx-core/src/<soc>/{gcc,armclang}/`
and the board's `board.cmake` selects between them based on
`NSX_TOOLCHAIN_FAMILY`.

## Toolchains

| Toolchain | Startup source | Linker script |
|---|---|---|
| `arm-none-eabi-gcc` (GCC) | `startup_gcc.c` | `linker_script_sbl.ld` |
| `armclang` (Arm Compiler 6) | `startup_keil6.c` | `linker_script_sbl.sct` |
| `clang` / ATfE (Arm Toolchain for Embedded) | `startup_gcc.c` | `linker_script_sbl.ld` |

ATfE shares GCC's startup and linker script — it is GCC-compatible at the
start-up surface.

## Dependencies

- `nsx-cmsis-core` — provides the CMSIS-6 core headers (`core_cm*.h`,
  `cmsis_compiler.h`, `cmsis_gcc.h`, `cmsis_armclang.h`, `cmsis_clang.h`)
  consumed by the startup sources.
- An active SDK provider (`nsx-ambiqsuite-r3/r4/r5`) — supplies
  `system_<part>.c`.
- The selected board module — supplies `board.cmake`, the linker script path,
  and the part/family defines.
