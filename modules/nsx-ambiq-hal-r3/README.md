# nsx-ambiq-hal-r3

`nsx-ambiq-hal-r3` exposes the AmbiqSuite R3 HAL surface used by NSX.

This wrapper module expects an active `nsx-ambiqsuite-r3` provider and publishes
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
- `nsx-ambiqsuite-r3` (required) — SDK provider.
