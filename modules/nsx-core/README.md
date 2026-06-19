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
| `nsx_mem.h` | Portable memory-placement macros (`NSX_MEM_SRAM`, `NSX_MEM_FAST_CODE`, etc.) and cache helpers (`nsx_cache_enable()`, `nsx_cache_flush()`) |

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

`nsx_mem.h` also exposes lightweight cache helpers for common portable flows:

- `nsx_cache_enable()` / `nsx_cache_disable()` for turning cache on or off.
- Guarantee-named coherence helpers that say *what* they promise rather than
  the overloaded word "flush". Each is gated by an `NSX_CACHE_HAS_*` capability
  macro and returns `NSX_CACHE_UNSUPPORTED` where the target cannot honor it:
  - `nsx_cache_publish_writes()` — make prior CPU writes visible to other bus
    masters (device → host/DMA).
  - `nsx_cache_invalidate_observed_data()` — discard stale CPU copies so the
    next read observes external writes (host/DMA → device).
  - `nsx_cache_sync_shared_data()` — conservative bidirectional sync point for a
    shared buffer.
- `nsx_cache_flush()` is retained as a backward-compatible alias of
  `nsx_cache_publish_writes()`.

Capability matrix (✓ = honored, — = reports `NSX_CACHE_UNSUPPORTED`):

| Family | enable/ disable | publish_writes | invalidate_observed | sync_shared | Underlying primitive |
| --- | :---: | :---: | :---: | :---: | --- |
| Apollo2 | ✓ | — | — | — | enable/disable only; no public bus-flush |
| Apollo3 / 3P | ✓ | ✓ | — | — | `am_hal_sysctrl_bus_write_flush()` (SYNC_READ) |
| Apollo4 / 4L / 4P | ✓ | ✓ | — | ✓ | DAXI flush (+invalidate); data memory is not CPU read-cached |
| Apollo5xx / 330P / 510L | ✓ | ✓ | ✓ | ✓ | split D-cache `clean` / `invalidate` / `clean+invalidate` |

`invalidate_observed_data` is honored only on Apollo5-class parts, which have a
real CPU data cache over data memory. On Apollo2/3/4 data memory is not CPU
read-cached (Apollo4 DAXI is a write buffer, not a read cache), so that
guarantee correctly reports unsupported rather than pretending to act.

Callers can branch at compile time on the capability macros
(`NSX_CACHE_HAS_PUBLISH_WRITES`, `NSX_CACHE_HAS_INVALIDATE_OBSERVED`,
`NSX_CACHE_HAS_SYNC_SHARED`, `NSX_CACHE_HAS_EXPLICIT_DCACHE`) or at run time on
the `NSX_CACHE_UNSUPPORTED` return code.

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
