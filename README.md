# nsx-ambiq-sdk

The unified Ambiq SDK package for NSX: curated AmbiqSuite SDK-core modules.

The unified `nsx-ambiq-sdk` package provides the AmbiqSuite platform substrate used by NSX apps: SDK provider metadata, curated HAL/BSP artifacts, CMSIS-Core, startup/system integration, board descriptors, SoC descriptors, and NSX-owned runtime services. A single SDK provider, `nsx-ambiqsuite`, mirrors the upstream AmbiqSuite `stable` baseline and is selected per board/SoC.

It is not a full AmbiqSuite mirror. Optional middleware and examples such as FreeRTOS, TinyUSB, Cordio, USB stacks, CMSIS-NN, CMSIS-DSP, and AmbiqSuite sample applications belong in separate optional modules or applications.

## Current Coverage

The provider payload is `nsx-ambiqsuite` (NSX package version `5.2.23`), with GCC and ATfE artifacts for:

The NSX package and module manifests use version `5.2.23`: `5.2` tracks the
upstream AmbiqSuite R5.2 line, while `.23` is the NSX-local curated revision
for module metadata, integration fixes, and packaging updates.

| Board | SoC/skew | Status |
| --- | --- | --- |
| `apollo330mP_evb` | `apollo330P` | staged |
| `apollo510_evb` | `apollo510` | staged |
| `apollo510b_evb` | `apollo510b` | staged |
| `apollo510dL_evb` | `apollo510L` | staged |
| `atomiq110_fpga_turbo` | `atomiq110` | experimental |

`apollo5b_evb` is descriptor-only until matching AmbiqSuite artifacts are promoted into the provider payload.

> **Experimental — not officially released:** `atomiq110` (AT110) and its `atomiq110_fpga_turbo` FPGA board are an early-evaluation preview only, not for production. See [Platform Maturity](docs/platform-coverage.md#platform-maturity).

See [docs/platform-coverage.md](docs/platform-coverage.md) for the current coverage table.

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

CI runs these checks on pull requests and pushes to `main`.

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

For contributor guidance, see [CONTRIBUTING.md](CONTRIBUTING.md).
