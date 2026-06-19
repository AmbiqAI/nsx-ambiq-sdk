# nsx-ambiq-hal

`nsx-ambiq-hal` exposes the AmbiqSuite HAL surface used by NSX.

This wrapper module expects an active `nsx-ambiqsuite` provider and publishes
the provider include paths, utility sources, and prebuilt HAL library
needed by generated apps.

## Toolchains

- `arm-none-eabi-gcc` — consumes `sdk/lib/gcc/<part>/libam_hal.a`
- ATfE (`clang`) — consumes `sdk/lib/atfe/<part>/libam_hal.a`
- Arm Compiler 6 (`armclang`) — consumes `sdk/lib/acfe/<part>/libam_hal.a`

## Dependencies

- `nsx-cmsis-core` (required) — CMSIS-6 core headers.
- `nsx-ambiqsuite` (required) — SDK provider.
