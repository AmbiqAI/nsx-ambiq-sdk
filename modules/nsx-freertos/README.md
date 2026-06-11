# nsx-freertos

Optional FreeRTOS kernel middleware for the NSX SDK. It vendors a pinned
upstream [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel) (tag
`V11.1.0`) and builds the CMSIS port selected by the SoC descriptor. The core
SDK links no kernel and claims no tick source; this module is opt-in.

## Status

Phase 1 MVP:

- Port: generic upstream `ARM_CM55_NTZ` (non-TrustZone)
- SoC: Apollo510 (Cortex-M55)
- Toolchain: `arm-none-eabi-gcc`
- Heap: `heap_4`
- Tick: plain SysTick (tickless idle disabled)

Later phases add the Cortex-M4F track (Apollo2/3/4), armclang and ATfE
toolchains, the Ambiq STIMER tickless ports, and low-power sleep hooks.

## What this module owns vs. the application

| Owned by nsx-freertos | Owned by the application |
| --- | --- |
| Kernel sources, selected port, `heap_4` | `FreeRTOSConfig.h` contents |
| `nsx::freertos` build target | `configTOTAL_HEAP_SIZE`, tick rate |
| Linking `nsx::soc_flags` (CPU/FPU/MVE) | Task topology, stack sizes, priorities |
| `nsx_freertos_start()` wrapper | Creating tasks before starting the scheduler |

## Integration

The kernel cannot compile without an application `FreeRTOSConfig.h`, so the
application must expose its location through an `nsx::freertos_config` interface
target **before** adding this module:

```cmake
add_library(app_freertos_config INTERFACE)
target_include_directories(app_freertos_config INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/config   # directory containing FreeRTOSConfig.h
)
add_library(nsx::freertos_config ALIAS app_freertos_config)

# ... select SoC/board, then:
add_subdirectory(<path-to>/nsx-freertos nsx-freertos)

target_link_libraries(my_app PRIVATE nsx::freertos)
```

Copy [templates/FreeRTOSConfig.h.template](templates/FreeRTOSConfig.h.template)
into your application as `FreeRTOSConfig.h` and tune it.

### Interrupt handler binding

The ARMv8-M port provides strong `SVC_Handler`, `PendSV_Handler`, and
`SysTick_Handler` symbols. NSX per-SoC startup declares those vectors `weak`, so
the port overrides them at link time automatically. Do not redefine them in the
application and do not remap them in `FreeRTOSConfig.h`.

## Vendored kernel

`sdk/third_party/FreeRTOS-Kernel/` is a curated subset of the upstream kernel at
tag `V11.1.0` (recorded in `NSX_VENDORED_VERSION.txt`). The upstream MIT license
is retained in that directory. To update, re-vendor from the upstream tag rather
than hand-editing kernel sources.
