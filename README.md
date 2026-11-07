# nsx-ambiq-sdk

The unified Ambiq SDK package for NSX: curated AmbiqSuite SDK-core modules.

The unified `nsx-ambiq-sdk` package provides the AmbiqSuite platform substrate used by NSX apps: SDK provider metadata, curated HAL/BSP artifacts, CMSIS-Core, startup/system integration, board descriptors, SoC descriptors, and NSX-owned runtime services. A single SDK provider, `nsx-ambiqsuite`, carries the curated AmbiqSuite payload: the `stable-2026.11.05` intake for Apollo parts and experimental `atomiq110` (see Current Coverage).

It is not a full AmbiqSuite mirror. Optional middleware and examples such as FreeRTOS, TinyUSB, Cordio, USB stacks, CMSIS-NN, CMSIS-DSP, and AmbiqSuite sample applications belong in separate optional modules or applications.

## Current Coverage

The `5.2.29` release candidate contains the `stable-2026.11.05` intake from
upstream commit `3cfb7d50718e6fb016f1a0128121cb40598109d5`.

All 73 HAL/BSP archives were rebuilt from prepared source commit
`6a2a0c08af553e33716f2e251cb74aa370e2ae0f`, which adds the reviewed AT110 XSPI
capacity/alignment patches. The artifact manifest records both identities.
Four payload patches preserve C++ compatibility and correct EM9305 receive
scratch capacity; the matching NSX transport fixes are included. Cordio retains
its separately recorded base snapshot and selected NSX changes.
This is not an official numbered AmbiqSuite release.

Five-board GCC startup/runtime and selected power/timer integration checks,
AP510 USB traffic and AP510B BLE traffic passed against the previous SDK.
See the qualification report for exact tested revisions and coverage limits.

The promoted payload contains GCC, ATfE, and ACfE artifact families for staged
Apollo2, Apollo3, Apollo4, Apollo330, and Apollo510 platforms. ACfE artifacts
are selected by the public `armclang` toolchain family.

`apollo5b_evb` is descriptor-only until matching AmbiqSuite artifacts are promoted into the provider payload.

> **Experimental — not officially released:** `atomiq110` (AT110) and its `atomiq110_fpga_turbo` FPGA board are an early-evaluation preview only, not for production. See [Platform Maturity](docs/platform-coverage.md#platform-maturity).

See [docs/platform-coverage.md](docs/platform-coverage.md) for the current coverage table.
Release-candidate provenance and qualification are recorded in
[`release/nsx-ambiq-sdk-5.2.29.yaml`](release/nsx-ambiq-sdk-5.2.29.yaml) and
[`release/qualification-5.2.29.md`](release/qualification-5.2.29.md).
The records of earlier versions are kept alongside them.

## Using The Modules

Select a board descriptor, add the staged modules your app needs, and consume the stable CMake targets. The board descriptor defines `nsx::board` and `nsx::board_flags`; the platform modules provide targets such as:

| Target | Purpose |
| --- | --- |
| `nsx::sdk_ambiqsuite` | SDK provider/version anchor |
| `nsx::cmsis_core` | CMSIS 6 Core(M) headers |
| `nsx::ambiq_hal` | AmbiqSuite HAL wrapper |
| `nsx::ambiq_bsp` | AmbiqSuite BSP wrapper |
| `nsx::cmsis_startup` | Startup, vector table, system source, linker script |
| `nsx::soc_hal` | Shared SoC HAL integration layer |
| `nsx::core` | NSX runtime initialization |
| `nsx::timer` | Timer, ticker, and periodic callback helpers |
| `nsx::power` | Power and sleep helpers |
| `nsx::perf` | Timing and performance helpers |
| `nsx::i2c`, `nsx::spi`, `nsx::uart` | Basic bus shims |

NSX is the recommended application-facing API surface for common tasks. Apps may still use AmbiqSuite APIs directly when they need lower-level control or a feature that NSX does not wrap yet. Prefer NSX modules first, keep wrapping common flows over time, and avoid baking board or app policy into generic modules.

The CMake contract is described in [cmake/README.md](cmake/README.md).

## Local Validation

Install test dependencies and run the validation suite:

```sh
uv sync --group ci
uv run --group ci pre-commit run --all-files
uv run --group ci python -m py_compile sdk-intake/build_ambiqsuite.py
uv run --group ci python -m pytest
```

CI runs these checks on pull requests and pushes to `main`. Optional [board tests](docs/local-hardware-tests.md) also run locally.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `boards/` | Board descriptors and board-level facts |
| `cmake/` | Shared CMake helpers and SoC descriptors |
| `modules/` | Logical NSX modules packaged in this release unit |
| `modules/nsx-ambiqsuite/` | Unified SDK provider module |
| `modules/nsx-ambiqsuite/sdk/` | Curated provider payload consumed by CMake |
| `sdk-intake/` | Tooling for staging and promoting AmbiqSuite drops |
| `tools/` | Opt-in local/CI helper scripts such as link smokes |
| `tests/` | Static and CMake contract tests |
| `docs/` | Maintainer notes for intake, scope, and platform coverage |

For contributor guidance, see [CONTRIBUTING.md](CONTRIBUTING.md). For repeated-START sensor reads, see the [I2C API](modules/nsx-i2c/README.md#combined-writeread-with-repeated-start).
