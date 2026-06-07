# nsx-ambiq-hal-r2

`nsx-ambiq-hal-r2` exposes the AmbiqSuite R2 HAL surface used by NSX.

This wrapper module expects an active `nsx-ambiqsuite-r2` provider and publishes
the release-specific include paths, utility sources, and prebuilt HAL library
needed by generated apps.

## Toolchains

- `arm-none-eabi-gcc` (reference)
- `armclang` (Arm Compiler 6)
- `clang` / ATfE (Arm Toolchain for Embedded)

The prebuilt `libam_hal.a` is GCC-built and AAPCS-compatible; armclang and
ATfE consume the same archive without recompilation.

## Dependencies

- `nsx-cmsis-core` (required) — CMSIS-6 core headers; the legacy CMSIS-5
  ARM headers previously exported via `CMSIS/ARM/Include` have been dropped.
- `nsx-ambiqsuite-r2` (required) — SDK provider.
