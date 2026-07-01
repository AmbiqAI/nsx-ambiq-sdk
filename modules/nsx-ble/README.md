# nsx-ble

Optional **app-facing Bluetooth LE convenience API** for NSX. This is the NSX
port of the legacy neuralSPOT `ns-ble` wrapper: it lets an application define a
single GATT service with read / write / notify characteristics and callbacks,
on top of the Cordio host stack provided by `nsx-cordio`.

## Status / scope (Phase 1 MVP)

- **Target:** Apollo4P Blue (`apollo4p`) with the Cooper controller.
- **Toolchain:** `arm-none-eabi-gcc` only.
- Validated end-to-end: a `web_ble`-style app builds and **links** to a full
  Apollo4P image (wrapper → Cordio → Cooper HCI driver → HAL/BSP → FreeRTOS v11)
  with zero undefined references. On-hardware runtime validation is the next step.

## API

The public API preserves the legacy `ns_ble_*` names (see `includes-api/ns_ble.h`)
so existing neuralSPOT ns-ble applications port with minimal changes:

- `ns_ble_pre_init`, `ns_ble_char2uuid`
- `ns_ble_create_service`, `ns_ble_create_characteristic`,
  `ns_ble_add_characteristic`, `ns_ble_start_service`
- `ns_ble_send_value`, `ns_ble_set_tx_power`

### Compatibility shim

`includes-api/nsx_ble_compat.h` maps the old neuralSPOT harness onto NSX so the
ported `ns_ble.c` stays essentially verbatim:

| legacy | NSX |
| --- | --- |
| `ns_lp_printf` | `nsx_low_power_printf` |
| `ns_interrupt_master_enable/disable` | `nsx_interrupt_master_enable/disable` |
| `ns_malloc` / `ns_free` | `pvPortMalloc` / `vPortFree` (FreeRTOS heap) |
| `ns_core_api_t` | `nsx_core_api_t` (layout-identical) |
| `NS_STATUS_*`, `NS_TRY` | `NSX_STATUS_*`, `NSX_TRY_ABORT` |

It also pulls in `am_mcu_apollo.h` / `am_bsp.h` (CMSIS/HAL/BSP symbols the
wrapper uses directly), available transitively via `nsx::ambiq_hal`/`ambiq_bsp`.

## Dependencies

```
nsx::ble
  ├─ nsx::cordio   (Cordio host stack; brings ambiq_hal/bsp/board_flags/freertos)
  ├─ nsx::core     (logging, interrupt master, status codes, API version)
  └─ nsx::freertos (dispatcher task runtime)
```

## App contract

- Provide a WSF buffer pool (`ns_ble_pool_config_t`).
- Create a FreeRTOS task that calls `wsfOsDispatcher()` in a loop, and call
  `ns_ble_pre_init()` to set radio IRQ priorities.
- Inherited limits from ns-ble: single service, single connection, no OOB
  pairing, minimal disconnect/error handling.
