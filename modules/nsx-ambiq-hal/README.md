# nsx-ambiq-hal-r5

`nsx-ambiq-hal-r5` exposes the AmbiqSuite R5 HAL surface used by NSX.

This wrapper module expects an active `nsx-ambiqsuite-r5` provider and publishes
the release-specific include paths, utility sources, and prebuilt HAL library
needed by generated apps.

## Toolchains

- `arm-none-eabi-gcc` — consumes `sdk/lib/gcc/<part>/libam_hal.a`
- ATfE (`clang`) — consumes `sdk/lib/atfe/<part>/libam_hal.a`
- Arm Compiler 6 (`armclang`) — consumes `sdk/lib/acfe/<part>/libam_hal.a`

## Dependencies

- `nsx-cmsis-core` (required) — CMSIS-6 core headers.
- `nsx-ambiqsuite-r5` (required) — SDK provider.
