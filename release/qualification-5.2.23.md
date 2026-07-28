# Qualification: nsx-ambiq-sdk 5.2.23

Date: 2026-07-28

## Decision

Version `5.2.23` is qualified as a repository-level golden baseline for the
listed staged Apollo descriptors, promoted artifacts, NSX wrappers, and
representative links, with explicit exclusions below.

This is not blanket board-level hardware qualification and not vendor SDK
release qualification. SWS AmbiqSuite remains the source of truth for HAL/BSP
implementation and silicon qualification. The exact provider input is
snapshot `stable-2026.06.18`, ref `stable`, commit
`caaf5af86087881647f56c70646c748d40c86e23`.

## Software And Build Evidence

| Layer | Status | Evidence and scope |
| --- | --- | --- |
| Repository CI | Passed | `uv sync --group ci`; pre-commit; intake-helper `py_compile`; full pytest. |
| Static/contracts | Passed | Manifest, repository-shape, toolchain, intake, public API, and CMake contract tests. |
| Descriptor configure | Passed | CMake contract tests validate staged SoC/board descriptors and required artifact paths. |
| Promoted artifacts | Recorded as built | The promoted artifact manifest records GCC, ATfE, and ACfE builds and per-artifact SHA-256 hashes. These archives were not rebuilt during release preparation. |
| GCC link smoke | Passed | GCC 15.2.1 minimal HAL/BSP/SoC links for `apollo330mP_evb`, `apollo510_evb`, `apollo510b_evb`, and `apollo510dL_evb`. |
| ArmClang link smoke | Passed | ArmClang 6.24.0 minimal HAL/BSP/SoC links for the same four representative boards. |
| ATfE link smoke | Passed | ATfE 22.1.0 minimal HAL/BSP/SoC links for the same four representative boards. |

The qualified descriptor scope is the `qualification.scope` list in
`release/nsx-ambiq-sdk-5.2.23.yaml`. Tests require each listed SoC descriptor,
SoC facts file, and board descriptor to exist.

## Hardware Evidence

Repository evidence is feature-level and limited:

- The bounded newlib `_sbrk` selection is recorded as hardware-validated on
  `apollo330mP_evb` and `apollo510_evb`.
- `nsx-psram` BA-driver selection, capacity, timing scan, synchronous
  transfers, and XIP behavior are recorded as hardware-validated on
  `apollo510_evb` and `apollo510b_evb`.
- `nsx-psram` XIP/DMA read-write is recorded as hardware-validated on
  `apollo4p_evb_disp_shield_rev2`.

These records do not qualify every module or peripheral on those boards.
There is no complete board-farm matrix for reset, UART, timer, sleep, PMU,
basic I/O, BLE, USB, and all optional modules. No broader hardware coverage is
inferred.

## Exclusions

- `apollo5b` and `apollo5b_evb` are descriptor-only. Matching promoted
  HAL/BSP artifacts are absent, so they are not configure/link qualified.
- `atomiq110` and `atomiq110_fpga_turbo` remain experimental and are excluded
  from the official qualified scope even though descriptors and artifacts are
  present.
- Open AT110 feature work is not part of this baseline.
- Optional-module support remains limited to each module's declared
  compatibility and evidence; inclusion in the distribution is not a claim
  of universal board or hardware qualification.
