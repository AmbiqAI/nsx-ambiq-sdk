# nsx-pmu-armv8m

`nsx-pmu-armv8m` provides Arm M-profile PMU event configuration, capture, and
accumulation for PMU-capable NSX targets.

Contents:

- PMU event configuration and snapshots
- named PMU event presets for CPU, memory, and MVE/Helium-oriented capture
- PMU accumulator helpers for per-op and per-run collection
- chunked model/layer capture helpers for bounded-memory profiling of large models
- abstract transport and CSV serialization helpers for emitting capture results over log, SWO, USB, or custom sinks

The current implementation is shared across Apollo5, Apollo510, and Apollo330
M55-class targets because the PMU programming model is the same at the Armv8-M
PMU layer.

Design notes:

- source is organized around the shared Armv8-M PMU implementation
- SoC gating still happens in CMake and module metadata so unsupported targets fail early
- accumulator helpers are intended for model, layer, and function profiling flows used by LiteRT for Microcontrollers and NSX runtime variants
- full-map MVE profiling still uses repeated passes because only four 32-bit PMU events can be sampled at once on the current M55 targets
- chunked capture trades additional model passes for bounded RAM use when a model has too many layers to retain in one dense matrix

## Toolchains

Built and validated under `arm-none-eabi-gcc` and ATfE. PMU access is via CMSIS-6 PMU intrinsics (`PMU_*` macros) supplied by
`nsx-cmsis-core`; toolchain-specific intrinsic mappings are handled inside
CMSIS-6's `cmsis_<compiler>.h`.

## Dependencies

- `nsx-cmsis-core` (required) — CMSIS-6 core + PMU headers.
- `nsx-core` — for shared status, delay, and runtime helper APIs used by the PMU layer.
