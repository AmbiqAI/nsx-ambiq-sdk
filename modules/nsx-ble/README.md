# nsx-ble

Optional **app-facing Bluetooth LE convenience API** for NSX. It lets an
application define a single GATT service with read / write / notify
characteristics and callbacks, on top of the Cordio host stack provided by
`nsx-cordio`.

## Status / scope

- **Status:** Registered/first-class NSX module (available via the standard
  `nsx module add nsx-ble` registry flow). The API and example have
  hardware-smoke coverage, but this is still the first NSX BLE baseline —
  scope is limited to a single service/connection with no OOB pairing, and
  broader app coverage is still growing.
- **Targets:** Apollo3 Blue Plus, Apollo4 Blue Plus, and Apollo510B EVB.
- **Toolchains:** GCC build-validated across the BLE targets; Apollo3/Apollo4
  module startup coverage also validates ATFE/armclang paths.
- **Runtime validation:** `ble_webble` has been flashed and smoke-tested on AP3,
  AP4, and AP510B for advertising, GAP name, Device Information Service fields,
  custom GATT service discovery, characteristic reads, and notifications.

## API

The public API preserves the legacy `ns_ble_*` names (see `includes-api/ns_ble.h`)
so existing neuralSPOT ns-ble applications port with minimal changes:

- `ns_ble_pre_init`, `ns_ble_char2uuid`
- `ns_ble_create_service`, `ns_ble_create_characteristic`,
  `ns_ble_add_characteristic`, `ns_ble_start_service`
- `ns_ble_send_value`, `ns_ble_set_tx_power`
- `ns_ble_service_set_device_info`, `ns_ble_service_set_connection_config`,
  `ns_ble_service_set_event_handler`
- `ns_ble_current_connection_id`, `ns_ble_request_mtu`

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
  `ns_ble_pre_init()` before starting the dispatcher.
- Own board/radio IRQ policy in the app: vector symbols, NVIC priorities, power,
  cache, task sizing, WSF pool sizing, service contents, TX power, and security
  policy are intentionally not owned by this module.
- For periodic notify characteristics, the notify callback updates the
  application value and returns a status; the wrapper sends the notification for
  non-async characteristics after a successful callback. Async characteristics
  may call `ns_ble_send_value()` themselves.
- Current intentional limits: one wrapper-managed service, one active
  connection, and no high-level pairing/bonding policy helpers.

## BLE model

`nsx-ble` intentionally exposes a small GATT-peripheral model. The app defines
service metadata, characteristics, callbacks, WSF buffers, tasking, and board
policy. The wrapper wires those pieces into Cordio so the app can advertise,
accept one connection, serve reads/writes, and send notifications. It does not
try to own product policy such as bonding, privacy, application protocol design,
or multi-service orchestration.
