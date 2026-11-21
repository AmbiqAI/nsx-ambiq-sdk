# nsx-ambiq-sdk

The unified Ambiq SDK package for NSX: curated AmbiqSuite SDK-core modules.

The unified `nsx-ambiq-sdk` package provides the AmbiqSuite platform substrate used by NSX apps: SDK provider metadata, curated HAL/BSP artifacts, CMSIS-Core, startup/system integration, board descriptors, SoC descriptors, and NSX-owned runtime services. A single SDK provider, `nsx-ambiqsuite`, carries the curated AmbiqSuite payload: the `stable-2026.11.05` intake for Apollo parts and experimental `atomiq110` (see Current Coverage).

It is not a full AmbiqSuite mirror. Optional middleware and examples such as FreeRTOS, TinyUSB, Cordio, USB stacks, CMSIS-NN, CMSIS-DSP, and AmbiqSuite sample applications belong in separate optional modules or applications.

## Licensing

The [root license](LICENSE) uses AmbiqSuite's BSD terms for Ambiq-authored NSX
integration. The [repository notice](NOTICE) and [license index](LICENSES/README.md)
identify the separately licensed components and their terms.

The imported SDK payload is subject to its upstream terms. For redistribution,
see sections 2.1 and 2.2 of the [AmbiqSuite agreement](modules/nsx-ambiqsuite/sdk/docs/licenses/LICENSE.rtf)
and the accompanying [Software License Terms](modules/nsx-ambiqsuite/sdk/docs/licenses/Ambiq-Software-License-Terms.pdf).
Retain the applicable component notices.

## Current Coverage

SDK 5.2.32 uses the same AmbiqSuite provider as 5.2.31. Its 73 HAL/BSP archives
come from prepared source `83bd69c37edf8d94cf77021140d707c3fe886242`, based on
`stable-2026.11.05` at `3cfb7d50718e6fb016f1a0128121cb40598109d5`. It retains
the AT110 XSPI fixes, bounded heap integration and four staged payload patches.
Cordio has its own recorded source revision.

The [power module](modules/nsx-power/README.md) exposes live domain status and
raw regulator pulse counts. Apollo330 Plus and Apollo510 Lite support status
reads and report buck pulse counting as unsupported.

The promoted payload contains GCC, ATfE, and ACfE artifact families for staged
Apollo2, Apollo3, Apollo4, Apollo330, and Apollo510 platforms. ACfE artifacts
are selected by the public `armclang` toolchain family.

`apollo5b_evb` is descriptor-only until matching AmbiqSuite artifacts are promoted into the provider payload.

> **Experimental — not officially released:** `atomiq110` (AT110) and its `atomiq110_fpga_turbo` FPGA board are an early-evaluation preview only, not for production. See [Platform Maturity](docs/platform-coverage.md#platform-maturity).

See [docs/platform-coverage.md](docs/platform-coverage.md) for the current coverage table.
Release-candidate provenance and qualification are recorded in
[`release/nsx-ambiq-sdk-5.2.32.yaml`](release/nsx-ambiq-sdk-5.2.32.yaml) and
[`release/qualification-5.2.32.md`](release/qualification-5.2.32.md).
The records of earlier versions are kept alongside them.

## Using The Modules

For registry-managed apps, use neuralspotx 0.9.6 or newer before selecting SDK
5.2.32. Its resolver understands BSP provider roles, including for built-in EVBs.
Direct CMake users can continue to select their board and staged modules.

The opt-in [BSP contract](modules/nsx-core/README.md#optional-bsp-debug-transports)
supports optional UART/SWO on Apollo330P and Apollo510. Existing EVBs default
to v1. The board descriptor defines `nsx::board` and `nsx::board_flags`. The
platform modules provide these CMake targets:

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

For local validation and contributor guidance, see [CONTRIBUTING.md](CONTRIBUTING.md). For repeated-START sensor reads, see the [I2C API](modules/nsx-i2c/README.md#combined-writeread-with-repeated-start).
