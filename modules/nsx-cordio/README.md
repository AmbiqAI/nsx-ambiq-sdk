# nsx-cordio

Optional **Cordio / WSF Bluetooth LE host stack** for NSX on AmbiqSuite silicon.

This module vendors the AmbiqSuite `third_party/cordio` stack (`ble-host` +
`ble-profiles` + `wsf`) plus `uecc`, pinned to the same SDK drop as the
`nsx-ambiqsuite` provider (`stable-2026.06.18`), and **builds it from source**
for the active SoC's BLE transport. It is the low-level host stack only; the
app-facing convenience API (an `ns_ble.h`-equivalent) lives in the separate
`nsx-ble` module.

## Status / scope

- **Status:** Registered/first-class NSX module (available via the standard
  `nsx module add nsx-cordio` registry flow). The supported transports have
  hardware-smoke coverage through `ble_webble`; Apollo510B EM9305 support
  remains build-gated pending hardware validation.
- **Targets/transports:**
  - Apollo3 / Apollo3P integrated BLE controller.
  - Apollo4P Blue Cooper external controller over IOM/SPI.
  - Apollo510B EM9305 external controller over SPI/GPIO IRQ.
- **Toolchain:** GCC build-validated across supported BLE targets.
- **RTOS:** the WSF port is the **FreeRTOS** port, built against NSX's FreeRTOS
  v11 (`nsx-freertos`).
- **Runtime validation:** `ble_webble` has been flashed and smoke-tested on AP3,
  AP4, and AP510B.

Other upstream transports, such as the 510L/330P IPC radio, are intentionally
not wired yet and should fail configure loudly until explicitly supported.

## Target / dependencies

```
nsx::cordio
  ├─ nsx::ambiq_hal   (AM_PART_* defines, HAL headers, provider devices/ + utils/)
  ├─ nsx::ambiq_bsp   (board pins/BSP for the Cooper transport)
  ├─ nsx::board_flags (part/package/board defines)
  └─ nsx::freertos    (FreeRTOS.h for the WSF FreeRTOS port)
```

Transport-specific Ambiq device/util sources that are BLE-only are compiled
here rather than in the core SDK provider.

## App contract

- WSF buffer pool sizing/descriptors are application/wrapper policy.
- `FreeRTOSConfig.h` is provided by the app via `nsx::freertos_config`
  (through `nsx-freertos`).
- The dispatcher task (`wsfOsDispatcher()`) and radio IRQ/NVIC glue are owned by
  the consuming wrapper (`nsx-ble`), not this module.

## Provenance

See `sdk/third_party/NSX_VENDORED_VERSION.txt`. Sources are an unmodified copy of
the AmbiqSuite `third_party/cordio` and `third_party/uecc` trees at the pinned
revision.
