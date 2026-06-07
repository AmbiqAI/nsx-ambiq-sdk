# nsx-ambiq-bsp-r3

`nsx-ambiq-bsp-r3` exposes the AmbiqSuite R3 board support package surface used
by NSX.

This wrapper module expects an active `nsx-ambiqsuite-r3` provider and publishes
the board BSP include path together with the prebuilt `libam_bsp.a` dependency.

## Toolchains

Validated under `arm-none-eabi-gcc`, `armclang`, and `clang`/ATfE. The
prebuilt `libam_bsp.a` is AAPCS-compatible and consumed by all three.
