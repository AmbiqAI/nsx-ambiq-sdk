# nsx-ambiq-bsp-r5

`nsx-ambiq-bsp-r5` exposes the AmbiqSuite R5 board support package surface used
by NSX.

This wrapper module expects an active `nsx-ambiqsuite-r5` provider and publishes
the board BSP include path together with the prebuilt `libam_bsp.a` dependency.

## Toolchains

- `arm-none-eabi-gcc` — consumes `sdk/lib/gcc/<part>/<board>/libam_bsp.a`
- ATfE (`clang`) — consumes `sdk/lib/atfe/<part>/<board>/libam_bsp.a`
- Arm Compiler 6 (`armclang`) — consumes `sdk/lib/acfe/<part>/<board>/libam_bsp.a`
