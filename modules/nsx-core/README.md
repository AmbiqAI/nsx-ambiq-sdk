# nsx-core

`nsx-core` provides the common runtime foundation used by bare-metal NSX
applications. It is composed of **cohesive sub-libraries**, each owning a single
concern, with a backward-compatible `nsx::core` umbrella that links them all.

## Sub-libraries

| CMake target | Concern | Sources / headers | Depends on |
|--------------|---------|-------------------|------------|
| `nsx::compiler` | Toolchain-portability macros (header-only) | `nsx_compiler.h` | — |
| `nsx::util` | Byte ring buffer + safe-memcpy helpers | `nsx_ring_buffer.{h,c}` | `soc_hal` |
| `nsx::mem` | Memory-placement macros + I/D cache control | `nsx_mem.h`, `nsx_cache.c` | `compiler`, `soc_hal` |
| `nsx::core_lifecycle` | Runtime init + API versioning (the real "core") | `nsx_core.h`, `nsx_core_lifecycle.c` | `compiler`, `soc_hal` |
| `nsx::runtime` | printf / delay / interrupt / debug shims | `nsx_core.h`, `nsx_core_portable.c` | `core_lifecycle`, `system`, `soc_hal` |
| `nsx::system` | System bring-up + per-SoC backend + newlib retarget/heap | `nsx_system.{h,c}`, `nsx_system_platform.c`, `nsx_retarget.c`, `sbrk.c` | `core_lifecycle`, `mem`, `soc_hal` |
| `nsx::presets` | Opinionated, opt-in system presets (not in the umbrella) | `nsx_presets.{h,c}` | `core_includes` |
| `nsx::core` | **Umbrella** over all of the above (backward compatible) | — | all sub-libraries |

New code may depend on the precise sub-library it needs (e.g. `nsx::util` for
just the ring buffer, with no system/BSP surface). Existing consumers that link
`nsx::core` continue to get the full surface unchanged.

## Headers

| Header | Purpose |
|--------|---------|
| `nsx_compiler.h` | Compiler-attribute portability (GCC / armclang / ATfE / IAR) |
| `nsx_core.h` | `nsx_*` runtime helpers (printf, low-power debug output, delay, interrupt control) plus the core lifecycle API (`nsx_core_init` — must be called first) |
| `nsx_ring_buffer.h` | Byte-oriented ring buffer with interrupt-safe push/pop operations |
| `nsx_system.h` | Modular system init: composable startup building blocks and documentation of boot sequence / gotchas |
| `nsx_presets.h` | Opinionated, opt-in system presets (`nsx_system_development` / `_inference` / `_minimal`); link `nsx::presets` |
| `nsx_mem.h` | Portable memory-placement macros (`NSX_MEM_SRAM`, `NSX_MEM_FAST_CODE`, etc.) and `nsx_cache_enable()` |

## nsx_system Quick Start

```c
#include "nsx_system.h"

int main(void) {
    // One-call init: HP mode, caches, ITM debug, SpotManager
    nsx_system_config_t cfg = nsx_system_development;
    cfg.skip_bsp_init = true;   // skip the BSP delay for fast local loops
    nsx_system_init(&cfg);

    nsx_printf("Hello from NSX\n");
}
```

Three built-in presets:

| Preset | Perf | Cache | Debug | SpotMgr | BSP init |
|--------|------|-------|-------|---------|----------|
| `nsx_system_development` | HIGH | yes | ITM | yes | yes |
| `nsx_system_inference` | HIGH | yes | none | yes | yes |
| `nsx_system_minimal` | LOW | no | none | no | skip |

Or call individual building blocks for fine-grained control — see the
header's "Recommended Initialization Order" section.

## Platform Backends

`nsx_system` is split into a platform-independent sequencing layer
(`src/nsx_system.c`) and per-SoC backends:

| Backend directory | SoCs covered |
|-------------------|--------------|
| `src/apollo510/` | Apollo510, Apollo510B, Apollo510L, Apollo5A, Apollo5B, Apollo330P |

CMake selects the correct backend via `NSX_SOC_FAMILY`.

## nsx_mem Memory Placement

```c
NSX_MEM_SRAM_BSS alignas(16) uint8_t arena[65536];  // shared SRAM, zeroed
NSX_MEM_SRAM     const uint8_t weights[] = {...};    // shared SRAM, from NVM
NSX_MEM_FAST_CODE void hot_isr(void) { ... }         // ITCM (AP510) / DTCM (AP510L/AP330P)
```

Macros degrade gracefully on simpler SoCs (fall back to default sections).

## Toolchains

`nsx-core` is built and validated with the staged R5 toolchains:

| Toolchain | Notes |
|---|---|
| `arm-none-eabi-gcc` (GCC 14.3+) | Reference toolchain |
| `clang` / ATfE (Arm Toolchain for Embedded 22.1+) | GCC-compatible front-end |

Compiler detection lives in `includes-api/nsx_compiler.h` — it normalizes
intrinsics, attributes, and inline-assembly differences so consumers don't need
their own ifdef ladders.

## Dependencies

- `nsx-cmsis-core` — CMSIS-6 core headers used by `nsx_system.c` and the
  per-SoC platform backends.
- The `nsx-ambiqsuite-r5` SDK provider for `am_hal_*` and the system init source.

This repo is CMake-first. CMake descriptors define the module contract.
