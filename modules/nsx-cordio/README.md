# nsx-cordio

Optional **Cordio / WSF Bluetooth LE host stack** for NSX on AmbiqSuite silicon.

This module vendors the AmbiqSuite `third_party/cordio` stack (`ble-host` +
`ble-profiles` + `wsf`) plus `uecc`, pinned to the same SDK drop as the
`nsx-ambiqsuite` provider (`stable-2026.06.18`), and **builds it from source**
for the active SoC's BLE transport. It is the low-level host stack only; the
app-facing convenience API (an `ns_ble.h`-equivalent) lives in the separate
`nsx-ble` module.

## Status / scope (Phase 1 MVP)

- **Target:** Apollo4P Blue (`apollo4p`) with the **Cooper** external controller
  over IOM/SPI.
- **Toolchain:** `arm-none-eabi-gcc` only.
- **RTOS:** the WSF port is the **FreeRTOS** port, built against NSX's FreeRTOS
  v11 (`nsx-freertos`).

Other transports are recognised by the upstream tree but intentionally **not**
wired yet (they fail configure loudly): EM9305 (Apollo510B), the integrated
Apollo3 controller, and the 510L/330P IPC radio.

## Target / dependencies

```
nsx::cordio
  ├─ nsx::ambiq_hal   (AM_PART_* defines, HAL headers, provider devices/ + utils/)
  ├─ nsx::ambiq_bsp   (board pins/BSP for the Cooper transport)
  ├─ nsx::board_flags (part/package/board defines)
  └─ nsx::freertos    (FreeRTOS.h for the WSF FreeRTOS port)
```

`am_devices_cooper.c` ships in the provider payload but is not compiled by any
core module; this module compiles it (the Cooper HCI driver depends on it).

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
